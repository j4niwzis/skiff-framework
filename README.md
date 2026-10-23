# skiff-framework

What an application on [skiff](https://github.com/j4niwzis/skiff) needs
besides its UI: the network, its data as C++ structures, a router, storage --
and how such a program is built, tested and served. skiff stays a library of
the UI; this is where the rest of an application goes.

```cpp
import skiff.ui;

auto counter() {
  return column(vbox(12.0f),
      bound<Counter>(label<Counter>([](const Counter &c) {
        return std::format("Pressed {} times", c.count);
      })),
      row(hbox(8.0f),
          SendButton<Decrement>("−", Decrement{}),
          SendButton<Increment>("+", Increment{})));
}
```

## The site

[`site/`](site) is skiff's page, written with skiff: one tree against
`skiff.ui` on `skiff.web`. It is built twice from the one source --

- natively, a program that mounts the page on a document kept in memory and
  writes it out: `static.html`, with no script at all, and the same markup put
  into the page Emscripten made, which a browser shows before anything has
  loaded;
- under Emscripten, the page itself, which takes over from that copy and
  runs: its examples are the code beside them.

```sh
emcmake cmake -S site -B build/wasm -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-fexceptions -mmutable-globals"
cmake --build build/wasm --target site
cmake -S site -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target site
mkdir -p public && cp build/wasm/index.{html,js,wasm} public/
build/native/site public/static.html public/index.html
```

The workflow in `.github/workflows/pages.yml` does the same and hands
`public/` to GitHub Pages.

## Written in the same hand

[skiff](https://github.com/j4niwzis/skiff) (UI),
[skiff-widgets](https://github.com/j4niwzis/skiff-widgets) (controls),
[knot](https://github.com/j4niwzis/knot) (JSON),
[chevron](https://github.com/j4niwzis/chevron) (XML and HTML),
[alef](https://github.com/j4niwzis/alef) (Unicode).
