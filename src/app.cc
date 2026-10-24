// skiff.app: what nearly every program written with skiff does, done once.
// A program says its tree (a function returning a view in skiff.ui) and
// what it adds (Hooks), and the program is
//
//   SKIFF_APP(myapp::page, myapp::Hooks)        // <skiff/app.h>
//
// In a browser (Emscripten): the tree mounted on the page -- over the copy
// of it written when the program was built, where there is one, taken over
// rather than made again -- the model moved at every frame and the page
// following it; and, with the framework's canvas feature and where the
// program says so, the same tree drawn by skiff's scene on a canvas instead,
// one model under both (skiff.app.canvas). Natively: the
// page written out as HTML, for a browser to show before the WebAssembly has
// loaded (prerender), or for a search engine. The model is the parts the
// tree's views keep (skiff::ui::ModelOf).
//
// What a program adds is said by Hooks, every member of it optional:
//   void dress()                          the theme, before anything is made
//   void start(Model &)                   the model as the program begins
//   void frame(Model &, double seconds)   at every frame, before the views follow
//   std::string_view stylesheet()         the program's CSS, after skiff's
//   void open(std::string_view href)      a link followed
// and, with the canvas feature:
//   bool onCanvas(const Model &)          the canvas shown, not the page
//   bool outlineRepaints(const Model &)   each repaint of the canvas outlined
//   skiff::scene::Colour paper()          what the canvas is cleared to
//   std::string_view head()               the <head> written out with the page
//
// With the framework's ssr feature, the model the page was written with
// travels with it (skiff.app.carry): written into the page as JSON by knot
// beside the markup and read back in the browser before the page is taken
// over, start() then not run. Without it -- a program that is a page in the
// browser alone -- nothing travels, and the page is made where nothing was
// written before it.
//
// With the live feature, natively, `program serve [port]` serves the page
// live instead (skiff.app.serve): the tree and a model for each browser kept
// on the server, the page one HTML stream appended to as the model moves,
// what is done on it posted back as a form -- no script, no WebAssembly.
// Parts said client(view) are the browser's own: the program built for the
// browser, served with the page (serve --client DIR), takes each over where
// the server wrote it -- rendered partly on the server, partly in the
// browser, where there is script.
//
// Without the canvas feature -- a frontend, whose page is the browser's
// elements and nothing else -- there is no scene in the build at all: no
// canvas, none of its JavaScript, and skiff's scene and nodes not compiled;
// onCanvas, if a program says it, is never asked.
module;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
export module skiff.app;

export import skiff.ui;
import std;
import skiff.app.carry;
import skiff.app.canvas;
import skiff.app.serve;
import skiff.app.net;

#ifdef __EMSCRIPTEN__
// ---- the page's side, in JavaScript ---------------------------------------
// A link followed, and the state the page was written with. The canvas's
// own is skiff.app.canvas's.
// clang-format off
EM_JS(void, skiffAppOpen, (const char *href, int length), { window.location.href = UTF8ToString(href, length); });
// The state the page was written with, where it carries one: its text, made
// for the program to free; else none.
// Whether the page was served live (skiff.app.serve): its parts the
// server's, but for those that are the browser's own (clients).
EM_JS(int, skiffAppServed, (), { return document.querySelector('skiff-live') ? 1 : 0; });
EM_JS(char *, skiffAppStateText, (), {
  const kept = document.getElementById('skiff-state');
  return kept ? stringToNewUTF8(kept.textContent) : 0;
});
// clang-format on
#endif

export namespace skiff::app {

// Whether this is the program in a browser, alive: not the one writing the
// page out when the program is built.
#ifdef __EMSCRIPTEN__
inline constexpr bool kInBrowser = true;
#else
inline constexpr bool kInBrowser = false;
#endif

// A program that adds nothing.
struct NoHooks {};


namespace detail {
// The page written out: what is inside the document's root, without the
// root's own tags.
inline std::string inner(std::string html) {
  const std::size_t open = html.find('>');
  const std::size_t close = html.rfind("</");
  if (open == std::string::npos || close == std::string::npos || close < open) return html;
  return html.substr(open + 1, close - open - 1);
}
inline std::string read(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
inline bool write(const std::string &path, std::string_view text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
  return static_cast<bool>(out);
}
inline constexpr std::string_view kHead = R"(<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
)";
} // namespace detail

// The program: its tree (View, a function returning a view in skiff.ui) and
// what it adds (Hooks). Everything here is the program's one of each --
// static, as a program has one page and one canvas.
template <auto View, class Hooks = NoHooks> struct App {
  using Tree = decltype(View());
  using Model = skiff::ui::ModelOf<Tree>;
  using PageTree = skiff::ui::Mounted<skiff::ui::OnPage, Tree>;
  static Tree view() { return View(); }

  static Model &model() {
    static Model kept;
    return kept;
  }
  static Hooks &hooks() {
    static Hooks kept;
    return kept;
  }
  // What nothing in the tree takes: a link followed, the program's to do.
  struct Sink {
    void take(const skiff::ui::OpenLink &link) const {
      if constexpr (requires { hooks().open(std::string_view(link.href)); })
        hooks().open(std::string_view(link.href));
#ifdef __EMSCRIPTEN__
      else
        skiffAppOpen(link.href.data(), static_cast<int>(link.href.size()));
#endif
    }
  };
  static Sink &sink() {
    static Sink kept;
    return kept;
  }

  // ---- the page ----
  static std::optional<skiff::web::Page<PageTree, Model>> &page() {
    static std::optional<skiff::web::Page<PageTree, Model>> kept;
    return kept;
  }
  // The stylesheets before the tree: skiff's theme, and the program's.
  static std::size_t sheets() {
    if constexpr (requires { hooks().stylesheet(); })
      return 2;
    else
      return 1;
  }
  static void mountPage() {
    const skiff::web::dom::Handle root = skiff::web::dom::root();
    skiff::ui::mountTheme(root);
    if constexpr (requires { hooks().stylesheet(); })
      skiff::web::mountStylesheet(root, hooks().stylesheet());
    page().emplace(skiff::ui::mount(skiff::ui::onPage, View()), model());
    page()->mount();
  }
  // What happens at a node of the page (skiff_web_event): the page's, or,
  // where the page was served live, the client's it happened in.
  static void event(std::uint32_t handle, const char *what, const char *value) {
    if (page()) page()->event(handle, std::string_view(what), std::string_view(value));
    if (served())
      skiff::web::forEachClient(*served(), [&](auto &one) {
        if (one.element() != skiff::web::dom::kNone && one.client().owns(handle))
          one.client().event(handle, std::string_view(what), std::string_view(value));
      });
  }
  // The tree, where the page was served live: kept for its clients alone --
  // each taken over where the server wrote it, its page the browser's.
  static std::optional<PageTree> &served() {
    static std::optional<PageTree> kept;
    return kept;
  }

  static void begin(bool arrived = false) {
    if constexpr (requires { hooks().dress(); })
      hooks().dress();
    if constexpr (requires { hooks().start(model()); })
      if (!arrived)
        hooks().start(model());
  }
  // The state the page was written with, written into it: where the model
  // travels (skiff.app.carry); else nothing.
  static std::string stateScript() { return skiff::app::carry::script(model().root()); }

  // Written out natively: `program [whole.html] [index.html]` -- the page
  // whole, and put into the page Emscripten made, in place of
  // @@SKIFF-PRERENDER@@ (text, which Emscripten keeps as it minifies the
  // page; a comment it takes out). With neither, to the output.
  static int prerender(int argc, char **argv) {
    begin();
    mountPage();
    // The page's events, compiled here as in the browser, never taken here:
    // what they answer is checked by this build too.
    if (argc < 0) event(0, "", "");
    const std::string markup = detail::inner(skiff::web::dom::dump(skiff::web::dom::root()));
    std::string head(detail::kHead);
    if constexpr (requires { hooks().head(); })
      head += std::string(hooks().head());
    const std::string state = stateScript();
    const std::string whole = "<!doctype html>\n<html lang=\"en\">\n<head>\n" + head + "</head>\n<body>\n<div id=\"skiff\">" + markup +
                              "</div>\n" + state + "\n</body>\n</html>\n";
    if (argc < 2) {
      std::print("{}", whole);
      return 0;
    }
    if (!detail::write(argv[1], whole)) {
      std::println(std::cerr, "skiff::app: cannot write {}", argv[1]);
      return 1;
    }
    if (argc >= 3) {
      std::string made = detail::read(argv[2]);
      constexpr std::string_view kMark = "@@SKIFF-PRERENDER@@";
      const std::size_t at = made.find(kMark);
      if (at == std::string::npos) {
        std::println(std::cerr, "skiff::app: no {} in {}", kMark, argv[2]);
        return 1;
      }
      made.replace(at, kMark.size(), markup);
      // The state beside it, where the page says where (@@SKIFF-STATE@@,
      // outside the page's root: what is taken over is the root's own).
      constexpr std::string_view kState = "@@SKIFF-STATE@@";
      if (const std::size_t there = made.find(kState); there != std::string::npos)
        made.replace(there, kState.size(), state);
      if (!detail::write(argv[2], made)) {
        std::println(std::cerr, "skiff::app: cannot write {}", argv[2]);
        return 1;
      }
    }
    return 0;
  }

#ifdef __EMSCRIPTEN__
  // The canvas: the tree drawn by skiff's scene instead of the page, where
  // the framework has its canvas feature and the program says so; else
  // nothing (skiff.app.canvas).
  using Canvas = skiff::app::canvas::On<App>;
  static void pointer(int kind, float x, float y, int button) { Canvas::pointer(kind, x, y, button); }
  static int key(int code, int shift, int control) { return Canvas::key(code, shift, control); }
  static int text(const char *typed) { return Canvas::text(typed); }

  // ---- the frame ----
  static double &startedAt() {
    static double kept = 0.0;
    return kept;
  }
  // What the model asks of the network (skiff.app.net), asked by the
  // browser's fetch: stepped at every frame.
  static skiff::app::net::Runner<Model, skiff::app::net::browser> &runner() {
    static skiff::app::net::Runner<Model, skiff::app::net::browser> kept{skiff::app::net::browser{}};
    return kept;
  }
  static void tick() {
    const double now = emscripten_get_now();
    if constexpr (requires { hooks().frame(model(), 0.0); })
      hooks().frame(model(), (now - startedAt()) / 1000.0);
    runner().step(model());
    bool onCanvas = false;
    if constexpr (requires { hooks().onCanvas(std::as_const(model())); })
      onCanvas = hooks().onCanvas(std::as_const(model()));
    if (onCanvas != Canvas::shown()) Canvas::show(onCanvas);
    if (Canvas::shown())
      Canvas::frame(now);
    else
      page()->refresh();
  }
  // In the browser: the copy written when the program was built -- the
  // stylesheets, then the page -- taken over, not made again: the same
  // elements, now the live page's. Where there is none, made.
  // The state the page was written with, read into the model: whether it
  // came.
  static bool arrive() {
    char *text = skiffAppStateText();
    if (!text) return false;
    const std::string kept(text);
    std::free(text);
    auto read = skiff::app::carry::read<typename Model::RootType>(kept);
    if (!read) return false;
    model() = Model(std::move(*read));
    return true;
  }
  // Served live: each client the server wrote taken over -- hydrated, with
  // its own model -- as it arrives in the stream; the rest is the server's.
  static void takeOver() {
    skiff::web::forEachClient(*served(), [](auto &one) {
      if (one.element() != skiff::web::dom::kNone) return;
      const skiff::web::dom::Handle at = skiff::web::dom::marked("data-skiff-client", std::to_string(one.fIndex));
      if (at == skiff::web::dom::kNone) return;
      one.fState.fHandle = at;
      one.client().hydrate(at, 0);
      skiff::web::dom::attr(at, "data-skiff-hydrated", "");
    });
  }
  static void servedTick() {
    takeOver();
    skiff::web::forEachClient(*served(), [](auto &one) {
      if (one.element() != skiff::web::dom::kNone) one.client().refresh();
    });
  }
  static int hybrid() {
    if constexpr (requires { hooks().dress(); })
      hooks().dress();
    served().emplace(skiff::ui::mount(skiff::ui::onPage, View()));
    skiff::web::numberClients(*served());
    emscripten_set_main_loop([] { servedTick(); }, 0, 0);
    return 0;
  }
  // ---- the page as its own server: loopback ----
  // What a server does with the page (skiff::web::Live), done here in the
  // browser: each session a model of its own and the page kept for it, its
  // stream handed out a piece at a time, a form posted taken -- for the
  // framework's service worker (web/skiff-sw.js) to answer requests with as
  // a server would, so that the page served, with script or without, is
  // seen with no server at all (the site's switch).
  struct Loopback {
    Model model;
    std::optional<skiff::web::Live<PageTree, Model>> page;
    // What its model asks of the network, asked by the browser's fetch.
    skiff::app::net::Runner<Model, skiff::app::net::browser> runner{skiff::app::net::browser{}};
  };
  static std::map<int, std::unique_ptr<Loopback>> &loopbacks() {
    static std::map<int, std::unique_ptr<Loopback>> kept;
    return kept;
  }
  // A piece of the stream, for the page's JavaScript to read and free; none
  // where there is nothing.
  static char *handedOut(const std::string &text) {
    if (text.empty()) return nullptr;
    char *made = static_cast<char *>(std::malloc(text.size() + 1));
    std::memcpy(made, text.data(), text.size());
    made[text.size()] = '\0';
    return made;
  }
  static int loopbackOpen(int script, const char *action) {
    static int last = 0;
    const int id = ++last;
    Loopback &one = *loopbacks().emplace(id, std::make_unique<Loopback>()).first->second;
    if constexpr (requires { hooks().start(one.model); })
      hooks().start(one.model);
    typename skiff::web::Live<PageTree, Model>::Options options;
    options.action = std::string(action) + "?s=" + std::to_string(id);
    options.css = skiff::ui::themeCss(skiff::widgets::theme());
    if constexpr (requires { hooks().stylesheet(); })
      options.css += std::string(hooks().stylesheet());
    if constexpr (requires { hooks().head(); })
      options.head = std::string(hooks().head());
    options.script = script != 0;
    one.page.emplace(skiff::ui::mount(skiff::ui::onPage, View()), one.model, std::move(options));
    return id;
  }
  static Loopback *loopback(int id) {
    const auto found = loopbacks().find(id);
    return found == loopbacks().end() ? nullptr : found->second.get();
  }
  static char *loopbackBegin(int id) {
    Loopback *one = loopback(id);
    return one ? handedOut(one->page->begin()) : nullptr;
  }
  static char *loopbackNext(int id) {
    Loopback *one = loopback(id);
    if (!one) return nullptr;
    one->runner.step(one->model);
    return handedOut(one->page->update());
  }
  static void loopbackPost(int id, const char *body) {
    if (Loopback *one = loopback(id)) {
      one->page->post(std::string_view(body));
      one->runner.step(one->model);
    }
  }
  static void loopbackClose(int id) { loopbacks().erase(id); }

  static int live() {
    if (skiffAppServed()) return hybrid();
    begin(arrive());
    const skiff::web::dom::Handle root = skiff::web::dom::root();
    if (skiff::web::dom::children(root) > sheets()) {
      page().emplace(skiff::ui::mount(skiff::ui::onPage, View()), model());
      page()->hydrate(root, sheets());
    } else {
      mountPage();
    }
    startedAt() = emscripten_get_now();
    emscripten_set_main_loop([] { tick(); }, 0, 0);
    return 0;
  }
#endif

  // The program: alive in a browser, written out natively.
  static int run(int argc, char **argv) {
#ifdef __EMSCRIPTEN__
    (void)argc;
    (void)argv;
    return live();
#else
    // Served live, where the framework has its live feature and the
    // program is asked to (`program serve [port]`, skiff.app.serve); else
    // the page written out.
    if (const std::optional<int> served = skiff::app::serve::run<App>(argc, argv)) return *served;
    return prerender(argc, argv);
#endif
  }
};

} // namespace skiff::app
