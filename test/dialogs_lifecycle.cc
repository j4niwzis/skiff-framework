// SPDX-License-Identifier: AGPL-3.0-only
#include <SDL3/SDL.h>
import std;
import skiff.platform.dialogs;
import skiff.platform.events;

namespace {
struct pending_dialog {
  SDL_DialogFileCallback callback;
  void* userdata;
  const char* offered;
};
std::vector<pending_dialog> pending;
std::vector<std::uint32_t> pushed;
}
extern "C" {
Uint32 SDL_RegisterEvents(int) { return 100; }
bool SDL_PushEvent(SDL_Event* event) { pushed.push_back(event->type); return true; }
const char* SDL_GetError() { return "picker failed"; }
void SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window*,
                            const SDL_DialogFileFilter*, int, const char*, bool) {
  pending.push_back({callback, userdata, nullptr});
}
void SDL_ShowSaveFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window*,
                            const SDL_DialogFileFilter*, int, const char* offered) {
  pending.push_back({callback, userdata, offered});
}
}
int main() {
  namespace platform = skiff::platform;
  const platform::events::kinds kinds{10, 11, 12};
  const char* selected[] = {"/picked/photo.png", nullptr};
  const char* cancelled[] = {nullptr};
  {
    auto owner = std::make_unique<platform::dialogs::dialogs>();
    owner->over(nullptr, kinds);
    owner->choose_files();
    owner->choose_save_path(std::string(1024, 'x'));
    owner.reset();
    pending[0].callback(pending[0].userdata, selected, 0);
    pending[1].callback(pending[1].userdata, nullptr, 0);
    pending.clear();
    if (!pushed.empty()) return 1;
  }
  platform::dialogs::dialogs owner;
  owner.over(nullptr, kinds);
  owner.choose_files();
  pending[0].callback(pending[0].userdata, selected, 0);
  pending.clear();
  if (owner.take_files() != std::vector<std::vector<std::string>>{{"/picked/photo.png"}}) return 2;
  if (!owner.take_files().empty() || pushed != std::vector<std::uint32_t>{11}) return 3;
  pushed.clear();
  owner.choose_files();
  pending[0].callback(pending[0].userdata, cancelled, 0);
  pending.clear();
  if (!owner.take_files().empty() || !pushed.empty()) return 4;
  owner.choose_files();
  pending[0].callback(pending[0].userdata, nullptr, 0);
  pending.clear();
  if (owner.take_failures() != std::vector<std::string>{"picker failed"}) return 5;
  const std::string first(1024, 'a'), second(2048, 'b');
  owner.choose_save_path(first);
  owner.choose_save_path(second);
  if (std::string(pending[0].offered) != first || std::string(pending[1].offered) != second) return 6;
  for (const auto& answer : pending) answer.callback(answer.userdata, selected, 0);
  if (owner.take_save_paths() != std::vector<std::string>{selected[0], selected[0]}) return 7;
  std::puts("Late file dialog callbacks and independent save names passed");
}
