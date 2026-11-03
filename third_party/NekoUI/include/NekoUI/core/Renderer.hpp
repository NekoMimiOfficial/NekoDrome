#pragma once

#include "Common.hpp"
#include "Style.hpp"

namespace neko {

struct TextMetrics {
  int w = 0, h = 0;
};

class Renderer {
 public:
  bool init(SDL_Renderer* r, const std::string& textFont, const std::string& glyphFont) {
    r_ = r;
    textFontPath_ = textFont;
    glyphFontPath_ = glyphFont.empty() ? textFont : glyphFont;
    if (!TTF_WasInit() && TTF_Init() != 0) return false;
    layers_.clear();
    layers_.push_back({1.0f, {0, 0}, 1.0f});
    return true;
  }

  void shutdown() {
    for (auto& kv : shapes_) SDL_DestroyTexture(kv.second);
    shapes_.clear();
    for (auto& e : pool_) if (e.tex) SDL_DestroyTexture(e.tex);
    pool_.clear();
    for (auto& kv : texts_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    texts_.clear();
    for (auto& kv : fonts_) TTF_CloseFont(kv.second);
    fonts_.clear();
  }

  SDL_Renderer* sdl() const { return r_; }

  void setOutput(float scale, Vec2 offset, Vec2 logicalSize) {
    scale = std::max(0.1f, scale);
    bool scaleChanged = std::fabs(scale - rootScale_) > 0.0001f;
    rootScale_ = scale;
    rootOffset_ = offset;
    logical_ = logicalSize;
    if (scaleChanged) {
      for (auto& kv : texts_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
      texts_.clear();
      for (auto& kv : fonts_) TTF_CloseFont(kv.second);
      fonts_.clear();
      ++generation_;
    }
  }
  uint32_t generation() const { return generation_; }
  float outputScale() const { return rootScale_; }

  void begin(Color clear) {
    ++frame_;
    if (frame_ % 90 == 0) trimTexts();
    layers_.resize(1);
    layers_[0] = {rootScale_, rootOffset_, 1.0f};
    clips_.clear();
    baseClips_ = 0;
    poolUsed_ = 0;
    SDL_RenderSetClipRect(r_, nullptr);
    if (rootOffset_.x > 0.5f || rootOffset_.y > 0.5f) {
      SDL_Rect base{(int)std::lround(rootOffset_.x), (int)std::lround(rootOffset_.y),
                    (int)std::lround(logical_.x * rootScale_), (int)std::lround(logical_.y * rootScale_)};
      clips_.push_back(base);
      baseClips_ = 1;
    }
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r_, clear.r8(), clear.g8(), clear.b8(), 255);
    SDL_RenderClear(r_);
    if (baseClips_) SDL_RenderSetClipRect(r_, &clips_.back());
  }

  void end() { SDL_RenderPresent(r_); }

  void pushLayer(Vec2 pivot, float scale, Vec2 translate, float opacity) {
    const Layer& p = layers_.back();
    Vec2 local((pivot.x * (1.0f - scale) + translate.x), (pivot.y * (1.0f - scale) + translate.y));
    Layer n;
    n.s = scale * p.s;
    n.o = Vec2(local.x * p.s + p.o.x, local.y * p.s + p.o.y);
    n.op = opacity * p.op;
    layers_.push_back(n);
  }
  void popLayer() {
    if (layers_.size() > 1) layers_.pop_back();
  }
  float scale() const { return layers_.back().s; }
  float opacity() const { return layers_.back().op; }

  void pushClip(const Rect& rect) {
    Rect m = map(rect);
    int x0 = (int)std::floor(m.x), y0 = (int)std::floor(m.y);
    int x1 = (int)std::ceil(m.right()), y1 = (int)std::ceil(m.bottom());
    SDL_Rect c{x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    if (!clips_.empty()) {
      const SDL_Rect& p = clips_.back();
      int nx0 = std::max(c.x, p.x), ny0 = std::max(c.y, p.y);
      int nx1 = std::min(c.x + c.w, p.x + p.w), ny1 = std::min(c.y + c.h, p.y + p.h);
      c = {nx0, ny0, std::max(0, nx1 - nx0), std::max(0, ny1 - ny0)};
    }
    clips_.push_back(c);
    SDL_RenderSetClipRect(r_, &clips_.back());
  }
  void popClip() {
    if ((int)clips_.size() <= baseClips_) return;
    clips_.pop_back();
    SDL_RenderSetClipRect(r_, clips_.empty() ? nullptr : &clips_.back());
  }
  bool clipEmpty() const { return !clips_.empty() && (clips_.back().w <= 0 || clips_.back().h <= 0); }

  void fillRect(const Rect& rect, Color c) {
    Rect m = map(rect);
    c.a *= layers_.back().op;
    if (c.a < 0.5f) return;
    SDL_Rect d = snap(m);
    if (d.w <= 0 || d.h <= 0) return;
    setDraw(c);
    SDL_RenderFillRect(r_, &d);
  }

  void fillRRect(const Rect& rect, float radius, Color c) {
    Rect m = map(rect);
    c.a *= layers_.back().op;
    if (c.a < 0.5f) return;
    SDL_Rect d = snap(m);
    if (d.w <= 0 || d.h <= 0) return;
    int ri = (int)std::lround(std::min(radius * layers_.back().s, std::min(d.w, d.h) * 0.5f));
    setDraw(c);
    if (ri <= 0) {
      SDL_RenderFillRect(r_, &d);
      return;
    }
    SDL_Rect mid{d.x, d.y + ri, d.w, d.h - 2 * ri};
    SDL_Rect top{d.x + ri, d.y, d.w - 2 * ri, ri};
    SDL_Rect bot{d.x + ri, d.y + d.h - ri, d.w - 2 * ri, ri};
    if (mid.h > 0) SDL_RenderFillRect(r_, &mid);
    if (top.w > 0) SDL_RenderFillRect(r_, &top);
    if (bot.w > 0) SDL_RenderFillRect(r_, &bot);
    corners(shape(Kind::Fill, ri, 0), d, ri, c);
  }

  void strokeRRect(const Rect& rect, float radius, float thickness, Color c) {
    Rect m = map(rect);
    c.a *= layers_.back().op;
    if (c.a < 0.5f || thickness <= 0) return;
    SDL_Rect d = snap(m);
    if (d.w <= 0 || d.h <= 0) return;
    float s = layers_.back().s;
    int ri = (int)std::lround(std::min(radius * s, std::min(d.w, d.h) * 0.5f));
    int t = std::max(1, (int)std::lround(thickness * s));
    t = std::min(t, std::min(d.w, d.h) / 2 > 0 ? std::min(d.w, d.h) / 2 : 1);
    setDraw(c);
    SDL_Rect top{d.x + ri, d.y, d.w - 2 * ri, t};
    SDL_Rect bot{d.x + ri, d.y + d.h - t, d.w - 2 * ri, t};
    SDL_Rect lef{d.x, d.y + ri, t, d.h - 2 * ri};
    SDL_Rect rig{d.x + d.w - t, d.y + ri, t, d.h - 2 * ri};
    if (top.w > 0) SDL_RenderFillRect(r_, &top);
    if (bot.w > 0) SDL_RenderFillRect(r_, &bot);
    if (lef.h > 0) SDL_RenderFillRect(r_, &lef);
    if (rig.h > 0) SDL_RenderFillRect(r_, &rig);
    if (ri > 0) corners(shape(Kind::Ring, ri, std::min(t, ri)), d, ri, c);
  }

  void cutoutCorners(const Rect& rect, float radius, Color c) {
    Rect m = map(rect);
    c.a *= layers_.back().op;
    if (c.a < 0.5f) return;
    SDL_Rect d = snap(m);
    int ri = (int)std::lround(std::min(radius * layers_.back().s, std::min(d.w, d.h) * 0.5f));
    if (ri <= 0) return;
    corners(shape(Kind::Cutout, ri, 0), d, ri, c);
  }

  void disc(Vec2 center, float radius, Color c) {
    Vec2 p(center.x * layers_.back().s + layers_.back().o.x, center.y * layers_.back().s + layers_.back().o.y);
    c.a *= layers_.back().op;
    if (c.a < 0.5f) return;
    int ri = std::max(1, (int)std::lround(radius * layers_.back().s));
    SDL_Texture* t = shape(Kind::Disc, ri, 0);
    SDL_Rect d{(int)std::lround(p.x) - ri, (int)std::lround(p.y) - ri, ri * 2, ri * 2};
    SDL_SetTextureColorMod(t, c.r8(), c.g8(), c.b8());
    SDL_SetTextureAlphaMod(t, c.a8());
    SDL_RenderCopy(r_, t, nullptr, &d);
  }

  void wave(float x, float cy, float length, float amp, float wavelength, float phase, float thickness, Color c) {
    const Layer& L = layers_.back();
    float s = L.s;
    c.a *= L.op;
    if (c.a < 0.5f || length <= 0) return;
    constexpr int S = 4;
    int n = (int)std::ceil(length * s);
    int x0 = (int)std::lround(x * s + L.o.x);
    float cyD = cy * s + L.o.y;
    float ampD = amp * s, th0 = thickness * s;
    float k = 2.0f * kPi / std::max(1.0f, wavelength * s);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    std::vector<SDL_Rect> full;
    full.reserve(n);
    struct Part { SDL_Rect r; Uint8 a; };
    std::vector<Part> parts;
    parts.reserve(n * 3);
    float tops[S], bots[S];
    for (int i = 0; i < n; ++i) {
      float minTop = 1.0e9f, maxBot = -1.0e9f;
      for (int q = 0; q < S; ++q) {
        float px = (float)i + ((float)q + 0.5f) / (float)S;
        float ph = k * px + phase;
        float y = cyD + ampD * std::sin(ph);
        float slope = ampD * k * std::cos(ph);
        float th = th0 * std::sqrt(1.0f + slope * slope);
        tops[q] = y - th * 0.5f;
        bots[q] = y + th * 0.5f;
        minTop = std::min(minTop, tops[q]);
        maxBot = std::max(maxBot, bots[q]);
      }
      int rowStart = (int)std::floor(minTop), rowEnd = (int)std::ceil(maxBot);
      int firstFull = -1, lastFull = -1;
      for (int row = rowStart; row < rowEnd; ++row) {
        float cov = 0.0f;
        for (int q = 0; q < S; ++q) {
          float o = std::min(bots[q], (float)row + 1.0f) - std::max(tops[q], (float)row);
          if (o > 0.0f) cov += o;
        }
        cov /= (float)S;
        if (cov >= 0.996f) {
          if (firstFull < 0) firstFull = row;
          lastFull = row;
        } else if (cov > 0.01f) {
          parts.push_back({{x0 + i, row, 1, 1}, (Uint8)clampf(c.a * cov + 0.5f, 0, 255)});
        }
      }
      if (firstFull >= 0) full.push_back({x0 + i, firstFull, 1, lastFull - firstFull + 1});
    }
    SDL_SetRenderDrawColor(r_, c.r8(), c.g8(), c.b8(), c.a8());
    if (!full.empty()) SDL_RenderFillRects(r_, full.data(), (int)full.size());
    for (const Part& p : parts) {
      SDL_SetRenderDrawColor(r_, c.r8(), c.g8(), c.b8(), p.a);
      SDL_RenderFillRect(r_, &p.r);
    }
  }

  void arc(Vec2 center, float radius, float startRad, float sweepRad, float thickness, Color c,
           float waveAmp = 0, float waveCount = 0, float phase = 0) {
    const Layer& L = layers_.back();
    c.a *= L.op;
    if (c.a < 0.5f || thickness <= 0.0f) return;
    if (sweepRad < 0.0f) {
      startRad += sweepRad;
      sweepRad = -sweepRad;
    }
    const float twoPi = 2.0f * kPi;
    const bool full = sweepRad >= twoPi - 0.001f;
    float cx = center.x * L.s + L.o.x, cy = center.y * L.s + L.o.y;
    float R = radius * L.s, T = thickness * L.s, A = std::fabs(waveAmp) * L.s;
    float half = R + A + T * 0.5f + 2.0f;
    int x0 = (int)std::floor(cx - half), y0 = (int)std::floor(cy - half);
    int w = (int)std::ceil(cx + half) - x0, h = (int)std::ceil(cy + half) - y0;
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) return;
    Scratch* sc = scratch(w, h);
    if (!sc) return;
    pixels_.assign((size_t)w * (size_t)h * 4u, 0);

    const float sg = waveAmp < 0 ? -1.0f : 1.0f;
    auto modR = [&](float t) { return R + (A > 0.0f ? sg * A * std::sin(waveCount * t + phase) : 0.0f); };
    float r0 = modR(0.0f), r1 = modR(sweepRad);
    float p0x = cx + std::cos(startRad) * r0, p0y = cy + std::sin(startRad) * r0;
    float p1x = cx + std::cos(startRad + sweepRad) * r1, p1y = cy + std::sin(startRad + sweepRad) * r1;
    float halfT = T * 0.5f;
    float band = A + halfT + 1.5f;

    for (int j = 0; j < h; ++j) {
      float py = (float)(y0 + j) + 0.5f;
      float dy = py - cy;
      Uint8* row = pixels_.data() + (size_t)j * (size_t)w * 4u;
      for (int i = 0; i < w; ++i) {
        float px = (float)(x0 + i) + 0.5f;
        float dx = px - cx;
        float r = std::sqrt(dx * dx + dy * dy);
        if (std::fabs(r - R) > band) continue;
        float dist = 1.0e9f;
        float t = std::atan2(dy, dx) - startRad;
        t = std::fmod(t, twoPi);
        if (t < 0.0f) t += twoPi;
        if (full || t <= sweepRad) {
          float rm = modR(t);
          float slope = 0.0f;
          if (A > 0.0f && r > 0.5f) slope = sg * A * waveCount * std::cos(waveCount * t + phase) / r;
          dist = std::fabs(r - rm) / std::sqrt(1.0f + slope * slope);
        }
        if (!full) {
          float d0 = std::sqrt((px - p0x) * (px - p0x) + (py - p0y) * (py - p0y));
          float d1 = std::sqrt((px - p1x) * (px - p1x) + (py - p1y) * (py - p1y));
          dist = std::min(dist, std::min(d0, d1));
        }
        float cov = saturate(halfT - dist + 0.5f);
        if (cov <= 0.0f) continue;
        Uint8* px4 = row + (size_t)i * 4u;
        px4[0] = px4[1] = px4[2] = 255;
        px4[3] = (Uint8)(cov * 255.0f + 0.5f);
      }
    }
    SDL_Rect up{0, 0, w, h};
    SDL_UpdateTexture(sc->tex, &up, pixels_.data(), w * 4);
    SDL_SetTextureColorMod(sc->tex, c.r8(), c.g8(), c.b8());
    SDL_SetTextureAlphaMod(sc->tex, c.a8());
    SDL_Rect dst{x0, y0, w, h};
    SDL_RenderCopy(r_, sc->tex, &up, &dst);
  }

  void texture(SDL_Texture* t, const SDL_Rect* src, const Rect& dst, float opacity = 1.0f, Color mod = Color(255, 255, 255, 255)) {
    Rect m = map(dst);
    SDL_Rect d = snap(m);
    if (d.w <= 0 || d.h <= 0) return;
    SDL_SetTextureColorMod(t, mod.r8(), mod.g8(), mod.b8());
    SDL_SetTextureAlphaMod(t, (Uint8)clampf(255.0f * opacity * layers_.back().op * (mod.a / 255.0f), 0, 255));
    SDL_RenderCopy(r_, t, src, &d);
  }

  int fontHeight(FontFace face, int size) {
    TTF_Font* f = font(face, devSize(size));
    return f ? (int)std::ceil((float)TTF_FontHeight(f) / rootScale_) : size;
  }

  TextMetrics textSize(const std::string& s, FontFace face, int size, int wrap = 0) {
    int ds = devSize(size);
    TTF_Font* f = font(face, ds);
    if (!f) return {0, size};
    if (s.empty()) return {0, (int)std::ceil((float)TTF_FontHeight(f) / rootScale_)};
    if (wrap > 0) {
      TextEntry* e = textEntry(s, face, ds, (int)std::lround(wrap * rootScale_));
      if (e) return {(int)std::ceil((float)e->w / rootScale_), (int)std::ceil((float)e->h / rootScale_)};
      return {0, (int)std::ceil((float)TTF_FontHeight(f) / rootScale_)};
    }
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s.c_str(), &w, &h);
    return {(int)std::ceil((float)w / rootScale_), (int)std::ceil((float)h / rootScale_)};
  }

  void text(const std::string& s, FontFace face, int size, Vec2 pos, Color c, int wrap = 0) {
    if (s.empty()) return;
    c.a *= layers_.back().op;
    if (c.a < 0.5f) return;
    TextEntry* e = textEntry(s, face, devSize(size), wrap > 0 ? (int)std::lround(wrap * rootScale_) : 0);
    if (!e || !e->tex) return;
    float k = layers_.back().s / rootScale_;
    Rect m = map({pos.x, pos.y, 0.0f, 0.0f});
    SDL_Rect d{(int)std::lround(m.x), (int)std::lround(m.y), std::max(1, (int)std::lround((float)e->w * k)),
               std::max(1, (int)std::lround((float)e->h * k))};
    SDL_SetTextureColorMod(e->tex, c.r8(), c.g8(), c.b8());
    SDL_SetTextureAlphaMod(e->tex, c.a8());
    SDL_RenderCopy(r_, e->tex, nullptr, &d);
  }

  Rect map(const Rect& r) const {
    const Layer& L = layers_.back();
    return {r.x * L.s + L.o.x, r.y * L.s + L.o.y, r.w * L.s, r.h * L.s};
  }

 private:
  enum class Kind { Fill, Ring, Cutout, Disc };
  struct Layer {
    float s;
    Vec2 o;
    float op;
  };
  struct Scratch {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
  };
  struct TextEntry {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    uint32_t used = 0;
  };

  SDL_Renderer* r_ = nullptr;
  std::string textFontPath_, glyphFontPath_;
  std::unordered_map<int, TTF_Font*> fonts_;
  std::unordered_map<uint64_t, SDL_Texture*> shapes_;
  std::unordered_map<std::string, TextEntry> texts_;
  std::vector<Layer> layers_;
  std::vector<SDL_Rect> clips_;
  uint32_t frame_ = 0;
  float rootScale_ = 1.0f;
  Vec2 rootOffset_;
  Vec2 logical_{960, 544};
  uint32_t generation_ = 1;
  int baseClips_ = 0;
  std::vector<Scratch> pool_;
  size_t poolUsed_ = 0;
  std::vector<Uint8> pixels_;

  Scratch* scratch(int w, int h) {
    if (poolUsed_ >= pool_.size()) pool_.emplace_back();
    Scratch& e = pool_[poolUsed_++];
    if (!e.tex || e.w < w || e.h < h) {
      if (e.tex) SDL_DestroyTexture(e.tex);
      int nw = std::max(w, e.w), nh = std::max(h, e.h);
      nw = (nw + 15) / 16 * 16;
      nh = (nh + 15) / 16 * 16;
      e.tex = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, nw, nh);
      if (!e.tex) {
        e.w = e.h = 0;
        return nullptr;
      }
      SDL_SetTextureBlendMode(e.tex, SDL_BLENDMODE_BLEND);
      e.w = nw;
      e.h = nh;
    }
    return &e;
  }

  int devSize(int size) const { return std::max(4, (int)std::lround((float)size * rootScale_)); }

  static SDL_Rect snap(const Rect& m) {
    int x0 = (int)std::lround(m.x), y0 = (int)std::lround(m.y);
    int x1 = (int)std::lround(m.x + m.w), y1 = (int)std::lround(m.y + m.h);
    return {x0, y0, x1 - x0, y1 - y0};
  }

  void setDraw(const Color& c) {
    SDL_SetRenderDrawBlendMode(r_, c.a >= 254.5f ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, c.r8(), c.g8(), c.b8(), c.a8());
  }

  void corners(SDL_Texture* t, const SDL_Rect& d, int ri, const Color& c) {
    SDL_SetTextureColorMod(t, c.r8(), c.g8(), c.b8());
    SDL_SetTextureAlphaMod(t, c.a8());
    SDL_Rect tl{d.x, d.y, ri, ri};
    SDL_Rect tr{d.x + d.w - ri, d.y, ri, ri};
    SDL_Rect bl{d.x, d.y + d.h - ri, ri, ri};
    SDL_Rect br{d.x + d.w - ri, d.y + d.h - ri, ri, ri};
    SDL_RenderCopy(r_, t, nullptr, &tl);
    SDL_RenderCopyEx(r_, t, nullptr, &tr, 0, nullptr, SDL_FLIP_HORIZONTAL);
    SDL_RenderCopyEx(r_, t, nullptr, &bl, 0, nullptr, SDL_FLIP_VERTICAL);
    SDL_RenderCopyEx(r_, t, nullptr, &br, 0, nullptr, (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL));
  }

  static float coverage(float x, float y, float rad) {
    constexpr int N = 6;
    int hits = 0;
    float r2 = rad * rad;
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < N; ++j) {
        float sx = x + (i + 0.5f) / N, sy = y + (j + 0.5f) / N;
        if (sx * sx + sy * sy <= r2) ++hits;
      }
    }
    return (float)hits / (N * N);
  }

  SDL_Texture* shape(Kind kind, int ri, int thick) {
    uint64_t key = ((uint64_t)kind << 48) | ((uint64_t)ri << 24) | (uint64_t)thick;
    auto it = shapes_.find(key);
    if (it != shapes_.end()) return it->second;
    int size = kind == Kind::Disc ? ri * 2 : ri;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_LockSurface(surf);
    for (int j = 0; j < size; ++j) {
      Uint8* row = (Uint8*)surf->pixels + j * surf->pitch;
      for (int i = 0; i < size; ++i) {
        float a;
        if (kind == Kind::Disc) {
          a = coverage((float)(i - ri), (float)(j - ri), (float)ri);
        } else {
          float outer = coverage((float)(i - ri), (float)(j - ri), (float)ri);
          if (kind == Kind::Fill) a = outer;
          else if (kind == Kind::Cutout) a = 1.0f - outer;
          else {
            float inner = (ri - thick) > 0 ? coverage((float)(i - ri), (float)(j - ri), (float)(ri - thick)) : 0.0f;
            a = std::max(0.0f, outer - inner);
          }
        }
        row[i * 4 + 0] = 255;
        row[i * 4 + 1] = 255;
        row[i * 4 + 2] = 255;
        row[i * 4 + 3] = (Uint8)(a * 255.0f + 0.5f);
      }
    }
    SDL_UnlockSurface(surf);
    SDL_Texture* t = SDL_CreateTextureFromSurface(r_, surf);
    SDL_FreeSurface(surf);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    if (shapes_.size() > 600) {
      for (auto& kv : shapes_) SDL_DestroyTexture(kv.second);
      shapes_.clear();
    }
    shapes_[key] = t;
    return t;
  }

  TTF_Font* font(FontFace face, int size) {
    int key = (face == FontFace::Glyph ? 1 << 20 : 0) | size;
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    const std::string& path = face == FontFace::Glyph ? glyphFontPath_ : textFontPath_;
    TTF_Font* f = TTF_OpenFont(path.c_str(), std::max(4, size));
    if (f) TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    fonts_[key] = f;
    return f;
  }

  TextEntry* textEntry(const std::string& s, FontFace face, int size, int wrap) {
    std::string key;
    key.reserve(s.size() + 16);
    key += face == FontFace::Glyph ? 'g' : 't';
    key += std::to_string(size);
    key += '|';
    key += std::to_string(wrap);
    key += '|';
    key += s;
    auto it = texts_.find(key);
    if (it != texts_.end()) {
      it->second.used = frame_;
      return &it->second;
    }
    TTF_Font* f = font(face, size);
    if (!f) return nullptr;
    SDL_Color white{255, 255, 255, 255};
    SDL_Surface* surf = wrap > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), white, (Uint32)wrap)
                                 : TTF_RenderUTF8_Blended(f, s.c_str(), white);
    TextEntry e;
    if (surf) {
      e.tex = SDL_CreateTextureFromSurface(r_, surf);
      e.w = surf->w;
      e.h = surf->h;
      SDL_FreeSurface(surf);
      if (e.tex) SDL_SetTextureBlendMode(e.tex, SDL_BLENDMODE_BLEND);
    }
    e.used = frame_;
    return &(texts_[key] = e);
  }

  void trimTexts() {
    if (texts_.size() < 300) return;
    for (auto it = texts_.begin(); it != texts_.end();) {
      if (frame_ - it->second.used > 120) {
        if (it->second.tex) SDL_DestroyTexture(it->second.tex);
        it = texts_.erase(it);
      } else {
        ++it;
      }
    }
  }
};

}
