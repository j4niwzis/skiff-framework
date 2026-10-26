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
// Blitz's hit test: the element under it, clicked along its node's path in
// the scene, and the program's after_event then, as the window's click is.
// A wheel scrolls what is under it, as a browser's does. What each part of a
// frame took is said as it goes.
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
import skiff.platform.window;
import :hooks;

export namespace skiff::platform::page {

namespace detail {
// A key that edits a field rather than types into it, as the engine takes
// it: none for the rest (what they type comes as text).
[[nodiscard]] inline std::optional<skiff::web::blitz::editing_key> editing_key_of(SDL_Keycode key) {
  using enum skiff::web::blitz::editing_key;
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

// The program, until its window is closed: as window::run, laid out and
// painted by Blitz.
template <class App> int run(App &app, const window::options &how, const events::kinds &kinds) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    std::println(std::cerr, "[page] no window: {}", SDL_GetError());
    return 1;
  }
  // skiff's own face, for what the program measures itself.
  skia::SkFont font;
  fonts::load_fonts<typename detail::faces_of<App>::type>(how.fonts, font);
  SDL_Window *shown = SDL_CreateWindow(how.title.c_str(), how.width, how.height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Renderer *renderer = shown ? SDL_CreateRenderer(shown, nullptr) : nullptr;
  if (!shown || !renderer) {
    std::println(std::cerr, "[page] no window: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
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
    // The pixels Blitz paints, and the texture they are shown through, at
    // the window's size in pixels.
    std::vector<std::uint8_t> pixels;
    SDL_Texture *texture = nullptr;
    int texture_w = 0, texture_h = 0;
    bool running = true, repaint = true, stirred = true;
    double mirrored_at = 0.0;
    constexpr double kMirrorAtLeastMs = 250.0;
    std::uint64_t frames = 0, dumped = 0;
    while (running) {
      SDL_Event event;
      bool got = SDL_WaitEventTimeout(&event, stirred || repaint ? 16 : static_cast<std::int32_t>(kMirrorAtLeastMs));
      stirred = stirred || got;
      while (got) {
        if (event.type == SDL_EVENT_QUIT) {
          running = false;
        } else if (event.type == kinds.wake) {
          detail::woken(app);
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          // The engine's own: a field focused, its caret put.
          skiff::web::blitz::point(skiff::web::blitz::pointer::pressed, event.button.x, event.button.y, event.button.button - 1);
          repaint = true;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
          skiff::web::blitz::point(skiff::web::blitz::pointer::moved, event.motion.x, event.motion.y, 0);
          repaint = repaint || event.motion.state != 0;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
          skiff::web::blitz::point(skiff::web::blitz::pointer::released, event.button.x, event.button.y, event.button.button - 1);
          if (const auto at = skiff::web::blitz::at(event.button.x, event.button.y); at != skiff::web::dom::kNone && mirror.click(scene, at))
            detail::after_event(app);
          repaint = true;
        } else if (event.type == SDL_EVENT_TEXT_INPUT) {
          // Typed: into the engine's field with the focus.
          skiff::web::blitz::commitText(event.text.text);
          repaint = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN && detail::for_the_program(event.key.key, event.key.mod) &&
                   mirror.keyed(scene, skiff::web::blitz::focused(),
                                skiff::scene::key::down{window::detail::key_of(event.key.key), window::detail::modifiers_of(event.key.mod),
                                                        event.key.repeat})) {
          // Taken by the program: Enter sending, Escape closing, a shortcut.
          detail::after_event(app);
          stirred = true;
          repaint = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN && detail::editing_key_of(event.key.key)) {
          const SDL_Keymod mod = event.key.mod;
          skiff::web::blitz::key(*detail::editing_key_of(event.key.key),
                                 {.shift = (mod & SDL_KMOD_SHIFT) != 0, .control = (mod & SDL_KMOD_CTRL) != 0, .alt = (mod & SDL_KMOD_ALT) != 0,
                                  .meta = (mod & SDL_KMOD_GUI) != 0});
          repaint = true;
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
          float x = 0.0f, y = 0.0f;
          (void)SDL_GetMouseState(&x, &y);
          repaint = skiff::web::blitz::scroll(x, y, -event.wheel.x * 48.0, -event.wheel.y * 48.0) || repaint;
        } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_D && (event.key.mod & SDL_KMOD_CTRL) != 0) {
          // Ctrl+D: the page as Blitz holds it, as HTML -- what the mirror wrote.
          const std::filesystem::path to = std::format("page-{}.html", ++dumped);
          const std::string html = "<html><head><style>" + std::string(detail::kPageCss) + skiff::web::pageCss() + "</style></head>" +
                                   skiff::web::dom::dump(skiff::web::dom::root()) + "</html>";
          std::ofstream(to, std::ios::binary) << html;
          std::println(std::cerr, "[page] {} written: {} bytes", std::filesystem::absolute(to).string(), html.size());
        } else if (event.type == SDL_EVENT_WINDOW_EXPOSED || event.type == SDL_EVENT_WINDOW_RESIZED ||
                   event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
          repaint = true;
        }
        got = SDL_PollEvent(&event);
      }
      if (!running)
        break;
      // What the engine's fields were edited to: each given to its node as
      // typing's edits are, the program told as of typing.
      for (const auto &[element, value] : skiff::web::blitz::takeEdits())
        if (mirror.typed(scene, element, value)) {
          detail::after_event(app);
          stirred = true;
        }
      const double began = detail::now_ms();
      if (!stirred && began - mirrored_at < kMirrorAtLeastMs && !repaint)
        continue;
      stirred = false;
      mirrored_at = began;
      // The engine's scrolling, into skiff's views that scroll: what the
      // program makes of where they are goes by it.
      mirror.readScrolled(scene.root(), [](skiff::web::dom::Handle element) { return skiff::web::blitz::scrolled(element); });
      detail::before_frame(app);
      scene.update(began);
      if (!skiff::scene::hostWork().pressed.empty())
        detail::after_event(app);
      int w = 0, h = 0, pw = 0, ph = 0;
      SDL_GetWindowSize(shown, &w, &h);
      SDL_GetWindowSizeInPixels(shown, &pw, &ph);
      if (pw <= 0 || ph <= 0)
        continue;
      // skiff's own layout too: a list makes the rows its view holds.
      const double updated = detail::now_ms();
      scene.layoutIfNeeded(skia::SkRect::MakeWH(static_cast<float>(w), static_cast<float>(h)));
      const double laid_skiff = detail::now_ms();
      mirror.sync(scene.root());
      for (const std::string &one : mirror.takeNotNative())
        std::println(std::cerr, "[mirror] not native: {}", one);
      const double mirrored = detail::now_ms();
      skiff::web::blitz::viewport(static_cast<std::uint32_t>(pw), static_cast<std::uint32_t>(ph), static_cast<float>(pw) / static_cast<float>(w));
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
      std::println(std::cerr,
                   "[page] #{}: skiff laid out in {:.2f} ms; {} nodes mirrored in {:.2f} ms; Blitz resolved in {:.2f} ms, painted in {:.2f} ms; "
                   "shown in {:.2f} ms",
                   frames, laid_skiff - updated, mirror.size(), mirrored - laid_skiff, resolved - mirrored, painted - resolved,
                   detail::now_ms() - painted);
    }
    if (texture) SDL_DestroyTexture(texture);
    detail::closing(app);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(shown);
  SDL_Quit();
  return 0;
}

} // namespace skiff::platform::page
