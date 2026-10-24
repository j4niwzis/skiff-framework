# skiff-framework

[skiff](https://github.com/j4niwzis/skiff) and
[skiff-widgets](https://github.com/j4niwzis/skiff-widgets) as one library,
with what nearly every program written with them does done once:
`skiff.app`. A program is its tree, the parts of the model its views keep,
and what it adds; the rest -- mounting the tree, the frame, the events, the
canvas, writing the page out -- is here.

```cpp
// page.cc
import skiff.app;

struct Counter { int count = 0; };
struct Increment {};

auto page() {
  return state<Counter>(
      handlers(handle<Increment>([] {
        return model::over<Counter>([](Counter c) { ++c.count; return c; });
      })),
      column(vbox(12.0f),
          text_for<Counter>([](const Counter &c) { return std::format("Pressed {} times", c.count); },
                            Text("", 20.0f, 0xff1d2b36u)),
          SendButton<Increment>("+", Increment{})));
}

// main.cc
import skiff.app;
import myapp.page;
#include <skiff/app.h>
SKIFF_APP(myapp::page)
```

```cmake
add_executable(myapp src/main.cc)
target_link_libraries(myapp PRIVATE myapp_ui)
skiff_app(myapp SHELL web/index.html)
```

- **In a browser** (Emscripten) the tree is mounted on the page -- skiff.web,
  the browser's own elements, laid out by the browser -- over the copy written
  when the program was built, taken over rather than made again. Where the
  program says so (`Hooks::onCanvas`), the same tree is drawn on a `<canvas>`
  by skiff's scene instead -- laid out by skiff, drawn through its painter on
  the 2D context, repainted only where it changed -- one model under both.
- **Natively** the program writes the page out: whole, and into the page
  Emscripten made, in place of `@@SKIFF-PRERENDER@@`.

The model is the parts the tree's views keep (`state<T>`): `skiff::ui::ModelOf`.
What a program adds is a `Hooks` struct, every member optional -- its theme
(`dress`), its model as it begins (`start`), what moves at every frame
(`frame`), whether the canvas is shown (`onCanvas`) and outlines its repaints
(`outlineRepaints`), its stylesheet, the canvas's paper, a link followed on
the canvas (`open`), the `<head>` written out. See `src/app.cc`.

## With or without SSR

The framework is a frontend first: in a browser, `skiff.app` makes the page
itself where nothing was written before it, and a program that is only that
asks for nothing more -- no server, nothing written out, nothing of knot.

Server-side rendering is a feature, `ssr` (`cme_find_package(skiff-framework
COMPONENTS ssr)`, or `-DSKIFF_FRAMEWORK_SSR=ON`): the page written out with
the model it was written with, as JSON through
[knot](https://github.com/j4niwzis/knot) -- `<script type="application/json"
id="skiff-state">` -- and read back in the browser into the same model
before the page is taken over (`skiff.app.carry`). A model travels where
every part of it is something knot reads and writes (a type described with
`json_schema`, a string, a bool, an integer, an optional, a vector or a map of
them). In the page Emscripten makes, `@@SKIFF-STATE@@` says where it goes.

## With or without the canvas

A page is the browser's own elements. Drawing the whole tree with skiff's
scene on a canvas instead -- what the site does on its "scene" tab -- is a
feature, `canvas` (`cme_find_package(skiff-framework COMPONENTS canvas)`, or
`-DSKIFF_FRAMEWORK_CANVAS=ON`): `skiff.app.canvas`, shown where the program's
`onCanvas` says so, cleared to its `paper`. Without it, a frontend gets the
page alone: no canvas and none of its JavaScript, and skiff's scene and nodes
are not compiled at all (unless the native window needs them).

## Served live, with no script

The `live` feature (`COMPONENTS live`, or `-DSKIFF_FRAMEWORK_LIVE=ON`; natively, through Boost.Beast) adds `program serve [port]`.
The program serves its page itself (`skiff.app.serve`), keeping the tree and a model on the server for each browser.
The browser gets one HTML stream that is never closed and only ever appended to, with no JavaScript and no WebAssembly:

- **Generation.** The tree's containers are written into a Declarative Shadow DOM, with a `<slot>` for each leaf and for each list's rows.
- **Islands.** Each leaf, and each list row with everything in it, is an element sent to its slot. When one changes, it is sent again as a new version and a stylesheet hides the old one. A row that is removed is hidden; a new row is sent and placed by its `order`.
  Any other change (a container's own element, or a row inserted between others) is sent as a new generation, and the old generation is hidden.
- **Events.** What is done on the page comes back as a form posted into a hidden frame (the server answers 204):
  - a press is the submit button that was pressed (a clickable element that isn't a button is wrapped in one);
  - every field is posted with it, under its node, part and version.

  A text field's value goes with the next press, or with Enter.
- **Heartbeat.** A quiet stream is kept alive by one every 25 s.

Where the browser runs script, an optional inline script of a few lines improves the same page; `--no-script` leaves it out:
- presses are posted by `fetch`, so the field the user is in keeps its focus;
- what is typed is posted as it is typed;
- the field being typed in is taken as already shown, so it is never sent again under the user's hand.

**Partly on the server, partly in the browser** (with script only). A part of the tree said `client(view)` belongs to the browser:
- it has a model of its own, which the server's model doesn't hold;
- the server writes it once and never sends it again;
- the program built for the browser takes it over where the server wrote it, served with the page by `serve --client DIR` (`index.js`, `index.wasm`);
- from then on it runs there, interactive and drawn, while everything around it stays the server's.

Without script, the part stays as the server wrote it.

The machinery is `skiff::web::Live` and `skiff::web::Client` in skiff. A canvas of the page's own needs script, so it stays blank here.

## The window

`skiff.platform.window` (`src/platform/`): skiff's scene in a window of the
system's -- SDL3 makes it and brings its input; Skia draws the scene, on the
GPU through GL where there is a GL and in software into the window's own
pixels where there is not; the system's file dialogs and clipboard; an
Android activity (SDL's NativeActivity) as one more platform. A frame is drawn
only where the scene changed or still moves, and the thread otherwise sleeps.

```cpp
import skiff.platform.window;
// app: its scene (window()), and the hooks it has -- see the comment over
// skiff::platform::window::run.
return skiff::platform::window::run(app, {.title = "myapp"}, kinds);
```

Each platform is a cmake-everywhere **platform feature** (`linux`,
`windows`, `macos`, `android`; `wasm` for the page): on for the platform a
build is for, and naming only what that platform needs -- a build for Android
brings the window's Android pieces and SDL, one for the browser none of it.
Sources follow the same names as overlays: `foo_android.cc` replaces
`foo.cc` on Android, and only the platform's own enter the build.

A program that wants only the window (no `skiff.app`) configures it with
`SKIFF_FRAMEWORK_APP OFF`; the framework's own build (the site) has no
window unless `SKIFF_FRAMEWORK_WINDOW` is on. On Android the program declares
the SDL port it builds with -- the NativeActivity fork, with its glue.

## The site

[`site/`](site) is skiff's page, a program of `skiff.app`: `page.cc` is its
tree and its hooks, `main.cc` is `SKIFF_APP(site::page, site::Hooks)`.

```sh
emcmake cmake -S . -B build/wasm -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-fexceptions -mmutable-globals"
cmake --build build/wasm --target site
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target site
mkdir -p public && cp build/wasm/site/index.{html,js,wasm} public/
build/native/site/site public/static.html public/index.html
```

The workflow in `.github/workflows/pages.yml` does the same and hands
`public/` to GitHub Pages.

## Written in the same hand

[skiff](https://github.com/j4niwzis/skiff) (UI),
[skiff-widgets](https://github.com/j4niwzis/skiff-widgets) (controls),
[knot](https://github.com/j4niwzis/knot) (JSON),
[chevron](https://github.com/j4niwzis/chevron) (XML and HTML),
[alef](https://github.com/j4niwzis/alef) (Unicode).
