// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page:paint, with Skia: what litehtml paints, drawn in the
// window's software surface (the window's own canvas target), text in the
// program's face (skiff.paint's). The page's other painter (paint_sdl.cc,
// where skiff is built without Skia) says the same.
export module skiff.platform.page:paint;

import std;
import sdl;
import skia;
import skiff.paint;
import skiff.platform.window;

export namespace skiff::platform::page::detail {

using colour = std::uint32_t;  // 0xAARRGGBB
struct box {
  float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
};
// Each corner's radius, across and down.
struct radii {
  float top_left_x = 0.0f, top_left_y = 0.0f, top_right_x = 0.0f, top_right_y = 0.0f;
  float bottom_right_x = 0.0f, bottom_right_y = 0.0f, bottom_left_x = 0.0f, bottom_left_y = 0.0f;
};
struct face_metrics {
  float ascent = 0.0f, descent = 0.0f, height = 0.0f, x_height = 0.0f, zero_width = 0.0f;
};

class painter {
public:
  painter(sdl::SDL_Window *window, const std::string &, const std::string &programs) : target_(window, true, programs) {}

  // A face at a size and weight, by its number here; its measures.
  [[nodiscard]] std::size_t font(float size, bool bold, face_metrics &said) {
    skia::SkFont face = skiff::paint::defaultFont() ? *skiff::paint::defaultFont() : skia::SkFont();
    face.setSize(size);
    face.setEmbolden(bold);
    skia::SkFontMetrics m;
    (void)face.getMetrics(&m);
    said.ascent = -m.fAscent;
    said.descent = m.fDescent;
    said.height = said.ascent + said.descent + m.fLeading;
    said.x_height = m.fXHeight > 0.0f ? m.fXHeight : size * 0.5f;
    said.zero_width = face.measureText("0", 1, skia::SkTextEncoding::kUTF8);
    fonts_.push_back({std::move(face), said.ascent});
    return fonts_.size() - 1;
  }
  [[nodiscard]] float measure(std::string_view text, std::size_t font) const {
    return fonts_.at(font).face.measureText(text.data(), text.size(), skia::SkTextEncoding::kUTF8);
  }
  // A frame begun: false where there is nothing to draw into yet.
  bool begin(float scale) {
    skia::SkSurface *surface = target_.surface();
    if (surface == nullptr) return false;
    canvas_ = surface->getCanvas();
    canvas_->save();
    canvas_->scale(scale, scale);
    return true;
  }
  void clear(colour c) { canvas_->clear(c); }
  void text(std::string_view words, std::size_t font, colour c, float x, float top) {
    const auto &[face, ascent] = fonts_.at(font);
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(c);
    canvas_->drawSimpleText(words.data(), words.size(), skia::SkTextEncoding::kUTF8, x, top + ascent, face, paint);
  }
  void fill(const box &at, const radii &r, colour c) {
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(c);
    canvas_->drawRRect(rounded(at, r), paint);
  }
  void stroke(const box &at, const radii &r, float width, colour c) {
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(c);
    paint.setStyle(skia::SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    canvas_->drawRRect(rounded(at, r), paint);
  }
  void push_clip(const box &at, const radii &r) {
    canvas_->save();
    canvas_->clipRRect(rounded(at, r), true);
  }
  void pop_clip() { canvas_->restore(); }
  void present() {
    canvas_->restore();
    target_.present();
  }

private:
  [[nodiscard]] static skia::SkRRect rounded(const box &at, const radii &r) {
    const std::array<skia::SkPoint, 4> corners{skia::SkPoint{r.top_left_x, r.top_left_y}, skia::SkPoint{r.top_right_x, r.top_right_y},
                                                skia::SkPoint{r.bottom_right_x, r.bottom_right_y},
                                                skia::SkPoint{r.bottom_left_x, r.bottom_left_y}};
    skia::SkRRect out;
    out.setRectRadii(skia::SkRect::MakeXYWH(at.x, at.y, at.width, at.height), corners.data());
    return out;
  }
  struct kept {
    skia::SkFont face;
    float ascent = 0.0f;
  };
  window::detail::canvas_target target_;
  skia::SkCanvas *canvas_ = nullptr;
  std::vector<kept> fonts_;
};

} // namespace skiff::platform::page::detail
