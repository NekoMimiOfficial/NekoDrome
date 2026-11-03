#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace neko {

constexpr float kInf = 1.0e9f;
constexpr float kPi = 3.14159265358979f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float saturate(float v) { return clampf(v, 0.0f, 1.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline bool isFinite(float v) { return v < kInf * 0.5f; }

struct Vec2 {
  float x = 0, y = 0;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  float length() const { return std::sqrt(x * x + y * y); }
};

struct Insets {
  float l = 0, t = 0, r = 0, b = 0;
  Insets() = default;
  Insets(float all) : l(all), t(all), r(all), b(all) {}
  Insets(float h, float v) : l(h), t(v), r(h), b(v) {}
  Insets(float l_, float t_, float r_, float b_) : l(l_), t(t_), r(r_), b(b_) {}
  float horizontal() const { return l + r; }
  float vertical() const { return t + b; }
};

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  Rect() = default;
  Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  float cx() const { return x + w * 0.5f; }
  float cy() const { return y + h * 0.5f; }
  bool empty() const { return w <= 0 || h <= 0; }
  bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
  Rect expanded(float v) const { return {x - v, y - v, w + 2 * v, h + 2 * v}; }
  Rect inset(const Insets& i) const {
    return {x + i.l, y + i.t, std::max(0.0f, w - i.horizontal()), std::max(0.0f, h - i.vertical())};
  }
  Rect intersect(const Rect& o) const {
    float x0 = std::max(x, o.x), y0 = std::max(y, o.y);
    float x1 = std::min(right(), o.right()), y1 = std::min(bottom(), o.bottom());
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
  }
};

struct Color {
  float r = 0, g = 0, b = 0, a = 255;
  Color() = default;
  Color(float r_, float g_, float b_, float a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}

  static Color hex(const std::string& s) {
    std::string h = s;
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    if (h.size() == 3) h = std::string{h[0], h[0], h[1], h[1], h[2], h[2]};
    unsigned long v = std::strtoul(h.c_str(), nullptr, 16);
    if (h.size() == 8) return Color((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
    return Color((v >> 16) & 255, (v >> 8) & 255, v & 255, 255);
  }
  static Color transparent() { return Color(0, 0, 0, 0); }
  static Color lerp(const Color& x, const Color& y, float t) {
    return Color(lerpf(x.r, y.r, t), lerpf(x.g, y.g, t), lerpf(x.b, y.b, t), lerpf(x.a, y.a, t));
  }
  Color withAlpha(float a255) const { return Color(r, g, b, a255); }
  Color scaledAlpha(float f) const { return Color(r, g, b, a * f); }
  Uint8 r8() const { return (Uint8)clampf(r + 0.5f, 0, 255); }
  Uint8 g8() const { return (Uint8)clampf(g + 0.5f, 0, 255); }
  Uint8 b8() const { return (Uint8)clampf(b + 0.5f, 0, 255); }
  Uint8 a8() const { return (Uint8)clampf(a + 0.5f, 0, 255); }
};

inline std::string encodeUtf8(uint32_t cp) {
  std::string s;
  if (cp < 0x80) {
    s += (char)cp;
  } else if (cp < 0x800) {
    s += (char)(0xC0 | (cp >> 6));
    s += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    s += (char)(0xE0 | (cp >> 12));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
  } else {
    s += (char)(0xF0 | (cp >> 18));
    s += (char)(0x80 | ((cp >> 12) & 0x3F));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
  }
  return s;
}

}
