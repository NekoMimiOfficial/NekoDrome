#include <cstring>
#include <deque>

#include "SettingsPages.hpp"

using namespace nt;

namespace {

enum Sec { SecHome, SecAlbums, SecArtists, SecPlaylists, SecSearch, SecFavorites, SecOffline, SecNowPlaying, SecSettings, SecCount };

struct NavDef {
  Sec sec;
  uint32_t icon;
  bool reduced;
};

const NavDef kNav[] = {
    {SecHome, ic::home, true},
    {SecAlbums, ic::album, true},
    {SecArtists, ic::person, false},
    {SecPlaylists, ic::queue_music, false},
    {SecSearch, ic::search, true},
    {SecFavorites, ic::favorite, false},
    {SecOffline, ic::offline_pin, true},
    {SecNowPlaying, ic::music_note, true},
    {SecSettings, ic::settings, true},
};

bool fontLoads(const std::string& path) {
  if (!TTF_WasInit() && TTF_Init() != 0) return false;
  TTF_Font* f = TTF_OpenFont(path.c_str(), 16);
  if (!f) return false;
  TTF_CloseFont(f);
  return true;
}

class NekoDrome {
 public:
  int run(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
      if (!std::strcmp(argv[i], "--tour") && i + 1 < argc) tourDir_ = argv[++i];
      else if (!std::strcmp(argv[i], "--smoke") && i + 1 < argc) {
        tourDir_ = argv[++i];
        smoke_ = true;
      } else if (!std::strcmp(argv[i], "--offline-tour") && i + 1 < argc) {
        tourDir_ = argv[++i];
        offlineTour_ = true;
      }
    Services& s = svc();
    s.store.load();
    s.net.insecure = s.store.cfg.insecure;
    s.net.start(platform::uiProfile().apiThreads, platform::uiProfile().transferThreads);

    if (!tourDir_.empty()) {
      SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
      SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    }

    platform::WindowConfig wc = platform::windowConfig();
    neko::Settings es;
    es.title = "NekoDrome";
    es.windowWidth = es.logicalWidth = wc.width;
    es.windowHeight = es.logicalHeight = wc.height;
    es.fullscreen = wc.fullscreen;
    es.resizable = wc.resizable;
    es.fontPath = platform::fontPath();
    if (es.fontPath != platform::defaultFontPath() && !fontLoads(es.fontPath)) es.fontPath = platform::defaultFontPath();
    es.glyphFontPath = platform::glyphFontPath();
    es.colorMode = s.store.cfg.dark ? neko::ColorMode::Dark : neko::ColorMode::Light;
    es.classes = appStyles();
    es.vsync = tourDir_.empty();
    es.imageBudgetBytes = wc.imageBudgetBytes;
    es.imageMaxDim = wc.imageMaxDim;
    if (!ui_.init(es)) {
      std::fprintf(stderr, "NekoUI init failed: %s\n", SDL_GetError());
      return 1;
    }
    s.ui = &ui_;
    applyPalette(ui_.colors(), s.store.cfg.palette);
    if (!ui_.input().hasPad()) {
      auto& g = ui_.input().glyphs;
      g[neko::Action::Accept] = {"Z", neko::FontFace::Text};
      g[neko::Action::Back] = {"X", neko::FontFace::Text};
      g[neko::Action::Extra] = {"A", neko::FontFace::Text};
      g[neko::Action::Option] = {"S", neko::FontFace::Text};
      g[neko::Action::L1] = {"Q", neko::FontFace::Text};
      g[neko::Action::R1] = {"W", neko::FontFace::Text};
      g[neko::Action::Start] = {"Space", neko::FontFace::Text};
      g[neko::Action::Select] = {"V", neko::FontFace::Text};
    }

    s.player = std::make_unique<PlayerCore>(s.net, s.api, s.store, ui_.audio());
    s.player->setVolume(s.store.cfg.volume);
    s.player->restoreState();
    s.player->onError = [](const std::string& msg) { notify("Playback problem", msg, ic::error, 5.0f); };
    s.store.trimTrackCache(800ll * 1024 * 1024);

    s.downloads.onFinished = [this](const Song& song, bool ok, const std::string& err) {
      if (ok) {
        ++finished_;
        if (svc().downloads.activeCount() == 0) {
          notify(finished_ == 1 ? "Download complete" : "Downloads complete", finished_ == 1 ? song.title : std::to_string(finished_) + " songs saved offline", ic::check_circle);
          finished_ = 0;
        }
      } else if (err != "cancelled") {
        notify("Download failed", song.title + ": " + err, ic::error, 5.0f);
      }
    };

    hookNav();
    buildShell();
    ui_.addGlobalKeybind({neko::Action::Start, "Play/Pause", []() { svc().player->toggle(); }, true});
    ui_.addGlobalKeybind({neko::Action::Select, "Up next", [this]() { ui_.shell().toggleDrawer(); }, true});
    ui_.addGlobalKeybind({neko::Action::L1, "Previous", []() { svc().player->previous(); }, false});
    ui_.addGlobalKeybind({neko::Action::R1, "Next", []() { svc().player->next(); }, false});

    ui_.onEvent([this](const SDL_Event& e) { onEvent(e); });
    ui_.onUpdate([this](float dt) { update(dt); });

    if (!tourDir_.empty()) smoke_ ? buildSmoke() : (offlineTour_ ? buildOfflineTour() : buildTour());
    begin();
    ui_.run();

    s.player->saveState();
    s.store.saveConfig();
    s.net.stop();
    sec_ = {};
    loginPage_.reset();
    return 0;
  }

 private:
  struct Entry {
    ViewPtr view;
    std::shared_ptr<Page> page;
    std::shared_ptr<Button> btn;
  };

  neko::Engine ui_;
  std::array<Entry, SecCount> sec_;
  ViewPtr login_, splash_;
  std::shared_ptr<Page> loginPage_;
  bool serverUp_ = false, inApp_ = false;
  float pingTimer_ = 0;
  int finished_ = 0;
  std::string tourDir_;
  bool offlineTour_ = false, smoke_ = false;
  struct Step {
    int wait;
    std::function<void()> fn;
  };
  std::deque<Step> tour_;
  std::vector<Song> tourSongs_;
  std::string tourAlbum_;

  void hookNav() {
    Nav& n = nav();
    n.toast = [this](const std::string& t, const std::string& b, float sec, uint32_t g) { ui_.shell().notify(t, b, sec, g); };
    n.openAlbum = [this](const std::string& id) {
      auto m = Page::create<CollectionPage>(CollectionPage::Kind::Album, id);
      ui_.shell().pushView(m.view);
    };
    n.openPlaylist = [this](const std::string& id) {
      auto m = Page::create<CollectionPage>(CollectionPage::Kind::Playlist, id);
      ui_.shell().pushView(m.view);
    };
    n.openArtist = [this](const std::string& id) {
      auto m = Page::create<ArtistPage>(id);
      ui_.shell().pushView(m.view);
    };
    n.openNowPlaying = [this]() { go(SecNowPlaying); };
    n.openOffline = [this]() { go(SecOffline); };
    n.openHome = [this]() { go(SecHome); };
    n.openSearch = [this]() { go(SecSearch); };
    n.openQueue = [this]() { ui_.shell().toggleDrawer(); };
    n.reconnect = [this]() { ping(true); };
    n.connectivityChanged = [this]() { applyConnectivity(); };
    n.loggedIn = [this]() {
      serverUp_ = true;
      enterApp();
    };
    n.logout = [this]() { logout(); };
  }

  void buildShell() {
    auto& sh = ui_.shell();
    sh.navHeader()->add(ui::image(platform::assetPath("logo.png"))->size(64, 64)->radius(32));
    addNavButtons();
    sh.drawer()->width(neko::Dim::px(430.0f));
    sh.drawer()->add(QueueDrawer::make());
    sh.nav()->visible_ = false;
  }

  template <class P>
  void addSection(Sec s) {
    auto m = Page::create<P>();
    sec_[(size_t)s].view = m.view;
    sec_[(size_t)s].page = m.page;
  }

  void begin() {
    Services& s = svc();
    if (!s.store.cfg.account.valid()) {
      showLogin();
      return;
    }
    s.api.setAccount(s.store.cfg.account);
    splash_ = View::make("NekoDrome");
    splash_->hideTop();
    splash_->hideBottom();
    splash_->body()->add(centered({ui::spinner(), ui::label("Connecting...")->fg("onSurfaceVariant")}));
    ui_.shell().resetView(splash_);
    ui_.shell().nav()->visible_ = false;
    if (s.store.cfg.offlineForced) {
      serverUp_ = false;
      enterApp();
      return;
    }
    s.api.ping([this](bool ok, const std::string&) {
      serverUp_ = ok;
      enterApp();
    });
  }

  void showLogin() {
    auto m = Page::create<LoginPage>();
    login_ = m.view;
    loginPage_ = m.page;
    ui_.shell().nav()->visible_ = false;
    ui_.shell().resetView(login_);
  }

  void enterApp() {
    Services& s = svc();
    if (inApp_) return;
    inApp_ = true;
    applyConnectivity(false);
    addSection<HomePage>(SecHome);
    addSection<AlbumsPage>(SecAlbums);
    addSection<ArtistsPage>(SecArtists);
    addSection<PlaylistsPage>(SecPlaylists);
    addSection<SearchPage>(SecSearch);
    addSection<FavoritesPage>(SecFavorites);
    addSection<OfflinePage>(SecOffline);
    addSection<NowPlayingPage>(SecNowPlaying);
    addSection<SettingsPage>(SecSettings);
    ui_.shell().nav()->visible_ = true;
    go(s.online ? SecHome : SecOffline);
    if (!s.online && s.store.cfg.account.valid() && !s.store.cfg.offlineForced) notify("Offline mode", "Couldn't reach the server", ic::cloud_off, 5.0f);
    login_.reset();
    loginPage_.reset();
    splash_.reset();
    s.player->flushScrobbles();
  }

  void logout() {
    Services& s = svc();
    s.player->clearQueue();
    s.downloads.cancelAll();
    s.store.logout();
    s.api.setAccount(Account{});
    inApp_ = false;
    serverUp_ = false;
    applyConnectivity(false);
    ui_.shell().closeDrawer();
    showLogin();
    sec_ = {};
    rebuildNavButtons();
  }

  void rebuildNavButtons() {
    auto& sh = ui_.shell();
    sh.navBody()->clear();
    addNavButtons();
  }

  void addNavButtons() {
    auto& sh = ui_.shell();
    bool reduced = platform::uiProfile().reducedNav;
    for (const NavDef& d : kNav) {
      if (reduced && !d.reduced) continue;
      Sec sec = d.sec;
      auto b = ui::navItem(d.icon, [this, sec]() { go(sec); }, false);
      sec_[(size_t)sec].btn = b;
      sh.navBody()->add(b);
    }
  }

  void go(Sec s) {
    Entry& e = sec_[(size_t)s];
    if (!e.view) return;
    ui_.shell().closeDrawer();
    if (ui_.shell().view() != e.view) ui_.shell().resetView(e.view);
    if (e.btn) e.btn->select();
  }

  void applyConnectivity(bool announce = true) {
    Services& s = svc();
    bool eff = serverUp_ && !s.store.cfg.offlineForced && s.store.cfg.account.valid();
    bool changed = eff != s.online;
    s.online = eff;
    s.covers.setOffline(!eff);
    if (s.player) s.player->setOffline(!eff);
    if (changed && announce && inApp_) {
      if (eff) {
        notify("Back online", "", ic::cloud_done);
        s.player->flushScrobbles();
      } else {
        notify("Offline mode", "", ic::cloud_off);
      }
    }
  }

  void ping(bool announce) {
    Services& s = svc();
    if (!s.store.cfg.account.valid()) return;
    s.api.ping([this, announce](bool ok, const std::string& err) {
      serverUp_ = ok;
      applyConnectivity();
      if (announce && !ok) notify("Couldn't reach the server", err, ic::cloud_off, 5.0f);
      if (announce && ok && svc().store.cfg.offlineForced) notify("Server reachable", "Turn off offline mode to use it", ic::cloud_done);
    });
  }

  void onEvent(const SDL_Event& e) {
    if (TextField::dispatch(e)) return;
    if (e.type != SDL_KEYDOWN) return;
    Services& s = svc();
    switch (e.key.keysym.sym) {
      case SDLK_AUDIOPLAY:
      case SDLK_AUDIOSTOP: s.player->toggle(); break;
      case SDLK_AUDIONEXT: s.player->next(); break;
      case SDLK_AUDIOPREV: s.player->previous(); break;
      case SDLK_F11: {
        bool fs = (SDL_GetWindowFlags(ui_.window()) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
        SDL_SetWindowFullscreen(ui_.window(), fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        break;
      }
      default: break;
    }
  }

  void update(float dt) {
    Services& s = svc();
    platform::pump();
    TextField::pump();
    s.net.pump();
    s.downloads.update();
    s.covers.tick(dt);
    s.player->update(dt);
    for (auto& e : sec_)
      if (e.page) e.page->update(dt);
    if (loginPage_) loginPage_->update(dt);
    pingTimer_ += dt;
    if (inApp_ && !serverUp_ && !s.store.cfg.offlineForced && pingTimer_ > 30.0f) {
      pingTimer_ = 0;
      ping(false);
    }
    runTour();
  }

  void pushKey(SDL_Keycode k) {
    for (Uint32 type : {(Uint32)SDL_KEYDOWN, (Uint32)SDL_KEYUP}) {
      SDL_Event e{};
      e.type = type;
      e.key.keysym.sym = k;
      e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
      SDL_PushEvent(&e);
    }
  }

  void pushText(const std::string& t) {
    SDL_Event e{};
    e.type = SDL_TEXTINPUT;
    std::snprintf(e.text.text, sizeof e.text.text, "%s", t.c_str());
    SDL_PushEvent(&e);
  }

  void shot(const std::string& name) {
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ui_.renderer().sdl(), nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
    SDL_SaveBMP(surf, (tourDir_ + "/" + name + ".bmp").c_str());
    SDL_FreeSurface(surf);
  }

  void runTour() {
    if (tour_.empty()) return;
    Step& st = tour_.front();
    if (st.wait > 0) {
      --st.wait;
      return;
    }
    auto fn = std::move(st.fn);
    tour_.pop_front();
    if (fn) fn();
  }

  void buildSmoke() {
    tour_.push_back({400, [this]() { shot("s1-home"); }});
    tour_.push_back({5, [this]() { ui_.quit(); }});
  }

  void buildOfflineTour() {
    auto add = [this](int wait, std::function<void()> fn) { tour_.push_back({wait, std::move(fn)}); };
    add(120, [this]() { shot("o1-offline-start"); });
    add(1, [this]() {
      std::vector<Song> songs;
      for (const auto& e : svc().store.offline()) songs.push_back(e.song);
      if (!songs.empty()) svc().player->setQueue(songs, 0, true);
    });
    add(60, [this]() { go(SecNowPlaying); });
    add(60, [this]() { shot("o2-offline-playing"); });
    add(1000, [this]() { shot("o3-offline-later"); go(SecHome); });
    add(60, [this]() { shot("o4-home-offline"); go(SecAlbums); });
    add(60, [this]() { shot("o5-albums-offline"); go(SecSettings); });
    add(60, [this]() { shot("o6-settings-offline"); });
    add(5, [this]() { ui_.quit(); });
  }

  void buildTour() {
    auto add = [this](int wait, std::function<void()> fn) { tour_.push_back({wait, std::move(fn)}); };
    Services& s = svc();
    add(60, [this]() { shot("01-login"); });
    add(1, [this]() { pushKey(SDLK_RETURN); });
    add(3, [this]() { pushText("127.0.0.1:4533"); });
    add(3, [this]() { pushKey(SDLK_RETURN); });
    add(3, [this]() { pushText("neko"); });
    add(3, [this]() { pushKey(SDLK_RETURN); });
    add(3, [this]() { pushText("nekopass"); });
    add(3, [this]() { shot("02-login-filled"); });
    add(3, [this]() { pushKey(SDLK_RETURN); });
    add(140, [this]() { shot("03-home"); });
    add(1, [&s, this]() {
      s.api.albumList("newest", 1, 0, [this](bool ok, std::vector<Album> a, const std::string&) {
        if (ok && !a.empty()) {
          tourAlbum_ = a[0].id;
          svc().api.album(tourAlbum_, [this](bool ok2, Album, std::vector<Song> songs, const std::string&) {
            if (ok2) tourSongs_ = songs;
          });
        }
      });
    });
    add(60, [this]() { go(SecAlbums); });
    add(100, [this]() { shot("04-albums"); go(SecArtists); });
    add(80, [this]() { shot("05-artists"); if (nav().openAlbum) nav().openAlbum(tourAlbum_); });
    add(100, [this]() { shot("06-album"); });
    add(1, [this]() {
      svc().player->setQueue(tourSongs_, 0, true);
      svc().downloads.enqueueAll(tourSongs_);
    });
    add(30, [this]() { go(SecOffline); });
    add(40, [this]() { shot("07-offline-downloading"); });
    add(300, [this]() { shot("07b-offline-downloading"); });
    add(900, [this]() { shot("08-offline-done"); go(SecNowPlaying); });
    add(40, [this]() { shot("09-nowplaying"); ui_.shell().openDrawer(); });
    add(90, [this]() { shot("10-queue"); ui_.shell().closeDrawer(); go(SecSearch); });
    add(30, [this]() { pushText("neon"); });
    add(100, [this]() { shot("11-search"); go(SecFavorites); });
    add(80, [this]() { shot("12-favorites"); go(SecSettings); });
    add(60, [this]() { shot("13-settings"); go(SecPlaylists); });
    add(60, [this]() { shot("14-playlists"); go(SecHome); });
    add(60, [this]() { shot("15-home-playing"); ui_.toggleColorMode(); });
    add(60, [this]() { shot("16-home-light"); });
    add(10, [this]() { ui_.quit(); });
  }
};

}

int main(int argc, char** argv) {
  platform::init(argc, argv);
  int rc;
  {
    NekoDrome app;
    rc = app.run(argc, argv);
  }
  platform::shutdown();
  return rc;
}
