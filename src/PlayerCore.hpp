#pragma once

#include <NekoUI/core/Audio.hpp>

#include <algorithm>
#include <random>

#include "Downloads.hpp"

namespace nt {

class PlayerCore {
 public:
  enum class Repeat { Off = 0, All = 1, One = 2 };

  std::function<void(const std::string&)> onError;

  PlayerCore(Net& net, Subsonic& api, Store& store, neko::MusicPlayer& music)
      : net_(net), api_(api), store_(store), music_(music) {
    music_.onFinished = [this]() { handleFinished(); };
  }

  const std::vector<Song>& queue() const { return queue_; }
  int index() const { return index_; }
  uint32_t revision() const { return rev_; }
  const Song* current() const { return index_ >= 0 && index_ < (int)queue_.size() ? &queue_[(size_t)index_] : nullptr; }
  bool shuffle() const { return shuffle_; }
  Repeat repeat() const { return repeat_; }
  bool playing() const { return music_.playing(); }
  bool buffering() const { return buffering_ || music_.buffering(); }
  float bufferProgress() const { return transfer_ ? transfer_->progress() : 0.0f; }
  bool offline() const { return offline_; }
  void setOffline(bool v) { offline_ = v; }
  double position() const { return music_.position(); }
  double duration() const {
    if (music_.duration() > 0) return music_.duration();
    const Song* s = current();
    return s ? s->duration : 0.0;
  }
  float progress() const {
    double d = duration();
    return d > 0 ? neko::saturate((float)(position() / d)) : 0.0f;
  }
  bool idle() const { return !started_ && !buffering_; }

  void setQueue(std::vector<Song> songs, int start, bool autoplay = true) {
    if (songs.empty()) return;
    queue_ = std::move(songs);
    original_ = queue_;
    if (shuffle_) shuffleAfter(std::max(0, start));
    else index_ = std::max(0, std::min(start, (int)queue_.size() - 1));
    ++rev_;
    if (autoplay) playIndex(index_);
    else saveState();
  }

  void playNext(const Song& s) {
    if (queue_.empty()) {
      setQueue({s}, 0, true);
      return;
    }
    queue_.insert(queue_.begin() + index_ + 1, s);
    original_.push_back(s);
    ++rev_;
  }

  void enqueue(const Song& s) {
    if (queue_.empty()) {
      setQueue({s}, 0, true);
      return;
    }
    queue_.push_back(s);
    original_.push_back(s);
    ++rev_;
  }

  void removeAt(int i) {
    if (i < 0 || i >= (int)queue_.size()) return;
    bool wasCurrent = i == index_;
    std::string id = queue_[(size_t)i].id;
    queue_.erase(queue_.begin() + i);
    auto it = std::find_if(original_.begin(), original_.end(), [&](const Song& s) { return s.id == id; });
    if (it != original_.end()) original_.erase(it);
    if (i < index_) --index_;
    ++rev_;
    if (queue_.empty()) {
      stop();
      index_ = -1;
      return;
    }
    if (wasCurrent) {
      index_ = std::min(index_, (int)queue_.size() - 1);
      playIndex(index_);
    }
  }

  void clearQueue() {
    stop();
    queue_.clear();
    original_.clear();
    index_ = -1;
    ++rev_;
    saveState();
  }

  void playIndex(int i) {
    if (queue_.empty()) return;
    index_ = std::max(0, std::min(i, (int)queue_.size() - 1));
    const Song s = queue_[(size_t)index_];
    ++ticket_;
    listened_ = 0;
    scrobbled_ = false;
    nowPlayingSent_ = false;
    startedAtMs_ = nowMs();
    started_ = false;
    loading_ = false;
    cancelTransfer();
    music_.stop();
    ++rev_;

    std::string local = resolveLocal(s);
    if (!local.empty()) {
      startLocal(local, s);
      return;
    }
    if (offline_ || store_.cfg.offlineForced) {
      fail("\"" + s.title + "\" is not available offline");
      return;
    }
    buffering_ = true;
    std::string dest = cachePath(s);
    uint32_t ticket = ticket_;
    transfer_ = net_.download(api_.streamUrl(s.id, store_.cfg.streamKbps), dest, [this, ticket, s, dest](bool ok, const std::string& err) {
      if (ticket != ticket_) return;
      buffering_ = false;
      transfer_.reset();
      if (!ok) {
        if (err != "cancelled") fail("Couldn't load \"" + s.title + "\" (" + err + ")");
        return;
      }
      startLocal(dest, s);
    });
  }

  void toggle() {
    if (queue_.empty()) return;
    if (buffering()) return;
    if (!started_) {
      playIndex(index_ < 0 ? 0 : index_);
      return;
    }
    music_.toggle();
  }

  void pause() {
    if (music_.playing()) music_.pause();
  }

  void next() {
    if (queue_.empty()) return;
    int n = index_ + 1;
    if (n >= (int)queue_.size()) n = 0;
    playIndex(n);
  }

  void previous() {
    if (queue_.empty()) return;
    if (music_.position() > 3.0) {
      music_.seek(0);
      return;
    }
    int n = index_ - 1;
    if (n < 0) n = repeat_ == Repeat::Off ? 0 : (int)queue_.size() - 1;
    playIndex(n);
  }

  void seekNormalized(float t) {
    if (!started_) return;
    double d = duration();
    if (d > 0) music_.seek((double)neko::saturate(t) * d);
  }

  void setVolume(float v) {
    music_.setVolume(v);
    store_.cfg.volume = music_.volume();
  }
  float volume() const { return music_.volume(); }

  void setShuffle(bool on) {
    if (on == shuffle_) return;
    shuffle_ = on;
    if (queue_.empty()) return;
    if (on) {
      original_ = queue_;
      shuffleAfter(index_);
    } else {
      std::string id = current() ? current()->id : "";
      queue_ = original_;
      index_ = 0;
      for (size_t i = 0; i < queue_.size(); ++i) if (queue_[i].id == id) index_ = (int)i;
    }
    ++rev_;
  }

  void cycleRepeat() {
    repeat_ = repeat_ == Repeat::Off ? Repeat::All : (repeat_ == Repeat::All ? Repeat::One : Repeat::Off);
    ++rev_;
  }

  void setStarred(const std::string& id, bool starred) {
    for (Song& s : queue_) if (s.id == id) s.starred = starred;
    for (Song& s : original_) if (s.id == id) s.starred = starred;
    ++rev_;
  }

  void update(float dt) {
    if (loading_ && music_.state() == neko::MusicPlayer::State::Stopped) {
      loading_ = false;
      started_ = false;
      const Song* s = current();
      if (s) {
        removeFile(cachePath(*s));
        fail("Couldn't play \"" + s->title + "\"");
      }
    }
    if (loading_ && music_.state() == neko::MusicPlayer::State::Playing) loading_ = false;

    if (music_.playing() && started_) {
      listened_ += dt;
      const Song* s = current();
      if (s && !nowPlayingSent_) {
        nowPlayingSent_ = true;
        if (store_.cfg.scrobble && !offline_ && !store_.cfg.offlineForced) api_.scrobble(s->id, 0, false);
      }
      double d = duration();
      double threshold = d > 0 ? std::min(d * 0.5, 240.0) : 30.0;
      if (s && !scrobbled_ && store_.cfg.scrobble && listened_ >= threshold) {
        scrobbled_ = true;
        submitScrobble(s->id, startedAtMs_);
      }
    }

    flushTimer_ += dt;
    if (flushTimer_ > 20.0f) {
      flushTimer_ = 0;
      flushScrobbles();
    }
    saveTimer_ += dt;
    if (saveTimer_ > 15.0f && rev_ != savedRev_) saveState();
  }

  void saveState() {
    SavedQueue q;
    q.songs = queue_;
    q.index = index_;
    q.shuffle = shuffle_;
    q.repeat = (int)repeat_;
    store_.saveQueue(q);
    savedRev_ = rev_;
    saveTimer_ = 0;
  }

  void restoreState() {
    SavedQueue q = store_.loadQueue();
    if (q.songs.empty()) return;
    queue_ = q.songs;
    original_ = queue_;
    index_ = q.index;
    shuffle_ = false;
    repeat_ = (Repeat)std::max(0, std::min(2, q.repeat));
    ++rev_;
    savedRev_ = rev_;
  }

  void flushScrobbles() {
    if (offline_ || store_.cfg.offlineForced || !api_.account().valid() || flushing_) return;
    auto& pend = store_.pendingScrobbles();
    if (pend.empty()) return;
    flushing_ = true;
    PendingScrobble p = pend.front();
    api_.scrobble(p.id, p.timeMs, true, [this, p](bool ok) {
      flushing_ = false;
      if (!ok) return;
      auto& q = store_.pendingScrobbles();
      if (!q.empty() && q.front().id == p.id && q.front().timeMs == p.timeMs) q.erase(q.begin());
      store_.savePending();
      flushScrobbles();
    });
  }

  size_t pendingScrobbles() { return store_.pendingScrobbles().size(); }

 private:
  Net& net_;
  Subsonic& api_;
  Store& store_;
  neko::MusicPlayer& music_;

  std::vector<Song> queue_, original_;
  int index_ = -1;
  bool shuffle_ = false;
  Repeat repeat_ = Repeat::Off;
  bool offline_ = false;
  bool started_ = false, buffering_ = false, loading_ = false;
  uint32_t ticket_ = 0, rev_ = 1, savedRev_ = 0;
  std::shared_ptr<Transfer> transfer_, prefetch_;
  std::string prefetchId_;
  double listened_ = 0;
  bool scrobbled_ = false, nowPlayingSent_ = false, flushing_ = false;
  long long startedAtMs_ = 0;
  float flushTimer_ = 0, saveTimer_ = 0;
  std::mt19937 rng_{std::random_device{}()};

  std::string cachePath(const Song& s) const {
    std::string ext = store_.cfg.streamKbps > 0 ? "mp3" : (s.suffix.empty() ? "audio" : s.suffix);
    return store_.trackCacheDir() + "/" + s.id + "_" + std::to_string(store_.cfg.streamKbps) + "." + ext;
  }

  std::string resolveLocal(const Song& s) const {
    if (const OfflineEntry* e = store_.findOffline(s.id)) return e->file;
    std::string c = cachePath(s);
    if (fileExists(c) && fileSize(c) > 0) return c;
    return "";
  }

  void cancelTransfer() {
    if (transfer_) transfer_->cancel = true;
    transfer_.reset();
    buffering_ = false;
  }

  void startLocal(const std::string& path, const Song& s) {
    touchFile(path);
    music_.play(path);
    music_.setDurationHint((double)s.duration);
    started_ = true;
    loading_ = true;
    ++rev_;
    prefetchNext();
  }

  void prefetchNext() {
    if (offline_ || store_.cfg.offlineForced || queue_.size() < 2) return;
    int n = index_ + 1;
    if (n >= (int)queue_.size()) {
      if (repeat_ != Repeat::All) return;
      n = 0;
    }
    const Song& s = queue_[(size_t)n];
    if (!resolveLocal(s).empty() || prefetchId_ == s.id) return;
    if (prefetch_) prefetch_->cancel = true;
    prefetchId_ = s.id;
    prefetch_ = net_.download(api_.streamUrl(s.id, store_.cfg.streamKbps), cachePath(s), nullptr);
  }

  void shuffleAfter(int keep) {
    if (queue_.empty()) return;
    keep = std::max(0, std::min(keep, (int)queue_.size() - 1));
    Song cur = queue_[(size_t)keep];
    std::vector<Song> rest;
    for (size_t i = 0; i < queue_.size(); ++i) if ((int)i != keep) rest.push_back(queue_[i]);
    std::shuffle(rest.begin(), rest.end(), rng_);
    queue_.clear();
    queue_.push_back(cur);
    for (auto& s : rest) queue_.push_back(s);
    index_ = 0;
  }

  void fail(const std::string& msg) {
    started_ = false;
    buffering_ = false;
    loading_ = false;
    ++rev_;
    if (onError) onError(msg);
  }

  void submitScrobble(const std::string& id, long long timeMs) {
    if (offline_ || store_.cfg.offlineForced) {
      store_.pendingScrobbles().push_back({id, timeMs});
      store_.savePending();
      return;
    }
    api_.scrobble(id, timeMs, true, [this, id, timeMs](bool ok) {
      if (ok) return;
      store_.pendingScrobbles().push_back({id, timeMs});
      store_.savePending();
    });
  }

  void handleFinished() {
    started_ = false;
    if (repeat_ == Repeat::One) {
      playIndex(index_);
      return;
    }
    if (index_ + 1 < (int)queue_.size()) {
      playIndex(index_ + 1);
      return;
    }
    if (repeat_ == Repeat::All && !queue_.empty()) {
      playIndex(0);
      return;
    }
    ++rev_;
  }

  void stop() {
    ++ticket_;
    cancelTransfer();
    music_.stop();
    started_ = false;
    loading_ = false;
  }
};

}
