// SPDX-License-Identifier: AGPL-3.0-only
module;
#include <emscripten.h>
#include <emscripten/fiber.h>
export module skiff.platform.fibers;
import std;

export namespace skiff::platform::fibers {
struct forced_unwind {};
class fiber;
namespace detail {
struct state {
  emscripten_fiber_t context{};
  std::vector<std::byte> stack, saved;
  std::weak_ptr<state> caller;
  bool finished = false, cancel = false;
  explicit state(std::size_t size) : stack(size), saved(size) {}
};
inline thread_local state *active = nullptr;
inline thread_local std::shared_ptr<state> root;
inline void current() {
  if (active) return;
  root = std::make_shared<state>(256 * 1024);
  emscripten_fiber_init_from_current_context(&root->context, root->saved.data(), root->saved.size());
  active = root.get();
}
}

// Emscripten's C entry point is the only erased boundary. Each fiber keeps
// its body's concrete type, and its shared owner destroys that type.
class fiber {
public:
  fiber() = default;
  explicit fiber(std::shared_ptr<detail::state> state) : state_(std::move(state)) {}
  fiber(const fiber &) = delete;
  fiber &operator=(const fiber &) = delete;
  fiber(fiber &&) noexcept = default;
  fiber &operator=(fiber &&other) noexcept {
    if (this != &other) { finish(); state_ = std::move(other.state_); }
    return *this;
  }
  ~fiber() { finish(); }
  fiber resume() && {
    detail::current();
    auto target = std::exchange(state_, {});
    if (!target || target->finished) return {};
    detail::state *from = detail::active;
    // A parent continuation must retain its original caller: swapping back
    // to the loop does not make the loop a child of the suspended fiber.
    if (target.get() != detail::root.get()) {
      target->caller = from == detail::root.get() ? detail::root : from->caller.lock();
    }
    detail::active = target.get();
    emscripten_fiber_swap(&from->context, &target->context);
    detail::active = from;
    if (from->cancel) throw forced_unwind{};
    return target->finished ? fiber{} : fiber(std::move(target));
  }
private:
  void finish() noexcept {
    if (!state_ || state_ == detail::root || state_->finished || state_.use_count() != 1) return;
    state_->cancel = true;
    try { auto stopped = std::move(*this).resume(); } catch (...) { std::terminate(); }
  }
  std::shared_ptr<detail::state> state_;
};

template <class Body> struct entry : detail::state {
  Body body;
  entry(Body value, std::size_t size) : detail::state(size), body(std::move(value)) {
    emscripten_fiber_init(&context, +[](void *opaque) {
      // The C API hands back the same concrete entry pointer.
      auto &self = *static_cast<entry *>(opaque);
      auto caller = self.caller.lock();
      try {
        fiber next = self.body(fiber(caller));
      } catch (const forced_unwind &) {
      }
      // This finished stack is released by its caller, so no owning local
      // may remain on it when we switch away permanently.
      auto *destination = caller.get();
      caller.reset();
      self.finished = true;
      detail::active = destination;
      emscripten_fiber_swap(&self.context, &destination->context);
      std::terminate();
    }, this, stack.data(), stack.size(), saved.data(), saved.size());
  }
};
template <class Body> fiber make(Body body, std::size_t stack) {
  detail::current();
  return fiber(std::make_shared<entry<Body>>(std::move(body), stack));
}
template <class Io> void run(Io &io) {
  // Fetch callbacks are delivered by the worker's JavaScript event loop.
  // A blocking io_context::run would prevent every response from arriving.
  while (!io.stopped()) { io.poll(); emscripten_sleep(1); }
}
}
