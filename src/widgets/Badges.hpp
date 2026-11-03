#pragma once

#include "../App.hpp"

namespace nt {

class DownloadBadge : public neko::ContainerT<DownloadBadge> {
 public:
  const char* typeName() const override { return "download-badge"; }

  static Ptr make(std::vector<std::string> ids, float size = 28.0f) {
    auto b = std::make_shared<DownloadBadge>();
    b->ids_ = std::move(ids);
    b->size_ = size;
    b->inline_.direction = neko::Direction::Stack;
    b->inline_.width = neko::Dim::px(size);
    b->inline_.height = neko::Dim::px(size);
    b->glyph_ = neko::Glyph::make(ic::file_download)->fontSize(size * 0.85f);
    b->glyph_->inline_.alignSelf = neko::Align::Center;
    b->glyph_->inline_.justifySelf = neko::Align::Center;
    b->ring_ = neko::CircularProgress::make(0.0f)->thickness(std::max(2.5f, size * 0.1f))->size(size, size);
    b->ring_->visible_ = false;
    b->addChild(b->glyph_);
    b->addChild(b->ring_);
    return b;
  }

  Ptr noGlyph() {
    noGlyph_ = true;
    return self();
  }

  bool interactive() const override { return false; }
  neko::Widget* hitTest(neko::Vec2) override { return nullptr; }

  void tick(neko::Context&, float) override {
    Services& s = svc();
    glyph_->visible_ = !noGlyph_;
    int done = 0, active = 0, queued = 0, failed = 0;
    float sum = 0;
    for (const std::string& id : ids_) {
      switch (s.downloads.state(id)) {
        case Downloads::State::Done: ++done; sum += 1.0f; break;
        case Downloads::State::Active: ++active; sum += s.downloads.progress(id); break;
        case Downloads::State::Queued: ++queued; break;
        case Downloads::State::Failed: ++failed; break;
        default: break;
      }
    }
    int n = std::max(1, (int)ids_.size());
    bool working = active + queued > 0;
    int mode = working ? 1 : (done == (int)ids_.size() && !ids_.empty() ? 2 : (failed > 0 ? 3 : 0));
    ring_->visible_ = working;
    if (working) {
      if (active > 0) ring_->value(sum / (float)n)->indeterminate(false);
      else ring_->indeterminate(true);
    }
    if (mode == lastMode_) return;
    lastMode_ = mode;
    float base = size_ * 0.85f;
    switch (mode) {
      case 1:
        glyph_->codepoint(ic::close);
        glyph_->inline_.fontSize = base * 0.55f;
        glyph_->inline_.color = neko::ColorRef("onSurfaceVariant");
        break;
      case 2:
        glyph_->codepoint(ic::check_circle);
        glyph_->inline_.fontSize = base;
        glyph_->inline_.color = neko::ColorRef("success");
        break;
      case 3:
        glyph_->codepoint(ic::error);
        glyph_->inline_.fontSize = base;
        glyph_->inline_.color = neko::ColorRef("error");
        break;
      default:
        glyph_->codepoint(ic::file_download);
        glyph_->inline_.fontSize = base;
        glyph_->inline_.color = neko::ColorRef("onSurfaceVariant");
        break;
    }
    glyph_->styleDirty_ = true;
  }

 private:
  std::vector<std::string> ids_;
  int lastMode_ = -1;
  bool noGlyph_ = false;
  float size_ = 28.0f;
  std::shared_ptr<neko::Glyph> glyph_;
  std::shared_ptr<neko::CircularProgress> ring_;
};

inline std::function<void()> downloadAction(std::vector<Song> songs) {
  return [songs]() {
    Services& s = svc();
    if (songs.empty()) return;
    bool anyBusy = false, allDone = true;
    for (const Song& x : songs) {
      auto st = s.downloads.state(x.id);
      if (st == Downloads::State::Active || st == Downloads::State::Queued) anyBusy = true;
      if (st != Downloads::State::Done) allDone = false;
    }
    if (anyBusy) {
      for (const Song& x : songs) s.downloads.cancel(x.id);
      return;
    }
    if (allDone) {
      for (const Song& x : songs) s.downloads.remove(x.id);
      notify(songs.size() > 1 ? "Removed downloads" : "Removed download", "", ic::delete_icon);
      return;
    }
    if (!s.online) {
      notify("You're offline", "Connect to the server to download", ic::cloud_off);
      return;
    }
    s.downloads.enqueueAll(songs);
    notify(songs.size() > 1 ? "Downloading " + std::to_string(songs.size()) + " songs" : "Downloading " + songs[0].title, "",
           ic::file_download);
  };
}

inline std::shared_ptr<neko::Button> downloadButton(std::vector<Song> songs, float size = 28.0f) {
  std::vector<std::string> ids;
  for (const Song& s : songs) ids.push_back(s.id);
  auto b = neko::Button::make()->cls("icon");
  b->inline_.padding = neko::Insets(8.0f);
  b->inline_.minWidth = 0.0f;
  b->inline_.minHeight = 0.0f;
  b->add(DownloadBadge::make(ids, size));
  b->onClick(downloadAction(std::move(songs)));
  return b;
}

class PlayingMark : public neko::WidgetT<PlayingMark> {
 public:
  const char* typeName() const override { return "playing-mark"; }

  static Ptr make(std::string id, int number = 0) {
    auto m = std::make_shared<PlayingMark>();
    m->id_ = std::move(id);
    m->number_ = number;
    return m;
  }

  bool interactive() const override { return false; }
  neko::Widget* hitTest(neko::Vec2) override { return nullptr; }
  neko::Vec2 contentSize(neko::Context&, neko::Vec2) override { return {28.0f, 24.0f}; }

  void tick(neko::Context&, float dt) override {
    const Song* c = svc().player->current();
    on_ = c && c->id == id_;
    playing_ = on_ && svc().player->playing();
    if (playing_) t_ += dt * 7.0f;
  }

  void drawContent(neko::Context& ctx) override {
    if (!on_) {
      if (number_ > 0) {
        std::string n = std::to_string(number_);
        neko::TextMetrics m = ctx.renderer->textSize(n, neko::FontFace::Text, 15);
        ctx.renderer->text(n, neko::FontFace::Text, 15, {rect_.cx() - (float)m.w * 0.5f, rect_.cy() - (float)m.h * 0.5f},
                           ctx.colors->get("onSurfaceVariant"));
      }
      return;
    }
    neko::Color col = ctx.colors->get("primary");
    for (int i = 0; i < 3; ++i) {
      float h = playing_ ? 6.0f + 12.0f * (0.5f + 0.5f * std::sin(t_ + (float)i * 1.9f)) : 8.0f;
      ctx.renderer->fillRRect({rect_.x + 3.0f + (float)i * 7.0f, rect_.bottom() - 3.0f - h, 4.0f, h}, 2.0f, col);
    }
  }

 private:
  std::string id_;
  int number_ = 0;
  bool on_ = false, playing_ = false;
  float t_ = 0;
};

}
