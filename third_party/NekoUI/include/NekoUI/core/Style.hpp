#pragma once

#include "Colors.hpp"
#include "Common.hpp"

namespace neko {

enum class Unit { Auto, Px, Percent, Fill };

struct Dim {
  Unit unit = Unit::Auto;
  float value = 0;
  Dim() = default;
  Dim(float px) : unit(Unit::Px), value(px) {}
  Dim(Unit u, float v) : unit(u), value(v) {}
  static Dim autoSize() { return Dim(Unit::Auto, 0); }
  static Dim px(float v) { return Dim(Unit::Px, v); }
  static Dim percent(float v) { return Dim(Unit::Percent, v); }
  static Dim fill(float weight = 1.0f) { return Dim(Unit::Fill, weight); }
};

enum class Direction { Row, Column, Stack };
enum class Align { Start, Center, End, Stretch };
enum class Justify { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };
enum class TextAlign { Left, Center, Right };
enum class ImageFit { Contain, Cover, Fill, None };
enum class FontFace { Text, Glyph };
enum class Overflow { Visible, Clip, Scroll };
enum class WidgetState { Hovered, Checked, Focused, Pressed, Disabled };

constexpr int kStateCount = 5;

struct ColorRef {
  std::string key;
  Color literal;
  bool isLiteral = false;
  bool isSet = false;
  float alpha = 1.0f;
  std::shared_ptr<ColorRef> over;
  float overAmount = 0.0f;

  ColorRef() = default;
  ColorRef(const char* k) : key(k), isSet(true) {}
  ColorRef(std::string k) : key(std::move(k)), isSet(true) {}
  ColorRef(Color c) : literal(c), isLiteral(true), isSet(true) {}

  static ColorRef transparent() { return ColorRef(Color::transparent()); }

  ColorRef a(float f) const {
    ColorRef r = *this;
    r.alpha *= f;
    return r;
  }
  ColorRef mix(const ColorRef& other, float t) const {
    ColorRef r = *this;
    r.over = std::make_shared<ColorRef>(other);
    r.overAmount = t;
    return r;
  }
  Color resolve(const ColorRegistry& reg) const {
    Color c = isLiteral ? literal : reg.get(key);
    if (over) c = Color::lerp(c, over->resolve(reg), overAmount);
    c.a *= alpha;
    return c;
  }
};

#define NEKO_STYLE_FIELDS(X)                                                                     \
  X(Dim, width) X(Dim, height) X(float, minWidth) X(float, minHeight) X(float, maxWidth)         \
  X(float, maxHeight) X(Insets, margin) X(Insets, padding) X(float, gap) X(Direction, direction) \
  X(Align, align) X(Justify, justify) X(Align, alignSelf) X(Align, justifySelf)                  \
  X(float, offsetX) X(float, offsetY) X(ColorRef, background) X(ColorRef, color)                 \
  X(ColorRef, borderColor) X(float, borderWidth) X(float, radius) X(float, opacity)              \
  X(float, fontSize) X(FontFace, fontFace) X(TextAlign, textAlign) X(int, maxLines)              \
  X(bool, wrap) X(ImageFit, imageFit) X(float, scale) X(Overflow, overflow)                      \
  X(ColorRef, accent) X(ColorRef, track) X(ColorRef, handle)

struct Style {
#define X(T, N) std::optional<T> N;
  NEKO_STYLE_FIELDS(X)
#undef X

  void merge(const Style& o) {
#define X(T, N) if (o.N) N = o.N;
    NEKO_STYLE_FIELDS(X)
#undef X
  }
};

struct ClassStyle {
  Style base;
  Style hovered, checked, focused, pressed, disabled;

  Style& forState(WidgetState s) {
    switch (s) {
      case WidgetState::Hovered: return hovered;
      case WidgetState::Checked: return checked;
      case WidgetState::Focused: return focused;
      case WidgetState::Pressed: return pressed;
      default: return disabled;
    }
  }
  const Style& forState(WidgetState s) const { return const_cast<ClassStyle*>(this)->forState(s); }

  void merge(const ClassStyle& o) {
    base.merge(o.base);
    hovered.merge(o.hovered);
    checked.merge(o.checked);
    focused.merge(o.focused);
    pressed.merge(o.pressed);
    disabled.merge(o.disabled);
  }
};

class StyleSheet {
 public:
  ClassStyle& cls(const std::string& name) {
    ++revision_;
    return classes_[name];
  }
  void define(const std::string& name, const ClassStyle& c) {
    classes_[name].merge(c);
    ++revision_;
  }
  void define(const std::string& name, const Style& base) {
    classes_[name].base.merge(base);
    ++revision_;
  }
  const ClassStyle* find(const std::string& name) const {
    auto it = classes_.find(name);
    return it == classes_.end() ? nullptr : &it->second;
  }
  void touch() { ++revision_; }
  uint32_t revision() const { return revision_; }

 private:
  std::unordered_map<std::string, ClassStyle> classes_;
  uint32_t revision_ = 1;
};

}
