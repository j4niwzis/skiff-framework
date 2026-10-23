// What the site keeps: the parts its live examples read and edit -- a
// counter, a list of things to do and the line being typed into it, the
// clock the moving section runs on, and which backend is being shown.
export module site.model;

import std;
import skiff.ui;

export namespace site {

struct Counter {
  int count = 0;
  friend bool operator==(const Counter &, const Counter &) = default;
};

struct Draft {
  std::string text;
  friend bool operator==(const Draft &, const Draft &) = default;
};

struct Todo {
  int id = 0;
  std::string text;
  bool done = false;
  friend bool operator==(const Todo &, const Todo &) = default;
};

// Seconds since the page began, at the last frame; still where nothing runs
// it (the page written when the site is built).
struct Clock {
  double seconds = 0.0;
  friend bool operator==(const Clock &, const Clock &) = default;
};

enum class Backend { kPage, kStatic, kScene };
struct Shown {
  Backend backend = Backend::kPage;
  // Whether the page is alive in the browser (WebAssembly), or the copy of
  // it written when the site was built.
  bool live = false;
  friend bool operator==(const Shown &, const Shown &) = default;
};

struct Root {
  skiff::model::Tracked<Counter> counter;
  skiff::model::Tracked<Draft> draft;
  skiff::model::Keyed<int, Todo> todos;
  skiff::model::Tracked<Clock> clock;
  skiff::model::Tracked<Shown> shown;
};
struct Reactions {};
using Model = skiff::model::Model<Root, Reactions>;

// What the controls send.
struct Increment {};
struct Decrement {};
struct AddTodo {
  std::string text;
};
struct RemoveTodo {
  int id = 0;
};

// The list the page begins with.
inline void seed(Model &model) {
  int id = 0;
  for (const char *text : {"Write the tree", "Bind it to the model", "Ship it as a page"}) {
    (void)model.apply(skiff::model::put<Todo>(id, Todo{id, text, id == 0}));
    ++id;
  }
}

} // namespace site
