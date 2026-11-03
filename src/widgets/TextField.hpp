#pragma once

#include <NekoUI/Ui.hpp>

#include "../Util.hpp"

namespace nt {

class TextField : public neko::WidgetT<TextField> {
 public:
  const char* typeName() const override { return "textfield"; }

  std::function<void(const std::string&)> onSubmit_, onChange_;

  TextField() { focusable_ = true; }

  static Ptr make(std::string placeholder = "", bool password = false) {
    auto t = std::make_shared<TextField>();
    t->placeholder_ = std::move(placeholder);
    t->password_ = password;
    return t;
  }

  Ptr text(std::string t) {
    text_ = std::move(t);
    caret_ = text_.size();
    return self();
  }
  const std::string& text() const { return text_; }
  Ptr placeholder(std::string p) { placeholder_ = std::move(p); return self(); }
  Ptr password(bool v) { password_ = v; return self(); }
  Ptr onSubmit(std::function<void(const std::string&)> f) { onSubmit_ = std::move(f); return self(); }
  Ptr onChange(std::function<void(const std::string&)> f) { onChange_ = std::move(f); return self(); }
  bool editing() const { return editing_; }

  bool interactive() const override { return true; }

  neko::Vec2 contentSize(neko::Context&, neko::Vec2 avail) override {
    return {neko::isFinite(avail.x) ? std::min(avail.x, 320.0f) : 320.0f, (float)std::lround(fontSize_ * 1.4f)};
  }

  void click(neko::Context& ctx) override {
    if (disabled_ || settling()) return;
    begin(ctx);
  }

  bool onAction(neko::Context& ctx, neko::Action a) override {
    if (a == neko::Action::Accept && !disabled_) {
      if (settling()) return true;
      flash_ = 0.12f;
      begin(ctx);
      return true;
    }
    return false;
  }

  void begin(neko::Context& ctx) {
    if (active() && active() != this) active()->end();
    editing_ = true;
    ctx_ = &ctx;
    ctx.input->captureKeyboard = true;
    active() = this;
    caret_ = text_.size();
    blink_ = 0.0f;
    native_ = false;
    if (platform::hasNativeTextInput()) {
      platform::TextRequest r;
      r.title = placeholder_;
      r.initial = text_;
      r.password = password_;
      native_ = platform::beginTextInput(r);
    }
    if (!native_) SDL_StartTextInput();
  }

  void end() {
    if (!editing_) return;
    editing_ = false;
    if (ctx_) ctx_->input->captureKeyboard = false;
    if (active() == this) active() = nullptr;
    if (native_) {
      native_ = false;
      platform::cancelTextInput();
    } else {
      SDL_StopTextInput();
    }
  }

  ~TextField() override { if (editing_ && active() == this) end(); }

  void tick(neko::Context&, float dt) override {
    blink_ += dt;
    if (editing_ && !focused_) end();
    float target = 0.0f;
    scroll_ += (scrollTarget_ - scroll_) * std::min(1.0f, dt * 18.0f);
    (void)target;
  }

  void drawContent(neko::Context& ctx) override {
    neko::Renderer& r = *ctx.renderer;
    neko::Insets pad = cs_.padding.value_or(neko::Insets());
    neko::Rect in = rect_.inset(pad);
    int sz = std::max(4, (int)std::lround(fontSize_));
    std::string shown = display();
    bool empty = text_.empty();
    const std::string& str = empty ? placeholder_ : shown;
    neko::Color col = empty ? ctx.colors->get("onSurfaceVariant").scaledAlpha(0.7f) : fg_.current();
    neko::TextMetrics full = r.textSize(str, neko::FontFace::Text, sz);
    float y = in.y + (in.h - (float)full.h) * 0.5f;
    float caretX = 0;
    if (editing_) {
      std::string before = password_ ? mask(caretCodepoints()) : text_.substr(0, caret_);
      caretX = (float)r.textSize(before, neko::FontFace::Text, sz).w;
      if (before.empty()) caretX = 0;
      if (caretX - scrollTarget_ > in.w - 4.0f) scrollTarget_ = caretX - in.w + 4.0f;
      if (caretX - scrollTarget_ < 0.0f) scrollTarget_ = caretX;
    } else {
      scrollTarget_ = 0.0f;
    }
    float off = editing_ ? scroll_ : 0.0f;
    r.pushClip(in.expanded(2.0f));
    if (!str.empty()) r.text(str, neko::FontFace::Text, sz, {in.x - off, y}, col);
    if (editing_ && std::fmod(blink_, 1.0f) < 0.6f) {
      neko::Color cc = ctx.colors->get("primary");
      r.fillRect({in.x + caretX - off, y + 1.0f, 2.0f, (float)full.h - 2.0f}, cc);
    }
    r.popClip();
    if (editing_) {
      neko::Color ring = ctx.colors->get("primary");
      r.strokeRRect(rect_, std::max(0.0f, radius_.value), 2.0f, ring);
    }
  }

  static bool dispatch(const SDL_Event& e) {
    TextField* t = active();
    if (!t || t->native_) return false;
    return t->handle(e);
  }

  static void pump() {
    TextField* t = active();
    if (t && t->native_) t->finishNative();
  }

 private:
  std::string text_, placeholder_;
  bool password_ = false, editing_ = false, native_ = false;
  size_t caret_ = 0;
  float blink_ = 0, scroll_ = 0, scrollTarget_ = 0;
  neko::Context* ctx_ = nullptr;

  static TextField*& active() {
    static TextField* a = nullptr;
    return a;
  }

  static uint32_t& closedAt() {
    static uint32_t t = 0;
    return t;
  }

  static bool settling() { return closedAt() != 0 && SDL_GetTicks() - closedAt() < 350; }

  void finishNative() {
    platform::TextResult r;
    if (!platform::pollTextInput(r)) return;
    closedAt() = SDL_GetTicks();
    if (!r.accepted) {
      end();
      return;
    }
    text_ = r.text;
    caret_ = text_.size();
    changed();
    std::string t = text_;
    auto cb = onSubmit_;
    end();
    if (cb) cb(t);
  }

  static std::string mask(size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) s += "\xE2\x80\xA2";
    return s;
  }

  size_t codepoints(const std::string& s) const {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
  }
  size_t caretCodepoints() const { return codepoints(text_.substr(0, caret_)); }

  std::string display() const { return password_ ? mask(codepoints(text_)) : text_; }

  size_t prevPos(size_t i) const {
    if (i == 0) return 0;
    --i;
    while (i > 0 && ((unsigned char)text_[i] & 0xC0) == 0x80) --i;
    return i;
  }
  size_t nextPos(size_t i) const {
    if (i >= text_.size()) return text_.size();
    ++i;
    while (i < text_.size() && ((unsigned char)text_[i] & 0xC0) == 0x80) ++i;
    return i;
  }

  void changed() {
    blink_ = 0.0f;
    if (onChange_) onChange_(text_);
  }

  void insert(const std::string& s) {
    text_.insert(caret_, s);
    caret_ += s.size();
    changed();
  }

  bool handle(const SDL_Event& e) {
    if (e.type == SDL_TEXTINPUT) {
      insert(e.text.text);
      return true;
    }
    if (e.type != SDL_KEYDOWN) return false;
    SDL_Keycode k = e.key.keysym.sym;
    bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
    switch (k) {
      case SDLK_BACKSPACE:
        if (caret_ > 0) {
          size_t p = prevPos(caret_);
          text_.erase(p, caret_ - p);
          caret_ = p;
          changed();
        }
        return true;
      case SDLK_DELETE:
        if (caret_ < text_.size()) {
          size_t n = nextPos(caret_);
          text_.erase(caret_, n - caret_);
          changed();
        }
        return true;
      case SDLK_LEFT: caret_ = prevPos(caret_); blink_ = 0; return true;
      case SDLK_RIGHT: caret_ = nextPos(caret_); blink_ = 0; return true;
      case SDLK_HOME: caret_ = 0; blink_ = 0; return true;
      case SDLK_END: caret_ = text_.size(); blink_ = 0; return true;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_TAB: {
        std::string t = text_;
        auto cb = onSubmit_;
        end();
        if (cb) cb(t);
        return true;
      }
      case SDLK_ESCAPE: end(); return true;
      case SDLK_v:
        if (ctrl) {
          char* clip = SDL_GetClipboardText();
          if (clip) {
            std::string c = clip;
            SDL_free(clip);
            c.erase(std::remove(c.begin(), c.end(), '\n'), c.end());
            c.erase(std::remove(c.begin(), c.end(), '\r'), c.end());
            insert(c);
          }
          return true;
        }
        return false;
      default: return false;
    }
  }
};

}
