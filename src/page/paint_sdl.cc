// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page:paint, without Skia: what litehtml paints, drawn with
// SDL3's renderer -- plates, rounded a row at a time at their corners;
// borders as plates; clips as the renderer's (square) -- and text with
// FreeType, each glyph rendered once for a face and kept as a texture,
// tinted as it is drawn. No shaping: a character a glyph, left to right.
// Where skiff is built without Skia (SKIFF_SKIA off), this is the page's
// painter; paint_skia.cc says the same with Skia.
module;
#include <SDL3/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
export module skiff.platform.page:paint;

import std;
import sdl;

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

namespace paint_sdl {
// The characters of UTF-8 text, as code points: a malformed byte stands for
// itself, so that nothing stops the drawing.
[[nodiscard]] inline std::vector<char32_t> code_points(std::string_view text) {
  std::vector<char32_t> out;
  out.reserve(text.size());
  for (std::size_t at = 0; at < text.size();) {
    const auto lead = static_cast<unsigned char>(text[at]);
    const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 1;
    if (at + length > text.size() || length == 1) {
      out.push_back(lead);
      ++at;
      continue;
    }
    char32_t point = lead & (0xFF >> (length + 1));
    for (std::size_t i = 1; i < length; ++i) point = (point << 6) | (static_cast<unsigned char>(text[at + i]) & 0x3F);
    out.push_back(point);
    at += length;
  }
  return out;
}
// The face's file: one of the faces skiff reads first where the directory
// has it, else the first face there.
[[nodiscard]] inline std::optional<std::filesystem::path> face_file(const std::string &directory) {
  std::vector<std::filesystem::path> all;
  std::error_code failed;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied, failed))
    if (entry.is_regular_file(failed) && (entry.path().extension() == ".ttf" || entry.path().extension() == ".otf"))
      all.push_back(entry.path());
  for (const std::string_view wanted : {"Inter-Regular", "NotoSans-Regular", "DejaVuSans", "LiberationSans-Regular", "Cantarell-Regular"})
    if (const auto found = std::ranges::find_if(all, [&](const auto &path) { return path.stem().string() == wanted; }); found != all.end())
      return *found;
  if (all.empty()) return std::nullopt;
  return all.front();
}
} // namespace paint_sdl

class painter {
public:
  painter(sdl::SDL_Window *window, const std::string &fonts, const std::string &) : renderer_(SDL_CreateRenderer(window, nullptr)) {
    if (renderer_ == nullptr) std::println(std::cerr, "[page] no renderer: {}", SDL_GetError());
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    if (FT_Init_FreeType(&library_) != 0) library_ = nullptr;
    file_ = paint_sdl::face_file(fonts);
    if (!file_) std::println(std::cerr, "[page] no face in {}: text is not drawn", fonts);
  }
  painter(const painter &) = delete;
  painter &operator=(const painter &) = delete;
  ~painter() {
    for (auto &one : fonts_) {
      for (auto &[point, glyph] : one.glyphs)
        if (glyph.texture) SDL_DestroyTexture(glyph.texture);
      if (one.face) FT_Done_Face(one.face);
    }
    if (library_) FT_Done_FreeType(library_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
  }

  // A face at a size and weight, by its number here; its measures.
  [[nodiscard]] std::size_t font(float size, bool bold, face_metrics &said) {
    kept made{.bold = bold};
    if (library_ && file_ && FT_New_Face(library_, file_->string().c_str(), 0, &made.face) == 0) {
      FT_Set_Pixel_Sizes(made.face, 0, static_cast<FT_UInt>(std::lround(std::max(1.0f, size))));
      const FT_Size_Metrics &m = made.face->size->metrics;
      said.ascent = static_cast<float>(m.ascender) / 64.0f;
      said.descent = -static_cast<float>(m.descender) / 64.0f;
      said.height = static_cast<float>(m.height) / 64.0f;
    } else {
      said.ascent = size * 0.8f;
      said.descent = size * 0.2f;
      said.height = size * 1.2f;
    }
    made.ascent = said.ascent;
    said.x_height = size * 0.5f;
    fonts_.push_back(std::move(made));
    said.zero_width = this->measure("0", fonts_.size() - 1);
    return fonts_.size() - 1;
  }
  [[nodiscard]] float measure(std::string_view text, std::size_t font) {
    float width = 0.0f;
    for (const char32_t point : paint_sdl::code_points(text)) width += this->glyph_of(font, point).advance;
    return width;
  }
  bool begin(float scale) {
    if (renderer_ == nullptr) return false;
    SDL_SetRenderScale(renderer_, scale, scale);
    clips_.clear();
    SDL_SetRenderClipRect(renderer_, nullptr);
    return true;
  }
  void clear(colour c) {
    this->colour_to(c);
    SDL_RenderClear(renderer_);
  }
  void text(std::string_view words, std::size_t font, colour c, float x, float top) {
    const float baseline = top + fonts_.at(font).ascent;
    for (const char32_t point : paint_sdl::code_points(words)) {
      const glyph &one = this->glyph_of(font, point);
      if (one.texture) {
        SDL_SetTextureColorMod(one.texture, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
        SDL_SetTextureAlphaMod(one.texture, (c >> 24) & 0xFF);
        const SDL_FRect to{x + one.left, baseline - one.top, static_cast<float>(one.width), static_cast<float>(one.height)};
        SDL_RenderTexture(renderer_, one.texture, nullptr, &to);
      }
      x += one.advance;
    }
  }
  // A plate: square, or rounded a row at a time where its corners are.
  void fill(const box &at, const radii &r, colour c) {
    this->colour_to(c);
    const float top = std::max(r.top_left_y, r.top_right_y), bottom = std::max(r.bottom_left_y, r.bottom_right_y);
    if (top <= 0.0f && bottom <= 0.0f) {
      const SDL_FRect whole{at.x, at.y, at.width, at.height};
      SDL_RenderFillRect(renderer_, &whole);
      return;
    }
    const SDL_FRect middle{at.x, at.y + top, at.width, std::max(0.0f, at.height - top - bottom)};
    SDL_RenderFillRect(renderer_, &middle);
    // How far in a corner's edge is at a row, its radius across and down.
    const auto inset = [](float rx, float ry, float row) {
      if (rx <= 0.0f || ry <= 0.0f) return 0.0f;
      const float dy = (ry - row) / ry;
      return rx * (1.0f - std::sqrt(std::max(0.0f, 1.0f - dy * dy)));
    };
    for (float row = 0.0f; row < top; row += 1.0f) {
      const float left = inset(r.top_left_x, r.top_left_y, row + 0.5f), right = inset(r.top_right_x, r.top_right_y, row + 0.5f);
      const SDL_FRect line{at.x + left, at.y + row, at.width - left - right, 1.0f};
      SDL_RenderFillRect(renderer_, &line);
    }
    for (float row = 0.0f; row < bottom; row += 1.0f) {
      const float left = inset(r.bottom_left_x, r.bottom_left_y, row + 0.5f), right = inset(r.bottom_right_x, r.bottom_right_y, row + 0.5f);
      const SDL_FRect line{at.x + left, at.y + at.height - 1.0f - row, at.width - left - right, 1.0f};
      SDL_RenderFillRect(renderer_, &line);
    }
  }
  // A line round a box: its four sides, square.
  void stroke(const box &at, const radii &, float width, colour c) {
    this->colour_to(c);
    const std::array<SDL_FRect, 4> sides{SDL_FRect{at.x, at.y, at.width, width}, SDL_FRect{at.x, at.y + at.height - width, at.width, width},
                                         SDL_FRect{at.x, at.y, width, at.height}, SDL_FRect{at.x + at.width - width, at.y, width, at.height}};
    SDL_RenderFillRects(renderer_, sides.data(), static_cast<int>(sides.size()));
  }
  // Clips: square, each within the one before.
  void push_clip(const box &at, const radii &) {
    SDL_Rect next{static_cast<int>(std::floor(at.x)), static_cast<int>(std::floor(at.y)), static_cast<int>(std::ceil(at.width)),
                  static_cast<int>(std::ceil(at.height))};
    if (!clips_.empty()) {
      SDL_Rect both{};
      if (!SDL_GetRectIntersection(&clips_.back(), &next, &both)) both = SDL_Rect{0, 0, 0, 0};
      next = both;
    }
    clips_.push_back(next);
    SDL_SetRenderClipRect(renderer_, &clips_.back());
  }
  void pop_clip() {
    if (!clips_.empty()) clips_.pop_back();
    SDL_SetRenderClipRect(renderer_, clips_.empty() ? nullptr : &clips_.back());
  }
  void present() { SDL_RenderPresent(renderer_); }

private:
  struct glyph {
    SDL_Texture *texture = nullptr;
    int width = 0, height = 0;
    float left = 0.0f, top = 0.0f, advance = 0.0f;
  };
  struct kept {
    FT_Face face = nullptr;
    bool bold = false;
    float ascent = 0.0f;
    std::unordered_map<char32_t, glyph> glyphs;
  };
  void colour_to(colour c) { SDL_SetRenderDrawColor(renderer_, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (c >> 24) & 0xFF); }
  // A character's glyph in a face: rendered the first time, kept after.
  const glyph &glyph_of(std::size_t font, char32_t point) {
    kept &in = fonts_.at(font);
    if (const auto found = in.glyphs.find(point); found != in.glyphs.end()) return found->second;
    struct glyph made;
    if (in.face && FT_Load_Char(in.face, point, FT_LOAD_DEFAULT) == 0) {
      FT_GlyphSlot slot = in.face->glyph;
      if (in.bold && slot->format == FT_GLYPH_FORMAT_OUTLINE) FT_Outline_Embolden(&slot->outline, 64);
      if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) == 0) {
        const FT_Bitmap &bits = slot->bitmap;
        made.width = static_cast<int>(bits.width);
        made.height = static_cast<int>(bits.rows);
        made.left = static_cast<float>(slot->bitmap_left);
        made.top = static_cast<float>(slot->bitmap_top);
        made.advance = static_cast<float>(slot->advance.x) / 64.0f + (in.bold ? 1.0f : 0.0f);
        if (made.width > 0 && made.height > 0 && renderer_) {
          // White, its coverage its alpha: tinted as it is drawn.
          std::vector<std::uint32_t> pixels(static_cast<std::size_t>(made.width) * static_cast<std::size_t>(made.height));
          for (int y = 0; y < made.height; ++y)
            for (int x = 0; x < made.width; ++x)
              pixels[static_cast<std::size_t>(y * made.width + x)] =
                  (static_cast<std::uint32_t>(bits.buffer[y * bits.pitch + x]) << 24) | 0x00FFFFFFu;
          made.texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, made.width, made.height);
          if (made.texture) {
            SDL_UpdateTexture(made.texture, nullptr, pixels.data(), made.width * 4);
            SDL_SetTextureBlendMode(made.texture, SDL_BLENDMODE_BLEND);
          }
        }
      }
    }
    return in.glyphs.emplace(point, made).first->second;
  }

  SDL_Renderer *renderer_ = nullptr;
  FT_Library library_ = nullptr;
  std::optional<std::filesystem::path> file_;
  std::vector<kept> fonts_;
  std::vector<SDL_Rect> clips_;
};

} // namespace skiff::platform::page::detail
