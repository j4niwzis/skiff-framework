// The site of skiff, drawn by skiff: the same tree as the page's (body.inc),
// said against skiff's scene -- skiff.compose and skiff-widgets -- laid out
// by skiff and drawn through its painter. In a browser the painter draws on
// a <canvas> by its 2D context, through skiff's stand-in for Skia's types
// (SKIFF_SKIA off): no Skia, and no element of the document but the canvas.
// What the page has of its own -- a link, code, a frame -- is drawn here.
module;
#include <skiff/named.h>
// Named over the scene's views: skiff.ui says the page's (web::Named) for
// the units built against it, this one included.
#undef SKIFF_NAMED_BASE
#define SKIFF_NAMED_BASE ::skiff::compose::Named
export module site.canvas;

import std;
import skia;
import skiff.core;
import skiff.model;
import skiff.bind;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
import skiff.compose;
import skiff.widgets;
import site.model;

namespace site::canvas {
namespace scene = skiff::scene;
namespace model = skiff::model;
using skiff::compose::bound;
using skiff::compose::column;
using skiff::compose::each;
using skiff::compose::handle;
using skiff::compose::handlers;
using skiff::compose::hbox;
using skiff::compose::onClick;
using skiff::compose::row;
using skiff::compose::scoped;
using skiff::compose::spec_for;
using skiff::compose::styled;
using skiff::compose::vbox;
using skiff::nodes::Box;
using skiff::nodes::Flow;
using skiff::nodes::Text;
using skiff::widgets::ChoiceTabs;
using skiff::widgets::SendButton;
using skiff::widgets::TextField;
using skiff::widgets::ToggleField;
} // namespace site::canvas

// Exported, every piece, as the page's are (page.cc).
export namespace site::canvas {

inline constexpr bool kOnCanvas = true;

#include "palette.inc"
#include "tokens.inc"

// ---- the nodes the canvas has of its own ----

// A link followed: what the program does with it (in a browser, the page
// goes there) -- sent up from the link pressed, taken by the program.
struct OpenLink {
  std::string href;
};
// A link: a text that sends where it goes when it is clicked.
[[nodiscard]] auto Link(std::string label, std::string href, float size, scene::Colour c, bool bold = false) {
  return onClick(OpenLink{std::move(href)}, Text(std::move(label), size, c, bold));
}

// The colour a run of code is drawn in, by its kind (the page's .c-*).
[[nodiscard]] inline scene::Colour codeColour(std::string_view kind) {
  if (kind == "kw") return 0xffff9d76u;
  if (kind == "name") return 0xff8fd3e0u;
  if (kind == "str") return 0xffe7c79bu;
  if (kind == "num") return 0xfff2b5d4u;
  if (kind == "com") return 0xff7d8f9bu;
  return colour::kCodeInk;
}
// Code, as it is written -- its lines kept, as wide as what holds it and cut
// there -- each run in the colour of its kind: drawn by the painter, run by
// run, where the page has a <pre> of spans.
class Code : public skiff::scene::Node {
public:
  static constexpr float kSize = 13.5f;
  static constexpr float kLine = kSize * 1.55f;
  explicit Code(std::string code) : fCode(std::move(code)) {
    while (!fCode.empty() && fCode.back() == '\n') fCode.pop_back();
    const auto lines = static_cast<float>(1 + std::ranges::count(fCode, '\n'));
    this->apply(scene::Spec{.fillX = true, .height = lines * kLine + kSize * 0.4f});
  }
  template <class Hooks, class P> void drawSelf(this auto &self, Hooks &hooks, P &p, float alpha) {
    const skia::SkRect &box = self.fState.fBounds;
    p.save();
    p.clipRect(box);
    float x = box.left();
    float y = box.top() + kSize;
    for (const auto &[kind, run] : codeTokens(self.fCode)) {
      const scene::Colour ink = codeColour(kind);
      std::size_t from = 0;
      for (;;) {
        const std::size_t end = run.find('\n', from);
        const std::string piece(run.substr(from, end == std::string_view::npos ? std::string_view::npos : end - from));
        if (!piece.empty()) {
          p.text(piece, x, y, kSize, ink, alpha, false);
          x += p.measure(piece, kSize, false);
        }
        if (end == std::string_view::npos) break;
        x = box.left();
        y += kLine;
        from = end + 1;
      }
    }
    p.restore();
    skiff::scene::detail::paintOver(hooks, self.fState, p, alpha);
  }
  std::string fCode;
};
#include "body.inc"

// The scene's root: the page in a column as wide as the window. A routing
// begins at the root and goes into its children, so the page's scope -- and
// its handlers, which take what the buttons send -- is the column's child.
[[nodiscard]] auto scenePage() { return column(vbox(0.0f, {.fillX = true, .autoSize = scene::axes::kY}), page()); }

} // namespace site::canvas
