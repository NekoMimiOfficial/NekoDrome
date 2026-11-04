#pragma once

#include "LibraryPages.hpp"
#include "Theme.hpp"

namespace nt {

class SettingsPage : public Page {
 public:
  SettingsPage() : Page("Settings") {
    Services& s = svc();
    Config& cfg = s.store.cfg;
    auto col = Box::make(Direction::Column)->gap(16)->padding(Insets(20.0f, 16.0f));

    auto appearance = Box::make(Direction::Column)->cls("card")->fillWidth();
    appearance->add(Label::make("Appearance")->cls("section-title"));
    appearance->add(row("Dark mode", "Switch between the dark and light theme",
                        ui::toggle(cfg.dark, [](bool on) {
                          svc().ui->setColorMode(on ? ColorMode::Dark : ColorMode::Light);
                          svc().store.cfg.dark = on;
                          svc().store.saveConfig();
                        })));
    auto pal = Box::make(Direction::Row)->gap(8);
    int idx = 0;
    for (const Palette& p : palettes()) {
      auto chip = Button::make()->cls("chip")->group("palette");
      chip->add(Label::make(p.name));
      int i = idx++;
      chip->onToggle([i](bool on) {
        if (!on) return;
        applyPalette(svc().ui->colors(), i);
        svc().store.cfg.palette = i;
        svc().store.saveConfig();
      });
      chip->checked_ = i == cfg.palette;
      pal->add(chip);
    }
    appearance->add(row("Accent color", "Swaps the color palette", pal));
    col->add(appearance);

    auto playback = Box::make(Direction::Column)->cls("card")->fillWidth();
    playback->add(Label::make("Playback")->cls("section-title"));
    auto quality = Box::make(Direction::Row)->gap(8);
    struct Q {
      const char* label;
      int kbps;
    };
    static const Q qs[] = {{"Original", 0}, {"320k", 320}, {"192k", 192}, {"128k", 128}};
    for (const Q& q : qs) {
      auto chip = Button::make()->cls("chip")->group("quality");
      chip->add(Label::make(q.label));
      int k = q.kbps;
      chip->onToggle([k](bool on) {
        if (!on) return;
        svc().store.cfg.streamKbps = k;
        svc().store.saveConfig();
      });
      chip->checked_ = q.kbps == cfg.streamKbps;
      quality->add(chip);
    }
    playback->add(row("Streaming quality", "Transcodes to MP3 on the server when not Original", quality));
    playback->add(row("Scrobbling", "Report plays to your server", ui::toggle(cfg.scrobble, [](bool on) {
                       svc().store.cfg.scrobble = on;
                       svc().store.saveConfig();
                     })));
    auto vol = Slider::make(cfg.volume, 0.0f, 1.0f)->onChange([](float v) { svc().player->setVolume(v); });
    vol->inline_.width = Dim::px(240.0f);
    playback->add(row("Volume", "", vol));
    col->add(playback);

    auto storage = Box::make(Direction::Column)->cls("card")->fillWidth();
    storage->add(Label::make("Storage")->cls("section-title"));
    offlineInfo_ = Label::make("")->cls("caption");
    cacheInfo_ = Label::make("")->cls("caption");
    storage->add(offlineInfo_)->add(cacheInfo_);
    auto actions = Box::make(Direction::Row)->gap(10);
    actions->add(ui::button("Clear stream cache", [this]() {
      svc().store.clearTrackCache();
      notify("Stream cache cleared", "", ic::delete_icon);
      refreshInfo();
    }, "tonal"));
    actions->add(ui::button("Delete downloads", []() {
      svc().ui->shell().dialog("Delete all downloads?", "This removes every offline song from this device.",
                               {{"Cancel", nullptr}, {"Delete", []() {
                                  svc().downloads.cancelAll();
                                  svc().store.clearOffline();
                                  notify("Downloads deleted", "", ic::delete_icon);
                                }}});
    }, "outlined"));
    storage->add(actions);
    col->add(storage);

    auto network = Box::make(Direction::Column)->cls("card")->fillWidth();
    network->add(Label::make("Connection")->cls("section-title"));
    serverInfo_ = Label::make("")->cls("caption");
    scrobbleInfo_ = Label::make("")->cls("caption");
    network->add(serverInfo_)->add(scrobbleInfo_);
    network->add(row("Offline mode", "Only use downloaded music", ui::toggle(cfg.offlineForced, [](bool on) {
                       svc().store.cfg.offlineForced = on;
                       svc().store.saveConfig();
                       if (nav().connectivityChanged) nav().connectivityChanged();
                     })));
    network->add(row("Allow self-signed certificates", "For servers with a private certificate", ui::toggle(cfg.insecure, [](bool on) {
                       svc().store.cfg.insecure = on;
                       svc().net.insecure = on;
                       svc().store.saveConfig();
                     })));
    auto netActions = Box::make(Direction::Row)->gap(10);
    netActions->add(ui::button("Reconnect", []() { if (nav().reconnect) nav().reconnect(); }, "tonal"));
    netActions->add(ui::button("Log out", []() {
      svc().ui->shell().dialog("Log out?", "Your downloads stay on this device.",
                               {{"Cancel", nullptr}, {"Log out", []() { if (nav().logout) nav().logout(); }}});
    }, "outlined"));
    network->add(netActions);
    col->add(network);

    auto about = Box::make(Direction::Column)->cls("card")->fillWidth();
    about->add(Label::make("About")->cls("section-title"));
    about->add(Label::make("NekoDrome \xC2\xB7 a Navidrome client built with NekoUI")->cls("caption"));
    col->add(about);

    auto sv = ScrollView::make();
    sv->add(col);
    sv->fill();
    panel.show(sv);
  }

  void start() override {
    refreshInfo();
    std::weak_ptr<Page> w = weak_from_this();
    view()->onEnter([w]() {
      if (auto p = w.lock()) static_cast<SettingsPage*>(p.get())->refreshInfo();
    });
  }

  void refreshInfo() {
    Services& s = svc();
    offlineInfo_->text("Downloads: " + std::to_string(s.store.offline().size()) + " songs \xC2\xB7 " + humanSize(s.store.offlineBytes()));
    cacheInfo_->text("Stream cache: " + humanSize(s.store.trackCacheBytes()));
    serverInfo_->text(s.store.cfg.account.user + " @ " + s.store.cfg.account.url + "  \xC2\xB7  " +
                      (s.online ? "connected" : (s.store.cfg.offlineForced ? "offline mode" : "unreachable")));
    size_t n = s.player ? s.player->pendingScrobbles() : 0;
    scrobbleInfo_->text(n ? std::to_string(n) + " plays waiting to be scrobbled" : "All plays are scrobbled");
  }

 private:
  std::shared_ptr<Label> offlineInfo_, cacheInfo_, serverInfo_, scrobbleInfo_;

  static std::shared_ptr<Box> row(const std::string& title, const std::string& sub, WidgetPtr control) {
    auto r = Box::make(Direction::Row)->cls("setting-row")->fillWidth();
    auto t = Box::make(Direction::Column)->gap(1)->fillWidth();
    t->add(Label::make(title));
    if (!sub.empty()) t->add(Label::make(sub)->cls("caption"));
    r->add(t)->add(control);
    return r;
  }
};

class LoginPage : public Page {
 public:
  LoginPage() : Page("Connect", false) {
    pending_->hideTop();
    pending_->hideBottom();
    Config& cfg = svc().store.cfg;
    server_ = TextField::make("Server URL, e.g. https://music.example.com")->cls("textfield")->text(cfg.account.url);
    user_ = TextField::make("Username")->cls("textfield")->text(cfg.account.user);
    pass_ = TextField::make("Password", true)->cls("textfield");
    server_->onSubmit([this](const std::string&) { edit(user_); });
    user_->onSubmit([this](const std::string&) { edit(pass_); });
    pass_->onSubmit([this](const std::string&) { connect(); });
    insecure_ = Checkbox::make(cfg.insecure);
    status_ = Label::make("")->fg("onSurfaceVariant")->wrap(true);
    spinner_ = CircularProgress::make()->indeterminate()->thickness(3.0f)->size(28, 28);
    spinner_->visible_ = false;
    connectBtn_ = Button::make();
    connectBtn_->add(Glyph::make(ic::sync)->fontSize(22.0f));
    connectBtn_->add(Label::make("Connect"));
    connectBtn_->onClick([this]() { connect(); });

    auto card = Box::make(Direction::Column)->cls("card")->width(Dim::px(540.0f))->gap(14)->padding(Insets(30.0f));
    card->add(Label::make("NekoDrome")->cls("hero-title")->fg("primary"));
    card->add(Label::make("Connect to your Navidrome server")->cls("caption"));
    card->add(Spacer::make(4.0f));
    card->add(server_)->add(user_)->add(pass_);
    auto insRow = Box::make(Direction::Row)->gap(10)->align(Align::Center);
    insRow->add(insecure_)->add(Label::make("Allow self-signed certificate")->cls("caption"));
    card->add(insRow);
    card->add(status_);
    auto actions = Box::make(Direction::Row)->gap(12)->align(Align::Center);
    actions->add(connectBtn_)->add(spinner_);
    card->add(actions);
    panel.show(centered({card}));
  }

 private:
  std::shared_ptr<TextField> server_, user_, pass_;
  std::shared_ptr<Checkbox> insecure_;
  std::shared_ptr<Label> status_;
  std::shared_ptr<CircularProgress> spinner_;
  std::shared_ptr<Button> connectBtn_;
  bool busy_ = false;

  static void edit(const std::shared_ptr<TextField>& f) {
    svc().ui->focus().set(f.get());
    f->begin(svc().ui->context());
  }

  void fail(const std::string& msg) {
    busy_ = false;
    spinner_->visible_ = false;
    status_->text(msg);
    status_->inline_.color = ColorRef("error");
    status_->markStyleDirty();
  }

  void connect() {
    if (busy_) return;
    std::string url = trim(server_->text()), user = trim(user_->text()), pw = pass_->text();
    if (url.empty() || user.empty() || pw.empty()) {
      fail("Enter the server address, username and password.");
      return;
    }
    if (url.find("://") == std::string::npos) url = "http://" + url;
    busy_ = true;
    spinner_->visible_ = true;
    status_->text("Connecting...");
    status_->inline_.color = ColorRef("onSurfaceVariant");
    status_->markStyleDirty();
    Services& s = svc();
    Account a = Subsonic::makeAccount(url, user, pw);
    bool insecure = insecure_->value();
    s.net.insecure = insecure;
    s.api.setAccount(a);
    std::weak_ptr<Page> w = weak_from_this();
    s.api.ping([w, a, insecure](bool ok, const std::string& err) {
      auto p = w.lock();
      if (!p) return;
      auto* self = static_cast<LoginPage*>(p.get());
      if (!ok) {
        svc().api.setAccount(Account{});
        self->fail("Connection failed: " + err);
        return;
      }
      svc().store.cfg.account = a;
      svc().store.cfg.insecure = insecure;
      svc().store.saveConfig();
      self->pass_->text("");
      self->busy_ = false;
      self->spinner_->visible_ = false;
      if (nav().loggedIn) nav().loggedIn();
    });
  }
};

}
