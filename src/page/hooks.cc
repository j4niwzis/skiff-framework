// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page:hooks -- what the page has of its own beyond the
// window's hooks (skiff.platform.window's, which it uses as they are): its
// own rules, and the clock its frames are timed by.
export module skiff.platform.page:hooks;

import std;

export namespace skiff::platform::page::detail {

[[nodiscard]] inline double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The page's own rules, before what the mirror writes: no margins, the
// program's face, and every box sized with its border.
inline constexpr std::string_view kPageCss = "html,body{margin:0;padding:0;height:100%}body{font-family:sans-serif}";

} // namespace skiff::platform::page::detail
