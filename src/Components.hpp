#pragma once

#include "App.hpp"
#include "widgets/Badges.hpp"
#include "widgets/Cover.hpp"
#include "widgets/TextField.hpp"
#include "widgets/WavySeek.hpp"

namespace nt {

using namespace neko;

inline Song& focusedSong() {
  static Song s;
  return s;
}

inline std::shared_ptr<Box> centered(std::initializer_list<WidgetPtr> items) {
  return Box::make(Direction::Column)->fill()->align(Align::Center)->justify(Justify::Center)->gap(14)->add(items);
}

struct Panel {
  std::shared_ptr<Box> box = Box::make(Direction::Column)->fill();

  void loading(const std::string& msg = "Loading") {
    box->clear();
    box->add(centered({ui::spinner(), ui::caption(msg)}));
  }
  void empty(uint32_t glyph, const std::string& text) {
    box->clear();
    box->add(centered({ui::glyph(glyph)->fontSize(56)->fg(ColorRef("onSurfaceVariant").a(0.5f)), ui::label(text)->fg("onSurfaceVariant")}));
  }
  void error(const std::string& msg, std::function<void()> retry) {
    box->clear();
    auto retryBtn = ui::button("Retry", std::move(retry), "tonal");
    box->add(centered({ui::glyph(ic::cloud_off)->fontSize(56)->fg("error"), ui::label(msg)->fg("onSurfaceVariant"), retryBtn}));
  }
  void show(WidgetPtr w) {
    box->clear();
    box->add(std::move(w));
  }
};

inline std::shared_ptr<Button> iconBtn(uint32_t cp, std::function<void()> fn, const std::string& variant = "icon") {
  auto b = ui::iconButton(cp, std::move(fn), variant);
  return b;
}

inline std::shared_ptr<Button> starButton(std::shared_ptr<std::vector<Song>> songs, int i) {
  bool st = (*songs)[(size_t)i].starred;
  auto g = Glyph::make(st ? ic::favorite : ic::favorite_border);
  g->inline_.color = ColorRef(st ? "primary" : "onSurfaceVariant");
  Glyph* gp = g.get();
  auto b = Button::make()->cls("icon");
  b->inline_.padding = Insets(8.0f);
  b->inline_.minWidth = 0.0f;
  b->inline_.minHeight = 0.0f;
  b->add(g);
  b->onClick([songs, i, gp]() {
    Song& s = (*songs)[(size_t)i];
    bool ns = !s.starred;
    s.starred = ns;
    gp->codepoint(ns ? ic::favorite : ic::favorite_border);
    gp->inline_.color = ColorRef(ns ? "primary" : "onSurfaceVariant");
    gp->markStyleDirty();
    Services& sv = svc();
    sv.player->setStarred(s.id, ns);
    sv.store.updateOfflineStar(s.id, ns);
    sv.api.setStarred(s.id, ns, [](bool ok) {
      if (!ok) notify("Couldn't update favorite", "", ic::error);
    });
  });
  return b;
}

inline WidgetPtr songRow(std::shared_ptr<std::vector<Song>> songs, int i, bool withCover = true, bool numbered = false) {
  const Song s0 = (*songs)[(size_t)i];
  auto row = Box::make(Direction::Row)->gap(4)->align(Align::Center);
  auto main = Button::make()->cls("list")->fillWidth();
  main->add(PlayingMark::make(s0.id, numbered && s0.track > 0 ? s0.track : 0));
  if (withCover) main->add(Cover::make(s0.coverArt, 44.0f, 300)->radius(10.0f));
  auto texts = Box::make(Direction::Column)->gap(2)->fillWidth();
  texts->add(Label::make(s0.title)->cls("row-title"));
  texts->add(Label::make(songSubtitle(s0))->cls("caption"));
  main->add(texts);
  main->add(Label::make(formatDuration(s0.duration))->cls("caption"));
  main->onClick([songs, i]() { svc().player->setQueue(*songs, i, true); });
  main->onFocus([songs, i]() { focusedSong() = (*songs)[(size_t)i]; });
  row->add(main);
  row->add(starButton(songs, i));
  row->add(downloadButton({s0}, 26.0f));
  return row;
}

inline std::shared_ptr<Button> albumTile(const Album& a) {
  float w = platform::uiProfile().tileWidth;
  auto b = Button::make()->cls("tile")->width(Dim::px(w));
  b->add(Cover::make(a.coverArt, w - 20.0f, 300)->radius(14.0f));
  b->add(Label::make(a.name)->cls("tile-title"));
  std::string sub = a.artist;
  if (a.year > 0) sub += (sub.empty() ? "" : " \xC2\xB7 ") + std::to_string(a.year);
  b->add(Label::make(sub)->cls("caption"));
  std::string id = a.id;
  b->onClick([id]() { if (nav().openAlbum) nav().openAlbum(id); });
  return b;
}

inline WidgetPtr tileRow(const std::vector<Album>& albums) {
  auto row = Box::make(Direction::Row)->gap(platform::uiProfile().tileGap);
  for (const Album& a : albums) row->add(albumTile(a));
  auto sv = ScrollView::make(ScrollAxis::Horizontal);
  sv->add(row);
  sv->fillWidth();
  sv->height(Dim::px(platform::uiProfile().tileWidth + 46.0f));
  return sv;
}

class AlbumGrid {
 public:
  std::shared_ptr<ListView> list;
  std::function<void()> loadMore;

  AlbumGrid() {
    cols_ = platform::uiProfile().albumColumns;
    albums_ = std::make_shared<std::vector<Album>>();
    list = ListView::make(0, platform::uiProfile().tileWidth + 54.0f, [this](int r) { return row(r); });
    list->fill()->padding(Insets(0.0f, 10.0f));
  }

  void set(std::vector<Album> a) {
    *albums_ = std::move(a);
    list->count(rows());
  }
  void append(const std::vector<Album>& a) {
    for (const Album& x : a) albums_->push_back(x);
    list->grow(rows());
  }
  size_t size() const { return albums_->size(); }
  int rows() const { return ((int)albums_->size() + cols_ - 1) / cols_; }

 private:
  int cols_ = 5;
  std::shared_ptr<std::vector<Album>> albums_;

  WidgetPtr row(int r) {
    auto b = Box::make(Direction::Row)->gap(platform::uiProfile().tileGap)->margin(Insets(20.0f, 0.0f));
    for (int c = 0; c < cols_; ++c) {
      size_t idx = (size_t)(r * cols_ + c);
      if (idx >= albums_->size()) break;
      b->add(albumTile((*albums_)[idx]));
    }
    if (r >= rows() - 2 && loadMore) loadMore();
    return b;
  }
};

class MiniPlayer : public ContainerT<MiniPlayer> {
 public:
  const char* typeName() const override { return "mini-player"; }

  static Ptr make() {
    auto m = std::make_shared<MiniPlayer>();
    m->build();
    return m;
  }

  void tick(Context&, float) override {
    PlayerCore& p = *svc().player;
    const Song* s = p.current();
    visible_ = s != nullptr && !hidden_;
    if (auto h = hints_.lock()) h->visible_ = !visible_;
    if (!s) return;
    if (p.revision() != rev_) {
      rev_ = p.revision();
      title_->text(s->title);
      artist_->text(songSubtitle(*s));
      cover_->coverId(s->coverArt);
    }
    bar_->value(p.progress())->waving(p.playing());
    glyph_->codepoint(p.playing() ? ic::pause : ic::play_arrow);
    glyph_->visible_ = !p.buffering();
    spin_->visible_ = p.buffering();
  }

  Ptr hide(bool h) {
    hidden_ = h;
    return self();
  }

  void yieldTo(const std::shared_ptr<HintBar>& h) { hints_ = h; }

 private:
  uint32_t rev_ = 0;
  bool hidden_ = false;
  std::weak_ptr<HintBar> hints_;
  std::shared_ptr<Label> title_, artist_;
  std::shared_ptr<Cover> cover_;
  std::shared_ptr<ProgressBar> bar_;
  std::shared_ptr<Glyph> glyph_;
  std::shared_ptr<CircularProgress> spin_;

  void build() {
    inline_.direction = Direction::Row;
    inline_.align = Align::Center;
    inline_.gap = 10.0f;
    cover_ = Cover::make("", 44.0f, 300)->radius(10.0f);
    title_ = Label::make("")->cls("row-title");
    artist_ = Label::make("")->cls("caption");
    auto texts = Box::make(Direction::Column)->gap(1)->fillWidth();
    texts->add(title_)->add(artist_);
    auto left = Button::make()->cls("mini")->fillWidth();
    left->add(cover_)->add(texts);
    left->onClick([]() { if (nav().openNowPlaying) nav().openNowPlaying(); });
    bar_ = ProgressBar::make(0.0f)->wavy()->thickness(5.0f)->amplitude(2.4f)->wavelength(22.0f)->width(Dim::px(190.0f));

    auto prev = iconBtn(ic::skip_previous, []() { svc().player->previous(); });
    glyph_ = Glyph::make(ic::play_arrow);
    spin_ = CircularProgress::make()->indeterminate()->thickness(3.0f)->size(26, 26);
    spin_->visible_ = false;
    auto play = Button::make()->cls("icon-filled")->direction(Direction::Stack)->onClick([]() { svc().player->toggle(); });
    play->inline_.padding = Insets(10.0f);
    play->inline_.minWidth = 44.0f;
    play->inline_.minHeight = 44.0f;
    play->add(glyph_)->add(spin_);
    auto next = iconBtn(ic::skip_next, []() { svc().player->next(); });
    for (auto& b : {prev, next}) {
      b->inline_.padding = Insets(8.0f);
      b->inline_.minWidth = 40.0f;
      b->inline_.minHeight = 40.0f;
    }
    addChild(left);
    addChild(bar_);
    addChild(prev);
    addChild(play);
    addChild(next);
  }
};

inline void decorate(const ViewPtr& v, bool mini = true) {
  v->bottom()->clear();
  if (mini) {
    auto m = MiniPlayer::make();
    if (platform::uiProfile().hideHintsWithPlayer) m->yieldTo(v->hints_);
    v->bottom()->add(m->fillWidth());
  } else {
    v->bottom()->add(Spacer::make());
  }
  v->hints_->inline_.width = Dim::autoSize();
  v->hints_->markStyleDirty();
  v->bottom()->add(v->hints_);
}

class Page : public std::enable_shared_from_this<Page> {
 public:
  explicit Page(const std::string& title, bool mini = true) : pending_(View::make(title)) {
    decorate(pending_, mini);
    pending_->body()->add(panel.box);
  }
  virtual ~Page() = default;

  virtual void start() {}
  virtual void refresh() {}
  virtual void update(float) {}

  ViewPtr view() const { return weak_.lock(); }
  Panel panel;

  template <class P>
  struct Made {
    ViewPtr view;
    std::shared_ptr<P> page;
  };

  template <class P, class... A>
  static Made<P> create(A&&... a) {
    auto p = std::make_shared<P>(std::forward<A>(a)...);
    ViewPtr v = p->pending_;
    p->pending_.reset();
    p->weak_ = v;
    v->keep(p);
    p->top_ = v->top();
    p->body_ = v->body();
    p->start();
    return {v, p};
  }

  ViewPtr viewPtr() const { return weak_.lock(); }

 protected:
  ViewPtr pending_;
  std::weak_ptr<View> weak_;
  std::shared_ptr<Box> top_, body_;

  std::shared_ptr<Box> topBar() { return pending_ ? pending_->top() : top_; }
  std::shared_ptr<Box> bodyBox() { return pending_ ? pending_->body() : body_; }
  ViewPtr self_view() { return pending_ ? pending_ : weak_.lock(); }
};

}
