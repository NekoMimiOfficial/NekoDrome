#pragma once

#include <cstdio>

#include "../core/Icons.hpp"
#include "Button.hpp"
#include "Label.hpp"
#include "Progress.hpp"
#include "Slider.hpp"

namespace neko {

class MusicController : public ContainerT<MusicController> {
 public:
  const char* typeName() const override { return "music-controller"; }
  std::function<void()> onPrevious, onNext, onToggle;

  static Ptr make(MusicPlayer* player = nullptr) {
    auto m = std::make_shared<MusicController>();
    m->player_ = player;
    m->build();
    return m;
  }

  Ptr bind(MusicPlayer* p) {
    player_ = p;
    return self();
  }

  bool interactive() const override { return false; }

  void tick(Context& ctx, float) override {
    if (!player_) player_ = ctx.audio;
    MusicPlayer* p = player_;
    if (!p) return;
    std::string title = p->title().empty() ? "Nothing playing" : p->title();
    title_->text(title);
    bool buffering = p->buffering();
    playGlyph_->codepoint(p->playing() ? icons::pause : icons::play_arrow);
    playGlyph_->visible_ = !buffering;
    spinner_->visible_ = buffering;
    if (!seek_->dragging()) seek_->value(p->progress());
    cur_->text(format(p->position()));
    total_->text(p->duration() > 0 ? format(p->duration()) : "--:--");
  }

 private:
  MusicPlayer* player_ = nullptr;
  std::shared_ptr<Label> title_, cur_, total_;
  std::shared_ptr<Slider> seek_;
  std::shared_ptr<Glyph> playGlyph_;
  std::shared_ptr<CircularProgress> spinner_;

  static std::string format(double s) {
    int t = (int)std::max(0.0, s);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", t / 60, t % 60);
    return buf;
  }

  void build() {
    inline_.direction = Direction::Column;
    title_ = Label::make("Nothing playing")->cls("music-title");
    cur_ = Label::make("0:00")->cls("caption");
    total_ = Label::make("--:--")->cls("caption");
    seek_ = Slider::make(0.0f)->onCommit([this](float v) {
      if (player_) player_->seekNormalized(v);
    });
    auto timeRow = Box::make(Direction::Row)->justify(Justify::SpaceBetween)->fillWidth()->add({cur_, total_});

    auto prev = Button::make()->cls("icon")->onClick([this]() {
      if (onPrevious) onPrevious();
      else if (player_) player_->previous();
    });
    prev->add(Glyph::make(icons::skip_previous));
    playGlyph_ = Glyph::make(icons::play_arrow);
    spinner_ = CircularProgress::make()->indeterminate()->thickness(3.0f)->size(28, 28);
    spinner_->visible_ = false;
    auto play = Button::make()->cls("icon-filled")->direction(Direction::Stack)->onClick([this]() {
      if (onToggle) onToggle();
      else if (player_) player_->toggle();
    });
    play->add(playGlyph_)->add(spinner_);
    auto next = Button::make()->cls("icon")->onClick([this]() {
      if (onNext) onNext();
      else if (player_) player_->next();
    });
    next->add(Glyph::make(icons::skip_next));
    auto controls = Box::make(Direction::Row)->gap(12)->align(Align::Center)->add({prev, play, next});
    auto controlsWrap = Box::make(Direction::Row)->justify(Justify::Center)->fillWidth()->add(controls);

    this->gap(6);
    addChild(title_);
    addChild(seek_->fillWidth());
    addChild(timeRow);
    addChild(controlsWrap);
  }
};

}
