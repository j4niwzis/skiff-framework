// skiff.app.serve: skiff.app's page served live -- the framework's live
// feature. `program serve [port] [--no-script] [--client DIR]` listens (127.0.0.1, 8080 unless said) and
// keeps, for each browser that opens the page, the program's tree and a
// model of its own on the server (skiff::web::Live): the page goes out as
// one HTML stream that is never closed, what the browser does comes back as
// a form posted into a hidden frame, and what that moved is appended to the
// stream. No script: the page is the browser's own HTML and CSS
// (Declarative Shadow DOM, a form, a frame). Where the browser runs script,
// a few lines of it make the same page better -- a press posted without
// leaving the field, what is typed posted as it is typed -- unless
// --no-script says the page is to have none.
//
//   GET  /                       the page, streamed; a session made for it
//   POST /skiff/event?s=SESSION  a form posted from the page: 204
//   GET  /skiff/client/index.js, index.wasm
//                                the program built for the browser (--client
//                                DIR), which takes over the parts that are
//                                its own (client(view)) where there is script
//
// Each session's start(Model &) is the program's (Hooks::start), as a page
// made in the browser starts. A quiet stream is kept open by a heartbeat.
module;

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

export module skiff.app.serve;

import std;
import skiff.ui;
import skiff.app.net;
import skiff.app.net.native;
import skiff.app.route;

namespace skiff::app::serve {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

inline constexpr std::chrono::seconds kHeartbeat{25};
inline constexpr std::string_view kEventPath = "/skiff/event";

// What a request asks, read once where it comes in: a page at a path (the
// program's route), a form posted, the program built for the browser.
struct Page {
  std::string path;
};
struct Event {
  std::string session;
};
// The program built for the browser, for the parts that are its own.
enum class Asset : std::uint8_t { kScript, kWasm };
struct File {
  Asset asset;
};
struct Missing {};
using Route = std::variant<Page, Event, File, Missing>;
inline constexpr std::string_view kClientPath = "/skiff/client/";

[[nodiscard]] inline Route routeOf(http::verb verb, std::string_view target) {
  const std::size_t q = std::min(target.find('?'), target.size());
  const std::string_view path = target.substr(0, q);
  const std::string_view query = target.substr(std::min(q + 1, target.size()));
  if (verb == http::verb::post && path == kEventPath && query.starts_with("s=")) return Event{std::string(query.substr(2))};
  if (verb == http::verb::get && path == std::string(kClientPath) + "index.js") return File{Asset::kScript};
  if (verb == http::verb::get && path == std::string(kClientPath) + "index.wasm") return File{Asset::kWasm};
  if (verb == http::verb::get && !path.starts_with("/skiff/")) return Page{std::string(path)};
  return Missing{};
}

// A session's name: 128 bits from the system's randomness, in hex -- what
// lets a form post into this page and no other.
[[nodiscard]] inline std::string sessionName() {
  std::random_device random;
  return std::views::iota(0, 4) | std::views::transform([&](int) { return std::format("{:08x}", random()); }) | std::views::join |
         std::ranges::to<std::string>();
}

template <class A> struct Session {
  using Live = skiff::web::Live<typename A::PageTree, typename A::Model>;
  typename A::Model model;
  std::optional<Live> page;
  // Woken where a form was posted, or an answer came from the network: the
  // stream sends what it moved.
  asio::steady_timer wake;
  // What its model asks of the network (skiff.app.net), asked from here.
  skiff::app::net::Runner<typename A::Model, skiff::app::net::native> runner;
  explicit Session(asio::any_io_executor ex) : wake(ex, asio::steady_timer::time_point::max()), runner(skiff::app::net::native(ex)) {
    runner.transport().wakes(&wake);
  }
};

template <class A> struct Server {
  std::unordered_map<std::string, std::unique_ptr<Session<A>>> sessions;
  std::string css;
  bool script = true;
  // Where the program built for the browser is (index.js, index.wasm), if
  // it is served with the page.
  std::optional<std::filesystem::path> client;

  asio::awaitable<void> stream(beast::tcp_stream &socket, unsigned version, std::string path) {
    const std::string name = sessionName();
    auto &session = *sessions.emplace(name, std::make_unique<Session<A>>(socket.get_executor())).first->second;
    if constexpr (requires { A::hooks().start(session.model); })
      A::hooks().start(session.model);
    typename Session<A>::Live::Options options;
    options.action = std::string(kEventPath) + "?s=" + name;
    options.css = css;
    options.script = script;
    // Its links, the program's routes, read against the server's root.
    options.head = "<base href=\"/\">\n";
    if constexpr (requires { A::hooks().head(); })
      options.head += std::string(A::hooks().head());
    // The route the path asked for: where the session begins.
    skiff::app::route::Router<typename A::Model> router;
    (void)router.arrive(session.model, path);
    if (client && script) options.head += "<script async src=\"" + std::string(kClientPath) + "index.js\"></script>\n";
    session.page.emplace(skiff::ui::mount(skiff::ui::onPage, A::view()), session.model, std::move(options));

    http::response<http::empty_body> head{http::status::ok, version};
    head.set(http::field::content_type, "text/html; charset=utf-8");
    head.set(http::field::cache_control, "no-store");
    head.chunked(true);
    http::response_serializer<http::empty_body> serializer{head};
    beast::error_code ec;
    co_await http::async_write_header(socket, serializer, asio::redirect_error(asio::use_awaitable, ec));
    std::string piece = session.page->begin();
    while (!ec) {
      if (!piece.empty()) {
        co_await asio::async_write(socket.socket(), http::make_chunk(asio::buffer(piece)), asio::redirect_error(asio::use_awaitable, ec));
        if (ec) break;
      }
      session.runner.step(session.model);
      piece = session.page->update();
      if (!piece.empty()) continue;
      // Nothing to send: waited for -- a post, an answer, a request to try
      // again (looked at often while one waits), or the heartbeat.
      session.wake.expires_after(session.runner.busy() ? std::chrono::milliseconds(100) : std::chrono::milliseconds(kHeartbeat));
      beast::error_code woken;
      co_await session.wake.async_wait(asio::redirect_error(asio::use_awaitable, woken));
      session.runner.step(session.model);
      piece = session.page->update();
      if (piece.empty() && !woken && !session.runner.busy()) piece = std::string(skiff::web::live::kHeartbeat);
    }
    sessions.erase(name);
  }

  asio::awaitable<void> event(beast::tcp_stream &socket, unsigned version, const Event &to, std::string_view body) {
    const auto found = sessions.find(to.session);
    http::response<http::empty_body> answer{found == sessions.end() ? http::status::gone : http::status::no_content, version};
    if (found != sessions.end()) {
      found->second->page->post(body);
      found->second->wake.cancel();
    }
    beast::error_code ec;
    co_await http::async_write(socket, answer, asio::redirect_error(asio::use_awaitable, ec));
  }

  asio::awaitable<void> connection(tcp::socket accepted) {
    beast::tcp_stream socket(std::move(accepted));
    beast::flat_buffer buffer;
    for (;;) {
      http::request<http::string_body> request;
      beast::error_code ec;
      co_await http::async_read(socket, buffer, request, asio::redirect_error(asio::use_awaitable, ec));
      if (ec) co_return;
      const Route route = routeOf(request.method(), request.target());
      const unsigned version = request.version();
      // The page's stream is the connection's to its end.
      bool streamed = false;
      co_await std::visit(Answer{*this, socket, version, request.body(), streamed}, route);
      if (streamed || !request.keep_alive()) co_return;
    }
  }
  struct Answer {
    Server &server;
    beast::tcp_stream &socket;
    unsigned version;
    std::string_view body;
    bool &streamed;
    asio::awaitable<void> operator()(const Page &page) const {
      streamed = true;
      return server.stream(socket, version, page.path);
    }
    asio::awaitable<void> operator()(const Event &to) const { return server.event(socket, version, to, body); }
    asio::awaitable<void> operator()(const File &file) const { return server.file(socket, version, file); }
    asio::awaitable<void> operator()(const Missing &) const { return missing(socket, version); }
  };
  asio::awaitable<void> file(beast::tcp_stream &socket, unsigned version, const File &file) {
    if (!client) co_return co_await missing(socket, version);
    struct Kind {
      std::string_view name, type;
    };
    static constexpr std::array<Kind, 2> kKinds{Kind{"index.js", "text/javascript"}, Kind{"index.wasm", "application/wasm"}};
    const Kind kind = kKinds[std::to_underlying(file.asset)];
    std::ifstream in(*client / kind.name, std::ios::binary);
    if (!in) co_return co_await missing(socket, version);
    http::response<http::string_body> answer{http::status::ok, version};
    answer.set(http::field::content_type, kind.type);
    answer.body() = std::string(std::istreambuf_iterator<char>(in), {});
    answer.prepare_payload();
    beast::error_code ec;
    co_await http::async_write(socket, answer, asio::redirect_error(asio::use_awaitable, ec));
  }
  static asio::awaitable<void> missing(beast::tcp_stream &socket, unsigned version) {
    http::response<http::string_body> answer{http::status::not_found, version};
    answer.set(http::field::content_type, "text/plain; charset=utf-8");
    answer.body() = "not found\n";
    answer.prepare_payload();
    beast::error_code ec;
    co_await http::async_write(socket, answer, asio::redirect_error(asio::use_awaitable, ec));
  }

  asio::awaitable<void> listen(tcp::endpoint at) {
    tcp::acceptor acceptor(co_await asio::this_coro::executor, at);
    std::println("skiff::app: serving http://{}:{}/", at.address().to_string(), at.port());
    for (;;) {
      tcp::socket accepted = co_await acceptor.async_accept(asio::use_awaitable);
      asio::co_spawn(acceptor.get_executor(), connection(std::move(accepted)), asio::detached);
    }
  }
};

} // namespace skiff::app::serve

export namespace skiff::app::serve {
// The program served, where it is asked to be (`program serve [port]`): what
// it ends with; else nothing, and the program does what it does natively.
template <class A> [[nodiscard]] std::optional<int> run(int argc, char **argv) {
  const std::vector<std::string_view> args(argv + std::min(argc, 1), argv + argc);
  if (args.empty() || args.front() != "serve") return std::nullopt;
  std::uint16_t port = 8080;
  bool script = true;
  std::optional<std::filesystem::path> client;
  bool clientNext = false;
  for (const std::string_view arg : args | std::views::drop(1)) {
    if (clientNext) {
      client = std::filesystem::path(arg);
      clientNext = false;
      continue;
    }
    if (arg == "--no-script") {
      script = false;
      continue;
    }
    if (arg == "--client") {
      clientNext = true;
      continue;
    }
    const auto [end, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), port);
    if (ec != std::errc{} || end != arg.data() + arg.size()) {
      std::println(std::cerr, "skiff::app: not a port: {}", arg);
      return 2;
    }
  }
  if constexpr (requires { A::hooks().dress(); })
    A::hooks().dress();
  asio::io_context io;
  Server<A> server;
  server.script = script;
  server.client = client;
  server.css = skiff::ui::themeCss(skiff::widgets::theme());
  if constexpr (requires { A::hooks().stylesheet(); })
    server.css += std::string(A::hooks().stylesheet());
  asio::co_spawn(io, server.listen(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)), [](std::exception_ptr failed) {
    if (!failed) return;
    try {
      std::rethrow_exception(failed);
    } catch (const std::exception &e) {
      std::println(std::cerr, "skiff::app: {}", e.what());
    }
  });
  io.run();
  return 1;
}
} // namespace skiff::app::serve
