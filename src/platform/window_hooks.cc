// SPDX-License-Identifier: AGPL-3.0-only
// The application contract shared by native and browser scene hosts.
export module skiff.platform.window:hooks;
import std;
import skia;
import skiff.platform.fonts;
import skiff.platform.window_setup;

export namespace skiff::platform::window {
using fonts::load_fonts;
// The faces a program ships (skiff.platform.fonts): its App's `faces`, where
// it says any.
template <class App>
struct faces_of {
  using type = fonts::no_faces;
};
template <class App>
  requires requires { typename App::faces; }
struct faces_of<App> {
  using type = typename App::faces;
};
// What the window asks of a program beyond its scene, each with what it does
// where the program says nothing.
namespace hooks {
// The interface's scale, in percent of the system's: the program's, else 100.
template <class App>
int interface_scale(const App&) { return 100; }
template <class App>
  requires requires(const App& app) { { app.interface_scale() } -> std::convertible_to<int>; }
int interface_scale(const App& app) { return app.interface_scale(); }
// Whether the program is pointed at with a finger now, told as it changes.
// Whether the window is shaped as a phone's, for a long press to be its
// right press: narrow and taller than wide, unless the program says
// otherwise -- a layout held to portrait or landscape (mux's Settings).
template <class App>
bool phone_shaped(const App&, float width, float height) { return width < 600.0f && height > width; }
template <class App>
  requires requires(const App& app, float side) { { app.phone_shaped(side, side) } -> std::convertible_to<bool>; }
bool phone_shaped(const App& app, float width, float height) { return app.phone_shaped(width, height); }
template <class App>
void touch_changed(App&, bool) {}
template <class App>
  requires requires(App& app) { app.touch_changed(true); }
void touch_changed(App& app, bool on) { app.touch_changed(on); }
// A right press to give again, where the program keeps one: a menu closed by
// a right press, and another opened where it was pressed.
template <class App>
std::optional<skia::SkPoint> take_press_again(App&) { return std::nullopt; }
template <class App>
  requires requires(App& app) { { app.take_press_again() } -> std::convertible_to<std::optional<skia::SkPoint>>; }
std::optional<skia::SkPoint> take_press_again(App& app) { return app.take_press_again(); }
// How frames are made: the program's (its frames(), whatever its type, read
// by these names), else these.
struct frame_options {
  bool partial_redraw = true;  // only what changed repainted
  bool vsync = true;           // shown in step with the screen
  bool show_fps = false;       // the frame counter in the corner
  bool flash_redraws = false;  // what each frame repainted, outlined
};
template <class App>
frame_options frames(const App&) { return {}; }
template <class App>
  requires requires(const App& app) { app.frames().partial_redraw; app.frames().vsync; app.frames().show_fps; app.frames().flash_redraws; }
frame_options frames(const App& app) {
  const auto& said = app.frames();
  return {.partial_redraw = said.partial_redraw, .vsync = said.vsync, .show_fps = said.show_fps, .flash_redraws = said.flash_redraws};
}
// What the program is told, where it listens: each a no-op where it does not.
template <class App>
void woken(App&) {}
template <class App>
  requires requires(App& app) { app.woken(); }
void woken(App& app) { app.woken(); }
template <class App>
void after_event(App&) {}
template <class App>
  requires requires(App& app) { app.after_event(); }
void after_event(App& app) { app.after_event(); }
template <class App>
void before_frame(App&) {}
template <class App>
  requires requires(App& app) { app.before_frame(); }
void before_frame(App& app) { app.before_frame(); }
template <class App>
void closing(App&) {}
template <class App>
  requires requires(App& app) { app.closing(); }
void closing(App& app) { app.closing(); }
template <class App>
void backgrounded(App&) {}
template <class App>
  requires requires(App& app) { app.backgrounded(); }
void backgrounded(App& app) { app.backgrounded(); }
template <class App>
void focus_changed(App&, bool on) {}
template <class App>
  requires requires(App& app, bool on) { app.focus_changed(on); }
void focus_changed(App& app, bool on) { app.focus_changed(on); }
template <class App>
void shown_changed(App&, bool on) {}
template <class App>
  requires requires(App& app, bool on) { app.shown_changed(on); }
void shown_changed(App& app, bool on) { app.shown_changed(on); }
template <class App>
void open_link(App&, std::string url) {}
template <class App>
  requires requires(App& app, std::string url) { app.open_link(std::move(url)); }
void open_link(App& app, std::string url) { app.open_link(std::move(url)); }
template <class App>
void dialog_failed(App&, std::string why) {}
template <class App>
  requires requires(App& app, std::string why) { app.dialog_failed(std::move(why)); }
void dialog_failed(App& app, std::string why) { app.dialog_failed(std::move(why)); }
template <class App>
void save_path_chosen(App&, std::string path) {}
template <class App>
  requires requires(App& app, std::string path) { app.save_path_chosen(std::move(path)); }
void save_path_chosen(App& app, std::string path) { app.save_path_chosen(std::move(path)); }
template <class App>
void files_given(App&, std::vector<std::string>, bool) {}
template <class App>
  requires requires(App& app, std::vector<std::string> paths) { app.files_given(std::move(paths), true); }
void files_given(App& app, std::vector<std::string> paths, bool dropped) { app.files_given(std::move(paths), dropped); }
// The system's file dialogs, where the program has them (its system_dialogs,
// skiff.platform.dialogs): put over the window, and what they answered taken.
template <class App, class Window, class Kinds>
void dialogs_over(App&, Window*, const Kinds&) {}
template <class App, class Window, class Kinds>
  requires requires(App& app, Window* window, const Kinds& kinds) { app.system_dialogs.over(window, kinds); }
void dialogs_over(App& app, Window* window, const Kinds& kinds) { app.system_dialogs.over(window, kinds); }
template <class App>
void dialogs_answered(App&) {}
template <class App>
  requires requires(App& app) { app.system_dialogs.take_files(); app.system_dialogs.take_failures(); }
void dialogs_answered(App& app) {
  for (std::vector<std::string>& paths : app.system_dialogs.take_files())
    files_given(app, std::move(paths), false);
  for (std::string& why : app.system_dialogs.take_failures())
    dialog_failed(app, std::move(why));
}
template <class App>
void dialogs_saved(App&) {}
template <class App>
  requires requires(App& app) { app.system_dialogs.take_save_paths(); }
void dialogs_saved(App& app) {
  for (std::string& path : app.system_dialogs.take_save_paths())
    save_path_chosen(app, std::move(path));
}
}  // namespace hooks
}  // namespace skiff::platform::window

export namespace skiff::platform::window {

struct options {
  std::string title = "skiff";
  int width = 1100;
  int height = 720;
  bool software = false;  // skip the GPU even where there is one
  // A window with an alpha channel the desktop blends: what is repainted
  // cleared first, the scene no longer opaque at its bottom.
  bool transparent = false;
  std::string fonts = window_setup::fonts;
  // Where the GPU programs compiled are kept between runs (a directory);
  // compiled again at every run where it is empty.
  std::string programs;
};

} // namespace skiff::platform::window
