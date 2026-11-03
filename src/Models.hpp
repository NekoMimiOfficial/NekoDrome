#pragma once

#include <string>
#include <vector>

#include "Json.hpp"

namespace nt {

struct Song {
  std::string id, title, album, albumId, artist, artistId, coverArt, suffix, genre;
  int track = 0, disc = 0, year = 0, duration = 0, bitRate = 0;
  long long size = 0;
  bool starred = false;

  static Song fromJson(const Json& j) {
    Song s;
    s.id = j["id"].str();
    s.title = j["title"].str();
    s.album = j["album"].str();
    s.albumId = j["albumId"].str();
    s.artist = j["artist"].str();
    s.artistId = j["artistId"].str();
    s.coverArt = j["coverArt"].str();
    s.suffix = j["suffix"].str();
    s.genre = j["genre"].str();
    s.track = j["track"].integer();
    s.disc = j["discNumber"].integer();
    s.year = j["year"].integer();
    s.duration = j["duration"].integer();
    s.bitRate = j["bitRate"].integer();
    s.size = j["size"].integer64();
    s.starred = j.has("starred");
    return s;
  }

  Json toJson() const {
    Json j = Json::object();
    j.set("id", id).set("title", title).set("album", album).set("albumId", albumId).set("artist", artist);
    j.set("artistId", artistId).set("coverArt", coverArt).set("suffix", suffix).set("genre", genre);
    j.set("track", track).set("discNumber", disc).set("year", year).set("duration", duration);
    j.set("bitRate", bitRate).set("size", size);
    if (starred) j.set("starred", "1");
    return j;
  }
};

struct Album {
  std::string id, name, artist, artistId, coverArt, genre;
  int songCount = 0, duration = 0, year = 0;
  bool starred = false;

  static Album fromJson(const Json& j) {
    Album a;
    a.id = j["id"].str();
    a.name = j["name"].str(j["title"].str());
    a.artist = j["artist"].str();
    a.artistId = j["artistId"].str();
    a.coverArt = j["coverArt"].str();
    a.genre = j["genre"].str();
    a.songCount = j["songCount"].integer();
    a.duration = j["duration"].integer();
    a.year = j["year"].integer();
    a.starred = j.has("starred");
    return a;
  }
};

struct Artist {
  std::string id, name, coverArt;
  int albumCount = 0;
  bool starred = false;

  static Artist fromJson(const Json& j) {
    Artist a;
    a.id = j["id"].str();
    a.name = j["name"].str();
    a.coverArt = j["coverArt"].str();
    a.albumCount = j["albumCount"].integer();
    a.starred = j.has("starred");
    return a;
  }
};

struct Playlist {
  std::string id, name, owner, coverArt;
  int songCount = 0, duration = 0;

  static Playlist fromJson(const Json& j) {
    Playlist p;
    p.id = j["id"].str();
    p.name = j["name"].str();
    p.owner = j["owner"].str();
    p.coverArt = j["coverArt"].str();
    p.songCount = j["songCount"].integer();
    p.duration = j["duration"].integer();
    return p;
  }
};

template <class T>
std::vector<T> parseList(const Json& arr) {
  std::vector<T> out;
  for (const Json& j : arr.items()) out.push_back(T::fromJson(j));
  return out;
}

}
