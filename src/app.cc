// skiff.app: what nearly every program written with skiff does, done once.
// A program says its tree (a function returning a view in skiff.ui) and
// what it adds (Hooks), and the program is
//
//   SKIFF_APP(myapp::page, myapp::Hooks)        // <skiff/app.h>
//
// In a browser (Emscripten): the tree mounted on the page -- over the copy
// of it written when the program was built, where there is one, taken over
// rather than made again -- the model moved at every frame and the page
// following it; and, where the program says so, the same tree drawn by
// skiff's scene on a canvas instead, one model under both. Natively: the
// page written out as HTML, for a browser to show before the WebAssembly has
// loaded (prerender), or for a search engine. The model is the parts the
// tree's views keep (skiff::ui::ModelOf).
//
// What a program adds is said by Hooks, every member of it optional:
//   void dress()                          the theme, before anything is made
//   void start(Model &)                   the model as the program begins
//   void frame(Model &, double seconds)   at every frame, before the views follow
//   bool onCanvas(const Model &)          the canvas shown, not the page
//   bool outlineRepaints(const Model &)   each repaint of the canvas outlined
//   std::string_view stylesheet()         the program's CSS, after skiff's
//   skiff::scene::Colour paper()          what the canvas is cleared to
//   void open(std::string_view href)      a link followed on the canvas
//   std::string_view head()               the <head> written out with the page
//
// With the framework's ssr feature, the model the page was written with
// travels with it (skiff.app.carry): written into the page as JSON by knot
// beside the markup and read back in the browser before the page is taken
// over, start() then not run. Without it -- a program that is a page in the
// browser alone -- nothing travels, and the page is made where nothing was
// written before it.
module;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
export module skiff.app;

export import skiff.ui;
import std;
import skiff.app.carry;

#ifdef __EMSCRIPTEN__
// ---- the page's side, in JavaScript ---------------------------------------
// The canvas and the box it stays at the top of; the pointer and the keys
// passed in; the page and the canvas shown, measured and scrolled.
// clang-format off
EM_JS(void, skiffAppCanvasMake, (), {
  const host = document.createElement('div');
  host.id = 'skiff-canvas-host';
  host.style.cssText = 'display:none;position:relative;width:100%';
  const c = document.createElement('canvas');
  c.id = 'skiff-canvas';
  c.style.cssText = 'position:sticky;top:0;display:block;touch-action:pan-x pan-y';
  host.appendChild(c);
  document.body.appendChild(host);
  const at = (e) => {
    const r = host.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  };
  c.addEventListener('pointermove', (e) => { const p = at(e); Module._skiff_canvas_pointer(0, p[0], p[1], 0); });
  c.addEventListener('pointerdown', (e) => { const p = at(e); Module._skiff_canvas_pointer(1, p[0], p[1], e.button + 1); });
  c.addEventListener('pointerup', (e) => { const p = at(e); Module._skiff_canvas_pointer(2, p[0], p[1], e.button + 1); });
  c.addEventListener('pointercancel', (e) => { const p = at(e); Module._skiff_canvas_pointer(3, p[0], p[1], 0); });
  c.addEventListener('pointerleave', (e) => { Module._skiff_canvas_pointer(0, -1, -1, 0); });
  const codes = {Tab: 1, Enter: 2, Escape: 4, ArrowLeft: 5, ArrowRight: 6, ArrowUp: 7, ArrowDown: 8, Home: 9, End: 10,
                 Backspace: 11, Delete: 12, PageUp: 13, PageDown: 14};
  window.addEventListener('keydown', (e) => {
    if (host.style.display === 'none') return;
    let code = codes[e.key] || 0;
    if (!code && (e.ctrlKey || e.metaKey) && /^[a-z]$/i.test(e.key)) code = 101 + e.key.toLowerCase().charCodeAt(0) - 97;
    let taken = code && Module._skiff_canvas_key(code, e.shiftKey ? 1 : 0, (e.ctrlKey || e.metaKey) ? 1 : 0);
    if (!code && e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey)
      taken = Module.ccall('skiff_canvas_text', 'number', ['string'], [e.key]);
    if (taken) e.preventDefault();
  });
});
EM_JS(void, skiffAppShowPage, (int on), {
  const page = document.getElementById('skiff');
  if (page) page.style.display = on ? '' : 'none';
});
EM_JS(void, skiffAppShowCanvas, (int on), {
  const host = document.getElementById('skiff-canvas-host');
  if (host) host.style.display = on ? 'block' : 'none';
});
EM_JS(double, skiffAppScrollY, (), { return window.scrollY; });
EM_JS(void, skiffAppScrollTo, (double y), { window.scrollTo(window.scrollX, y); });
EM_JS(int, skiffAppCanvasWidth, (), {
  const host = document.getElementById('skiff-canvas-host');
  return host ? host.clientWidth : 0;
});
EM_JS(int, skiffAppWindowHeight, (), { return window.innerHeight; });
EM_JS(double, skiffAppWindowScale, (), { return window.devicePixelRatio || 1; });
EM_JS(void, skiffAppCanvasTall, (float px), { document.getElementById('skiff-canvas-host').style.height = px + 'px'; });
// How far the box has gone up past the canvas, which stays at the top.
EM_JS(double, skiffAppCanvasOffset, (), {
  const host = document.getElementById('skiff-canvas-host');
  const c = document.getElementById('skiff-canvas');
  return c.getBoundingClientRect().top - host.getBoundingClientRect().top;
});
EM_JS(void, skiffAppOpen, (const char *href, int length), { window.location.href = UTF8ToString(href, length); });
// The state the page was written with, where it carries one: its text, made
// for the program to free; else none.
EM_JS(char *, skiffAppStateText, (), {
  const kept = document.getElementById('skiff-state');
  return kept ? stringToNewUTF8(kept.textContent) : 0;
});
// clang-format on
#endif

export namespace skiff::app {

// Whether this is the program in a browser, alive: not the one writing the
// page out when the program is built.
#ifdef __EMSCRIPTEN__
inline constexpr bool kInBrowser = true;
#else
inline constexpr bool kInBrowser = false;
#endif

// A program that adds nothing.
struct NoHooks {};


namespace detail {
// The scene's root: the tree, as the program's -- found by its type where
// the scene's routing begins (startCarry), so a press and what a field typed
// into answers are answered where they are made, with the program's model
// and sink.
template <class A, class N> struct Root : N {
  using N::onKey;
  using N::onPointer;
  using N::onText;
  using N::onSemantic;
  explicit Root(N made) : N(std::move(made)) {}
};
template <class A, class N> auto startCarry(Root<A, N> &) { return skiff::bind::carryFrom(&A::model(), &A::sink()); }

// The page written out: what is inside the document's root, without the
// root's own tags.
inline std::string inner(std::string html) {
  const std::size_t open = html.find('>');
  const std::size_t close = html.rfind("</");
  if (open == std::string::npos || close == std::string::npos || close < open) return html;
  return html.substr(open + 1, close - open - 1);
}
inline std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
inline bool write(const std::string &path, std::string_view text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
  return static_cast<bool>(out);
}
inline constexpr std::string_view kHead = R"(<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
)";
} // namespace detail

// The program: its tree (View, a function returning a view in skiff.ui) and
// what it adds (Hooks). Everything here is the program's one of each --
// static, as a program has one page and one canvas.
template <auto View, class Hooks = NoHooks> struct App {
  using Tree = decltype(View());
  using Model = skiff::ui::ModelOf<Tree>;
  using PageTree = skiff::ui::Mounted<skiff::ui::OnPage, Tree>;
  using SceneTree = detail::Root<App, skiff::ui::Mounted<skiff::ui::OnScene, Tree>>;
  static_assert(!skiff::ui::kOnScene || std::same_as<skiff::bind::RootOf<SceneTree>, typename Model::RootType>,
                "skiff::app: the tree says other parts on the scene than on the page -- one model is under both");

  static Model &model() {
    static Model kept;
    return kept;
  }
  static Hooks &hooks() {
    static Hooks kept;
    return kept;
  }
  // What nothing in the tree takes: a link followed, the program's to do.
  struct Sink {
    void take(const skiff::ui::OpenLink &link) const {
      if constexpr (requires { hooks().open(std::string_view(link.href)); })
        hooks().open(std::string_view(link.href));
#ifdef __EMSCRIPTEN__
      else
        skiffAppOpen(link.href.data(), static_cast<int>(link.href.size()));
#endif
    }
  };
  static Sink &sink() {
    static Sink kept;
    return kept;
  }

  // ---- the page ----
  static std::optional<skiff::web::Page<PageTree, Model>> &page() {
    static std::optional<skiff::web::Page<PageTree, Model>> kept;
    return kept;
  }
  // The stylesheets before the tree: skiff's theme, and the program's.
  static std::size_t sheets() {
    if constexpr (requires { hooks().stylesheet(); })
      return 2;
    else
      return 1;
  }
  static void mountPage() {
    const skiff::web::dom::Handle root = skiff::web::dom::root();
    skiff::ui::mountTheme(root);
    if constexpr (requires { hooks().stylesheet(); })
      skiff::web::mountStylesheet(root, hooks().stylesheet());
    page().emplace(skiff::ui::mount(skiff::ui::onPage, View()), model());
    page()->mount();
  }
  // What happens at a node of the page (skiff_web_event).
  static void event(std::uint32_t handle, const char *what, const char *value) {
    if (page()) page()->event(handle, std::string_view(what), std::string_view(value));
  }

  static void begin(bool arrived = false) {
    if constexpr (requires { hooks().dress(); })
      hooks().dress();
    if constexpr (requires { hooks().start(model()); })
      if (!arrived)
        hooks().start(model());
  }
  // The state the page was written with, written into it: where the model
  // travels (skiff.app.carry); else nothing.
  static std::string stateScript() { return skiff::app::carry::script(model().root()); }

  // Written out natively: `program [whole.html] [index.html]` -- the page
  // whole, and put into the page Emscripten made, in place of
  // @@SKIFF-PRERENDER@@ (text, which Emscripten keeps as it minifies the
  // page; a comment it takes out). With neither, to the output.
  static int prerender(int argc, char **argv) {
    begin();
    mountPage();
    // The page's events, compiled here as in the browser, never taken here:
    // what they answer is checked by this build too.
    if (argc < 0) event(0, "", "");
    const std::string markup = detail::inner(skiff::web::dom::dump(skiff::web::dom::root()));
    std::string head(detail::kHead);
    if constexpr (requires { hooks().head(); })
      head += std::string(hooks().head());
    const std::string state = stateScript();
    const std::string whole = "<!doctype html>\n<html lang=\"en\">\n<head>\n" + head + "</head>\n<body>\n<div id=\"skiff\">" + markup +
                              "</div>\n" + state + "\n</body>\n</html>\n";
    if (argc < 2) {
      std::print("{}", whole);
      return 0;
    }
    if (!detail::write(argv[1], whole)) {
      std::println(std::cerr, "skiff::app: cannot write {}", argv[1]);
      return 1;
    }
    if (argc >= 3) {
      std::string made = detail::read(argv[2]);
      constexpr std::string_view kMark = "@@SKIFF-PRERENDER@@";
      const std::size_t at = made.find(kMark);
      if (at == std::string::npos) {
        std::println(std::cerr, "skiff::app: no {} in {}", kMark, argv[2]);
        return 1;
      }
      made.replace(at, kMark.size(), markup);
      // The state beside it, where the page says where (@@SKIFF-STATE@@,
      // outside the page's root: what is taken over is the root's own).
      constexpr std::string_view kState = "@@SKIFF-STATE@@";
      if (const std::size_t there = made.find(kState); there != std::string::npos)
        made.replace(there, kState.size(), state);
      if (!detail::write(argv[2], made)) {
        std::println(std::cerr, "skiff::app: cannot write {}", argv[2]);
        return 1;
      }
    }
    return 0;
  }

#ifdef __EMSCRIPTEN__
  // ---- the canvas ----
  // Made the first time it is shown. The canvas stays at the top of the
  // window (sticky) in a box as tall as the scene, so the browser scrolls it
  // as it scrolls a page; the scene is drawn moved up by as much as the box
  // has gone by.
  struct Canvas {
    std::optional<skiff::scene::Scene<SceneTree>> scene;
    skiff::bind::Binding<Model> binding;
    std::unique_ptr<skia::SkCanvas> canvas;
    skia::Sp<skia::SkFontMgr> fonts;
    skia::SkFont font;
    int width = 0, height = 0;
    double scale = 0.0, offset = -1.0;
    float tall = 0.0f;
    bool shown = false;
    // Where the last frame outlined what it repainted: painted over next.
    std::vector<skia::SkRect> outlined;
  };
  static Canvas &canvas() {
    static Canvas kept;
    return kept;
  }
  static void makeCanvas() {
    Canvas &c = canvas();
    skiffAppCanvasMake();
    c.fonts = skia::SkFontMgr_New_Custom_Directory("");
    auto regular = c.fonts->matchFamilyStyle(nullptr, skia::SkFontStyle::Normal());
    auto bold = c.fonts->matchFamilyStyle(nullptr, skia::SkFontStyle::Bold());
    skiff::paint::fonts().setPrimary(regular, bold);
    c.font = skia::SkFont(regular);
    skiff::paint::defaultFont() = &c.font;
    skiff::scene::hostWork().pressed.clear();
    c.scene.emplace(std::in_place, skiff::ui::mount(skiff::ui::onScene, View()));
    c.binding.refresh(c.scene->root(), model());
  }
  // What was pressed and kept by its path -- where nothing was carried --
  // and what was answered and kept, delivered.
  static void deliver() {
    auto &work = skiff::scene::hostWork();
    auto &root = canvas().scene->root();
    for (const auto &way : std::exchange(work.pressed, {}))
      (void)skiff::bind::press(root, model(), way, &sink());
    for (const auto &one : std::exchange(work.answers, {}))
      (void)skiff::bind::answer(root, model(), one, &sink());
  }
  static void canvasFrame(double nowMs) {
    Canvas &h = canvas();
    const int width = skiffAppCanvasWidth();
    const int height = skiffAppWindowHeight();
    const double scale = skiffAppWindowScale();
    if (width <= 0 || height <= 0) return;
    bool whole = false;
    if (width != h.width || height != h.height || scale != h.scale || !h.canvas) {
      h.width = width;
      h.height = height;
      h.scale = scale;
      h.canvas = skia::SkCanvas::OnPage("skiff-canvas", width, height, static_cast<float>(scale));
      whole = true;
    }
    deliver();
    h.binding.refresh(h.scene->root(), model());
    h.scene->update(nowMs);
    (void)h.scene->layoutIfNeeded(skia::SkRect::MakeXYWH(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)));
    const skiff::scene::FrameResult frame = h.scene->finishFrame();
    deliver();
    const float tall = h.scene->rootState().fBounds.height();
    if (tall != h.tall) {
      h.tall = tall;
      skiffAppCanvasTall(std::max(tall, static_cast<float>(height)));
    }
    const double offset = skiffAppCanvasOffset();
    if (offset != h.offset) {
      h.offset = offset;
      whole = true;
    }
    // What is repainted: all of it, where the canvas or the scroll moved;
    // else what the scene damaged, piece by piece -- and where the last
    // frame outlined its repaints, under those outlines, so none is left.
    const skia::SkRect seen =
        skia::SkRect::MakeXYWH(0.0f, static_cast<float>(offset), static_cast<float>(width), static_cast<float>(height));
    std::vector<skia::SkRect> pieces;
    if (whole) {
      pieces.push_back(seen);
      h.outlined.clear();
    } else if (!frame.fDamageRects.empty()) {
      pieces = frame.fDamageRects;
    } else if (!frame.fDamage.isEmpty()) {
      pieces.push_back(frame.fDamage);
    }
    std::vector<skia::SkRect> painted = pieces;
    for (const skia::SkRect &under : std::exchange(h.outlined, {}))
      painted.push_back(under);
    std::erase_if(painted, [&](skia::SkRect &piece) { return !piece.intersect(seen); });
    if (painted.empty()) return;
    skia::SkCanvas &c = *h.canvas;
    skiff::scene::detail::PlainPaint paint;
    skia::SkPaint paper;
    if constexpr (requires { hooks().paper(); })
      paper.setColor(hooks().paper());
    else
      paper.setColor(0xffffffffu);
    c.save();
    c.translate(0.0f, -static_cast<float>(offset));
    for (const skia::SkRect &piece : painted) {
      c.save();
      c.clipRect(piece);
      c.drawRect(piece, paper);
      h.scene->draw(paint, &c);
      c.restore();
    }
    // What this frame repainted, outlined, where that is asked for: drawn
    // over the frame, and painted over at the next.
    bool outline = false;
    if constexpr (requires { hooks().outlineRepaints(std::as_const(model())); })
      outline = hooks().outlineRepaints(std::as_const(model()));
    if (outline) {
      skia::SkPaint stroke;
      stroke.setStyle(skia::SkPaint::kStroke_Style);
      stroke.setStrokeWidth(2.0f);
      stroke.setColor(0xdcff00a0u);
      for (const skia::SkRect &piece : pieces) {
        if (piece.isEmpty()) continue;
        c.drawRect(piece.makeInset(1.0f, 1.0f), stroke);
        h.outlined.push_back(piece.makeOutset(2.0f, 2.0f));
      }
    }
    c.restore();
  }
  // The page or the canvas shown, the window scrolled where it was: the one
  // coming is put in place first -- the canvas laid out and its box as tall
  // as the scene -- and only then the other goes, so the document is never
  // for a moment shorter than the scroll and the browser never takes it back
  // to the top.
  static void show(bool onCanvas) {
    Canvas &h = canvas();
    if (onCanvas && !h.scene) makeCanvas();
    const double at = skiffAppScrollY();
    h.offset = -1.0;
    if (onCanvas) {
      skiffAppShowCanvas(1);
      h.shown = true;
      canvasFrame(emscripten_get_now());
      skiffAppShowPage(0);
    } else {
      skiffAppShowPage(1);
      h.shown = false;
      skiffAppShowCanvas(0);
    }
    skiffAppScrollTo(at);
  }
  // The pointer, a key, words typed, on the canvas (skiff_canvas_*).
  static void pointer(int kind, float x, float y, int button) {
    Canvas &h = canvas();
    if (!h.scene || !h.shown) return;
    auto &s = *h.scene;
    switch (kind) {
    case 0: (void)s.dispatchPointer(skiff::scene::pointer::move{x, y}); break;
    case 1: (void)s.dispatchPointer(skiff::scene::pointer::down{x, y, button}); break;
    case 2: (void)s.dispatchPointer(skiff::scene::pointer::up{x, y, button}); break;
    default: (void)s.dispatchPointer(skiff::scene::pointer::cancel{x, y}); break;
    }
  }
  static int key(int code, int shift, int control) {
    Canvas &h = canvas();
    if (!h.scene || !h.shown) return 0;
    skiff::scene::Modifiers modifiers;
    if (shift) modifiers = modifiers | skiff::scene::modifiers::kShift;
    if (control) modifiers = modifiers | skiff::scene::modifiers::kControl;
    return h.scene->dispatchKey(skiff::scene::key::down{skiff::scene::Key{static_cast<std::uint16_t>(code)}, modifiers, false}) ? 1 : 0;
  }
  static int text(const char *typed) {
    Canvas &h = canvas();
    if (!h.scene || !h.shown) return 0;
    const std::string kept(typed);
    return h.scene->dispatchText(skiff::scene::text::commit{kept}) ? 1 : 0;
  }

  // ---- the frame ----
  static double &startedAt() {
    static double kept = 0.0;
    return kept;
  }
  static void tick() {
    const double now = emscripten_get_now();
    if constexpr (requires { hooks().frame(model(), 0.0); })
      hooks().frame(model(), (now - startedAt()) / 1000.0);
    bool onCanvas = false;
    if constexpr (requires { hooks().onCanvas(std::as_const(model())); })
      onCanvas = hooks().onCanvas(std::as_const(model()));
    if (onCanvas != canvas().shown) show(onCanvas);
    if (canvas().shown)
      canvasFrame(now);
    else
      page()->refresh();
  }
  // In the browser: the copy written when the program was built -- the
  // stylesheets, then the page -- taken over, not made again: the same
  // elements, now the live page's. Where there is none, made.
  // The state the page was written with, read into the model: whether it
  // came.
  static bool arrive() {
    char *text = skiffAppStateText();
    if (!text) return false;
    const std::string kept(text);
    std::free(text);
    auto read = skiff::app::carry::read<typename Model::RootType>(kept);
    if (!read) return false;
    model() = Model(std::move(*read));
    return true;
  }
  static int live() {
    begin(arrive());
    const skiff::web::dom::Handle root = skiff::web::dom::root();
    if (skiff::web::dom::children(root) > sheets()) {
      page().emplace(skiff::ui::mount(skiff::ui::onPage, View()), model());
      page()->hydrate(root, sheets());
    } else {
      mountPage();
    }
    startedAt() = emscripten_get_now();
    emscripten_set_main_loop([] { tick(); }, 0, 0);
    return 0;
  }
#endif

  // The program: alive in a browser, written out natively.
  static int run(int argc, char **argv) {
#ifdef __EMSCRIPTEN__
    (void)argc;
    (void)argv;
    return live();
#else
    return prerender(argc, argv);
#endif
  }
};

} // namespace skiff::app
