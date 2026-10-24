// skiff.app.net natively: a request asked by the model goes out through
// Beast to a server of the test's own on this machine, which is overloaded
// once (503, Retry-After 0) and then answers; tried again, and the answer
// read into the model.
module;
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
export module skiff_framework_net_native_test;
import std;
import knot;
import skiff.model;
import skiff.app.net;
import skiff.app.net.native;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

namespace probe {
struct Count {
  int n = 0;
};
consteval auto json_schema(knot::type<Count>) { return knot::schema<Count>(); }
struct Ask {
  std::string url;
  using answer = Count;
  skiff::app::net::request request() const { return {.url = url, .timeout = std::chrono::milliseconds(5000)}; }
  skiff::app::net::retry retries() const { return {.attempts = 3, .first = std::chrono::milliseconds(10), .most = std::chrono::milliseconds(20)}; }
};
struct Reactions {};
using Model = skiff::model::Model<skiff::model::Assembled<skiff::model::Tracked<skiff::app::net::Fetched<Ask>>>, Reactions>;
using Fetched = skiff::app::net::Fetched<Ask>;
template <class Want> struct Picks {
  const Want *operator()(const Want &one) const { return &one; }
  template <class Other> const Want *operator()(const Other &) const { return nullptr; }
};
} // namespace probe

// The server: the first request refused as overloaded, the rest answered.
asio::awaitable<void> serve(asio::ip::tcp::acceptor &acceptor, int &served) {
  for (;;) {
    auto socket = co_await acceptor.async_accept(asio::use_awaitable);
    beast::tcp_stream stream(std::move(socket));
    beast::flat_buffer buffer;
    http::request<http::string_body> asked;
    co_await http::async_read(stream, buffer, asked, asio::use_awaitable);
    http::response<http::string_body> answer{++served == 1 ? http::status::service_unavailable : http::status::ok, 11};
    if (served == 1) answer.set(http::field::retry_after, "0");
    answer.body() = served == 1 ? "busy" : R"({"n":42})";
    answer.keep_alive(false);
    answer.prepare_payload();
    co_await http::async_write(stream, answer, asio::use_awaitable);
  }
}

export int run() {
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io, {asio::ip::make_address("127.0.0.1"), 0});
  int served = 0;
  asio::co_spawn(io, serve(acceptor, served), asio::detached);
  const std::string url = std::format("http://127.0.0.1:{}/count", acceptor.local_endpoint().port());
  probe::Model m;
  skiff::app::net::Runner<probe::Model, skiff::app::net::native> runner(skiff::app::net::native(io.get_executor()));
  (void)m.apply(skiff::model::over<probe::Fetched>(skiff::app::net::ask(probe::Ask{url})));
  for (int i = 0; i < 500; ++i) {
    runner.step(m);
    io.run_for(std::chrono::milliseconds(10));
    if (std::visit(probe::Picks<skiff::app::net::state::ok<probe::Count>>{}, m.look<probe::Fetched>()->now)) break;
  }
  const auto *got = std::visit(probe::Picks<skiff::app::net::state::ok<probe::Count>>{}, m.look<probe::Fetched>()->now);
  std::println("served {} times; {}", served, got ? std::format("answered {}", got->value.n) : std::string("not answered"));
  return got && got->value.n == 42 && served == 2 ? 0 : 1;
}
