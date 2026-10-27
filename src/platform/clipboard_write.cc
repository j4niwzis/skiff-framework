// SPDX-License-Identifier: AGPL-3.0-only
export module skiff.platform.clipboard:write;
import std;
import sdl;
export namespace skiff::platform::clipboard {
// A picture put on the system's clipboard: its PNG, offered as image/png --
// what every program that pastes a picture takes -- held until the
// clipboard lets it go. SDL asks for it through the two functions it is
// given, as its C interface has it.
inline void copy_picture(std::string png) {
  auto* held = new std::string(std::move(png));
  const char* types[] = {"image/png"};
  sdl::SDL_SetClipboardData(
      +[](void* data, const char*, std::size_t* size) -> const void* {
        const auto* bytes = static_cast<const std::string*>(data);
        *size = bytes->size();
        return bytes->data();
      },
      +[](void* data) { delete static_cast<std::string*>(data); }, held, types, 1);
}

// Offer both plain labels and Matrix-compatible HTML for inline custom
// emoji. Other clients can consume the HTML without the in-process atoms.
inline void copy_text(std::string text, std::string html) {
  struct contents { std::string text, html; };
  auto* held = new contents{std::move(text), std::move(html)};
  const char* types[] = {"text/plain;charset=utf-8", "text/plain", "text/html"};
  sdl::SDL_SetClipboardData(
      +[](void* data, const char* mime, std::size_t* size) -> const void* {
        const auto& value = *static_cast<const contents*>(data);
        const auto& bytes = std::string_view(mime) == "text/html" ? value.html : value.text;
        *size = bytes.size();
        return bytes.data();
      }, +[](void* data) { delete static_cast<contents*>(data); }, held, types, 3);
}

}
