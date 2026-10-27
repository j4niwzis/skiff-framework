// skiff.app.net's runner, with a transport of the test's own: a request
// asked by an edit goes out, an overloaded server's answer is tried again,
// the answer read into its type and written into the model; one refused for
// good fails at once; one asked again goes out again.
import std;
import knot;
import skiff.model;
import skiff.app.net;

// Method tags must be usable through the public app.net API alone.
static_assert(skiff::app::net::name(skiff::app::net::method::get{}) == "GET");
static_assert(skiff::app::net::name(skiff::app::net::method::post{}) == "POST");

namespace probe {
struct Count {
  int n = 0;
  friend bool operator==(const Count &, const Count &) = default;
};
consteval auto json_schema(knot::type<Count>) { return knot::schema<Count>(); }
struct Ask {
  std::string path;
  using answer = Count;
  skiff::app::net::request request() const { return {.url = "http://127.0.0.1" + path}; }
  skiff::app::net::retry retries() const { return {.attempts = 3, .first = std::chrono::milliseconds(1), .most = std::chrono::milliseconds(2)}; }
  friend bool operator==(const Ask &, const Ask &) = default;
};
// What answers, in turn: each request takes the next.
struct Scripted {
  std::vector<skiff::app::net::outcome> *answers;
  std::vector<std::string> *asked;
  std::map<std::uint64_t, skiff::app::net::outcome> started;
  void start(std::uint64_t ticket, const skiff::app::net::request &r) {
    asked->push_back(r.url);
    started.insert_or_assign(ticket, answers->front());
    answers->erase(answers->begin());
  }
  std::optional<skiff::app::net::outcome> take(std::uint64_t ticket) {
    const auto found = started.find(ticket);
    if (found == started.end()) return std::nullopt;
    auto got = found->second;
    started.erase(found);
    return got;
  }
};
struct Reactions {};
using Root = skiff::model::Assembled<skiff::model::Tracked<skiff::app::net::Fetched<Ask>>>;
using Model = skiff::model::Model<Root, Reactions>;
using Fetched = skiff::app::net::Fetched<Ask>;
} // namespace probe

// The state wanted, where it is the one the part is in: picked by overload.
template <class Want> struct Picks {
  const Want *operator()(const Want &one) const { return &one; }
  template <class Other> const Want *operator()(const Other &) const { return nullptr; }
};
template <class Want> const Want *in(const probe::Model &m) { return std::visit(Picks<Want>{}, m.look<probe::Fetched>()->now); }

// Stepped until it settles, or a while.
template <class Runner> void settle(Runner &runner, probe::Model &m) {
  for (int i = 0; i < 400 && runner.busy() | (i == 0); ++i) {
    runner.step(m);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
int failures = 0;
void check(bool ok, std::string_view what) {
  std::println("{} {}", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  using namespace skiff::app::net;
  std::vector<outcome> answers{{.status = 503, .body = "busy"}, {.status = 200, .body = R"({"n":42})"},
                               {.status = 404, .body = "no"}, {.status = 200, .body = R"({"n":7})"}};
  std::vector<std::string> asked;
  probe::Model m;
  Runner<probe::Model, probe::Scripted> runner(probe::Scripted{&answers, &asked, {}});
  runner.step(m);
  check(asked.empty(), "nothing asked, nothing sent");

  (void)m.apply(skiff::model::over<probe::Fetched>(ask(probe::Ask{"/count"})));
  runner.step(m);
  check(in<state::loading>(m) != nullptr, "asked: on its way");
  settle(runner, m);
  const auto *ok = in<state::ok<probe::Count>>(m);
  check(ok && ok->value.n == 42, "a 503 tried again, the answer read into its type");
  check(asked.size() == 2, "two tries");

  (void)m.apply(skiff::model::over<probe::Fetched>(ask(probe::Ask{"/gone"})));
  settle(runner, m);
  const auto *failed = in<state::failed>(m);
  check(failed && failed->status == 404, "a 404 not tried again: failed");
  check(asked.size() == 3, "one try");

  (void)m.apply(skiff::model::over<probe::Fetched>(ask(probe::Ask{"/gone"})));
  settle(runner, m);
  const auto *again = in<state::ok<probe::Count>>(m);
  check(again && again->value.n == 7, "the same asked again: asked again");
  check(asked.back() == "http://127.0.0.1/gone", "where it was asked");
  return failures == 0 ? 0 : 1;
}
