#pragma once

#include "Assets.hpp"
#include "Common.hpp"

namespace neko {

class MusicPlayer {
 public:
  enum class State { Stopped, Buffering, Playing, Paused };

  std::function<void()> onFinished;
  std::function<void(const std::string&)> onTrackChanged;
  std::function<void(State)> onStateChanged;

  bool init(AssetLoader* loader, int frequency = 44100, int chunk = 2048) {
    loader_ = loader;
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, chunk) != 0) return false;
    Mix_VolumeMusic((int)(volume_ * MIX_MAX_VOLUME));
    open_ = true;
    instance() = this;
    Mix_HookMusicFinished(&MusicPlayer::finishedHook);
    return true;
  }

  void shutdown() {
    if (!open_) return;
    Mix_HookMusicFinished(nullptr);
    Mix_HaltMusic();
    if (music_) Mix_FreeMusic(music_);
    music_ = nullptr;
    Mix_CloseAudio();
    open_ = false;
    instance() = nullptr;
  }

  void setPlaylist(std::vector<std::string> tracks) {
    playlist_ = std::move(tracks);
    index_ = -1;
  }
  const std::vector<std::string>& playlist() const { return playlist_; }
  int index() const { return index_; }

  void playIndex(int i) {
    if (playlist_.empty()) return;
    index_ = (i % (int)playlist_.size() + (int)playlist_.size()) % (int)playlist_.size();
    play(playlist_[index_]);
  }
  void next() { playIndex(index_ + 1); }
  void previous() {
    if (position_ > 3.0) seek(0);
    else playIndex(index_ - 1);
  }

  void play(const std::string& path) {
    if (!open_ || !loader_) return;
    Mix_HaltMusic();
    if (music_) {
      Mix_FreeMusic(music_);
      music_ = nullptr;
    }
    path_ = path;
    title_ = baseName(path);
    position_ = 0;
    duration_ = 0;
    setState(State::Buffering);
    uint32_t ticket = ++ticket_;
    auto holder = std::make_shared<Mix_Music*>(nullptr);
    loader_->submit([holder, path]() { *holder = Mix_LoadMUS(path.c_str()); },
                    [this, holder, ticket]() {
                      Mix_Music* m = *holder;
                      if (ticket != ticket_ || !open_) {
                        if (m) Mix_FreeMusic(m);
                        return;
                      }
                      if (!m) {
                        setState(State::Stopped);
                        return;
                      }
                      music_ = m;
                      startLoaded();
                    });
    if (onTrackChanged) onTrackChanged(path_);
  }

  void pause() {
    if (state_ != State::Playing) return;
    Mix_PauseMusic();
    setState(State::Paused);
  }
  void resume() {
    if (state_ != State::Paused) return;
    Mix_ResumeMusic();
    setState(State::Playing);
  }
  void toggle() {
    if (state_ == State::Playing) pause();
    else if (state_ == State::Paused) resume();
    else if (state_ == State::Stopped && !playlist_.empty()) playIndex(index_ < 0 ? 0 : index_);
  }
  void stop() {
    ++ticket_;
    Mix_HaltMusic();
    position_ = 0;
    setState(State::Stopped);
  }

  void seek(double seconds) {
    if (!music_) return;
    seconds = std::max(0.0, duration_ > 0 ? std::min(seconds, duration_) : seconds);
    if (Mix_SetMusicPosition(seconds) == 0) position_ = seconds;
  }
  void seekNormalized(float t) { seek((double)saturate(t) * duration_); }

  void setVolume(float v) {
    volume_ = saturate(v);
    if (open_) Mix_VolumeMusic((int)(volume_ * MIX_MAX_VOLUME));
  }
  float volume() const { return volume_; }

  State state() const { return state_; }
  bool playing() const { return state_ == State::Playing; }
  bool buffering() const { return state_ == State::Buffering; }
  double position() const { return position_; }
  double duration() const { return duration_; }
  float progress() const { return duration_ > 0 ? (float)saturate((float)(position_ / duration_)) : 0.0f; }
  const std::string& title() const { return title_; }
  const std::string& path() const { return path_; }
  void setDurationHint(double d) { if (duration_ <= 0) duration_ = d; }

  void update(float dt) {
    if (!open_) return;
    if (finished_) {
      finished_ = false;
      if (state_ == State::Playing || state_ == State::Paused) {
        position_ = 0;
        setState(State::Stopped);
        if (onFinished) onFinished();
        if (!playlist_.empty() && index_ >= 0) next();
      }
    }
    if (state_ == State::Playing) {
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
      double p = Mix_GetMusicPosition(music_);
      if (p >= 0) position_ = p;
      else position_ += dt;
#else
      position_ += dt;
#endif
    }
  }

 private:
  AssetLoader* loader_ = nullptr;
  Mix_Music* music_ = nullptr;
  bool open_ = false;
  volatile bool finished_ = false;
  State state_ = State::Stopped;
  std::vector<std::string> playlist_;
  int index_ = -1;
  std::string path_, title_;
  double position_ = 0, duration_ = 0;
  float volume_ = 0.8f;
  uint32_t ticket_ = 0;

  static MusicPlayer*& instance() {
    static MusicPlayer* p = nullptr;
    return p;
  }
  static void finishedHook() {
    if (instance()) instance()->finished_ = true;
  }

  static std::string baseName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    std::string n = s == std::string::npos ? p : p.substr(s + 1);
    size_t d = n.find_last_of('.');
    return d == std::string::npos ? n : n.substr(0, d);
  }

  void startLoaded() {
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
    double d = Mix_MusicDuration(music_);
    if (d > 0) duration_ = d;
    const char* t = Mix_GetMusicTitle(music_);
    if (t && *t) title_ = t;
#endif
    if (Mix_PlayMusic(music_, 1) != 0) {
      setState(State::Stopped);
      return;
    }
    finished_ = false;
    setState(State::Playing);
  }

  void setState(State s) {
    if (s == state_) return;
    state_ = s;
    if (onStateChanged) onStateChanged(s);
  }
};

}
