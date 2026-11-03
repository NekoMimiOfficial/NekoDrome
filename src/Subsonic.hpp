#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Md5.hpp"
#include "Models.hpp"
#include "Net.hpp"

namespace nt {

struct Account {
  std::string url, user, salt, token;
  bool valid() const { return !url.empty() && !user.empty() && !token.empty(); }
};

using Params = std::vector<std::pair<std::string, std::string>>;

class Subsonic {
 public:
  using Reply = std::function<void(bool ok, const Json& body, const std::string& error)>;

  explicit Subsonic(Net& net) : net_(net) {}

  void setAccount(Account a) {
    while (!a.url.empty() && a.url.back() == '/') a.url.pop_back();
    acct_ = std::move(a);
  }
  const Account& account() const { return acct_; }

  static Account makeAccount(std::string url, std::string user, const std::string& password) {
    Account a;
    a.url = std::move(url);
    a.user = std::move(user);
    a.salt = randomHex(10);
    a.token = md5Hex(password + a.salt);
    return a;
  }

  std::string url(const std::string& endpoint, const Params& extra = {}) const {
    std::string u = acct_.url + "/rest/" + endpoint + "?u=" + urlEncode(acct_.user) + "&t=" + acct_.token +
                    "&s=" + acct_.salt + "&v=1.16.1&c=NekoDrome&f=json";
    for (const auto& p : extra) u += "&" + p.first + "=" + urlEncode(p.second);
    return u;
  }

  std::string streamUrl(const std::string& id, int maxBitRate) const {
    Params p{{"id", id}};
    if (maxBitRate > 0) {
      p.push_back({"maxBitRate", std::to_string(maxBitRate)});
      p.push_back({"format", "mp3"});
    }
    return url("stream", p);
  }

  std::string coverUrl(const std::string& id, int size) const {
    return url("getCoverArt", {{"id", id}, {"size", std::to_string(size)}});
  }

  void request(const std::string& endpoint, const Params& p, Reply cb) {
    net_.get(url(endpoint, p), [cb](HttpResult r) {
      if (!r.ok && r.body.empty()) {
        cb(false, Json(), r.error);
        return;
      }
      bool parsed = false;
      Json root = Json::parse(r.body, &parsed);
      if (!parsed) {
        cb(false, Json(), r.ok ? "Invalid server response" : r.error);
        return;
      }
      const Json& resp = root["subsonic-response"];
      if (resp["status"].str() != "ok") {
        std::string msg = resp["error"]["message"].str("Request failed");
        cb(false, resp, msg);
        return;
      }
      cb(true, resp, "");
    });
  }

  void ping(std::function<void(bool, const std::string&)> cb) {
    request("ping", {}, [cb](bool ok, const Json&, const std::string& err) { cb(ok, err); });
  }

  void albumList(const std::string& type, int size, int offset,
                 std::function<void(bool, std::vector<Album>, const std::string&)> cb) {
    request("getAlbumList2", {{"type", type}, {"size", std::to_string(size)}, {"offset", std::to_string(offset)}},
            [cb](bool ok, const Json& r, const std::string& err) {
              cb(ok, ok ? parseList<Album>(r["albumList2"]["album"]) : std::vector<Album>{}, err);
            });
  }

  void album(const std::string& id, std::function<void(bool, Album, std::vector<Song>, const std::string&)> cb) {
    request("getAlbum", {{"id", id}}, [cb](bool ok, const Json& r, const std::string& err) {
      if (!ok) {
        cb(false, Album{}, {}, err);
        return;
      }
      cb(true, Album::fromJson(r["album"]), parseList<Song>(r["album"]["song"]), "");
    });
  }

  void artists(std::function<void(bool, std::vector<Artist>, const std::string&)> cb) {
    request("getArtists", {}, [cb](bool ok, const Json& r, const std::string& err) {
      std::vector<Artist> out;
      if (ok)
        for (const Json& idx : r["artists"]["index"].items())
          for (const Json& a : idx["artist"].items()) out.push_back(Artist::fromJson(a));
      cb(ok, std::move(out), err);
    });
  }

  void artist(const std::string& id, std::function<void(bool, Artist, std::vector<Album>, const std::string&)> cb) {
    request("getArtist", {{"id", id}}, [cb](bool ok, const Json& r, const std::string& err) {
      if (!ok) {
        cb(false, Artist{}, {}, err);
        return;
      }
      cb(true, Artist::fromJson(r["artist"]), parseList<Album>(r["artist"]["album"]), "");
    });
  }

  struct SearchResult {
    std::vector<Artist> artists;
    std::vector<Album> albums;
    std::vector<Song> songs;
  };

  void search(const std::string& q, std::function<void(bool, SearchResult, const std::string&)> cb) {
    request("search3", {{"query", q}, {"artistCount", "8"}, {"albumCount", "12"}, {"songCount", "30"}},
            [cb](bool ok, const Json& r, const std::string& err) {
              SearchResult s;
              if (ok) {
                s.artists = parseList<Artist>(r["searchResult3"]["artist"]);
                s.albums = parseList<Album>(r["searchResult3"]["album"]);
                s.songs = parseList<Song>(r["searchResult3"]["song"]);
              }
              cb(ok, std::move(s), err);
            });
  }

  void playlists(std::function<void(bool, std::vector<Playlist>, const std::string&)> cb) {
    request("getPlaylists", {}, [cb](bool ok, const Json& r, const std::string& err) {
      cb(ok, ok ? parseList<Playlist>(r["playlists"]["playlist"]) : std::vector<Playlist>{}, err);
    });
  }

  void playlist(const std::string& id, std::function<void(bool, Playlist, std::vector<Song>, const std::string&)> cb) {
    request("getPlaylist", {{"id", id}}, [cb](bool ok, const Json& r, const std::string& err) {
      if (!ok) {
        cb(false, Playlist{}, {}, err);
        return;
      }
      cb(true, Playlist::fromJson(r["playlist"]), parseList<Song>(r["playlist"]["entry"]), "");
    });
  }

  void starred(std::function<void(bool, SearchResult, const std::string&)> cb) {
    request("getStarred2", {}, [cb](bool ok, const Json& r, const std::string& err) {
      SearchResult s;
      if (ok) {
        s.artists = parseList<Artist>(r["starred2"]["artist"]);
        s.albums = parseList<Album>(r["starred2"]["album"]);
        s.songs = parseList<Song>(r["starred2"]["song"]);
      }
      cb(ok, std::move(s), err);
    });
  }

  void randomSongs(int size, std::function<void(bool, std::vector<Song>, const std::string&)> cb) {
    request("getRandomSongs", {{"size", std::to_string(size)}}, [cb](bool ok, const Json& r, const std::string& err) {
      cb(ok, ok ? parseList<Song>(r["randomSongs"]["song"]) : std::vector<Song>{}, err);
    });
  }

  void setStarred(const std::string& id, bool star, std::function<void(bool)> cb = nullptr) {
    request(star ? "star" : "unstar", {{"id", id}}, [cb](bool ok, const Json&, const std::string&) {
      if (cb) cb(ok);
    });
  }

  void scrobble(const std::string& id, long long timeMs, bool submission, std::function<void(bool)> cb = nullptr) {
    Params p{{"id", id}, {"submission", submission ? "true" : "false"}};
    if (timeMs > 0) p.push_back({"time", std::to_string(timeMs)});
    request("scrobble", p, [cb](bool ok, const Json&, const std::string&) {
      if (cb) cb(ok);
    });
  }

 private:
  Net& net_;
  Account acct_;
};

}
