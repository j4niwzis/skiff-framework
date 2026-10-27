// SPDX-License-Identifier: AGPL-3.0-only
module;
#include <emscripten.h>
export module skiff.platform.clipboard:write;
import std;
// clang-format off
EM_JS(void, skiffBrowserCopyPicture, (const char *data, size_t size), {
  if (!navigator.clipboard?.write || typeof ClipboardItem === 'undefined') return;
  const blob = new Blob([HEAPU8.slice(data,data+size)],{type:'image/png'});
  navigator.clipboard?.write([new ClipboardItem({'image/png':blob})]).catch(error => console.warn('Clipboard:',error));
});
EM_JS(void, skiffBrowserCopyText, (const char *plain, const char *rich), {
  const text = UTF8ToString(plain), html = UTF8ToString(rich);
  if (html && navigator.clipboard?.write && typeof ClipboardItem !== 'undefined') {
    navigator.clipboard.write([new ClipboardItem({'text/plain':new Blob([text],{type:'text/plain'}),
      'text/html':new Blob([html],{type:'text/html'})})]).catch(error => console.warn('Clipboard:',error));
  } else navigator.clipboard?.writeText(text).catch(() => {});
});
// clang-format on
export namespace skiff::platform::clipboard {
inline void copy_picture(std::string png) { skiffBrowserCopyPicture(png.data(), png.size()); }
inline void copy_text(std::string text, std::string html) { skiffBrowserCopyText(text.c_str(), html.c_str()); }
}
