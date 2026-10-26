// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page -- A program's scene shown through an HTML engine
// embedded and called directly: litehtml, no JavaScript and no browser.
//
// The program is the window's (window::run's App): its scene, its hooks.
// What differs is who lays it out and paints it. Each frame the scene is
// mirrored into skiff.web's document (skiff.mirror: a flow a flex box, words
// their text, ...); where the document changed, litehtml is given it anew
// -- a litehtml document is finished once, and changed by making another --
// lays it out at the window's width, and paints it through Skia here (the
// container). A press is litehtml's: the element it was on, clicked along
// its node's path in the scene, and the program's after_event then, as the
// window's click is. What each part of a frame took is said as it goes:
// what this host is for is measuring the engine.
module;
#include <litehtml.h>
export module skiff.platform.page;

import std;
import sdl;
import skia;
import splice;
import skiff.paint;
import skiff.scene;
import skiff.web;
import skiff.mirror;
import skiff.platform.events;
import skiff.platform.fonts;
import skiff.platform.window;

export namespace skiff::platform::page {

namespace detail {

[[nodiscard]] inline double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
[[nodiscard]] inline skia::SkColor colour_of(const litehtml::web_color &c) { return skia::SkColorSetARGB(c.alpha, c.red, c.green, c.blue); }
[[nodiscard]] inline skia::SkRect rect_of(const litehtml::position &p) { return skia::SkRect::MakeXYWH(p.x, p.y, p.width, p.height); }
[[nodiscard]] inline skia::SkRRect rounded_of(const litehtml::position &p, const litehtml::border_radiuses &r) {
  const std::array<skia::SkVector, 4> radii{skia::SkVector{r.top_left_x, r.top_left_y}, skia::SkVector{r.top_right_x, r.top_right_y},
                                            skia::SkVector{r.bottom_right_x, r.bottom_right_y},
                                            skia::SkVector{r.bottom_left_x, r.bottom_left_y}};
  skia::SkRRect out;
  out.setRectRadii(rect_of(p), radii.data());
  return out;
}

// What litehtml asks of its host: fonts and their measures, painting on a
// Skia canvas, the viewport, and which element a press was on.
class container final : public litehtml::document_container {
public:
  skia::SkCanvas *canvas = nullptr;
  float width = 0.0f, height = 0.0f;
  // The element the last press was on, by the handle the mirror wrote on it.
  std::optional<skiff::web::dom::Handle> clicked;

  // A font: the program's own face (skiff.paint's), at the size and weight
  // asked. Kept by its place here, given to litehtml as that number + 1.
  litehtml::uint_ptr create_font(const litehtml::font_description &asked, const litehtml::document *, litehtml::font_metrics *fm) override {
    skia::SkFont font = skiff::paint::defaultFont() ? *skiff::paint::defaultFont() : skia::SkFont();
    font.setSize(asked.size);
    font.setEmbolden(asked.weight >= 600);
    skia::SkFontMetrics m;
    (void)font.getMetrics(&m);
    const float ascent = -m.fAscent, descent = m.fDescent;
    if (fm != nullptr) {
      fm->font_size = asked.size;
      fm->ascent = ascent;
      fm->descent = descent;
      fm->height = ascent + descent + m.fLeading;
      fm->x_height = m.fXHeight > 0.0f ? m.fXHeight : asked.size * 0.5f;
      fm->ch_width = font.measureText("0", 1, skia::SkTextEncoding::kUTF8);
      fm->draw_spaces = false;
    }
    fonts_.push_back({std::move(font), ascent});
    return fonts_.size();
  }
  void delete_font(litehtml::uint_ptr) override {}
  litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr font) override {
    return this->font(font).font.measureText(text, std::strlen(text), skia::SkTextEncoding::kUTF8);
  }
  void draw_text(litehtml::uint_ptr, const char *text, litehtml::uint_ptr font, litehtml::web_color colour, const litehtml::position &at) override {
    const auto &[face, ascent] = this->font(font);
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(colour_of(colour));
    canvas->drawSimpleText(text, std::strlen(text), skia::SkTextEncoding::kUTF8, at.x, at.y + ascent, face, paint);
  }
  litehtml::pixel_t pt_to_px(float pt) const override { return pt * 96.0f / 72.0f; }
  litehtml::pixel_t get_default_font_size() const override { return 14.0f; }
  const char *get_default_font_name() const override { return "sans-serif"; }
  void draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker &) override {}
  void load_image(const char *, const char *, bool) override {}
  void get_image_size(const char *, const char *, litehtml::size &size) override { size = {}; }
  void draw_image(litehtml::uint_ptr, const litehtml::background_layer &, const std::string &, const std::string &) override {}
  void draw_solid_fill(litehtml::uint_ptr, const litehtml::background_layer &layer, const litehtml::web_color &colour) override {
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(colour_of(colour));
    canvas->save();
    canvas->clipRect(rect_of(layer.clip_box));
    canvas->drawRRect(rounded_of(layer.border_box, layer.border_radius), paint);
    canvas->restore();
  }
  // Gradients: their first colour, as a plate -- what a wallpaper's needs.
  void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                            const litehtml::background_layer::linear_gradient &gradient) override {
    if (!gradient.color_points.empty()) this->draw_solid_fill(hdc, layer, gradient.color_points.front().color);
  }
  void draw_radial_gradient(litehtml::uint_ptr, const litehtml::background_layer &, const litehtml::background_layer::radial_gradient &) override {}
  void draw_conic_gradient(litehtml::uint_ptr, const litehtml::background_layer &, const litehtml::background_layer::conic_gradient &) override {}
  // Borders: each side a plate of its width; all four alike and rounded, a
  // rounded line.
  void draw_borders(litehtml::uint_ptr, const litehtml::borders &b, const litehtml::position &at, bool) override {
    const bool alike = b.left.width == b.top.width && b.top.width == b.right.width && b.right.width == b.bottom.width &&
                       b.left.color == b.top.color && b.top.color == b.right.color && b.right.color == b.bottom.color;
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    if (alike && b.top.width > 0.0f && b.top.style != litehtml::border_style_none) {
      paint.setColor(colour_of(b.top.color));
      paint.setStyle(skia::SkPaint::kStroke_Style);
      paint.setStrokeWidth(b.top.width);
      const float half = b.top.width * 0.5f;
      litehtml::position inner = at;
      inner.x += half;
      inner.y += half;
      inner.width -= b.top.width;
      inner.height -= b.top.width;
      canvas->drawRRect(rounded_of(inner, b.radius), paint);
      return;
    }
    const auto side = [&](const litehtml::border &one, skia::SkRect box) {
      if (one.width <= 0.0f || one.style == litehtml::border_style_none) return;
      paint.setColor(colour_of(one.color));
      canvas->drawRect(box, paint);
    };
    side(b.top, skia::SkRect::MakeXYWH(at.x, at.y, at.width, b.top.width));
    side(b.bottom, skia::SkRect::MakeXYWH(at.x, at.y + at.height - b.bottom.width, at.width, b.bottom.width));
    side(b.left, skia::SkRect::MakeXYWH(at.x, at.y, b.left.width, at.height));
    side(b.right, skia::SkRect::MakeXYWH(at.x + at.width - b.right.width, at.y, b.right.width, at.height));
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
  void set_clip(const litehtml::position &at, const litehtml::border_radiuses &radius) override {
    canvas->save();
    canvas->clipRRect(rounded_of(at, radius), true);
  }
  void del_clip() override { canvas->restore(); }
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

private:
  struct face {
    skia::SkFont font;
    float ascent = 0.0f;
  };
  [[nodiscard]] const face &font(litehtml::uint_ptr which) const { return fonts_.at(which - 1); }
  std::vector<face> fonts_;
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
  skia::SkFont font;
  fonts::load_fonts<typename detail::faces_of<App>::type>(how.fonts, font);
  sdl::SDL_Window *shown = sdl::SDL_CreateWindow(how.title.c_str(), how.width, how.height, sdl::kWindowResizable | sdl::kWindowHighPixelDensity);
  if (!shown) {
    std::println(std::cerr, "[page] no window: {}", sdl::SDL_GetError());
    sdl::SDL_Quit();
    return 1;
  }
  {
    // Drawn in software: what is measured is the engine, not a GPU.
    window::detail::canvas_target target(shown, true, how.programs);
    auto &scene = app.window();
    skiff::scene::hostWork().laidOutElsewhere = true;
    skiff::web::dom::reset();
    skiff::mirror::Mirror mirror;
    detail::container host;
    std::shared_ptr<litehtml::document> document;
    std::string written;
    float scrolled = 0.0f;
    bool running = true, repaint = true;
    // Measures: of the frame that last built a document, and how many did.
    std::uint64_t built = 0;
    while (running) {
      sdl::SDL_Event event;
      bool got = sdl::SDL_WaitEventTimeout(&event, 16);
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
        } else if (event.type == sdl::SDL_EVENT_WINDOW_RESIZED) {
          written.clear();  // laid out again at the new width
          repaint = true;
        }
        got = sdl::SDL_PollEvent(&event);
      }
      if (!running)
        break;
      const double began = detail::now_ms();
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
      repaint = false;
      skia::SkSurface *surface = target.surface();
      if (surface == nullptr)
        continue;
      const double painting = detail::now_ms();
      skia::SkCanvas *canvas = surface->getCanvas();
      host.canvas = canvas;
      canvas->save();
      canvas->clear(skia::SK_ColorWHITE);
      canvas->scale(sdl::SDL_GetWindowPixelDensity(shown), sdl::SDL_GetWindowPixelDensity(shown));
      const litehtml::position clip(0, 0, host.width, host.height);
      document->draw(0, 0, -scrolled, &clip);
      canvas->restore();
      target.present();
      std::println(std::cerr, "[page] painted in {:.2f} ms", detail::now_ms() - painting);
    }
    detail::closing(app);
  }
  sdl::SDL_DestroyWindow(shown);
  sdl::SDL_Quit();
  return 0;
}

} // namespace skiff::platform::page
