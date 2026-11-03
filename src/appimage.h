#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace AppImageUtils {

inline bool isRunningInAppImage() { return std::getenv("APPDIR") != nullptr; }

inline std::filesystem::path getAppDir() {
  const char *appdir = std::getenv("APPDIR");
  if (appdir != nullptr) {
    return std::filesystem::path(appdir);
  }
  return std::filesystem::path();
}

inline std::filesystem::path getAppImagePath() {
  const char *appimage = std::getenv("APPIMAGE");
  if (appimage != nullptr) {
    return std::filesystem::path(appimage);
  }
  return std::filesystem::path();
}

inline std::filesystem::path
getAssetPath(const std::filesystem::path &relativePath) {
  if (isRunningInAppImage()) {
    auto appdir = getAppDir();

    auto fhsPath = appdir / "opt" / relativePath;
    if (std::filesystem::exists(fhsPath)) {
      return fhsPath;
    }

    return appdir / relativePath;
  } else {
    return std::filesystem::current_path() / relativePath;
  }
}

}
