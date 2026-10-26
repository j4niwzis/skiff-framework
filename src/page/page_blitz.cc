// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page, on Blitz -- the program's scene laid out and painted by
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
    skiff::web::blitz::addCss(detail::kPageCss);
    skiff::mirror::Mirror mirror;
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
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
          if (const auto at = skiff::web::blitz::at(event.button.x, event.button.y); at != skiff::web::dom::kNone && mirror.click(scene, at))
            detail::after_event(app);
          repaint = true;
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
          float x = 0.0f, y = 0.0f;
          (void)SDL_GetMouseState(&x, &y);
          repaint = skiff::web::blitz::scroll(x, y, -event.wheel.x * 48.0, -event.wheel.y * 48.0) || repaint;
        } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_D && (event.key.mod & SDL_KMOD_CTRL) != 0) {
          // Ctrl+D: the page as Blitz holds it, as HTML -- what the mirror wrote.
          const std::filesystem::path to = std::format("page-{}.html", ++dumped);
          const std::string html = "<html><head><style>" + std::string(detail::kPageCss) + "</style></head>" +
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
      const double began = detail::now_ms();
      if (!stirred && began - mirrored_at < kMirrorAtLeastMs && !repaint)
        continue;
      stirred = false;
      mirrored_at = began;
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
