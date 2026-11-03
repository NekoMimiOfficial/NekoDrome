#pragma once

#include "Common.hpp"

namespace neko {

enum class ColorMode { Dark, Light };

struct ColorPair {
  Color dark, light;
  const Color& get(ColorMode m) const { return m == ColorMode::Dark ? dark : light; }
};

struct ColorOverride {
  std::string name;
  Color dark, light;
};

class ColorRegistry {
 public:
  std::function<void(ColorMode)> onModeChanged;

  void set(const std::string& name, Color dark, Color light) {
    map_[name] = {dark, light};
    ++revision_;
  }
  void set(const std::string& name, Color both) { set(name, both, both); }
  void setDark(const std::string& name, Color c) {
    auto it = map_.find(name);
    if (it == map_.end()) set(name, c, c);
    else { it->second.dark = c; ++revision_; }
  }
  void setLight(const std::string& name, Color c) {
    auto it = map_.find(name);
    if (it == map_.end()) set(name, c, c);
    else { it->second.light = c; ++revision_; }
  }
  void remove(const std::string& name) {
    if (map_.erase(name)) ++revision_;
  }
  bool has(const std::string& name) const { return map_.count(name) != 0; }

  Color get(const std::string& name) const { return get(name, mode_); }
  Color get(const std::string& name, ColorMode m) const {
    auto it = map_.find(name);
    if (it == map_.end()) return Color(255, 0, 255, 255);
    return it->second.get(m);
  }

  ColorMode mode() const { return mode_; }
  void setMode(ColorMode m) {
    if (m == mode_) return;
    mode_ = m;
    ++revision_;
    if (onModeChanged) onModeChanged(m);
  }
  void toggleMode() { setMode(mode_ == ColorMode::Dark ? ColorMode::Light : ColorMode::Dark); }
  uint32_t revision() const { return revision_; }

  std::vector<std::string> names() const {
    std::vector<std::string> out;
    out.reserve(map_.size());
    for (auto& kv : map_) out.push_back(kv.first);
    return out;
  }

 private:
  std::unordered_map<std::string, ColorPair> map_;
  ColorMode mode_ = ColorMode::Dark;
  uint32_t revision_ = 1;
};

inline void registerNekoppuccin(ColorRegistry& reg) {
  struct Raw { const char* name; const char* hex; };
  static const Raw raw[] = {
      {"ash", "#251e2e"}, {"ashDark", "#1f1927"}, {"ashDarker", "#130f18"}, {"ashDarkest", "#0e0713"},
      {"purple", "#c6c1f0"}, {"purpleDark", "#ada5f1"}, {"purpleDarker", "#9d92ef"}, {"purpleDarkest", "#907ded"},
      {"sPurple", "#cab9ef"}, {"sPurpleDark", "#b896f0"}, {"sPurpleDarker", "#b283ed"}, {"sPurpleDarkest", "#a771eb"},
      {"hPurple", "#cb6af5"},
      {"purpleAshSpectrum0", "#a399c8"}, {"purpleAshSpectrum1", "#7b6d8b"},
      {"purpleAshSpectrum2", "#6f5873"}, {"purpleAshSpectrum3", "#3a3246"},
      {"purpleText", "#d7cdf4"}, {"purpleGlow", "#58336e"},
      {"blue", "#b0cdea"}, {"blueDark", "#9bbcef"}, {"blueDarker", "#90b5ed"}, {"blueDarkest", "#70a2ed"}, {"hBlue", "#93b5ff"},
      {"green", "#c0e8b8"}, {"greenDark", "#a0e277"}, {"greenDarker", "#91d368"}, {"greenDarkest", "#75c245"}, {"sGreen", "#b6fc9f"},
      {"red", "#e8b8b8"}, {"redDark", "#eb9896"}, {"redDarker", "#ee716e"}, {"redDarkest", "#dd5d59"}, {"hRed", "#ff5f5f"},
      {"yellow", "#e8e0b8"}, {"yellowDark", "#f5d27a"}, {"yellowDarker", "#f2c452"}, {"yellowDarkest", "#f1ba32"}, {"hYellow", "#f1c949"},
      {"warnRaw", "#fab78e"},
  };
  for (const Raw& r : raw) reg.set(r.name, Color::hex(r.hex));
  reg.set("warnBack", Color(250, 179, 135, 38));
  reg.set("errorBack", Color(250, 117, 125, 38));
  reg.set("successBack", Color(166, 227, 161, 38));

  struct Role { const char* name; const char* dark; const char* light; };
  static const Role roles[] = {
      {"background", "#130f18", "#f8f5fd"},
      {"surface", "#1f1927", "#f3effa"},
      {"surfaceContainer", "#251e2e", "#ece7f7"},
      {"surfaceContainerHigh", "#2f2739", "#e3dcf1"},
      {"surfaceContainerHighest", "#3a3246", "#d9d1ea"},
      {"onSurface", "#d7cdf4", "#251e2e"},
      {"onSurfaceVariant", "#a399c8", "#5b4f6b"},
      {"outline", "#7b6d8b", "#8a7d9a"},
      {"outlineVariant", "#3a3246", "#cfc6e0"},
      {"primary", "#c6c1f0", "#6a52d9"},
      {"onPrimary", "#251e2e", "#ffffff"},
      {"primaryContainer", "#58336e", "#e0d9fb"},
      {"onPrimaryContainer", "#ecdfff", "#2a1b6b"},
      {"secondary", "#b896f0", "#8a4fd1"},
      {"onSecondary", "#251e2e", "#ffffff"},
      {"secondaryContainer", "#3f3352", "#eadcfa"},
      {"onSecondaryContainer", "#e6d9fb", "#3a1f66"},
      {"tertiary", "#9bbcef", "#3f78d0"},
      {"onTertiary", "#130f18", "#ffffff"},
      {"tertiaryContainer", "#2b3f61", "#d6e4fb"},
      {"onTertiaryContainer", "#d8e6ff", "#0e2a57"},
      {"error", "#eb9896", "#c43d3a"},
      {"onError", "#130f18", "#ffffff"},
      {"errorContainer", "#5a2a2c", "#f9d6d5"},
      {"success", "#b6fc9f", "#3f8f2a"},
      {"warn", "#fab78e", "#c26a2a"},
      {"inverseSurface", "#d7cdf4", "#251e2e"},
      {"onInverseSurface", "#251e2e", "#f3effa"},
      {"focusRing", "#cb6af5", "#6a52d9"},
      {"scrim", "#000000", "#000000"},
      {"glow", "#58336e", "#cbb8f5"},
  };
  for (const Role& r : roles) reg.set(r.name, Color::hex(r.dark), Color::hex(r.light));
}

}
