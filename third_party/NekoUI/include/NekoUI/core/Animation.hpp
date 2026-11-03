#pragma once

#include "Common.hpp"

namespace neko {

struct Motion {
  float stiffness = 560.0f;
  float ratio = 1.0f;
  static Motion bouncy() { return {520.0f, 0.5f}; }
  static Motion smooth() { return {560.0f, 1.0f}; }
  static Motion snappy() { return {900.0f, 0.95f}; }
  static Motion gentle() { return {220.0f, 0.85f}; }
};

struct Spring {
  float value = 0, velocity = 0, target = 0, eps = 0.002f;
  Motion motion = Motion::smooth();

  Spring() = default;
  explicit Spring(float v) : value(v), target(v) {}

  void snap(float v) { value = target = v; velocity = 0; }
  void to(float t) { target = t; }
  bool settled() const { return std::fabs(target - value) < eps && std::fabs(velocity) < eps; }

  void update(float dt) {
    if (settled()) { value = target; velocity = 0; return; }
    dt = std::min(dt, 0.05f);
    int steps = std::max(1, (int)std::ceil(dt / 0.004f));
    float h = dt / steps;
    float damping = 2.0f * motion.ratio * std::sqrt(motion.stiffness);
    for (int i = 0; i < steps; ++i) {
      float a = motion.stiffness * (target - value) - damping * velocity;
      velocity += a * h;
      value += velocity * h;
    }
  }
};

struct ColorSpring {
  Spring r, g, b, a;

  ColorSpring() {
    for (Spring* s : {&r, &g, &b, &a}) s->eps = 0.2f;
  }

  void snap(const Color& c) { r.snap(c.r); g.snap(c.g); b.snap(c.b); a.snap(c.a); }

  void to(Color t) {
    if (a.value < 1.0f && a.velocity < 1.0f) {
      r.snap(t.r); g.snap(t.g); b.snap(t.b);
    }
    if (t.a < 1.0f) { t.r = r.value; t.g = g.value; t.b = b.value; }
    r.to(t.r); g.to(t.g); b.to(t.b); a.to(t.a);
  }

  void update(float dt) { r.update(dt); g.update(dt); b.update(dt); a.update(dt); }

  Color current() const {
    return Color(clampf(r.value, 0, 255), clampf(g.value, 0, 255), clampf(b.value, 0, 255), clampf(a.value, 0, 255));
  }
};

}
