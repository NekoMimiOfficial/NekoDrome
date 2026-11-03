#pragma once

#include <unordered_map>

#include "Store.hpp"

namespace nt {

class Covers {
 public:
  Covers(Net& net, Subsonic& api, Store& store) : net_(net), api_(api), store_(store) {}

  std::string path(const std::string& id, int size) const {
    return store_.coverDir() + "/" + sanitize(id) + "_" + std::to_string(size) + ".jpg";
  }

  bool ready(const std::string& id, int size) {
    if (id.empty()) return false;
    std::string key = id + "#" + std::to_string(size);
    auto it = state_.find(key);
    if (it != state_.end()) {
      if (it->second == State::Ready) return true;
      if (it->second == State::Pending) return false;
      if (it->second == State::Failed && failedAt_[key] + 20.0 > clock_) return false;
    }
    std::string p = path(id, size);
    if (fileExists(p)) {
      state_[key] = State::Ready;
      return true;
    }
    if (offline_ || !api_.account().valid()) return false;
    state_[key] = State::Pending;
    net_.download(api_.coverUrl(id, size), p, [this, key](bool ok, const std::string&) {
      state_[key] = ok ? State::Ready : State::Failed;
      failedAt_[key] = clock_;
    }, true);
    return false;
  }

  void setOffline(bool v) { offline_ = v; }
  void tick(double dt) { clock_ += dt; }

 private:
  enum class State { Pending, Ready, Failed };
  Net& net_;
  Subsonic& api_;
  Store& store_;
  std::unordered_map<std::string, State> state_;
  std::unordered_map<std::string, double> failedAt_;
  double clock_ = 0;
  bool offline_ = false;

  static std::string sanitize(const std::string& s) {
    std::string o;
    for (char c : s) o += (std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    return o;
  }
};

}
