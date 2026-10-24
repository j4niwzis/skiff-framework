// The state a page carries (skiff.app): a model's parts written as JSON by
// knot into the page's <script>, and read back into the same parts -- the
// value and its revision, every part in its place; "</" in a value never
// ending the script.
import std;
import knot;
import skiff.app;

namespace probe {
struct Counter {
  int count = 0;
  friend bool operator==(const Counter &, const Counter &) = default;
};
consteval auto json_schema(knot::type<Counter>) { return knot::schema<Counter>(); }
struct Note {
  std::string text;
  std::vector<std::string> tags;
  std::optional<bool> pinned;
  friend bool operator==(const Note &, const Note &) = default;
};
consteval auto json_schema(knot::type<Note>) { return knot::schema<Note>(); }
// Not knot's: a model with it does not travel.
struct Opaque {
  double seconds = 0.0;
};
} // namespace probe

using Root = skiff::model::Assembled<skiff::model::Tracked<probe::Counter>, skiff::model::Tracked<probe::Note>>;
static_assert(skiff::app::TransferableRoot<Root>);
static_assert(!skiff::app::TransferableRoot<skiff::model::Assembled<skiff::model::Tracked<probe::Opaque>>>);

int main() {
  Root root;
  root.part = {.fValue = {.count = 41}, .fRevision = 3};
  root.rest.part = {.fValue = {.text = "a </script> in it", .tags = {"x", "y"}, .pinned = true}, .fRevision = 7};
  const std::string script = skiff::app::detail::stateScript(root);
  std::println("{}", script);
  const std::string_view open = R"(<script type="application/json" id="skiff-state">)";
  if (!script.starts_with(open) || !script.ends_with("</script>")) {
    std::println(std::cerr, "not a script");
    return 1;
  }
  const std::string json = script.substr(open.size(), script.size() - open.size() - std::string_view("</script>").size());
  if (json.contains("</")) {
    std::println(std::cerr, "a </ left in the state");
    return 1;
  }
  auto back = knot::try_read<Root>(std::string_view(json));
  if (!back) {
    std::println(std::cerr, "not read back");
    return 1;
  }
  const bool same = back->part.fValue == root.part.fValue && back->part.fRevision == 3 &&
                    back->rest.part.fValue == root.rest.part.fValue && back->rest.part.fRevision == 7;
  std::println("read back: {}", same ? "the same" : "different");
  return same ? 0 : 1;
}
