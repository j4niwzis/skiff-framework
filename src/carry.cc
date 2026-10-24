// skiff.app.carry, without the ssr feature: nothing travels with a page --
// no state written into it, none read back. A program that is a page in the
// browser alone needs nothing of knot.
export module skiff.app.carry;

import std;

export namespace skiff::app::carry {
// What is written into the page beside its markup: nothing.
template <class Root> std::string script(const Root &) { return {}; }
// The model's parts, read back from what the page carried: none.
template <class Root> std::optional<Root> read(std::string_view) { return std::nullopt; }
} // namespace skiff::app::carry
