#pragma once

#include "../widgets/Flex.hpp"
#include "../widgets/HintBar.hpp"
#include "../widgets/Label.hpp"

namespace neko {

class View : public std::enable_shared_from_this<View> {
 public:
  using Ptr = std::shared_ptr<View>;

  std::string name_;
  std::shared_ptr<Box> frame_, top_, body_, bottom_;
  std::shared_ptr<Label> title_;
  std::shared_ptr<HintBar> hints_;
  std::vector<KeyBind> keybinds_;
  std::function<void()> onEnter_, onLeave_;
  std::shared_ptr<void> keep_;

  explicit View(std::string name) : name_(std::move(name)) {
    top_ = Box::make(Direction::Row)->cls("shell-top");
    title_ = Label::make(name_)->cls("view-title");
    top_->add(title_)->add(Spacer::make());
    body_ = Box::make(Direction::Column)->cls("shell-body");
    hints_ = HintBar::make()->cls("hintbar")->fillWidth();
    bottom_ = Box::make(Direction::Row)->cls("shell-bottom");
    bottom_->add(hints_);
    frame_ = Box::make(Direction::Column)->cls("shell-main");
    frame_->add({top_, body_, bottom_});
  }

  static Ptr make(std::string name) { return std::make_shared<View>(std::move(name)); }

  const std::string& name() const { return name_; }
  std::shared_ptr<Box> top() { return top_; }
  std::shared_ptr<Box> body() { return body_; }
  std::shared_ptr<Box> bottom() { return bottom_; }

  Ptr title(const std::string& t) {
    name_ = t;
    title_->text(t);
    return shared_from_this();
  }
  Ptr hideTop(bool hide = true) {
    top_->visible_ = !hide;
    return shared_from_this();
  }
  Ptr hideBottom(bool hide = true) {
    bottom_->visible_ = !hide;
    return shared_from_this();
  }
  Ptr bind(Action a, std::string label, std::function<void()> cb, bool hint = true) {
    keybinds_.push_back({a, std::move(label), std::move(cb), hint});
    return shared_from_this();
  }
  Ptr keep(std::shared_ptr<void> owner) {
    keep_ = std::move(owner);
    return shared_from_this();
  }
  Ptr onEnter(std::function<void()> f) {
    onEnter_ = std::move(f);
    return shared_from_this();
  }
  Ptr onLeave(std::function<void()> f) {
    onLeave_ = std::move(f);
    return shared_from_this();
  }
};

using ViewPtr = View::Ptr;

}
