// SPDX-License-Identifier: AGPL-3.0-only
import std;
import skiff.platform.fibers;

int main() {
  namespace fibers = skiff::platform::fibers;
  int state = 0;
  auto task = fibers::make([&](fibers::fiber back) {
    state = 1;
    back = std::move(back).resume();
    state = 2;
    return back;
  }, 256 * 1024);
  task = std::move(task).resume();
  if (state != 1) return 1;
  task = std::move(task).resume();
  if (state != 2 || fibers::detail::root.use_count() != 1) return 2;
  bool unwound = false;
  {
    auto cancelled = fibers::make([&](fibers::fiber back) {
      struct cleanup { bool &done; ~cleanup() { done = true; } } guard{unwound};
      back = std::move(back).resume();
      return back;
    }, 256 * 1024);
    cancelled = std::move(cancelled).resume();
  }
  if (!unwound || fibers::detail::root.use_count() != 1) return 3;
  return 0;
}
