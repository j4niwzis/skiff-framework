// skiff.app.carry, with the ssr feature: the model a page was written with
// travels with it, through knot. Written into the page as JSON --
// <script type="application/json" id="skiff-state">, "</" in it written
// "<\\/" so that nothing in it ends the script -- and read back in the
// browser into the same model before the page is taken over.
//
// Only where every part of the model is something knot reads and writes
// (TransferableRoot: a described type -- json_schema -- a string, a bool, an
// integer, and an optional, a vector or a map of them, tracked or not):
// another model travels not, and starts as start() makes it.
export module skiff.app.carry;

import std;
import knot;
import skiff.model;

// skiff.model's own types, as knot reads and writes them: found by
// argument-dependent lookup, so that skiff.model needs nothing of knot. A
// tracked value is its value and its revision; the parts of a model, each in
// its place.
export namespace skiff::model {
template <class T> consteval auto json_schema(knot::type<Tracked<T>>) {
  return knot::schema<Tracked<T>>().template member<"fValue">(knot::key("value")).template member<"fRevision">(knot::key("revision"));
}
template <class... P> consteval auto json_schema(knot::type<Assembled<P...>>) { return knot::schema<Assembled<P...>>(); }
} // namespace skiff::model

export namespace skiff::app::carry {
template <class T> struct Transferable : std::bool_constant<knot::described<T> || knot::transparent<T>> {};
template <> struct Transferable<std::string> : std::true_type {};
template <> struct Transferable<bool> : std::true_type {};
template <class T>
  requires std::integral<T>
struct Transferable<T> : std::true_type {};
template <class T> struct Transferable<std::optional<T>> : Transferable<T> {};
template <class T> struct Transferable<std::vector<T>> : Transferable<T> {};
template <class T> struct Transferable<std::map<std::string, T>> : Transferable<T> {};
template <class T> struct Transferable<skiff::model::Tracked<T>> : Transferable<T> {};
template <class... P> struct Transferable<skiff::model::Assembled<P...>> : std::bool_constant<(Transferable<P>::value && ...)> {};
template <class Root>
concept TransferableRoot = Transferable<Root>::value;

// What is written into the page beside its markup: the model's parts, where
// they travel.
template <class Root> std::string script(const Root &) { return {}; }
template <TransferableRoot Root> std::string script(const Root &root) {
  using namespace std::string_view_literals;
  return "<script type=\"application/json\" id=\"skiff-state\">" +
         (knot::to_json_string(root) | std::views::split("</"sv) | std::views::join_with("<\\/"sv) | std::ranges::to<std::string>()) +
         "</script>";
}
// The model's parts, read back from what the page carried: where they
// travel and were read whole.
template <class Root> std::optional<Root> read(std::string_view) { return std::nullopt; }
template <TransferableRoot Root> std::optional<Root> read(std::string_view text) {
  auto read = knot::try_read<Root>(text);
  if (!read) return std::nullopt;
  return std::move(*read);
}
} // namespace skiff::app::carry
