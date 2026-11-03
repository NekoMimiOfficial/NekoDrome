#pragma once

#include "Common.hpp"
#include "Style.hpp"

namespace neko {

enum class Action {
  Up, Down, Left, Right, Accept, Back, Extra, Option, L1, R1, L2, R2, Start, Select, Count
};
constexpr int kActionCount = (int)Action::Count;

struct KeyBind {
  Action action = Action::Accept;
  std::string label;
  std::function<void()> callback;
  bool hint = true;
};

struct GlyphSpec {
  std::string text;
  FontFace face = FontFace::Text;
};

inline std::map<Action, GlyphSpec> defaultActionGlyphs() {
  return {
      {Action::Accept, {"Cross", FontFace::Text}},   {Action::Back, {"Circle", FontFace::Text}},
      {Action::Extra, {"Square", FontFace::Text}},   {Action::Option, {"Triangle", FontFace::Text}},
      {Action::L1, {"L", FontFace::Text}},           {Action::R1, {"R", FontFace::Text}},
      {Action::L2, {"L2", FontFace::Text}},          {Action::R2, {"R2", FontFace::Text}},
      {Action::Start, {"Start", FontFace::Text}},    {Action::Select, {"Select", FontFace::Text}},
      {Action::Up, {"Up", FontFace::Text}},          {Action::Down, {"Down", FontFace::Text}},
      {Action::Left, {"Left", FontFace::Text}},      {Action::Right, {"Right", FontFace::Text}},
  };
}

class Input {
 public:
  struct Pointer {
    Vec2 pos;
    Vec2 delta;
    bool down = false, pressed = false, released = false, moved = false;
    bool present = false, isTouch = false;
    Vec2 wheel;
  } pointer;

  Vec2 stick;
  bool padActive = true;
  bool circleAccept = false;
  bool keyboardAlways = false;
  bool captureKeyboard = false;
  float repeatDelay = 0.38f;
  float repeatRate = 0.085f;
  std::map<Action, GlyphSpec> glyphs = defaultActionGlyphs();

  void init(Vec2 viewport) {
    viewport_ = viewport;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) openPad(i);
  }
  void shutdown() {
    if (pad_) SDL_GameControllerClose(pad_);
    pad_ = nullptr;
  }
  void setViewport(Vec2 v) { viewport_ = v; }
  bool hasPad() const { return pad_ != nullptr; }

  void beginFrame() {
    for (int i = 0; i < kActionCount; ++i) pressedEdge_[i] = releasedEdge_[i] = false;
    pointer.pressed = pointer.released = pointer.moved = false;
    pointer.delta = {0, 0};
    pointer.wheel = {0, 0};
  }

  bool pressed(Action a) const { return pressedEdge_[(int)a] || repeated_[(int)a]; }
  bool justPressed(Action a) const { return pressedEdge_[(int)a]; }
  bool released(Action a) const { return releasedEdge_[(int)a]; }
  bool held(Action a) const { return mask_[(int)a] != 0; }

  void update(float dt) {
    for (int i = 0; i < kActionCount; ++i) {
      repeated_[i] = false;
      if (!mask_[i]) { holdTime_[i] = 0; continue; }
      Action a = (Action)i;
      bool rep = a == Action::Up || a == Action::Down || a == Action::Left || a == Action::Right;
      if (!rep) continue;
      holdTime_[i] += dt;
      if (holdTime_[i] >= repeatDelay) {
        holdTime_[i] -= repeatRate;
        repeated_[i] = true;
      }
    }
  }

  void handle(const SDL_Event& e) {
    switch (e.type) {
      case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
      case SDL_CONTROLLERDEVICEREMOVED:
        if (pad_ && SDL_GameControllerFromInstanceID(e.cdevice.which) == pad_) {
          SDL_GameControllerClose(pad_);
          pad_ = nullptr;
        }
        break;
      case SDL_CONTROLLERBUTTONDOWN: padButton(e.cbutton.button, true); break;
      case SDL_CONTROLLERBUTTONUP: padButton(e.cbutton.button, false); break;
      case SDL_CONTROLLERAXISMOTION: padAxis(e.caxis.axis, e.caxis.value); break;
      case SDL_KEYDOWN:
        if (!e.key.repeat) key(e.key.keysym.sym, true);
        break;
      case SDL_KEYUP: key(e.key.keysym.sym, false); break;
      case SDL_MOUSEMOTION:
        if (e.motion.which == SDL_TOUCH_MOUSEID) break;
        pointerMove({(float)e.motion.x, (float)e.motion.y}, false);
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) break;
        pointerMove({(float)e.button.x, (float)e.button.y}, false);
        pointerDown(false);
        break;
      case SDL_MOUSEBUTTONUP:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) break;
        pointerMove({(float)e.button.x, (float)e.button.y}, false);
        pointerUp(false);
        break;
      case SDL_MOUSEWHEEL:
        pointer.wheel.x += (float)e.wheel.x;
        pointer.wheel.y += (float)e.wheel.y;
        break;
      case SDL_FINGERDOWN:
        if (!frontTouch(e.tfinger.touchId)) break;
        pointerMove({e.tfinger.x * viewport_.x, e.tfinger.y * viewport_.y}, true);
        pointerDown(true);
        break;
      case SDL_FINGERMOTION:
        if (!frontTouch(e.tfinger.touchId)) break;
        pointerMove({e.tfinger.x * viewport_.x, e.tfinger.y * viewport_.y}, true);
        break;
      case SDL_FINGERUP:
        if (!frontTouch(e.tfinger.touchId)) break;
        pointerMove({e.tfinger.x * viewport_.x, e.tfinger.y * viewport_.y}, true);
        pointerUp(true);
        break;
      default: break;
    }
  }

  GlyphSpec glyphFor(Action a) const {
    auto it = glyphs.find(a);
    return it == glyphs.end() ? GlyphSpec{"?", FontFace::Text} : it->second;
  }

 private:
  SDL_GameController* pad_ = nullptr;
  Vec2 viewport_{960, 544};
  uint8_t mask_[kActionCount] = {};
  bool pressedEdge_[kActionCount] = {};
  bool releasedEdge_[kActionCount] = {};
  bool repeated_[kActionCount] = {};
  float holdTime_[kActionCount] = {};

  void openPad(int index) {
    if (pad_ || !SDL_IsGameController(index)) return;
    pad_ = SDL_GameControllerOpen(index);
  }

  bool frontTouch(SDL_TouchID id) const {
    SDL_TouchID front = SDL_GetNumTouchDevices() > 0 ? SDL_GetTouchDevice(0) : id;
    return id == front;
  }

  void setSource(Action a, uint8_t bit, bool down) {
    int i = (int)a;
    if (circleAccept) {
      if (a == Action::Accept) i = (int)Action::Back;
      else if (a == Action::Back) i = (int)Action::Accept;
    }
    bool was = mask_[i] != 0;
    if (down) mask_[i] |= bit;
    else mask_[i] &= ~bit;
    bool now = mask_[i] != 0;
    if (now && !was) {
      pressedEdge_[i] = true;
      holdTime_[i] = 0;
      padActive = true;
    }
    if (!now && was) releasedEdge_[i] = true;
  }

  void padButton(Uint8 b, bool down) {
    Action a;
    switch (b) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: a = Action::Up; break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: a = Action::Down; break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: a = Action::Left; break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: a = Action::Right; break;
      case SDL_CONTROLLER_BUTTON_A: a = Action::Accept; break;
      case SDL_CONTROLLER_BUTTON_B: a = Action::Back; break;
      case SDL_CONTROLLER_BUTTON_X: a = Action::Extra; break;
      case SDL_CONTROLLER_BUTTON_Y: a = Action::Option; break;
      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: a = Action::L1; break;
      case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: a = Action::R1; break;
      case SDL_CONTROLLER_BUTTON_START: a = Action::Start; break;
      case SDL_CONTROLLER_BUTTON_BACK: a = Action::Select; break;
      default: return;
    }
    setSource(a, 1, down);
  }

  void padAxis(Uint8 axis, Sint16 v) {
    float f = v / 32767.0f;
    const float th = 0.55f;
    switch (axis) {
      case SDL_CONTROLLER_AXIS_LEFTX:
        setSource(Action::Left, 4, f < -th);
        setSource(Action::Right, 4, f > th);
        break;
      case SDL_CONTROLLER_AXIS_LEFTY:
        setSource(Action::Up, 4, f < -th);
        setSource(Action::Down, 4, f > th);
        break;
      case SDL_CONTROLLER_AXIS_RIGHTX: stick.x = std::fabs(f) > 0.18f ? f : 0.0f; break;
      case SDL_CONTROLLER_AXIS_RIGHTY: stick.y = std::fabs(f) > 0.18f ? f : 0.0f; break;
      case SDL_CONTROLLER_AXIS_TRIGGERLEFT: setSource(Action::L2, 4, f > 0.5f); break;
      case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: setSource(Action::R2, 4, f > 0.5f); break;
      default: break;
    }
  }

  void key(SDL_Keycode k, bool down) {
    if (captureKeyboard && down) return;
    if (pad_ && !keyboardAlways) return;
    switch (k) {
      case SDLK_UP: setSource(Action::Up, 2, down); break;
      case SDLK_DOWN: setSource(Action::Down, 2, down); break;
      case SDLK_LEFT: setSource(Action::Left, 2, down); break;
      case SDLK_RIGHT: setSource(Action::Right, 2, down); break;
      case SDLK_z:
      case SDLK_RETURN: setSource(Action::Accept, 2, down); break;
      case SDLK_x:
      case SDLK_ESCAPE: setSource(Action::Back, 2, down); break;
      case SDLK_a: setSource(Action::Extra, 2, down); break;
      case SDLK_s: setSource(Action::Option, 2, down); break;
      case SDLK_q: setSource(Action::L1, 2, down); break;
      case SDLK_w: setSource(Action::R1, 2, down); break;
      case SDLK_SPACE: setSource(Action::Start, 2, down); break;
      case SDLK_v: setSource(Action::Select, 2, down); break;
      case SDLK_i: stick.y = down ? -1.0f : (stick.y < 0 ? 0.0f : stick.y); break;
      case SDLK_k: stick.y = down ? 1.0f : (stick.y > 0 ? 0.0f : stick.y); break;
      case SDLK_j: stick.x = down ? -1.0f : (stick.x < 0 ? 0.0f : stick.x); break;
      case SDLK_l: stick.x = down ? 1.0f : (stick.x > 0 ? 0.0f : stick.x); break;
      default: break;
    }
  }

  void pointerMove(Vec2 p, bool touch) {
    Vec2 d = p - pointer.pos;
    if (pointer.present || pointer.down) {
      pointer.delta = pointer.delta + d;
      if (d.length() > 0.0f) pointer.moved = true;
    }
    pointer.pos = p;
    pointer.isTouch = touch;
    if (!touch) pointer.present = true;
    if (d.length() > 2.0f || pointer.down) padActive = false;
  }
  void pointerDown(bool touch) {
    pointer.down = pointer.pressed = true;
    pointer.present = true;
    pointer.isTouch = touch;
    padActive = false;
  }
  void pointerUp(bool touch) {
    pointer.down = false;
    pointer.released = true;
    if (touch) pointer.present = false;
  }
};

}
