// The site of skiff, written with skiff: on the page, skiff.ui on skiff.web --
// its nodes the document's elements, bound to the model. In a browser it is
// the page itself, C++ compiled to WebAssembly; when the site is built, the
// same tree is mounted natively on a document kept in memory and written
// out as HTML, which is what a browser shows first. The tree is body.inc;
// the canvas draws the same one (canvas.cc).
module;
#include <skiff/named.h>
export module site.page;

import std;
import skiff.ui;
import site.model;

namespace site::html {
using namespace skiff;
using namespace skiff::ui;
namespace dom = skiff::web::dom;
} // namespace site::html

// Exported, every piece: kept to the module, clang 23 recursed without end
// looking for their definitions from the unit importing the page.
export namespace site::html {

inline constexpr bool kOnCanvas = false;

#include "palette.inc"

// ---- the nodes the page has of its own ----

// A link: a text that is an <a>.
struct Link : Text {
  Link(std::string label, std::string href, float size, scene::Colour c, bool bold = false)
      : Text(std::move(label), size, c, bold), fHref(std::move(href)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "a"; }
  void mounted() {
    Text::mounted();
    dom::attr(fState.fHandle, "class", "skiff-text site-link");
    dom::attr(fState.fHandle, "href", fHref);
  }
  std::string fHref;
};
// Code, as it is written -- its lines kept -- and coloured as an editor
// colours C++: each word, string, number and comment a span of its kind.
struct Code : skiff::web::Node {
  explicit Code(std::string code) : fCode(std::move(code)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "pre"; }
  void mounted() {
    dom::attr(fState.fHandle, "class", "site-code");
    for (const auto &[kind, text] : tokens(fCode)) {
      const dom::Handle span = dom::create("span");
      if (!kind.empty()) dom::attr(span, "class", std::string("c-").append(kind));
      dom::text(span, text);
      dom::append(fState.fHandle, span);
    }
  }
  // The code cut into runs of a kind: "kw", "name", "str", "num", "com", or
  // "" for the rest.
  [[nodiscard]] static std::vector<std::pair<std::string_view, std::string_view>> tokens(std::string_view code) {
    static constexpr std::array<std::string_view, 24> kKeywords{
        "auto", "return", "const", "constexpr", "struct", "class", "template", "typename", "using", "namespace", "if", "else",
        "for", "while", "static", "inline", "void", "bool", "int", "float", "true", "false", "import", "this"};
    std::vector<std::pair<std::string_view, std::string_view>> out;
    const auto add = [&](std::string_view kind, std::size_t from, std::size_t to) {
      if (to <= from) return;
      if (kind.empty() && !out.empty() && out.back().first.empty())
        out.back().second = code.substr(out.back().second.data() - code.data(), to - (out.back().second.data() - code.data()));
      else
        out.emplace_back(kind, code.substr(from, to - from));
    };
    const auto word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    std::size_t i = 0;
    while (i < code.size()) {
      const char c = code[i];
      if (code.substr(i, 2) == "//") {
        const std::size_t end = std::min(code.find('\n', i), code.size());
        add("com", i, end);
        i = end;
      } else if (c == '"' || c == '\'') {
        std::size_t end = i + 1;
        while (end < code.size() && code[end] != c) end += code[end] == '\\' ? 2 : 1;
        end = std::min(end + 1, code.size());
        add("str", i, end);
        i = end;
      } else if (std::isdigit(static_cast<unsigned char>(c))) {
        std::size_t end = i;
        while (end < code.size() && (word(code[end]) || code[end] == '.')) ++end;
        add("num", i, end);
        i = end;
      } else if (word(c)) {
        std::size_t end = i;
        while (end < code.size() && word(code[end])) ++end;
        const std::string_view w = code.substr(i, end - i);
        std::size_t next = end;
        while (next < code.size() && code[next] == ' ') ++next;
        const bool called = next < code.size() && (code[next] == '(' || code[next] == '<');
        add(std::ranges::contains(kKeywords, w) ? "kw" : (called || std::isupper(static_cast<unsigned char>(w[0]))) ? "name" : "", i, end);
        i = end;
      } else {
        add("", i, i + 1);
        ++i;
      }
    }
    return out;
  }
  std::string fCode;
};
// Another page, framed: the copy of this one written when the site was built.
struct Framed : skiff::web::Node {
  explicit Framed(std::string src, scene::Spec spec = {}) : fSrc(std::move(src)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "iframe"; }
  void mounted() {
    dom::attr(fState.fHandle, "src", fSrc);
    dom::attr(fState.fHandle, "loading", "lazy");
    dom::attr(fState.fHandle, "title", "This page, as it was written when the site was built");
    dom::attr(fState.fHandle, "class", "site-frame");
  }
  std::string fSrc;
};

#include "body.inc"

// The stylesheet the site adds to skiff's: the page's font, code kept as it
// is written, links, the framed copy and the pictures.
inline constexpr std::string_view kSiteCss = R"(
html,body{margin:0;background:#f4efe6}
#skiff{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#1d2b36}
.site-code{margin:0;white-space:pre;display:block;overflow-x:auto;max-width:100%;box-sizing:border-box;font:13.5px/1.55 ui-monospace,SFMono-Regular,Menlo,monospace;color:#e8eef2}
.c-kw{color:#ff9d76}.c-name{color:#8fd3e0}.c-str{color:#e7c79b}.c-num{color:#f2b5d4}.c-com{color:#7d8f9b;font-style:italic}
.site-link{text-decoration:none}
.site-link:hover{text-decoration:underline}
.site-frame{border:1px solid #c9bfb1;border-radius:10px;background:#f4efe6;width:100%}
#skiff .skiff-tab{color:#5a6b77;font-size:17px;padding:6px 0}
@media (max-width:640px){[data-view=Header]{flex-wrap:wrap;row-gap:6px}}
#skiff .skiff-tab:hover{color:#1d2b36}
#skiff .skiff-tab[aria-selected=true]{color:#1d2b36;font-weight:700;box-shadow:inset 0 -2px #d9603b}
)";

} // namespace site::html
