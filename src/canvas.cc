// skiff.app.canvas: skiff.app's tree drawn by skiff's scene on a canvas,
// instead of the page's elements -- the framework's canvas feature. The
// canvas is made the first time the program says it is to be shown
// (Hooks::onCanvas), one model under the page and the canvas; drawn by
// skiff's Skia stand-in, through the browser's 2D context. Without the
// feature, canvas_off.cc answers this module with nothing.
module;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
export module skiff.app.canvas;

import std;
import skiff.ui;

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
  if (page) page.style.display = on ? "" : "none";
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
// clang-format on
#endif

export namespace skiff::app::canvas {

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

template <class A> struct On {
  using SceneTree = Root<A, skiff::ui::Mounted<skiff::ui::OnScene, typename A::Tree>>;
  static_assert(std::same_as<skiff::bind::RootOf<SceneTree>, typename A::Model::RootType>,
                "skiff::app: the tree says other parts on the scene than on the page -- one model is under both");

#ifdef __EMSCRIPTEN__
  // Made the first time it is shown. The canvas stays at the top of the
  // window (sticky) in a box as tall as the scene, so the browser scrolls it
  // as it scrolls a page; the scene is drawn moved up by as much as the box
  // has gone by.
  struct Held {
    std::optional<skiff::scene::Scene<SceneTree>> scene;
    skiff::bind::Binding<typename A::Model> binding;
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
  static Held &canvas() {
    static Held kept;
    return kept;
  }
  static void makeCanvas() {
    Held &c = canvas();
    skiffAppCanvasMake();
    c.fonts = skia::SkFontMgr_New_Custom_Directory("");
    auto regular = c.fonts->matchFamilyStyle(nullptr, skia::SkFontStyle::Normal());
    auto bold = c.fonts->matchFamilyStyle(nullptr, skia::SkFontStyle::Bold());
    skiff::paint::fonts().setPrimary(regular, bold);
    c.font = skia::SkFont(regular);
    skiff::paint::defaultFont() = &c.font;
    skiff::scene::hostWork().pressed.clear();
    c.scene.emplace(std::in_place, skiff::ui::mount(skiff::ui::onScene, A::view()));
    c.binding.refresh(c.scene->root(), A::model());
  }
  // What was pressed and kept by its path -- where nothing was carried --
  // and what was answered and kept, delivered.
  static void deliver() {
    auto &work = skiff::scene::hostWork();
    auto &root = canvas().scene->root();
    for (const auto &way : std::exchange(work.pressed, {}))
      (void)skiff::bind::press(root, A::model(), way, &A::sink());
    for (const auto &one : std::exchange(work.answers, {}))
      (void)skiff::bind::answer(root, A::model(), one, &A::sink());
  }
  static void frame(double nowMs) {
    Held &h = canvas();
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
    h.binding.refresh(h.scene->root(), A::model());
    h.scene->update(nowMs);
    (void)h.scene->layoutIfNeeded(skia::SkRect::MakeXYWH(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)));
    const skiff::scene::FrameResult done = h.scene->finishFrame();
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
    } else if (!done.fDamageRects.empty()) {
      pieces = done.fDamageRects;
    } else if (!done.fDamage.isEmpty()) {
      pieces.push_back(done.fDamage);
    }
    std::vector<skia::SkRect> painted = pieces;
    for (const skia::SkRect &under : std::exchange(h.outlined, {}))
      painted.push_back(under);
    std::erase_if(painted, [&](skia::SkRect &piece) { return !piece.intersect(seen); });
    if (painted.empty()) return;
    skia::SkCanvas &c = *h.canvas;
    skiff::scene::detail::PlainPaint paint;
    skia::SkPaint paper;
    if constexpr (requires { A::hooks().paper(); })
      paper.setColor(A::hooks().paper());
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
    if constexpr (requires { A::hooks().outlineRepaints(std::as_const(A::model())); })
      outline = A::hooks().outlineRepaints(std::as_const(A::model()));
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
    Held &h = canvas();
    if (onCanvas && !h.scene) makeCanvas();
    const double at = skiffAppScrollY();
    h.offset = -1.0;
    if (onCanvas) {
      skiffAppShowCanvas(1);
      h.shown = true;
      frame(emscripten_get_now());
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
    Held &h = canvas();
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
    Held &h = canvas();
    if (!h.scene || !h.shown) return 0;
    skiff::scene::Modifiers modifiers;
    if (shift) modifiers = modifiers | skiff::scene::modifiers::kShift;
    if (control) modifiers = modifiers | skiff::scene::modifiers::kControl;
    return h.scene->dispatchKey(skiff::scene::key::down{skiff::scene::Key{static_cast<std::uint16_t>(code)}, modifiers, false}) ? 1 : 0;
  }
  static int text(const char *typed) {
    Held &h = canvas();
    if (!h.scene || !h.shown) return 0;
    const std::string kept(typed);
    return h.scene->dispatchText(skiff::scene::text::commit{kept}) ? 1 : 0;
  }

  [[nodiscard]] static bool shown() { return canvas().shown; }
#else
  // Natively nothing is drawn: the page is written out.
  [[nodiscard]] static bool shown() { return false; }
  static void show(bool) {}
  static void frame(double) {}
  static void pointer(int, float, float, int) {}
  static int key(int, int, int) { return 0; }
  static int text(const char *) { return 0; }
#endif
};

} // namespace skiff::app::canvas
