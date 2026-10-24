// SKIFF_APP(page, Hooks): the program -- main, and what the page's
// JavaScript calls (skiff_web_event, skiff_canvas_*) -- for a tree said by
// `page` (a function returning a view in skiff.ui) and what the program adds
// (Hooks; see skiff.app). Written once, at namespace scope, in the unit with
// no main of its own, after `import skiff.app;`:
//
//   import std;
//   import skiff.app;
//   import myapp.page;
//   #include <skiff/app.h>
//   SKIFF_APP(myapp::page, myapp::Hooks)
//
// A macro, because main and the functions JavaScript calls are the
// program's own symbols: made here, each calls the program's App directly.
#pragma once

#ifdef __EMSCRIPTEN__
#define SKIFF_APP_ENTRIES(app)                                                                                           \
  extern "C" __attribute__((used)) void skiff_web_event(unsigned handle, const char *what, const char *value) {          \
    app::event(handle, what, value);                                                                                     \
  }                                                                                                                      \
  extern "C" __attribute__((used)) void skiff_canvas_pointer(int kind, float x, float y, int button) {                   \
    app::pointer(kind, x, y, button);                                                                                    \
  }                                                                                                                      \
  extern "C" __attribute__((used)) int skiff_canvas_key(int code, int shift, int control) {                              \
    return app::key(code, shift, control);                                                                               \
  }                                                                                                                      \
  extern "C" __attribute__((used)) int skiff_canvas_text(const char *typed) { return app::text(typed); }               \
  extern "C" __attribute__((used)) int skiff_loopback_open(int script, const char *action) {                             \
    return app::loopbackOpen(script, action);                                                                            \
  }                                                                                                                      \
  extern "C" __attribute__((used)) char *skiff_loopback_begin(int id) { return app::loopbackBegin(id); }                 \
  extern "C" __attribute__((used)) char *skiff_loopback_next(int id) { return app::loopbackNext(id); }                   \
  extern "C" __attribute__((used)) void skiff_loopback_post(int id, const char *body) { app::loopbackPost(id, body); }  \
  extern "C" __attribute__((used)) void skiff_loopback_close(int id) { app::loopbackClose(id); }                       \
  extern "C" __attribute__((used)) void skiff_app_wake() { app::settle(); }
#else
#define SKIFF_APP_ENTRIES(app)
#endif

#define SKIFF_APP(page, ...)                                                                                             \
  using skiff_app_t = ::skiff::app::App<&page __VA_OPT__(, ) __VA_ARGS__>;                                               \
  SKIFF_APP_ENTRIES(skiff_app_t)                                                                                         \
  int main(int argc, char **argv) { return skiff_app_t::run(argc, argv); }
