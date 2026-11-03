#pragma once

#include <deque>
#include <functional>
#include <memory>

#include "Covers.hpp"

namespace nt {

class Downloads {
 public:
  enum class State { None, Queued, Active, Done, Failed };

  struct Item {
    Song song;
    State state = State::Queued;
    std::shared_ptr<Transfer> transfer;
    std::string error;
  };

  std::function<void(const Song&, bool, const std::string&)> onFinished;

  Downloads(Net& net, Subsonic& api, Store& store, Covers& covers)
      : net_(net), api_(api), store_(store), covers_(covers) {}

  void enqueue(const Song& s) {
    if (store_.isOffline(s.id)) return;
    for (auto& it : items_) {
      if (it->song.id == s.id) {
        if (it->state == State::Failed) {
          it->state = State::Queued;
          it->error.clear();
          ++rev_;
        }
        return;
      }
    }
    auto it = std::make_shared<Item>();
    it->song = s;
    items_.push_back(it);
    ++rev_;
  }

  void enqueueAll(const std::vector<Song>& songs) {
    for (const Song& s : songs) enqueue(s);
  }

  void cancel(const std::string& id) {
    for (auto it = items_.begin(); it != items_.end(); ++it) {
      if ((*it)->song.id != id) continue;
      if ((*it)->transfer) (*it)->transfer->cancel = true;
      else {
        items_.erase(it);
        ++rev_;
      }
      return;
    }
  }

  void cancelAll() {
    for (auto& it : items_) {
      if (it->transfer) it->transfer->cancel = true;
    }
    items_.erase(std::remove_if(items_.begin(), items_.end(), [](const std::shared_ptr<Item>& i) { return !i->transfer; }),
                 items_.end());
    ++rev_;
  }

  void dismissFailed() {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](const std::shared_ptr<Item>& i) { return i->state == State::Failed; }),
                 items_.end());
    ++rev_;
  }

  void remove(const std::string& id) {
    store_.removeOffline(id);
    ++rev_;
  }

  State state(const std::string& id) const {
    if (store_.isOffline(id)) return State::Done;
    for (const auto& it : items_)
      if (it->song.id == id) return it->state;
    return State::None;
  }

  float progress(const std::string& id) const {
    for (const auto& it : items_)
      if (it->song.id == id && it->transfer) return it->transfer->progress();
    return 0.0f;
  }

  const std::vector<std::shared_ptr<Item>>& items() const { return items_; }
  uint32_t revision() const { return rev_ + store_.offlineRevision(); }
  int activeCount() const {
    int n = 0;
    for (const auto& it : items_) if (it->state == State::Active || it->state == State::Queued) ++n;
    return n;
  }

  float overallProgress() const {
    float sum = 0;
    int n = 0;
    for (const auto& it : items_) {
      if (it->state == State::Failed) continue;
      ++n;
      if (it->transfer) sum += it->transfer->progress();
    }
    return n ? sum / (float)n : 0.0f;
  }

  void update() {
    int active = 0;
    for (auto& it : items_) if (it->state == State::Active) ++active;
    for (auto& it : items_) {
      if (active >= 2) break;
      if (it->state != State::Queued) continue;
      start(it);
      ++active;
    }
  }

 private:
  Net& net_;
  Subsonic& api_;
  Store& store_;
  Covers& covers_;
  std::vector<std::shared_ptr<Item>> items_;
  uint32_t rev_ = 1;

  void start(const std::shared_ptr<Item>& item) {
    item->state = State::Active;
    ++rev_;
    std::string ext = item->song.suffix.empty() ? "audio" : item->song.suffix;
    std::string dest = store_.offlineDir() + "/" + item->song.id + "." + ext;
    std::weak_ptr<Item> weak = item;
    item->transfer = net_.download(api_.url("download", {{"id", item->song.id}}), dest, [this, weak, dest](bool ok, const std::string& err) {
      auto it = weak.lock();
      if (!it) return;
      if (ok) {
        store_.addOffline(it->song, dest);
        covers_.ready(it->song.coverArt, 300);
        covers_.ready(it->song.coverArt, 600);
        items_.erase(std::remove(items_.begin(), items_.end(), it), items_.end());
      } else if (err == "cancelled") {
        items_.erase(std::remove(items_.begin(), items_.end(), it), items_.end());
      } else {
        it->state = State::Failed;
        it->error = err;
      }
      ++rev_;
      if (onFinished) onFinished(it->song, ok, err);
    });
  }
};

}
