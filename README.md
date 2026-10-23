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
