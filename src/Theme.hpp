#pragma once

#include <NekoUI/Ui.hpp>

namespace nt {

struct Palette {
  const char* name;
  const char* primaryD;
  const char* primaryL;
  const char* onPrimaryD;
  const char* onPrimaryL;
  const char* containerD;
  const char* containerL;
  const char* onContainerD;
  const char* onContainerL;
  const char* secondaryD;
  const char* secondaryL;
  const char* secContainerD;
  const char* secContainerL;
  const char* onSecContainerD;
  const char* onSecContainerL;
  const char* ringD;
  const char* ringL;
};

inline const std::vector<Palette>& palettes() {
  static const std::vector<Palette> p = {
      {"Neko", "#c6c1f0", "#6a52d9", "#251e2e", "#ffffff", "#58336e", "#e0d9fb", "#ecdfff", "#2a1b6b", "#b896f0",
       "#8a4fd1", "#3f3352", "#eadcfa", "#e6d9fb", "#3a1f66", "#cb6af5", "#6a52d9"},
      {"Sakura", "#ffb1c8", "#b4235a", "#3a0b1f", "#ffffff", "#6b2442", "#ffd9e3", "#ffd9e3", "#3e001e", "#e8b6c8",
       "#8c4a63", "#4a2e3a", "#f7dce6", "#f7dce6", "#3a1b27", "#ff7fa8", "#b4235a"},
      {"Mint", "#8fe0bd", "#00795a", "#00382a", "#ffffff", "#0f5a45", "#b6f2d8", "#b6f2d8", "#002116", "#a5cfbd",
       "#3f6757", "#2b4a3e", "#d1ebe0", "#d1ebe0", "#0b2a1f", "#5ee0b0", "#00795a"},
      {"Ocean", "#9bc4ff", "#1f5fbf", "#00315e", "#ffffff", "#1c4a80", "#d6e6ff", "#d6e6ff", "#001b3d", "#b0c6e8",
       "#4b6086", "#2e4260", "#dae5f8", "#dae5f8", "#0d1b33", "#62a8ff", "#1f5fbf"},
  };
  return p;
}

inline void applyPalette(neko::ColorRegistry& reg, int index) {
  const auto& all = palettes();
  const Palette& p = all[(size_t)std::max(0, std::min(index, (int)all.size() - 1))];
  auto set = [&](const char* name, const char* d, const char* l) { reg.set(name, neko::Color::hex(d), neko::Color::hex(l)); };
  set("primary", p.primaryD, p.primaryL);
  set("onPrimary", p.onPrimaryD, p.onPrimaryL);
  set("primaryContainer", p.containerD, p.containerL);
  set("onPrimaryContainer", p.onContainerD, p.onContainerL);
  set("secondary", p.secondaryD, p.secondaryL);
  set("secondaryContainer", p.secContainerD, p.secContainerL);
  set("onSecondaryContainer", p.onSecContainerD, p.onSecContainerL);
  set("focusRing", p.ringD, p.ringL);
}

inline std::vector<std::pair<std::string, neko::ClassStyle>> appStyles() {
  using neko::ColorRef;
  using neko::Dim;
  using neko::Insets;
  std::vector<std::pair<std::string, neko::ClassStyle>> out;

  neko::ClassStyle tile;
  tile.base.direction = neko::Direction::Column;
  tile.base.align = neko::Align::Start;
  tile.base.justify = neko::Justify::Start;
  tile.base.gap = 6.0f;
  tile.base.padding = Insets(10.0f);
  tile.base.minHeight = 0.0f;
  tile.base.radius = 22.0f;
  tile.base.width = Dim::px(200.0f);
  tile.base.background = ColorRef("surfaceContainer");
  tile.base.color = ColorRef("onSurface");
  tile.hovered.background = ColorRef("surfaceContainerHigh");
  tile.focused.background = ColorRef("surfaceContainerHighest");
  tile.pressed.background = ColorRef("surfaceContainerHighest");
  tile.pressed.scale = 0.96f;
  tile.pressed.radius = 16.0f;
  out.push_back({"button.tile", tile});

  neko::ClassStyle chip;
  chip.base.padding = Insets(16.0f, 6.0f);
  chip.base.minHeight = 38.0f;
  chip.base.gap = 6.0f;
  chip.base.background = ColorRef("surfaceContainerHigh");
  chip.base.color = ColorRef("onSurface");
  chip.hovered.background = ColorRef("surfaceContainerHighest");
  chip.focused.background = ColorRef("surfaceContainerHighest");
  chip.pressed.background = ColorRef("surfaceContainerHighest");
  chip.pressed.scale = 0.94f;
  chip.checked.background = ColorRef("primary");
  chip.checked.color = ColorRef("onPrimary");
  out.push_back({"button.chip", chip});

  neko::ClassStyle mini;
  mini.base.justify = neko::Justify::Start;
  mini.base.padding = Insets(6.0f);
  mini.base.minHeight = 0.0f;
  mini.base.gap = 12.0f;
  mini.base.radius = 16.0f;
  mini.base.background = ColorRef::transparent();
  mini.base.color = ColorRef("onSurface");
  mini.hovered.background = ColorRef("onSurface").a(0.08f);
  mini.focused.background = ColorRef("onSurface").a(0.12f);
  mini.pressed.background = ColorRef("onSurface").a(0.16f);
  mini.pressed.scale = 0.97f;
  out.push_back({"button.mini", mini});

  neko::ClassStyle field;
  field.base.background = ColorRef("surfaceContainerHigh");
  field.base.color = ColorRef("onSurface");
  field.base.radius = 16.0f;
  field.base.borderWidth = 2.0f;
  field.base.borderColor = ColorRef("outlineVariant");
  field.base.padding = Insets(16.0f, 13.0f);
  field.base.width = Dim::fill();
  field.base.fontSize = 17.0f;
  field.hovered.borderColor = ColorRef("outline");
  field.focused.borderColor = ColorRef("primary");
  field.pressed.scale = 0.99f;
  out.push_back({"textfield", field});

  neko::ClassStyle seek;
  seek.base.accent = ColorRef("primary");
  seek.base.track = ColorRef("secondaryContainer");
  seek.base.handle = ColorRef("primary");
  seek.base.width = Dim::fill();
  seek.focused.scale = 1.0f;
  out.push_back({"wavyseek", seek});

  neko::ClassStyle rowTitle;
  rowTitle.base.fontSize = 17.0f;
  out.push_back({"row-title", rowTitle});

  neko::ClassStyle tileTitle;
  tileTitle.base.fontSize = 15.0f;
  out.push_back({"tile-title", tileTitle});

  neko::ClassStyle section;
  section.base.fontSize = 22.0f;
  section.base.color = ColorRef("onSurface");
  out.push_back({"section-title", section});

  neko::ClassStyle big;
  big.base.fontSize = 34.0f;
  out.push_back({"hero-title", big});

  neko::ClassStyle banner;
  banner.base.background = ColorRef("tertiaryContainer");
  banner.base.color = ColorRef("onTertiaryContainer");
  banner.base.radius = 20.0f;
  banner.base.padding = Insets(18.0f, 12.0f);
  banner.base.direction = neko::Direction::Row;
  banner.base.align = neko::Align::Center;
  banner.base.gap = 12.0f;
  out.push_back({"banner", banner});

  neko::ClassStyle errorBanner;
  errorBanner.base.background = ColorRef("errorContainer");
  errorBanner.base.color = ColorRef("error");
  errorBanner.base.radius = 16.0f;
  errorBanner.base.padding = Insets(16.0f, 10.0f);
  out.push_back({"error-banner", errorBanner});

  neko::ClassStyle settingRow;
  settingRow.base.direction = neko::Direction::Row;
  settingRow.base.align = neko::Align::Center;
  settingRow.base.gap = 14.0f;
  settingRow.base.minHeight = 48.0f;
  out.push_back({"setting-row", settingRow});

  return out;
}

}
