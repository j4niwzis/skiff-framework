// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page -- the program's scene laid out and painted by
// an HTML engine embedded and called directly: Blitz (Stylo's CSS, Taffy's
// layout, Vello painting on the processor), no JavaScript and no browser.
//
// The program is the window's (window::run's App): its scene, its hooks.
// Each frame the scene is mirrored into skiff.web's document, which here is
// Blitz's (SKIFF_WEB_DOM=blitz): each change the mirror makes, one mutation
// of it -- nothing written out and read again. Blitz brings its styles and
// layout up to date with what changed, paints the page into pixels, and the
// window shows them (SDL's renderer, a texture: nothing of Skia). A press is
// Blitz's -- a field focused, a caret put, text selected -- and the
// program's too, at the point skiff has for where it is in the element
// Blitz has there: given to skiff's input as the window gives it. Keys and
// text go to Blitz's fields, Enter, Escape and shortcuts to the program
// first. A wheel scrolls what is under it, as a browser's does. The hooks are
// the window's (window::hooks): the dialogs, files dropped, the clipboard,
// links followed, as the window does them. What each part of a frame took is
// said as it goes.
module;
#include <SDL3/SDL.h>
export module skiff.platform.page;

import std;
import sdl;
import skia;
import skiff.scene;
import skiff.web;
import skiff.mirror;
import skiff.platform.events;
import skiff.platform.fonts;
import skiff.platform.clipboard;
import skiff.platform.window;
import :hooks;

export namespace skiff::platform::page {

namespace detail {
// A key that edits a field rather than types into it, as the engine takes
// it: none for the rest (what they type comes as text).
[[nodiscard]] inline std::optional<skiff::web::blitz::editing_key> editing_key_of(SDL_Keycode key, SDL_Keymod mod) {
  using enum skiff::web::blitz::editing_key;
  if (key >= SDLK_A && key <= SDLK_Z && (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0) return std::nullopt;
  switch (key) {
    case SDLK_BACKSPACE: return backspace;
    case SDLK_DELETE: return erase;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return enter;
    case SDLK_LEFT: return left;
    case SDLK_RIGHT: return right;
    case SDLK_UP: return up;
    case SDLK_DOWN: return down;
    case SDLK_HOME: return home;
    case SDLK_END: return end;
    case SDLK_TAB: return tab;
    case SDLK_A: return select_all;
    case SDLK_C: return copy;
    case SDLK_X: return cut;
    case SDLK_V: return paste;
    case SDLK_Z: return undo;
    case SDLK_Y: return redo;
    default: return std::nullopt;
  }
}
// Keys the program has first, before the engine's field: Enter (sending,
// or a new line, as the program says), Escape, and shortcuts with Control
// or the system's key -- but those the engine's field does itself: all of
// it selected, copied, cut, pasted, undone, done again.
[[nodiscard]] inline bool for_the_program(SDL_Keycode key, SDL_Keymod mod) {
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE) return true;
  if ((mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0) return false;
  switch (key) {
    case SDLK_A:
    case SDLK_C:
    case SDLK_X:
    case SDLK_V:
    case SDLK_Z:
    case SDLK_Y:
    case SDLK_LEFT:
    case SDLK_RIGHT:
    case SDLK_BACKSPACE:
    case SDLK_DELETE: return false;
    default: return true;
  }
}
} // namespace detail

// The SDL cursor for each shape the engine asks for (skiff.web:blitz's), made
// as first asked.
class cursors {
public:
  cursors() = default;
  cursors(const cursors &) = delete;
  cursors &operator=(const cursors &) = delete;
  ~cursors() {
    for (SDL_Cursor *one : made_)
      if (one) SDL_DestroyCursor(one);
  }
  void show(skiff::web::blitz::cursor shape) {
    if (shape == shown_) return;
    shown_ = shape;
    const auto system = [&] {
      using enum skiff::web::blitz::cursor;
      switch (shape) {
        case hand: return SDL_SYSTEM_CURSOR_POINTER;
        case text: return SDL_SYSTEM_CURSOR_TEXT;
        case resize_across: return SDL_SYSTEM_CURSOR_EW_RESIZE;
        case resize_down: return SDL_SYSTEM_CURSOR_NS_RESIZE;
        case not_allowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
        case wait: return SDL_SYSTEM_CURSOR_PROGRESS;
        default: return SDL_SYSTEM_CURSOR_DEFAULT;
      }
    }();
    SDL_Cursor *&made = made_[static_cast<std::size_t>(system)];
    if (made == nullptr) made = SDL_CreateSystemCursor(system);
    if (made) SDL_SetCursor(made);
  }

private:
  std::array<SDL_Cursor *, SDL_SYSTEM_CURSOR_COUNT> made_{};
  skiff::web::blitz::cursor shown_ = skiff::web::blitz::cursor::arrow;
};

// The program, until its window is closed: as window::run -- the same App,
// the same hooks (window::hooks) -- laid out and painted by Blitz.
template <class App> int run(App &app, const window::options &how, const events::kinds &kinds) {
  namespace hooks = window::hooks;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    std::println(std::cerr, "[page] no window: {}", SDL_GetError());
    return 1;
  }
  // skiff's own face, for what the program measures itself.
  skia::SkFont font;
  fonts::load_fonts<typename window::faces_of<App>::type>(how.fonts, font);
  using Faces = typename window::faces_of<App>::type;
  if constexpr (fonts::ships_text<Faces>) {
    skiff::web::blitz::fontData(std::ranges::data(Faces::regular), std::ranges::size(Faces::regular), skiff::web::blitz::font_face::regular);
    skiff::web::blitz::fontData(std::ranges::data(Faces::semibold), std::ranges::size(Faces::semibold), skiff::web::blitz::font_face::semibold);
  }
  if constexpr (fonts::ships_emoji<Faces>)
    skiff::web::blitz::fontData(std::ranges::data(Faces::emoji), std::ranges::size(Faces::emoji), skiff::web::blitz::font_face::emoji);
  SDL_Window *shown = SDL_CreateWindow(how.title.c_str(), how.width, how.height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Renderer *renderer = shown ? SDL_CreateRenderer(shown, nullptr) : nullptr;
  if (!shown || !renderer) {
    std::println(std::cerr, "[page] no window: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  hooks::dialogs_over(app, shown, kinds);
  {
    auto &scene = app.window();
    skiff::scene::hostWork().laidOutElsewhere = true;
    skiff::web::dom::reset();
    // The page's own rules, and skiff.web's: what is done under the pointer
    // and with the focus, a bubble's tail.
    skiff::web::blitz::addCss(std::string(detail::kPageCss) + skiff::web::pageCss());
    skiff::mirror::Mirror mirror;
    // Text typed comes as text (SDL_EVENT_TEXT_INPUT), an input method's too.
    SDL_StartTextInput(shown);
    // Pictures pasted, each in a file of its own.
    clipboard::paster paste;
    bool clipboard_stale = true;
    const auto fresh_clipboard = [&] {
      if (!std::exchange(clipboard_stale, false)) return;
      char *text = SDL_GetClipboardText();
      skiff::scene::clipboardContents() = text ? text : "";
      SDL_free(text);
    };
    // The interface's scale (the program's, in percent): the page zoomed by
    // it, and skiff laid out in its points -- the pointer's points divided
    // by it on the way in.
    float zoom = 1.0f;
    // The program's own input, as the window gives it -- presses, drags,
    // right presses, the wheel where the engine scrolled nothing -- at the
    // point skiff has for where the pointer is in the element the engine
    // has there; while a press is held, at the same distance from it.
    skiff::scene::InputRouter router;
    const std::array layers{skiff::scene::InputRouter::Layer{scene.handle(), false}};
    router.setLayers(layers);
    std::optional<skia::SkPoint> held;  // scene's point less the page's, from the press
    const auto inScene = [&](float x, float y) -> std::optional<skia::SkPoint> {
      if (held) return skia::SkPoint::Make(x + held->fX, y + held->fY);
      const skiff::web::dom::Handle element = mirror.ownerOf(skiff::web::blitz::at(x, y));
      const auto box = element == skiff::web::dom::kNone ? std::nullopt : skiff::web::blitz::boxOf(element);
      return box ? mirror.toScene(element, x - box->x, y - box->y) : std::nullopt;
    };
    cursors pointers;
    // The pixels Blitz paints, and the texture they are shown through, at
    // the window's size in pixels.
    std::vector<std::uint8_t> pixels;
    SDL_Texture *texture = nullptr;
    int texture_w = 0, texture_h = 0;
    bool running = true, repaint = true, stirred = true, on_screen = true;
    double mirrored_at = 0.0;
    const auto read_edits = [&] {
      for (const auto &[element, value] : skiff::web::blitz::takeEdits())
        if (mirror.typed(scene, element, value)) {
          hooks::after_event(app);
          stirred = true;
        }
    };
    constexpr double kMirrorAtLeastMs = 250.0;
    std::uint64_t frames = 0, dumped = 0;
    while (running) {
      SDL_Event event;
      bool got = SDL_WaitEventTimeout(&event, stirred || repaint ? 16 : static_cast<std::int32_t>(kMirrorAtLeastMs));
      stirred = stirred || got;
      zoom = static_cast<float>(std::max(1, hooks::interface_scale(app))) / 100.0f;
      while (got) {
        // In the page's CSS pixels: the window's points, unzoomed.
        const auto css = [&](float v) { return v / zoom; };
        switch (event.type) {
          case SDL_EVENT_QUIT:
          case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running = false;
            break;
          case SDL_EVENT_CLIPBOARD_UPDATE:
            if (!event.clipboard.owner) {
              skiff::scene::clipboardCandidate().reset();
              skiff::scene::clipboardRichText().reset();
            }
            clipboard_stale = true;
            break;
          case SDL_EVENT_WINDOW_FOCUS_GAINED:
            clipboard_stale = true;
            hooks::focus_changed(app, true);
            break;
          case SDL_EVENT_WINDOW_FOCUS_LOST:
            hooks::focus_changed(app, false);
            break;
          // Off screen: no frames, nothing but the connections.
          case SDL_EVENT_WINDOW_HIDDEN:
          case SDL_EVENT_WINDOW_MINIMIZED:
          case SDL_EVENT_WINDOW_OCCLUDED:
            on_screen = false;
            hooks::shown_changed(app, false);
            break;
          case SDL_EVENT_WINDOW_SHOWN:
          case SDL_EVENT_WINDOW_RESTORED:
          case SDL_EVENT_WINDOW_EXPOSED:
            on_screen = true;
            hooks::shown_changed(app, true);
            repaint = true;
            break;
          case SDL_EVENT_WINDOW_RESIZED:
          case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
          case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            repaint = true;
            break;
          case SDL_EVENT_DROP_FILE:
            if (event.drop.data) hooks::files_given(app, std::vector<std::string>{event.drop.data}, true);
            break;
          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // The engine's own: a field focused, its caret put. And the
            // program's: a press, a right press -- its menu -- a drag begun.
            skiff::web::blitz::point(skiff::web::blitz::pointer::pressed, css(event.button.x), css(event.button.y), event.button.button - 1);
            if (const auto at = inScene(css(event.button.x), css(event.button.y))) {
              held = skia::SkPoint::Make(at->fX - css(event.button.x), at->fY - css(event.button.y));
              router.pointer(skiff::scene::pointer::down{at->fX, at->fY, event.button.button});
            }
            repaint = true;
            break;
          case SDL_EVENT_MOUSE_MOTION:
            skiff::web::blitz::point(skiff::web::blitz::pointer::moved, css(event.motion.x), css(event.motion.y), 0);
            if (const auto at = inScene(css(event.motion.x), css(event.motion.y))) router.pointer(skiff::scene::pointer::move{at->fX, at->fY});
            repaint = repaint || event.motion.state != 0;
            break;
          case SDL_EVENT_MOUSE_BUTTON_UP:
            skiff::web::blitz::point(skiff::web::blitz::pointer::released, css(event.button.x), css(event.button.y), event.button.button - 1);
            if (const auto at = inScene(css(event.button.x), css(event.button.y)))
              router.pointer(skiff::scene::pointer::up{at->fX, at->fY, event.button.button});
            held.reset();
            repaint = true;
            break;
          case SDL_EVENT_MOUSE_WHEEL: {
            const float x = css(event.wheel.mouse_x), y = css(event.wheel.mouse_y);
            // The engine's scrolling; where it scrolled nothing, the
            // program's wheel (a picture zoomed).
            if (!skiff::web::blitz::scroll(x, y, event.wheel.x * 48.0, event.wheel.y * 48.0))
              if (const auto at = inScene(x, y)) router.pointer(skiff::scene::pointer::scroll{at->fX, at->fY, event.wheel.x, event.wheel.y});
            repaint = true;
            break;
          }
          case SDL_EVENT_TEXT_INPUT:
            // Typed: into the engine's field with the focus.
            skiff::web::blitz::commitText(event.text.text);
            repaint = true;
            break;
          case SDL_EVENT_KEY_DOWN: {
            // Text and Enter can arrive in the same SDL batch. The draft
            // must reach the program before Enter asks it to send.
            read_edits();
            const SDL_Keymod mod = event.key.mod;
            // Ctrl+V with a picture on the clipboard: the picture, as if
            // dropped on the window -- not the text a field would paste.
            if ((event.key.key == SDLK_V || event.key.scancode == SDL_SCANCODE_V) && (mod & SDL_KMOD_CTRL) != 0 && !event.key.repeat)
              if (std::vector<std::string> pictures = paste.pictures(); !pictures.empty()) {
                hooks::files_given(app, std::move(pictures), true);
                break;
              }
            // Ctrl+D: the page as Blitz holds it, as HTML -- what the mirror wrote.
            if (event.key.key == SDLK_D && (mod & SDL_KMOD_CTRL) != 0 && (mod & SDL_KMOD_SHIFT) != 0) {
              const std::filesystem::path to = std::format("page-{}.html", ++dumped);
              const std::string html = "<html><head><style>" + std::string(detail::kPageCss) + skiff::web::pageCss() + "</style></head>" +
                                       skiff::web::dom::dump(skiff::web::dom::root()) + "</html>";
              std::ofstream(to, std::ios::binary) << html;
              std::println(std::cerr, "[page] {} written: {} bytes", std::filesystem::absolute(to).string(), html.size());
              break;
            }
            // The program's first (Enter, Escape, a shortcut); else, a key
            // that edits, the engine's field's.
            if (detail::for_the_program(event.key.key, mod) &&
                mirror.keyed(scene, skiff::web::blitz::focused(),
                             skiff::scene::key::down{window::detail::key_of(event.key.key), window::detail::modifiers_of(mod), event.key.repeat}))
              repaint = true;
            else if (const auto editing = detail::editing_key_of(event.key.key, mod))
              skiff::web::blitz::key(*editing, {.shift = (mod & SDL_KMOD_SHIFT) != 0, .control = (mod & SDL_KMOD_CTRL) != 0,
                                                .alt = (mod & SDL_KMOD_ALT) != 0, .meta = (mod & SDL_KMOD_GUI) != 0});
            repaint = true;
            break;
          }
          default:
            if (event.type == kinds.wake)
              hooks::woken(app);
            else if (event.type == kinds.files)
              hooks::dialogs_answered(app);
            else if (event.type == kinds.save)
              hooks::dialogs_saved(app);
            break;
        }
        hooks::after_event(app);
        got = SDL_PollEvent(&event);
      }
      if (!running)
        break;
      // What the engine's fields were edited to: each given to its node as
      // typing's edits are, the program told as of typing.
      read_edits();
      // What the engine asked of the host: what it copied, put on the
      // clipboard; the clipboard's text, where it pastes; where an input
      // method's window goes; the pointer's shape.
      if (const auto copied = skiff::web::blitz::takeCopied()) SDL_SetClipboardText(copied->c_str());
      if (skiff::web::blitz::takePasteRequest()) {
        char *text = SDL_GetClipboardText();
        skiff::web::blitz::paste(text ? text : "");
        SDL_free(text);
      }
      if (const auto area = skiff::web::blitz::takeImeArea()) {
        const SDL_Rect told{static_cast<int>(area->x * zoom), static_cast<int>(area->y * zoom), static_cast<int>(area->width * zoom),
                            static_cast<int>(area->height * zoom)};
        SDL_SetTextInputArea(shown, &told, 0);
      }
      pointers.show(skiff::web::blitz::cursorShape());
      // What the scene left for the host: what was copied put on the
      // clipboard, the links pressed followed.
      auto &work = skiff::scene::hostWork();
      if (auto copied = std::exchange(work.copied, std::nullopt)) {
        const auto &rich = skiff::scene::clipboardRichText();
        if (rich && rich->text == *copied && !rich->atoms.empty())
          clipboard::copy_text(*copied, clipboard::emoji_html(*rich));
        else
          SDL_SetClipboardText(copied->c_str());
        skiff::scene::clipboardContents() = std::move(*copied);
      }
      for (auto &url : std::exchange(work.links, {}))
        hooks::open_link(app, std::move(url));
      // Off screen: no frame -- not the program's either, which marks what
      // is in view as read. Woken only by events and the network.
      if (!on_screen)
        continue;
      const double began = detail::now_ms();
      if (!stirred && began - mirrored_at < kMirrorAtLeastMs && !repaint)
        continue;
      stirred = false;
      mirrored_at = began;
      // The engine's scrolling, into skiff's views that scroll: what the
      // program makes of where they are goes by it.
      mirror.readScrolled(scene.root(), [](skiff::web::dom::Handle element) { return skiff::web::blitz::scrolled(element); });
      hooks::before_frame(app);
      // Keys the program's own controls stand for -- a text menu's Paste is
      // Ctrl+V -- given to what has the focus, down and up, before the frame.
      for (const skiff::scene::key::down &press : std::exchange(skiff::scene::hostWork().keys, {})) {
        const bool pastes = press.key == skiff::scene::keys::kV && press.modifiers.template has<skiff::scene::modifier::control>();
        if (pastes)
          if (std::vector<std::string> pictures = paste.pictures(); !pictures.empty()) {
            hooks::files_given(app, std::move(pictures), true);
            continue;
          }
        if (pastes) fresh_clipboard();
        router.key(press);
        router.key(skiff::scene::key::up{press.key, press.modifiers});
      }
      scene.update(began);
      if (!skiff::scene::hostWork().pressed.empty())
        hooks::after_event(app);
      int w = 0, h = 0, pw = 0, ph = 0;
      SDL_GetWindowSize(shown, &w, &h);
      SDL_GetWindowSizeInPixels(shown, &pw, &ph);
      if (pw <= 0 || ph <= 0)
        continue;
      // skiff's own layout too, in its points: a list makes the rows its
      // view holds.
      const double updated = detail::now_ms();
      skiff::scene::pixelScale() = static_cast<float>(pw) / static_cast<float>(w) * zoom;
      scene.layoutIfNeeded(skia::SkRect::MakeWH(static_cast<float>(w) / zoom, static_cast<float>(h) / zoom));
      // A right press that closed a menu, given again to what is under it
      // now the menu is gone: another menu opened where it was pressed.
      if (const auto again = hooks::take_press_again(app)) {
        router.pointer(skiff::scene::pointer::down{again->fX, again->fY, SDL_BUTTON_RIGHT});
        router.pointer(skiff::scene::pointer::up{again->fX, again->fY, SDL_BUTTON_RIGHT});
        hooks::after_event(app);
      }
      const double laid_skiff = detail::now_ms();
      mirror.sync(scene.root());
      for (const std::string &one : mirror.takeNotNative())
        std::println(std::cerr, "[mirror] not native: {}", one);
      const double mirrored = detail::now_ms();
      skiff::web::blitz::viewport(static_cast<std::uint32_t>(pw), static_cast<std::uint32_t>(ph), static_cast<float>(pw) / static_cast<float>(w), zoom);
      skiff::web::blitz::resolve(began);
      const double resolved = detail::now_ms();
      if (pw != texture_w || ph != texture_h) {
        if (texture) SDL_DestroyTexture(texture);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, pw, ph);
        texture_w = pw;
        texture_h = ph;
        pixels.assign(static_cast<std::size_t>(pw) * static_cast<std::size_t>(ph) * 4u, 0);
      }
      skiff::web::blitz::paint(pixels.data(), static_cast<std::size_t>(pw) * 4u);
      const double painted = detail::now_ms();
      SDL_UpdateTexture(texture, nullptr, pixels.data(), pw * 4);
      SDL_RenderTexture(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
      repaint = false;
      ++frames;
      // Said where a frame was slow -- longer than one at 60 a second -- and
      // of the first: said of every frame, it buried everything else.
      if (frames == 1 || window::detail::now_ms() - began > 16.0)
        std::println(std::cerr,
                   "[page] #{}: skiff laid out in {:.2f} ms; {} nodes mirrored in {:.2f} ms; Blitz resolved in {:.2f} ms, painted in {:.2f} ms; "
                   "shown in {:.2f} ms",
                   frames, laid_skiff - updated, mirror.size(), mirrored - laid_skiff, resolved - mirrored, painted - resolved,
                   detail::now_ms() - painted);
    }
    if (texture) SDL_DestroyTexture(texture);
    hooks::closing(app);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(shown);
  SDL_Quit();
  return 0;
}

} // namespace skiff::platform::page
