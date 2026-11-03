#pragma once

#include "../core/Animation.hpp"
#include "../core/Context.hpp"

namespace neko {

struct WidgetWatcher {
  virtual ~WidgetWatcher() = default;
  virtual void widgetDestroyed(Widget* w) = 0;
};

inline std::vector<WidgetWatcher*>& widgetWatchers() {
  static std::vector<WidgetWatcher*> w;
  return w;
}

using WidgetPtr = std::shared_ptr<Widget>;

class Widget : public std::enable_shared_from_this<Widget> {
 public:
  virtual ~Widget() {
    for (WidgetWatcher* w : std::vector<WidgetWatcher*>(widgetWatchers())) w->widgetDestroyed(this);
  }

  virtual const char* typeName() const = 0;

  std::string widgetId_;
  std::vector<std::string> classes_;
  Style inline_;
  Style inlineStates_[kStateCount];
  Rect rect_;
  Vec2 measured_;
  Widget* parent_ = nullptr;
  std::vector<WidgetPtr> children_;

  bool visible_ = true, disabled_ = false, hovered_ = false, focused_ = false;
  bool pressed_ = false, checked_ = false, focusable_ = false;
  float flash_ = 0.0f;

  Style cs_;
  bool styleDirty_ = true, colorsDirty_ = true, visualsInit_ = false, radiusInit_ = false;
  unsigned lastMask_ = ~0u;
  uint32_t lastSheetRev_ = 0, lastColorRev_ = 0;
  Color bgT_, borderT_, fgT_, accentT_, trackT_, handleT_;
  ColorSpring bg_, border_, fg_, accent_, track_, handle_;
  Spring scale_, radius_, opacity_, focusAnim_;
  float fontSize_ = 16.0f;
  FontFace fontFace_ = FontFace::Text;

  std::function<void()> onClick_, onFocus_, onBlur_;
  std::function<void(bool)> onHover_;
  std::string emit_;

  Widget() {
    scale_.snap(1.0f);
    scale_.motion = Motion::bouncy();
    scale_.eps = 0.0005f;
    radius_.motion = Motion::bouncy();
    radius_.eps = 0.02f;
    opacity_.snap(1.0f);
    opacity_.eps = 0.002f;
    focusAnim_.motion = Motion::bouncy();
    focusAnim_.eps = 0.002f;
  }

  const std::string& id() const { return widgetId_; }
  const Rect& rect() const { return rect_; }
  bool visible() const { return visible_; }
  bool disabled() const { return disabled_; }
  bool checked() const { return checked_; }
  bool focused() const { return focused_; }
  bool hovered() const { return hovered_; }
  bool pressed() const { return pressed_; }
  Widget* parent() const { return parent_; }
  const std::vector<WidgetPtr>& children() const { return children_; }
  const Style& computed() const { return cs_; }

  void addChild(WidgetPtr c) {
    c->parent_ = this;
    children_.push_back(std::move(c));
  }
  void insertChild(size_t index, WidgetPtr c) {
    c->parent_ = this;
    children_.insert(children_.begin() + (long)std::min(index, children_.size()), std::move(c));
  }
  void removeChild(Widget* c) {
    for (size_t i = 0; i < children_.size(); ++i) {
      if (children_[i].get() == c) {
        WidgetPtr keep = children_[i];
        children_.erase(children_.begin() + (long)i);
        keep->parent_ = nullptr;
        return;
      }
    }
  }
  void clearChildren() {
    std::vector<WidgetPtr> old;
    old.swap(children_);
    for (auto& c : old) c->parent_ = nullptr;
  }

  Widget* findById(const std::string& id) {
    if (widgetId_ == id) return this;
    for (auto& c : children_)
      if (Widget* f = c->findById(id)) return f;
    return nullptr;
  }
  template <class T>
  T* find(const std::string& id) {
    return dynamic_cast<T*>(findById(id));
  }

  virtual float spacerWeight() const { return 0.0f; }
  virtual bool interactive() const { return focusable_ || (bool)onClick_; }
  virtual bool clipsChildren() const { return cs_.overflow && *cs_.overflow != Overflow::Visible; }
  virtual Vec2 contentSize(Context&, Vec2) { return {0, 0}; }
  virtual void layoutChildren(Context&, Rect) {}
  virtual void tick(Context&, float) {}
  virtual void drawContent(Context&) {}
  virtual void drawOverlay(Context&) {}
  virtual bool onAction(Context& ctx, Action a) {
    if (a == Action::Accept && !disabled_ && (onClick_ || !emit_.empty())) {
      flash_ = 0.14f;
      click(ctx);
      return true;
    }
    return false;
  }
  virtual void click(Context& ctx) {
    if (disabled_) return;
    if (onClick_) onClick_();
    if (!emit_.empty() && ctx.signals) ctx.signals->emit(emit_);
  }
  virtual void onPointerDown(Context&, Vec2) { pressed_ = true; }
  virtual void onPointerMove(Context&, Vec2) {}
  virtual void onPointerUp(Context& ctx, Vec2, bool inside) {
    bool wasPressed = pressed_;
    pressed_ = false;
    if (wasPressed && inside) click(ctx);
  }
  virtual void onPointerCancel(Context&) { pressed_ = false; }
  virtual bool capturesDrag() const { return false; }
  virtual bool wantsDrag(Vec2) const { return false; }
  virtual bool scrollBy(Context&, Vec2) { return false; }
  virtual bool ensureChildVisible(const Rect&) { return false; }

  virtual Widget* hitTest(Vec2 p) {
    if (!visible_ || !rect_.contains(p)) return nullptr;
    if (clipsChildren() && !rect_.contains(p)) return nullptr;
    for (size_t i = children_.size(); i-- > 0;) {
      if (Widget* h = children_[i]->hitTest(p)) return h;
    }
    return this;
  }

  Vec2 measure(Context& ctx, Vec2 avail) {
    if (!visible_) {
      measured_ = {0, 0};
      return measured_;
    }
    Insets pad = cs_.padding.value_or(Insets());
    float w = 0, h = 0;
    bool autoW = true, autoH = true;
    if (cs_.width) {
      const Dim& d = *cs_.width;
      if (d.unit == Unit::Px) { w = d.value; autoW = false; }
      else if (d.unit == Unit::Percent && isFinite(avail.x)) { w = avail.x * d.value / 100.0f; autoW = false; }
      else if (d.unit == Unit::Fill && isFinite(avail.x)) { w = avail.x; autoW = false; }
    }
    if (cs_.height) {
      const Dim& d = *cs_.height;
      if (d.unit == Unit::Px) { h = d.value; autoH = false; }
      else if (d.unit == Unit::Percent && isFinite(avail.y)) { h = avail.y * d.value / 100.0f; autoH = false; }
      else if (d.unit == Unit::Fill && isFinite(avail.y)) { h = avail.y; autoH = false; }
    }
    if (autoW || autoH) {
      float aw = autoW ? avail.x : w, ah = autoH ? avail.y : h;
      Vec2 inner{isFinite(aw) ? std::max(0.0f, aw - pad.horizontal()) : kInf,
                 isFinite(ah) ? std::max(0.0f, ah - pad.vertical()) : kInf};
      Vec2 c = contentSize(ctx, inner);
      if (autoW) w = c.x + pad.horizontal();
      if (autoH) h = c.y + pad.vertical();
    }
    if (cs_.minWidth) w = std::max(w, *cs_.minWidth);
    if (cs_.minHeight) h = std::max(h, *cs_.minHeight);
    if (cs_.maxWidth) w = std::min(w, *cs_.maxWidth);
    if (cs_.maxHeight) h = std::min(h, *cs_.maxHeight);
    measured_ = {w, h};
    return measured_;
  }

  void arrange(Context& ctx, Rect r) {
    r.x += cs_.offsetX.value_or(0.0f);
    r.y += cs_.offsetY.value_or(0.0f);
    rect_ = r;
    Insets pad = cs_.padding.value_or(Insets());
    layoutChildren(ctx, r.inset(pad));
  }

  void updateTree(Context& ctx, float dt) {
    refreshStyle(ctx);
    stepVisuals(ctx, dt);
    if (flash_ > 0.0f) {
      flash_ -= dt;
      pressed_ = flash_ > 0.0f;
    }
    tick(ctx, dt);
    for (size_t i = 0; i < children_.size(); ++i) children_[i]->updateTree(ctx, dt);
  }

  void draw(Context& ctx) {
    if (!visible_) return;
    Renderer& r = *ctx.renderer;
    float op = clampf(opacity_.value, 0.0f, 1.0f);
    if (op <= 0.004f) return;
    float sc = scale_.value;
    bool xf = std::fabs(sc - 1.0f) > 0.0005f || op < 0.998f;
    if (xf) r.pushLayer({rect_.cx(), rect_.cy()}, sc, {0, 0}, op);
    drawBox(ctx);
    drawContent(ctx);
    bool clip = clipsChildren();
    if (clip) r.pushClip(rect_);
    for (size_t i = 0; i < children_.size(); ++i) children_[i]->draw(ctx);
    if (clip) r.popClip();
    drawOverlay(ctx);
    drawFocus(ctx);
    if (xf) r.popLayer();
  }

  Color bgColor() const { return bg_.current(); }
  Color fgColor() const { return fg_.current(); }

  void markStyleDirty() { styleDirty_ = true; }


 protected:
  void refreshStyle(Context& ctx) {
    bool showFocus = focused_ && ctx.showFocus;
    unsigned mask = (hovered_ ? 1u : 0u) | (checked_ ? 2u : 0u) | (showFocus ? 4u : 0u) | (pressed_ ? 8u : 0u) |
                    (disabled_ ? 16u : 0u);
    if (!styleDirty_ && mask == lastMask_ && ctx.styles->revision() == lastSheetRev_) return;
    Style s;
    const bool active[kStateCount] = {hovered_, checked_, showFocus, pressed_, disabled_};
    auto apply = [&](const ClassStyle* cl) {
      if (!cl) return;
      s.merge(cl->base);
      for (int i = 0; i < kStateCount; ++i)
        if (active[i]) s.merge(cl->forState((WidgetState)i));
    };
    std::string type = typeName();
    apply(ctx.styles->find(type));
    for (const std::string& c : classes_) {
      apply(ctx.styles->find(c));
      apply(ctx.styles->find(type + "." + c));
    }
    s.merge(inline_);
    for (int i = 0; i < kStateCount; ++i)
      if (active[i]) s.merge(inlineStates_[i]);
    cs_ = std::move(s);
    styleDirty_ = false;
    lastMask_ = mask;
    lastSheetRev_ = ctx.styles->revision();
    colorsDirty_ = true;
  }

  static Color resolveOpt(const ColorRegistry& reg, const std::optional<ColorRef>& c, const Color& fallback) {
    return c ? c->resolve(reg) : fallback;
  }

  void stepVisuals(Context& ctx, float dt) {
    const ColorRegistry& reg = *ctx.colors;
    if (colorsDirty_ || lastColorRev_ != reg.revision()) {
      bgT_ = resolveOpt(reg, cs_.background, Color::transparent());
      borderT_ = resolveOpt(reg, cs_.borderColor, Color::transparent());
      accentT_ = resolveOpt(reg, cs_.accent, reg.get("primary"));
      trackT_ = resolveOpt(reg, cs_.track, reg.get("surfaceContainerHighest"));
      handleT_ = resolveOpt(reg, cs_.handle, reg.get("primary"));
      if (cs_.color) fgT_ = cs_.color->resolve(reg);
      colorsDirty_ = false;
      lastColorRev_ = reg.revision();
      if (!visualsInit_) {
        bg_.snap(bgT_);
        border_.snap(borderT_);
        accent_.snap(accentT_);
        track_.snap(trackT_);
        handle_.snap(handleT_);
        fg_.snap(cs_.color ? fgT_ : reg.get("onSurface"));
        visualsInit_ = true;
      }
      bg_.to(bgT_);
      border_.to(borderT_);
      accent_.to(accentT_);
      track_.to(trackT_);
      handle_.to(handleT_);
      if (cs_.color) fg_.to(fgT_);
    }
    if (!cs_.color) {
      if (parent_) fg_.snap(parent_->fg_.current());
      else fg_.to(reg.get("onSurface"));
    }
    fontSize_ = cs_.fontSize ? *cs_.fontSize : (parent_ ? parent_->fontSize_ : ctx.baseFontSize);
    fontFace_ = cs_.fontFace ? *cs_.fontFace : (parent_ ? parent_->fontFace_ : FontFace::Text);

    float maxR = std::min(rect_.w, rect_.h) * 0.5f;
    float rt = std::min(cs_.radius.value_or(0.0f), maxR);
    if (!radiusInit_ && !rect_.empty()) {
      radius_.snap(rt);
      radiusInit_ = true;
    }
    radius_.to(rt);
    scale_.to(cs_.scale.value_or(1.0f));
    opacity_.to(cs_.opacity.value_or(1.0f) * (disabled_ ? 0.38f : 1.0f));
    focusAnim_.to(focused_ && ctx.showFocus ? 1.0f : 0.0f);

    bg_.update(dt);
    border_.update(dt);
    fg_.update(dt);
    accent_.update(dt);
    track_.update(dt);
    handle_.update(dt);
    scale_.update(dt);
    radius_.update(dt);
    opacity_.update(dt);
    focusAnim_.update(dt);
  }

  void drawBox(Context& ctx) {
    Renderer& r = *ctx.renderer;
    float rad = std::max(0.0f, radius_.value);
    Color bg = bg_.current();
    if (bg.a > 0.5f) r.fillRRect(rect_, rad, bg);
    float bw = cs_.borderWidth.value_or(0.0f);
    if (bw > 0.0f) {
      Color bc = border_.current();
      if (bc.a > 0.5f) r.strokeRRect(rect_, rad, bw, bc);
    }
  }

  void drawFocus(Context& ctx) {
    float f = focusAnim_.value;
    if (f <= 0.01f) return;
    Renderer& r = *ctx.renderer;
    float off = 3.0f * f;
    Color c = ctx.colors->get("focusRing");
    c.a *= saturate(f * 1.4f);
    r.strokeRRect(rect_.expanded(off), std::max(0.0f, radius_.value) + off, 2.5f, c);
  }
};

template <class D, class B = Widget>
class WidgetT : public B {
 public:
  using Ptr = std::shared_ptr<D>;
  Ptr self() { return std::static_pointer_cast<D>(this->shared_from_this()); }

  Ptr id(std::string v) { this->widgetId_ = std::move(v); return self(); }
  Ptr cls(const std::string& names) {
    size_t i = 0;
    while (i < names.size()) {
      size_t j = names.find(' ', i);
      if (j == std::string::npos) j = names.size();
      if (j > i) this->classes_.push_back(names.substr(i, j - i));
      i = j + 1;
    }
    this->styleDirty_ = true;
    return self();
  }
  Ptr style(const std::function<void(Style&)>& f) { f(this->inline_); this->styleDirty_ = true; return self(); }
  Ptr on(WidgetState s, const std::function<void(Style&)>& f) {
    f(this->inlineStates_[(int)s]);
    this->styleDirty_ = true;
    return self();
  }
  Ptr hovered(const std::function<void(Style&)>& f) { return on(WidgetState::Hovered, f); }
  Ptr focused(const std::function<void(Style&)>& f) { return on(WidgetState::Focused, f); }
  Ptr pressed(const std::function<void(Style&)>& f) { return on(WidgetState::Pressed, f); }
  Ptr checkedStyle(const std::function<void(Style&)>& f) { return on(WidgetState::Checked, f); }

  Ptr width(Dim d) { this->inline_.width = d; this->styleDirty_ = true; return self(); }
  Ptr height(Dim d) { this->inline_.height = d; this->styleDirty_ = true; return self(); }
  Ptr size(float w, float h) { this->inline_.width = Dim::px(w); this->inline_.height = Dim::px(h); this->styleDirty_ = true; return self(); }
  Ptr fill() { this->inline_.width = Dim::fill(); this->inline_.height = Dim::fill(); this->styleDirty_ = true; return self(); }
  Ptr fillWidth(float weight = 1.0f) { this->inline_.width = Dim::fill(weight); this->styleDirty_ = true; return self(); }
  Ptr fillHeight(float weight = 1.0f) { this->inline_.height = Dim::fill(weight); this->styleDirty_ = true; return self(); }
  Ptr minSize(float w, float h) { this->inline_.minWidth = w; this->inline_.minHeight = h; this->styleDirty_ = true; return self(); }
  Ptr maxSize(float w, float h) { this->inline_.maxWidth = w; this->inline_.maxHeight = h; this->styleDirty_ = true; return self(); }
  Ptr margin(Insets v) { this->inline_.margin = v; this->styleDirty_ = true; return self(); }
  Ptr margin(float h, float v) { return margin(Insets(h, v)); }
  Ptr padding(Insets v) { this->inline_.padding = v; this->styleDirty_ = true; return self(); }
  Ptr padding(float h, float v) { return padding(Insets(h, v)); }
  Ptr gap(float v) { this->inline_.gap = v; this->styleDirty_ = true; return self(); }
  Ptr align(Align v) { this->inline_.align = v; this->styleDirty_ = true; return self(); }
  Ptr justify(Justify v) { this->inline_.justify = v; this->styleDirty_ = true; return self(); }
  Ptr alignSelf(Align v) { this->inline_.alignSelf = v; this->styleDirty_ = true; return self(); }
  Ptr justifySelf(Align v) { this->inline_.justifySelf = v; this->styleDirty_ = true; return self(); }
  Ptr offset(float x, float y) { this->inline_.offsetX = x; this->inline_.offsetY = y; this->styleDirty_ = true; return self(); }
  Ptr bg(ColorRef c) { this->inline_.background = std::move(c); this->styleDirty_ = true; return self(); }
  Ptr fg(ColorRef c) { this->inline_.color = std::move(c); this->styleDirty_ = true; return self(); }
  Ptr accent(ColorRef c) { this->inline_.accent = std::move(c); this->styleDirty_ = true; return self(); }
  Ptr track(ColorRef c) { this->inline_.track = std::move(c); this->styleDirty_ = true; return self(); }
  Ptr handle(ColorRef c) { this->inline_.handle = std::move(c); this->styleDirty_ = true; return self(); }
  Ptr border(float w, ColorRef c) {
    this->inline_.borderWidth = w;
    this->inline_.borderColor = std::move(c);
    this->styleDirty_ = true;
    return self();
  }
  Ptr radius(float v) { this->inline_.radius = v; this->styleDirty_ = true; return self(); }
  Ptr opacity(float v) { this->inline_.opacity = v; this->styleDirty_ = true; return self(); }
  Ptr scale(float v) { this->inline_.scale = v; this->styleDirty_ = true; return self(); }
  Ptr fontSize(float v) { this->inline_.fontSize = v; this->styleDirty_ = true; return self(); }
  Ptr fontFace(FontFace v) { this->inline_.fontFace = v; this->styleDirty_ = true; return self(); }
  Ptr direction(Direction v) { this->inline_.direction = v; this->styleDirty_ = true; return self(); }
  Ptr overflow(Overflow v) { this->inline_.overflow = v; this->styleDirty_ = true; return self(); }

  Ptr visible(bool v) { this->visible_ = v; return self(); }
  Ptr disabled(bool v) { this->disabled_ = v; return self(); }
  Ptr focusable(bool v) { this->focusable_ = v; return self(); }
  Ptr checked(bool v) { this->checked_ = v; return self(); }
  Ptr onClick(std::function<void()> f) { this->onClick_ = std::move(f); return self(); }
  Ptr onFocus(std::function<void()> f) { this->onFocus_ = std::move(f); return self(); }
  Ptr onBlur(std::function<void()> f) { this->onBlur_ = std::move(f); return self(); }
  Ptr onHover(std::function<void(bool)> f) { this->onHover_ = std::move(f); return self(); }
  Ptr emits(std::string signal) { this->emit_ = std::move(signal); return self(); }

  using B::id;
  using B::disabled;
  using B::checked;
  using B::visible;
  using B::focused;
  using B::hovered;
  using B::pressed;
};

inline void FocusManager::set(Widget* w) {
  if (current_ == w) return;
  Widget* old = current_;
  current_ = w;
  if (old) {
    old->focused_ = false;
    if (old->onBlur_) old->onBlur_();
  }
  if (w) {
    w->focused_ = true;
    for (Widget* p = w->parent_; p; p = p->parent_) p->ensureChildVisible(w->rect_);
    if (w->onFocus_) w->onFocus_();
  }
}

inline void collectFocusable(Widget* w, std::vector<Widget*>& out) {
  if (!w->visible_) return;
  if (w->focusable_ && !w->disabled_ && !w->rect_.empty()) out.push_back(w);
  for (auto& c : w->children_) collectFocusable(c.get(), out);
}

inline Widget* FocusManager::first(Widget* root) {
  std::vector<Widget*> list;
  if (root) collectFocusable(root, list);
  Widget* best = nullptr;
  for (Widget* w : list) {
    if (!best || w->rect_.y < best->rect_.y - 1.0f || (std::fabs(w->rect_.y - best->rect_.y) <= 1.0f && w->rect_.x < best->rect_.x))
      best = w;
  }
  return best;
}

inline void FocusManager::ensureValid(Widget* root) {
  if (!root) return;
  bool ok = false;
  if (current_) {
    for (Widget* p = current_; p; p = p->parent_) {
      if (p == root) { ok = true; break; }
    }
    if (ok) {
      for (Widget* p = current_; p; p = p->parent_)
        if (!p->visible_) { ok = false; break; }
    }
    if (ok && (current_->disabled_ || !current_->focusable_)) ok = false;
  }
  if (!ok) set(first(root));
}

inline bool FocusManager::move(Widget* root, Action dir) {
  if (!root) return false;
  std::vector<Widget*> list;
  collectFocusable(root, list);
  if (list.empty()) return false;
  if (!current_) {
    set(first(root));
    return current_ != nullptr;
  }
  const Rect c = current_->rect_;
  Widget* best = nullptr;
  float bestScore = 1.0e18f;
  for (Widget* w : list) {
    if (w == current_) continue;
    const Rect r = w->rect_;
    float dx = r.cx() - c.cx(), dy = r.cy() - c.cy();
    float primary, secondary, gap;
    switch (dir) {
      case Action::Right:
        if (dx <= 1.0f || r.right() <= c.right() + 0.5f) continue;
        primary = dx; secondary = std::fabs(dy); gap = std::max(0.0f, r.x - c.right());
        break;
      case Action::Left:
        if (dx >= -1.0f || r.x >= c.x - 0.5f) continue;
        primary = -dx; secondary = std::fabs(dy); gap = std::max(0.0f, c.x - r.right());
        break;
      case Action::Down:
        if (dy <= 1.0f || r.bottom() <= c.bottom() + 0.5f) continue;
        primary = dy; secondary = std::fabs(dx); gap = std::max(0.0f, r.y - c.bottom());
        break;
      case Action::Up:
        if (dy >= -1.0f || r.y >= c.y - 0.5f) continue;
        primary = -dy; secondary = std::fabs(dx); gap = std::max(0.0f, c.y - r.bottom());
        break;
      default: return false;
    }
    bool horiz = dir == Action::Left || dir == Action::Right;
    float crossOverlap = horiz ? std::min(c.bottom(), r.bottom()) - std::max(c.y, r.y)
                               : std::min(c.right(), r.right()) - std::max(c.x, r.x);
    float score = gap * 1.0f + primary * 0.35f + secondary * (crossOverlap > 0 ? 1.2f : 3.0f);
    if (score < bestScore) {
      bestScore = score;
      best = w;
    }
  }
  if (!best) return false;
  set(best);
  return true;
}

}
