// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.dialogs -- The system's dialogs for opening files and for
// saving one.
export module skiff.platform.dialogs;

import std;
import sdl;
import skiff.platform.events;

export namespace skiff::platform::dialogs {

// The dialogs of a window: the program's, put over the window as it is made.
// What they answer -- on a thread of SDL's choosing -- is kept here, and the
// window told with an event of its own; it takes the answers on its thread.
class dialogs {
 private:
  // A callback may arrive after the window and its dialogs owner are gone.
  // It only reaches this shared mailbox, never the dialogs object itself.
  struct mailbox {
    events::kinds kinds{0, 0, 0};
    std::mutex lock;
    std::vector<std::vector<std::string>> files;
    std::vector<std::string> save_paths, failures;

    void chose(const char* const* list) {
      auto paths = std::views::iota(std::size_t{0}) | std::views::take_while([list](std::size_t at) { return list[at] != nullptr; }) | std::views::transform([list](std::size_t at) { return std::string(list[at]); }) | std::ranges::to<std::vector>();
      if (paths.empty()) return;
      std::uint32_t kind;
      {
        const std::lock_guard held(lock);
        files.push_back(std::move(paths));
        kind = kinds.files;
      }
      events::push(kind);
    }
    void failed(const char* why) {
      std::uint32_t kind;
      {
        const std::lock_guard held(lock);
        failures.emplace_back(why != nullptr && *why != '\0' ? why : "no reason given");
        kind = kinds.files;
      }
      events::push(kind);
    }
    void saved(const char* path) {
      std::uint32_t kind;
      {
        const std::lock_guard held(lock);
        save_paths.emplace_back(path);
        kind = kinds.save;
      }
      events::push(kind);
    }
  };
  struct pending {
    std::weak_ptr<mailbox> box;
    // Each outstanding save dialog owns its suggested name independently.
    std::string offered;
  };

 public:
  dialogs() = default;
  dialogs(const dialogs&) = delete;
  dialogs& operator=(const dialogs&) = delete;

  void over(sdl::SDL_Window* window, const events::kinds& kinds) {
    parent_ = window;
    const std::lock_guard held(box_->lock);
    box_->kinds = kinds;
  }
  void choose_files() {
    auto answer = std::make_unique<pending>(pending{box_, {}});
    sdl::SDL_ShowOpenFileDialog(
        +[](void* self, const char* const* list, int) {
          const std::unique_ptr<pending> answer(static_cast<pending*>(self));
          if (auto box = answer->box.lock()) {
            if (list) box->chose(list);
            else box->failed(sdl::SDL_GetError());
          }
        },
        answer.release(), parent_, nullptr, 0, nullptr, true);
  }
  void choose_save_path(std::string offered) {
    auto answer = std::make_unique<pending>(pending{box_, std::move(offered)});
    const char* name = answer->offered.empty() ? nullptr : answer->offered.c_str();
    sdl::SDL_ShowSaveFileDialog(
        +[](void* self, const char* const* list, int) {
          const std::unique_ptr<pending> answer(static_cast<pending*>(self));
          if (auto box = answer->box.lock()) {
            if (list && *list) box->saved(*list);
            else if (!list) box->failed(sdl::SDL_GetError());
          }
        },
        answer.release(), parent_, nullptr, 0, name);
  }
  [[nodiscard]] std::vector<std::vector<std::string>> take_files() {
    const std::lock_guard held(box_->lock);
    return std::exchange(box_->files, {});
  }
  [[nodiscard]] std::vector<std::string> take_save_paths() {
    const std::lock_guard held(box_->lock);
    return std::exchange(box_->save_paths, {});
  }
  [[nodiscard]] std::vector<std::string> take_failures() {
    const std::lock_guard held(box_->lock);
    return std::exchange(box_->failures, {});
  }

 private:
  sdl::SDL_Window* parent_ = nullptr;
  std::shared_ptr<mailbox> box_ = std::make_shared<mailbox>();
};

}  // namespace skiff::platform::dialogs
