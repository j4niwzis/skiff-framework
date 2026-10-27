// skiff.app.net.native: a runner's transport natively (skiff.app.net) --
// HTTP and HTTPS through Boost.Beast and OpenSSL, on an Asio loop: the
// server's (skiff.app.serve), so that what a served page asks is asked from
// the server. A request a coroutine of its own: its name looked up, the
// connection made and secured (the name told, the certificate checked
// against it and the system's trust), the request written and its answer
// read, all within its timeout; up to five redirects followed. What it got
// kept for its ticket until the runner takes it, and the loop's waiting
// woken (wakes).
module;

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/ssl.h>

export module skiff.app.net.native;

import std;
import skiff.app.net;
import skiff.platform.net;

namespace skiff::app::net::detail {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

// Where a URL points, read once: whether it is secured, the host, its port,
// and the target on it.
struct where {
  bool secured = true;
  std::string host;
  std::string port;
  std::string target = "/";
};
[[nodiscard]] inline std::optional<where> where_of(std::string_view url) {
  where made;
  if (url.starts_with("https://")) {
    url.remove_prefix(8);
    made.port = "443";
  } else if (url.starts_with("http://")) {
    url.remove_prefix(7);
    made.secured = false;
    made.port = "80";
  } else {
    return std::nullopt;
  }
  const std::size_t slash = std::min(url.find('/'), url.size());
  const std::string_view authority = url.substr(0, slash);
  if (slash < url.size()) made.target = std::string(url.substr(slash));
  const std::size_t colon = authority.rfind(':');
  if (colon != std::string_view::npos && authority.find(']') == std::string_view::npos) {
    made.host = std::string(authority.substr(0, colon));
    made.port = std::string(authority.substr(colon + 1));
  } else {
    made.host = std::string(authority);
  }
  if (made.host.empty()) return std::nullopt;
  return made;
}

// What the runner and the coroutines share: the outcomes kept, how many are
// on their way, and the loop's waiting to wake -- let go with the transport,
// while a coroutine may still be finishing.
struct shared {
  std::map<std::uint64_t, outcome> done;
  int running = 0;
  asio::steady_timer *wake = nullptr;
};

template <class Stream>
asio::awaitable<outcome> talk(Stream &stream, const request &r, const where &at) {
  http::request<http::string_body> out;
  out.method_string(name(r.method));
  out.target(at.target);
  out.version(11);
  out.set(http::field::host, at.host);
  out.set(http::field::user_agent, "skiff");
  for (const auto &[header, value] : r.headers) out.set(header, value);
  if (!r.body.empty()) {
    out.set(http::field::content_type, r.type);
    out.body() = r.body;
  }
  out.prepare_payload();
  auto [wrote, written] = co_await http::async_write(stream, out, asio::as_tuple(asio::use_awaitable));
  if (wrote) co_return outcome{.status = 0, .body = "writing to " + at.host + ": " + wrote.message()};
  beast::flat_buffer buffer;
  http::response_parser<http::string_body> parser;
  parser.body_limit(64 * 1024 * 1024);
  auto [read, got] = co_await http::async_read(stream, buffer, parser, asio::as_tuple(asio::use_awaitable));
  if (read) co_return outcome{.status = 0, .body = "reading from " + at.host + ": " + read.message()};
  auto answer = parser.release();
  outcome made{.status = static_cast<int>(answer.result_int()), .body = std::move(answer.body())};
  if (const auto after = answer.find(http::field::retry_after); after != answer.end()) {
    long long seconds = 0;
    const std::string_view said = after->value();
    if (std::from_chars(said.data(), said.data() + said.size(), seconds).ec == std::errc{} && seconds >= 0)
      made.after = std::chrono::milliseconds(seconds * 1000);
  }
  if (const auto to = answer.find(http::field::location); to != answer.end()) made.body = std::string(to->value());
  co_return made;
}

inline asio::awaitable<outcome> once(asio::ssl::context &tls, const request &r, const where &at) {
  const auto executor = co_await asio::this_coro::executor;
  tcp::resolver resolver(executor);
  auto [resolved, found] = co_await resolver.async_resolve(at.host, at.port, asio::as_tuple(asio::use_awaitable));
  if (resolved) co_return outcome{.status = 0, .body = "looking up " + at.host + ": " + resolved.message()};
  beast::tcp_stream plain(executor);
  plain.expires_after(r.timeout);
  auto [connected, to] = co_await plain.async_connect(found, asio::as_tuple(asio::use_awaitable));
  if (connected) co_return outcome{.status = 0, .body = "connecting to " + at.host + ": " + connected.message()};
  if (!at.secured) co_return co_await talk(plain, r, at);
  beast::ssl_stream<beast::tcp_stream> secured(std::move(plain), tls);
  // The name told (SNI), and the certificate checked against it.
  if (!SSL_set_tlsext_host_name(secured.native_handle(), at.host.c_str()))
    co_return outcome{.status = 0, .body = "naming " + at.host + " to it"};
  secured.set_verify_callback(asio::ssl::host_name_verification(at.host));
  auto [shaken] = co_await secured.async_handshake(asio::ssl::stream_base::client, asio::as_tuple(asio::use_awaitable));
  if (shaken) co_return outcome{.status = 0, .body = "securing the connection to " + at.host + ": " + shaken.message()};
  co_return co_await talk(secured, r, at);
}

inline asio::awaitable<void> exchange(std::shared_ptr<shared> kept, std::shared_ptr<asio::ssl::context> tls, std::uint64_t ticket,
                                      request r) {
  ++kept->running;
  outcome got{.status = 0, .body = "not a URL this speaks: " + r.url};
  for (int hop = 0; hop < 6; ++hop) {
    const auto at = where_of(r.url);
    if (!at) break;
    got = co_await once(*tls, r, *at);
    // Moved: followed where it says, the request as it was (a 303 a GET).
    const bool moved = got.status == 301 || got.status == 302 || got.status == 303 || got.status == 307 || got.status == 308;
    if (!moved || got.body.empty()) break;
    r.url = got.body.starts_with('/') ? std::string(at->secured ? "https://" : "http://") + at->host + ":" + at->port + got.body : got.body;
    if (got.status == 303) {
      r.method = method::get{};
      r.body.clear();
    }
  }
  kept->done.insert_or_assign(ticket, std::move(got));
  --kept->running;
  if (kept->wake) kept->wake->cancel();
}

} // namespace skiff::app::net::detail

export namespace skiff::app::net {

class native {
public:
  explicit native(boost::asio::any_io_executor executor)
      : fExecutor(std::move(executor)), fShared(std::make_shared<detail::shared>()),
        fTls(std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_client)) {
    fTls->set_default_verify_paths();
    fTls->set_verify_mode(boost::asio::ssl::verify_peer);
  }
  native(native &&) = default;
  native &operator=(native &&) = default;
  ~native() {
    if (fShared) fShared->wake = nullptr;
  }
  // What waits on the loop for news: woken as an answer comes.
  void wakes(boost::asio::steady_timer *timer) { fShared->wake = timer; }
  void start(std::uint64_t ticket, request r) {
    boost::asio::co_spawn(fExecutor, detail::exchange(fShared, fTls, ticket, std::move(r)), boost::asio::detached);
  }
  [[nodiscard]] std::optional<outcome> take(std::uint64_t ticket) {
    const auto found = fShared->done.find(ticket);
    if (found == fShared->done.end()) return std::nullopt;
    outcome got = std::move(found->second);
    fShared->done.erase(found);
    return got;
  }

private:
  boost::asio::any_io_executor fExecutor;
  std::shared_ptr<detail::shared> fShared;
  std::shared_ptr<boost::asio::ssl::context> fTls;
};

} // namespace skiff::app::net
