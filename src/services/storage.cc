// SPDX-License-Identifier: AGPL-3.0-only
// Browser persistence shared by applications and the model's store keeper.
module;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
export module skiff.platform.storage;
import std;

#ifdef __EMSCRIPTEN__
// clang-format off
EM_JS_DEPS(skiff_storage, "$stringToNewUTF8,$UTF8ToString,$FS,$IDBFS");
EM_ASYNC_JS(char *, skiffStorageMount, (const char *root), {
  const path = UTF8ToString(root);
  try {
    FS.mkdirTree(path);
    FS.mount(IDBFS,{autoPersist:true},path);
    await new Promise((resolve,reject) => FS.syncfs(true,error => error ? reject(error):resolve()));
    return 0;
  } catch (error) {return stringToNewUTF8(String(error.message || error));}
});
EM_ASYNC_JS(char *, skiffStorageFlush, (), {
  try {
    await new Promise((resolve,reject) => FS.syncfs(false,error => error ? reject(error):resolve()));
    return 0;
  } catch (error) {return stringToNewUTF8(String(error.message || error));}
});
EM_JS(char *, skiffStorageRead, (const char *key), {
  try {
    const text = localStorage.getItem('skiff:' + UTF8ToString(key));
    return text === null ? 0 : stringToNewUTF8(text);
  } catch (error) {return 0;}
});
EM_JS(int, skiffStorageWrite, (const char *key, const char *text), {
  try {localStorage.setItem('skiff:' + UTF8ToString(key),UTF8ToString(text));return 1;}
  catch (error) {return 0;}
});
EM_JS(void, skiffStorageWritten, (const char *file), {
  const path = UTF8ToString(file), name = Module.skiffDownloads?.get(path);
  if (!name) return;
  const bytes = FS.readFile(path), url = URL.createObjectURL(new Blob([Uint8Array.from(bytes)]));
  const link = document.createElement('a');link.href = url;link.download = name;
  document.body.append(link);link.click();link.remove();
  Module.skiffDownloads.delete(path);FS.unlink(path);
  setTimeout(() => URL.revokeObjectURL(url),30000);
});
// clang-format on
#endif

export namespace skiff::platform::storage {
namespace detail {
inline std::optional<std::string> taken(char *text) {
  if (!text) return std::nullopt;
  std::string result(text);
  std::free(text);
  return result;
}
}
// Mounted before the application reads any files. The same filesystem API
// then serves account databases, encrypted stores and cached binary media.
inline std::optional<std::string> mount(const std::string &root) {
#ifdef __EMSCRIPTEN__
  return detail::taken(skiffStorageMount(root.c_str()));
#else
  std::error_code error;
  std::filesystem::create_directories(root, error);
  return error ? std::optional(error.message()) : std::nullopt;
#endif
}
inline std::optional<std::string> flush() {
#ifdef __EMSCRIPTEN__
  return detail::taken(skiffStorageFlush());
#else
  return std::nullopt;
#endif
}
inline std::optional<std::string> read(const std::string &key) {
#ifdef __EMSCRIPTEN__
  return detail::taken(skiffStorageRead(key.c_str()));
#else
  return std::nullopt;
#endif
}
inline bool write(const std::string &key, const std::string &text) {
#ifdef __EMSCRIPTEN__
  return skiffStorageWrite(key.c_str(), text.c_str()) != 0;
#else
  return false;
#endif
}
inline std::optional<std::string> read_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  std::string bytes;
  std::array<char, 8192> block;
  while (input.read(block.data(), static_cast<std::streamsize>(block.size())) || input.gcount() > 0)
    bytes.append(block.data(), static_cast<std::size_t>(input.gcount()));
  if (input.bad() || (!input.eof() && input.fail())) return std::nullopt;
  return bytes;
}
inline bool write_file(const std::string &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (output.fail()) return false;
#ifdef __EMSCRIPTEN__
  skiffStorageWritten(path.c_str());
#endif
  return true;
}

}
