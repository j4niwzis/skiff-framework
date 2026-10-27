// SPDX-License-Identifier: AGPL-3.0-only
// Shared platform HTTP types and the browser transport.
module;
#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif
export module skiff.platform.net;
import std;
export namespace skiff::platform::net {
// How a request asks: read off it once, where it is written.
namespace method {
struct get {};
struct post {};
struct put {};
struct patch {};
struct remove {};
struct head {};
struct options {};
} // namespace method
using method_t = std::variant<method::get, method::post, method::put, method::patch, method::remove, method::head, method::options>;
[[nodiscard]] constexpr std::string_view name(method::get) { return "GET"; }
[[nodiscard]] constexpr std::string_view name(method::post) { return "POST"; }
[[nodiscard]] constexpr std::string_view name(method::put) { return "PUT"; }
[[nodiscard]] constexpr std::string_view name(method::patch) { return "PATCH"; }
[[nodiscard]] constexpr std::string_view name(method::remove) { return "DELETE"; }
[[nodiscard]] constexpr std::string_view name(method::head) { return "HEAD"; }
[[nodiscard]] constexpr std::string_view name(method::options) { return "OPTIONS"; }
[[nodiscard]] inline std::string_view name(const method_t &m) {
  return std::visit([](auto one) { return name(one); }, m);
}
[[nodiscard]] inline std::optional<method_t> method_of(std::string_view text) {
  using entry = std::pair<std::string_view, method_t>;
  const std::array names{entry{"GET", method::get{}}, entry{"POST", method::post{}}, entry{"PUT", method::put{}},
                         entry{"PATCH", method::patch{}}, entry{"DELETE", method::remove{}},
                         entry{"HEAD", method::head{}}, entry{"OPTIONS", method::options{}}};
  const auto found = std::ranges::find(names, text, &entry::first);
  return found == names.end() ? std::nullopt : std::optional(found->second);
}

// What is sent: where (http:// or https://), how, what with, and how long
// it is waited for, each time it is tried.
struct request {
  method_t method = method::get{};
  std::string url;
  std::string body;
  std::string type = "application/json";
  std::vector<std::pair<std::string, std::string>> headers;
  std::chrono::milliseconds timeout{30000};
  std::size_t max_body = 64 * 1024 * 1024;
};
// What came back of one try: the status (0: the server was not reached, or
// not in time), the body -- or why it failed -- and what Retry-After said.
struct outcome {
  int status = 0;
  std::string body;
  std::optional<std::chrono::milliseconds> after;
  std::optional<std::string> location;
  bool cancelled = false;
};

struct progress {
  std::size_t read = 0;
  std::optional<std::size_t> total;
};

#ifdef __EMSCRIPTEN__
// Each request owns its binary body, headers and C callback state until
// taken or cancelled. Callers drain data; no program callbacks are kept.
class browser {
public:
  browser() = default;
  browser(const browser &) = delete;
  browser &operator=(const browser &) = delete;
  browser(browser &&) = default;
  browser &operator=(browser &&) = default;
  void start(std::uint64_t ticket, const request &asked) {
    auto owned = std::make_unique<transfer>();
    owned->asked = asked;
    auto &state = *owned;
    for (const auto &[name, value] : state.asked.headers) {
      if (name.find_first_of("\r\n") != std::string::npos || value.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("Invalid HTTP header");
      state.headers.push_back(name.c_str()); state.headers.push_back(value.c_str());
    }
    if (!state.asked.body.empty()) {
      state.headers.push_back("Content-Type"); state.headers.push_back(state.asked.type.c_str());
    }
    state.headers.push_back(nullptr);
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    const auto method = name(asked.method);
    std::ranges::copy(method, attr.requestMethod);
    attr.requestMethod[method.size()] = '\0';
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.requestHeaders = state.headers.data();
    attr.requestData = state.asked.body.data();
    attr.requestDataSize = state.asked.body.size();
    attr.timeoutMSecs = static_cast<unsigned long>(std::clamp<std::int64_t>(asked.timeout.count(), 1, 2147483647));
    attr.userData = &state;
    // The platform's C API requires callbacks. Everything beyond that
    // boundary is typed request/outcome data, drained by the caller.
    attr.onsuccess = attr.onerror = +[](emscripten_fetch_t *fetch) {
      auto &state = *static_cast<transfer *>(fetch->userData);
      // Closing an in-progress fetch invokes onerror synchronously. Preserve
      // the terminal size-limit or cancellation outcome that caused close.
      if (state.result) return;
      outcome result;
      result.status = fetch->status <= 599 ? fetch->status : 0;
      if (fetch->numBytes > state.asked.max_body) {
        result.status = 0; result.body = "Response exceeded the size limit";
      } else if (fetch->data) result.body.assign(fetch->data, static_cast<std::size_t>(fetch->numBytes));
      if (result.status == 0 && result.body.empty()) result.body = "Browser request failed (network, TLS, or server CORS policy)";
      const auto size = emscripten_fetch_get_response_headers_length(fetch);
      std::string headers(size + 1, '\0');
      emscripten_fetch_get_response_headers(fetch, headers.data(), headers.size());
      headers.resize(size);
      read_headers(result, headers);
      state.result = std::move(result);
    };
    attr.onprogress = +[](emscripten_fetch_t *fetch) {
      auto &state = *static_cast<transfer *>(fetch->userData);
      if (state.result) return;
      state.bytes = {.read = static_cast<std::size_t>(fetch->dataOffset + fetch->numBytes),
                     .total = fetch->totalBytes > 0 ? std::optional<std::size_t>(fetch->totalBytes) : std::nullopt};
      if (state.bytes.read > state.asked.max_body || (state.bytes.total && *state.bytes.total > state.asked.max_body)) {
        state.result = outcome{.status = 0, .body = "Response exceeded the size limit"};
        state.fetch = nullptr;
        emscripten_fetch_close(fetch);
      }
    };
    state.fetch = emscripten_fetch(&attr, state.asked.url.c_str());
    if (!state.fetch) state.result = outcome{.status = 0, .body = "Could not start browser request"};
    flights_.insert_or_assign(ticket, std::move(owned));
  }
  std::optional<outcome> take(std::uint64_t ticket) {
    const auto found = flights_.find(ticket);
    if (found == flights_.end() || !found->second->result) return std::nullopt;
    auto result = std::move(found->second->result);
    flights_.erase(found);
    return result;
  }
  progress transferred(std::uint64_t ticket) const {
    const auto found = flights_.find(ticket);
    return found == flights_.end() ? progress{} : found->second->bytes;
  }
  void abort(std::uint64_t ticket) {
    const auto found = flights_.find(ticket);
    if (found == flights_.end()) return;
    auto &state = *found->second;
    state.result = outcome{.status = 0, .body = "Request cancelled", .cancelled = true};
    if (auto *fetch = std::exchange(state.fetch, nullptr)) emscripten_fetch_close(fetch);
  }
private:
  struct transfer {
    request asked;
    std::vector<const char *> headers;
    emscripten_fetch_t *fetch = nullptr;
    progress bytes;
    std::optional<outcome> result;
    ~transfer() { if (fetch) emscripten_fetch_close(fetch); }
  };
  enum class header { other, retry_after, location };
  static void read_headers(outcome &into, std::string_view lines) {
    using named_header = std::pair<std::string_view, header>;
    constexpr std::array names{named_header{"retry-after", header::retry_after}, named_header{"location", header::location}};
    for (auto line : lines | std::views::split('\n')) {
      const std::string_view text(line.begin(), line.end());
      const auto colon = text.find(':');
      if (colon == std::string_view::npos) continue;
      const auto key = text.substr(0, colon) | std::views::transform([](unsigned char c) { return static_cast<char>(std::tolower(c)); })
                                           | std::ranges::to<std::string>();
      auto value = text.substr(colon + 1);
      while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
      while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) value.remove_suffix(1);
      const auto found = std::ranges::find(names, key, &named_header::first);
      switch (found == names.end() ? header::other : found->second) {
        case header::retry_after: {
          std::int64_t seconds = 0;
          const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), seconds);
          if (error == std::errc{} && end == value.data() + value.size() && seconds >= 0 && seconds <= 2147483647)
            into.after = std::chrono::milliseconds(seconds * 1000);
          break;
        }
        case header::location: into.location = std::string(value); break;
        case header::other: break;
      }
    }
  }
  std::map<std::uint64_t, std::unique_ptr<transfer>> flights_;
};
#endif
}
