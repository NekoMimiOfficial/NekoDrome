#pragma once

#include "Components.hpp"

namespace nt {

inline void bindQueueKeys(const ViewPtr& v) {
  v->bind(Action::Extra, "Add to queue", []() {
    if (focusedSong().id.empty()) return;
    svc().player->enqueue(focusedSong());
    notify("Added to queue", focusedSong().title, ic::queue_music);
  });
  v->bind(Action::Option, "Play next", []() {
    if (focusedSong().id.empty()) return;
    svc().player->playNext(focusedSong());
    notify("Playing next", focusedSong().title, ic::queue_music);
  });
}

inline std::shared_ptr<Button> playButton(const std::string& text, uint32_t glyph, std::function<void()> fn, const std::string& variant = "") {
  auto b = Button::make();
  if (!variant.empty()) b->cls(variant);
  b->add(Glyph::make(glyph)->fontSize(22.0f));
  b->add(Label::make(text));
  b->onClick(std::move(fn));
  return b;
}

inline long long totalDuration(const std::vector<Song>& s) {
  long long d = 0;
  for (const Song& x : s) d += x.duration;
  return d;
}

inline std::string durationText(long long sec) {
  long long m = sec / 60;
  if (m >= 60) return std::to_string(m / 60) + " h " + std::to_string(m % 60) + " min";
  return std::to_string(m) + " min";
}

class HomePage : public Page {
 public:
  HomePage() : Page("Home") {
    topBar()->add(iconBtn(ic::search, []() { if (nav().openSearch) nav().openSearch(); }));
    topBar()->add(iconBtn(ic::refresh, [this]() { refresh(); }));
    content_ = Box::make(Direction::Column)->gap(18)->padding(Insets(20.0f, 16.0f));
    auto sv = ScrollView::make();
    sv->add(content_);
    sv->fill();
    panel.show(sv);
  }

  void start() override {
    refresh();
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) {
        auto* h = static_cast<HomePage*>(p.get());
        if (SDL_GetTicks() - h->loadedAt_ > 60000 || h->wasOnline_ != svc().online) h->refresh();
      }
    });
  }

  void refresh() override {
    loadedAt_ = SDL_GetTicks();
    wasOnline_ = svc().online;
    content_->clear();
    Services& s = svc();
    auto actions = Box::make(Direction::Row)->gap(10);
    if (s.online) {
      actions->add(playButton("Shuffle all", ic::shuffle, []() {
        svc().api.randomSongs(100, [](bool ok, std::vector<Song> songs, const std::string& err) {
          if (!ok || songs.empty()) {
            notify("Couldn't load songs", err, ic::error);
            return;
          }
          svc().player->setShuffle(false);
          svc().player->setQueue(std::move(songs), 0, true);
        });
      }));
    }
    actions->add(playButton("Offline library", ic::offline_pin, []() { if (nav().openOffline) nav().openOffline(); }, "tonal"));
    content_->add(actions);

    if (!s.online) {
      auto banner = Box::make(Direction::Row)->cls("banner");
      banner->add(Glyph::make(ic::cloud_off));
      banner->add(Label::make("You're offline. Your downloaded songs are still available.")->fillWidth());
      content_->add(banner);
      return;
    }

    struct Sec {
      const char* title;
      const char* type;
    };
    static const Sec secs[] = {{"Recently played", "recent"}, {"Newest", "newest"}, {"Most played", "frequent"}, {"Discover", "random"}};
    for (const Sec& sec : secs) {
      auto box = Box::make(Direction::Column)->gap(10);
      box->add(Label::make(sec.title)->cls("section-title"));
      auto holder = Box::make(Direction::Column);
      holder->add(Label::make("Loading")->cls("caption"));
      box->add(holder);
      box->visible_ = true;
      content_->add(box);
      std::weak_ptr<Page> w = weak_from_this();
      s.api.albumList(sec.type, 16, 0, [w, box, holder](bool ok, std::vector<Album> albums, const std::string& err) {
        if (!w.lock()) return;
        holder->clear();
        if (!ok) {
          holder->add(Label::make("Couldn't load: " + err)->fg("error"));
          return;
        }
        if (albums.empty()) {
          box->visible_ = false;
          return;
        }
        holder->add(tileRow(albums));
      });
    }
  }

 private:
  std::shared_ptr<Box> content_;
  uint32_t loadedAt_ = 0;
  bool wasOnline_ = false;
};

class AlbumsPage : public Page {
 public:
  AlbumsPage() : Page("Albums") {
    struct Sort {
      const char* label;
      const char* type;
    };
    static const Sort sorts[] = {{"Newest", "newest"}, {"A-Z", "alphabeticalByName"}, {"Recent", "recent"}, {"Frequent", "frequent"}, {"Random", "random"}};
    for (const Sort& s : sorts) {
      auto chip = Button::make()->cls("chip")->group("sort");
      chip->add(Label::make(s.label));
      std::string type = s.type;
      chip->onToggle([this, type](bool on) {
        if (!on) return;
        type_ = type;
        load(true);
      });
      if (type == type_) chip->checked_ = true;
      topBar()->add(chip);
    }
    status_ = centered({ui::spinner(), ui::caption("Loading")});
    auto stack = Box::make(Direction::Stack)->fill();
    stack->add(grid_.list)->add(status_);
    panel.show(stack);
    grid_.loadMore = [this]() {
      if (!loading_ && !exhausted_) load(false);
    };
  }

  void start() override {
    load(true);
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) {
        auto* a = static_cast<AlbumsPage*>(p.get());
        if (a->wasOnline_ != svc().online) a->load(true);
      }
    });
  }

  void refresh() override { load(true); }

 private:
  AlbumGrid grid_;
  std::shared_ptr<Box> status_;
  std::string type_ = "newest";
  int offset_ = 0;
  bool loading_ = false, exhausted_ = false, wasOnline_ = false;
  uint32_t gen_ = 0;

  void load(bool reset) {
    wasOnline_ = svc().online;
    if (reset) {
      ++gen_;
      offset_ = 0;
      exhausted_ = false;
      grid_.set({});
      status_->clear();
      status_->visible_ = true;
      status_->add(ui::spinner());
      status_->add(ui::caption("Loading"));
    }
    if (!svc().online) {
      status_->clear();
      status_->add(ui::glyph(ic::cloud_off)->fontSize(56)->fg(ColorRef("onSurfaceVariant").a(0.5f)));
      status_->add(ui::label("You're offline")->fg("onSurfaceVariant"));
      return;
    }
    loading_ = true;
    std::weak_ptr<Page> w = weak_from_this();
    uint32_t gen = gen_;
    svc().api.albumList(type_, 60, offset_, [w, gen](bool ok, std::vector<Album> albums, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<AlbumsPage*>(p.get());
      if (gen != self->gen_) return;
      self->loading_ = false;
      if (!ok) {
        self->status_->clear();
        self->status_->visible_ = true;
        self->status_->add(ui::glyph(ic::error)->fontSize(48)->fg("error"));
        self->status_->add(ui::label("Couldn't load albums: " + err)->fg("onSurfaceVariant"));
        return;
      }
      self->offset_ += (int)albums.size();
      self->exhausted_ = albums.size() < 60;
      self->grid_.append(albums);
      self->status_->visible_ = self->grid_.size() == 0;
      if (self->grid_.size() == 0) {
        self->status_->clear();
        self->status_->add(ui::glyph(ic::album)->fontSize(56)->fg(ColorRef("onSurfaceVariant").a(0.5f)));
        self->status_->add(ui::label("No albums yet")->fg("onSurfaceVariant"));
      }
    });
  }
};

class ArtistsPage : public Page {
 public:
  ArtistsPage() : Page("Artists") {
    artists_ = std::make_shared<std::vector<Artist>>();
    list_ = ListView::make(0, 64.0f, [this](int i) {
      const Artist& a = (*artists_)[(size_t)i];
      auto b = Button::make()->cls("list")->fillWidth()->margin(Insets(20.0f, 0.0f));
      b->add(Glyph::make(ic::person));
      b->add(Label::make(a.name)->cls("row-title")->fillWidth());
      b->add(Label::make(std::to_string(a.albumCount) + (a.albumCount == 1 ? " album" : " albums"))->cls("caption"));
      std::string id = a.id;
      b->onClick([id]() { if (nav().openArtist) nav().openArtist(id); });
      return b;
    });
    list_->fill()->gap(2);
    topBar()->add(iconBtn(ic::refresh, [this]() { refresh(); }));
  }

  void start() override {
    refresh();
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) {
        auto* a = static_cast<ArtistsPage*>(p.get());
        if (a->wasOnline_ != svc().online) a->refresh();
      }
    });
  }

  void refresh() override {
    wasOnline_ = svc().online;
    if (!svc().online) {
      panel.empty(ic::cloud_off, "You're offline");
      return;
    }
    panel.loading();
    std::weak_ptr<Page> w = weak_from_this();
    svc().api.artists([w](bool ok, std::vector<Artist> artists, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<ArtistsPage*>(p.get());
      if (!ok) {
        self->panel.error("Couldn't load artists: " + err, [self]() { self->refresh(); });
        return;
      }
      *self->artists_ = std::move(artists);
      self->list_->count((int)self->artists_->size());
      if (self->artists_->empty()) self->panel.empty(ic::person, "No artists");
      else self->panel.show(self->list_);
    });
  }

 private:
  std::shared_ptr<std::vector<Artist>> artists_;
  std::shared_ptr<ListView> list_;
  bool wasOnline_ = false;
};

class ArtistPage : public Page {
 public:
  explicit ArtistPage(std::string id) : Page("Artist"), id_(std::move(id)) {}

  void start() override { refresh(); }

  void refresh() override {
    panel.loading();
    std::weak_ptr<Page> w = weak_from_this();
    svc().api.artist(id_, [w](bool ok, Artist a, std::vector<Album> albums, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<ArtistPage*>(p.get());
      if (!ok) {
        self->panel.error("Couldn't load artist: " + err, [self]() { self->refresh(); });
        return;
      }
      if (auto v = self->view()) v->title(a.name);
      self->grid_.set(std::move(albums));
      self->panel.show(self->grid_.list);
    });
  }

 private:
  std::string id_;
  AlbumGrid grid_;
};

class CollectionPage : public Page {
 public:
  enum class Kind { Album, Playlist };

  CollectionPage(Kind k, std::string id) : Page(k == Kind::Album ? "Album" : "Playlist"), kind_(k), id_(std::move(id)) {
    bindQueueKeys(pending_);
  }

  void start() override { refresh(); }

  void refresh() override {
    panel.loading();
    std::weak_ptr<Page> w = weak_from_this();
    auto fail = [w](const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<CollectionPage*>(p.get());
      if (self->kind_ == Kind::Album) {
        std::vector<Song> local;
        for (const auto& e : svc().store.offline())
          if (e.song.albumId == self->id_) local.push_back(e.song);
        if (!local.empty()) {
          std::sort(local.begin(), local.end(), [](const Song& a, const Song& b) { return a.disc != b.disc ? a.disc < b.disc : a.track < b.track; });
          self->build(local[0].album, local[0].artist, local[0].artistId, local[0].coverArt, local[0].year, std::move(local));
          return;
        }
      }
      self->panel.error("Couldn't load: " + err, [self]() { self->refresh(); });
    };
    if (kind_ == Kind::Album) {
      svc().api.album(id_, [w, fail](bool ok, Album a, std::vector<Song> songs, const std::string& err) {
        auto p = w.lock();
        if (!p) return;
        if (!ok) {
          fail(err);
          return;
        }
        static_cast<CollectionPage*>(p.get())->build(a.name, a.artist, a.artistId, a.coverArt, a.year, std::move(songs));
      });
    } else {
      svc().api.playlist(id_, [w, fail](bool ok, Playlist pl, std::vector<Song> songs, const std::string& err) {
        auto p = w.lock();
        if (!p) return;
        if (!ok) {
          fail(err);
          return;
        }
        static_cast<CollectionPage*>(p.get())->build(pl.name, "", "", pl.coverArt, 0, std::move(songs));
      });
    }
  }

 private:
  Kind kind_;
  std::string id_;

  void build(const std::string& name, const std::string& artist, const std::string& artistId, const std::string& cover, int year,
             std::vector<Song> songsIn) {
    auto songs = std::make_shared<std::vector<Song>>(std::move(songsIn));
    if (auto v = view()) v->title(name);
    bool isAlbum = kind_ == Kind::Album;

    auto header = Box::make(Direction::Row)->gap(24)->padding(Insets(24.0f, 20.0f))->align(Align::Center);
    header->add(Cover::make(cover, 190.0f, 600)->radius(24.0f));
    auto info = Box::make(Direction::Column)->gap(8)->fillWidth();
    info->add(Label::make(name)->cls("title"));
    if (!artist.empty()) {
      std::string aid = artistId;
      auto ab = Button::make()->cls("text")->onClick([aid]() { if (nav().openArtist && !aid.empty()) nav().openArtist(aid); });
      ab->inline_.padding = Insets(0.0f, 4.0f);
      ab->inline_.justify = Justify::Start;
      ab->add(Label::make(artist)->cls("subtitle")->fg("primary"));
      info->add(ab);
    }
    std::string meta;
    if (year > 0) meta += std::to_string(year) + " \xC2\xB7 ";
    meta += std::to_string(songs->size()) + (songs->size() == 1 ? " song" : " songs") + " \xC2\xB7 " + durationText(totalDuration(*songs));
    info->add(Label::make(meta)->cls("caption"));

    auto actions = Box::make(Direction::Row)->gap(10)->margin(Insets(0.0f, 8.0f, 0.0f, 0.0f));
    actions->add(playButton("Play", ic::play_arrow, [songs]() {
      svc().player->setShuffle(false);
      svc().player->setQueue(*songs, 0, true);
    }));
    actions->add(playButton("Shuffle", ic::shuffle, [songs]() {
      svc().player->setShuffle(true);
      svc().player->setQueue(*songs, 0, true);
    }, "tonal"));
    std::vector<std::string> ids;
    for (const Song& s : *songs) ids.push_back(s.id);
    auto dl = Button::make()->cls("tonal");
    dl->add(DownloadBadge::make(ids, 24.0f));
    dl->add(Label::make("Download"));
    dl->onClick(downloadAction(*songs));
    actions->add(dl);
    info->add(actions);
    header->add(info);

    auto list = ListView::make((int)songs->size(), 64.0f, [songs, isAlbum](int i) {
      auto r = songRow(songs, i, !isAlbum, isAlbum);
      r->inline_.margin = Insets(14.0f, 0.0f);
      return r;
    });
    list->fill()->gap(2);

    auto col = Box::make(Direction::Column)->fill();
    col->add(header)->add(list);
    panel.show(col);
  }
};

class PlaylistsPage : public Page {
 public:
  PlaylistsPage() : Page("Playlists") {
    items_ = std::make_shared<std::vector<Playlist>>();
    list_ = ListView::make(0, 72.0f, [this](int i) {
      const Playlist& p = (*items_)[(size_t)i];
      auto b = Button::make()->cls("list")->fillWidth()->margin(Insets(20.0f, 0.0f));
      b->add(Cover::make(p.coverArt, 48.0f, 300)->radius(10.0f));
      auto t = Box::make(Direction::Column)->gap(2)->fillWidth();
      t->add(Label::make(p.name)->cls("row-title"));
      t->add(Label::make(std::to_string(p.songCount) + " songs \xC2\xB7 " + durationText(p.duration))->cls("caption"));
      b->add(t);
      std::string id = p.id;
      b->onClick([id]() { if (nav().openPlaylist) nav().openPlaylist(id); });
      return b;
    });
    list_->fill()->gap(2);
    topBar()->add(iconBtn(ic::refresh, [this]() { refresh(); }));
  }

  void start() override {
    refresh();
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) {
        auto* a = static_cast<PlaylistsPage*>(p.get());
        if (a->wasOnline_ != svc().online) a->refresh();
      }
    });
  }

  void refresh() override {
    wasOnline_ = svc().online;
    if (!svc().online) {
      panel.empty(ic::cloud_off, "You're offline");
      return;
    }
    panel.loading();
    std::weak_ptr<Page> w = weak_from_this();
    svc().api.playlists([w](bool ok, std::vector<Playlist> pls, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<PlaylistsPage*>(p.get());
      if (!ok) {
        self->panel.error("Couldn't load playlists: " + err, [self]() { self->refresh(); });
        return;
      }
      *self->items_ = std::move(pls);
      self->list_->count((int)self->items_->size());
      if (self->items_->empty()) self->panel.empty(ic::queue_music, "No playlists yet");
      else self->panel.show(self->list_);
    });
  }

 private:
  std::shared_ptr<std::vector<Playlist>> items_;
  std::shared_ptr<ListView> list_;
  bool wasOnline_ = false;
};

class SearchPage : public Page {
 public:
  SearchPage() : Page("Search") {
    bindQueueKeys(pending_);
    field_ = TextField::make("Search songs, albums and artists")->cls("textfield")->onChange([this](const std::string&) {
      dirty_ = true;
      lastEdit_ = SDL_GetTicks();
    })->onSubmit([this](const std::string&) {
      dirty_ = false;
      run();
    });
    field_->inline_.margin = Insets(20.0f, 16.0f, 20.0f, 4.0f);
    results_ = Box::make(Direction::Column)->fill();
    auto col = Box::make(Direction::Column)->fill();
    col->add(field_)->add(results_);
    panel.show(col);
    showHint();
  }

  void start() override {
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) {
        auto* s = static_cast<SearchPage*>(p.get());
        Services& sv = svc();
        sv.ui->focus().set(s->field_.get());
        if (!sv.ui->input().hasPad()) s->field_->begin(sv.ui->context());
      }
    });
  }

  void update(float) override {
    if (dirty_ && SDL_GetTicks() - lastEdit_ > 450) {
      dirty_ = false;
      run();
    }
  }

 private:
  std::shared_ptr<TextField> field_;
  std::shared_ptr<Box> results_;
  bool dirty_ = false;
  uint32_t lastEdit_ = 0, gen_ = 0;

  void showHint() {
    results_->clear();
    results_->add(centered({ui::glyph(ic::search)->fontSize(56)->fg(ColorRef("onSurfaceVariant").a(0.5f)),
                            ui::label(svc().online ? "Type to search your library" : "Search needs a server connection")->fg("onSurfaceVariant")}));
  }

  void run() {
    std::string q = trim(field_->text());
    ++gen_;
    if (q.empty()) {
      showHint();
      return;
    }
    if (!svc().online) {
      showHint();
      return;
    }
    results_->clear();
    results_->add(centered({ui::spinner()}));
    std::weak_ptr<Page> w = weak_from_this();
    uint32_t gen = gen_;
    svc().api.search(q, [w, gen](bool ok, Subsonic::SearchResult r, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<SearchPage*>(p.get());
      if (gen != self->gen_) return;
      self->results_->clear();
      if (!ok) {
        self->results_->add(centered({ui::glyph(ic::error)->fontSize(48)->fg("error"), ui::label("Search failed: " + err)->fg("onSurfaceVariant")}));
        return;
      }
      if (r.artists.empty() && r.albums.empty() && r.songs.empty()) {
        self->results_->add(centered({ui::glyph(ic::search)->fontSize(56)->fg(ColorRef("onSurfaceVariant").a(0.5f)), ui::label("No results")->fg("onSurfaceVariant")}));
        return;
      }
      auto col = Box::make(Direction::Column)->gap(14)->padding(Insets(20.0f, 12.0f));
      if (!r.artists.empty()) {
        col->add(Label::make("Artists")->cls("section-title"));
        auto chips = Box::make(Direction::Row)->gap(10);
        for (const Artist& a : r.artists) {
          std::string id = a.id;
          auto c = Button::make()->cls("chip")->onClick([id]() { if (nav().openArtist) nav().openArtist(id); });
          c->add(Glyph::make(ic::person)->fontSize(20.0f));
          c->add(Label::make(a.name));
          chips->add(c);
        }
        auto sv = ScrollView::make(ScrollAxis::Horizontal);
        sv->add(chips);
        sv->fillWidth();
        sv->height(Dim::px(46.0f));
        col->add(sv);
      }
      if (!r.albums.empty()) {
        col->add(Label::make("Albums")->cls("section-title"));
        col->add(tileRow(r.albums));
      }
      if (!r.songs.empty()) {
        col->add(Label::make("Songs")->cls("section-title"));
        auto songs = std::make_shared<std::vector<Song>>(std::move(r.songs));
        auto list = Box::make(Direction::Column)->gap(2);
        for (size_t i = 0; i < songs->size(); ++i) list->add(songRow(songs, (int)i));
        col->add(list);
      }
      auto sv = ScrollView::make();
      sv->add(col);
      sv->fill();
      self->results_->add(sv);
    });
  }
};

class FavoritesPage : public Page {
 public:
  FavoritesPage() : Page("Favorites") {
    bindQueueKeys(pending_);
    topBar()->add(iconBtn(ic::refresh, [this]() { refresh(); }));
  }

  void start() override {
    refresh();
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) static_cast<FavoritesPage*>(p.get())->refresh();
    });
  }

  void refresh() override {
    if (!svc().online) {
      panel.empty(ic::cloud_off, "You're offline");
      return;
    }
    if (!shown_) panel.loading();
    std::weak_ptr<Page> w = weak_from_this();
    svc().api.starred([w](bool ok, Subsonic::SearchResult r, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<FavoritesPage*>(p.get());
      if (!ok) {
        self->panel.error("Couldn't load favorites: " + err, [self]() { self->refresh(); });
        return;
      }
      self->shown_ = true;
      if (r.albums.empty() && r.songs.empty()) {
        self->panel.empty(ic::favorite_border, "Nothing favorited yet");
        return;
      }
      auto col = Box::make(Direction::Column)->gap(14)->padding(Insets(20.0f, 14.0f));
      auto songs = std::make_shared<std::vector<Song>>(std::move(r.songs));
      if (!songs->empty()) {
        auto actions = Box::make(Direction::Row)->gap(10);
        actions->add(playButton("Play favorites", ic::play_arrow, [songs]() {
          svc().player->setShuffle(false);
          svc().player->setQueue(*songs, 0, true);
        }));
        actions->add(playButton("Shuffle", ic::shuffle, [songs]() {
          svc().player->setShuffle(true);
          svc().player->setQueue(*songs, 0, true);
        }, "tonal"));
        col->add(actions);
      }
      if (!r.albums.empty()) {
        col->add(Label::make("Albums")->cls("section-title"));
        col->add(tileRow(r.albums));
      }
      if (!songs->empty()) {
        col->add(Label::make("Songs")->cls("section-title"));
        auto list = Box::make(Direction::Column)->gap(2);
        for (size_t i = 0; i < songs->size(); ++i) list->add(songRow(songs, (int)i));
        col->add(list);
      }
      auto sv = ScrollView::make();
      sv->add(col);
      sv->fill();
      self->panel.show(sv);
    });
  }

 private:
  bool shown_ = false;
};

}
