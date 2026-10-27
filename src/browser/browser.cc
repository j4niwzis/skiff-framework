// SPDX-License-Identifier: AGPL-3.0-only
// The scene and its bindings, hosted by the browser's DOM.
module;
#include <emscripten.h>
export module skiff.platform.browser;
import std;
import splice;
import sdl;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.web;
import skiff.mirror;
import skiff.platform.window;
import skiff.platform.events;
import skiff.platform.fonts;
import skiff.platform.clipboard;

namespace skiff::platform::browser::detail {
struct pointer {
  web::dom::Handle element;
  int action, button;
  float x, y;
};
struct edit { web::dom::Handle element; std::string text; };
struct selection { web::dom::Handle element; std::size_t anchor, caret; };
struct files { std::vector<std::string> paths; };
struct file_error { std::string why; };
inline std::vector<std::string> pending_files;
struct key { web::dom::Handle element; std::uint16_t code; unsigned modifiers; bool repeated; };
using event = std::variant<pointer, edit, selection, key, files, file_error>;
inline std::deque<event> events;
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_pointer(unsigned element, int action, float x, float y, int button) {
  skiff::platform::browser::detail::events.emplace_back(skiff::platform::browser::detail::pointer{element, action, button, x, y});
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_input(unsigned element, const char *text) {
  skiff::platform::browser::detail::events.emplace_back(skiff::platform::browser::detail::edit{element, text});
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_key(unsigned element, int code, unsigned modifiers, int repeated) {
  skiff::platform::browser::detail::events.emplace_back(skiff::platform::browser::detail::key{element, static_cast<std::uint16_t>(code), modifiers, repeated != 0});
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_selection(unsigned element, unsigned anchor, unsigned caret) {
  skiff::platform::browser::detail::events.emplace_back(skiff::platform::browser::detail::selection{element, anchor, caret});
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_file(const char *path, int last) {
  namespace detail = skiff::platform::browser::detail;
  if (path && *path) detail::pending_files.emplace_back(path);
  if (last) detail::events.emplace_back(detail::files{std::exchange(detail::pending_files, {})});
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_file_error(const char *why) {
  skiff::platform::browser::detail::pending_files.clear();
  skiff::platform::browser::detail::events.emplace_back(skiff::platform::browser::detail::file_error{why});
}
// clang-format off
EM_JS(void, skiffBrowserInstall, (const char *css), {
  const style = document.createElement('style');style.textContent = UTF8ToString(css);document.head.append(style);
  let held = null;
  const node = target => target instanceof Element ? target.closest('[data-skiff-h]') : null;
  const field = target => target instanceof Element && !!target.closest('input,textarea,select,[contenteditable]');
  const point = (e,action) => {
    if (field(e.target) && e.button !== 2) return;
    const el = held || node(e.target);if (!el) return;
    if (action === 1) held = el;
    const r = el.getBoundingClientRect(), zoom = Module.skiffBrowserZoom || 1;
    Module.ccall('skiff_browser_pointer',null,['number','number','number','number','number'],
      [Number(el.dataset.skiffH),action,(e.clientX-r.left)/zoom,(e.clientY-r.top)/zoom,e.button+1]);
    if (action === 3) held = null;
  };
  document.addEventListener('pointerdown',e => point(e,1));
  document.addEventListener('pointermove',e => point(e,2));
  document.addEventListener('pointerup',e => point(e,3));
  document.addEventListener('pointercancel',e => {point(e,3);held = null;});
  document.addEventListener('contextmenu',e => {if (node(e.target)) e.preventDefault();});
  document.addEventListener('input',e => {
    const el = node(e.target);if (!el || !('value' in e.target)) return;
    Module.ccall('skiff_browser_input',null,['number','string'],[Number(el.dataset.skiffH),e.target.value]);
  });
  const selected = target => {
    const el = node(target);if (!el || typeof target.selectionStart !== 'number') return;
    const first = new TextEncoder().encode(target.value.slice(0,target.selectionStart)).length;
    const last = new TextEncoder().encode(target.value.slice(0,target.selectionEnd)).length;
    const backwards = target.selectionDirection === 'backward';
    Module.ccall('skiff_browser_selection',null,['number','number','number'],
      [Number(el.dataset.skiffH),backwards ? last:first,backwards ? first:last]);
  };
  document.addEventListener('select',e => selected(e.target));
  document.addEventListener('selectionchange',() => {if (field(document.activeElement)) selected(document.activeElement);});
  document.addEventListener('input',e => selected(e.target));
  document.addEventListener('click',e => {if (node(e.target) && e.target.closest('a')) e.preventDefault();});
  const importFiles = async files => {
    try {
      FS.mkdirTree('/uploads');
      for (const file of files) {
        Module.skiffUploadSequence = (Module.skiffUploadSequence || 0)+1;
        // Numeric codes survive EM_JS's C string processing unchanged.
        const name = Array.from(file.name, part => {
          const code = part.codePointAt(0);
          return code < 32 || code === 47 || code === 92 ? '_' : part;
        }).join("") || 'file';
        const path = '/uploads/drop-'+Module.skiffUploadSequence+'-'+name;
        FS.writeFile(path,new Uint8Array(await file.arrayBuffer()));
        Module.ccall('skiff_browser_file',null,['string','number'],[path,0]);
      }
      Module.ccall('skiff_browser_file',null,['string','number'],["",1]);
    } catch (error) {Module.ccall('skiff_browser_file_error',null,['string'],[String(error.message || error)]);}
  };
  document.addEventListener('dragover',e => {if (e.dataTransfer?.types.includes('Files')) e.preventDefault();});
  document.addEventListener('drop',e => {
    if (!e.dataTransfer?.files.length) return;
    e.preventDefault();void importFiles(Array.from(e.dataTransfer.files));
  });
  document.addEventListener('paste',e => {
    if (!e.clipboardData?.files.length) return;
    e.preventDefault();void importFiles(Array.from(e.clipboardData.files));
  });
  const keys = {Tab:1,Enter:2,Space:3,Escape:4,ArrowLeft:5,ArrowRight:6,ArrowUp:7,ArrowDown:8,Home:9,End:10,Backspace:11,Delete:12,PageUp:13,PageDown:14};
  document.addEventListener('keydown',e => {
    if (e.isComposing) return;
    const editing = field(e.target), command = e.ctrlKey || e.metaKey;
    // The browser edits its own fields; program actions (including Enter)
    // go through the same key handlers as the desktop app.
    if (editing && e.code !== 'Enter' && e.code !== 'NumpadEnter' && e.code !== 'Escape' &&
        (!command || ['KeyA','KeyC','KeyX','KeyV','KeyZ','KeyY','ArrowLeft','ArrowRight','Backspace','Delete'].includes(e.code))) return;
    let code = keys[e.code] || (e.code === 'NumpadEnter' ? 2 : 0);
    if (/^Key[A-Z]$/.test(e.code)) code = 101+e.code.charCodeAt(3)-65;
    if (/^Digit[0-9]$/.test(e.code)) code = 130+e.code.charCodeAt(5)-48;
    if (!code) return;
    e.preventDefault();
    selected(e.target);
    const el = node(e.target);
    Module.ccall('skiff_browser_key',null,['number','number','number','number'],
      [el ? Number(el.dataset.skiffH):0,code,(e.shiftKey?1:0)|(command?2:0)|(e.altKey?4:0),e.repeat?1:0]);
  });
});
EM_JS(void, skiffBrowserSelection, (), {
  const field = document.activeElement;
  if (!field?.hasAttribute('data-skiff-caret')) return;
  const anchor = Number(field.dataset.skiffAnchor), caret = Number(field.dataset.skiffCaret);
  const stamp = anchor+':'+caret+':'+field.value;
  if (field.skiffSelectionStamp === stamp) return;
  field.skiffSelectionStamp = stamp;
  const bytes = new TextEncoder().encode(field.value), decoder = new TextDecoder();
  const first = decoder.decode(bytes.subarray(0,Math.min(anchor,caret))).length;
  const last = decoder.decode(bytes.subarray(0,Math.max(anchor,caret))).length;
  field.setSelectionRange(first,last,anchor > caret ? 'backward':'forward');
});
EM_JS(float, skiffBrowserWidth, (), {return innerWidth;});
EM_JS(float, skiffBrowserHeight, (), {return innerHeight;});
EM_JS(float, skiffBrowserScale, (), {return devicePixelRatio || 1;});
EM_JS(int, skiffBrowserVisible, (), {return document.visibilityState !== 'hidden';});
EM_JS(void, skiffBrowserZoom, (float zoom), {
  Module.skiffBrowserZoom = zoom;
  const root = document.getElementById('skiff') || document.body;
  root.style.zoom = zoom;root.style.width = (100/zoom)+'%';root.style.height = (100/zoom)+'%';
});
EM_JS(float, skiffBrowserScroll, (unsigned handle), {return Module.skiffDom.els[handle]?.scrollTop || 0;});
// clang-format on

export namespace skiff::platform::browser {
template <class App> int run(App &app, const window::options &how, const events::kinds &kinds) {
  namespace hooks = window::hooks;
  if (!sdl::SDL_Init(sdl::kInitEvents | sdl::kInitAudio)) return 1;
  auto &scene = app.window();
  scene::hostWork().laidOutElsewhere = true;
  web::dom::reset();
  skia::SkFont font;
  fonts::load_fonts<typename window::faces_of<App>::type>(how.fonts, font);
  const auto css = "html,body,#skiff{margin:0;padding:0;width:100%;height:100%;overflow:hidden}body{font-family:'Open Sans',sans-serif}" + web::pageCss();
  skiffBrowserInstall(css.c_str());
  hooks::dialogs_over(app, static_cast<sdl::SDL_Window *>(nullptr), kinds);
  mirror::Mirror mirror;
  scene::InputRouter router;
  const std::array layers{scene::InputRouter::Layer{scene.handle(), false}};
  router.setLayers(layers);
  bool running = true;
  while (running) {
    sdl::SDL_Event event;
    while (sdl::SDL_PollEvent(&event)) {
      if (event.type == sdl::SDL_EVENT_QUIT) running = false;
      else if (event.type == kinds.wake) hooks::woken(app);
      else if (event.type == kinds.files) hooks::dialogs_answered(app);
      else if (event.type == kinds.save) hooks::dialogs_saved(app);
      hooks::after_event(app);
    }
    for (const auto &event : std::exchange(detail::events, {})) {
      std::visit(spl::overloaded{
        [&](const detail::edit &edit) { (void)mirror.typed(scene, edit.element, edit.text); },
        [&](const detail::files &files) { hooks::files_given(app, files.paths, true); },
        [&](const detail::file_error &error) { hooks::dialog_failed(app, error.why); },
        [&](const detail::selection &selection) { (void)mirror.selected(scene, selection.element, selection.anchor, selection.caret); },
        [&](const detail::key &key) {
          scene::Modifiers held;
          if (key.modifiers & 1u) held = held | scene::modifiers::kShift;
          if (key.modifiers & 2u) held = held | scene::modifiers::kControl;
          if (key.modifiers & 4u) held = held | scene::modifiers::kAlt;
          (void)mirror.keyed(scene, key.element, scene::key::down{scene::Key{key.code}, held, key.repeated});
        },
        [&](const detail::pointer &point) {
          if (const auto at = mirror.toScene(point.element, point.x, point.y)) {
            switch (point.action) {
              case 1: router.pointer(scene::pointer::down{at->fX, at->fY, point.button}); break;
              case 2: router.pointer(scene::pointer::move{at->fX, at->fY}); break;
              case 3: router.pointer(scene::pointer::up{at->fX, at->fY, point.button}); break;
              default: break;
            }
          }
        }}, event);
      hooks::after_event(app);
    }
    hooks::after_event(app);
    const bool visible = skiffBrowserVisible() != 0;
    hooks::shown_changed(app, visible);
    if (visible) {
      const float zoom = static_cast<float>(std::max(1, hooks::interface_scale(app))) / 100.0f;
      skiffBrowserZoom(zoom);
      mirror.readScrolled(scene.root(), [](web::dom::Handle handle) { return skiffBrowserScroll(handle); });
      hooks::before_frame(app);
      scene.update(emscripten_get_now());
      scene::pixelScale() = skiffBrowserScale() * zoom;
      scene.layoutIfNeeded(skia::SkRect::MakeWH(skiffBrowserWidth() / zoom, skiffBrowserHeight() / zoom));
      mirror.sync(scene.root());
      skiffBrowserSelection();
      if (auto copied = std::exchange(scene::hostWork().copied, std::nullopt)) {
        const auto &rich = scene::clipboardRichText();
        clipboard::copy_text(*copied, rich ? clipboard::emoji_html(*rich) : std::string());
      }
      for (auto &link : std::exchange(scene::hostWork().links, {})) hooks::open_link(app, std::move(link));
    }
    emscripten_sleep(visible ? 16 : 100);
  }
  hooks::closing(app);
  sdl::SDL_Quit();
  return 0;
}
}
