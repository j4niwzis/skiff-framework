// skiff.app.store: parts of the model kept where the program runs -- in a
// browser, its localStorage -- through knot: read back as the program
// starts, written again as they change. A part kept is store::Kept<T>, T a
// type knot reads and writes saying where it is kept:
//
//   struct Settings {
//     static constexpr std::string_view key = "settings";
//     bool dark = false;
//     ... json_schema ...
//   };
//   ... bound<store::Kept<Settings>>(view) ...
//
// The framework's store feature; served, nothing is kept (a session lives
// as long as its page).
module;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
export module skiff.app.store;

import std;
import knot;
import skiff.model;

#ifdef __EMSCRIPTEN__
// clang-format off
EM_JS(char *, skiffStoreRead, (const char *key), {
  try {
    const kept = localStorage.getItem('skiff:' + UTF8ToString(key));
    return kept === null ? 0 : stringToNewUTF8(kept);
  } catch (failed) { return 0; }
});
EM_JS(void, skiffStoreWrite, (const char *key, const char *text), {
  try { localStorage.setItem('skiff:' + UTF8ToString(key), UTF8ToString(text)); } catch (failed) {}
});
// clang-format on
#endif

export namespace skiff::app::store {

template <class T> struct Kept {
  T value{};
  friend bool operator==(const Kept &, const Kept &) = default;
};

namespace detail {
template <class Part> struct kept_of {
  using type = std::tuple<>;
};
template <class T> struct kept_of<skiff::model::Tracked<Kept<T>>> {
  using type = std::tuple<std::type_identity<T>>;
};
template <class Root> struct kept_in {
  using type = std::tuple<>;
};
template <class... P> struct kept_in<skiff::model::Assembled<P...>> {
  using type = decltype(std::tuple_cat(std::declval<typename kept_of<P>::type>()...));
};
} // namespace detail

// What keeps a model's Kept parts: read back once, written as they change.
// Where: a browser's localStorage; else nowhere.
template <class Model> class Keeper {
  using Parts = typename detail::kept_in<typename Model::RootType>::type;

public:
  void restore(Model &model) {
    std::apply([&](auto... one) { (this->restore_one(model, one), ...); }, Parts{});
    fWritten = {};
    this->save(model);
  }
  void save(const Model &model) {
    std::size_t at = 0;
    std::apply([&](auto... one) { (this->save_one(model, one, at++), ...); }, Parts{});
  }

private:
  template <class T> void restore_one(Model &model, std::type_identity<T>) {
#ifdef __EMSCRIPTEN__
    const std::string key(T::key);
    char *text = skiffStoreRead(key.c_str());
    if (!text) return;
    const std::string kept(text);
    std::free(text);
    // Read once, here: what comes back as text from the store.
    auto read = knot::try_read<T>(std::string_view(kept));
    if (!read) return;
    (void)model.apply(skiff::model::over<Kept<T>>([value = std::move(*read)](Kept<T> k) {
      k.value = value;
      return k;
    }));
#else
    (void)model;
#endif
  }
  template <class T> void save_one(const Model &model, std::type_identity<T>, std::size_t at) {
    const auto seen = model.template look<Kept<T>>();
    if (!seen || fWritten[at] == seen.fRevision) return;
    fWritten[at] = seen.fRevision;
#ifdef __EMSCRIPTEN__
    const std::string key(T::key);
    skiffStoreWrite(key.c_str(), knot::to_json_string(seen->value).c_str());
#endif
  }

  std::array<std::uint64_t, std::tuple_size_v<Parts>> fWritten{};
};

} // namespace skiff::app::store
