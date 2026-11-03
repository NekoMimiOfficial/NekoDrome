#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "platform/Platform.hpp"

namespace nt {

inline std::string urlEncode(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

inline std::string formatDuration(double sec) {
  int t = (int)std::max(0.0, sec);
  char buf[24];
  if (t >= 3600) std::snprintf(buf, sizeof buf, "%d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
  else std::snprintf(buf, sizeof buf, "%d:%02d", t / 60, t % 60);
  return buf;
}

inline std::string humanSize(long long bytes) {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double v = (double)bytes;
  int u = 0;
  while (v >= 1024.0 && u < 4) {
    v /= 1024.0;
    ++u;
  }
  char buf[32];
  if (u == 0) std::snprintf(buf, sizeof buf, "%lld B", bytes);
  else std::snprintf(buf, sizeof buf, "%.1f %s", v, units[u]);
  return buf;
}

inline long long nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

inline std::string randomHex(size_t n) {
  static std::mt19937_64 rng{std::random_device{}()};
  static const char* hex = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) s += hex[rng() & 15];
  return s;
}

inline std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

inline std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

inline std::string dataDir() { return platform::dataDir(); }

inline std::string cacheDir() { return platform::cacheDir(); }

inline void ensureDir(const std::string& p) { platform::makeDirs(p); }

inline bool fileExists(const std::string& p) { return platform::pathExists(p); }

inline long long fileSize(const std::string& p) { return platform::fileSize(p); }

inline bool removeFile(const std::string& p) { return platform::removeFile(p); }

inline bool removeTree(const std::string& p) { return platform::removeTree(p); }

inline bool renameFile(const std::string& a, const std::string& b) { return platform::renameFile(a, b); }

inline bool touchFile(const std::string& p) { return platform::touchFile(p); }

inline std::string parentDir(const std::string& p) {
  size_t i = p.find_last_of('/');
  return i == std::string::npos ? "" : p.substr(0, i);
}

inline std::string readFile(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

inline bool writeFile(const std::string& p, const std::string& data) {
  ensureDir(parentDir(p));
  std::string tmp = p + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) return false;
    f << data;
  }
  return renameFile(tmp, p);
}

inline long long dirSize(const std::string& p) {
  long long total = 0;
  for (const platform::DirEntry& e : platform::listDir(p)) total += e.isFile ? e.size : dirSize(e.path);
  return total;
}

}
