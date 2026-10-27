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
import skiff.platform.net;

export namespace skiff::app::net {

namespace method = skiff::platform::net::method;
using skiff::platform::net::method_t;
using skiff::platform::net::name;
using skiff::platform::net::request;
using skiff::platform::net::outcome;
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
using browser = skiff::platform::net::browser;
#endif

} // namespace skiff::app::net
