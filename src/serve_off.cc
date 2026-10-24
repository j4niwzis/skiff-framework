// skiff.app.serve without the framework's live feature: nothing served, and
// nothing of Boost in the build. The program does what it does natively.
export module skiff.app.serve;

import std;

export namespace skiff::app::serve {
template <class A> [[nodiscard]] std::optional<int> run(int, char **) { return std::nullopt; }
} // namespace skiff::app::serve
