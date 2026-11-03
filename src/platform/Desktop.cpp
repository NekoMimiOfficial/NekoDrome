#include "Platform.hpp"
#include "appimage.h"

#include <SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;
namespace ai = AppImageUtils;

namespace nt::platform {

namespace {

std::string baseDir() {
  char* bp = SDL_GetBasePath();
  std::string d = bp ? bp : "./";
  if (bp) SDL_free(bp);
  return d;
}

std::string homeBase() {
  if (const char* o = std::getenv("NEKOTUNE_HOME")) return o;
  return "";
}

long long toSeconds(fs::file_time_type t) {
  auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
  return std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count();
}

}

void init(int, char**) {}

void shutdown() {}

void pump() {}

std::string dataDir() {
  std::string b = homeBase();
  if (!b.empty()) return b + "/data";
  if (const char* x = std::getenv("XDG_DATA_HOME")) return std::string(x) + "/neko-tune";
  const char* h = std::getenv("HOME");
  return std::string(h ? h : ".") + "/.local/share/neko-tune";
}

std::string cacheDir() {
  std::string b = homeBase();
  if (!b.empty()) return b + "/cache";
  if (const char* x = std::getenv("XDG_CACHE_HOME")) return std::string(x) + "/neko-tune";
  const char* h = std::getenv("HOME");
  return std::string(h ? h : ".") + "/.cache/neko-tune";
}

std::string assetPath(const std::string& relative) {
  if (ai::isRunningInAppImage()) {
    return ai::getAssetPath("usr/share/nekodrome/"+relative);
  }

  fs::path optAsset = fs::path("/opt/nekodrome") / relative;
  if (fs::exists(optAsset)) {
    return optAsset.string();
  }

  return (fs::path(baseDir()) / "data" / relative).string();
}

std::string fontPath() {
  std::string custom = dataDir() + "/font.ttf";
  if (pathExists(custom)) return custom;
  return defaultFontPath();
}

std::string defaultFontPath() { return assetPath("fonts/font.ttf"); }

std::string glyphFontPath() { return assetPath("fonts/MaterialIcons-Regular.ttf"); }

WindowConfig windowConfig() { return WindowConfig{}; }

const UiProfile& uiProfile() {
  static const UiProfile p;
  return p;
}

struct WorkerPool::Impl {
  std::vector<std::thread> threads;
  std::deque<std::function<void()>> queue;
  std::mutex mutex;
  std::condition_variable cv;
  bool quit = false;

  void loop() {
    for (;;) {
      std::function<void()> fn;
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return quit || !queue.empty(); });
        if (quit) return;
        fn = std::move(queue.front());
        queue.pop_front();
      }
      fn();
    }
  }
};

WorkerPool::WorkerPool() : impl_(new Impl) {}

WorkerPool::~WorkerPool() { stop(); }

void WorkerPool::start(int threads) {
  for (int i = 0; i < threads; ++i) impl_->threads.emplace_back([p = impl_.get()] { p->loop(); });
}

void WorkerPool::push(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->queue.push_back(std::move(task));
  }
  impl_->cv.notify_one();
}

void WorkerPool::stop() {
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->quit = true;
  }
  impl_->cv.notify_all();
  for (std::thread& t : impl_->threads)
    if (t.joinable()) t.join();
  impl_->threads.clear();
}

void configureCurl(void*) {}

bool hasNativeTextInput() { return false; }

bool beginTextInput(const TextRequest&) { return false; }

bool pollTextInput(TextResult&) { return false; }

void cancelTextInput() {}

bool makeDirs(const std::string& path) {
  std::error_code ec;
  fs::create_directories(path, ec);
  return !ec;
}

bool pathExists(const std::string& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

long long fileSize(const std::string& path) {
  std::error_code ec;
  auto n = fs::file_size(path, ec);
  return ec ? 0 : (long long)n;
}

bool removeFile(const std::string& path) {
  std::error_code ec;
  return fs::remove(path, ec);
}

bool removeTree(const std::string& path) {
  std::error_code ec;
  fs::remove_all(path, ec);
  return !ec;
}

bool renameFile(const std::string& from, const std::string& to) {
  std::error_code ec;
  fs::rename(from, to, ec);
  return !ec;
}

bool touchFile(const std::string& path) {
  std::error_code ec;
  fs::last_write_time(path, fs::file_time_type::clock::now(), ec);
  return !ec;
}

std::vector<DirEntry> listDir(const std::string& path) {
  std::vector<DirEntry> out;
  std::error_code ec;
  for (auto& e : fs::directory_iterator(path, ec)) {
    DirEntry d;
    d.path = e.path().string();
    d.isFile = e.is_regular_file(ec);
    d.size = d.isFile ? (long long)e.file_size(ec) : 0;
    d.mtime = toSeconds(e.last_write_time(ec));
    out.push_back(std::move(d));
  }
  return out;
}

}
