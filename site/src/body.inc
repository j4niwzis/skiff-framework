// The site's tree, said once for both of its backends: included where each
// says its names -- skiff.ui on the page (page.cc), skiff's scene on a canvas
// (canvas.cc) -- after the nodes each has of its own (Link, Code)
// and kOnCanvas, which of the two it is.
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
             Text("skiff", 26.0f, colour::kSand, true), styled({.grow = scene::axes::kX}, Box<>(scene::Colour{0})),
             Link("Docs", "https://github.com/j4niwzis/skiff#readme", 17.0f, colour::kPaper),
             Link("Widgets", "https://github.com/j4niwzis/skiff-widgets", 17.0f, colour::kPaper),
             Link("Framework", "https://github.com/j4niwzis/skiff-framework", 17.0f, colour::kPaper),
             Link("GitHub", "https://github.com/j4niwzis/skiff", 17.0f, colour::kSand, true)));
}

[[nodiscard]] inline std::string whoIsLayingOut(const Shown &s) {
  if (kOnCanvas)
    return "skiff is drawing this page right now, on a canvas \u2014 laid out by skiff's own scene, not by the browser, and "
           "drawn through its painter.";
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
  return row(hbox(0.0f, {.fillX = true, .height = 400.0f, .border = scene::Border{colour::kPaper, 2.0f}}),
             spec_for<Clock>(LeftLook{}, row(hbox(0.0f, {}), lorem())), spec_for<Clock>(RightLook{}, row(hbox(0.0f, {}), lorem())));
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
  return SKIFF_NAMED(Backends, section("backends", colour::kPaper, title("One tree, two backends", 44.0f, colour::kCoral),
                 words("skiff.ui is the same names on either backend: the program says which it is built for. Try changing it:"),
                 bound<model::Field<&Shown::backend>>(ChoiceTabs<Backend>(
                     {{"Page", Backend::kPage}, {"Canvas", Backend::kScene}})),
                 spec_for<Shown>(when(Backend::kPage),
                                 card(colour::kPanel, words(kOnCanvas ? "Choose it to go back to the page: the same tree, its nodes the document's "
                                                                        "elements, laid out by the browser and bound to the model."
                                                                      : "You are looking at it: skiff.web makes each node an element of the "
                                                                        "document and binds it to the model. The page is C++ compiled to "
                                                                        "WebAssembly; there is no framework of JavaScript under it, only "
                                                                        "the few lines that pass the document's events in."))),
                 spec_for<Shown>(when(Backend::kScene),
                                 card(colour::kPanel, words(kOnCanvas ? "You are looking at it: this whole page drawn by skiff's own scene on a canvas -- "
                                                                        "laid out by skiff, not the browser, drawn through its painter on "
                                                                        "the browser's 2D context, and repainted only where it changed. "
                                                                        "No Skia: skiff's stand-in for its types draws what the scene asks."
                                                                      : "Choose it: this whole page drawn by skiff's own scene on a canvas "
                                                                        "-- the same tree, laid out by skiff instead of the browser, drawn "
                                                                        "through its painter, as a window on Skia draws it natively.")))));
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

