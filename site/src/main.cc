// The site, as a program. In a browser (Emscripten): the page mounted over
// the copy written when the site was built, its clock moved at every frame;
// and, chosen on it, the same tree drawn by skiff's scene on a canvas
// (site.canvas) -- one model under both, the switch an edit of it.
// Natively, when the site is built: the page mounted on the document kept in
// memory and written out -- static.html whole, with no script; and, given
// the page Emscripten made, the same markup put into it, for the browser to
// show before the WebAssembly has loaded.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
import std;
import skiff.ui;
import site.model;
import site.page;
#ifdef __EMSCRIPTEN__
import skia;
import skiff.scene;
import skiff.bind;
import skiff.paint;
import site.canvas;
#endif

namespace {
site::Model gModel;
using Tree = decltype(site::html::page());
std::optional<skiff::ui::Page<Tree, site::Model>> gPage;

void mountPage() {
  skiff::ui::mountTheme(skiff::web::dom::root());
  skiff::web::mountStylesheet(skiff::web::dom::root(), site::html::kSiteCss);
  gPage.emplace(site::html::page(), gModel);
  gPage->mount();
}
} // namespace

#ifdef __EMSCRIPTEN__
// ---- the canvas: the same tree, on skiff's scene ----

// The page's side of the canvas, in JavaScript: the box and the canvas made,
// the pointer and the keys passed in, the box shown, measured and scrolled.
// clang-format off
EM_JS(void, siteCanvasMake, (), {
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
EM_JS(void, siteCanvasShow, (int on), {
  const host = document.getElementById('skiff-canvas-host');
  const page = document.getElementById('skiff');
  if (page) page.style.display = on ? 'none' : '';
  if (host) host.style.display = on ? 'block' : 'none';
});
EM_JS(int, siteCanvasWidth, (), {
  const host = document.getElementById('skiff-canvas-host');
  return host ? host.clientWidth : 0;
});
EM_JS(int, siteWindowHeight, (), { return window.innerHeight; });
EM_JS(double, siteWindowScale, (), { return window.devicePixelRatio || 1; });
EM_JS(void, siteCanvasTall, (float px), { document.getElementById('skiff-canvas-host').style.height = px + 'px'; });
// How far the box has gone up past the canvas, which stays at the top.
EM_JS(double, siteCanvasOffset, (), {
  const host = document.getElementById('skiff-canvas-host');
  const c = document.getElementById('skiff-canvas');
  return c.getBoundingClientRect().top - host.getBoundingClientRect().top;
});
EM_JS(void, siteOpen, (const char *href), { window.location.href = UTF8ToString(href); });
// clang-format on

namespace site {
// What nothing in the tree takes, the program does: a link followed.
struct CanvasSink {
  void take(const canvas::OpenLink &link) const {
    siteOpen(link.href.c_str());
  }
};
using CanvasTree = decltype(canvas::scenePage());
} // namespace site

namespace {
site::CanvasSink gSink;
} // namespace

// What the scene's routing starts with, found by its root's type: the model
// its scopes edit and the program's sink -- so a press, and what a field
// typed into answers, is answered where it is made. In a release build the
// root is the composed type, of site's; outside one a compose::Held.
namespace site {
template <class R>
  requires(!skiff::scene::kErasedCombinators && std::same_as<R, CanvasTree>)
auto startCarry(R &) {
  return skiff::bind::carryFrom(&gModel, &gSink);
}
} // namespace site
namespace skiff::compose {
template <class R>
  requires(skiff::scene::kErasedCombinators && std::same_as<R, ::site::CanvasTree>)
auto startCarry(R &) {
  return skiff::bind::carryFrom(&gModel, &gSink);
}
} // namespace skiff::compose

namespace {
// The canvas and the scene drawn on it, made the first time it is chosen.
// The canvas stays at the top of the window (sticky) in a box as tall as the
// scene, so the browser scrolls as it scrolls a page; the scene is drawn
// moved up by as much as the box has gone by.
struct CanvasHost {
  std::optional<skiff::scene::Scene<site::CanvasTree>> scene;
  skiff::bind::Binding<site::Model> binding;
  std::unique_ptr<skia::SkCanvas> canvas;
  skia::Sp<skia::SkFontMgr> fonts;
  skia::SkFont font;
  int width = 0, height = 0;
  double scale = 0.0, offset = -1.0;
  float tall = 0.0f;
  bool shown = false;
};
CanvasHost gCanvas;

void makeCanvas() {
  siteCanvasMake();
  gCanvas.fonts = skia::SkFontMgr_New_Custom_Directory("");
  auto regular = gCanvas.fonts->matchFamilyStyle(nullptr, skia::SkFontStyle::Normal());
  auto bold = gCanvas.fonts->matchFamilyStyle(nullptr, skia::SkFontStyle::Bold());
  skiff::paint::fonts().setPrimary(regular, bold);
  gCanvas.font = skia::SkFont(regular);
  skiff::paint::defaultFont() = &gCanvas.font;
  skiff::scene::hostWork().pressed.clear();
  gCanvas.scene.emplace(std::in_place, site::canvas::scenePage());
  gCanvas.binding.refresh(gCanvas.scene->root(), gModel);
}

// The page or the canvas shown, as the model says.
void show(bool canvas) {
  if (canvas && !gCanvas.scene) makeCanvas();
  gCanvas.shown = canvas;
  gCanvas.offset = -1.0;
  siteCanvasShow(canvas ? 1 : 0);
}

// What was pressed and kept by its path -- where nothing was carried -- and
// what was answered and kept, delivered.
void deliver() {
  auto &work = skiff::scene::hostWork();
  for (const auto &way : std::exchange(work.pressed, {}))
    (void)skiff::bind::press(gCanvas.scene->root(), gModel, way, &gSink);
  for (const auto &one : std::exchange(work.answers, {}))
    (void)skiff::bind::answer(gCanvas.scene->root(), gModel, one, &gSink);
}

void canvasFrame(double nowMs) {
  CanvasHost &h = gCanvas;
  const int width = siteCanvasWidth();
  const int height = siteWindowHeight();
  const double scale = siteWindowScale();
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
  h.binding.refresh(h.scene->root(), gModel);
  h.scene->update(nowMs);
  (void)h.scene->layoutIfNeeded(skia::SkRect::MakeXYWH(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)));
  const skiff::scene::FrameResult frame = h.scene->finishFrame();
  deliver();
  const float tall = h.scene->rootState().fBounds.height();
  if (tall != h.tall) {
    h.tall = tall;
    siteCanvasTall(std::max(tall, static_cast<float>(height)));
  }
  const double offset = siteCanvasOffset();
  if (offset != h.offset) {
    h.offset = offset;
    whole = true;
  }
  if (!whole && frame.fDamage.isEmpty()) return;
  skia::SkCanvas &c = *h.canvas;
  skiff::scene::detail::PlainPaint paint;
  skia::SkPaint paper;
  paper.setColor(site::canvas::colour::kPaper);
  c.save();
  c.translate(0.0f, -static_cast<float>(offset));
  const skia::SkRect seen = skia::SkRect::MakeXYWH(0.0f, static_cast<float>(offset), static_cast<float>(width), static_cast<float>(height));
  skia::SkRect redrawn = whole ? seen : frame.fDamage;
  if (!whole && !redrawn.intersect(seen)) {
    c.restore();
    return;
  }
  c.clipRect(redrawn);
  c.drawRect(redrawn, paper);
  h.scene->draw(paint, &c);
  c.restore();
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void skiff_canvas_pointer(int kind, float x, float y, int button) {
  if (!gCanvas.scene || !gCanvas.shown) return;
  auto &s = *gCanvas.scene;
  switch (kind) {
  case 0: (void)s.dispatchPointer(skiff::scene::pointer::move{x, y}); break;
  case 1: (void)s.dispatchPointer(skiff::scene::pointer::down{x, y, button}); break;
  case 2: (void)s.dispatchPointer(skiff::scene::pointer::up{x, y, button}); break;
  default: (void)s.dispatchPointer(skiff::scene::pointer::cancel{x, y}); break;
  }
}
extern "C" EMSCRIPTEN_KEEPALIVE int skiff_canvas_key(int code, int shift, int control) {
  if (!gCanvas.scene || !gCanvas.shown) return 0;
  skiff::scene::Modifiers modifiers;
  if (shift) modifiers = modifiers | skiff::scene::modifiers::kShift;
  if (control) modifiers = modifiers | skiff::scene::modifiers::kControl;
  return gCanvas.scene->dispatchKey(skiff::scene::key::down{skiff::scene::Key{static_cast<std::uint16_t>(code)}, modifiers, false}) ? 1 : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int skiff_canvas_text(const char *text) {
  if (!gCanvas.scene || !gCanvas.shown) return 0;
  const std::string kept(text);
  return gCanvas.scene->dispatchText(skiff::scene::text::commit{kept}) ? 1 : 0;
}

namespace {
double gStart = 0.0;
void tick() {
  const double now = emscripten_get_now();
  const double seconds = (now - gStart) / 1000.0;
  (void)gModel.apply(skiff::model::over<site::Clock>(skiff::model::setTo(site::Clock{seconds})));
  const bool canvas = gModel.root().shown.fValue.backend == site::Backend::kScene;
  if (canvas != gCanvas.shown) show(canvas);
  if (gCanvas.shown) canvasFrame(now);
  else gPage->refresh();
}
} // namespace
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_web_event(std::uint32_t handle, const char *event, const char *value) {
  if (gPage) gPage->event(handle, std::string_view(event), std::string_view(value));
}
int main() {
  site::seed(gModel);
  (void)gModel.apply(skiff::model::over<site::Shown>([](site::Shown s) {
    s.live = true;
    return s;
  }));
  // The copy written when the site was built -- the theme, the site's
  // stylesheet, then the page -- taken over, not made again: the same
  // elements, now the live page's. Where there is none, made.
  const skiff::web::dom::Handle root = skiff::web::dom::root();
  if (skiff::web::dom::children(root) >= 3) {
    gPage.emplace(site::html::page(), gModel);
    gPage->hydrate(root, 2);
  } else {
    mountPage();
  }
  gStart = emscripten_get_now();
  emscripten_set_main_loop(tick, 0, 0);
  return 0;
}
#else
namespace {
// What is inside the document's root, without the root's own tags.
std::string inner(std::string html) {
  const std::size_t open = html.find('>');
  const std::size_t close = html.rfind("</");
  if (open == std::string::npos || close == std::string::npos || close < open) return html;
  return html.substr(open + 1, close - open - 1);
}
std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
bool write(const std::string &path, std::string_view text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
  return static_cast<bool>(out);
}
constexpr std::string_view kHead = R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>skiff -- a C++ UI library</title>
<meta name="description" content="skiff: a C++ UI library -- a typed tree, a model it is bound to, and two backends: a scene drawn by Skia, and the page.">
</head>
<body>
<div id="skiff">)";
constexpr std::string_view kTail = "</div>\n</body>\n</html>\n";
} // namespace

// site [static.html] [index.html]: the page written out whole, and put into
// the page Emscripten made, in place of @@SKIFF-PRERENDER@@ -- text, which
// Emscripten keeps as it minifies the page; a comment it takes out.
int main(int argc, char **argv) {
  site::seed(gModel);
  mountPage();
  // The page's events, compiled here as in the browser (never taken here):
  // what they answer is checked by this build too.
  if (argc < 0) gPage->event(0, {}, {});
  const std::string markup = inner(skiff::web::dom::dump(skiff::web::dom::root()));
  if (argc < 2) {
    std::print("{}{}{}", kHead, markup, kTail);
    return 0;
  }
  if (!write(argv[1], std::string(kHead) + markup + std::string(kTail))) {
    std::println(std::cerr, "site: cannot write {}", argv[1]);
    return 1;
  }
  if (argc >= 3) {
    std::string page = read(argv[2]);
    constexpr std::string_view kMark = "@@SKIFF-PRERENDER@@";
    const std::size_t at = page.find(kMark);
    if (at == std::string::npos) {
      std::println(std::cerr, "site: no {} in {}", kMark, argv[2]);
      return 1;
    }
    page.replace(at, kMark.size(), markup);
    if (!write(argv[2], page)) {
      std::println(std::cerr, "site: cannot write {}", argv[2]);
      return 1;
    }
  }
  return 0;
}
#endif
