// skiff.app.net: what a program asks of the network, as skiff asks anything
// -- declared in its model, not called for. A request is a part of the model,
// Fetched<Ask>: what is asked (an Ask, the program's own type: where, how,
// and the type its answer is read into), and where it is -- not asked yet,
// on its way, answered, failed. A handler asks by an edit (ask(...)); the
// framework runs what was asked (Runner), on the browser's fetch -- the
// program woken as each answer comes, no loop waiting on it -- or natively
// on the server's loop, tries again where it is worth it -- the network
// failing, the server overloaded (5xx, 429, its Retry-After honoured) -- a
// little longer each time, and writes what came back into the model as an
// edit, read once into the Ask's answer type by knot. The tree, bound to the
// part, shows each state as it shows anything.
//
//   struct Weather { double temp; ... json_schema ... };
//   struct WeatherIn {
//     std::string city;
//     using answer = Weather;               // or net::text: the body as it came
//     net::request request() const { return {.url = "https://.../" + city}; }
//   };
//   ... bound<net::Fetched<WeatherIn>>(view) ...
//   handle<Pressed>(...) -> model::over<net::Fetched<WeatherIn>>(net::ask(WeatherIn{"Oslo"}))
//
// The framework's net feature; without it, nothing of this is built.
module;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
export module skiff.app.net;

import std;
import knot;
import skiff.model;

#ifdef __EMSCRIPTEN__
// ---- the browser's fetch ------------------------------------------------------
// Each request by its ticket: begun, and its outcome kept until it is taken
// -- the status (0: it did not reach the server, or timed out), the body or
// why it failed, and what Retry-After said.
// clang-format off
EM_JS(void, skiffNetStart, (double ticket, const char *method, const char *url, const char *body, int bodyLength,
                            const char *type, const char *headers, int timeoutMs), {
  const net = Module.skiffNet || (Module.skiffNet = {done: {}});
  const asked = new Headers();
  const pairs = UTF8ToString(headers).split('\n');
  for (let i = 0; i + 1 < pairs.length; i += 2) asked.append(pairs[i], pairs[i + 1]);
  const said = bodyLength > 0 ? UTF8ToString(body, bodyLength) : undefined;
  if (said !== undefined) asked.set('Content-Type', UTF8ToString(type));
  const stop = new AbortController();
  const timer = setTimeout(() => stop.abort(), timeoutMs);
  fetch(UTF8ToString(url), {method: UTF8ToString(method), headers: asked, body: said, signal: stop.signal})
    .then(async (answer) => {
      const text = await answer.text();
      const after = Number(answer.headers.get('Retry-After'));
      net.done[ticket] = {status: answer.status, body: text, after: Number.isFinite(after) && after > 0 ? after * 1000 : -1};
    })
    .catch((failed) => { net.done[ticket] = {status: 0, body: String((failed && failed.message) || failed), after: -1}; })
    .finally(() => {
      clearTimeout(timer);
      // The program woken for it: no loop waits on it.
      if (Module._skiff_app_wake) Module._skiff_app_wake();
    });
});
EM_JS(int, skiffNetStatus, (double ticket), {
  const done = Module.skiffNet && Module.skiffNet.done[ticket];
  return done ? done.status : -1;
});
EM_JS(char *, skiffNetBody, (double ticket), { return stringToNewUTF8(Module.skiffNet.done[ticket].body); });
EM_JS(int, skiffNetAfter, (double ticket), { return Module.skiffNet.done[ticket].after; });
EM_JS(void, skiffNetForget, (double ticket), { delete Module.skiffNet.done[ticket]; });
// clang-format on
#endif

export namespace skiff::app::net {

// How a request asks: read off it once, where it is written.
namespace method {
struct get {};
struct post {};
struct put {};
struct patch {};
struct remove {};
} // namespace method
using method_t = std::variant<method::get, method::post, method::put, method::patch, method::remove>;
[[nodiscard]] constexpr std::string_view name(method::get) { return "GET"; }
[[nodiscard]] constexpr std::string_view name(method::post) { return "POST"; }
[[nodiscard]] constexpr std::string_view name(method::put) { return "PUT"; }
[[nodiscard]] constexpr std::string_view name(method::patch) { return "PATCH"; }
[[nodiscard]] constexpr std::string_view name(method::remove) { return "DELETE"; }
[[nodiscard]] inline std::string_view name(const method_t &m) {
  return std::visit([](auto one) { return name(one); }, m);
}

// What is sent: where (http:// or https://), how, what with, and how long
// it is waited for, each time it is tried.
struct request {
  method_t method = method::get{};
  std::string url;
  std::string body;
  std::string type = "application/json";
  std::vector<std::pair<std::string, std::string>> headers;
  std::chrono::milliseconds timeout{30000};
};
// How often it is tried, and how long between: the first wait, doubled
// each time, to a most.
struct retry {
  int attempts = 4;
  std::chrono::milliseconds first{500};
  std::chrono::milliseconds most{8000};
};
// An answer kept as it came: an Ask whose answer is this reads nothing.
struct text {
  std::string body;
  friend bool operator==(const text &, const text &) = default;
};

// Where a request is.
namespace state {
struct idle {
  friend bool operator==(const idle &, const idle &) = default;
};
// On its way: the how-manyeth try.
struct loading {
  int attempt = 1;
  friend bool operator==(const loading &, const loading &) = default;
};
template <class T> struct ok {
  T value;
  int status = 200;
  friend bool operator==(const ok &, const ok &) = default;
};
// Given up on: why, and the server's status where it answered.
struct failed {
  std::string why;
  int status = 0;
  friend bool operator==(const failed &, const failed &) = default;
};
} // namespace state

// A request, as a part of the model: what is asked, which time (asked
// again: one more), and where it is. Bound to a view, each state shown.
template <class Ask> struct Fetched {
  using Answer = typename Ask::answer;
  using State = std::variant<state::idle, state::loading, state::ok<Answer>, state::failed>;
  std::optional<Ask> asked;
  std::uint64_t round = 0;
  State now = state::idle{};
};
// The edit a handler answers to ask: model::over<Fetched<Ask>>(ask(a)). The
// same asked again is asked again.
template <class Ask> [[nodiscard]] auto ask(Ask a) {
  return [a = std::move(a)](Fetched<Ask> f) {
    f.asked = a;
    ++f.round;
    return f;
  };
}

// What came back of one try: the status (0: the server was not reached, or
// not in time), the body -- or why it failed -- and what Retry-After said.
struct outcome {
  int status = 0;
  std::string body;
  std::optional<std::chrono::milliseconds> after;
};

// An answer read into its type, once: a body kept as text, else read by knot.
[[nodiscard]] inline std::optional<text> answer_of(std::type_identity<text>, std::string body) { return text{std::move(body)}; }
template <class T> [[nodiscard]] std::optional<T> answer_of(std::type_identity<T>, const std::string &body) {
  auto read = knot::try_read<T>(std::string_view(body));
  if (!read) return std::nullopt;
  return std::optional<T>(std::move(*read));
}
// An Ask's retries: its own, where it says them.
template <class Ask> [[nodiscard]] retry retries_of(const Ask &) { return {}; }
template <class Ask>
  requires requires(const Ask &a) {
    { a.retries() } -> std::convertible_to<retry>;
  }
[[nodiscard]] retry retries_of(const Ask &a) {
  return a.retries();
}

namespace detail {
// One Ask's request as the runner follows it: the round it is for, the try,
// the ticket on its way, and when it is tried again.
template <class Ask> struct flight {
  std::uint64_t round = 0;
  int attempt = 0;
  std::uint64_t ticket = 0;
  std::optional<std::chrono::steady_clock::time_point> again;
};
template <class Part> struct flight_of {
  using type = std::tuple<>;
};
template <class Ask> struct flight_of<skiff::model::Tracked<Fetched<Ask>>> {
  using type = std::tuple<flight<Ask>>;
};
template <class Root> struct flights_of {
  using type = std::tuple<>;
};
template <class... P> struct flights_of<skiff::model::Assembled<P...>> {
  using type = decltype(std::tuple_cat(std::declval<typename flight_of<P>::type>()...));
};
} // namespace detail

// What runs a model's requests: each part Fetched<Ask> of its root, asked
// and not yet on its way, sent; its outcome written back; tried again where
// it is worth it. Stepped by the framework at every frame (in a browser) or
// whenever its loop wakes (served). Transport: what sends -- browser (fetch)
// or native (skiff.app.net.native), or a test's.
template <class Model, class Transport> class Runner {
public:
  explicit Runner(Transport transport) : fTransport(std::move(transport)) {}
  [[nodiscard]] Transport &transport() noexcept { return fTransport; }
  // Whether anything is on its way or waits to be tried again.
  [[nodiscard]] bool busy() const {
    return std::apply([](const auto &...one) { return ((one.ticket != 0 || one.again.has_value()) || ... || false); }, fFlights);
  }
  void step(Model &model) {
    std::apply([&](auto &...one) { (this->follow(model, one), ...); }, fFlights);
  }
  // When the soonest request waiting to be tried again is to be: what to
  // be woken for.
  [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> next() const {
    std::optional<std::chrono::steady_clock::time_point> soonest;
    const auto take = [&](const auto &one) {
      if (one.again && (!soonest || *one.again < *soonest)) soonest = one.again;
    };
    std::apply([&](const auto &...one) { (take(one), ...); }, fFlights);
    return soonest;
  }

private:
  using Clock = std::chrono::steady_clock;
  template <class Ask> void set(Model &model, typename Fetched<Ask>::State now) {
    (void)model.apply(skiff::model::over<Fetched<Ask>>([now = std::move(now)](Fetched<Ask> f) {
      f.now = now;
      return f;
    }));
  }
  template <class Ask> void send(detail::flight<Ask> &f, const Ask &ask) {
    f.ticket = ++fTickets;
    fTransport.start(f.ticket, ask.request());
  }
  template <class Ask> void follow(Model &model, detail::flight<Ask> &f) {
    const auto seen = model.template look<Fetched<Ask>>();
    if (!seen || !seen->asked) return;
    const Ask ask = *seen->asked;
    // Asked anew: what was on its way for the round before let go.
    if (seen->round != f.round) {
      f = detail::flight<Ask>{.round = seen->round, .attempt = 1};
      this->send(f, ask);
      this->set<Ask>(model, state::loading{1});
      return;
    }
    if (f.again && Clock::now() >= *f.again) {
      f.again.reset();
      this->send(f, ask);
    }
    if (f.ticket == 0) return;
    std::optional<outcome> got = fTransport.take(f.ticket);
    if (!got) return;
    f.ticket = 0;
    this->settle(model, f, ask, std::move(*got));
  }
  template <class Ask> void settle(Model &model, detail::flight<Ask> &f, const Ask &ask, outcome got) {
    using Answer = typename Ask::answer;
    if (got.status >= 200 && got.status < 300) {
      if (auto answer = answer_of(std::type_identity<Answer>{}, std::move(got.body)))
        this->set<Ask>(model, state::ok<Answer>{std::move(*answer), got.status});
      else
        this->set<Ask>(model, state::failed{"the answer was not understood", got.status});
      return;
    }
    // Worth another try: the server not reached, overloaded, or failing.
    const bool again = got.status == 0 || got.status == 429 || got.status >= 500;
    const retry policy = retries_of(ask);
    if (again && f.attempt < policy.attempts) {
      const auto doubled = policy.first * (1LL << std::min(f.attempt - 1, 20));
      const auto wait = got.after.value_or(std::min<std::chrono::milliseconds>(doubled, policy.most));
      f.again = Clock::now() + wait;
      ++f.attempt;
      this->set<Ask>(model, state::loading{f.attempt});
      return;
    }
    this->set<Ask>(model, state::failed{got.status == 0 ? got.body : "the server answered " + std::to_string(got.status), got.status});
  }

  Transport fTransport;
  std::uint64_t fTickets = 0;
  typename detail::flights_of<typename Model::RootType>::type fFlights;
};

#ifdef __EMSCRIPTEN__
// The browser's fetch, as a runner's transport.
struct browser {
  void start(std::uint64_t ticket, const request &r) {
    const std::string headers = r.headers | std::views::transform([](const auto &one) { return one.first + "\n" + one.second; }) |
                                std::views::join_with('\n') | std::ranges::to<std::string>();
    const std::string method(name(r.method));
    skiffNetStart(static_cast<double>(ticket), method.c_str(), r.url.c_str(), r.body.data(), static_cast<int>(r.body.size()),
                  r.type.c_str(), headers.c_str(), static_cast<int>(r.timeout.count()));
  }
  [[nodiscard]] std::optional<outcome> take(std::uint64_t ticket) {
    const double at = static_cast<double>(ticket);
    const int status = skiffNetStatus(at);
    if (status < 0) return std::nullopt;
    char *body = skiffNetBody(at);
    outcome got{.status = status, .body = std::string(body)};
    std::free(body);
    if (const int after = skiffNetAfter(at); after >= 0) got.after = std::chrono::milliseconds(after);
    skiffNetForget(at);
    return got;
  }
};
#endif

} // namespace skiff::app::net
