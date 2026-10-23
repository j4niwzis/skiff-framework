// The site, as a program. In a browser (Emscripten): the page mounted over
// the copy written when the site was built, its clock moved at every frame.
// Natively, when the site is built: the page mounted on the document kept in
// memory and written out -- static.html whole, with no script; and, given
// the page Emscripten made, the same markup put into it, for the browser to
// show before the WebAssembly has loaded.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
import std;
import skiff.ui;
import site.model;
import site.page;

namespace {
site::Model gModel;
using Tree = decltype(site::page());
std::optional<skiff::ui::Page<Tree, site::Model>> gPage;

void mountPage() {
  skiff::ui::mountTheme(skiff::web::dom::root());
  skiff::web::mountStylesheet(skiff::web::dom::root(), site::kSiteCss);
  gPage.emplace(site::page(), gModel);
  gPage->mount();
}
} // namespace

#ifdef __EMSCRIPTEN__
namespace {
double gStart = 0.0;
void tick() {
  const double seconds = (emscripten_get_now() - gStart) / 1000.0;
  (void)gModel.apply(skiff::model::over<site::Clock>(skiff::model::setTo(site::Clock{seconds})));
  gPage->refresh();
}
} // namespace
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_web_event(std::uint32_t handle, const char *event, const char *value) {
  if (gPage) gPage->event(handle, std::string_view(event), std::string_view(value));
}
int main() {
  site::seed(gModel);
  (void)gModel.apply(skiff::model::over<site::Shown>([](site::Shown s) {
    s.live = true;
    return s;
  }));
  // The copy written when the site was built, gone: the live page in its place.
  // The document's root found first: reset() clears what it has found.
  (void)skiff::web::dom::root();
  skiff::web::dom::reset();
  mountPage();
  gStart = emscripten_get_now();
  emscripten_set_main_loop(tick, 0, 0);
  return 0;
}
#else
namespace {
// What is inside the document's root, without the root's own tags.
std::string inner(std::string html) {
  const std::size_t open = html.find('>');
  const std::size_t close = html.rfind("</");
  if (open == std::string::npos || close == std::string::npos || close < open) return html;
  return html.substr(open + 1, close - open - 1);
}
std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
bool write(const std::string &path, std::string_view text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
  return static_cast<bool>(out);
}
constexpr std::string_view kHead = R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>skiff -- a C++ UI library</title>
<meta name="description" content="skiff: a C++ UI library -- a typed tree, a model it is bound to, and two backends: a scene drawn by Skia, and the page.">
</head>
<body>
<div id="skiff">)";
constexpr std::string_view kTail = "</div>\n</body>\n</html>\n";
} // namespace

// site [static.html] [index.html]: the page written out whole, and put into
// the page Emscripten made, in place of @@SKIFF-PRERENDER@@ -- text, which
// Emscripten keeps as it minifies the page; a comment it takes out.
int main(int argc, char **argv) {
  site::seed(gModel);
  mountPage();
  // The page's events, compiled here as in the browser (never taken here):
  // what they answer is checked by this build too.
  if (argc < 0) gPage->event(0, {}, {});
  const std::string markup = inner(skiff::web::dom::dump(skiff::web::dom::root()));
  if (argc < 2) {
    std::print("{}{}{}", kHead, markup, kTail);
    return 0;
  }
  if (!write(argv[1], std::string(kHead) + markup + std::string(kTail))) {
    std::println(std::cerr, "site: cannot write {}", argv[1]);
    return 1;
  }
  if (argc >= 3) {
    std::string page = read(argv[2]);
    constexpr std::string_view kMark = "@@SKIFF-PRERENDER@@";
    const std::size_t at = page.find(kMark);
    if (at == std::string::npos) {
      std::println(std::cerr, "site: no {} in {}", kMark, argv[2]);
      return 1;
    }
    page.replace(at, kMark.size(), markup);
    if (!write(argv[2], page)) {
      std::println(std::cerr, "site: cannot write {}", argv[2]);
      return 1;
    }
  }
  return 0;
}
#endif
