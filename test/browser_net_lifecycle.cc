// SPDX-License-Identifier: AGPL-3.0-only
#include <emscripten/fetch.h>
import std;
import skiff.platform.net;

// Model the SDK's synchronous onerror callback during fetch_close. The
// browser fixture also exercises real HTTP, but this makes reentry explicit.
namespace {
emscripten_fetch_t *flight = nullptr;
unsigned closes = 0;
}
extern "C" {
void emscripten_fetch_attr_init(emscripten_fetch_attr_t *attr) { *attr = {}; }
emscripten_fetch_t *emscripten_fetch(emscripten_fetch_attr_t *attr, const char *) {
  flight = new emscripten_fetch_t{};
  flight->__attributes = *attr;
  flight->userData = attr->userData;
  flight->id = 1;
  flight->readyState = 1;
  return flight;
}
EMSCRIPTEN_RESULT emscripten_fetch_close(emscripten_fetch_t *fetch) {
  ++closes;
  if (fetch->readyState != 4 && fetch->__attributes.onerror) {
    fetch->status = static_cast<unsigned short>(-1);
    fetch->__attributes.onerror(fetch);
  }
  if (flight == fetch) flight = nullptr;
  delete fetch;
  return EMSCRIPTEN_RESULT_SUCCESS;
}
std::size_t emscripten_fetch_get_response_headers_length(emscripten_fetch_t *) { return 0; }
std::size_t emscripten_fetch_get_response_headers(emscripten_fetch_t *, char *text, std::size_t size) {
  if (size) *text = '\0';
  return 0;
}
}
int main() {
  namespace net = skiff::platform::net;
  net::browser browser;
  browser.start(1, {.url = "https://example.invalid/large", .max_body = 16});
  flight->readyState = 3;
  flight->status = 200;
  flight->totalBytes = 65536;
  flight->__attributes.onprogress(flight);
  auto oversized = browser.take(1);
  if (!oversized || oversized->status != 0 || oversized->body != "Response exceeded the size limit" || closes != 1) return 1;
  browser.start(2, {.url = "https://example.invalid/slow"});
  browser.abort(2);
  auto cancelled = browser.take(2);
  if (!cancelled || cancelled->status != 0 || !cancelled->cancelled || closes != 2) return 2;
  browser.start(3, {.url = "https://example.invalid/completed", .max_body = 16});
  flight->readyState = 4;
  flight->status = 200;
  flight->numBytes = 17;
  flight->__attributes.onsuccess(flight);
  auto completed = browser.take(3);
  if (!completed || completed->status != 0 || completed->body != "Response exceeded the size limit" || closes != 3) return 3;
  std::println("Browser fetch size limits and cancellation survive reentrant callbacks");
}
