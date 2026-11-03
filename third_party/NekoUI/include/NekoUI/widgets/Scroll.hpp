#pragma once

#include "Flex.hpp"

namespace neko {

enum class ScrollAxis { Vertical, Horizontal, Both };

class ScrollCore : public Flex {
 public:
  ScrollAxis axis_ = ScrollAxis::Vertical;
  float step_ = 56.0f;

  ScrollCore() {
    for (Spring* s : {&sx_, &sy_}) {
      s->motion = Motion::smooth();
      s->motion.stiffness = 420.0f;
      s->eps = 0.05f;
    }
  }

  bool interactive() const override { return true; }
  bool clipsChildren() const override { return true; }

  bool vertical() const { return axis_ != ScrollAxis::Horizontal; }
  bool horizontal() const { return axis_ != ScrollAxis::Vertical; }

  Vec2 offset() const { return {sx_.value, sy_.value}; }
  Vec2 maxOffset() const {
    return {std::max(0.0f, content_.x - view_.w), std::max(0.0f, content_.y - view_.h)};
  }
  bool canScroll() const {
    Vec2 m = maxOffset();
    return (vertical() && m.y > 0.5f) || (horizontal() && m.x > 0.5f);
  }

  void scrollTo(Vec2 target, bool animate = true) {
    Vec2 m = maxOffset();
    target.x = clampf(target.x, 0.0f, m.x);
    target.y = clampf(target.y, 0.0f, m.y);
    if (!horizontal()) target.x = sx_.value;
    if (!vertical()) target.y = sy_.value;
    sx_.to(target.x);
    sy_.to(target.y);
    if (!animate) {
      sx_.snap(target.x);
      sy_.snap(target.y);
    }
    shownFor_ = 1.2f;
  }

  bool scrollBy(Context&, Vec2 d) override {
    if (!canScroll()) return false;
    Vec2 m = maxOffset();
    Vec2 t{sx_.target + (horizontal() ? d.x : 0.0f), sy_.target + (vertical() ? d.y : 0.0f)};
    Vec2 c{clampf(t.x, 0.0f, m.x), clampf(t.y, 0.0f, m.y)};
    bool moved = std::fabs(c.x - sx_.target) > 0.01f || std::fabs(c.y - sy_.target) > 0.01f;
    sx_.to(c.x);
    sy_.to(c.y);
    shownFor_ = 1.2f;
    return moved;
  }

  bool wantsDrag(Vec2 total) const override {
    if (!canScroll()) return false;
    Vec2 m = maxOffset();
    if (vertical() && m.y > 0.5f && std::fabs(total.y) >= std::fabs(total.x)) return true;
    if (horizontal() && m.x > 0.5f && std::fabs(total.x) > std::fabs(total.y)) return true;
    return false;
  }

  void onPointerDown(Context&, Vec2 p) override {
    dragging_ = true;
    last_ = p;
    vel_ = {0, 0};
    sx_.snap(sx_.value);
    sy_.snap(sy_.value);
    sx_.to(sx_.value);
    sy_.to(sy_.value);
  }

  void onPointerMove(Context& ctx, Vec2 p) override {
    if (!dragging_) return;
    Vec2 d = p - last_;
    last_ = p;
    Vec2 m = maxOffset();
    auto apply = [&](Spring& s, float delta, float mx) {
      float v = s.value - delta;
      if (v < 0.0f || v > mx) {
        float over = v < 0.0f ? v : v - mx;
        v = s.value - delta * 0.4f;
        (void)over;
      }
      s.snap(v);
      s.to(clampf(v, 0.0f, mx));
    };
    if (vertical()) apply(sy_, d.y, m.y);
    if (horizontal()) apply(sx_, d.x, m.x);
    float dt = std::max(ctx.dt, 0.001f);
    vel_.x = lerpf(vel_.x, -d.x / dt, 0.35f);
    vel_.y = lerpf(vel_.y, -d.y / dt, 0.35f);
    shownFor_ = 1.2f;
  }

  void onPointerUp(Context&, Vec2, bool) override { release(); }
  void onPointerCancel(Context&) override { release(); }

  bool ensureChildVisible(const Rect& r) override {
    Rect v = view_;
    if (v.empty()) return false;
    float margin = 24.0f;
    Vec2 t{sx_.target, sy_.target};
    Vec2 cur{sx_.value, sy_.value};
    if (vertical()) {
      if (r.y < v.y + margin) t.y = cur.y + (r.y - v.y - margin);
      else if (r.bottom() > v.bottom() - margin) t.y = cur.y + (r.bottom() - v.bottom() + margin);
    }
    if (horizontal()) {
      if (r.x < v.x + margin) t.x = cur.x + (r.x - v.x - margin);
      else if (r.right() > v.right() - margin) t.x = cur.x + (r.right() - v.right() + margin);
    }
    scrollTo(t, true);
    return true;
  }

  Vec2 contentSize(Context& ctx, Vec2 avail) override {
    Vec2 a{horizontal() ? kInf : avail.x, vertical() ? kInf : avail.y};
    return run(ctx, a, nullptr);
  }

  void layoutChildren(Context& ctx, Rect inner) override {
    view_ = inner;
    Vec2 a{horizontal() ? kInf : inner.w, vertical() ? kInf : inner.h};
    Vec2 c = run(ctx, a, nullptr);
    content_ = c;
    Vec2 m = maxOffset();
    if (!dragging_) {
      sx_.target = clampf(sx_.target, 0.0f, m.x);
      sy_.target = clampf(sy_.target, 0.0f, m.y);
    }
    Rect place{inner.x - sx_.value, inner.y - sy_.value, std::max(c.x, inner.w), std::max(c.y, inner.h)};
    if (!horizontal()) place.w = inner.w;
    if (!vertical()) place.h = inner.h;
    run(ctx, {place.w, vertical() ? kInf : place.h}, &place);
  }

  void tick(Context&, float dt) override {
    if (!dragging_) {
      sx_.update(dt);
      sy_.update(dt);
    }
    if (shownFor_ > 0.0f) shownFor_ -= dt;
    bar_.to(shownFor_ > 0.0f || dragging_ ? 1.0f : 0.0f);
    bar_.update(dt);
  }

  void drawOverlay(Context& ctx) override {
    float a = saturate(bar_.value);
    if (a <= 0.01f || !canScroll()) return;
    Renderer& r = *ctx.renderer;
    Color c = ctx.colors->get("onSurfaceVariant");
    c.a = 150.0f * a;
    Vec2 m = maxOffset();
    if (vertical() && m.y > 0.5f) {
      float trackH = view_.h - 8.0f;
      float h = std::max(24.0f, trackH * view_.h / content_.y);
      float y = view_.y + 4.0f + (trackH - h) * saturate(sy_.value / m.y);
      r.fillRRect({view_.right() - 5.0f, y, 3.0f, h}, 1.5f, c);
    }
    if (horizontal() && m.x > 0.5f) {
      float trackW = view_.w - 8.0f;
      float w = std::max(24.0f, trackW * view_.w / content_.x);
      float x = view_.x + 4.0f + (trackW - w) * saturate(sx_.value / m.x);
      r.fillRRect({x, view_.bottom() - 5.0f, w, 3.0f}, 1.5f, c);
    }
  }

 protected:
  Spring sx_, sy_;
  Rect view_;
  Vec2 content_, last_, vel_;
  bool dragging_ = false;
  float shownFor_ = 0.0f;
  Spring bar_;

  void release() {
    if (!dragging_) return;
    dragging_ = false;
    Vec2 m = maxOffset();
    sx_.velocity = 0;
    sy_.velocity = 0;
    float kx = clampf(sx_.value + vel_.x * 0.32f, 0.0f, m.x);
    float ky = clampf(sy_.value + vel_.y * 0.32f, 0.0f, m.y);
    if (vertical() && std::fabs(vel_.y) > 80.0f) sy_.to(ky);
    else sy_.to(clampf(sy_.value, 0.0f, m.y));
    if (horizontal() && std::fabs(vel_.x) > 80.0f) sx_.to(kx);
    else sx_.to(clampf(sx_.value, 0.0f, m.x));
    sy_.velocity = vertical() ? vel_.y * 0.5f : 0.0f;
    sx_.velocity = horizontal() ? vel_.x * 0.5f : 0.0f;
    shownFor_ = 1.2f;
  }
};

class ScrollView : public ContainerT<ScrollView, ScrollCore> {
 public:
  const char* typeName() const override { return "scroll"; }

  static Ptr make(ScrollAxis axis = ScrollAxis::Vertical) {
    auto s = std::make_shared<ScrollView>();
    s->axis_ = axis;
    return s;
  }
  Ptr axis(ScrollAxis a) { axis_ = a; return self(); }
};

class ListView : public ContainerT<ListView, ScrollCore> {
 public:
  using Builder = std::function<WidgetPtr(int)>;
  const char* typeName() const override { return "list"; }

  static Ptr make(int count, float itemHeight, Builder b) {
    auto l = std::make_shared<ListView>();
    l->count_ = count;
    l->itemH_ = itemHeight;
    l->builder_ = std::move(b);
    return l;
  }

  Ptr count(int n) {
    count_ = std::max(0, n);
    reset();
    return self();
  }
  Ptr grow(int n) {
    count_ = std::max(count_, n);
    return self();
  }
  Ptr itemHeight(float h) { itemH_ = h; reset(); return self(); }
  Ptr builder(Builder b) { builder_ = std::move(b); reset(); return self(); }
  int count() const { return count_; }

  void reset() {
    items_.clear();
    clearChildren();
  }

  void scrollToIndex(int i, bool animate = true) {
    float y = (float)i * (itemH_ + cs_.gap.value_or(0.0f));
    float center = y - (view_.h - itemH_) * 0.5f;
    scrollTo({sx_.value, center}, animate);
  }

  Vec2 contentSize(Context&, Vec2 avail) override {
    return {isFinite(avail.x) ? avail.x : 0.0f, totalHeight()};
  }

  void layoutChildren(Context& ctx, Rect inner) override {
    view_ = inner;
    float gap = cs_.gap.value_or(0.0f);
    float pitch = itemH_ + gap;
    content_ = {inner.w, totalHeight()};
    Vec2 m = maxOffset();
    if (!dragging_) {
      sy_.target = clampf(sy_.target, 0.0f, m.y);
    }
    float sy = sy_.value;
    int first = pitch > 0 ? (int)std::floor(sy / pitch) - 2 : 0;
    int last = pitch > 0 ? (int)std::ceil((sy + inner.h) / pitch) + 2 : 0;
    first = std::max(0, first);
    last = std::min(count_ - 1, last);
    bool changed = false;
    for (auto it = items_.begin(); it != items_.end();) {
      bool keep = it->first >= first && it->first <= last;
      if (!keep && it->second->focused_) keep = true;
      if (!keep) {
        it = items_.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    for (int i = first; i <= last; ++i) {
      if (items_.count(i) || !builder_) continue;
      WidgetPtr w = builder_(i);
      if (!w) continue;
      w->parent_ = this;
      w->updateTree(ctx, 0.0f);
      items_[i] = std::move(w);
      changed = true;
    }
    if (changed) {
      children_.clear();
      for (auto& kv : items_) children_.push_back(kv.second);
    }
    for (auto& kv : items_) {
      Widget* w = kv.second.get();
      Rect cell{inner.x, inner.y + (float)kv.first * pitch - sy, inner.w, itemH_};
      Insets mg = w->cs_.margin.value_or(Insets());
      w->measure(ctx, {cell.w - mg.horizontal(), cell.h - mg.vertical()});
      w->arrange(ctx, {cell.x + mg.l, cell.y + mg.t, cell.w - mg.horizontal(), cell.h - mg.vertical()});
    }
  }

 private:
  int count_ = 0;
  float itemH_ = 56.0f;
  Builder builder_;
  std::map<int, WidgetPtr> items_;

  float totalHeight() const {
    float gap = cs_.gap.value_or(0.0f);
    return count_ > 0 ? (float)count_ * itemH_ + (float)(count_ - 1) * gap : 0.0f;
  }
};

}
