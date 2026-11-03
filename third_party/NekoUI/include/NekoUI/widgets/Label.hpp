#pragma once

#include "Widget.hpp"

namespace neko {

class Label : public WidgetT<Label> {
 public:
  const char* typeName() const override { return "label"; }

  static Ptr make(std::string text = "") {
    auto l = std::make_shared<Label>();
    l->text_ = std::move(text);
    return l;
  }

  Ptr text(std::string t) {
    if (t != text_) {
      text_ = std::move(t);
      cacheValid_ = false;
    }
    return self();
  }
  const std::string& text() const { return text_; }
  Ptr wrap(bool v) { inline_.wrap = v; styleDirty_ = true; return self(); }
  Ptr maxLines(int v) { inline_.maxLines = v; styleDirty_ = true; return self(); }
  Ptr textAlign(TextAlign v) { inline_.textAlign = v; styleDirty_ = true; return self(); }

  bool interactive() const override { return (bool)onClick_; }

  Vec2 contentSize(Context& ctx, Vec2 avail) override {
    layoutText(ctx, avail.x);
    return {(float)shownW_, (float)shownH_};
  }

  void drawContent(Context& ctx) override {
    Insets pad = cs_.padding.value_or(Insets());
    Rect in = rect_.inset(pad);
    if (!cacheValid_ || in.w != lastWidth_) layoutText(ctx, in.w);
    if (shown_.empty()) return;
    TextAlign ta = cs_.textAlign.value_or(TextAlign::Left);
    float x = in.x;
    if (ta == TextAlign::Center) x = in.x + (in.w - (float)shownW_) * 0.5f;
    else if (ta == TextAlign::Right) x = in.right() - (float)shownW_;
    float y = in.y + (in.h - (float)shownH_) * 0.5f;
    Renderer& r = *ctx.renderer;
    bool clip = wrapped_ && drawnH_ < fullH_;
    if (clip) r.pushClip(in);
    r.text(shown_, fontFace_, fontPx(), {x, y}, fg_.current(), wrapped_ ? wrapWidth_ : 0);
    if (clip) r.popClip();
  }

 protected:
  std::string text_, shown_;
  bool cacheValid_ = false, wrapped_ = false;
  int shownW_ = 0, shownH_ = 0, wrapWidth_ = 0, fullH_ = 0, drawnH_ = 0;
  float lastWidth_ = -1.0f, lastSize_ = 0;
  FontFace lastFace_ = FontFace::Text;
  uint32_t lastGen_ = 0;

  int fontPx() const { return std::max(4, (int)std::lround(fontSize_)); }

  void layoutText(Context& ctx, float availW) {
    if (cacheValid_ && availW == lastWidth_ && lastSize_ == fontSize_ && lastFace_ == fontFace_ &&
        lastGen_ == ctx.renderer->generation())
      return;
    Renderer& r = *ctx.renderer;
    bool wrap = cs_.wrap.value_or(false);
    int sz = fontPx();
    wrapped_ = wrap && isFinite(availW);
    if (wrapped_) {
      wrapWidth_ = std::max(1, (int)availW);
      TextMetrics m = r.textSize(text_, fontFace_, sz, wrapWidth_);
      shown_ = text_;
      fullH_ = m.h;
      int lines = cs_.maxLines.value_or(0);
      int lh = r.fontHeight(fontFace_, sz);
      drawnH_ = lines > 0 ? std::min(m.h, lines * lh) : m.h;
      shownW_ = m.w;
      shownH_ = drawnH_;
    } else {
      TextMetrics m = r.textSize(text_, fontFace_, sz);
      shown_ = text_;
      if (isFinite(availW) && (float)m.w > availW && availW > 0) {
        std::string cut = ellipsize(r, text_, sz, availW);
        shown_ = cut;
        m = r.textSize(shown_, fontFace_, sz);
      }
      shownW_ = m.w;
      shownH_ = m.h;
      drawnH_ = fullH_ = m.h;
    }
    lastWidth_ = availW;
    lastSize_ = fontSize_;
    lastFace_ = fontFace_;
    lastGen_ = ctx.renderer->generation();
    cacheValid_ = true;
  }

  std::string ellipsize(Renderer& r, const std::string& s, int sz, float maxW) {
    const std::string dots = "\xE2\x80\xA6";
    size_t lo = 0, hi = s.size();
    auto fits = [&](size_t n) {
      while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) --n;
      return r.textSize(s.substr(0, n) + dots, fontFace_, sz).w <= maxW;
    };
    while (lo < hi) {
      size_t mid = (lo + hi + 1) / 2;
      if (fits(mid)) lo = mid;
      else hi = mid - 1;
    }
    size_t n = lo;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) --n;
    return s.substr(0, n) + dots;
  }

 public:
  void tick(Context&, float) override {
    if (lastSize_ != fontSize_ || lastFace_ != fontFace_) cacheValid_ = false;
  }
};

class Glyph : public WidgetT<Glyph> {
 public:
  const char* typeName() const override { return "glyph"; }

  static Ptr make(uint32_t codepoint) {
    auto g = std::make_shared<Glyph>();
    g->cp_ = codepoint;
    return g;
  }

  Ptr codepoint(uint32_t cp) { cp_ = cp; return self(); }
  uint32_t codepoint() const { return cp_; }
  bool interactive() const override { return (bool)onClick_; }

  Vec2 contentSize(Context&, Vec2) override { return {fontSize_, fontSize_}; }

  void drawContent(Context& ctx) override {
    Insets pad = cs_.padding.value_or(Insets());
    Rect in = rect_.inset(pad);
    int sz = std::max(4, (int)std::lround(fontSize_));
    std::string s = encodeUtf8(cp_);
    TextMetrics m = ctx.renderer->textSize(s, FontFace::Glyph, sz);
    ctx.renderer->text(s, FontFace::Glyph, sz, {in.cx() - (float)m.w * 0.5f, in.cy() - (float)m.h * 0.5f}, fg_.current());
  }

 private:
  uint32_t cp_ = 0;
};

}
