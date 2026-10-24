// skiff.app.route: where the program is, as its URL says it -- read once into
// the program's own types, and kept as a part of its model. Routes, a
// std::variant of route types, each saying how a path reads into it and what
// path it is:
//
//   struct Home {
//     static std::optional<Home> parse(std::string_view path) { return path.empty() ? std::optional(Home{}) : std::nullopt; }
//     std::string path() const { return ""; }
//   };
//   struct User {
//     int id;
//     static std::optional<User> parse(std::string_view path);  // "users/42", with route::segments
//     std::string path() const { return std::format("users/{}", id); }
//   };
//   using Routes = std::variant<Home, User>;
//   ... bound<route::At<Routes>>(view) ...   model::over<route::At<Routes>>(route::go<Routes>(User{42}))
//
// Paths are the app's own, relative to where it is served from (its base):
// no leading slash, "" the first page -- a link's href is one as it is.
// The framework keeps the part and the URL one: in a browser the address
// bar follows the part (the History API) and the part the address bar (back
// and forward, a link to a route clicked); served, the path asked for is the
// part a session begins with -- a link a request of its own, with no script.
export module skiff.app.route;

import std;
import skiff.model;

export namespace skiff::app::route {

// The part: where the program is.
template <class Routes> struct At {
  Routes now{};
  friend bool operator==(const At &, const At &) = default;
};
// The edit a handler answers to go somewhere.
template <class Routes, class Route> [[nodiscard]] auto go(Route to) {
  return [to = std::move(to)](At<Routes> at) {
    at.now = Routes(to);
    return at;
  };
}

// A path's segments, its slashes taken out: what a route reads itself from.
[[nodiscard]] inline std::vector<std::string_view> segments(std::string_view path) {
  return path | std::views::split('/') | std::views::transform([](auto one) { return std::string_view(one.begin(), one.end()); }) |
         std::views::filter([](std::string_view one) { return !one.empty(); }) | std::ranges::to<std::vector>();
}
// A path as a route reads it: without the query or fragment, without the
// slashes at its ends.
[[nodiscard]] inline std::string_view trimmed(std::string_view path) {
  path = path.substr(0, std::min(path.find_first_of("?#"), path.size()));
  while (path.starts_with('/')) path.remove_prefix(1);
  while (path.ends_with('/')) path.remove_suffix(1);
  return path;
}

// The first route that reads the path; none where none does.
template <class Routes> [[nodiscard]] std::optional<Routes> parse(std::string_view path) {
  const std::string_view mine = trimmed(path);
  std::optional<Routes> found;
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    ((found ? void() : void([&] {
       if (auto one = std::variant_alternative_t<I, Routes>::parse(mine)) found = Routes(std::in_place_index<I>, std::move(*one));
     }())),
     ...);
  }(std::make_index_sequence<std::variant_size_v<Routes>>{});
  return found;
}
// The path a route is.
template <class Routes> [[nodiscard]] std::string path_of(const Routes &where) {
  return std::visit([](const auto &one) { return std::string(one.path()); }, where);
}

namespace detail {
template <class Part> struct routes_of_part {
  using type = std::tuple<>;
};
template <class Routes> struct routes_of_part<skiff::model::Tracked<At<Routes>>> {
  using type = std::tuple<Routes>;
};
template <class Root> struct routes_in {
  using type = std::tuple<>;
};
template <class... P> struct routes_in<skiff::model::Assembled<P...>> {
  using type = decltype(std::tuple_cat(std::declval<typename routes_of_part<P>::type>()...));
};
} // namespace detail

// What keeps a model's part and its URL one: the model's At<Routes>, where
// its root has one (the first, where it has several); else nothing to keep.
template <class Model> class Router {
  using Found = typename detail::routes_in<typename Model::RootType>::type;

public:
  static constexpr bool kRoutes = std::tuple_size_v<Found> > 0;
  // A path come -- the one served, the address bar's, a link's: the part
  // set to the route it reads, where one does. Whether one did.
  bool arrive(Model &model, std::string_view path) {
    if constexpr (kRoutes) {
      using Routes = std::tuple_element_t<0, Found>;
      auto read = parse<Routes>(path);
      if (!read) return false;
      fShown = path_of(*read);
      (void)model.apply(skiff::model::over<At<Routes>>([to = std::move(*read)](At<Routes> at) {
        at.now = to;
        return at;
      }));
      return true;
    } else {
      (void)model;
      (void)path;
      return false;
    }
  }
  // Whether a path is one of the program's routes: a link to it is the
  // program's to follow, not the browser's.
  [[nodiscard]] static bool known(std::string_view path) {
    if constexpr (kRoutes)
      return parse<std::tuple_element_t<0, Found>>(path).has_value();
    else
      return false;
  }
  // Where the part has gone since the URL was last said: the path to say
  // now (pushed to the address bar); none where it has not moved.
  [[nodiscard]] std::optional<std::string> moved(const Model &model) {
    if constexpr (kRoutes) {
      using Routes = std::tuple_element_t<0, Found>;
      const auto seen = model.template look<At<Routes>>();
      if (!seen) return std::nullopt;
      std::string now = path_of(seen->now);
      if (fShown && *fShown == now) return std::nullopt;
      fShown = now;
      return now;
    } else {
      (void)model;
      return std::nullopt;
    }
  }

private:
  std::optional<std::string> fShown;
};

} // namespace skiff::app::route
