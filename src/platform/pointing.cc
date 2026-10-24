// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.pointing -- What the platform is pointed at with first,
// before any press says: a mouse, or a finger (pointing_android.cc).
export module skiff.platform.pointing;

export namespace skiff::platform::pointing {
// A finger from the start: true where the platform is one held in the hand,
// whatever the devices SDL has found yet.
[[nodiscard]] constexpr bool touch_first() { return false; }
}  // namespace skiff::platform::pointing
