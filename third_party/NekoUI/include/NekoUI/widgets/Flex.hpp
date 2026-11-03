#pragma once

#include "Widget.hpp"

namespace neko {

class Flex : public Widget {
 public:
  Vec2 contentSize(Context& ctx, Vec2 avail) override { return run(ctx, avail, nullptr); }
  void layoutChildren(Context& ctx, Rect inner) override {
    Vec2 avail{inner.w, inner.h};
    run(ctx, avail, &inner);
  }

 protected:
  struct Slot {
    Widget* w;
    Vec2 size;
    Insets m;
    float weight;
  };

  static bool dimIsAuto(const std::optional<Dim>& d) { return !d || d->unit == Unit::Auto; }

  Vec2 run(Context& ctx, Vec2 avail, const Rect* place) {
    Direction dir = cs_.direction.value_or(Direction::Column);
    float gap = cs_.gap.value_or(0.0f);
    std::vector<Slot> slots;
    slots.reserve(children_.size());
    for (auto& c : children_)
      if (c->visible_) slots.push_back({c.get(), {0, 0}, c->cs_.margin.value_or(Insets()), 0.0f});
    if (dir == Direction::Stack) return runStack(ctx, slots, avail, place);

    const bool row = dir == Direction::Row;
    const float availMain = row ? avail.x : avail.y;
    const float availCross = row ? avail.y : avail.x;
    const bool finiteMain = isFinite(availMain);
    float used = 0, totalWeight = 0, crossMax = 0;
    const float gaps = slots.empty() ? 0.0f : gap * (float)(slots.size() - 1);

    for (Slot& s : slots) {
      const std::optional<Dim>& d = row ? s.w->cs_.width : s.w->cs_.height;
      float mm = row ? s.m.horizontal() : s.m.vertical();
      float sw = s.w->spacerWeight();
      if (finiteMain && (sw > 0.0f || (d && d->unit == Unit::Fill))) {
        s.weight = sw > 0.0f ? sw : std::max(d->value, 0.0001f);
        totalWeight += s.weight;
        used += mm;
        continue;
      }
      float cm = row ? s.m.vertical() : s.m.horizontal();
      float remain = finiteMain ? std::max(0.0f, availMain - used - mm - gaps) : kInf;
      float crossAv = isFinite(availCross) ? std::max(0.0f, availCross - cm) : kInf;
      s.size = s.w->measure(ctx, row ? Vec2{remain, crossAv} : Vec2{crossAv, remain});
      used += (row ? s.size.x : s.size.y) + mm;
    }
    used += gaps;
    float remaining = finiteMain ? std::max(0.0f, availMain - used) : 0.0f;
    for (Slot& s : slots) {
      if (s.weight <= 0.0f) continue;
      float allot = remaining * s.weight / totalWeight;
      float cm = row ? s.m.vertical() : s.m.horizontal();
      float crossAv = isFinite(availCross) ? std::max(0.0f, availCross - cm) : kInf;
      s.size = s.w->measure(ctx, row ? Vec2{allot, crossAv} : Vec2{crossAv, allot});
      if (s.w->spacerWeight() > 0.0f) {
        if (row) s.size = {allot, 0.0f};
        else s.size = {0.0f, allot};
      }
      used += row ? s.size.x : s.size.y;
    }
    for (Slot& s : slots) {
      float cm = row ? s.m.vertical() : s.m.horizontal();
      crossMax = std::max(crossMax, (row ? s.size.y : s.size.x) + cm);
    }
    if (!place) return row ? Vec2{used, crossMax} : Vec2{crossMax, used};

    const Rect& in = *place;
    const float innerMain = row ? in.w : in.h;
    const float innerCross = row ? in.h : in.w;
    float freeSpace = innerMain - used;
    float start = 0, extra = 0;
    const size_t n = slots.size();
    if (totalWeight <= 0.0f && freeSpace > 0.0f && n > 0) {
      switch (cs_.justify.value_or(Justify::Start)) {
        case Justify::Center: start = freeSpace * 0.5f; break;
        case Justify::End: start = freeSpace; break;
        case Justify::SpaceBetween: extra = n > 1 ? freeSpace / (float)(n - 1) : 0.0f; break;
        case Justify::SpaceAround: extra = freeSpace / (float)n; start = extra * 0.5f; break;
        case Justify::SpaceEvenly: extra = freeSpace / (float)(n + 1); start = extra; break;
        default: break;
      }
    }
    float pos = (row ? in.x : in.y) + start;
    for (Slot& s : slots) {
      Align a = s.w->cs_.alignSelf.value_or(cs_.align.value_or(Align::Start));
      float cm0 = row ? s.m.t : s.m.l, cm1 = row ? s.m.b : s.m.r;
      float mm0 = row ? s.m.l : s.m.t, mm1 = row ? s.m.r : s.m.b;
      float crossSize = row ? s.size.y : s.size.x;
      float crossAvail = std::max(0.0f, innerCross - cm0 - cm1);
      const std::optional<Dim>& cd = row ? s.w->cs_.height : s.w->cs_.width;
      if (a == Align::Stretch && dimIsAuto(cd)) crossSize = crossAvail;
      float crossOff = a == Align::Center ? (crossAvail - crossSize) * 0.5f : (a == Align::End ? crossAvail - crossSize : 0.0f);
      pos += mm0;
      Rect cr = row ? Rect{pos, in.y + cm0 + crossOff, s.size.x, crossSize}
                    : Rect{in.x + cm0 + crossOff, pos, crossSize, s.size.y};
      s.w->arrange(ctx, cr);
      pos += (row ? s.size.x : s.size.y) + mm1 + gap + extra;
    }
    return {in.w, in.h};
  }

  Vec2 runStack(Context& ctx, std::vector<Slot>& slots, Vec2 avail, const Rect* place) {
    Vec2 content{0, 0};
    for (Slot& s : slots) {
      Vec2 av{isFinite(avail.x) ? std::max(0.0f, avail.x - s.m.horizontal()) : kInf,
              isFinite(avail.y) ? std::max(0.0f, avail.y - s.m.vertical()) : kInf};
      s.size = s.w->measure(ctx, av);
      content.x = std::max(content.x, s.size.x + s.m.horizontal());
      content.y = std::max(content.y, s.size.y + s.m.vertical());
    }
    if (!place) return content;
    const Rect& in = *place;
    for (Slot& s : slots) {
      Align ha = s.w->cs_.alignSelf.value_or(Align::Start);
      Align va = s.w->cs_.justifySelf.value_or(Align::Start);
      float aw = std::max(0.0f, in.w - s.m.horizontal()), ah = std::max(0.0f, in.h - s.m.vertical());
      float w = ha == Align::Stretch && dimIsAuto(s.w->cs_.width) ? aw : s.size.x;
      float h = va == Align::Stretch && dimIsAuto(s.w->cs_.height) ? ah : s.size.y;
      float x = in.x + s.m.l + (ha == Align::Center ? (aw - w) * 0.5f : (ha == Align::End ? aw - w : 0.0f));
      float y = in.y + s.m.t + (va == Align::Center ? (ah - h) * 0.5f : (va == Align::End ? ah - h : 0.0f));
      s.w->arrange(ctx, {x, y, w, h});
    }
    return {in.w, in.h};
  }
};

template <class D, class B = Flex>
class ContainerT : public WidgetT<D, B> {
 public:
  using Ptr = std::shared_ptr<D>;
  Ptr add(WidgetPtr c) {
    this->addChild(std::move(c));
    return this->self();
  }
  Ptr add(std::initializer_list<WidgetPtr> list) {
    for (const WidgetPtr& c : list) this->addChild(c);
    return this->self();
  }
  Ptr clear() {
    this->clearChildren();
    return this->self();
  }
};

class Box : public ContainerT<Box> {
 public:
  const char* typeName() const override { return "box"; }
  static Ptr make(Direction d = Direction::Column) {
    auto b = std::make_shared<Box>();
    b->inline_.direction = d;
    return b;
  }
  bool interactive() const override { return (bool)onClick_ || focusable_; }
};

class Spacer : public WidgetT<Spacer> {
 public:
  const char* typeName() const override { return "spacer"; }
  static Ptr make(float px = 0.0f) {
    auto s = std::make_shared<Spacer>();
    if (px > 0.0f) {
      s->inline_.width = Dim::px(px);
      s->inline_.height = Dim::px(px);
    } else {
      s->weight_ = 1.0f;
    }
    return s;
  }
  Ptr weight(float w) { weight_ = w; return self(); }
  float spacerWeight() const override { return weight_; }
  bool interactive() const override { return false; }
  Widget* hitTest(Vec2) override { return nullptr; }

 private:
  float weight_ = 0.0f;
};

class Divider : public WidgetT<Divider> {
 public:
  const char* typeName() const override { return "divider"; }
  static Ptr make() { return std::make_shared<Divider>(); }
  bool interactive() const override { return false; }
};

}
