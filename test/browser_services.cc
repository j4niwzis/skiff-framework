// SPDX-License-Identifier: AGPL-3.0-only
#include <emscripten.h>
import std;
import skiff.platform.net;
import skiff.platform.storage;
#ifdef SKIFF_TEST_IMAGES
import skia;
#endif

EM_JS(char *, checkBase, (), {return stringToNewUTF8(new URL('./',location.href).href);});
EM_JS(void, checkDone, (int round), {document.body.dataset.servicesRound = String(round);});
EM_JS(void, checkFailed, (const char *why), {document.body.dataset.servicesError = UTF8ToString(why);});

void require(bool condition, const char *why) { if (!condition) throw std::runtime_error(why); }
int main() {
  namespace net = skiff::platform::net;
  namespace storage = skiff::platform::storage;
  try {
    char *baseText = checkBase();
    const std::string base(baseText);
    std::free(baseText);
    net::browser browser;
    const auto take = [&](std::uint64_t id) {
      const double deadline = emscripten_get_now() + 10000;
      while (emscripten_get_now() < deadline) {
        if (auto value = browser.take(id)) return *std::move(value);
        emscripten_sleep(5);
      }
      throw std::runtime_error("Browser HTTP request did not finish");
    };
    const std::string binary{"a\0b\xff", 4};
    browser.start(1, {.method = net::method::post{}, .url = base + "echo", .body = binary,
      .type = "application/octet-stream", .headers = {{"Authorization", "Bearer fixture"}}});
    const auto echo = take(1);
    require(echo.status == 200 && echo.body == binary, "HTTP changed binary request or response bytes");
    require(echo.after == std::chrono::seconds(2) && echo.location == "/next", "Response headers were lost");
    browser.start(2, {.url = base + "missing"});
    require(take(2).status == 404, "HTTP errors lost their status");
    browser.start(3, {.url = base + "slow"});
    browser.abort(3);
    require(take(3).cancelled, "Cancelled request was not returned as cancelled");
    browser.start(4, {.url = base + "large", .max_body = 16});
    const auto large = take(4);
    require(large.status == 0 && large.body == "Response exceeded the size limit", "Oversized response was accepted");
#ifdef SKIFF_TEST_IMAGES
    const std::array<std::uint8_t, 8> pixels{255, 0, 0, 255, 0, 255, 0, 255};
    const auto image = skia::imageFromRGBA(2, 1, pixels.data());
    const auto png = skia::encodeImage(*image, false);
    require(!png.empty(), "Browser PNG encoding failed");
    const auto decoded = skia::decodeImage(png.data(), png.size());
    require(decoded && decoded->width() == 2 && decoded->height() == 1 && decoded->getColor(0, 0) == 0xffff0000u
      && decoded->getColor(1, 0) == 0xff00ff00u, "Browser PNG round trip changed pixels");
    const auto thumbnail = skia::decodeImageAtMost(png.data(), png.size(), 1);
    require(thumbnail && thumbnail->width() == 1 && thumbnail->height() == 1, "Browser thumbnail was not bounded");
    const auto jpeg = skia::encodeImage(*image, true);
    require(jpeg.size() > 2 && static_cast<unsigned char>(jpeg[0]) == 0xffu && static_cast<unsigned char>(jpeg[1]) == 0xd8u,
      "Browser JPEG encoding failed");
#endif
    if (const auto error = storage::mount("/checks")) throw std::runtime_error(*error);
    const auto previous = storage::read_file("/checks/persisted");
    if (previous) require(*previous == binary, "IndexedDB changed persisted binary bytes");
    require(storage::write_file("/checks/persisted", binary), "Could not write a browser file");
    if (const auto error = storage::flush()) throw std::runtime_error(*error);
    checkDone(previous ? 2 : 1);
    return 0;
  } catch (const std::exception &error) {
    checkFailed(error.what());
    return 1;
  }
}
