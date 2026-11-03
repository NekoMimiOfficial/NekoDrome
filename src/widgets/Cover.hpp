#pragma once

#include "../App.hpp"

namespace nt {

class Cover : public neko::ContainerT<Cover> {
 public:
  const char* typeName() const override { return "cover"; }

  static Ptr make(std::string coverId, float size, int requestSize = 300) {
    auto c = std::make_shared<Cover>();
    c->id_ = std::move(coverId);
    c->req_ = requestSize;
    c->inline_.direction = neko::Direction::Stack;
    c->inline_.width = neko::Dim::px(size);
    c->inline_.height = neko::Dim::px(size);
    c->inline_.radius = 14.0f;
    c->inline_.background = neko::ColorRef("surfaceContainerHigh");
    c->inline_.overflow = neko::Overflow::Clip;
    c->icon_ = neko::Glyph::make(ic::music_note)->fontSize(size * 0.42f)->fg(neko::ColorRef("onSurfaceVariant").a(0.45f));
    c->icon_->inline_.alignSelf = neko::Align::Center;
    c->icon_->inline_.justifySelf = neko::Align::Center;
    c->img_ = neko::Image::make("")->fill();
    c->img_->inline_.width = neko::Dim::fill();
    c->img_->inline_.height = neko::Dim::fill();
    c->addChild(c->icon_);
    c->addChild(c->img_);
    return c;
  }

  Ptr coverId(const std::string& id) {
    if (id != id_) {
      id_ = id;
      loaded_ = false;
      img_->src("");
      if (flat_) {
        flat_ = false;
        inline_.background = neko::ColorRef("surfaceContainerHigh");
        styleDirty_ = true;
      }
    }
    return self();
  }

  bool interactive() const override { return false; }
  neko::Widget* hitTest(neko::Vec2) override { return nullptr; }

  void tick(neko::Context&, float) override {
    if (cs_.radius && (!radiusSet_ || *cs_.radius != lastRadius_)) {
      img_->inline_.radius = *cs_.radius;
      img_->markStyleDirty();
      lastRadius_ = *cs_.radius;
      radiusSet_ = true;
    }
    if (loaded_ && img_->loaded() && !flat_) {
      flat_ = true;
      inline_.background = neko::ColorRef::transparent();
      styleDirty_ = true;
    }
    if (loaded_ || id_.empty()) return;
    Services& s = svc();
    if (s.covers.ready(id_, req_)) {
      img_->src(s.covers.path(id_, req_));
      loaded_ = true;
    }
  }

 private:
  std::string id_;
  int req_ = 300;
  bool loaded_ = false, flat_ = false, radiusSet_ = false;
  float lastRadius_ = 0.0f;
  std::shared_ptr<neko::Glyph> icon_;
  std::shared_ptr<neko::Image> img_;
};

}
