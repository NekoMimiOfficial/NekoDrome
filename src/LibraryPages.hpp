#pragma once

#include "BrowsePages.hpp"

namespace nt {

class NowPlayingPanel : public ContainerT<NowPlayingPanel> {
 public:
  const char* typeName() const override { return "now-playing"; }

  static Ptr make() {
    auto p = std::make_shared<NowPlayingPanel>();
    p->build();
    return p;
  }

  bool interactive() const override { return false; }

  void tick(Context&, float) override {
    PlayerCore& p = *svc().player;
    const Song* s = p.current();
    if (p.revision() != rev_) {
      rev_ = p.revision();
      refreshSong(s);
    }
    bool has = s != nullptr;
    seek_->active(p.playing())->value(p.progress());
    cur_->text(formatDuration(p.position()));
    total_->text(has ? formatDuration(p.duration()) : "0:00");
    playGlyph_->codepoint(p.playing() ? ic::pause : ic::play_arrow);
    bool buf = p.buffering();
    playGlyph_->visible_ = !buf;
    ring_->visible_ = buf;
    if (buf) {
      float bp = p.bufferProgress();
      if (bp > 0.001f) ring_->value(bp)->indeterminate(false);
      else ring_->indeterminate(true);
    }
    shuffle_->checked_ = p.shuffle();
    repeat_->checked_ = p.repeat() != PlayerCore::Repeat::Off;
    repeatGlyph_->codepoint(p.repeat() == PlayerCore::Repeat::One ? ic::repeat_one : ic::repeat);
    if (!volumeSlider_->dragging()) volumeSlider_->value(p.volume());
  }

 private:
  uint32_t rev_ = 0;
  std::shared_ptr<Cover> cover_;
  std::shared_ptr<Label> title_, artist_, album_, cur_, total_, quality_;
  std::shared_ptr<WavySeek> seek_;
  std::shared_ptr<Glyph> playGlyph_, repeatGlyph_, favGlyph_;
  std::shared_ptr<CircularProgress> ring_;
  std::shared_ptr<Button> shuffle_, repeat_;
  std::shared_ptr<Box> dlHolder_;
  std::shared_ptr<Slider> volumeSlider_;

  void refreshSong(const Song* s) {
    title_->text(s ? s->title : "Nothing playing");
    artist_->text(s ? s->artist : "Pick something from your library");
    album_->text(s ? s->album : "");
    cover_->coverId(s ? s->coverArt : "");
    bool fav = s && s->starred;
    favGlyph_->codepoint(fav ? ic::favorite : ic::favorite_border);
    favGlyph_->inline_.color = ColorRef(fav ? "primary" : "onSurfaceVariant");
    favGlyph_->markStyleDirty();
    dlHolder_->clear();
    if (s) dlHolder_->add(downloadButton({*s}, 30.0f));
    if (s) {
      std::string q;
      if (!s->suffix.empty()) q += s->suffix;
      if (s->bitRate > 0) q += (q.empty() ? "" : " \xC2\xB7 ") + std::to_string(s->bitRate) + " kbps";
      quality_->text(q);
    } else {
      quality_->text("");
    }
  }

  void build() {
    inline_.direction = Direction::Row;
    inline_.align = Align::Center;
    const platform::UiProfile& u = platform::uiProfile();
    inline_.gap = u.nowPlayingGap;
    inline_.padding = Insets(u.nowPlayingPadX, u.nowPlayingPadY);
    inline_.width = Dim::fill();
    inline_.height = Dim::fill();

    cover_ = Cover::make("", u.nowPlayingCover, 600)->radius(34.0f * u.nowPlayingCover / 400.0f);
    addChild(cover_);

    auto col = Box::make(Direction::Column)->gap(8)->fillWidth();
    title_ = Label::make("Nothing playing")->cls("title");
    artist_ = Label::make("")->cls("subtitle")->fg("primary");
    album_ = Label::make("")->cls("caption");
    quality_ = Label::make("")->cls("caption");
    col->add(title_)->add(artist_)->add(album_)->add(quality_);
    col->add(Spacer::make(10.0f));

    seek_ = WavySeek::make()->cls("wavyseek")->onSeek([](float v) { svc().player->seekNormalized(v); });
    col->add(seek_);
    cur_ = Label::make("0:00")->cls("caption");
    total_ = Label::make("0:00")->cls("caption");
    col->add(Box::make(Direction::Row)->fillWidth()->add({cur_, Spacer::make(), total_}));

    shuffle_ = Button::make()->cls("icon")->onClick([]() { svc().player->setShuffle(!svc().player->shuffle()); });
    shuffle_->add(Glyph::make(ic::shuffle));
    auto prev = iconBtn(ic::skip_previous, []() { svc().player->previous(); });
    playGlyph_ = Glyph::make(ic::play_arrow)->fontSize(48.0f);
    ring_ = CircularProgress::make()->indeterminate()->thickness(4.0f)->size(46, 46);
    ring_->visible_ = false;
    auto play = Button::make()->cls("icon-filled")->direction(Direction::Stack)->onClick([]() { svc().player->toggle(); });
    play->inline_.padding = Insets(14.0f);
    play->inline_.minWidth = 76.0f;
    play->inline_.minHeight = 76.0f;
    play->inline_.alignSelf = Align::Center;
    play->add(playGlyph_)->add(ring_);
    auto next = iconBtn(ic::skip_next, []() { svc().player->next(); });
    for (auto& b : {prev, next, shuffle_}) {
      b->inline_.padding = Insets(14.0f);
      b->inline_.minWidth = 56.0f;
      b->inline_.minHeight = 56.0f;
    }
    repeat_ = Button::make()->cls("icon")->onClick([]() { svc().player->cycleRepeat(); });
    repeatGlyph_ = Glyph::make(ic::repeat);
    repeat_->add(repeatGlyph_);
    repeat_->inline_.padding = Insets(14.0f);
    repeat_->inline_.minWidth = 56.0f;
    repeat_->inline_.minHeight = 56.0f;
    auto controls = Box::make(Direction::Row)->gap(10)->align(Align::Center)->justify(Justify::Center)->fillWidth();
    controls->add({shuffle_, prev, play, next, repeat_});
    col->add(controls);

    favGlyph_ = Glyph::make(ic::favorite_border);
    auto fav = Button::make()->cls("icon")->onClick([this]() {
      PlayerCore& p = *svc().player;
      const Song* s = p.current();
      if (!s) return;
      bool ns = !s->starred;
      std::string id = s->id;
      p.setStarred(id, ns);
      svc().store.updateOfflineStar(id, ns);
      svc().api.setStarred(id, ns, [](bool ok) {
        if (!ok) notify("Couldn't update favorite", "", ic::error);
      });
    });
    fav->add(favGlyph_);
    dlHolder_ = Box::make(Direction::Row);
    auto queueBtn = iconBtn(ic::queue_music, []() { if (nav().openQueue) nav().openQueue(); });
    volumeSlider_ = Slider::make(0.8f, 0.0f, 1.0f)->onChange([](float v) { svc().player->setVolume(v); });
    volumeSlider_->inline_.width = Dim::px(180.0f);
    auto extra = Box::make(Direction::Row)->gap(8)->align(Align::Center)->justify(Justify::Center)->fillWidth();
    extra->add({fav, dlHolder_, queueBtn, Glyph::make(ic::volume_up)->fg("onSurfaceVariant"), volumeSlider_});
    col->add(Spacer::make(4.0f));
    col->add(extra);
    addChild(col);
  }
};

class NowPlayingPage : public Page {
 public:
  NowPlayingPage() : Page("Now playing", false) {
    pending_->bind(Action::L2, "Seek -10s", []() { seekBy(-10.0); }, false);
    pending_->bind(Action::R2, "Seek +10s", []() { seekBy(10.0); }, false);
    topBar()->add(iconBtn(ic::queue_music, []() { if (nav().openQueue) nav().openQueue(); }));
    panel.show(NowPlayingPanel::make());
  }

 private:
  static void seekBy(double d) {
    PlayerCore& p = *svc().player;
    if (p.duration() > 0) p.seekNormalized((float)((p.position() + d) / p.duration()));
  }
};

class QueueDrawer : public ContainerT<QueueDrawer> {
 public:
  const char* typeName() const override { return "queue-drawer"; }

  static Ptr make() {
    auto q = std::make_shared<QueueDrawer>();
    q->build();
    return q;
  }

  bool interactive() const override { return false; }

  void tick(Context&, float) override {
    PlayerCore& p = *svc().player;
    if (p.revision() == rev_) return;
    rev_ = p.revision();
    list_->count((int)p.queue().size());
    if (p.index() >= 0) list_->scrollToIndex(p.index(), false);
    summary_->text(std::to_string(p.queue().size()) + (p.queue().size() == 1 ? " song" : " songs"));
  }

 private:
  uint32_t rev_ = 0;
  std::shared_ptr<ListView> list_;
  std::shared_ptr<Label> summary_;

  void build() {
    inline_.direction = Direction::Column;
    inline_.gap = 8.0f;
    inline_.width = Dim::fill();
    inline_.height = Dim::fill();
    summary_ = Label::make("")->cls("caption");
    auto head = Box::make(Direction::Row)->align(Align::Center)->fillWidth()->padding(Insets(8.0f, 4.0f));
    auto titles = Box::make(Direction::Column)->fillWidth();
    titles->add(Label::make("Up next")->cls("headline"))->add(summary_);
    auto clear = ui::button("Clear queue", []() { svc().player->clearQueue(); }, "text");
    head->add(titles);
    list_ = ListView::make(0, 62.0f, [](int i) {
      PlayerCore& p = *svc().player;
      const Song s = p.queue()[(size_t)i];
      auto row = Box::make(Direction::Row)->gap(2)->align(Align::Center);
      auto b = Button::make()->cls("list")->fillWidth();
      b->inline_.padding = Insets(10.0f, 8.0f);
      b->inline_.gap = 12.0f;
      b->add(PlayingMark::make(s.id));
      b->add(Cover::make(s.coverArt, 40.0f, 300)->radius(8.0f));
      auto t = Box::make(Direction::Column)->gap(1)->fillWidth();
      t->add(Label::make(s.title)->cls("row-title"));
      t->add(Label::make(s.artist)->cls("caption"));
      b->add(t);
      b->onClick([i]() { svc().player->playIndex(i); });
      auto rm = iconBtn(ic::close, [i]() { svc().player->removeAt(i); });
      rm->inline_.padding = Insets(8.0f);
      rm->inline_.minWidth = 0.0f;
      rm->inline_.minHeight = 0.0f;
      row->add(b)->add(rm);
      return row;
    });
    list_->fill()->gap(2);
    addChild(head);
    addChild(list_);
    addChild(clear);
  }
};

class OfflinePage : public Page {
 public:
  OfflinePage() : Page("Offline") {
    bindQueueKeys(pending_);
    songs_ = std::make_shared<std::vector<Song>>();
    badgeHolder_ = Box::make(Direction::Row);
    topBar()->add(badgeHolder_);
    summary_ = Label::make("")->cls("caption")->fillWidth();
    list_ = ListView::make(0, 64.0f, [this](int i) {
      auto r = songRow(songs_, i, true);
      r->inline_.margin = Insets(14.0f, 0.0f);
      return r;
    });
    list_->fill()->gap(2);
    emptyBox_ = centered({ui::glyph(ic::offline_pin)->fontSize(56)->fg(ColorRef("onSurfaceVariant").a(0.5f)),
                          ui::label("No downloaded songs yet")->fg("onSurfaceVariant"),
                          ui::caption("Use the download button on any song or album")});
    area_ = Box::make(Direction::Stack)->fill();
    area_->add(list_)->add(emptyBox_);
    activeBox_ = Box::make(Direction::Column)->gap(2)->padding(Insets(14.0f, 0.0f));
    banner_ = Box::make(Direction::Row)->cls("banner")->margin(Insets(20.0f, 4.0f, 20.0f, 0.0f));
    banner_->add(Glyph::make(ic::cloud_off));
    banner_->add(Label::make("Offline mode")->fillWidth());
    banner_->add(ui::button("Reconnect", []() { if (nav().reconnect) nav().reconnect(); }, "tonal"));

    auto actions = Box::make(Direction::Row)->gap(10)->align(Align::Center)->padding(Insets(20.0f, 12.0f))->fillWidth();
    auto play = playButton("Play all", ic::play_arrow, [this]() {
      if (songs_->empty()) return;
      svc().player->setShuffle(false);
      svc().player->setQueue(*songs_, 0, true);
    });
    auto shuf = playButton("Shuffle", ic::shuffle, [this]() {
      if (songs_->empty()) return;
      svc().player->setShuffle(true);
      svc().player->setQueue(*songs_, 0, true);
    }, "tonal");
    auto del = iconBtn(ic::delete_icon, [this]() {
      if (songs_->empty()) return;
      svc().ui->shell().dialog("Delete all downloads?", "This removes every offline song from this device.",
                               {{"Cancel", nullptr}, {"Delete", []() {
                                  svc().downloads.cancelAll();
                                  svc().store.clearOffline();
                                  notify("Downloads deleted", "", ic::delete_icon);
                                }}});
    });
    actions->add({summary_, play, shuf, del});

    auto col = Box::make(Direction::Column)->fill();
    col->add({banner_, actions, activeBox_, area_});
    panel.show(col);
  }

  void start() override {
    rebuild();
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) static_cast<OfflinePage*>(p.get())->rebuild();
    });
  }

  void update(float) override {
    if (rev_ != svc().downloads.revision() || online_ != svc().online) rebuild();
  }

 private:
  std::shared_ptr<std::vector<Song>> songs_;
  std::shared_ptr<ListView> list_;
  std::shared_ptr<Box> activeBox_, badgeHolder_, emptyBox_, area_, banner_;
  std::shared_ptr<Label> summary_;
  uint32_t rev_ = 0;
  bool online_ = true;

  void rebuild() {
    Services& s = svc();
    rev_ = s.downloads.revision();
    online_ = s.online;
    banner_->visible_ = !s.online;
    std::vector<Song> list;
    for (const auto& e : s.store.offline()) list.push_back(e.song);
    std::sort(list.begin(), list.end(), [](const Song& a, const Song& b) {
      if (a.artist != b.artist) return lower(a.artist) < lower(b.artist);
      if (a.album != b.album) return lower(a.album) < lower(b.album);
      return a.disc != b.disc ? a.disc < b.disc : a.track < b.track;
    });
    *songs_ = std::move(list);
    list_->count((int)songs_->size());
    emptyBox_->visible_ = songs_->empty();
    list_->visible_ = !songs_->empty();
    summary_->text(std::to_string(songs_->size()) + (songs_->size() == 1 ? " song" : " songs") + " \xC2\xB7 " +
                   humanSize(s.store.offlineBytes()));

    activeBox_->clear();
    badgeHolder_->clear();
    std::vector<Song> activeSongs;
    for (const auto& it : s.downloads.items()) activeSongs.push_back(it->song);
    if (!activeSongs.empty()) {
      activeBox_->add(Label::make("Downloading")->cls("section-title")->margin(Insets(6.0f, 4.0f, 0.0f, 6.0f)));
      for (const auto& it : s.downloads.items()) {
        auto row = Box::make(Direction::Row)->gap(12)->align(Align::Center)->padding(Insets(10.0f, 4.0f));
        row->add(Cover::make(it->song.coverArt, 40.0f, 300)->radius(8.0f));
        auto t = Box::make(Direction::Column)->gap(1)->fillWidth();
        t->add(Label::make(it->song.title)->cls("row-title"));
        std::string st = it->state == Downloads::State::Failed ? "Failed: " + it->error : (it->state == Downloads::State::Queued ? "Waiting" : "Downloading");
        t->add(Label::make(it->song.artist + " \xC2\xB7 " + st)->cls("caption"));
        row->add(t);
        row->add(downloadButton({it->song}, 34.0f));
        activeBox_->add(row);
      }
      std::vector<std::string> ids;
      for (const Song& x : activeSongs) ids.push_back(x.id);
      badgeHolder_->add(Label::make(std::to_string(activeSongs.size()) + " downloading")->cls("caption")->margin(Insets(0.0f, 0.0f, 8.0f, 0.0f)));
      badgeHolder_->add(DownloadBadge::make(ids, 30.0f)->noGlyph());
    }
  }
};

}
