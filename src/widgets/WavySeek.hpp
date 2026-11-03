#pragma once

#include <NekoUI/Ui.hpp>

namespace nt {

class WavySeek : public neko::WidgetT<WavySeek> {
 public:
  const char* typeName() const override { return "wavyseek"; }
  std::function<void(float)> onSeek_;

  WavySeek() {
    focusable_ = true;
    shown_.motion = neko::Motion::snappy();
    amp_.motion = neko::Motion::gentle();
    handleW_.motion = neko::Motion::bouncy();
    handleW_.snap(4.0f);
  }

  static Ptr make() { return std::make_shared<WavySeek>(); }

  Ptr onSeek(std::function<void(float)> f) { onSeek_ = std::move(f); return self(); }
  Ptr active(bool a) { active_ = a; return self(); }
  Ptr value(float v) {
    if (!dragging_) value_ = neko::saturate(v);
    return self();
  }
  bool dragging() const { return dragging_; }

  bool interactive() const override { return true; }
  bool capturesDrag() const override { return true; }

  neko::Vec2 contentSize(neko::Context&, neko::Vec2 avail) override {
    return {neko::isFinite(avail.x) ? avail.x : 300.0f, 40.0f};
  }

  void onPointerDown(neko::Context&, neko::Vec2 p) override {
    pressed_ = true;
    dragging_ = true;
    fromX(p.x);
  }
  void onPointerMove(neko::Context&, neko::Vec2 p) override {
    if (dragging_) fromX(p.x);
  }
  void onPointerUp(neko::Context&, neko::Vec2, bool) override { finish(); }
  void onPointerCancel(neko::Context&) override { finish(); }

  bool onAction(neko::Context&, neko::Action a) override {
    if (a == neko::Action::Left || a == neko::Action::Right) {
      value_ = neko::saturate(value_ + (a == neko::Action::Left ? -0.03f : 0.03f));
      shown_.snap(value_);
      if (onSeek_) onSeek_(value_);
      return true;
    }
    return false;
  }

  void tick(neko::Context&, float dt) override {
    shown_.to(value_);
    if (dragging_) shown_.snap(value_);
    shown_.update(dt);
    amp_.to(active_ || dragging_ ? 3.4f : 0.0f);
    amp_.update(dt);
    handleW_.to(dragging_ ? 2.0f : 4.0f);
    handleW_.update(dt);
    if (active_ || dragging_) phase_ += dt * 5.5f;
  }

  void drawContent(neko::Context& ctx) override {
    neko::Renderer& r = *ctx.renderer;
    neko::Rect in = rect_.inset(cs_.padding.value_or(neko::Insets()));
    float th = 6.0f, cy = in.cy();
    float usable = std::max(1.0f, in.w - 6.0f);
    float hx = in.x + 3.0f + usable * neko::saturate(shown_.value);
    float hw = std::max(1.0f, handleW_.value);
    float gap = 7.0f;
    neko::Color active = accent_.current(), track = track_.current(), handle = handle_.current();
    float wl = 30.0f, amp = amp_.value;
    float leftEnd = hx - hw * 0.5f - gap;
    float len = leftEnd - in.x;
    if (len > 1.0f) {
      if (amp > 0.1f) {
        float k = 2.0f * neko::kPi / wl;
        r.wave(in.x, cy, len, amp, wl, phase_, th, active);
        r.disc({in.x, cy + amp * std::sin(phase_)}, th * 0.5f, active);
        r.disc({in.x + len, cy + amp * std::sin(k * len + phase_)}, th * 0.5f, active);
      } else {
        r.fillRRect({in.x, cy - th * 0.5f, len, th}, th * 0.5f, active);
      }
    }
    float rs = hx + hw * 0.5f + gap;
    if (rs < in.right()) r.fillRRect({rs, cy - th * 0.5f, in.right() - rs, th}, th * 0.5f, track);
    r.fillRRect({hx - hw * 0.5f, cy - 15.0f, hw, 30.0f}, hw * 0.5f, handle);
  }

 private:
  float value_ = 0;
  bool dragging_ = false, active_ = false;
  neko::Spring shown_, amp_, handleW_;
  float phase_ = 0;

  void fromX(float x) {
    neko::Rect in = rect_.inset(cs_.padding.value_or(neko::Insets()));
    float usable = std::max(1.0f, in.w - 6.0f);
    value_ = neko::saturate((x - in.x - 3.0f) / usable);
  }

  void finish() {
    if (!dragging_) return;
    dragging_ = false;
    pressed_ = false;
    if (onSeek_) onSeek_(value_);
  }
};

}
