// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.window:toasts -- tdesktop's own notifications: a small window each, stacked from the screen's corner.
export module skiff.platform.window:toasts;

import std;
import sdl;
import splice.bytes;
import splice;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.platform.events;
import skiff.platform.fonts;
import skiff.platform.clipboard;
import skiff.platform.dialogs;
import :input;
import :canvas;

export namespace skiff::platform::window {
namespace detail {

// tdesktop's own notifications: a small window each, borderless, over the
// rest, stacked up from the screen's bottom right, gone after five seconds
// or when pressed -- the press opening the chat. Drawn in software: small,
// and no second GL context beside the window's.
template <class App>
class toasts {
 public:
  using chat_type = decltype(std::declval<typename App::toast_due>().chat);
  static constexpr int kWidth = 340, kHeight = 76, kMargin = 16, kGap = 8;
  toasts() = default;
  toasts(const toasts&) = delete;
  toasts& operator=(const toasts&) = delete;
  ~toasts() { shown_.clear(); }

  // In the program's colours, as the window is.
  void show(const typename App::toast_due& due, const auto& colours) {
    sdl::SDL_Window* made = sdl::SDL_CreateWindow("", kWidth, kHeight,
                                        sdl::kWindowBorderless | sdl::kWindowAlwaysOnTop | sdl::kWindowUtility |
                                            sdl::kWindowNotFocusable | sdl::kWindowHidden);
    if (!made)
      return;
    shown_.push_back(std::make_unique<one>(made, due, colours));
    // Three at most, as tdesktop keeps: the oldest goes.
    while (shown_.size() > 3)
      shown_.pop_front();
    this->place();
    sdl::SDL_ShowWindow(made);
  }
  [[nodiscard]] bool empty() const { return shown_.empty(); }
  // Whether an event is one of theirs; a press on one, the chat it is of.
  [[nodiscard]] bool owns(sdl::SDL_Window* window) const {
    return std::ranges::any_of(shown_, [&](const auto& each) { return each->window == window; });
  }
  // The notifications due, put up: the newest three at most -- a window made
  // for each only to be closed at once, where many came together, was the
  // whole of a frame.
  template <class Owner>
  void take(Owner& app) {
    auto due_now = app.take_toasts();
    for (std::size_t i = due_now.size() > 3 ? due_now.size() - 3 : 0; i < due_now.size(); ++i)
      this->show(due_now[i], app.colours);
  }
  // A press on one of them: what it is of, opened (open_notified); whether
  // it was one.
  template <class Owner>
  bool press(sdl::SDL_Window* window, Owner& app) {
    const auto chat = this->pressed(window);
    if (chat)
      app.open_notified(*chat);
    return chat.has_value();
  }
  [[nodiscard]] std::optional<chat_type> pressed(sdl::SDL_Window* window) {
    const auto found = std::ranges::find_if(shown_, [&](const auto& each) { return each->window == window; });
    if (found == shown_.end())
      return std::nullopt;
    chat_type chat = (*found)->chat;
    shown_.erase(found);
    this->place();
    return chat;
  }
  // Each frame: the old ones gone, the rest drawn.
  void frame(auto& paint) {
    const auto now = std::chrono::steady_clock::now();
    const auto before = shown_.size();
    std::erase_if(shown_, [&](const auto& each) { return each->until <= now; });
    if (shown_.size() != before)
      this->place();
    for (auto& each : shown_)
      each->draw(paint);
  }

 private:
  struct one {
    sdl::SDL_Window* window;
    canvas_target target;
    skiff::scene::Scene<typename App::toast_card> scene;
    chat_type chat;
    std::chrono::steady_clock::time_point until;
    one(sdl::SDL_Window* made, const typename App::toast_due& due, const auto& colours)
        : window(made), target(made, true), scene(std::in_place, App::make_toast(colours, due.key, due.title, due.text)), chat(due.chat),
          until(std::chrono::steady_clock::now() + std::chrono::seconds(5)) {}
    one(const one&) = delete;
    one& operator=(const one&) = delete;
    ~one() { sdl::SDL_DestroyWindow(window); }
    void draw(auto& paint) {
      const float scale = sdl::SDL_GetWindowDisplayScale(window);
      scene.layoutIfNeeded(skia::SkRect::MakeWH(static_cast<float>(kWidth), static_cast<float>(kHeight)));
      skia::SkSurface* surface = target.surface();
      if (!surface)
        return;
      skia::SkCanvas* canvas = surface->getCanvas();
      canvas->clear(skia::colorSetARGB(255, 24, 27, 30));
      canvas->save();
      canvas->scale(scale, scale);
      scene.draw(paint, canvas);
      canvas->restore();
      target.present();
    }
  };
  // Stacked from the bottom right of the screen's usable part, the newest
  // lowest, as tdesktop stacks them.
  void place() {
    sdl::SDL_Rect area{};
    if (!sdl::SDL_GetDisplayUsableBounds(sdl::SDL_GetPrimaryDisplay(), &area))
      return;
    int index = 0;
    for (auto it = shown_.rbegin(); it != shown_.rend(); ++it, ++index)
      sdl::SDL_SetWindowPosition((*it)->window, area.x + area.w - kMargin - kWidth,
                            area.y + area.h - kMargin - kHeight - index * (kHeight + kGap));
  }
  std::list<std::unique_ptr<one>> shown_;
};

// A program without notifications of its own (no toast_due): none shown,
// nothing asked of it.
class no_toasts {
 public:
  [[nodiscard]] bool empty() const { return true; }
  [[nodiscard]] bool owns(sdl::SDL_Window*) const { return false; }
  template <class Owner>
  void take(Owner&) {}
  template <class Owner>
  bool press(sdl::SDL_Window*, Owner&) { return false; }
  template <class Painting>
  void frame(Painting&) {}
};
template <class App>
struct toasts_for {
  using type = no_toasts;
};
template <class App>
  requires requires { typename App::toast_due; }
struct toasts_for<App> {
  using type = toasts<App>;
};
}  // namespace detail
}  // namespace skiff::platform::window
