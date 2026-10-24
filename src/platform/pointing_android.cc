// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.pointing on Android: a finger from the start. SDL lists a
// phone's touch screen only once it has been touched, and the field given
// the focus before that put the keyboard up as the program opened.
export module skiff.platform.pointing;

export namespace skiff::platform::pointing {
[[nodiscard]] constexpr bool touch_first() { return true; }
}  // namespace skiff::platform::pointing
