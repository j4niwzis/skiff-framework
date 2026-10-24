// skiff.app.route: a path read into the program's routes and said back; the
// part kept by a Router, which says where it has gone since.
import std;
import skiff.model;
import skiff.app.route;

namespace probe {
struct Home {
  static std::optional<Home> parse(std::string_view path) { return path.empty() ? std::optional(Home{}) : std::nullopt; }
  std::string path() const { return ""; }
  friend bool operator==(const Home &, const Home &) = default;
};
struct User {
  int id = 0;
  static std::optional<User> parse(std::string_view path) {
    const auto parts = skiff::app::route::segments(path);
    int id = 0;
    if (parts.size() != 2 || parts[0] != "users" || std::from_chars(parts[1].data(), parts[1].data() + parts[1].size(), id).ec != std::errc{})
      return std::nullopt;
    return User{id};
  }
  std::string path() const { return std::format("users/{}", id); }
  friend bool operator==(const User &, const User &) = default;
};
using Routes = std::variant<Home, User>;
struct Reactions {};
using Model = skiff::model::Model<skiff::model::Assembled<skiff::model::Tracked<skiff::app::route::At<Routes>>>, Reactions>;
} // namespace probe

int failures = 0;
void check(bool ok, std::string_view what) {
  std::println("{} {}", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  using namespace skiff::app::route;
  check(parse<probe::Routes>("/users/42/") == probe::Routes(probe::User{42}), "a path read into its route");
  check(parse<probe::Routes>("") == probe::Routes(probe::Home{}), "the first page");
  check(!parse<probe::Routes>("users/x"), "no route reads it: none");
  check(path_of(probe::Routes(probe::User{7})) == "users/7", "a route said as its path");
  probe::Model m;
  Router<probe::Model> router;
  check(router.arrive(m, "users/3") && m.look<At<probe::Routes>>()->now == probe::Routes(probe::User{3}), "arrived: the part set");
  check(!router.moved(m), "where it arrived: nothing to say");
  (void)m.apply(skiff::model::over<At<probe::Routes>>(go<probe::Routes>(probe::Home{})));
  const auto moved = router.moved(m);
  check(moved && moved->empty(), "gone home: its path to say");
  check(!router.moved(m), "said once");
  check(Router<probe::Model>::known("users/1") && !Router<probe::Model>::known("about"), "known routes");
  return failures == 0 ? 0 : 1;
}
