// SPDX-License-Identifier: AGPL-3.0-only
// skiff.app in a window: the program's tree, its model and its hooks, as the
// page and the canvas have them, shown by skiff.platform.window -- laid out
// and painted by skiff's scene, drawn with Skia, its input SDL's. The same
// tree as the page's: what the program says of its look is in the nodes'
// specs and themes, so a window and a page look alike.
//
// The program is
//
//   SKIFF_APP_WINDOW(myapp::page, myapp::Hooks)        // <skiff/app_window.h>
//
// and a build that has the window (SKIFF_FRAMEWORK_WINDOW) and the scene
// (SKIFF_FRAMEWORK_CANVAS) has it.
export module skiff.app.window;

import std;
import skiff.app;
import skiff.app.canvas;
import skiff.scene;
import skiff.bind;
import skiff.ui;
import skiff.platform.window;
import skiff.platform.events;

export namespace skiff::app::window {

// What window::run is given: the scene the tree is mounted on, rooted where
// the routing begins with the program's model and sink (canvas::Root), and
// the paint it is drawn with.
template <class A> struct Program {
  using SceneTree = skiff::app::canvas::Root<A, skiff::ui::Mounted<skiff::ui::OnScene, typename A::Tree>>;

  skiff::scene::Scene<SceneTree> scene{std::in_place, skiff::ui::mount(skiff::ui::onScene, A::view())};
  skiff::bind::Binding<typename A::Model> binding;
  skiff::scene::detail::PlainPaint paint;

  Program() { binding.refresh(scene.root(), A::model()); }

  skiff::scene::Scene<SceneTree> &window() { return scene; }
  skiff::scene::detail::PlainPaint &painting() { return paint; }

  // What a handler asked for and the model's change, shown: right after
  // each event, and between them before a frame.
  void after_event() { settle(); }
  void before_frame() { settle(); }

 private:
  // What was pressed and kept by its path -- where nothing was carried --
  // and what was answered and kept, delivered; then the tree read again
  // from the model.
  void settle() {
    auto &work = skiff::scene::hostWork();
    auto &root = scene.root();
    for (const auto &way : std::exchange(work.pressed, {}))
      (void)skiff::bind::press(root, A::model(), way, &A::sink());
    for (const auto &one : std::exchange(work.answers, {}))
      (void)skiff::bind::answer(root, A::model(), one, &A::sink());
    binding.refresh(root, A::model());
  }
};

// The program, until its window is closed: begun as the page begins it (its
// theme, its model as it starts), titled by the hooks where they say.
template <class A> int run(int, char **) {
  A::begin();
  skiff::platform::window::options how;
  if constexpr (requires { std::string(A::hooks().title()); })
    how.title = std::string(A::hooks().title());
  if constexpr (requires { A::hooks().window_size(); }) {
    const auto [width, height] = A::hooks().window_size();
    how.width = width;
    how.height = height;
  }
  Program<A> program;
  const skiff::platform::events::kinds kinds;
  return skiff::platform::window::run(program, how, kinds);
}

}  // namespace skiff::app::window
