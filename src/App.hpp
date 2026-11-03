#pragma once

#include <NekoUI/Ui.hpp>

#include <memory>

#include "Downloads.hpp"
#include "Icons.hpp"
#include "PlayerCore.hpp"

namespace nt {

struct Services {
  Net net;
  Subsonic api{net};
  Store store;
  Covers covers{net, api, store};
  Downloads downloads{net, api, store, covers};
  std::unique_ptr<PlayerCore> player;
  neko::Engine* ui = nullptr;
  bool online = false;
};

inline Services& svc() {
  static Services s;
  return s;
}

struct Nav {
  std::function<void(const std::string&)> openAlbum, openArtist, openPlaylist;
  std::function<void()> openNowPlaying, openQueue, openOffline, openHome, openSearch;
  std::function<void()> reconnect, connectivityChanged, loggedIn, logout;
  std::function<void(const std::string&, const std::string&, float, uint32_t)> toast;
};

inline Nav& nav() {
  static Nav n;
  return n;
}

inline void notify(const std::string& title, const std::string& body = "", uint32_t glyph = 0, float seconds = 3.5f) {
  if (nav().toast) nav().toast(title, body, seconds, glyph);
}

inline std::string songSubtitle(const Song& s) {
  std::string t = s.artist;
  if (!s.album.empty()) t += (t.empty() ? "" : " \xC2\xB7 ") + s.album;
  return t;
}

}
