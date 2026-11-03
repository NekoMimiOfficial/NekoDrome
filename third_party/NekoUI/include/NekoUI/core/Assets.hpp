#pragma once

#include "Common.hpp"

namespace neko {

struct ImageAsset {
  enum class State { Pending, Ready, Failed };
  State state = State::Pending;
  SDL_Texture* tex = nullptr;
  int w = 0, h = 0;
  size_t bytes = 0;
  uint32_t lastUsed = 0;
  std::string path;

  ImageAsset() = default;
  ImageAsset(const ImageAsset&) = delete;
  ImageAsset& operator=(const ImageAsset&) = delete;
  ~ImageAsset() {
    if (tex) SDL_DestroyTexture(tex);
  }
  bool ready() const { return state == State::Ready && tex; }
};

class AssetLoader {
 public:
  using Work = std::function<void()>;
  using Done = std::function<void()>;

  bool init(SDL_Renderer* renderer, size_t budgetBytes = 48u * 1024u * 1024u, int maxDim = 1024) {
    renderer_ = renderer;
    budget_ = budgetBytes;
    maxDim_ = maxDim;
    mutex_ = SDL_CreateMutex();
    cond_ = SDL_CreateCond();
    running_ = true;
#if SDL_VERSION_ATLEAST(2, 0, 9)
    thread_ = SDL_CreateThreadWithStackSize(&AssetLoader::threadMain, "neko-assets", 256 * 1024, this);
#else
    thread_ = SDL_CreateThread(&AssetLoader::threadMain, "neko-assets", this);
#endif
    return thread_ != nullptr;
  }

  void shutdown() {
    if (thread_) {
      SDL_LockMutex(mutex_);
      running_ = false;
      SDL_CondSignal(cond_);
      SDL_UnlockMutex(mutex_);
      SDL_WaitThread(thread_, nullptr);
      thread_ = nullptr;
    }
    jobs_.clear();
    finished_.clear();
    images_.clear();
    used_ = 0;
    if (cond_) SDL_DestroyCond(cond_);
    if (mutex_) SDL_DestroyMutex(mutex_);
    cond_ = nullptr;
    mutex_ = nullptr;
  }

  void submit(Work work, Done done = nullptr) {
    SDL_LockMutex(mutex_);
    jobs_.push_back({std::move(work), std::move(done)});
    ++inflight_;
    SDL_CondSignal(cond_);
    SDL_UnlockMutex(mutex_);
  }

  size_t pending() const { return inflight_; }

  std::shared_ptr<ImageAsset> image(const std::string& path) {
    auto it = images_.find(path);
    if (it != images_.end()) {
      it->second->lastUsed = tick_;
      return it->second;
    }
    auto asset = std::make_shared<ImageAsset>();
    asset->path = path;
    asset->lastUsed = tick_;
    images_[path] = asset;

    auto holder = std::make_shared<SDL_Surface*>(nullptr);
    int maxDim = maxDim_;
    submit(
        [holder, path, maxDim]() {
          SDL_Surface* s = IMG_Load(path.c_str());
          if (!s) return;
          if (s->w > maxDim || s->h > maxDim) {
            float k = (float)maxDim / (float)std::max(s->w, s->h);
            int nw = std::max(1, (int)(s->w * k)), nh = std::max(1, (int)(s->h * k));
            SDL_Surface* src = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
            SDL_FreeSurface(s);
            s = nullptr;
            if (src) {
              SDL_Surface* dst = SDL_CreateRGBSurfaceWithFormat(0, nw, nh, 32, SDL_PIXELFORMAT_ARGB8888);
              if (dst) {
                SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
                SDL_BlitScaled(src, nullptr, dst, nullptr);
                s = dst;
              }
              SDL_FreeSurface(src);
            }
          }
          *holder = s;
        },
        [this, holder, asset]() {
          SDL_Surface* s = *holder;
          if (s) {
            asset->tex = SDL_CreateTextureFromSurface(renderer_, s);
            asset->w = s->w;
            asset->h = s->h;
            SDL_FreeSurface(s);
          }
          if (asset->tex) {
            SDL_SetTextureBlendMode(asset->tex, SDL_BLENDMODE_BLEND);
            asset->bytes = (size_t)asset->w * asset->h * 4;
            asset->state = ImageAsset::State::Ready;
            used_ += asset->bytes;
            evict();
          } else {
            asset->state = ImageAsset::State::Failed;
          }
        });
    return asset;
  }

  void release(const std::string& path) {
    auto it = images_.find(path);
    if (it == images_.end()) return;
    used_ -= std::min(used_, it->second->bytes);
    images_.erase(it);
  }

  void pump(float budgetMs = 3.0f) {
    ++tick_;
    Uint64 start = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();
    for (;;) {
      Done d;
      SDL_LockMutex(mutex_);
      if (finished_.empty()) {
        SDL_UnlockMutex(mutex_);
        break;
      }
      d = std::move(finished_.front());
      finished_.pop_front();
      --inflight_;
      SDL_UnlockMutex(mutex_);
      if (d) d();
      double ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / freq;
      if (ms >= budgetMs) break;
    }
  }

 private:
  struct Job {
    Work work;
    Done done;
  };

  SDL_Renderer* renderer_ = nullptr;
  SDL_Thread* thread_ = nullptr;
  SDL_mutex* mutex_ = nullptr;
  SDL_cond* cond_ = nullptr;
  std::deque<Job> jobs_;
  std::deque<Done> finished_;
  bool running_ = false;
  size_t inflight_ = 0;
  std::unordered_map<std::string, std::shared_ptr<ImageAsset>> images_;
  size_t used_ = 0, budget_ = 0;
  int maxDim_ = 1024;
  uint32_t tick_ = 0;

  static int threadMain(void* p) {
    static_cast<AssetLoader*>(p)->run();
    return 0;
  }

  void run() {
    for (;;) {
      Job job;
      SDL_LockMutex(mutex_);
      while (running_ && jobs_.empty()) SDL_CondWait(cond_, mutex_);
      if (!running_) {
        SDL_UnlockMutex(mutex_);
        return;
      }
      job = std::move(jobs_.front());
      jobs_.pop_front();
      SDL_UnlockMutex(mutex_);
      if (job.work) job.work();
      SDL_LockMutex(mutex_);
      finished_.push_back(std::move(job.done));
      SDL_UnlockMutex(mutex_);
    }
  }

  void evict() {
    while (used_ > budget_) {
      auto victim = images_.end();
      for (auto it = images_.begin(); it != images_.end(); ++it) {
        if (it->second.use_count() != 1 || !it->second->ready()) continue;
        if (victim == images_.end() || it->second->lastUsed < victim->second->lastUsed) victim = it;
      }
      if (victim == images_.end()) break;
      used_ -= std::min(used_, victim->second->bytes);
      images_.erase(victim);
    }
  }
};

}
