// A page alone, as a frontend has it: skiff.app with no canvas feature
// needed -- words, and a canvas of the page's own (a <canvas> element among
// the others, drawn by its node through the page's painter), not the whole
// tree drawn by the scene. Written out natively; the test reads the page.
import std;
import skiff.app;
#include <skiff/app.h>

namespace probe {
// A drawing of the page's own: a triangle and a dot, in a <canvas> 160 by 90.
struct Sketch : skiff::web::Canvas {
  Sketch() { setSize(160, 90); }
  template <class P> void paint(P &p) {
    p.clear(0xff102030u);
    const skiff::scene::Point tri[] = {{10.0f, 80.0f}, {80.0f, 10.0f}, {150.0f, 80.0f}};
    p.fillPolygon(tri, 0xffe7c79bu);
    p.circle({80.0f, 55.0f}, 8.0f, 0xffff9d76u);
  }
};
// The drawing as a view: on the page only -- there is no other backend here.
struct Drawing {
  auto on(skiff::ui::OnPage) && { return Sketch(); }
};
auto page() { return skiff::ui::column(skiff::ui::Text("A page, and a canvas in it", 20.0f, 0xff000000u, true), Drawing()); }
} // namespace probe

SKIFF_APP(probe::page)
