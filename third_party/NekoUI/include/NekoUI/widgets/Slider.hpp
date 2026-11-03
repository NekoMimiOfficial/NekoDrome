#pragma once

#include "Widget.hpp"

namespace neko {

class Slider : public WidgetT<Slider> {
 public:
  const char* typeName() const override { return "slider"; }
  std::function<void(float)> onChange_, onCommit_;
  float trackHeight_ = 16.0f;

  Slider() {
    focusable_ = true;
    shown_.motion = Motion::snappy();
    handleW_.motion = Motion::bouncy();
    handleW_.snap(4.0f);
  }

  static Ptr make(float value = 0.0f, float lo = 0.0f, float hi = 1.0f) {
    auto s = std::make_shared<Slider>();
    s->min_ = lo;
    s->max_ = hi;
    s->value_ = clampf(value, lo, hi);
    s->shown_.snap(s->norm());
    return s;
  }

  Ptr range(float lo, float hi) { min_ = lo; max_ = hi; value_ = clampf(value_, lo, hi); return self(); }
  Ptr step(float s) { step_ = s; return self(); }
  Ptr thickness(float t) { trackHeight_ = t; return self(); }
  Ptr onChange(std::function<void(float)> f) { onChange_ = std::move(f); return self(); }
  Ptr onCommit(std::function<void(float)> f) { onCommit_ = std::move(f); return self(); }
  bool dragging() const { return dragging_; }
  Ptr value(float v) { setValue(v, false); return self(); }
  float value() const { return value_; }

  bool interactive() const override { return true; }
  bool capturesDrag() const override { return true; }

  Vec2 contentSize(Context&, Vec2 avail) override {
    return {isFinite(avail.x) ? std::min(avail.x, 240.0f) : 240.0f, 40.0f};
  }

  void setValue(float v, bool notify) {
    v = clampf(v, min_, max_);
    if (step_ > 0.0f) v = clampf(min_ + std::round((v - min_) / step_) * step_, min_, max_);
    if (v == value_) return;
    value_ = v;
    if (notify) {
      if (onChange_) onChange_(value_);
      if (!emit_.empty() && sig_) sig_->emit(emit_, value_);
    }
  }

  void onPointerDown(Context& ctx, Vec2 p) override {
    pressed_ = true;
    dragging_ = true;
    sig_ = ctx.signals;
    fromX(p.x);
  }
  void onPointerMove(Context& ctx, Vec2 p) override {
    sig_ = ctx.signals;
    if (dragging_) fromX(p.x);
  }
  void onPointerUp(Context&, Vec2, bool) override {
    bool was = dragging_;
    pressed_ = false;
    dragging_ = false;
    if (was && onCommit_) onCommit_(value_);
  }
  void onPointerCancel(Context&) override {
    pressed_ = false;
    dragging_ = false;
  }

  bool onAction(Context& ctx, Action a) override {
    if (disabled_) return false;
    float inc = step_ > 0.0f ? step_ : (max_ - min_) / 20.0f;
    sig_ = ctx.signals;
    if (a == Action::Left) { setValue(value_ - inc, true); if (onCommit_) onCommit_(value_); return true; }
    if (a == Action::Right) { setValue(value_ + inc, true); if (onCommit_) onCommit_(value_); return true; }
    return false;
  }

  void tick(Context&, float dt) override {
    shown_.to(norm());
    if (dragging_) shown_.snap(norm());
    shown_.update(dt);
    handleW_.to(dragging_ || pressed_ ? 2.0f : 4.0f);
    handleW_.update(dt);
  }

  void drawContent(Context& ctx) override {
    Renderer& r = *ctx.renderer;
    Rect in = rect_.inset(cs_.padding.value_or(Insets()));
    float th = std::min(trackHeight_, in.h);
    float handleH = std::min(in.h, th + 12.0f);
    float hw = std::max(1.0f, handleW_.value);
    float pad = 6.0f;
    float usable = std::max(1.0f, in.w - 4.0f);
    float hx = in.x + 2.0f + usable * saturate(shown_.value);
    float cy = in.cy();
    float rad = th * 0.5f;
    float smallR = 3.0f;
    Color active = accent_.current();
    Color track = track_.current();
    Color handle = handle_.current();
    float leftEnd = hx - hw * 0.5f - pad;
    float rightStart = hx + hw * 0.5f + pad;
    if (leftEnd > in.x) r.fillRRect({in.x, cy - th * 0.5f, leftEnd - in.x, th}, rad, active);
    if (rightStart < in.right()) r.fillRRect({rightStart, cy - th * 0.5f, in.right() - rightStart, th}, rad, track);
    r.fillRRect({hx - hw * 0.5f, cy - handleH * 0.5f, hw, handleH}, hw * 0.5f, handle);
    (void)smallR;
  }

 private:
  float value_ = 0, min_ = 0, max_ = 1, step_ = 0;
  Spring shown_, handleW_;
  bool dragging_ = false;
  SignalBus* sig_ = nullptr;

  float norm() const { return max_ > min_ ? (value_ - min_) / (max_ - min_) : 0.0f; }

  void fromX(float x) {
    Rect in = rect_.inset(cs_.padding.value_or(Insets()));
    float usable = std::max(1.0f, in.w - 4.0f);
    float t = saturate((x - in.x - 2.0f) / usable);
    setValue(min_ + t * (max_ - min_), true);
  }
};

}
