// SPDX-License-Identifier: AGPL-3.0-only
// SKIFF_APP_WINDOW(page, Hooks): the program in a window of the system's --
// main, its tree and model and hooks as SKIFF_APP has them, shown by
// skiff.platform.window (skiff.app.window). A build with the window and the
// scene (SKIFF_FRAMEWORK_WINDOW, SKIFF_FRAMEWORK_CANVAS).
//
//   import skiff.app.window;
//   import myapp.page;
//   #include <skiff/app_window.h>
//   SKIFF_APP_WINDOW(myapp::page, myapp::Hooks)
#pragma once

#define SKIFF_APP_WINDOW(page, ...)                                                                                      \
  using skiff_app_window_t = ::skiff::app::App<&page __VA_OPT__(, ) __VA_ARGS__>;                                        \
  int main(int argc, char **argv) { return ::skiff::app::window::run<skiff_app_window_t>(argc, argv); }
