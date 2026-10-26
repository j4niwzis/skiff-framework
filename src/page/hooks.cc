// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.page:hooks -- what either engine's page asks of the program
// beyond its scene (its hooks, each doing nothing where the program says
// nothing, as the window's), the faces it ships, the page's own rules, and
// the clock its frames are timed by.
export module skiff.platform.page:hooks;

import std;
import skiff.platform.fonts;

export namespace skiff::platform::page::detail {

[[nodiscard]] inline double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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

} // namespace skiff::platform::page::detail
