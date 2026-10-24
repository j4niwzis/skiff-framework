// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.fonts -- The faces the window's text is drawn with: those a
// program ships in its binary first (its faces), the system's behind them.
export module skiff.platform.fonts;

import std;
import skia;
import skiff.paint;

export namespace skiff::platform::fonts {
// The faces a program ships, said by a type with static members, each pair
// optional: `regular` and `semibold` (its text's -- bold drawn with the
// semibold face, not one thickened) and `emoji` (put before the system's,
// so that emoji are the same on every machine and never boxes). Each is the
// bytes of a font file as a contiguous range, kept by the program for as
// long as it runs: Skia reads them where they are, not copied.
//
//   struct faces {
//     static constexpr unsigned char regular[] = {
//   #embed "fonts/OpenSans-Regular.ttf"
//     };
//     ...
//   };
//
// A program's App says which with `using faces = ...;` (skiff.platform.window).
struct no_faces {};
template <class Faces>
concept ships_text = requires { Faces::regular; Faces::semibold; };
template <class Faces>
concept ships_emoji = requires { Faces::emoji; };

inline skia::Sp<skia::SkFontMgr> installed_manager;
inline skia::Sp<skia::SkTypeface> default_monospace;
// The program's own text faces, as loaded: what select_fonts goes back to.
inline skia::Sp<skia::SkTypeface> shipped_regular, shipped_semibold;

namespace detail {
template <std::ranges::contiguous_range Bytes>
skia::Sp<skia::SkTypeface> face_of(const skia::Sp<skia::SkFontMgr>& manager, const Bytes& bytes) {
  // The consumer keeps a pointer to them: the program's own, kept as long
  // as it runs -- not a copy.
  return manager->makeFromData(skia::SkData::MakeWithoutCopy(std::ranges::data(bytes), std::ranges::size(bytes)));
}
// The program's text faces, where it ships them; none else.
template <class Faces>
void load_text(const skia::Sp<skia::SkFontMgr>&) {}
template <ships_text Faces>
void load_text(const skia::Sp<skia::SkFontMgr>& manager) {
  shipped_regular = face_of(manager, Faces::regular);
  shipped_semibold = face_of(manager, Faces::semibold);
}
// Its emoji face, where it ships one: before anything of the system's.
template <class Faces>
void load_emoji(const skia::Sp<skia::SkFontMgr>&) {}
template <ships_emoji Faces>
void load_emoji(const skia::Sp<skia::SkFontMgr>& manager) {
  if (auto emoji = face_of(manager, Faces::emoji))
    skiff::paint::fonts().addFallback(std::move(emoji));
}
}  // namespace detail

// The program's faces first (Faces), the system's behind them for what they
// do not cover -- CJK, emoji. `font`: what every Text draws with, kept by the
// caller as long as the window draws.
template <class Faces = no_faces>
void load_fonts(const std::string& directory, skia::SkFont& font) {
  auto manager = skia::SkFontMgr_New_Custom_Directory(directory.c_str());
  if (!manager) {
    // No fonts found: the default face, rather than no text at all.
    font = skia::SkFont();
    skiff::paint::defaultFont() = &font;
    return;
  }
  skia::Sp<skia::SkTypeface> primary;
  for (const char* family : {"Inter", "Noto Sans", "DejaVu Sans", "Liberation Sans", "Cantarell"}) {
    primary = manager->matchFamilyStyle(family, skia::SkFontStyle());
    if (primary)
      break;
  }
  if (!primary && manager->countFamilies() > 0)
    primary = manager->createStyleSet(0)->createTypeface(0);
  detail::load_text<Faces>(manager);
  if (shipped_regular && shipped_semibold) {
    primary = shipped_regular;
    skiff::paint::fonts().setPrimary(shipped_regular, shipped_semibold);
  } else if (primary) {
    skiff::paint::fonts().setPrimary(primary);
  }
  detail::load_emoji<Faces>(manager);
  // Where a character no face loaded here has is looked for: the system's.
  installed_manager = manager;
  skiff::paint::fonts().setFontManager(manager);
  // Code in a monospace face of the system's: the first of the usual ones
  // that is there.
  for (const char* family : {"DejaVu Sans Mono", "Noto Sans Mono", "Liberation Mono", "Ubuntu Mono", "JetBrains Mono",
                             "Fira Mono", "Source Code Pro", "Cascadia Mono", "Consolas", "Menlo", "Courier New"})
    if (auto face = manager->matchFamilyStyle(family, skia::SkFontStyle())) {
      default_monospace = face;
      skiff::paint::fonts().setMonospace(std::move(face));
      break;
    }
  // What every Text and widget draws with. Without it they draw nothing:
  // the window was its boxes and no words.
  font = skia::SkFont(primary);
  // Smoothed, and fitted to the pixels only lightly, as Qt draws on Linux:
  // full hinting makes the text look heavier.
  font.setEdging(skia::SkFont::Edging::kAntiAlias);
  font.setHinting(skia::SkFontHinting::kSlight);
  font.setSubpixel(true);
  skiff::paint::defaultFont() = &font;
  for (const std::int32_t sample : {0x3042, 0xAC00, 0x4E00, 0x0627, 0x05D0, 0x0915, 0x1F600}) {
    if (auto face = manager->matchFamilyStyleCharacter(nullptr, skia::SkFontStyle(), nullptr, 0, sample))
      skiff::paint::fonts().addFallback(std::move(face));
  }
}

// The text's faces chosen by name or file, the shipped ones where a name is
// empty: whether they were found.
inline bool select_fonts(std::string_view regular, std::string_view monospace) {
  auto manager = installed_manager;
  if (!manager) return regular.empty() && monospace.empty();
  const auto face_for = [&](std::string_view name) {
    const std::string named(name);
    std::error_code error;
    if (std::filesystem::is_regular_file(std::filesystem::path(named), error))
      return manager->makeFromFile(named.c_str());
    for (int index = 0; index < manager->countFamilies(); ++index) {
      skia::SkString family;
      manager->getFamilyName(index, &family);
      if (named == family.c_str()) return manager->matchFamilyStyle(named.c_str(), skia::SkFontStyle());
    }
    return skia::Sp<skia::SkTypeface>{};
  };
  auto primary = regular.empty() ? shipped_regular : face_for(regular);
  auto code = monospace.empty() ? default_monospace : face_for(monospace);
  if (!primary || (!monospace.empty() && !code)) return false;
  if (regular.empty() && shipped_semibold)
    skiff::paint::fonts().setPrimary(primary, shipped_semibold);
  else
    skiff::paint::fonts().setPrimary(primary);
  skiff::paint::fonts().setMonospace(std::move(code));
  if (auto* font = skiff::paint::defaultFont()) font->setTypeface(std::move(primary));
  return true;
}

}  // namespace skiff::platform::fonts
