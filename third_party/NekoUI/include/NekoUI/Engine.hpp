#pragma once

#include "core/Assets.hpp"
#include "core/Audio.hpp"
#include "core/Colors.hpp"
#include "core/Context.hpp"
#include "core/DefaultStyles.hpp"
#include "core/Input.hpp"
#include "core/Renderer.hpp"
#include "core/Style.hpp"
#include "shell/Shell.hpp"
#include "widgets/Scroll.hpp"

namespace neko {

struct Settings {
  std::string title = "NekoUI";
  int windowWidth = 960;
  int windowHeight = 544;
  int logicalWidth = 960;
  int logicalHeight = 544;
  bool fullscreen = false;
  bool resizable = true;
  bool vsync = true;
  int targetFps = 60;

  std::string fontPath;
  std::string glyphFontPath;
  float baseFontSize = 16.0f;

  ColorMode colorMode = ColorMode::Dark;
  std::vector<ColorOverride> colors;
  std::vector<std::pair<std::string, ClassStyle>> classes;
  std::vector<KeyBind> globalKeybinds;
  std::map<Action, GlyphSpec> actionGlyphs;

  bool circleAccept = false;
  bool keyboardAlways = false;
  bool enableAudio = true;
  bool defaultStyles = true;
  size_t imageBudgetBytes = 48u * 1024u * 1024u;
  int imageMaxDim = 1024;

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
};

class Engine : public WidgetWatcher {
 public:
  Engine() = default;
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  ~Engine() { shutdown(); }

  bool init(const Settings& s) {
    settings_ = s;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER;
    if (s.enableAudio) flags |= SDL_INIT_AUDIO;
    if (SDL_WasInit(0) == 0 && SDL_Init(flags) != 0) return false;
    SDL_InitSubSystem(flags);

    if (s.window && s.renderer) {
      window_ = s.window;
      renderer_ = s.renderer;
    } else {
      Uint32 wf = (Uint32)SDL_WINDOW_SHOWN | (s.resizable ? (Uint32)SDL_WINDOW_RESIZABLE : 0u) |
                  (s.fullscreen ? (Uint32)SDL_WINDOW_FULLSCREEN_DESKTOP : 0u);
      window_ = SDL_CreateWindow(s.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, s.windowWidth,
                                 s.windowHeight, wf);
      if (!window_) return false;
      Uint32 rf = (Uint32)SDL_RENDERER_ACCELERATED | (s.vsync ? (Uint32)SDL_RENDERER_PRESENTVSYNC : 0u);
      renderer_ = SDL_CreateRenderer(window_, -1, rf);
      if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
      if (!renderer_) return false;
      ownsWindow_ = true;
    }

    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
    if (!renderer_r_.init(renderer_, s.fontPath, s.glyphFontPath)) return false;

    registerNekoppuccin(colors_);
    for (const ColorOverride& o : s.colors) colors_.set(o.name, o.dark, o.light);
    colors_.setMode(s.colorMode);

    if (s.defaultStyles) registerDefaultStyles(styles_);
    for (const auto& c : s.classes) styles_.define(c.first, c.second);

    for (const auto& g : s.actionGlyphs) input_.glyphs[g.first] = g.second;
    input_.circleAccept = s.circleAccept;
    input_.keyboardAlways = s.keyboardAlways;
    input_.init({(float)s.logicalWidth, (float)s.logicalHeight});

    assets_.init(renderer_, s.imageBudgetBytes, s.imageMaxDim);
    if (s.enableAudio) audio_.init(&assets_);

    globalKeybinds_ = s.globalKeybinds;

    ctx_.renderer = &renderer_r_;
    ctx_.colors = &colors_;
    ctx_.styles = &styles_;
    ctx_.input = &input_;
    ctx_.assets = &assets_;
    ctx_.audio = &audio_;
    ctx_.signals = &signals_;
    ctx_.focus = &focus_;
    ctx_.viewport = {(float)s.logicalWidth, (float)s.logicalHeight};
    ctx_.baseFontSize = s.baseFontSize;

    shell_ = std::make_unique<Shell>();
    shell_->focusProbe_ = [this]() -> std::weak_ptr<Widget> {
      Widget* f = focus_.current();
      return f ? f->weak_from_this() : std::weak_ptr<Widget>();
    };

    widgetWatchers().push_back(static_cast<WidgetWatcher*>(this));
    last_ = SDL_GetPerformanceCounter();
    running_ = true;
    initialized_ = true;
    return true;
  }

  void shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    auto& w = widgetWatchers();
    w.erase(std::remove(w.begin(), w.end(), static_cast<WidgetWatcher*>(this)), w.end());
    shell_.reset();
    audio_.shutdown();
    assets_.shutdown();
    input_.shutdown();
    renderer_r_.shutdown();
    if (ownsWindow_) {
      SDL_DestroyRenderer(renderer_);
      SDL_DestroyWindow(window_);
    }
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
  }

  bool running() const { return running_; }
  void quit() { running_ = false; }

  ColorRegistry& colors() { return colors_; }
  StyleSheet& styles() { return styles_; }
  Input& input() { return input_; }
  AssetLoader& assets() { return assets_; }
  MusicPlayer& audio() { return audio_; }
  SignalBus& signals() { return signals_; }
  Shell& shell() { return *shell_; }
  Renderer& renderer() { return renderer_r_; }
  Context& context() { return ctx_; }
  FocusManager& focus() { return focus_; }
  SDL_Window* window() const { return window_; }
  float deltaTime() const { return ctx_.dt; }

  void setColorMode(ColorMode m) { colors_.setMode(m); }
  void toggleColorMode() { colors_.toggleMode(); }
  ColorMode colorMode() const { return colors_.mode(); }

  void addGlobalKeybind(KeyBind k) {
    globalKeybinds_.push_back(std::move(k));
    hintSig_.clear();
  }
  void clearGlobalKeybinds() {
    globalKeybinds_.clear();
    hintSig_.clear();
  }

  void onEvent(std::function<void(const SDL_Event&)> f) { eventHook_ = std::move(f); }
  void onUpdate(std::function<void(float)> f) { updateHook_ = std::move(f); }

  Widget* findWidget(const std::string& id) {
    if (!shell_) return nullptr;
    if (Widget* w = shell_->root()->findById(id)) return w;
    return shell_->drawer()->findById(id);
  }
  template <class T>
  T* find(const std::string& id) {
    return dynamic_cast<T*>(findWidget(id));
  }

  bool frame() {
    if (!initialized_) return false;
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now - last_) / (double)SDL_GetPerformanceFrequency());
    last_ = now;
    dt = clampf(dt, 0.0001f, 0.05f);

    updateOutputScale();
    input_.beginFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running_ = false;
      toLogical(e);
      input_.handle(e);
      if (eventHook_) eventHook_(e);
    }
    input_.update(dt);
    assets_.pump(3.0f);
    audio_.update(dt);

    ctx_.dt = dt;
    ctx_.time += dt;
    ctx_.showFocus = input_.padActive;

    refreshHints();
    dispatchInput(dt);
    if (updateHook_) updateHook_(dt);

    shell_->update(ctx_, dt);
    focus_.ensureValid(shell_->focusRoot());

    renderer_r_.begin(colors_.get("background"));
    shell_->draw(ctx_);
    renderer_r_.end();

    if (!settings_.vsync && settings_.targetFps > 0) {
      double target = 1000.0 / settings_.targetFps;
      double spent = (double)(SDL_GetPerformanceCounter() - now) * 1000.0 / (double)SDL_GetPerformanceFrequency();
      if (spent < target) SDL_Delay((Uint32)(target - spent));
    }
    return running_;
  }

  void run(std::function<void(float)> perFrame = nullptr) {
    if (perFrame) updateHook_ = std::move(perFrame);
    while (frame()) {
    }
  }

 private:
  Settings settings_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  bool ownsWindow_ = false, initialized_ = false, running_ = false;
  Uint64 last_ = 0;

  Renderer renderer_r_;
  ColorRegistry colors_;
  StyleSheet styles_;
  Input input_;
  AssetLoader assets_;
  MusicPlayer audio_;
  SignalBus signals_;
  FocusManager focus_;
  Context ctx_;
  std::unique_ptr<Shell> shell_;
  std::vector<KeyBind> globalKeybinds_;
  std::function<void(const SDL_Event&)> eventHook_;
  std::function<void(float)> updateHook_;
  std::string hintSig_;

  Widget* hover_ = nullptr;
  Widget* capture_ = nullptr;
  bool stolen_ = false;
  Vec2 pressStart_;

  void widgetDestroyed(Widget* w) override {
    focus_.forget(w);
    if (hover_ == w) hover_ = nullptr;
    if (capture_ == w) capture_ = nullptr;
  }

  float outScale_ = 1.0f, outOffX_ = 0.0f, outOffY_ = 0.0f, winToOut_ = 1.0f, winToOutY_ = 1.0f;

  void updateOutputScale() {
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(renderer_, &ow, &oh);
    if (ow <= 0 || oh <= 0) return;
    float lw = (float)settings_.logicalWidth, lh = (float)settings_.logicalHeight;
    outScale_ = std::min((float)ow / lw, (float)oh / lh);
    outOffX_ = std::floor(((float)ow - lw * outScale_) * 0.5f);
    outOffY_ = std::floor(((float)oh - lh * outScale_) * 0.5f);
    int ww = ow, wh = oh;
    if (window_) SDL_GetWindowSize(window_, &ww, &wh);
    winToOut_ = ww > 0 ? (float)ow / (float)ww : 1.0f;
    winToOutY_ = wh > 0 ? (float)oh / (float)wh : 1.0f;
    renderer_r_.setOutput(outScale_, {outOffX_, outOffY_}, {lw, lh});
  }

  void toLogical(SDL_Event& e) {
    auto lx = [&](float x) { return (x * winToOut_ - outOffX_) / outScale_; };
    auto ly = [&](float y) { return (y * winToOutY_ - outOffY_) / outScale_; };
    switch (e.type) {
      case SDL_MOUSEMOTION:
        e.motion.x = (Sint32)std::lround(lx((float)e.motion.x));
        e.motion.y = (Sint32)std::lround(ly((float)e.motion.y));
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        e.button.x = (Sint32)std::lround(lx((float)e.button.x));
        e.button.y = (Sint32)std::lround(ly((float)e.button.y));
        break;
      case SDL_FINGERDOWN:
      case SDL_FINGERMOTION:
      case SDL_FINGERUP: {
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(renderer_, &ow, &oh);
        e.tfinger.x = lx(e.tfinger.x * (float)ow / winToOut_) / (float)settings_.logicalWidth;
        e.tfinger.y = ly(e.tfinger.y * (float)oh / winToOutY_) / (float)settings_.logicalHeight;
        break;
      }
      default: break;
    }
  }

  void refreshHints() {
    std::vector<Hint> hints;
    std::string sig;
    auto add = [&](const KeyBind& k) {
      if (!k.hint) return;
      for (const Hint& h : hints)
        if (h.action == k.action) return;
      hints.push_back({k.action, k.label});
      sig += std::to_string((int)k.action) + ":" + k.label + ";";
    };
    if (shell_->modalActive()) {
      add({Action::Back, "Close", nullptr, true});
    } else {
      if (ViewPtr v = shell_->view())
        for (const KeyBind& k : v->keybinds_) add(k);
      for (const KeyBind& k : globalKeybinds_) add(k);
    }
    if (sig != hintSig_) {
      hintSig_ = sig;
      ctx_.hints = std::move(hints);
      ++ctx_.hintRevision;
    }
  }

  bool fireKeybinds(const std::vector<KeyBind>& list, Action a) {
    bool any = false;
    for (const KeyBind& k : list) {
      if (k.action == a && k.callback) {
        k.callback();
        any = true;
      }
    }
    return any;
  }

  void handleAction(Action a) {
    Widget* f = focus_.current();
    if (f && f->onAction(ctx_, a)) return;
    if (a == Action::Back && shell_->dismissTop()) return;
    if (!shell_->modalActive()) {
      if (ViewPtr v = shell_->view())
        if (fireKeybinds(v->keybinds_, a)) return;
      if (fireKeybinds(globalKeybinds_, a)) return;
    }
    switch (a) {
      case Action::Up:
      case Action::Down:
      case Action::Left:
      case Action::Right:
        if (focus_.move(shell_->focusRoot(), a)) return;
        scrollFallback(a);
        break;
      case Action::Back:
        if (!shell_->modalActive()) shell_->popView();
        break;
      default: break;
    }
  }

  Widget* scrollTarget() {
    for (Widget* p = focus_.current(); p; p = p->parent_)
      if (dynamic_cast<ScrollCore*>(p) && static_cast<ScrollCore*>(p)->canScroll()) return p;
    if (input_.pointer.present) {
      bool scrim = false;
      for (Widget* p = shell_->pick(input_.pointer.pos, scrim); p; p = p->parent_)
        if (dynamic_cast<ScrollCore*>(p) && static_cast<ScrollCore*>(p)->canScroll()) return p;
    }
    return findScroll(shell_->focusRoot());
  }

  static Widget* findScroll(Widget* w) {
    if (!w || !w->visible_) return nullptr;
    auto* s = dynamic_cast<ScrollCore*>(w);
    if (s && s->canScroll()) return w;
    for (auto& c : w->children_)
      if (Widget* f = findScroll(c.get())) return f;
    return nullptr;
  }

  void scrollFallback(Action a) {
    Widget* t = scrollTarget();
    if (!t) return;
    Vec2 d{0, 0};
    float st = static_cast<ScrollCore*>(t)->step_;
    if (a == Action::Up) d.y = -st;
    if (a == Action::Down) d.y = st;
    if (a == Action::Left) d.x = -st;
    if (a == Action::Right) d.x = st;
    t->scrollBy(ctx_, d);
  }

  static Widget* resolveInteractive(Widget* h) {
    while (h && !h->interactive()) h = h->parent_;
    return h;
  }

  void setHover(Widget* w) {
    if (w == hover_) return;
    if (hover_) {
      hover_->hovered_ = false;
      if (hover_->onHover_) hover_->onHover_(false);
    }
    hover_ = w;
    if (hover_ && !hover_->disabled_) {
      hover_->hovered_ = true;
      if (hover_->onHover_) hover_->onHover_(true);
    }
  }

  void dispatchInput(float dt) {
    static const Action order[] = {Action::Up,   Action::Down,   Action::Left,  Action::Right, Action::Accept,
                                   Action::Back, Action::Extra,  Action::Option, Action::L1,    Action::R1,
                                   Action::L2,   Action::R2,     Action::Start,  Action::Select};
    for (Action a : order)
      if (input_.pressed(a)) handleAction(a);

    Vec2 s = input_.stick;
    if (s.x != 0.0f || s.y != 0.0f) {
      if (Widget* t = scrollTarget()) {
        float speed = 900.0f * dt;
        t->scrollBy(ctx_, {s.x * speed, s.y * speed});
      }
    }

    Input::Pointer& p = input_.pointer;
    bool scrim = false;
    Widget* under = nullptr;
    if (p.present) under = shell_->pick(p.pos, scrim);
    if (!capture_) setHover((!p.isTouch || p.down) && p.present ? under : nullptr);

    if (p.wheel.x != 0.0f || p.wheel.y != 0.0f) {
      for (Widget* w = under; w; w = w->parent_) {
        if (w->scrollBy(ctx_, {-p.wheel.x * 64.0f, -p.wheel.y * 64.0f})) break;
      }
    }

    if (p.pressed) {
      if (scrim) {
        shell_->dismissTop();
      } else if (under && !under->disabled_) {
        capture_ = under;
        stolen_ = false;
        pressStart_ = p.pos;
        if (under->focusable_) focus_.set(under);
        under->onPointerDown(ctx_, p.pos);
      }
    }

    if (capture_ && p.down && !p.pressed) {
      Vec2 total = p.pos - pressStart_;
      if (!stolen_ && !capture_->capturesDrag() && total.length() > 8.0f) {
        for (Widget* w = capture_; w; w = w->parent_) {
          if (w->wantsDrag(total)) {
            if (w != capture_) {
              capture_->onPointerCancel(ctx_);
              capture_ = w;
              capture_->onPointerDown(ctx_, p.pos - total * 0.0f);
            }
            stolen_ = true;
            break;
          }
        }
      }
      if (!stolen_ && !capture_->capturesDrag()) {
        capture_->pressed_ = capture_->rect_.contains(p.pos);
      }
      capture_->onPointerMove(ctx_, p.pos);
    }

    if (p.released && capture_) {
      Widget* c = capture_;
      capture_ = nullptr;
      bool inside = c->rect_.contains(p.pos);
      c->onPointerUp(ctx_, p.pos, inside && !stolen_);
      stolen_ = false;
      if (p.isTouch) setHover(nullptr);
    } else if (p.released && p.isTouch) {
      setHover(nullptr);
    }
  }
};

}
