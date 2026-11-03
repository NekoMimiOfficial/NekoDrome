#pragma once

#include "../widgets/Button.hpp"
#include "../widgets/Flex.hpp"
#include "../widgets/Label.hpp"
#include "View.hpp"

namespace neko {

class ViewHost : public ContainerT<ViewHost> {
 public:
  const char* typeName() const override { return "view-host"; }

  ViewHost() {
    inline_.direction = Direction::Stack;
    inline_.width = Dim::fill();
    inline_.height = Dim::fill();
    t_.motion = Motion::bouncy();
    t_.motion.stiffness = 380.0f;
    t_.snap(1.0f);
  }

  bool interactive() const override { return false; }
  bool transitioning() const { return outgoing_ != nullptr; }

  void show(const ViewPtr& v, int direction) {
    if (current_ == v) return;
    if (current_ && onLeave_) onLeave_(current_);
    outgoing_ = current_ ? current_->frame_ : nullptr;
    outDir_ = direction;
    clearChildren();
    current_ = v;
    if (v) addChild(v->frame_);
    if (outgoing_) {
      t_.snap(0.0f);
      t_.to(1.0f);
      outgoing_->parent_ = nullptr;
    } else {
      t_.snap(1.0f);
    }
    if (v && v->onEnter_) v->onEnter_();
    applyOffsets();
  }

  const ViewPtr& current() const { return current_; }
  std::function<void(const ViewPtr&)> onLeave_;

  void tick(Context& ctx, float dt) override {
    t_.update(dt);
    applyOffsets();
    if (outgoing_) {
      outgoing_->updateTree(ctx, dt);
      if (t_.settled() && t_.value >= 0.999f) {
        outgoing_.reset();
        if (current_) {
          current_->frame_->inline_.offsetX = 0.0f;
          current_->frame_->inline_.opacity = 1.0f;
          current_->frame_->styleDirty_ = true;
        }
      }
    }
  }

  void layoutChildren(Context& ctx, Rect inner) override {
    Flex::layoutChildren(ctx, inner);
    if (outgoing_) {
      outgoing_->measure(ctx, {inner.w, inner.h});
      outgoing_->arrange(ctx, inner);
    }
  }

  void drawContent(Context& ctx) override {
    if (outgoing_) outgoing_->draw(ctx);
  }

 private:
  ViewPtr current_;
  std::shared_ptr<Box> outgoing_;
  Spring t_;
  int outDir_ = 1;

  void applyOffsets() {
    if (!current_) return;
    float t = t_.value;
    float dx = 72.0f;
    Box* in = current_->frame_.get();
    in->inline_.offsetX = (1.0f - t) * dx * (float)outDir_;
    in->inline_.opacity = saturate(t * 1.6f);
    in->styleDirty_ = true;
    if (outgoing_) {
      outgoing_->inline_.offsetX = -t * dx * 0.6f * (float)outDir_;
      outgoing_->inline_.opacity = saturate(1.0f - t * 1.8f);
      outgoing_->styleDirty_ = true;
    }
  }
};

class Shell {
 public:
  enum class LayerKind { Drawer, Dialog };

  struct Layer {
    WidgetPtr content;
    LayerKind kind = LayerKind::Dialog;
    bool modal = true;
    bool dismissable = true;
    bool closing = false;
    Spring anim;
    std::function<void()> onClose;
    std::weak_ptr<Widget> prevFocus;
    Vec2 size;
  };

  struct Toast {
    WidgetPtr w;
    Spring enter;
    float ttl = 4.0f;
    bool leaving = false;
    Vec2 size;
    float slot = 0.0f;
  };

  Shell() {
    root_ = Box::make(Direction::Row)->cls("shell-root");
    root_->inline_.width = Dim::fill();
    root_->inline_.height = Dim::fill();
    host_ = std::make_shared<ViewHost>();
    host_->onLeave_ = [this](const ViewPtr& v) {
      if (v && v->onLeave_) v->onLeave_();
    };
    navHeader_ = Box::make(Direction::Column)->cls("shell-nav-header");
    navBody_ = Box::make(Direction::Column)->cls("shell-nav-body");
    nav_ = Box::make(Direction::Column)->cls("shell-nav");
    nav_->add({navHeader_, navBody_});
    root_->add({host_, nav_});
    drawerBox_ = Box::make(Direction::Column)->cls("shell-drawer");
    drawer_.content = drawerBox_;
    drawer_.kind = LayerKind::Drawer;
    drawer_.anim.motion = Motion::bouncy();
    drawer_.anim.motion.stiffness = 420.0f;
    drawer_.anim.snap(0.0f);
    drawerBox_->parent_ = nullptr;
  }

  std::shared_ptr<Box> root() { return root_; }
  std::shared_ptr<Box> drawer() { return drawerBox_; }
  std::shared_ptr<Box> navHeader() { return navHeader_; }
  std::shared_ptr<Box> navBody() { return navBody_; }
  std::shared_ptr<Box> nav() { return nav_; }
  ViewPtr view() const { return host_->current(); }

  void setView(const ViewPtr& v) {
    if (!history_.empty()) history_.back() = v;
    else history_.push_back(v);
    host_->show(v, 1);
    focusNewView(v);
  }
  void resetView(const ViewPtr& v) {
    history_.clear();
    history_.push_back(v);
    host_->show(v, 1);
    focusNewView(v);
  }
  void pushView(const ViewPtr& v) {
    history_.push_back(v);
    host_->show(v, 1);
    focusNewView(v);
  }
  bool popView() {
    if (history_.size() <= 1) return false;
    history_.pop_back();
    host_->show(history_.back(), -1);
    focusNewView(history_.back());
    return true;
  }
  size_t historySize() const { return history_.size(); }
  bool transitioning() const { return host_->transitioning(); }

  void openDrawer() {
    if (drawerOpen_) return;
    drawerOpen_ = true;
    drawer_.anim.to(1.0f);
    if (focusProbe_) drawer_.prevFocus = focusProbe_();
  }
  void closeDrawer() {
    if (!drawerOpen_) return;
    drawerOpen_ = false;
    drawer_.anim.to(0.0f);
    restoreFocus_ = drawer_.prevFocus;
  }
  void toggleDrawer() { drawerOpen_ ? closeDrawer() : openDrawer(); }
  bool drawerOpen() const { return drawerOpen_; }

  Layer& showDialog(WidgetPtr content, bool dismissable = true, std::function<void()> onClose = nullptr) {
    overlays_.emplace_back();
    Layer& l = overlays_.back();
    l.content = std::move(content);
    l.kind = LayerKind::Dialog;
    l.dismissable = dismissable;
    l.onClose = std::move(onClose);
    l.anim.motion = Motion::bouncy();
    l.anim.motion.stiffness = 480.0f;
    l.anim.snap(0.0f);
    l.anim.to(1.0f);
    if (focusProbe_) l.prevFocus = focusProbe_();
    return l;
  }

  void dialog(const std::string& title, const std::string& message,
              std::vector<std::pair<std::string, std::function<void()>>> actions) {
    auto box = Box::make(Direction::Column)->cls("dialog");
    box->add(Label::make(title)->cls("headline"));
    box->add(Label::make(message)->wrap(true)->fg("onSurfaceVariant"));
    auto row = Box::make(Direction::Row)->gap(8)->justify(Justify::End)->fillWidth();
    Widget* key = box.get();
    for (size_t i = 0; i < actions.size(); ++i) {
      auto fn = actions[i].second;
      auto b = Button::make()->cls(i + 1 == actions.size() ? "" : "text");
      b->add(Label::make(actions[i].first));
      b->onClick([this, key, fn]() {
        closeLayerFor(key);
        if (fn) fn();
      });
      row->add(b);
    }
    box->add(row);
    showDialog(box, true);
  }

  void closeLayerFor(Widget* content) {
    for (Layer& l : overlays_) {
      if (l.content.get() == content && !l.closing) {
        l.closing = true;
        l.anim.to(0.0f);
        restoreFocus_ = l.prevFocus;
        if (l.onClose) l.onClose();
      }
    }
  }

  bool modalActive() const {
    for (const Layer& l : overlays_)
      if (!l.closing) return true;
    return drawerOpen_;
  }

  bool dismissTop() {
    for (size_t i = overlays_.size(); i-- > 0;) {
      Layer& l = overlays_[i];
      if (l.closing) continue;
      if (!l.dismissable) return true;
      closeLayerFor(l.content.get());
      return true;
    }
    if (drawerOpen_) {
      closeDrawer();
      return true;
    }
    return false;
  }

  void notify(const std::string& title, const std::string& body = "", float seconds = 4.0f, uint32_t glyph = 0) {
    auto card = Box::make(Direction::Row)->cls("toast")->gap(14)->align(Align::Center);
    if (glyph) card->add(Glyph::make(glyph));
    auto col = Box::make(Direction::Column)->gap(2);
    col->add(Label::make(title)->cls("toast-title"));
    if (!body.empty()) col->add(Label::make(body)->cls("toast-body"));
    card->add(col);
    toasts_.emplace_back();
    Toast& t = toasts_.back();
    t.w = card;
    t.ttl = seconds;
    t.enter.motion = Motion::bouncy();
    t.enter.motion.stiffness = 460.0f;
    t.enter.snap(0.0f);
    t.enter.to(1.0f);
    while (toasts_.size() > 4) toasts_.erase(toasts_.begin());
  }

  Widget* focusRoot() {
    for (size_t i = overlays_.size(); i-- > 0;)
      if (!overlays_[i].closing) return overlays_[i].content.get();
    if (drawerOpen_) return drawerBox_.get();
    return root_.get();
  }

  Widget* pick(Vec2 p, bool& scrimHit) {
    scrimHit = false;
    for (size_t i = overlays_.size(); i-- > 0;) {
      Layer& l = overlays_[i];
      if (l.closing) continue;
      if (Widget* h = l.content->hitTest(p)) return resolve(h);
      scrimHit = true;
      return nullptr;
    }
    if (drawerOpen_ || drawer_.anim.value > 0.05f) {
      if (Widget* h = drawerBox_->hitTest(p)) return resolve(h);
      if (drawerOpen_) {
        scrimHit = true;
        return nullptr;
      }
    }
    if (Widget* h = root_->hitTest(p)) return resolve(h);
    return nullptr;
  }

  void update(Context& ctx, float dt) {
    if (!restoreFocus_.expired() && ctx.focus) {
      if (auto w = restoreFocus_.lock()) {
        bool visible = true;
        for (Widget* p = w.get(); p; p = p->parent_) if (!p->visible_) visible = false;
        if (visible && w->focusable_) ctx.focus->set(w.get());
      }
      restoreFocus_.reset();
    }
    drawer_.anim.update(dt);
    root_->updateTree(ctx, dt);
    if (drawer_.anim.value > 0.002f || drawerOpen_) drawerBox_->updateTree(ctx, dt);
    for (Layer& l : overlays_) {
      l.anim.update(dt);
      l.content->updateTree(ctx, dt);
    }
    overlays_.erase(std::remove_if(overlays_.begin(), overlays_.end(),
                                   [](const Layer& l) { return l.closing && l.anim.value < 0.01f; }),
                    overlays_.end());
    for (Toast& t : toasts_) {
      t.enter.update(dt);
      t.w->updateTree(ctx, dt);
      if (!t.leaving) {
        t.ttl -= dt;
        if (t.ttl <= 0.0f) {
          t.leaving = true;
          t.enter.to(0.0f);
        }
      }
    }
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [](const Toast& t) { return t.leaving && t.enter.value < 0.01f; }),
                  toasts_.end());
    layout(ctx);
    settleViewFocus(ctx, dt);
  }

  void draw(Context& ctx) {
    Renderer& r = *ctx.renderer;
    Rect full{0, 0, ctx.viewport.x, ctx.viewport.y};
    root_->draw(ctx);
    Color scrim = ctx.colors->get("scrim");
    float da = saturate(drawer_.anim.value);
    if (da > 0.003f) {
      r.fillRect(full, scrim.withAlpha(150.0f * da));
      drawerBox_->draw(ctx);
    }
    for (Layer& l : overlays_) {
      float a = saturate(l.anim.value);
      r.fillRect(full, scrim.withAlpha(150.0f * a));
      Rect rc = l.content->rect_;
      float s = lerpf(0.86f, 1.0f, l.anim.value);
      r.pushLayer({rc.cx(), rc.cy()}, s, {0, 0}, a);
      l.content->draw(ctx);
      r.popLayer();
    }
    for (Toast& t : toasts_) {
      float e = t.enter.value;
      float rise = (1.0f - e) * (t.size.y + 40.0f);
      r.pushLayer({0, 0}, 1.0f, {0, rise}, saturate(e * 1.5f));
      t.w->draw(ctx);
      r.popLayer();
    }
  }

  std::function<std::weak_ptr<Widget>()> focusProbe_;

 private:
  std::shared_ptr<Box> root_, nav_, navHeader_, navBody_, drawerBox_;
  std::shared_ptr<ViewHost> host_;
  Layer drawer_;
  std::vector<Layer> overlays_;
  std::vector<Toast> toasts_;
  std::vector<ViewPtr> history_;
  std::weak_ptr<Widget> restoreFocus_;
  bool drawerOpen_ = false;
  ViewPtr focusView_;
  float focusWait_ = 0.0f;

  void focusNewView(const ViewPtr& v) {
    restoreFocus_.reset();
    focusView_ = v;
    focusWait_ = 1.5f;
  }

  void settleViewFocus(Context& ctx, float dt) {
    if (!focusView_ || !ctx.focus) return;
    if (focusRoot() != root_.get()) return;
    focusWait_ -= dt;
    Widget* frame = focusView_->frame_.get();
    for (Widget* p = ctx.focus->current(); p; p = p->parent_) {
      if (p == frame) {
        focusView_.reset();
        return;
      }
    }
    Widget* target = ctx.focus->first(focusView_->body_.get());
    if (!target && focusWait_ <= 0.0f) target = ctx.focus->first(frame);
    if (target) {
      ctx.focus->set(target);
      focusView_.reset();
    } else if (focusWait_ <= 0.0f) {
      focusView_.reset();
    }
  }

  static Widget* resolve(Widget* h) {
    while (h && !h->interactive()) h = h->parent_;
    return h;
  }

  void layout(Context& ctx) {
    Vec2 v = ctx.viewport;
    root_->measure(ctx, v);
    root_->arrange(ctx, {0, 0, v.x, v.y});

    Insets dm = drawerBox_->cs_.margin.value_or(Insets(12.0f));
    Vec2 ds = drawerBox_->measure(ctx, {v.x - dm.horizontal(), v.y - dm.vertical()});
    float da = drawer_.anim.value;
    float x = dm.l - (1.0f - da) * (ds.x + dm.horizontal() + 8.0f);
    drawerBox_->arrange(ctx, {x, dm.t, ds.x, ds.y});

    for (Layer& l : overlays_) {
      Vec2 s = l.content->measure(ctx, {v.x * 0.9f, v.y * 0.9f});
      l.content->arrange(ctx, {(v.x - s.x) * 0.5f, (v.y - s.y) * 0.5f, s.x, s.y});
    }

    float margin = 18.0f;
    float y = v.y - margin;
    for (size_t i = toasts_.size(); i-- > 0;) {
      Toast& t = toasts_[i];
      t.size = t.w->measure(ctx, {std::min(360.0f, v.x * 0.6f), v.y});
      y -= t.size.y;
      t.w->arrange(ctx, {v.x - margin - t.size.x, y, t.size.x, t.size.y});
      y -= 10.0f;
    }
  }
};

}
