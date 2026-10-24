// skiff.ui's switch_on: of several views, the one a part of the model says
// shown -- on the page as the model moves, and served (Live) the same.
import std;
import skiff.app;

using namespace skiff;
using namespace skiff::ui;

namespace probe {
struct Which {
  int n = 0;
  friend bool operator==(const Which &, const Which &) = default;
};
struct Choose {
  [[nodiscard]] int operator()(const Which &w) const { return w.n; }
};
auto tree() { return state<Which>(column(vbox(), switch_on<Which>(Choose{}, Text("first view"), Text("second view")))); }
} // namespace probe

int failures = 0;
void check(bool ok, std::string_view what, std::string_view shown = {}) {
  std::println("{} {}", ok ? "ok  " : "FAIL", what);
  if (!ok) {
    ++failures;
    if (!shown.empty()) std::println("---\n{}\n---", shown);
  }
}
// Whether the text is in an element shown: its own start tag -- the one
// right before it -- does not say display:none.
bool shown(const std::string &html, std::string_view text) {
  const std::size_t at = html.find(text);
  if (at == std::string::npos) return false;
  const std::size_t tag = html.rfind('<', at);
  return html.substr(tag, at - tag).find("display:none") == std::string::npos;
}

int main() {
  using Tree = decltype(probe::tree());
  skiff::web::dom::reset();
  skiff::ui::ModelOf<Tree> m;
  skiff::web::Page page(skiff::ui::mount(skiff::ui::onPage, probe::tree()), m);
  page.mount();
  std::string html = skiff::web::dom::dump(skiff::web::dom::root());
  check(shown(html, "first view") && !shown(html, "second view"), "the first, as the part says", html);
  (void)m.apply(skiff::model::over<probe::Which>(skiff::model::setTo(probe::Which{1})));
  page.refresh();
  html = skiff::web::dom::dump(skiff::web::dom::root());
  check(!shown(html, "first view") && shown(html, "second view"), "the part moved: the second", html);

  skiff::ui::ModelOf<Tree> served;
  skiff::web::Live live(skiff::ui::mount(skiff::ui::onPage, probe::tree()), served, {.script = false});
  const std::string first = live.begin();
  check(first.contains("first view") && first.contains("second view"), "served: both written");
  (void)served.apply(skiff::model::over<probe::Which>(skiff::model::setTo(probe::Which{1})));
  const std::string next = live.update();
  check(!next.empty(), "served: the switch sent", next);
  check(shown(next, "second view") && !shown(next, "first view"), "served: the second shown, the first not", next);
  return failures == 0 ? 0 : 1;
}
