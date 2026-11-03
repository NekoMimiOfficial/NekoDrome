#pragma once

#include "Widget.hpp"

namespace neko {

class Image : public WidgetT<Image> {
 public:
  const char* typeName() const override { return "image"; }

  static Ptr make(std::string path = "") {
    auto i = std::make_shared<Image>();
    i->path_ = std::move(path);
    return i;
  }

  Ptr src(std::string path) {
    if (path != path_) {
      path_ = std::move(path);
      asset_.reset();
      loadedShown_ = false;
    }
    return self();
  }
  Ptr fit(ImageFit f) { inline_.imageFit = f; styleDirty_ = true; return self(); }
  const std::string& path() const { return path_; }
  bool loaded() const { return asset_ && asset_->ready(); }
  bool failed() const { return asset_ && asset_->state == ImageAsset::State::Failed; }

  bool interactive() const override { return (bool)onClick_; }

  Vec2 contentSize(Context& ctx, Vec2) override {
    request(ctx);
    if (asset_ && asset_->ready()) return {(float)asset_->w, (float)asset_->h};
    return {0, 0};
  }

  void tick(Context& ctx, float dt) override {
    request(ctx);
    if (asset_ && asset_->ready() && !loadedShown_) {
      loadedShown_ = true;
      fade_.snap(0.0f);
    }
    fade_.to(loaded() ? 1.0f : 0.0f);
    fade_.update(dt);
    shimmer_ += dt;
  }

  void drawContent(Context& ctx) override {
    Renderer& r = *ctx.renderer;
    float rad = std::max(0.0f, radius_.value);
    Rect box = rect_;
    float fade = saturate(fade_.value);
    if (fade < 0.999f) {
      float pulse = 0.5f + 0.5f * std::sin(shimmer_ * 3.2f);
      Color base = ctx.colors->get("surfaceContainerHigh");
      Color hi = ctx.colors->get("surfaceContainerHighest");
      if (!failed()) r.fillRRect(box, rad, Color::lerp(base, hi, pulse));
      else r.fillRRect(box, rad, ctx.colors->get("errorContainer").scaledAlpha(0.5f));
    }
    if (!loaded() || fade <= 0.0f) return;
    ImageFit fit = cs_.imageFit.value_or(ImageFit::Cover);
    float iw = (float)asset_->w, ih = (float)asset_->h;
    SDL_Rect src{0, 0, asset_->w, asset_->h};
    Rect dst = box;
    if (fit == ImageFit::Contain) {
      float k = std::min(box.w / iw, box.h / ih);
      dst = {box.cx() - iw * k * 0.5f, box.cy() - ih * k * 0.5f, iw * k, ih * k};
    } else if (fit == ImageFit::Cover) {
      float k = std::max(box.w / iw, box.h / ih);
      float vw = box.w / k, vh = box.h / k;
      src = {(int)((iw - vw) * 0.5f), (int)((ih - vh) * 0.5f), std::max(1, (int)vw), std::max(1, (int)vh)};
    } else if (fit == ImageFit::None) {
      dst = {box.cx() - iw * 0.5f, box.cy() - ih * 0.5f, iw, ih};
    }
    r.pushClip(box);
    r.texture(asset_->tex, &src, dst, fade);
    r.popClip();
    if (rad > 0.5f) r.cutoutCorners(box, rad, cutoutColor(ctx));
  }

 private:
  std::string path_;
  std::shared_ptr<ImageAsset> asset_;
  Spring fade_;
  float shimmer_ = 0;
  bool loadedShown_ = false;

  void request(Context& ctx) {
    if (!asset_ && !path_.empty() && ctx.assets) asset_ = ctx.assets->image(path_);
  }

  Color cutoutColor(Context& ctx) const {
    for (Widget* p = parent_; p; p = p->parent_) {
      Color c = p->bgColor();
      if (c.a > 250.0f) return c;
    }
    return ctx.colors->get("background");
  }
};

}
