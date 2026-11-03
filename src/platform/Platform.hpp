#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nt::platform {

struct DirEntry {
  std::string path;
  long long size = 0;
  long long mtime = 0;
  bool isFile = false;
};

struct WindowConfig {
  int width = 1280;
  int height = 720;
  bool fullscreen = false;
  bool resizable = true;
  size_t imageBudgetBytes = 96u * 1024u * 1024u;
  int imageMaxDim = 1024;
};

struct UiProfile {
  bool reducedNav = false;
  bool hideHintsWithPlayer = false;
  int apiThreads = 3;
  int transferThreads = 2;
  int albumColumns = 5;
  float tileWidth = 200.0f;
  float tileGap = 16.0f;
  float nowPlayingCover = 400.0f;
  float nowPlayingGap = 44.0f;
  float nowPlayingPadX = 36.0f;
  float nowPlayingPadY = 16.0f;
};

class WorkerPool {
 public:
  WorkerPool();
  ~WorkerPool();
  WorkerPool(const WorkerPool&) = delete;
  WorkerPool& operator=(const WorkerPool&) = delete;

  void start(int threads);
  void push(std::function<void()> task);
  void stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct TextRequest {
  std::string title;
  std::string initial;
  bool password = false;
  int maxLength = 512;
};

struct TextResult {
  bool accepted = false;
  std::string text;
};

void init(int argc, char** argv);
void shutdown();
void pump();

std::string dataDir();
std::string cacheDir();
std::string assetPath(const std::string& relative);
std::string fontPath();
std::string defaultFontPath();
std::string glyphFontPath();
WindowConfig windowConfig();
const UiProfile& uiProfile();
void configureCurl(void* curl);

bool hasNativeTextInput();
bool beginTextInput(const TextRequest& request);
bool pollTextInput(TextResult& result);
void cancelTextInput();

bool makeDirs(const std::string& path);
bool pathExists(const std::string& path);
long long fileSize(const std::string& path);
bool removeFile(const std::string& path);
bool removeTree(const std::string& path);
bool renameFile(const std::string& from, const std::string& to);
bool touchFile(const std::string& path);
std::vector<DirEntry> listDir(const std::string& path);

}
