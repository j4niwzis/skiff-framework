// SPDX-License-Identifier: AGPL-3.0-only
// Browser file pickers return virtual paths; completed exports become downloads.
module;
#include <emscripten.h>
export module skiff.platform.dialogs;
import std;
import sdl;
import skiff.platform.events;

namespace skiff::platform::dialogs::detail {
struct mailbox {
  events::kinds kinds{0, 0, 0};
  std::vector<std::string> pending, saves, failures;
  std::vector<std::vector<std::string>> files;
};
inline unsigned next = 0;
inline std::map<unsigned, mailbox> boxes;
}
extern "C" EMSCRIPTEN_KEEPALIVE void skiff_browser_dialog(unsigned id, int action, const char *text) {
  using namespace skiff::platform;
  const auto found = dialogs::detail::boxes.find(id);
  if (found == dialogs::detail::boxes.end()) return;
  auto &box = found->second;
  switch (action) {
    case 1: box.pending.emplace_back(text); break;
    case 2:
      if (!box.pending.empty()) box.files.push_back(std::exchange(box.pending, {}));
      events::push(box.kinds.files);
      break;
    case 3: box.saves.emplace_back(text); events::push(box.kinds.save); break;
    case 4: box.pending.clear(); box.failures.emplace_back(text); events::push(box.kinds.files); break;
    default: break;
  }
}
// clang-format off
EM_JS(void, skiffBrowserChooseFiles, (unsigned id), {
  const input = document.createElement('input');input.type = 'file';input.multiple = true;input.hidden = true;
  input.oncancel = () => input.remove();
  input.onchange = async () => {
    try {
      FS.mkdirTree('/uploads');
      for (const file of input.files) {
        Module.skiffUploadSequence = (Module.skiffUploadSequence || 0)+1;
        const name = file.name.replace(/[\\/\x00-\x1f]/g,'_') || 'file';
        const path = '/uploads/'+id+'-'+Module.skiffUploadSequence+'-'+name;
        FS.writeFile(path,new Uint8Array(await file.arrayBuffer()));
        Module.ccall('skiff_browser_dialog',null,['number','number','string'],[id,1,path]);
      }
      Module.ccall('skiff_browser_dialog',null,['number','number','string'],[id,2,'']);
    } catch (error) {Module.ccall('skiff_browser_dialog',null,['number','number','string'],[id,4,String(error.message || error)]);}
    finally {input.remove();}
  };
  document.body.append(input);input.click();
});
EM_JS(void, skiffBrowserSavePath, (unsigned id, const char *offered), {
  try {
    const name = UTF8ToString(offered).split(/[\\/]/).pop().replace(/[\x00-\x1f]/g,'_') || 'download';
    FS.mkdirTree('/exports');
    Module.skiffExportSequence = (Module.skiffExportSequence || 0)+1;
    const path = '/exports/'+Module.skiffExportSequence+'-'+name;
    Module.skiffDownloads ||= new Map();Module.skiffDownloads.set(path,name);
    Module.ccall('skiff_browser_dialog',null,['number','number','string'],[id,3,path]);
  } catch (error) {Module.ccall('skiff_browser_dialog',null,['number','number','string'],[id,4,String(error.message || error)]);}
});
// clang-format on
export namespace skiff::platform::dialogs {
class dialogs {
public:
  dialogs() : id_(++detail::next) { detail::boxes.try_emplace(id_); }
  dialogs(const dialogs &) = delete;
  dialogs &operator=(const dialogs &) = delete;
  ~dialogs() { detail::boxes.erase(id_); }
  void over(sdl::SDL_Window *, const events::kinds &kinds) { detail::boxes.at(id_).kinds = kinds; }
  void choose_files() { skiffBrowserChooseFiles(id_); }
  void choose_save_path(std::string offered) { skiffBrowserSavePath(id_, offered.c_str()); }
  std::vector<std::vector<std::string>> take_files() { return std::exchange(detail::boxes.at(id_).files, {}); }
  std::vector<std::string> take_save_paths() { return std::exchange(detail::boxes.at(id_).saves, {}); }
  std::vector<std::string> take_failures() { return std::exchange(detail::boxes.at(id_).failures, {}); }
private:
  unsigned id_;
};
}
