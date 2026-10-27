// SPDX-License-Identifier: AGPL-3.0-only
// skiff.platform.clipboard -- Pictures put on the system's clipboard, and
// taken from it.
export module skiff.platform.clipboard;
export import :write;

import std;
import sdl;
import chevron.escape;

export namespace skiff::platform::clipboard {

template <class Fragment> std::string emoji_html(const Fragment& fragment) {
  std::string html;
  const auto text = [&](std::string_view part) {
    for (std::size_t at = 0; at < part.size();) {
      const auto end = std::min(part.find('\n', at), part.size());
      html += chevron::escaped(part.substr(at, end - at));
      if (end < part.size()) html += "<br>";
      at = end + 1;
    }
  };
  std::size_t at = 0;
  for (const auto& atom : fragment.atoms) {
    text(std::string_view(fragment.display).substr(at, atom.first - at));
    if (atom.picture)
      html += std::format(R"(<img data-mx-emoticon src="{}" alt="{}">)", chevron::escaped(atom.target), chevron::escaped(atom.plain));
    else text(atom.plain);
    at = atom.last;
  }
  text(std::string_view(fragment.display).substr(at));
  return html;
}

// A picture on the clipboard, for Ctrl+V: its bytes put in a file of its
// own, to go where a picture dropped on the window goes. The first of the
// kinds a picture comes as that the clipboard has.
struct picture_kind {
  const char* mime;
  const char* extension;
};
inline constexpr std::array kPictureKinds{
    picture_kind{"image/png", "png"},   picture_kind{"image/jpeg", "jpg"}, picture_kind{"image/gif", "gif"},
    picture_kind{"image/webp", "webp"}, picture_kind{"image/bmp", "bmp"},
};
// A file:// URI's path, its %XX escapes read: what a file manager puts on
// the clipboard (text/uri-list) as it copies files. Nothing for another
// scheme.
[[nodiscard]] inline std::optional<std::string> path_of_uri(std::string_view uri) {
  constexpr std::string_view scheme = "file://";
  if (!uri.starts_with(scheme))
    return std::nullopt;
  uri.remove_prefix(scheme.size());
  // file://host/path: the path from its first slash.
  if (const auto slash = uri.find('/'); slash != std::string_view::npos)
    uri.remove_prefix(slash);
  std::string out;
  for (std::size_t at = 0; at < uri.size(); ++at) {
    unsigned value = 0;
    if (uri[at] == '%' && at + 2 < uri.size() &&
        std::from_chars(uri.data() + at + 1, uri.data() + at + 3, value, 16).ec == std::errc{}) {
      out.push_back(static_cast<char>(value));
      at += 2;
    } else {
      out.push_back(uri[at]);
    }
  }
  return out;
}

// What pastes pictures: each in a file of its own, numbered -- or the
// picture files a file manager copied, where they are.
class paster {
 public:
  paster() = default;
  paster(const paster&) = delete;
  paster& operator=(const paster&) = delete;
  // The folder gone with the program: what was pasted was read as it was
  // given (files_given), and is kept by what took it.
  ~paster() {
    if (folder_) {
      std::error_code failed;
      std::filesystem::remove_all(*folder_, failed);
    }
  }
  [[nodiscard]] std::vector<std::string> pictures() {
    // What SDL was told the clipboard offers (on X11, its owner's answer to
    // TARGETS): said with what went wrong where a picture was among it and
    // none was pasted.
    std::size_t count = 0;
    char** offered = sdl::SDL_GetClipboardMimeTypes(&count);
    const std::vector<std::string> mimes =
        offered ? std::views::counted(offered, static_cast<std::ptrdiff_t>(count)) | std::views::transform([](const char* one) { return std::string(one); }) | std::ranges::to<std::vector>()
                : std::vector<std::string>{};
    sdl::SDL_free(offered);
    // Each kind fetched straight away (SDL_GetClipboardData): nothing where
    // the clipboard does not offer it. Asked first with
    // SDL_HasClipboardData, a picture crossed the X server twice -- X11's
    // HasClipboardData is a whole GetClipboardData, thrown away.
    // A picture itself, in one of the kinds read here.
    for (const picture_kind& kind : kPictureKinds)
      if (auto made = this->picture_of(kind, std::ranges::contains(mimes, std::string_view(kind.mime))))
        return {std::move(*made)};
    // Files a file manager copied: those that are pictures.
    if (std::vector<std::string> files = this->uris(); !files.empty())
      return files;
    // Said at every Ctrl+V that pasted no picture, text ones too: a picture
    // copied in another program pasted nothing and said nothing, the list
    // holding text as well.
    std::println(std::cerr, "[paste] nothing on the clipboard read as a picture; it offers: {}{}", mimes,
                 mimes.empty() ? " (SDL was told of nothing)" : "");
    return {};
  }

 private:
  // A kind's bytes, in a file of their own. Where the clipboard said it has
  // the kind, what failed is said: SDL's fetch (on X11, "Selection timeout"
  // where the owner took more than a second, after which SDL takes the
  // clipboard over itself), or the file.
  std::optional<std::string> picture_of(const picture_kind& kind, bool listed) {
    std::size_t size = 0;
    sdl::SDL_ClearError();
    void* data = sdl::SDL_GetClipboardData(kind.mime, &size);
    if (!data) {
      if (listed)
        std::println(std::cerr, "[paste] {} listed, but SDL gave nothing for it: {}", kind.mime, sdl::SDL_GetError());
      return std::nullopt;
    }
    const std::filesystem::path* folder = this->folder();
    if (!folder) {
      sdl::SDL_free(data);
      std::println(std::cerr, "[paste] no folder of its own for what is pasted could be made in the temporary folder");
      return std::nullopt;
    }
    const std::filesystem::path path = *folder / std::format("pasted-{}.{}", ++counter_, kind.extension);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    sdl::SDL_free(data);
    if (!out) {
      std::println(std::cerr, "[paste] {} bytes of {} not written to {}: {}", size, kind.mime, path.string(),
                   out.rdbuf()->is_open() ? "the write failed" : "the file did not open");
      return std::nullopt;
    }
    return path.string();
  }
  // The copied files' paths, each a picture by its name's extension.
  std::vector<std::string> uris() {
    std::size_t size = 0;
    void* data = sdl::SDL_GetClipboardData("text/uri-list", &size);
    if (!data)
      return {};
    const std::string list(static_cast<const char*>(data), size);
    sdl::SDL_free(data);
    static constexpr std::array kExtensions{".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp"};
    return list | std::views::split('\n') | std::views::transform([](auto line) {
             std::string_view one(line.begin(), line.end());
             if (one.ends_with('\r'))
               one.remove_suffix(1);
             return one.starts_with('#') ? std::optional<std::string>() : path_of_uri(one);
           }) | std::views::filter([](const std::optional<std::string>& path) {
             if (!path)
               return false;
             std::string ext = std::filesystem::path(*path).extension().string();
             std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
             return std::ranges::contains(kExtensions, std::string_view(ext));
           }) | std::views::transform([](const std::optional<std::string>& path) { return *path; }) | std::ranges::to<std::vector>();
  }

  // A folder of this program's own, made new under the temporary one: a
  // shared /tmp/mux-pasted could be another's -- left by another user, or
  // another run under another account -- and nothing could be written into
  // it (the user's: "810463 bytes of image/png not written to
  // /tmp/mux-pasted/pasted-1.png"). Made the first time a picture is pasted;
  // its name drawn at random until one is new.
  const std::filesystem::path* folder() {
    if (folder_)
      return &*folder_;
    std::error_code failed;
    const std::filesystem::path under = std::filesystem::temp_directory_path(failed);
    if (failed)
      return nullptr;
    std::random_device seed;
    for (int tries = 0; tries < 8; ++tries)
      if (std::filesystem::path made = under / std::format("mux-pasted-{:08x}", seed());
          std::filesystem::create_directory(made, failed)) {
        folder_ = std::move(made);
        return &*folder_;
      }
    return nullptr;
  }

  std::optional<std::filesystem::path> folder_;
  unsigned counter_ = 0;
};

}  // namespace skiff::platform::clipboard
