#include "Platform.hpp"

#include <curl/curl.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/rtc.h>
#include <psp2/sysmodule.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>

extern "C" {
int _newlib_heap_size_user = 160 * 1024 * 1024;
}

namespace nt::platform {

namespace {

const char* const kDataDir = "ux0:/data/nekodrome";
const char* const kCustomFont = "ux0:/data/nekodrome/font.ttf";
const char* const kFontDir = "app0:/font/";
const char* const kAssetDir = "app0:/assets/";

char g_netPool[1024 * 1024];
bool g_netUp = false;

enum class Ime { Idle, Requested, Running, Aborting };
Ime g_ime = Ime::Idle;
TextRequest g_request;
SceWChar16 g_title[SCE_IME_DIALOG_MAX_TITLE_LENGTH + 1];
SceWChar16 g_initial[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
SceWChar16 g_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];

void utf8ToUtf16(const std::string& in, SceWChar16* out, size_t capacity) {
  size_t n = 0;
  size_t i = 0;
  while (i < in.size() && n + 2 < capacity) {
    unsigned char c = (unsigned char)in[i];
    uint32_t cp = 0;
    int extra = 0;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      extra = 1;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      extra = 2;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      extra = 3;
    }
    ++i;
    for (int k = 0; k < extra && i < in.size(); ++k, ++i) cp = (cp << 6) | ((unsigned char)in[i] & 0x3F);
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out[n++] = (SceWChar16)(0xD800 + (cp >> 10));
      out[n++] = (SceWChar16)(0xDC00 + (cp & 0x3FF));
    } else {
      out[n++] = (SceWChar16)cp;
    }
  }
  out[n] = 0;
}

std::string utf16ToUtf8(const SceWChar16* in) {
  std::string out;
  for (; *in; ++in) {
    uint32_t cp = *in;
    if (cp >= 0xD800 && cp < 0xDC00 && in[1] >= 0xDC00 && in[1] < 0xE000) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (uint32_t)(in[1] - 0xDC00);
      ++in;
    }
    if (cp < 0x80) {
      out += (char)cp;
    } else if (cp < 0x800) {
      out += (char)(0xC0 | (cp >> 6));
      out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += (char)(0xE0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    } else {
      out += (char)(0xF0 | (cp >> 18));
      out += (char)(0x80 | ((cp >> 12) & 0x3F));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }
  }
  return out;
}

bool startDialog() {
  size_t maxLen = (size_t)std::max(1, std::min(g_request.maxLength, (int)SCE_IME_DIALOG_MAX_TEXT_LENGTH));
  utf8ToUtf16(g_request.title, g_title, SCE_IME_DIALOG_MAX_TITLE_LENGTH + 1);
  utf8ToUtf16(g_request.initial, g_initial, maxLen + 1);
  std::memset(g_buffer, 0, sizeof g_buffer);

  SceImeDialogParam p;
  sceImeDialogParamInit(&p);
  p.supportedLanguages = 0x0001FFFF;
  p.languagesForced = SCE_FALSE;
  p.type = SCE_IME_TYPE_DEFAULT;
  p.option = SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
  p.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
  p.textBoxMode = g_request.password ? SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD : SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
  p.title = g_title;
  p.maxTextLength = (SceUInt32)maxLen;
  p.initialText = g_initial;
  p.inputTextBuffer = g_buffer;
  return sceImeDialogInit(&p) >= 0;
}

bool isDirMode(SceMode m) { return SCE_S_ISDIR(m); }

long long toSeconds(const SceDateTime& dt) {
  time_t t = 0;
  if (sceRtcGetTime_t(&dt, &t) < 0) return 0;
  return (long long)t;
}

}

void init(int, char**) {
  sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
  SceNetInitParam np;
  np.memory = g_netPool;
  np.size = sizeof g_netPool;
  np.flags = 0;
  if (sceNetInit(&np) >= 0) {
    sceNetCtlInit();
    g_netUp = true;
  }
  makeDirs(kDataDir);
}

void shutdown() {
  if (g_ime == Ime::Running) {
    sceImeDialogAbort();
    sceImeDialogTerm();
  }
  g_ime = Ime::Idle;
  if (g_netUp) {
    sceNetCtlTerm();
    sceNetTerm();
    g_netUp = false;
  }
}

void pump() {
  if (g_ime != Ime::Aborting) return;
  SceCommonDialogStatus st = sceImeDialogGetStatus();
  if (st == SCE_COMMON_DIALOG_STATUS_FINISHED) sceImeDialogTerm();
  if (st != SCE_COMMON_DIALOG_STATUS_RUNNING) g_ime = Ime::Idle;
}

std::string dataDir() { return kDataDir; }

std::string cacheDir() { return std::string(kDataDir) + "/cache"; }

std::string assetPath(const std::string& relative) { return kAssetDir + relative; }

std::string fontPath() {
  if (pathExists(kCustomFont)) return kCustomFont;
  return defaultFontPath();
}

std::string defaultFontPath() { return std::string(kFontDir) + "font.ttf"; }

std::string glyphFontPath() { return std::string(kFontDir) + "MaterialIcons-Regular.ttf"; }

WindowConfig windowConfig() {
  WindowConfig w;
  w.width = 960;
  w.height = 544;
  w.fullscreen = false;
  w.resizable = true;
  w.imageBudgetBytes = 32u * 1024u * 1024u;
  w.imageMaxDim = 512;
  return w;
}

const UiProfile& uiProfile() {
  static const UiProfile p = [] {
    UiProfile u;
    u.reducedNav = true;
    u.hideHintsWithPlayer = true;
    u.apiThreads = 1;
    u.transferThreads = 1;
    u.albumColumns = 4;
    u.tileWidth = 188.0f;
    u.tileGap = 12.0f;
    u.nowPlayingCover = 320.0f;
    u.nowPlayingGap = 28.0f;
    u.nowPlayingPadX = 24.0f;
    u.nowPlayingPadY = 12.0f;
    return u;
  }();
  return p;
}

struct WorkerPool::Impl {
  std::vector<SceUID> threads;
  std::deque<std::function<void()>> queue;
  std::mutex mutex;
  bool quit = false;

  static int entry(SceSize, void* argp) {
    Impl* self = *static_cast<Impl**>(argp);
    for (;;) {
      std::function<void()> fn;
      {
        std::lock_guard<std::mutex> lock(self->mutex);
        if (!self->queue.empty()) {
          fn = std::move(self->queue.front());
          self->queue.pop_front();
        } else if (self->quit) {
          return 0;
        }
      }
      if (fn) fn();
      else sceKernelDelayThread(2000);
    }
  }
};

WorkerPool::WorkerPool() : impl_(new Impl) {}

WorkerPool::~WorkerPool() { stop(); }

void WorkerPool::start(int threads) {
  for (int i = 0; i < threads; ++i) {
    SceUID id = sceKernelCreateThread("NekoPoolWorker", (SceKernelThreadEntry)Impl::entry, 160, 64 * 1024, 0, 0, nullptr);
    if (id < 0) continue;
    impl_->threads.push_back(id);
    Impl* self = impl_.get();
    sceKernelStartThread(id, sizeof(Impl*), &self);
  }
}

void WorkerPool::push(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->queue.push_back(std::move(task));
}

void WorkerPool::stop() {
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->quit = true;
  }
  for (SceUID id : impl_->threads) {
    sceKernelWaitThreadEnd(id, nullptr, nullptr);
    sceKernelDeleteThread(id);
  }
  impl_->threads.clear();
}

void configureCurl(void* handle) {
  CURL* c = static_cast<CURL*>(handle);
  curl_easy_setopt(c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
}

bool hasNativeTextInput() { return true; }

bool beginTextInput(const TextRequest& request) {
  pump();
  if (g_ime != Ime::Idle) return false;
  g_request = request;
  g_ime = Ime::Requested;
  return true;
}

bool pollTextInput(TextResult& result) {
  if (g_ime == Ime::Requested) {
    if (startDialog()) {
      g_ime = Ime::Running;
    } else {
      g_ime = Ime::Idle;
      result = TextResult{};
      return true;
    }
  }
  if (g_ime != Ime::Running) return false;
  SceCommonDialogStatus st = sceImeDialogGetStatus();
  if (st == SCE_COMMON_DIALOG_STATUS_RUNNING) return false;
  result = TextResult{};
  if (st == SCE_COMMON_DIALOG_STATUS_FINISHED) {
    SceImeDialogResult r;
    std::memset(&r, 0, sizeof r);
    sceImeDialogGetResult(&r);
    if (r.button == SCE_IME_DIALOG_BUTTON_ENTER) {
      result.accepted = true;
      result.text = utf16ToUtf8(g_buffer);
    }
    sceImeDialogTerm();
  }
  g_ime = Ime::Idle;
  return true;
}

void cancelTextInput() {
  if (g_ime == Ime::Requested) {
    g_ime = Ime::Idle;
  } else if (g_ime == Ime::Running) {
    sceImeDialogAbort();
    g_ime = Ime::Aborting;
  }
}

bool makeDirs(const std::string& path) {
  for (size_t i = 1; i <= path.size(); ++i) {
    if (i != path.size() && path[i] != '/') continue;
    std::string prefix = path.substr(0, i);
    if (prefix.empty() || prefix.back() == ':' || prefix.back() == '/') continue;
    sceIoMkdir(prefix.c_str(), 0777);
  }
  SceIoStat st;
  return sceIoGetstat(path.c_str(), &st) >= 0 && isDirMode(st.st_mode);
}

bool pathExists(const std::string& path) {
  SceIoStat st;
  return sceIoGetstat(path.c_str(), &st) >= 0;
}

long long fileSize(const std::string& path) {
  SceIoStat st;
  if (sceIoGetstat(path.c_str(), &st) < 0) return 0;
  return (long long)st.st_size;
}

bool removeFile(const std::string& path) { return sceIoRemove(path.c_str()) >= 0; }

bool removeTree(const std::string& path) {
  for (const DirEntry& e : listDir(path)) {
    if (e.isFile) removeFile(e.path);
    else removeTree(e.path);
  }
  return sceIoRmdir(path.c_str()) >= 0;
}

bool renameFile(const std::string& from, const std::string& to) {
  if (pathExists(to)) sceIoRemove(to.c_str());
  return sceIoRename(from.c_str(), to.c_str()) >= 0;
}

bool touchFile(const std::string& path) {
  SceIoStat st;
  std::memset(&st, 0, sizeof st);
  sceRtcGetCurrentClockLocalTime(&st.st_mtime);
  return sceIoChstat(path.c_str(), &st, SCE_CST_MT) >= 0;
}

std::vector<DirEntry> listDir(const std::string& path) {
  std::vector<DirEntry> out;
  SceUID d = sceIoDopen(path.c_str());
  if (d < 0) return out;
  SceIoDirent ent;
  std::memset(&ent, 0, sizeof ent);
  while (sceIoDread(d, &ent) > 0) {
    std::string name = ent.d_name;
    if (name == "." || name == "..") {
      std::memset(&ent, 0, sizeof ent);
      continue;
    }
    DirEntry e;
    e.path = path + (path.empty() || path.back() == '/' ? "" : "/") + name;
    e.isFile = SCE_S_ISREG(ent.d_stat.st_mode);
    e.size = e.isFile ? (long long)ent.d_stat.st_size : 0;
    e.mtime = toSeconds(ent.d_stat.st_mtime);
    out.push_back(std::move(e));
    std::memset(&ent, 0, sizeof ent);
  }
  sceIoDclose(d);
  return out;
}

}
