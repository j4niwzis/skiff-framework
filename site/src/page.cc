// The site of skiff, written with skiff: one tree against skiff.ui. In a
// browser it is the page itself, its nodes the document's elements, bound
// to the model -- C++ compiled to WebAssembly; when the site is built, the
// same tree is mounted natively on a document kept in memory and written
// out as HTML, which is what a browser shows first.
module;
#include <skiff/named.h>
export module site.page;

import std;
import skiff.ui;
import site.model;

namespace site {
using namespace skiff;
using namespace skiff::ui;
namespace dom = skiff::web::dom;
} // namespace site

// Exported, every piece: as the maze visualizer's are. Kept to the module,
// clang 23 recursed without end looking for their definitions from the unit
// importing the page (Sema::hasAcceptableDefinition).
export namespace site {

namespace colour {
inline constexpr scene::Colour kPaper = 0xfff4efe6u;  // sailcloth
inline constexpr scene::Colour kInk = 0xff1d2b36u;    // the sea at night
inline constexpr scene::Colour kSea = 0xff2a7f8fu;
inline constexpr scene::Colour kCoral = 0xffd9603bu;
inline constexpr scene::Colour kSand = 0xffe7c79bu;
inline constexpr scene::Colour kMuted = 0xff5a6b77u;
inline constexpr scene::Colour kFoam = 0xffffffffu;
inline constexpr scene::Colour kCodeInk = 0xffe8eef2u;
inline constexpr scene::Colour kPanel = 0xffebe3d7u;
} // namespace colour

// ---- the nodes the site has of its own ----

// A link: a text that is an <a>.
struct Link : Text {
  Link(std::string label, std::string href, float size, scene::Colour c, bool bold = false)
      : Text(std::move(label), size, c, bold), fHref(std::move(href)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "a"; }
  void mounted() {
    Text::mounted();
    dom::attr(fState.fHandle, "class", "skiff-text site-link");
    dom::attr(fState.fHandle, "href", fHref);
  }
  std::string fHref;
};
// Code, as it is written -- its lines kept -- and coloured as an editor
// colours C++: each word, string, number and comment a span of its kind.
struct Code : skiff::web::Node {
  explicit Code(std::string code) : fCode(std::move(code)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "pre"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "site-code");
    for (const auto &[kind, text] : tokens(fCode)) {
      const dom::Handle span = dom::create("span");
      if (!kind.empty()) dom::attr(span, "class", std::string("c-").append(kind));
      dom::text(span, text);
      dom::append(fState.fHandle, span);
    }
  }
  // The code cut into runs of a kind: "kw", "name", "str", "num", "com", or
  // "" for the rest.
  [[nodiscard]] static std::vector<std::pair<std::string_view, std::string_view>> tokens(std::string_view code) {
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
  std::string fCode;
};
// Another page, framed: the copy of this one written when the site was built.
struct Framed : skiff::web::Node {
  explicit Framed(std::string src, scene::Spec spec = {}) : fSrc(std::move(src)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "iframe"; }
  void mounted() {
    dom::attr(fState.fHandle, "src", fSrc);
    dom::attr(fState.fHandle, "loading", "lazy");
    dom::attr(fState.fHandle, "title", "This page, as it was written when the site was built");
    dom::attr(fState.fHandle, "class", "site-frame");
  }
  std::string fSrc;
};
// A text that reads its part of the model.
template <class T, class F> struct Label : Text {
  Label(F format, float size, scene::Colour c, bool bold = false) : Text(std::string(), size, c, bold), fFormat(std::move(format)) {
    setWrapped(true);
  }
  void read(const T &now) { setText(fFormat(now)); }
  F fFormat;
};
template <class T, class F> Label<T, F> label(F format, float size = 16.0f, scene::Colour c = colour::kInk, bool bold = false) {
  return Label<T, F>(std::move(format), size, c, bold);
}
// The button of a row of the list: it takes its row away.
struct RemoveButton : SendButton<RemoveTodo> {
  RemoveButton() : SendButton<RemoveTodo>("×", RemoveTodo{}) {}
  void read(const Todo &t) { SendButton<RemoveTodo>::read(RemoveTodo{t.id}); }
};
// A thing to do put into the list, where it says something: an Add with
// nothing typed puts nothing.
struct PutTodo {
  Todo todo;
  void operator()(model::Keyed<int, Todo> &list) const {
    if (!todo.text.empty())
      list.put(todo.id, todo, std::numeric_limits<std::size_t>::max());
  }
};
// The button that adds what is typed.
struct AddButton : SendButton<AddTodo> {
  AddButton() : SendButton<AddTodo>("Add", AddTodo{}) {}
  void read(const Draft &d) { SendButton<AddTodo>::read(AddTodo{d.text}); }
};

// ---- the pieces of the page ----

[[nodiscard]] Text title(std::string s, float size = 40.0f, scene::Colour c = colour::kInk) { return Text(std::move(s), size, c, true); }
[[nodiscard]] Text words(std::string s, float size = 18.0f, scene::Colour c = colour::kInk) {
  Text t(std::move(s), size, c);
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
             Text("skiff", 26.0f, colour::kSand, true), Box<>(scene::Spec{.grow = scene::axes::kX}),
             Link("Docs", "https://github.com/j4niwzis/skiff#readme", 17.0f, colour::kPaper),
             Link("Widgets", "https://github.com/j4niwzis/skiff-widgets", 17.0f, colour::kPaper),
             Link("Framework", "https://github.com/j4niwzis/skiff-framework", 17.0f, colour::kPaper),
             Link("GitHub", "https://github.com/j4niwzis/skiff", 17.0f, colour::kSand, true)));
}

[[nodiscard]] inline std::string whoIsLayingOut(const Shown &s) {
  return s.live ? "skiff is laying out this page right now — every element you see is a node of a C++ tree, compiled to WebAssembly and "
                  "bound to a model."
                : "skiff wrote this page when the site was built — the same C++ tree, mounted on a document kept in memory and saved as "
                  "HTML. The live one takes over as soon as its WebAssembly has loaded.";
}

auto hero() {
  return SKIFF_NAMED(Hero, section("top", colour::kPaper,
                 sides(side(title("A C++ UI library: a typed tree, a model it is bound to, and two backends.", 52.0f),
                            bound<Shown>(label<Shown>(whoIsLayingOut, 26.0f, colour::kCoral, true))),
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

inline constexpr std::string_view kCounterSource = R"(auto counter() {
  return column(vbox(12.0f),
      bound<Counter>(label<Counter>([](const Counter &c) {
        return std::format("Pressed {} times", c.count);
      })),
      row(hbox(8.0f),
          SendButton<Decrement>("−", Decrement{}),
          SendButton<Increment>("+", Increment{})));
}
// ...and what the buttons send, answered:
handle<Increment>([] {
  return model::over<Counter>([](Counter c) {
    ++c.count;
    return c;
  });
})
)";

auto counter() {
  return column(vbox(12.0f, {.fillX = true, .autoSize = scene::axes::kY}),
                bound<Counter>(label<Counter>([](const Counter &c) { return std::format("Pressed {} times", c.count); }, 28.0f, colour::kInk, true)),
                row(hbox(8.0f, {.autoSize = scene::axes::kBoth}), SendButton<Decrement>("−", Decrement{}),
                    SendButton<Increment>("+", Increment{})));
}

auto declarative() {
  return SKIFF_NAMED(Syntax, section("syntax", colour::kPaper, title("Declarative, and typed", 44.0f, colour::kCoral),
                 sides(side(words("A screen is an expression: nested rows and columns of nodes, each said once, with what it reads from the model "
                                  "and what it sends."),
                            words("Mix it with C++ as it is: functions are components, loops make rows, an if is a part shown or not."),
                            card(colour::kInk, Code(std::string(kCounterSource)))),
                       side(card(colour::kPanel, Text("Live", 15.0f, colour::kMuted, true), counter(),
                                 words("That is the code on the left, running: the same function, compiled into this page.", 15.0f, colour::kMuted))))));
}

auto todoRow(const Todo &) {
  // Each part reads the row's element itself: the row is only where they sit.
  return row(hbox(10.0f, {.fillX = true, .autoSize = scene::axes::kY, .padding = {6.0f, 10.0f, 6.0f, 10.0f}, .cornerRadius = 8.0f,
                          .background = colour::kFoam}),
             bound<model::Field<&Todo::done>>(ToggleField()),
             styled({.grow = scene::axes::kX}, bound<Todo>(label<Todo>([](const Todo &t) { return t.text; }, 17.0f))),
             bound<Todo>(RemoveButton()));
}

auto binding() {
  return SKIFF_NAMED(Binding, section("model", colour::kPanel, title("A model you edit, a view that follows", 44.0f, colour::kSea),
                 sides(side(words("The program's state is one model: tracked parts, lists of them by key. A control edits its part; the binding "
                                  "walks only to the nodes that read what changed."),
                            words("No observers to subscribe, no virtual DOM to diff: what reads a part is known from the tree's type."),
                            card(colour::kInk, Code(R"(each<int, Todo>([](const Todo &) {
  return row(hbox(10.0f),
      bound<model::Field<&Todo::done>>(ToggleField()),
      bound<Todo>(label<Todo>([](const Todo &t) {
        return t.text;
      })),
      bound<Todo>(RemoveButton()));
}))"))),
                       side(card(colour::kPaper, Text("Live", 15.0f, colour::kMuted, true),
                                 row(hbox(8.0f, {.fillX = true, .autoSize = scene::axes::kY}),
                                     styled({.grow = scene::axes::kX}, bound<model::Field<&Draft::text>>(TextField<std::string>("Something to do"))),
                                     bound<Draft>(AddButton())),
                                 each<int, Todo>([](const Todo &t) { return todoRow(t); }, vbox(8.0f, {.fillX = true, .autoSize = scene::axes::kY})))))));
}

// A bar of the moving section: its width and colour as the clock says. A
// type of its own, not a lambda that captures: clang 23 recursed without end
// looking for such a closure's definition from the unit importing the page.
struct BarLook {
  int index = 0;
  [[nodiscard]] scene::Spec operator()(const Clock &c) const {
    const double phase = c.seconds * 1.6 + index * 0.45;
    const float wave = static_cast<float>(0.5 + 0.5 * std::sin(phase));
    const std::array<scene::Colour, 3> colours{colour::kSea, colour::kCoral, colour::kSand};
    // A share of the column's width, not pixels: on a phone as on a screen.
    return scene::Spec{.width = 0.1f + 0.9f * wave, .relativeSize = scene::axes::kX,
                       .background = colours[static_cast<std::size_t>(index) % colours.size()]};
  }
};
auto bar(int i) { return spec_for<Clock>(BarLook{i}, Box<>(scene::Spec{.height = 14.0f, .cornerRadius = 7.0f})); }
template <std::size_t... I> auto bars(std::index_sequence<I...>) {
  return column(vbox(8.0f, {.fillX = true, .autoSize = scene::axes::kY}), bar(static_cast<int>(I))...);
}

auto motion() {
  return SKIFF_NAMED(Motion, section("motion", colour::kInk, title("Fast enough to move every frame", 44.0f, colour::kSand),
                 sides(side(words("Each bar is a node, its width read from the model's clock. Every frame the clock moves, the binding walks to the "
                                  "bars that read it, and the page writes only the widths that changed.",
                                  18.0f, colour::kPaper),
                            words("Natively the same holds for the scene: what moved is damaged and repainted, the rest kept as it was drawn.", 18.0f,
                                  colour::kPaper)),
                       side(bars(std::make_index_sequence<12>{})))));
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
  return SKIFF_NAMED(Backends, section("backends", colour::kPaper, title("One tree, two backends", 44.0f, colour::kCoral),
                 words("skiff.ui is the same names on either backend: the program says which it is built for. Try changing it:"),
                 bound<model::Field<&Shown::backend>>(ChoiceTabs<Backend>(
                     {{"This page", Backend::kPage}, {"Without JavaScript", Backend::kStatic}, {"Canvas", Backend::kScene}})),
                 spec_for<Shown>(when(Backend::kPage),
                                 card(colour::kPanel, words("You are looking at it: skiff.web makes each node an element of the document and binds it "
                                                            "to the model. The page is C++ compiled to WebAssembly; there is no framework of "
                                                            "JavaScript under it, only the few lines that pass the document's events in."))),
                 spec_for<Shown>(when(Backend::kStatic),
                                 card(colour::kPanel, words("The same tree, written out when the site was built: mounted natively on a document kept in "
                                                            "memory and saved as HTML -- what a search engine and a slow phone see first. No script "
                                                            "at all:"),
                                      Framed("static.html", {.fillX = true, .height = 520.0f}))),
                 spec_for<Shown>(when(Backend::kScene),
                                 card(colour::kPanel, words("Natively the same tree is a scene: laid out and drawn by skiff itself, each frame "
                                                            "damaged and repainted where it changed. Here the scene draws on a canvas -- "
                                                            "coming next.")))));
}

auto footer() {
  return SKIFF_NAMED(Footer, section("footer", colour::kInk,
                 sides(side(Text("skiff", 22.0f, colour::kSand, true),
                            words("This page is site/ in skiff-framework: skiff.ui on skiff.web, built with Emscripten, served by GitHub Pages.", 15.0f,
                                  colour::kPaper)),
                       side(Text("Written in the same hand", 15.0f, colour::kSand, true),
                            row(hbox(18.0f, {.autoSize = scene::axes::kBoth}), Link("skiff", "https://github.com/j4niwzis/skiff", 16.0f, colour::kPaper),
                                Link("skiff-widgets", "https://github.com/j4niwzis/skiff-widgets", 16.0f, colour::kPaper),
                                Link("knot", "https://github.com/j4niwzis/knot", 16.0f, colour::kPaper),
                                Link("chevron", "https://github.com/j4niwzis/chevron", 16.0f, colour::kPaper),
                                Link("alef", "https://github.com/j4niwzis/alef", 16.0f, colour::kPaper))))));
}

// The stylesheet the site adds to skiff's: the page's font, code kept as it
// is written, links, the framed copy and the pictures.
inline constexpr std::string_view kSiteCss = R"(
html,body{margin:0;background:#f4efe6}
#skiff{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#1d2b36}
.site-code{margin:0;white-space:pre;display:block;overflow-x:auto;max-width:100%;box-sizing:border-box;font:13.5px/1.55 ui-monospace,SFMono-Regular,Menlo,monospace;color:#e8eef2}
.c-kw{color:#ff9d76}.c-name{color:#8fd3e0}.c-str{color:#e7c79b}.c-num{color:#f2b5d4}.c-com{color:#7d8f9b;font-style:italic}
.site-link{text-decoration:none}
.site-link:hover{text-decoration:underline}
.site-frame{border:1px solid #c9bfb1;border-radius:10px;background:#f4efe6;width:100%}
#skiff .skiff-tab{color:#5a6b77;font-size:17px;padding:6px 0}
@media (max-width:640px){[data-view=Header]{flex-wrap:wrap;row-gap:6px}}
#skiff .skiff-tab:hover{color:#1d2b36}
#skiff .skiff-tab[aria-selected=true]{color:#1d2b36;font-weight:700;box-shadow:inset 0 -2px #d9603b}
)";

auto page() {
  return scoped<Root>(
      handlers(handle<Increment>([] { return model::over<Counter>([](Counter c) { ++c.count; return c; }); }),
               handle<Decrement>([] { return model::over<Counter>([](Counter c) { --c.count; return c; }); }),
               handle<AddTodo>([](const AddTodo &add) {
                 static int next = 100;
                 const int id = next++;
                 return std::tuple{model::over<model::Keyed<int, Todo>>(PutTodo{Todo{id, add.text, false}}),
                                   model::over<Draft>(model::setTo(Draft{}))};
               }),
               handle<RemoveTodo>([](const RemoveTodo &remove) { return model::take<Todo>(remove.id); })),
      column(vbox(0.0f, {.fillX = true, .autoSize = scene::axes::kY}), header(), hero(), features(), declarative(), binding(), motion(),
             backends(), footer()));
}

} // namespace site
