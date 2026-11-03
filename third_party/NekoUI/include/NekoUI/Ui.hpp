#pragma once

#include "Engine.hpp"
#include "core/Icons.hpp"
#include "shell/Shell.hpp"
#include "shell/View.hpp"
#include "widgets/Button.hpp"
#include "widgets/Flex.hpp"
#include "widgets/HintBar.hpp"
#include "widgets/Image.hpp"
#include "widgets/Label.hpp"
#include "widgets/MusicController.hpp"
#include "widgets/Progress.hpp"
#include "widgets/Scroll.hpp"
#include "widgets/Slider.hpp"
#include "widgets/Toggle.hpp"

namespace neko::ui {

inline std::shared_ptr<Box> row(std::initializer_list<WidgetPtr> children = {}) {
  return Box::make(Direction::Row)->add(children);
}
inline std::shared_ptr<Box> column(std::initializer_list<WidgetPtr> children = {}) {
  return Box::make(Direction::Column)->add(children);
}
inline std::shared_ptr<Box> stack(std::initializer_list<WidgetPtr> children = {}) {
  return Box::make(Direction::Stack)->add(children);
}
inline std::shared_ptr<Box> card(std::initializer_list<WidgetPtr> children = {}) {
  return Box::make(Direction::Column)->cls("card")->add(children);
}

inline std::shared_ptr<Spacer> spacer(float px = 0.0f) { return Spacer::make(px); }
inline std::shared_ptr<Divider> divider() { return Divider::make()->cls("divider"); }

inline std::shared_ptr<Label> label(std::string text) { return Label::make(std::move(text)); }
inline std::shared_ptr<Label> title(std::string text) { return Label::make(std::move(text))->cls("title"); }
inline std::shared_ptr<Label> headline(std::string text) { return Label::make(std::move(text))->cls("headline"); }
inline std::shared_ptr<Label> caption(std::string text) { return Label::make(std::move(text))->cls("caption"); }
inline std::shared_ptr<Glyph> glyph(uint32_t cp) { return Glyph::make(cp)->cls("glyph"); }
inline std::shared_ptr<Image> image(std::string path) { return Image::make(std::move(path)); }

inline std::shared_ptr<Button> button(const std::string& text, std::function<void()> onClick = nullptr,
                                      const std::string& variant = "") {
  auto b = Button::make();
  if (!variant.empty()) b->cls(variant);
  b->add(Label::make(text));
  if (onClick) b->onClick(std::move(onClick));
  return b;
}

inline std::shared_ptr<Button> iconButton(uint32_t cp, std::function<void()> onClick = nullptr,
                                          const std::string& variant = "icon") {
  auto b = Button::make()->cls(variant);
  b->add(Glyph::make(cp));
  if (onClick) b->onClick(std::move(onClick));
  return b;
}

inline std::shared_ptr<Button> listItem(uint32_t icon, const std::string& text, std::function<void()> onClick = nullptr) {
  auto b = Button::make()->cls("list");
  if (icon) b->add(Glyph::make(icon));
  b->add(Label::make(text));
  if (onClick) b->onClick(std::move(onClick));
  return b;
}

inline std::shared_ptr<Button> navItem(uint32_t icon, std::function<void()> onClick = nullptr, bool checked = false,
                                       const std::string& group = "nav") {
  auto b = Button::make()->cls("nav");
  b->add(Glyph::make(icon));
  if (onClick) b->onClick(std::move(onClick));
  if (!group.empty()) b->group(group);
  b->checked_ = checked;
  return b;
}

inline std::shared_ptr<Slider> slider(float value = 0.0f, float lo = 0.0f, float hi = 1.0f,
                                      std::function<void(float)> onChange = nullptr) {
  auto s = Slider::make(value, lo, hi);
  if (onChange) s->onChange(std::move(onChange));
  return s;
}
inline std::shared_ptr<Toggle> toggle(bool on = false, std::function<void(bool)> onChange = nullptr) {
  auto t = Toggle::make(on);
  if (onChange) t->onChange(std::move(onChange));
  return t;
}
inline std::shared_ptr<Checkbox> checkbox(bool on = false, std::function<void(bool)> onChange = nullptr) {
  auto c = Checkbox::make(on);
  if (onChange) c->onChange(std::move(onChange));
  return c;
}
inline std::shared_ptr<Radio> radio(const std::string& group, bool on = false,
                                    std::function<void(bool)> onChange = nullptr) {
  auto r = Radio::make(group, on);
  if (onChange) r->onChange(std::move(onChange));
  return r;
}
inline std::shared_ptr<ProgressBar> progress(float value = 0.0f, bool wavy = false) {
  return ProgressBar::make(value)->wavy(wavy);
}
inline std::shared_ptr<CircularProgress> spinner() { return CircularProgress::make()->indeterminate(); }

inline std::shared_ptr<ScrollView> scroll(std::initializer_list<WidgetPtr> children = {},
                                          ScrollAxis axis = ScrollAxis::Vertical) {
  return ScrollView::make(axis)->add(children);
}
inline std::shared_ptr<ListView> list(int count, float itemHeight, ListView::Builder builder) {
  return ListView::make(count, itemHeight, std::move(builder));
}
inline std::shared_ptr<MusicController> musicPlayer(MusicPlayer* player = nullptr) {
  return MusicController::make(player);
}
inline std::shared_ptr<HintBar> hintBar() { return HintBar::make()->cls("hintbar"); }

}
