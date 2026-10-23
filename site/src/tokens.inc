// C++ as an editor colours it, for either backend's Code: the code cut into
// runs of a kind -- "kw", "name", "str", "num", "com", or "" for the rest.
[[nodiscard]] inline std::vector<std::pair<std::string_view, std::string_view>> codeTokens(std::string_view code) {
  static constexpr std::array<std::string_view, 24> kKeywords{
      "auto", "return", "const", "constexpr", "struct", "class", "template", "typename", "using", "namespace", "if", "else",
      "for", "while", "static", "inline", "void", "bool", "int", "float", "true", "false", "import", "this"};
  std::vector<std::pair<std::string_view, std::string_view>> out;
  const auto add = [&](std::string_view kind, std::size_t from, std::size_t to) {
    if (to <= from) return;
    if (kind.empty() && !out.empty() && out.back().first.empty())
      out.back().second = code.substr(out.back().second.data() - code.data(), to - (out.back().second.data() - code.data()));
    else
      out.emplace_back(kind, code.substr(from, to - from));
  };
  const auto word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
  std::size_t i = 0;
  while (i < code.size()) {
    const char c = code[i];
    if (code.substr(i, 2) == "//") {
      const std::size_t end = std::min(code.find('\n', i), code.size());
      add("com", i, end);
      i = end;
    } else if (c == '"' || c == '\'') {
      std::size_t end = i + 1;
      while (end < code.size() && code[end] != c) end += code[end] == '\\' ? 2 : 1;
      end = std::min(end + 1, code.size());
      add("str", i, end);
      i = end;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t end = i;
      while (end < code.size() && (word(code[end]) || code[end] == '.')) ++end;
      add("num", i, end);
      i = end;
    } else if (word(c)) {
      std::size_t end = i;
      while (end < code.size() && word(code[end])) ++end;
      const std::string_view w = code.substr(i, end - i);
      std::size_t next = end;
      while (next < code.size() && code[next] == ' ') ++next;
      const bool called = next < code.size() && (code[next] == '(' || code[next] == '<');
      add(std::ranges::contains(kKeywords, w) ? "kw" : (called || std::isupper(static_cast<unsigned char>(w[0]))) ? "name" : "", i, end);
      i = end;
    } else {
      add("", i, i + 1);
      ++i;
    }
  }
  return out;
}
