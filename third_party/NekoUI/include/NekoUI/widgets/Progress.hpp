#pragma once

#include "Widget.hpp"

namespace neko {

class ProgressBar : public WidgetT<ProgressBar> {
 public:
  const char* typeName() const override { return "progress"; }

  ProgressBar() {
    shown_.motion = Motion::snappy();
    amp_.motion = Motion::gentle();
  }

  static Ptr make(float value = 0.0f) {
    auto p = std::make_shared<ProgressBar>();
    p->value_ = saturate(value);
    p->shown_.snap(p->value_);
    return p;
  }

  Ptr value(float v) { value_ = saturate(v); return self(); }
  float value() const { return value_; }
  Ptr wavy(bool w = true) { wavy_ = w; return self(); }
  Ptr indeterminate(bool v = true) { indeterminate_ = v; return self(); }
  Ptr thickness(float t) { thickness_ = t; return self(); }
  Ptr amplitude(float a) { amplitude_ = a; return self(); }
  Ptr waving(bool v) { waving_ = v; return self(); }
  Ptr wavelength(float w) { wavelength_ = w; return self(); }

  bool interactive() const override { return false; }

  Vec2 contentSize(Context&, Vec2 avail) override {
    float h = wavy_ ? thickness_ + amplitude_ * 2.0f + 2.0f : thickness_;
    return {isFinite(avail.x) ? avail.x : 200.0f, h};
  }

  void tick(Context&, float dt) override {
    shown_.to(value_);
    shown_.update(dt);
    float a = (wavy_ && waving_ && (indeterminate_ || (shown_.value > 0.02f && shown_.value < 0.995f))) ? amplitude_ : 0.0f;
    amp_.to(a);
    amp_.update(dt);
    if (waving_) phase_ += dt * 5.0f;
    anim_ += dt;
  }

  void drawContent(Context& ctx) override {
    Renderer& r = *ctx.renderer;
    Rect in = rect_.inset(cs_.padding.value_or(Insets()));
    float th = thickness_;
    float cy = in.cy();
    float gap = 4.0f;
    Color active = accent_.current();
    Color track = track_.current();
    float s, e;
    if (indeterminate_) {
      float t = std::fmod(anim_ * 0.9f, 2.0f);
      float head = t < 1.0f ? (1.0f - std::pow(1.0f - t, 2.2f)) : 1.0f;
      float tail = t < 1.0f ? 0.0f : 1.0f - std::pow(2.0f - t, 2.2f);
      s = in.w * (tail * 0.9f);
      e = in.w * head;
      if (e - s < th) e = s + th;
    } else {
      s = 0.0f;
      e = in.w * saturate(shown_.value);
    }
    if (s > gap) r.fillRRect({in.x, cy - th * 0.5f, s - gap, th}, th * 0.5f, track);
    float tx = in.x + e + gap;
    if (tx < in.right()) r.fillRRect({tx, cy - th * 0.5f, in.right() - tx, th}, th * 0.5f, track);
    if (e - s <= 0.5f) return;
    float amp = amp_.value;
    if (amp > 0.1f) {
      float x0 = in.x + s, len = e - s;
      float k = 2.0f * kPi / wavelength_;
      r.wave(x0, cy, len, amp, wavelength_, phase_, th, active);
      r.disc({x0, cy + amp * std::sin(phase_)}, th * 0.5f, active);
      r.disc({x0 + len, cy + amp * std::sin(k * len + phase_)}, th * 0.5f, active);
    } else {
      r.fillRRect({in.x + s, cy - th * 0.5f, e - s, th}, th * 0.5f, active);
    }
    if (!indeterminate_ && e < in.w - 2.0f * th - gap) r.disc({in.right() - th * 0.5f, cy}, th * 0.5f - 1.0f > 1.0f ? 2.0f : 1.0f, active);
  }

 private:
  float value_ = 0, thickness_ = 6.0f, amplitude_ = 3.0f, wavelength_ = 28.0f;
  bool wavy_ = false, indeterminate_ = false, waving_ = true;
  Spring shown_, amp_;
  float phase_ = 0, anim_ = 0;
};

class CircularProgress : public WidgetT<CircularProgress> {
 public:
  const char* typeName() const override { return "circular-progress"; }

  CircularProgress() { shown_.motion = Motion::snappy(); }

  static Ptr make(float value = 0.0f) {
    auto p = std::make_shared<CircularProgress>();
    p->value_ = saturate(value);
    p->shown_.snap(p->value_);
    return p;
  }

  Ptr value(float v) { value_ = saturate(v); return self(); }
  Ptr wavy(bool w = true) { wavy_ = w; return self(); }
  Ptr indeterminate(bool v = true) { indeterminate_ = v; return self(); }
  Ptr thickness(float t) { thickness_ = t; return self(); }

  bool interactive() const override { return false; }
  Vec2 contentSize(Context&, Vec2) override { return {48.0f, 48.0f}; }

  void tick(Context&, float dt) override {
    shown_.to(value_);
    shown_.update(dt);
    anim_ += dt;
  }

  void drawContent(Context& ctx) override {
    Renderer& r = *ctx.renderer;
    Rect in = rect_.inset(cs_.padding.value_or(Insets()));
    float d = std::min(in.w, in.h);
    float rad = d * 0.5f - thickness_ * 0.5f - (wavy_ ? 1.5f : 0.0f);
    Vec2 c{in.cx(), in.cy()};
    r.arc(c, rad, 0.0f, 2.0f * kPi, thickness_, track_.current());
    float start, sweep;
    if (indeterminate_) {
      float t = anim_ * 1.4f;
      start = t * 2.0f * kPi * 0.9f;
      sweep = (0.12f + 0.55f * (0.5f + 0.5f * std::sin(t * 2.0f))) * 2.0f * kPi;
    } else {
      start = -kPi * 0.5f;
      sweep = saturate(shown_.value) * 2.0f * kPi;
    }
    if (sweep < 0.02f) return;
    r.arc(c, rad, start, sweep, thickness_, accent_.current(), wavy_ ? 1.5f : 0.0f, 9.0f, anim_ * 4.0f);
  }

 private:
  float value_ = 0, thickness_ = 5.0f;
  bool wavy_ = false, indeterminate_ = false;
  Spring shown_;
  float anim_ = 0;
};

}
