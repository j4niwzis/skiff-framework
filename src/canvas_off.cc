// skiff.app.canvas without the framework's canvas feature: no canvas, and no
// scene in the build. The page is all there is; whatever the program says
// of a canvas (Hooks::onCanvas) is never asked, and what the page's
// JavaScript would pass a canvas is taken by nothing.
export module skiff.app.canvas;

export namespace skiff::app::canvas {

template <class A> struct On {
  [[nodiscard]] static bool shown() { return false; }
  static void show(bool) {}
  static void frame(double) {}
  static void pointer(int, float, float, int) {}
  static int key(int, int, int) { return 0; }
  static int text(const char *) { return 0; }
};

} // namespace skiff::app::canvas
