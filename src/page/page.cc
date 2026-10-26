// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page -- A program's scene shown through an HTML engine
// embedded and called directly: litehtml, no JavaScript and no browser.
//
// The program is the window's (window::run's App): its scene, its hooks.
// What differs is who lays it out and paints it. Each frame the scene is
// mirrored into skiff.web's document (skiff.mirror: a flow a flex box, words
// their text, ...); where the document changed, litehtml is given it anew
// -- a litehtml document is finished once, and changed by making another --
// lays it out at the window's width, and paints it (the container, through
// the painter: Skia's, or -- skiff built without Skia -- SDL's renderer and
// FreeType's glyphs). A press is litehtml's: the element it was on, clicked along
// its node's path in the scene, and the program's after_event then, as the
// window's click is. What each part of a frame took is said as it goes:
// what this host is for is measuring the engine.
module;
#include <litehtml.h>
export module skiff.platform.page;

import std;
import sdl;
import splice;
import skiff.scene;
import skiff.web;
import skiff.mirror;
import skiff.platform.events;
import skiff.platform.fonts;
import skiff.platform.window;
import :paint;

export namespace skiff::platform::page {

namespace detail {

[[nodiscard]] inline double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
[[nodiscard]] inline colour colour_of(const litehtml::web_color &c) {
  return (static_cast<colour>(c.alpha) << 24) | (static_cast<colour>(c.red) << 16) | (static_cast<colour>(c.green) << 8) | c.blue;
}
[[nodiscard]] inline box box_of(const litehtml::position &p) { return {p.x, p.y, p.width, p.height}; }
[[nodiscard]] inline radii radii_of(const litehtml::border_radiuses &r) {
  return {r.top_left_x, r.top_left_y, r.top_right_x, r.top_right_y, r.bottom_right_x, r.bottom_right_y, r.bottom_left_x, r.bottom_left_y};
}

// What litehtml asks of its host: fonts and their measures, painting (the
// painter: Skia's, or SDL's and FreeType's), the viewport, and which
// element a press was on.
class container final : public litehtml::document_container {
public:
  explicit container(painter &with) : paint(with) {}
  painter &paint;
  float width = 0.0f, height = 0.0f;
  // The element the last press was on, by the handle the mirror wrote on it.
  std::optional<skiff::web::dom::Handle> clicked;

  // A face, by the painter's number for it + 1 (0 is none to litehtml).
  litehtml::uint_ptr create_font(const litehtml::font_description &asked, const litehtml::document *, litehtml::font_metrics *fm) override {
    face_metrics said;
    const std::size_t made = paint.font(asked.size, asked.weight >= 600, said);
    if (fm != nullptr) {
      fm->font_size = asked.size;
      fm->ascent = said.ascent;
      fm->descent = said.descent;
      fm->height = said.height;
      fm->x_height = said.x_height;
      fm->ch_width = said.zero_width;
      fm->draw_spaces = false;
    }
    return made + 1;
  }
  void delete_font(litehtml::uint_ptr) override {}
  litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr font) override { return paint.measure(text, font - 1); }
  void draw_text(litehtml::uint_ptr, const char *text, litehtml::uint_ptr font, litehtml::web_color c, const litehtml::position &at) override {
    paint.text(text, font - 1, colour_of(c), at.x, at.y);
  }
  litehtml::pixel_t pt_to_px(float pt) const override { return pt * 96.0f / 72.0f; }
  litehtml::pixel_t get_default_font_size() const override { return 14.0f; }
  const char *get_default_font_name() const override { return "sans-serif"; }
  void draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker &) override {}
  void load_image(const char *, const char *, bool) override {}
  void get_image_size(const char *, const char *, litehtml::size &size) override { size = {}; }
  void draw_image(litehtml::uint_ptr, const litehtml::background_layer &, const std::string &, const std::string &) override {}
  void draw_solid_fill(litehtml::uint_ptr, const litehtml::background_layer &layer, const litehtml::web_color &c) override {
    paint.push_clip(box_of(layer.clip_box), {});
    paint.fill(box_of(layer.border_box), radii_of(layer.border_radius), colour_of(c));
    paint.pop_clip();
  }
  // Gradients: their first colour, as a plate -- what a wallpaper's needs.
  void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                            const litehtml::background_layer::linear_gradient &gradient) override {
    if (!gradient.color_points.empty()) this->draw_solid_fill(hdc, layer, gradient.color_points.front().color);
  }
  void draw_radial_gradient(litehtml::uint_ptr, const litehtml::background_layer &, const litehtml::background_layer::radial_gradient &) override {}
  void draw_conic_gradient(litehtml::uint_ptr, const litehtml::background_layer &, const litehtml::background_layer::conic_gradient &) override {}
  // Borders: all four alike, a line round the box (rounded where it is);
  // else each side a plate of its width.
  void draw_borders(litehtml::uint_ptr, const litehtml::borders &b, const litehtml::position &at, bool) override {
    // In floats: litehtml's pixel converts to and from float both ways, and
    // compared or summed with one it is ambiguous.
    const float x = at.x, y = at.y, w = at.width, h = at.height;
    const float top = b.top.width, bottom = b.bottom.width, left = b.left.width, right = b.right.width;
    const bool alike = left == top && top == right && right == bottom && b.left.color == b.top.color && b.top.color == b.right.color &&
                       b.right.color == b.bottom.color;
    if (alike && top > 0.0f && b.top.style != litehtml::border_style_none) {
      const float half = top * 0.5f;
      paint.stroke({x + half, y + half, w - top, h - top}, radii_of(b.radius), top, colour_of(b.top.color));
      return;
    }
    const auto side = [&](const litehtml::border &one, float width, box plate) {
      if (width > 0.0f && one.style != litehtml::border_style_none) paint.fill(plate, {}, colour_of(one.color));
    };
    side(b.top, top, {x, y, w, top});
    side(b.bottom, bottom, {x, y + h - bottom, w, bottom});
    side(b.left, left, {x, y, left, h});
    side(b.right, right, {x + w - right, y, right, h});
  }
  void set_caption(const char *) override {}
  void set_base_url(const char *) override {}
  void link(const std::shared_ptr<litehtml::document> &, const litehtml::element::ptr &) override {}
  void on_anchor_click(const char *, const litehtml::element::ptr &) override {}
  // The element pressed, or the nearest above it the mirror wrote.
  bool on_element_click(const litehtml::element::ptr &el) override {
    for (litehtml::element::ptr at = el; at; at = at->parent())
      if (const char *handle = at->get_attr("data-skiff-h")) {
        skiff::web::dom::Handle h = 0;
        const std::string_view said(handle);
        if (std::from_chars(said.data(), said.data() + said.size(), h).ec == std::errc{}) {
          clicked = h;
          return true;
        }
      }
    return false;
  }
  void on_mouse_event(const litehtml::element::ptr &, litehtml::mouse_event) override {}
  void set_cursor(const char *) override {}
  void transform_text(std::string &, litehtml::text_transform) override {}
  void import_css(std::string &, const std::string &, std::string &) override {}
  void set_clip(const litehtml::position &at, const litehtml::border_radiuses &radius) override { paint.push_clip(box_of(at), radii_of(radius)); }
  void del_clip() override { paint.pop_clip(); }
  void get_viewport(litehtml::position &viewport) const override { viewport = litehtml::position(0, 0, width, height); }
  litehtml::element::ptr create_element(const char *, const litehtml::string_map &, const std::shared_ptr<litehtml::document> &) override {
    return nullptr;
  }
  void get_media_features(litehtml::media_features &media) const override {
    media.type = litehtml::media_type_screen;
    media.width = width;
    media.height = height;
    media.device_width = width;
    media.device_height = height;
    media.color = 8;
    media.monochrome = 0;
    media.color_index = 256;
    media.resolution = 96;
  }
  void get_language(std::string &language, std::string &culture) const override {
    language = "en";
    culture.clear();
  }
};

// What the page asks of the program beyond its scene, each doing nothing
// where the program says nothing: as the window's hooks.
template <class App> void before_frame(App &) {}
template <class App>
  requires requires(App &app) { app.before_frame(); }
void before_frame(App &app) { app.before_frame(); }
template <class App> void after_event(App &) {}
template <class App>
  requires requires(App &app) { app.after_event(); }
void after_event(App &app) { app.after_event(); }
template <class App> void woken(App &) {}
template <class App>
  requires requires(App &app) { app.woken(); }
void woken(App &app) { app.woken(); }
template <class App> void closing(App &) {}
template <class App>
  requires requires(App &app) { app.closing(); }
void closing(App &app) { app.closing(); }
template <class App> struct faces_of {
  using type = fonts::no_faces;
};
template <class App>
  requires requires { typename App::faces; }
struct faces_of<App> {
  using type = typename App::faces;
};

// The page's own rules, before what the mirror writes: no margins, the
// program's face, and every box sized with its border.
inline constexpr std::string_view kPageCss = "html,body{margin:0;padding:0;height:100%}body{font-family:sans-serif}";

} // namespace detail

// The program, until its window is closed: as window::run, laid out and
// painted by litehtml.
template <class App> int run(App &app, const window::options &how, const events::kinds &kinds) {
  if (!sdl::SDL_Init(sdl::kInitVideo | sdl::kInitEvents)) {
    std::println(std::cerr, "[page] no window: {}", sdl::SDL_GetError());
    return 1;
  }
  // skiff's own face, for what the program measures itself (skia's
  // stand-in's, where skiff is built without Skia).
  skia::SkFont font;
  fonts::load_fonts<typename detail::faces_of<App>::type>(how.fonts, font);
  sdl::SDL_Window *shown = sdl::SDL_CreateWindow(how.title.c_str(), how.width, how.height, sdl::kWindowResizable | sdl::kWindowHighPixelDensity);
  if (!shown) {
    std::println(std::cerr, "[page] no window: {}", sdl::SDL_GetError());
    sdl::SDL_Quit();
    return 1;
  }
  {
    // Drawn in software where it is Skia: what is measured is the engine.
    detail::painter paint(shown, how.fonts, how.programs);
    auto &scene = app.window();
    skiff::scene::hostWork().laidOutElsewhere = true;
    skiff::web::dom::reset();
    skiff::mirror::Mirror mirror;
    detail::container host(paint);
    std::shared_ptr<litehtml::document> document;
    std::string written;
    float scrolled = 0.0f;
    bool running = true, repaint = true;
    // Mirrored again only where something may have changed it: an event, a
    // wake from the program's other threads, or -- for what changes on its
    // own (a timer, an animation's end) -- a while since. Mirrored at every
    // tick, the whole tree was walked and written out sixty times a second
    // with nothing changed.
    bool stirred = true;
    double mirrored_at = 0.0;
    constexpr double kMirrorAtLeastMs = 250.0;
    // Measures: of the frame that last built a document, and how many did.
    std::uint64_t built = 0;
    while (running) {
      sdl::SDL_Event event;
      bool got = sdl::SDL_WaitEventTimeout(&event, stirred || repaint ? 16 : static_cast<std::int32_t>(kMirrorAtLeastMs));
      stirred = stirred || got;
      while (got) {
        if (event.type == sdl::SDL_EVENT_QUIT) {
          running = false;
        } else if (event.type == kinds.wake) {
          detail::woken(app);
        } else if (event.type == sdl::SDL_EVENT_MOUSE_BUTTON_DOWN && document) {
          (void)document->on_lbutton_down(event.button.x, event.button.y + scrolled, event.button.x, event.button.y, [](const litehtml::position &) {});
        } else if (event.type == sdl::SDL_EVENT_MOUSE_BUTTON_UP && document) {
          host.clicked.reset();
          (void)document->on_lbutton_up(event.button.x, event.button.y + scrolled, event.button.x, event.button.y, [](const litehtml::position &) {});
          if (host.clicked && mirror.click(scene, *host.clicked)) {
            detail::after_event(app);
            repaint = true;
          }
        } else if (event.type == sdl::SDL_EVENT_MOUSE_WHEEL) {
          scrolled = std::max(0.0f, scrolled - event.wheel.y * 48.0f);
          repaint = true;
        } else if (event.type == sdl::SDL_EVENT_WINDOW_EXPOSED || event.type == sdl::SDL_EVENT_WINDOW_SHOWN ||
                   event.type == sdl::SDL_EVENT_WINDOW_RESTORED) {
          // Shown again, or what was over it gone: painted again, as it was.
          repaint = true;
        } else if (event.type == sdl::SDL_EVENT_WINDOW_RESIZED) {
          written.clear();  // laid out again at the new width
          repaint = true;
        }
        got = sdl::SDL_PollEvent(&event);
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
      const double ticked = detail::now_ms();
      mirror.sync(scene.root());
      const std::string html = "<html><head><style>" + std::string(detail::kPageCss) + "</style></head>" +
                               skiff::web::dom::dump(skiff::web::dom::root()) + "</html>";
      const double mirrored = detail::now_ms();
      int w = 0, h = 0;
      sdl::SDL_GetWindowSize(shown, &w, &h);
      host.width = static_cast<float>(w);
      host.height = static_cast<float>(h);
      if (html != written) {
        written = html;
        document = litehtml::document::createFromString(html, &host);
        const double made = detail::now_ms();
        (void)document->render(host.width);
        const double laid = detail::now_ms();
        ++built;
        std::println(std::cerr, "[page] #{}: {} nodes mirrored in {:.2f} ms, {} bytes of HTML; litehtml made it in {:.2f} ms, laid it out in {:.2f} ms",
                     built, mirror.size(), mirrored - ticked, html.size(), made - mirrored, laid - made);
        repaint = true;
      }
      if (!repaint || !document)
        continue;
      const double painting = detail::now_ms();
      // Painted once there is something to paint into: until then, asked
      // again at the next frame -- not let go, as the first frame's paint
      // was where the window's surface was not there yet.
      if (!paint.begin(sdl::SDL_GetWindowPixelDensity(shown)))
        continue;
      repaint = false;
      paint.clear(0xFFFFFFFFu);
      const litehtml::position clip(0, 0, host.width, host.height);
      document->draw(0, 0, -scrolled, &clip);
      paint.present();
      std::println(std::cerr, "[page] painted in {:.2f} ms", detail::now_ms() - painting);
    }
    detail::closing(app);
  }
  sdl::SDL_DestroyWindow(shown);
  sdl::SDL_Quit();
  return 0;
}

} // namespace skiff::platform::page
