// The site of skiff, written with skiff: one tree in skiff.ui, said once,
// compiled once, and mounted twice by the program (main.cc) -- on the page,
// skiff.web's nodes, the browser's elements laid out by the browser; and on
// the scene, skiff's own nodes laid out by skiff and drawn on a canvas. One
// model under both: the parts the tree's views say they keep (state<T>),
// each said here beside what reads it.
module;
#include <skiff/named.h>
export module site.page;

import std;
import skiff.ui;

namespace site {
using namespace skiff;
using namespace skiff::ui;
namespace dom = skiff::web::dom;
} // namespace site

// Exported, every piece: kept to the module, clang 23 recursed without end
// looking for their definitions from the unit importing the page.
export namespace site {

namespace colour {
inline constexpr scene::Colour kPaper = 0xfff4efe6u;  // sailcloth
inline constexpr scene::Colour kInk = 0xff1d2b36u;    // the sea at night
inline constexpr scene::Colour kSea = 0xff2a7f8fu;
inline constexpr scene::Colour kCoral = 0xffd9603bu;
inline constexpr scene::Colour kAmber = 0xffe18a32u;
inline constexpr scene::Colour kSand = 0xffe7c79bu;
inline constexpr scene::Colour kMuted = 0xff5a6b77u;
inline constexpr scene::Colour kFoam = 0xffffffffu;
inline constexpr scene::Colour kCodeInk = 0xffe8eef2u;
inline constexpr scene::Colour kPanel = 0xffebe3d7u;
} // namespace colour

// C++ as an editor colours it, for either backend's Code: the code cut into
// runs of a kind -- "kw", "name", "str", "num", "com", or "" for the rest.
[[nodiscard]] inline std::vector<std::pair<std::string_view, std::string_view>> codeTokens(std::string_view code) {
  static constexpr std::array<std::string_view, 24> kKeywords{
      "auto", "return", "const", "constexpr", "struct", "class", "template", "typename", "using", "namespace", "if", "else",
      "for", "while", "static", "inline", "void", "bool", "int", "float", "true", "false", "import", "this"};
  std::vector<std::pair<std::string_view, std::string_view>> out;
  const auto add = [&](std::string_view kind, std::size_t from, std::size_t to) {
    if (to <= from) return;
    if (kind.empty() && !out.empty() && out.back().first.empty())
      out.back().second = code.substr(out.back().second.data() - code.data(), to - (out.back().second.data() - code.data()));
    else
      out.emplace_back(kind, code.substr(from, to - from));
  };
  const auto word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
  std::size_t i = 0;
  while (i < code.size()) {
    const char c = code[i];
    if (code.substr(i, 2) == "//") {
      const std::size_t end = std::min(code.find('\n', i), code.size());
      add("com", i, end);
      i = end;
    } else if (c == '"' || c == '\'') {
      std::size_t end = i + 1;
      while (end < code.size() && code[end] != c) end += code[end] == '\\' ? 2 : 1;
      end = std::min(end + 1, code.size());
      add("str", i, end);
      i = end;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t end = i;
      while (end < code.size() && (word(code[end]) || code[end] == '.')) ++end;
      add("num", i, end);
      i = end;
    } else if (word(c)) {
      std::size_t end = i;
      while (end < code.size() && word(code[end])) ++end;
      const std::string_view w = code.substr(i, end - i);
      std::size_t next = end;
      while (next < code.size() && code[next] == ' ') ++next;
      const bool called = next < code.size() && (code[next] == '(' || code[next] == '<');
      add(std::ranges::contains(kKeywords, w) ? "kw" : (called || std::isupper(static_cast<unsigned char>(w[0]))) ? "name" : "", i, end);
      i = end;
    } else {
      add("", i, i + 1);
      ++i;
    }
  }
  return out;
}

// ---- the nodes the site has of its own ----

// Code, as it is written -- its lines kept -- and coloured as an editor
// colours C++: each word, string, number and comment in the colour of its
// kind. A node of the site's own, said for each backend.
// On the page: a <pre> of spans, a span for each run, of its kind.
struct PageCode : skiff::web::Node {
  explicit PageCode(std::string code) : fCode(std::move(code)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "pre"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "site-code");
    for (const auto &[kind, text] : codeTokens(fCode)) {
      const dom::Handle span = dom::create("span");
      if (!kind.empty()) dom::attr(span, "class", std::string("c-").append(kind));
      dom::text(span, text);
      dom::append(fState.fHandle, span);
    }
  }
  std::string fCode;
};
// The colour a run of code is drawn in, by its kind (the page's .c-*).
[[nodiscard]] inline scene::Colour codeColour(std::string_view kind) {
  if (kind == "kw") return 0xffff9d76u;
  if (kind == "name") return 0xff8fd3e0u;
  if (kind == "str") return 0xffe7c79bu;
  if (kind == "num") return 0xfff2b5d4u;
  if (kind == "com") return 0xff7d8f9bu;
  return colour::kCodeInk;
}
// On the scene: drawn by the painter run by run, its lines kept, as wide
// as what holds it and cut there.
class SceneCode : public skiff::scene::Node {
public:
  static constexpr float kSize = 13.5f;
  static constexpr float kLine = kSize * 1.55f;
  explicit SceneCode(std::string code) : fCode(std::move(code)) {
    while (!fCode.empty() && fCode.back() == '\n') fCode.pop_back();
    const auto lines = static_cast<float>(1 + std::ranges::count(fCode, '\n'));
    this->apply(scene::Spec{.fillX = true, .height = lines * kLine + kSize * 0.4f});
  }
  template <class Hooks, class P> void drawSelf(this auto &self, Hooks &hooks, P &p, float alpha) {
    const skia::SkRect &box = self.fState.fBounds;
    p.save();
    p.clipRect(box);
    float x = box.left();
    float y = box.top() + kSize;
    for (const auto &[kind, run] : codeTokens(self.fCode)) {
      const scene::Colour ink = codeColour(kind);
      std::size_t from = 0;
      for (;;) {
        const std::size_t end = run.find('\n', from);
        const std::string piece(run.substr(from, end == std::string_view::npos ? std::string_view::npos : end - from));
        if (!piece.empty()) {
          p.text(piece, x, y, kSize, ink, alpha, false);
          x += p.measure(piece, kSize, false);
        }
        if (end == std::string_view::npos) break;
        x = box.left();
        y += kLine;
        from = end + 1;
      }
    }
    p.restore();
    skiff::scene::detail::paintOver(hooks, self.fState, p, alpha);
  }
  std::string fCode;
};
// Either, as it is mounted.
struct Code {
  std::string fCode;
  explicit Code(std::string code) : fCode(std::move(code)) {}
  auto on(OnPage) && { return PageCode(std::move(fCode)); }
  auto on(OnScene) && { return SceneCode(std::move(fCode)); }
};

// A text that reads its part of the model: its words made of it.
template <class T, class F> auto label(F format, float size = 16.0f, scene::Colour c = colour::kInk, bool bold = false) {
  Text t(std::string(), size, c, bold);
  t.setWrapped(true);
  return text_for<T>(std::move(format), std::move(t));
}

// ---- the pieces of the page ----

[[nodiscard]] Text title(std::string s, float size = 40.0f, scene::Colour c = colour::kInk) { return Text(std::move(s), size, c, true); }
[[nodiscard]] Text words(std::string s, float size = 18.0f, scene::Colour c = colour::kInk, bool bold = false) {
  Text t(std::move(s), size, c, bold);
  t.setWrapped(true);
  return t;
}
// A section: as wide as the page lets it, at most 1100 across, in the middle.
template <class... Parts> auto section(std::string_view /*id*/, scene::Colour background, Parts... parts) {
  return column(vbox(0.0f, {.fillX = true, .autoSize = scene::axes::kY, .padding = {48.0f, 24.0f, 48.0f, 24.0f}, .background = background}),
                column(vbox(28.0f, {.fillX = true, .autoSize = scene::axes::kY, .maxWidth = 1100.0f, .alignSelf = scene::align::kMiddle}),
                       std::move(parts)...));
}
// Two sides that sit beside each other where there is room, one over the
// other where there is not.
template <class... Parts> auto sides(Parts... parts) {
  auto flow = Flow(scene::FlowOptions{.direction = scene::direction::horizontal{}, .spacingX = 40.0f, .spacingY = 28.0f, .wrap = true},
                   std::move(parts)...);
  flow.apply({.fillX = true, .autoSize = scene::axes::kY});
  return flow;
}
template <class... Parts> auto side(Parts... parts) {
  return column(vbox(14.0f, {.autoSize = scene::axes::kY, .grow = scene::axes::kX, .minWidth = 300.0f}), std::move(parts)...);
}
template <class... Parts> auto card(scene::Colour background, Parts... parts) {
  return column(vbox(12.0f, {.fillX = true, .autoSize = scene::axes::kY, .padding = scene::Margin::all(20.0f), .cornerRadius = 12.0f,
                             .background = background}),
                std::move(parts)...);
}
auto pill(std::string text, scene::Colour plate, scene::Colour ink = colour::kFoam) {
  return row(hbox(0.0f, {.autoSize = scene::axes::kBoth, .padding = {8.0f, 14.0f, 8.0f, 14.0f}, .cornerRadius = 18.0f, .background = plate}),
             Text(std::move(text), 17.0f, ink, true));
}

auto header() {
  return SKIFF_NAMED(Header, row(hbox(24.0f, {.fillX = true, .autoSize = scene::axes::kY, .padding = {14.0f, 24.0f, 14.0f, 24.0f}, .background = colour::kInk}),
             Text("skiff", 26.0f, colour::kSand, true), styled({.grow = scene::axes::kX}, Box<>(scene::Colour{0})),
             Link("Docs", "https://github.com/j4niwzis/skiff#readme", 17.0f, colour::kPaper),
             Link("Widgets", "https://github.com/j4niwzis/skiff-widgets", 17.0f, colour::kPaper),
             Link("Framework", "https://github.com/j4niwzis/skiff-framework", 17.0f, colour::kPaper),
             Link("GitHub", "https://github.com/j4niwzis/skiff", 17.0f, colour::kSand, true)));
}

// Which backend is shown -- kept by the section that chooses it, read by the
// hero too -- and whether the page is alive yet.
enum class Backend { kPage, kScene };
struct Shown {
  Backend backend = Backend::kPage;
  // Whether the page is alive in the browser (WebAssembly), or the copy of
  // it written when the site was built.
  bool live = false;
  friend bool operator==(const Shown &, const Shown &) = default;
};

// Which backend this is, from the hero: what each says of itself.
[[nodiscard]] inline std::string whoIsLayingOut(const Shown &s) {
  return s.live ? "skiff is laying out this page right now — every element you see is a node of a C++ tree, compiled to WebAssembly and "
                  "bound to a model."
                : "skiff wrote this page when the site was built — the same C++ tree, mounted on a document kept in memory and saved as "
                  "HTML. The live one takes over as soon as its WebAssembly has loaded.";
}

auto hero() {
  return SKIFF_NAMED(Hero, section("top", colour::kPaper,
                 sides(side(title("A C++ UI library: a typed tree, a model it is bound to, and two backends.", 52.0f),
                            either(label<Shown>(whoIsLayingOut, 26.0f, colour::kCoral, true),
                                   words("skiff is drawing this page right now, on a canvas \u2014 laid out by skiff's own scene, not by "
                                         "the browser, and drawn through its painter.",
                                         26.0f, colour::kCoral, true))),
                       column(vbox(12.0f, {.autoSize = scene::axes::kBoth, .alignSelf = scene::align::kMiddle}), pill("C++23 modules", colour::kSea),
                              pill("One tree, two backends", colour::kCoral), pill("A model you edit, a view that follows", colour::kInk),
                              pill("No virtual calls in a release build", colour::kSea), pill("Tests without a browser", colour::kCoral)))));
}

auto features() {
  return SKIFF_NAMED(Features, section("features", colour::kInk,
                 sides(side(Code("import skiff.ui;"), words("Modules all the way down: no headers to keep in step, one macro (SKIFF_NAMED).", 18.0f,
                                                                 colour::kPaper),
                            words("The tree is its type: what a node holds, how it is laid out and what it reads are known when it compiles.", 18.0f,
                                  colour::kPaper)),
                       side(Text("Backend agnostic.", 24.0f, colour::kSand, true),
                            words("Natively, a scene drawn by Skia: every frame laid out, damaged and repainted where it changed.", 18.0f, colour::kPaper),
                            words("Here, the browser's own elements: a Box a div, a Text a span, a button a button -- laid out by the browser, written "
                                  "by skiff only where the model moved.",
                                  18.0f, colour::kPaper)))));
}

// ---- the counter: its part of the model, what it is sent, how it looks ----

struct Counter {
  int count = 0;
  friend bool operator==(const Counter &, const Counter &) = default;
};

struct Increment {};
struct Decrement {};

inline constexpr std::string_view kCounterSource = R"(auto counter() {
  // Its part of the model, kept here; what it is
  // sent, answered here.
  return state<Counter>(
      handlers(handle<Increment>([] {
        return model::over<Counter>([](Counter c) {
          ++c.count;
          return c;
        });
      })),
      column(vbox(12.0f),
          label<Counter>([](const Counter &c) {
            return std::format("Pressed {} times", c.count);
          }),
          row(hbox(8.0f),
              SendButton<Decrement>("−", Decrement{}),
              SendButton<Increment>("+", Increment{}))));
}
)";

// Counted one way, or the other.
struct Count {
  int by = 1;
  [[nodiscard]] Counter operator()(Counter c) const {
    c.count += by;
    return c;
  }
};
auto counter() {
  return state<Counter>(handlers(handle<Increment>([] { return model::over<Counter>(Count{1}); }),
                                 handle<Decrement>([] { return model::over<Counter>(Count{-1}); })),
                        column(vbox(12.0f, {.fillX = true, .autoSize = scene::axes::kY}),
                               label<Counter>([](const Counter &c) { return std::format("Pressed {} times", c.count); }, 28.0f,
                                              colour::kInk, true),
                               row(hbox(8.0f, {.autoSize = scene::axes::kBoth}), SendButton<Decrement>("−", Decrement{}),
                                   SendButton<Increment>("+", Increment{}))));
}

auto declarative() {
  return SKIFF_NAMED(Syntax, section("syntax", colour::kPaper, title("Declarative, and typed", 44.0f, colour::kCoral),
                 sides(side(words("A screen is an expression: nested rows and columns of nodes, each said once, with what it reads from the model "
                                  "and what it sends."),
                            words("A component keeps its part of the model where it is made, and answers what is sent to it there. The "
                                  "model is the parts the tree says: no root to write, nothing to keep in step by hand."),
                            card(colour::kInk, Code(std::string(kCounterSource)))),
                       side(card(colour::kPanel, Text("Live", 15.0f, colour::kMuted, true), counter(),
                                 words("That is the code on the left, running: the same function, compiled into this page.", 15.0f, colour::kMuted))))));
}

// ---- the list: its part of the model, what it is sent, how it looks ----

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
// The list of things to do and the line being typed into it: one part,
// kept by the list's view.
struct TodoList {
  skiff::model::Tracked<Draft> draft;
  skiff::model::Keyed<int, Todo> items;
};

struct AddTodo {
  std::string text;
};
struct RemoveTodo {
  int id = 0;
};
// The list the page begins with, put into the model that keeps it.
template <class Model> void seed(Model &model) {
  int id = 0;
  for (const char *text : {"Write the tree", "Bind it to the model", "Ship it as a page"}) {
    (void)model.apply(skiff::model::put<Todo>(id, Todo{id, text, id == 0}));
    ++id;
  }
}


// The button of a row of the list: it takes its row away -- the row's
// element read, made what it sends.
struct RemoveOf {
  [[nodiscard]] RemoveTodo operator()(const Todo &t) const { return RemoveTodo{t.id}; }
};
auto removeButton() { return projected<Todo>(RemoveOf{}, SendButton<RemoveTodo>("×", RemoveTodo{})); }
// The button that adds what is typed: the line read, made what it sends.
struct AddOf {
  [[nodiscard]] AddTodo operator()(const Draft &d) const { return AddTodo{d.text}; }
};
auto addButton() { return projected<Draft>(AddOf{}, SendButton<AddTodo>("Add", AddTodo{})); }
// A thing to do put into the list, where it says something: an Add with
// nothing typed puts nothing.
struct PutTodo {
  Todo todo;
  void operator()(model::Keyed<int, Todo> &list) const {
    if (!todo.text.empty())
      list.put(todo.id, todo, std::numeric_limits<std::size_t>::max());
  }
};

auto todoRow(const Todo &) {
  // Each part reads the row's element itself: the row is only where they sit.
  return row(hbox(10.0f, {.fillX = true, .autoSize = scene::axes::kY, .padding = {6.0f, 10.0f, 6.0f, 10.0f}, .cornerRadius = 8.0f,
                          .background = colour::kFoam}),
             bound<model::Field<&Todo::done>>(ToggleField()),
             styled({.grow = scene::axes::kX}, label<Todo>([](const Todo &t) { return t.text; }, 17.0f)), removeButton());
}

// The list: its part of the model -- the line typed and the things to do --
// kept here, and what its buttons send answered here.
auto todoList() {
  return state<TodoList>(
      handlers(handle<AddTodo>([](const AddTodo &add) {
                 static int next = 100;
                 const int id = next++;
                 return std::tuple{model::over<model::Keyed<int, Todo>>(PutTodo{Todo{id, add.text, false}}),
                                   model::over<Draft>(model::setTo(Draft{}))};
               }),
               handle<RemoveTodo>([](const RemoveTodo &remove) { return model::take<Todo>(remove.id); })),
      card(colour::kPaper, Text("Live", 15.0f, colour::kMuted, true),
           row(hbox(8.0f, {.fillX = true, .autoSize = scene::axes::kY}),
               styled({.grow = scene::axes::kX}, bound<model::Field<&Draft::text>>(TextField<std::string>("Something to do"))),
               addButton()),
           each<int, Todo>([](const Todo &t) { return todoRow(t); }, vbox(8.0f, {.fillX = true, .autoSize = scene::axes::kY}))));
}

auto binding() {
  return SKIFF_NAMED(Binding, section("model", colour::kPanel, title("A model you edit, a view that follows", 44.0f, colour::kSea),
                 sides(side(words("The program's state is one model: the parts its views keep, tracked, and lists of them by key. A control "
                                  "edits its part; the binding walks only to the nodes that read what changed."),
                            words("No observers to subscribe, no virtual DOM to diff: what reads a part is known from the tree's type."),
                            card(colour::kInk, Code(R"(each<int, Todo>([](const Todo &) {
  return row(hbox(10.0f),
      bound<model::Field<&Todo::done>>(ToggleField()),
      label<Todo>([](const Todo &t) {
        return t.text;
      }),
      removeButton());
}))"))),
                       side(todoList()))));
}

// ---- the moving boxes: the clock they read ----

// Seconds since the page began, at the last frame; still where nothing runs
// it (the page written when the site is built).
struct Clock {
  double seconds = 0.0;
  friend bool operator==(const Clock &, const Clock &) = default;
};

// Two colours mixed: `t` of the way from one to the other.
[[nodiscard]] inline scene::Colour mix(scene::Colour a, scene::Colour b, float t) {
  const auto channel = [&](int shift) {
    const float x = static_cast<float>((a >> shift) & 0xffu), y = static_cast<float>((b >> shift) & 0xffu);
    return static_cast<scene::Colour>(std::lround(x + (y - x) * t)) << shift;
  };
  return channel(24) | channel(16) | channel(8) | channel(0);
}
// clay's own demonstration of speed, as clay's site shows it: two boxes in a
// frame, the left from 30% of it to 70% and back, the right the rest, their
// colours going each the other's way -- the same words in both, wrapped anew
// at every width, every frame. `t` goes from 0 to 1 and back in two seconds,
// straight, as clay's does.
[[nodiscard]] inline float breath(const Clock &c) { return 1.0f - std::abs(static_cast<float>(std::fmod(c.seconds, 2.0)) - 1.0f); }
struct LeftLook {
  [[nodiscard]] scene::Spec operator()(const Clock &c) const {
    const float t = breath(c);
    return scene::Spec{.width = 0.3f + 0.4f * t, .height = 1.0f, .relativeSize = scene::axes::kBoth, .padding = scene::Margin::all(18.0f),
                       .background = mix(colour::kCoral, colour::kAmber, t), .masking = true};
  }
};
struct RightLook {
  [[nodiscard]] scene::Spec operator()(const Clock &c) const {
    const float t = breath(c);
    return scene::Spec{.height = 1.0f, .relativeSize = scene::axes::kY, .grow = scene::axes::kX, .padding = scene::Margin::all(18.0f),
                       .background = mix(colour::kAmber, colour::kCoral, t), .masking = true};
  }
};
inline constexpr std::string_view kLorem =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
// The words, in the middle of their box's height.
[[nodiscard]] Text lorem() {
  Text t = words(std::string(kLorem), 20.0f, colour::kPaper);
  t.apply({.grow = scene::axes::kX, .alignSelf = scene::align::kMiddle});
  return t;
}
auto breathing() {
  return state<Clock>(row(hbox(0.0f, {.fillX = true, .height = 400.0f, .border = scene::Border{colour::kPaper, 2.0f}}),
             spec_for<Clock>(LeftLook{}, row(hbox(0.0f, {}), lorem())), spec_for<Clock>(RightLook{}, row(hbox(0.0f, {}), lorem()))));
}

auto motion() {
  return SKIFF_NAMED(Motion, section("motion", colour::kInk, title("Fast enough to move every frame", 44.0f, colour::kSand),
                 sides(side(words("The boxes' widths and colours are read from the model's clock. Every frame the clock moves, the binding "
                                  "walks to the two nodes that read it, and the page writes their widths and colours -- the browser wraps "
                                  "the words again.",
                                  18.0f, colour::kPaper),
                            words("On the canvas, and natively, the same holds for the scene: the boxes are laid out again, their words wrapped anew, and only "
                                  "what moved is repainted.",
                                  18.0f, colour::kPaper)),
                       side(breathing()))));
}

// Shown while the backend chosen is this one. A type of its own rather than
// a lambda made by a lambda: clang 23 recursed without end looking for the
// definition of that inner closure from a unit importing this module.
struct ShownIs {
  Backend backend;
  [[nodiscard]] scene::Spec operator()(const Shown &s) const { return {.visible = s.backend == backend}; }
};

auto backends() {
  const auto when = [](Backend b) { return ShownIs{b}; };
  return SKIFF_NAMED(Backends, state<Shown>(section("backends", colour::kPaper, title("One tree, two backends", 44.0f, colour::kCoral),
                 words("skiff.ui is one tree for both backends: this program mounts it on the page and on the scene, one model under both. "
                       "Try changing it:"),
                 bound<model::Field<&Shown::backend>>(ChoiceTabs<Backend>(
                     {{"Page", Backend::kPage}, {"Canvas", Backend::kScene}})),
                 spec_for<Shown>(when(Backend::kPage),
                                 card(colour::kPanel, either(words("You are looking at it: skiff.web makes each node an element of the document and binds "
                                                                  "it to the model. The page is C++ compiled to WebAssembly; there is no "
                                                                  "framework of JavaScript under it, only the few lines that pass the "
                                                                  "document's events in."),
                                                            words("Choose it to go back to the page: the same tree, its nodes the "
                                                                  "document's elements, laid out by the browser and bound to the model.")))),
                 spec_for<Shown>(when(Backend::kScene),
                                 card(colour::kPanel, either(words("Choose it: this whole page drawn by skiff's own scene on a canvas -- the same tree, "
                                                                  "laid out by skiff instead of the browser, drawn through its painter, as a "
                                                                  "window on Skia draws it natively."),
                                                            words("You are looking at it: this whole page drawn by skiff's own scene on a "
                                                                  "canvas -- laid out by skiff, not the browser, drawn through its painter "
                                                                  "on the browser's 2D context, and repainted only where it changed. No "
                                                                  "Skia: skiff's stand-in for its types draws what the scene asks.")))))));
}

auto footer() {
  return SKIFF_NAMED(Footer, section("footer", colour::kInk,
                 sides(side(Text("skiff", 22.0f, colour::kSand, true),
                            words("This page is site/ in skiff-framework: one tree in skiff.ui, mounted on skiff.web and on skiff's scene, built with "
                                  "Emscripten, served by GitHub Pages.",
                                  15.0f,
                                  colour::kPaper)),
                       side(Text("Written in the same hand", 15.0f, colour::kSand, true),
                            row(hbox(18.0f, {.autoSize = scene::axes::kBoth}), Link("skiff", "https://github.com/j4niwzis/skiff", 16.0f, colour::kPaper),
                                Link("skiff-widgets", "https://github.com/j4niwzis/skiff-widgets", 16.0f, colour::kPaper),
                                Link("knot", "https://github.com/j4niwzis/knot", 16.0f, colour::kPaper),
                                Link("chevron", "https://github.com/j4niwzis/chevron", 16.0f, colour::kPaper),
                                Link("alef", "https://github.com/j4niwzis/alef", 16.0f, colour::kPaper))))));
}

// The page: what each part keeps and answers is its own (state<T>).
auto page() {
  return column(vbox(0.0f, {.fillX = true, .autoSize = scene::axes::kY}), header(), hero(), features(), declarative(), binding(), motion(),
                backends(), footer());
}


// The theme both backends dress their controls in, said once: the site's
// tabs sit on its paper, in its ink. Before anything is made.
inline void dress() {
  Theme &t = theme();
  t.fTabText = colour::kInk;
  t.fTabTextDim = colour::kMuted;
}

// The model: the parts the page's views say they keep.
using Model = skiff::ui::ModelOf<decltype(page())>;

// The stylesheet the site adds to skiff's: the page's font, code kept as it
// is written, links, the framed copy and the pictures.
inline constexpr std::string_view kSiteCss = R"(
html,body{margin:0;background:#f4efe6}
#skiff{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#1d2b36}
.site-code{margin:0;white-space:pre;display:block;overflow-x:auto;max-width:100%;box-sizing:border-box;font:13.5px/1.55 ui-monospace,SFMono-Regular,Menlo,monospace;color:#e8eef2}
.c-kw{color:#ff9d76}.c-name{color:#8fd3e0}.c-str{color:#e7c79b}.c-num{color:#f2b5d4}.c-com{color:#7d8f9b;font-style:italic}
.site-link{text-decoration:none}
.site-link:hover{text-decoration:underline}
#skiff .skiff-tab{font-size:17px;padding:6px 0}
@media (max-width:640px){[data-view=Header]{flex-wrap:wrap;row-gap:6px}}
#skiff .skiff-tab[aria-selected=true]{font-weight:700;box-shadow:inset 0 -2px #d9603b}
)";

} // namespace site
