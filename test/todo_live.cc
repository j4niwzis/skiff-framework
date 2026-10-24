// The site's todo list, served live with no script: the same views, the
// forms posted as the browser would post them -- a line typed and added, a
// row ticked, a row taken away -- and what the stream sends for each.
import std;
import skiff.app;

using namespace skiff;
using namespace skiff::ui;

namespace probe {
struct Draft {
  std::string text;
  friend bool operator==(const Draft &, const Draft &) = default;
};
struct Todo {
  int id = 0;
  std::string text;
  bool done = false;
  friend bool operator==(const Todo &, const Todo &) = default;
};
struct TodoList {
  skiff::model::Tracked<Draft> draft;
  skiff::model::Keyed<int, Todo> items;
};
struct AddTodo {
  std::string text;
};
struct RemoveTodo {
  int id = 0;
};
struct RemoveOf {
  [[nodiscard]] RemoveTodo operator()(const Todo &t) const { return RemoveTodo{t.id}; }
};
struct AddOf {
  [[nodiscard]] AddTodo operator()(const Draft &d) const { return AddTodo{d.text}; }
};
struct PutTodo {
  Todo todo;
  void operator()(model::Keyed<int, Todo> &list) const {
    if (!todo.text.empty()) list.put(todo.id, todo, std::numeric_limits<std::size_t>::max());
  }
};
auto todoRow(const Todo &) {
  Text label(std::string(), 17.0f, 0xff000000u, false);
  return row(hbox(10.0f), bound<model::Field<&Todo::done>>(ToggleField()),
             text_for<Todo>([](const Todo &t) { return t.text; }, std::move(label)),
             projected<Todo>(RemoveOf{}, SendButton<RemoveTodo>("x", RemoveTodo{})));
}
auto todoList() {
  return state<TodoList>(
      handlers(handle<AddTodo>([](const AddTodo &add) {
                 static int next = 100;
                 const int id = next++;
                 return std::tuple{model::over<model::Keyed<int, Todo>>(PutTodo{Todo{id, add.text, false}}),
                                   model::over<Draft>(model::setTo(Draft{}))};
               }),
               handle<RemoveTodo>([](const RemoveTodo &remove) { return model::take<Todo>(remove.id); })),
      column(vbox(12.0f),
             row(hbox(8.0f), bound<model::Field<&Draft::text>>(TextField<std::string>("Something to do")),
                 projected<Draft>(AddOf{}, SendButton<AddTodo>("Add", AddTodo{}))),
             each<int, Todo>([](const Todo &t) { return todoRow(t); }, vbox(8.0f))));
}
} // namespace probe

int failures = 0;
void check(bool ok, std::string_view what, std::string_view shown = {}) {
  std::println("{} {}", ok ? "ok  " : "FAIL", what);
  if (!ok) {
    ++failures;
    if (!shown.empty()) std::println("---\n{}\n---", shown);
  }
}
// The name a field is posted under, by what comes right after it in its tag
// (its placeholder, its row's text): the first one found before `near`.
std::string field_before(const std::string &html, std::string_view near) {
  const std::size_t at = html.find(near);
  if (at == std::string::npos) return {};
  const std::size_t name = html.rfind("name=\"v.", at);
  if (name == std::string::npos) return {};
  const std::size_t from = name + 6;
  return html.substr(from, html.find('"', from) - from);
}
// The name of the field whose own tag says `near` (its placeholder).
std::string field_in_tag(const std::string &html, std::string_view near) {
  const std::size_t at = html.find(near);
  if (at == std::string::npos) return {};
  const std::size_t start = html.rfind('<', at), end = html.find('>', at);
  const std::size_t name = html.find("name=\"v.", start);
  if (name == std::string::npos || name > end) return {};
  const std::size_t from = name + 6;
  return html.substr(from, html.find('"', from) - from);
}
std::string press_before(const std::string &html, std::string_view near) {
  const std::size_t at = html.find(near);
  const std::size_t value = html.rfind("name=\"e\" value=\"", at);
  if (at == std::string::npos || value == std::string::npos) return {};
  const std::size_t from = value + 16;
  return html.substr(from, html.find('"', from) - from);
}
std::string press_after(const std::string &html, std::string_view near) {
  const std::size_t at = html.find(near);
  const std::size_t value = html.find("name=\"e\" value=\"", at);
  if (at == std::string::npos || value == std::string::npos) return {};
  const std::size_t from = value + 16;
  return html.substr(from, html.find('"', from) - from);
}

int main() {
  using Tree = decltype(probe::todoList());
  skiff::ui::ModelOf<Tree> m;
  int id = 0;
  for (const char *text : {"Write the tree", "Bind it to the model", "Ship it as a page"}) {
    (void)m.apply(skiff::model::put<probe::Todo>(id, probe::Todo{id, text, id == 0}));
    ++id;
  }
  skiff::web::Live page(skiff::ui::mount(skiff::ui::onPage, probe::todoList()), m, {.script = false});
  std::string all = page.begin();
  check(all.contains(">Bind it to the model<"), "the list's rows sent", all);
  check(all.contains("placeholder=\"Something to do\""), "the line to type into", all);

  // Typed, and Add pressed: one form, its field and its button.
  const std::string line = field_in_tag(all, "placeholder=\"Something to do\"");
  const std::string add = press_after(all, "placeholder=\"Something to do\"");
  const std::string ticked = field_before(all, ">Write the tree<");  // the first row is done: its box posted ticked
  check(!line.empty() && !add.empty() && !ticked.empty(), "the form's names read off the page", all);
  page.post(line + "=milk&" + ticked + "=on&e=" + add);
  std::string next = page.update();
  all += next;
  check(next.contains(">milk<"), "added: its row sent", next);
  check(next.contains("order:3;"), "at the end of the list", next);
  check(!next.contains("skiff-live"), "no new generation", next);
  check(next.contains("placeholder=\"Something to do\"") && next.contains("value=\"\""), "the line emptied, sent again", next);
  check(m.look<probe::TodoList>()->items.fItems.size() == 4, "four things to do");

  // The second row ticked: posted with the first, still ticked.
  const std::string second = field_before(all, ">Bind it to the model<");
  page.post(ticked + "=on&" + second + "=on&e=");
  next = page.update();
  all += next;
  // Ticked in the model; the page shows it ticked already: nothing sent.
  const auto &items = m.look<probe::TodoList>()->items.fItems;
  const bool done = std::ranges::any_of(items, [](const auto &one) { return one.second->fValue.text == "Bind it to the model" && one.second->fValue.done; });
  check(done, "ticked: the model says it is done");
  check(!next.contains(">Bind it to the model<"), "and its row not sent again under the hand", next);

  // The third row taken away.
  const std::string remove = press_after(all, ">Ship it as a page<");
  page.post(ticked + "=on&" + second + "=on&e=" + remove);
  next = page.update();
  check(next.contains("{display:none!important}"), "taken away: its row hidden", next);
  check(m.look<probe::TodoList>()->items.fItems.size() == 3, "three left");
  return failures == 0 ? 0 : 1;
}
