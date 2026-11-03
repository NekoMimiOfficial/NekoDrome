#pragma once

#include "../core/Icons.hpp"
#include "Widget.hpp"

namespace neko {

template <class D>
class CheckableT : public WidgetT<D> {
 public:
  using Ptr = std::shared_ptr<D>;
  std::function<void(bool)> onChange_;

  CheckableT() { this->focusable_ = true; }

  Ptr value(bool v) {
    this->checked_ = v;
    return this->self();
  }
  bool value() const { return this->checked_; }
  Ptr onChange(std::function<void(bool)> f) {
    onChange_ = std::move(f);
    return this->self();
  }
  bool interactive() const override { return true; }

  virtual void setChecked(Context& ctx, bool v) {
    if (this->checked_ == v) return;
    this->checked_ = v;
    if (onChange_) onChange_(v);
    if (!this->emit_.empty() && ctx.signals) ctx.signals->emit(this->emit_, v);
  }

  void click(Context& ctx) override {
    if (this->disabled_) return;
    setChecked(ctx, !this->checked_);
    if (this->onClick_) this->onClick_();
  }
  bool onAction(Context& ctx, Action a) override {
    if (a == Action::Accept && !this->disabled_) {
      this->flash_ = 0.14f;
      click(ctx);
      return true;
    }
    return false;
  }
};

class Toggle : public CheckableT<Toggle> {
 public:
  const char* typeName() const override { return "toggle"; }

  Toggle() {
    pos_.motion = Motion::bouncy();
    size_.motion = Motion::bouncy();
    size_.snap(16.0f);
  }

  static Ptr make(bool on = false) {
    auto t = std::make_shared<Toggle>();
    t->checked_ = on;
    t->pos_.snap(on ? 1.0f : 0.0f);
    t->size_.snap(on ? 24.0f : 16.0f);
    return t;
  }

  Vec2 contentSize(Context&, Vec2) override { return {52.0f, 32.0f}; }

  void tick(Context&, float dt) override {
    pos_.to(checked_ ? 1.0f : 0.0f);
    float target = pressed_ ? 28.0f : (checked_ ? 24.0f : 16.0f);
    size_.to(target);
    pos_.update(dt);
    size_.update(dt);
  }

  void drawContent(Context& ctx) override {
    Rect in = rect_;
    float d = std::max(4.0f, size_.value);
    float minC = in.x + 16.0f;
    float maxC = in.right() - 16.0f;
    float cx = lerpf(minC, maxC, pos_.value);
    ctx.renderer->fillRRect({cx - d * 0.5f, in.cy() - d * 0.5f, d, d}, d * 0.5f, handle_.current());
    if (checked_ && pos_.value > 0.55f) {
      Color c = track_.current();
      float k = saturate((pos_.value - 0.55f) / 0.45f);
      c.a *= k;
      std::string g = encodeUtf8(icons::check);
      int sz = 16;
      TextMetrics m = ctx.renderer->textSize(g, FontFace::Glyph, sz);
      ctx.renderer->text(g, FontFace::Glyph, sz, {cx - m.w * 0.5f, in.cy() - m.h * 0.5f}, c);
    }
  }

 private:
  Spring pos_, size_;
};

class Checkbox : public CheckableT<Checkbox> {
 public:
  const char* typeName() const override { return "checkbox"; }

  Checkbox() { mark_.motion = Motion::bouncy(); }

  static Ptr make(bool on = false) {
    auto c = std::make_shared<Checkbox>();
    c->checked_ = on;
    c->mark_.snap(on ? 1.0f : 0.0f);
    return c;
  }

  Vec2 contentSize(Context&, Vec2) override { return {24.0f, 24.0f}; }

  void tick(Context&, float dt) override {
    mark_.to(checked_ ? 1.0f : 0.0f);
    mark_.update(dt);
  }

  void drawContent(Context& ctx) override {
    float k = mark_.value;
    if (k <= 0.02f) return;
    Color c = fg_.current();
    c.a *= saturate(k * 1.5f);
    int sz = std::max(6, (int)std::lround(18.0f * std::max(0.1f, k)));
    std::string g = encodeUtf8(icons::check);
    TextMetrics m = ctx.renderer->textSize(g, FontFace::Glyph, sz);
    ctx.renderer->text(g, FontFace::Glyph, sz, {rect_.cx() - m.w * 0.5f, rect_.cy() - m.h * 0.5f}, c);
  }

 private:
  Spring mark_;
};

class Radio : public CheckableT<Radio> {
 public:
  const char* typeName() const override { return "radio"; }
  std::string group_;

  Radio() { dot_.motion = Motion::bouncy(); }

  static Ptr make(std::string group = "", bool on = false) {
    auto r = std::make_shared<Radio>();
    r->group_ = std::move(group);
    r->checked_ = on;
    r->dot_.snap(on ? 1.0f : 0.0f);
    return r;
  }

  Ptr group(std::string g) { group_ = std::move(g); return self(); }

  Vec2 contentSize(Context&, Vec2) override { return {24.0f, 24.0f}; }

  void setChecked(Context& ctx, bool v) override {
    if (!v && checked_ && !group_.empty()) return;
    bool was = checked_;
    CheckableT<Radio>::setChecked(ctx, v);
    if (v && !was && !group_.empty() && parent_) {
      for (auto& c : parent_->children_) {
        Radio* o = dynamic_cast<Radio*>(c.get());
        if (o && o != this && o->group_ == group_ && o->checked_) {
          o->checked_ = false;
          if (o->onChange_) o->onChange_(false);
        }
      }
    }
  }

  void tick(Context&, float dt) override {
    dot_.to(checked_ ? 1.0f : 0.0f);
    dot_.update(dt);
  }

  void drawContent(Context& ctx) override {
    float r = 6.0f * std::max(0.0f, dot_.value);
    if (r < 0.5f) return;
    ctx.renderer->disc({rect_.cx(), rect_.cy()}, r, accent_.current());
  }

 private:
  Spring dot_;
};

}
