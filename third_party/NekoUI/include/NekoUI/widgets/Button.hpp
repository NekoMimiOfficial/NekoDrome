#pragma once

#include "Flex.hpp"

namespace neko {

class Button : public ContainerT<Button> {
 public:
  const char* typeName() const override { return "button"; }
  std::function<void(bool)> onToggle_;
  bool toggleable_ = false;
  std::string group_;

  Button() { focusable_ = true; }

  static Ptr make() { return std::make_shared<Button>(); }

  Ptr toggleable(bool v = true) { toggleable_ = v; return self(); }
  Ptr onToggle(std::function<void(bool)> f) { onToggle_ = std::move(f); toggleable_ = true; return self(); }
  Ptr group(std::string g) { group_ = std::move(g); return self(); }

  void select() {
    bool was = checked_;
    checked_ = true;
    if (!group_.empty() && parent_) {
      for (auto& c : parent_->children_) {
        Button* o = dynamic_cast<Button*>(c.get());
        if (o && o != this && o->group_ == group_ && o->checked_) {
          o->checked_ = false;
          if (o->onToggle_) o->onToggle_(false);
        }
      }
    }
    if (!was && onToggle_) onToggle_(true);
  }

  bool interactive() const override { return true; }

  void click(Context& ctx) override {
    if (disabled_) return;
    if (!group_.empty()) {
      select();
    } else if (toggleable_) {
      checked_ = !checked_;
      if (onToggle_) onToggle_(checked_);
    }
    Widget::click(ctx);
  }
  bool onAction(Context& ctx, Action a) override {
    if (a == Action::Accept && !disabled_) {
      flash_ = 0.14f;
      click(ctx);
      return true;
    }
    return false;
  }
};

}
