#pragma once

#include "Assets.hpp"
#include "Audio.hpp"
#include "Colors.hpp"
#include "Common.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "Style.hpp"

namespace neko {

class Widget;

struct Hint {
  Action action;
  std::string label;
};

class SignalBus {
 public:
  using Payload = std::variant<std::monostate, bool, int, float, std::string>;
  using Handler = std::function<void(const Payload&)>;

  int connect(const std::string& name, Handler h) {
    entries_.push_back({next_, name, std::move(h)});
    return next_++;
  }
  void disconnect(int handle) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.id == handle; }),
                   entries_.end());
  }
  void emit(const std::string& name, const Payload& p = Payload{}) {
    std::vector<Handler> run;
    for (auto& e : entries_) if (e.name == name) run.push_back(e.fn);
    for (auto& h : run) h(p);
  }

 private:
  struct Entry {
    int id;
    std::string name;
    Handler fn;
  };
  std::vector<Entry> entries_;
  int next_ = 1;
};

class FocusManager {
 public:
  Widget* current() const { return current_; }
  void set(Widget* w);
  void clear() { set(nullptr); }
  void forget(Widget* w) {
    if (current_ == w) current_ = nullptr;
  }
  bool move(Widget* scopeRoot, Action dir);
  Widget* first(Widget* scopeRoot);
  void ensureValid(Widget* scopeRoot);

 private:
  Widget* current_ = nullptr;
};

struct Context {
  Renderer* renderer = nullptr;
  ColorRegistry* colors = nullptr;
  StyleSheet* styles = nullptr;
  Input* input = nullptr;
  AssetLoader* assets = nullptr;
  MusicPlayer* audio = nullptr;
  SignalBus* signals = nullptr;
  FocusManager* focus = nullptr;
  Vec2 viewport{960, 544};
  float time = 0, dt = 0;
  bool showFocus = true;
  std::vector<Hint> hints;
  uint32_t hintRevision = 0;
  float baseFontSize = 16.0f;
};

}
