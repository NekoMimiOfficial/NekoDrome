#pragma once

#include <algorithm>
#include <vector>

#include "Subsonic.hpp"
#include "Util.hpp"

namespace nt {

struct Config {
  Account account;
  bool scrobble = true;
  int streamKbps = 320;
  float volume = 0.8f;
  bool dark = true;
  int palette = 0;
  bool offlineForced = false;
  bool insecure = false;
};

struct OfflineEntry {
  Song song;
  std::string file;
};

struct PendingScrobble {
  std::string id;
  long long timeMs = 0;
};

struct SavedQueue {
  std::vector<Song> songs;
  int index = -1;
  bool shuffle = false;
  int repeat = 0;
};

class Store {
 public:
  Config cfg;

  std::string offlineDir() const { return dataDir() + "/offline"; }
  std::string coverDir() const { return dataDir() + "/covers"; }
  std::string trackCacheDir() const { return cacheDir() + "/tracks"; }

  void load() {
    ensureDir(dataDir());
    ensureDir(offlineDir());
    ensureDir(coverDir());
    ensureDir(trackCacheDir());
    Json c = Json::parse(readFile(dataDir() + "/config.json"));
    cfg.account.url = c["url"].str();
    cfg.account.user = c["user"].str();
    cfg.account.salt = c["salt"].str();
    cfg.account.token = c["token"].str();
    cfg.scrobble = c["scrobble"].boolean(true);
    cfg.streamKbps = c["streamKbps"].integer(320);
    cfg.volume = (float)c["volume"].num(0.8);
    cfg.dark = c["dark"].boolean(true);
    cfg.palette = c["palette"].integer(0);
    cfg.offlineForced = c["offlineForced"].boolean(false);
    cfg.insecure = c["insecure"].boolean(false);

    offline_.clear();
    Json o = Json::parse(readFile(dataDir() + "/offline.json"));
    for (const Json& e : o.items()) {
      OfflineEntry oe;
      oe.song = Song::fromJson(e["song"]);
      oe.file = e["file"].str();
      if (!oe.song.id.empty() && fileExists(oe.file)) offline_.push_back(std::move(oe));
    }

    pending_.clear();
    Json s = Json::parse(readFile(dataDir() + "/scrobbles.json"));
    for (const Json& e : s.items()) pending_.push_back({e["id"].str(), e["time"].integer64()});
  }

  void saveConfig() const {
    Json c = Json::object();
    c.set("url", cfg.account.url).set("user", cfg.account.user).set("salt", cfg.account.salt);
    c.set("token", cfg.account.token).set("scrobble", cfg.scrobble).set("streamKbps", cfg.streamKbps);
    c.set("volume", (double)cfg.volume).set("dark", cfg.dark).set("palette", cfg.palette);
    c.set("offlineForced", cfg.offlineForced).set("insecure", cfg.insecure);
    writeFile(dataDir() + "/config.json", c.dump(2));
  }

  const std::vector<OfflineEntry>& offline() const { return offline_; }
  uint32_t offlineRevision() const { return offlineRev_; }

  bool isOffline(const std::string& id) const { return findOffline(id) != nullptr; }

  const OfflineEntry* findOffline(const std::string& id) const {
    for (const auto& e : offline_) if (e.song.id == id) return &e;
    return nullptr;
  }

  void addOffline(const Song& s, const std::string& file) {
    removeOfflineEntry(s.id);
    offline_.push_back({s, file});
    saveOffline();
  }

  void removeOffline(const std::string& id) {
    const OfflineEntry* e = findOffline(id);
    if (e) removeFile(e->file);
    removeOfflineEntry(id);
    saveOffline();
  }

  void clearOffline() {
    for (const auto& e : offline_) removeFile(e.file);
    offline_.clear();
    saveOffline();
  }

  long long offlineBytes() const {
    long long n = 0;
    for (const auto& e : offline_) n += fileSize(e.file);
    return n;
  }

  void updateOfflineStar(const std::string& id, bool starred) {
    for (auto& e : offline_) if (e.song.id == id) e.song.starred = starred;
  }

  std::vector<PendingScrobble>& pendingScrobbles() { return pending_; }

  void savePending() const {
    Json a = Json::array();
    for (const auto& p : pending_) {
      Json e = Json::object();
      e.set("id", p.id).set("time", p.timeMs);
      a.push(e);
    }
    writeFile(dataDir() + "/scrobbles.json", a.dump());
  }

  void saveQueue(const SavedQueue& q) const {
    Json j = Json::object();
    Json songs = Json::array();
    for (const Song& s : q.songs) songs.push(s.toJson());
    j.set("songs", songs).set("index", q.index).set("shuffle", q.shuffle).set("repeat", q.repeat);
    writeFile(dataDir() + "/queue.json", j.dump());
  }

  SavedQueue loadQueue() const {
    SavedQueue q;
    Json j = Json::parse(readFile(dataDir() + "/queue.json"));
    for (const Json& s : j["songs"].items()) q.songs.push_back(Song::fromJson(s));
    q.index = j["index"].integer(-1);
    q.shuffle = j["shuffle"].boolean(false);
    q.repeat = j["repeat"].integer(0);
    if (q.index >= (int)q.songs.size()) q.index = -1;
    return q;
  }

  long long trackCacheBytes() const { return dirSize(trackCacheDir()); }

  void clearTrackCache() const {
    removeTree(trackCacheDir());
    ensureDir(trackCacheDir());
  }

  void trimTrackCache(long long maxBytes) const {
    std::vector<platform::DirEntry> files;
    long long total = 0;
    for (auto& e : platform::listDir(trackCacheDir())) {
      if (!e.isFile) continue;
      total += e.size;
      files.push_back(std::move(e));
    }
    std::sort(files.begin(), files.end(), [](const platform::DirEntry& a, const platform::DirEntry& b) { return a.mtime < b.mtime; });
    for (const platform::DirEntry& f : files) {
      if (total <= maxBytes) break;
      removeFile(f.path);
      total -= f.size;
    }
  }

  void logout() {
    cfg.account = Account{};
    saveConfig();
  }

 private:
  std::vector<OfflineEntry> offline_;
  std::vector<PendingScrobble> pending_;
  uint32_t offlineRev_ = 1;

  void removeOfflineEntry(const std::string& id) {
    offline_.erase(std::remove_if(offline_.begin(), offline_.end(), [&](const OfflineEntry& e) { return e.song.id == id; }),
                   offline_.end());
    ++offlineRev_;
  }

  void saveOffline() {
    ++offlineRev_;
    Json a = Json::array();
    for (const auto& e : offline_) {
      Json o = Json::object();
      o.set("song", e.song.toJson()).set("file", e.file);
      a.push(o);
    }
    writeFile(dataDir() + "/offline.json", a.dump());
  }
};

}
