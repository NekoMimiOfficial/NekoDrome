#pragma once

#include "Style.hpp"

namespace neko {

inline void registerDefaultStyles(StyleSheet& sh) {
  using R = ColorRef;
  auto mixed = [](const char* base, const char* over, float t) { return R(base).mix(R(over), t); };

  auto stateLayers = [&](ClassStyle& c, const char* base, const char* over) {
    c.hovered.background = mixed(base, over, 0.08f);
    c.focused.background = mixed(base, over, 0.12f);
    c.pressed.background = mixed(base, over, 0.16f);
    c.pressed.scale = 0.95f;
    c.pressed.radius = 18.0f;
  };
  auto ghostLayers = [&](ClassStyle& c, const char* over) {
    c.hovered.background = R(over).a(0.08f);
    c.focused.background = R(over).a(0.12f);
    c.pressed.background = R(over).a(0.16f);
    c.pressed.scale = 0.94f;
    c.pressed.radius = 16.0f;
  };

  sh.cls("title").base.fontSize = 28.0f;
  sh.cls("headline").base.fontSize = 22.0f;
  {
    ClassStyle& c = sh.cls("subtitle");
    c.base.fontSize = 18.0f;
    c.base.color = R("onSurfaceVariant");
  }
  sh.cls("body").base.fontSize = 16.0f;
  {
    ClassStyle& c = sh.cls("caption");
    c.base.fontSize = 13.0f;
    c.base.color = R("onSurfaceVariant");
  }
  {
    ClassStyle& c = sh.cls("glyph");
    c.base.fontSize = 24.0f;
    c.base.fontFace = FontFace::Glyph;
  }

  {
    ClassStyle& c = sh.cls("button");
    c.base.direction = Direction::Row;
    c.base.align = Align::Center;
    c.base.justify = Justify::Center;
    c.base.gap = 8.0f;
    c.base.padding = Insets(24.0f, 10.0f);
    c.base.minHeight = 48.0f;
    c.base.radius = 999.0f;
    c.base.background = R("primary");
    c.base.color = R("onPrimary");
    stateLayers(c, "primary", "onPrimary");
  }
  {
    ClassStyle& c = sh.cls("button.tonal");
    c.base.background = R("secondaryContainer");
    c.base.color = R("onSecondaryContainer");
    stateLayers(c, "secondaryContainer", "onSecondaryContainer");
  }
  {
    ClassStyle& c = sh.cls("button.outlined");
    c.base.background = R::transparent();
    c.base.color = R("primary");
    c.base.borderWidth = 1.5f;
    c.base.borderColor = R("outline");
    ghostLayers(c, "primary");
    c.focused.borderColor = R("primary");
  }
  {
    ClassStyle& c = sh.cls("button.text");
    c.base.background = R::transparent();
    c.base.color = R("primary");
    c.base.padding = Insets(16.0f, 10.0f);
    ghostLayers(c, "primary");
  }
  {
    ClassStyle& c = sh.cls("button.icon");
    c.base.background = R::transparent();
    c.base.color = R("onSurfaceVariant");
    c.base.padding = Insets(12.0f);
    c.base.minWidth = 48.0f;
    c.base.minHeight = 48.0f;
    ghostLayers(c, "onSurface");
    c.pressed.scale = 0.9f;
    c.pressed.radius = 14.0f;
    c.checked.background = R("secondaryContainer");
    c.checked.color = R("onSecondaryContainer");
  }
  {
    ClassStyle& c = sh.cls("button.icon-filled");
    c.base.padding = Insets(16.0f);
    c.base.minWidth = 56.0f;
    c.base.minHeight = 56.0f;
    c.base.background = R("primary");
    c.base.color = R("onPrimary");
    stateLayers(c, "primary", "onPrimary");
    c.pressed.scale = 0.9f;
    c.pressed.radius = 16.0f;
  }
  {
    ClassStyle& c = sh.cls("button.nav");
    c.base.background = R::transparent();
    c.base.color = R("onSurfaceVariant");
    c.base.padding = Insets(16.0f);
    c.base.minWidth = 56.0f;
    c.base.minHeight = 56.0f;
    c.base.radius = 20.0f;
    ghostLayers(c, "onSurface");
    c.pressed.radius = 12.0f;
    c.checked.background = R("secondaryContainer");
    c.checked.color = R("onSecondaryContainer");
    c.checked.radius = 999.0f;
  }
  {
    ClassStyle& c = sh.cls("button.list");
    c.base.justify = Justify::Start;
    c.base.padding = Insets(18.0f, 12.0f);
    c.base.minHeight = 56.0f;
    c.base.gap = 16.0f;
    c.base.radius = 20.0f;
    c.base.background = R::transparent();
    c.base.color = R("onSurface");
    c.hovered.background = R("onSurface").a(0.08f);
    c.focused.background = R("primaryContainer");
    c.focused.color = R("onPrimaryContainer");
    c.pressed.background = R("primaryContainer");
    c.pressed.color = R("onPrimaryContainer");
    c.pressed.scale = 0.98f;
    c.pressed.radius = 14.0f;
    c.checked.background = R("secondaryContainer");
    c.checked.color = R("onSecondaryContainer");
  }

  {
    ClassStyle& c = sh.cls("card");
    c.base.background = R("surfaceContainer");
    c.base.radius = 24.0f;
    c.base.padding = Insets(16.0f);
    c.base.gap = 8.0f;
  }
  {
    ClassStyle& c = sh.cls("divider");
    c.base.height = Dim::px(1.0f);
    c.base.width = Dim::fill();
    c.base.background = R("outlineVariant");
  }
  {
    ClassStyle& c = sh.cls("toggle");
    c.base.background = R("surfaceContainerHighest");
    c.base.borderWidth = 2.0f;
    c.base.borderColor = R("outline");
    c.base.radius = 999.0f;
    c.base.handle = R("outline");
    c.base.track = R("primary");
    c.checked.background = R("primary");
    c.checked.borderColor = R("primary");
    c.checked.handle = R("onPrimary");
    c.checked.track = R("primary");
    c.hovered.borderColor = R("onSurfaceVariant");
  }
  {
    ClassStyle& c = sh.cls("checkbox");
    c.base.background = R::transparent();
    c.base.borderWidth = 2.0f;
    c.base.borderColor = R("onSurfaceVariant");
    c.base.radius = 6.0f;
    c.base.color = R("onPrimary");
    c.hovered.background = R("onSurface").a(0.08f);
    c.pressed.scale = 0.9f;
    c.checked.background = R("primary");
    c.checked.borderColor = R("primary");
  }
  {
    ClassStyle& c = sh.cls("radio");
    c.base.background = R::transparent();
    c.base.borderWidth = 2.0f;
    c.base.borderColor = R("onSurfaceVariant");
    c.base.radius = 999.0f;
    c.base.accent = R("primary");
    c.hovered.background = R("onSurface").a(0.08f);
    c.pressed.scale = 0.9f;
    c.checked.borderColor = R("primary");
  }
  {
    ClassStyle& c = sh.cls("progress");
    c.base.accent = R("primary");
    c.base.track = R("secondaryContainer");
  }
  {
    ClassStyle& c = sh.cls("circular-progress");
    c.base.accent = R("primary");
    c.base.track = R("secondaryContainer");
  }
  {
    ClassStyle& c = sh.cls("slider");
    c.base.accent = R("primary");
    c.base.track = R("secondaryContainer");
    c.base.handle = R("primary");
    c.base.width = Dim::fill();
  }

  {
    ClassStyle& c = sh.cls("hintbar");
    c.base.direction = Direction::Row;
    c.base.gap = 20.0f;
    c.base.align = Align::Center;
    c.base.justify = Justify::Center;
  }
  {
    ClassStyle& c = sh.cls("hint");
    c.base.direction = Direction::Row;
    c.base.gap = 8.0f;
    c.base.align = Align::Center;
  }
  {
    ClassStyle& c = sh.cls("hint-key");
    c.base.background = R("surfaceContainerHighest");
    c.base.color = R("onSurface");
    c.base.radius = 999.0f;
    c.base.padding = Insets(10.0f, 3.0f);
    c.base.fontSize = 12.0f;
  }
  {
    ClassStyle& c = sh.cls("hint-label");
    c.base.fontSize = 13.0f;
    c.base.color = R("onSurfaceVariant");
  }

  {
    ClassStyle& c = sh.cls("music-title");
    c.base.fontSize = 18.0f;
  }
  {
    ClassStyle& c = sh.cls("view-title");
    c.base.fontSize = 22.0f;
    c.base.color = R("onSurface");
  }
  {
    ClassStyle& c = sh.cls("toast");
    c.base.background = R("inverseSurface");
    c.base.color = R("onInverseSurface");
    c.base.radius = 20.0f;
    c.base.padding = Insets(20.0f, 16.0f);
    c.base.gap = 4.0f;
    c.base.maxWidth = 360.0f;
  }
  sh.cls("toast-title").base.fontSize = 16.0f;
  {
    ClassStyle& c = sh.cls("toast-body");
    c.base.fontSize = 14.0f;
    c.base.wrap = true;
  }
  {
    ClassStyle& c = sh.cls("dialog");
    c.base.background = R("surfaceContainerHigh");
    c.base.color = R("onSurface");
    c.base.radius = 28.0f;
    c.base.padding = Insets(24.0f);
    c.base.gap = 16.0f;
    c.base.minWidth = 320.0f;
    c.base.maxWidth = 520.0f;
  }

  {
    ClassStyle& c = sh.cls("shell-root");
    c.base.background = R("background");
    c.base.direction = Direction::Row;
    c.base.padding = Insets(12.0f);
    c.base.gap = 12.0f;
    c.base.color = R("onSurface");
  }
  {
    ClassStyle& c = sh.cls("shell-main");
    c.base.direction = Direction::Column;
    c.base.width = Dim::fill();
    c.base.height = Dim::fill();
    c.base.gap = 10.0f;
  }
  {
    ClassStyle& c = sh.cls("shell-top");
    c.base.direction = Direction::Row;
    c.base.align = Align::Center;
    c.base.gap = 12.0f;
    c.base.padding = Insets(18.0f, 6.0f);
    c.base.minHeight = 56.0f;
    c.base.width = Dim::fill();
    c.base.background = R("surfaceContainer");
    c.base.radius = 24.0f;
  }
  {
    ClassStyle& c = sh.cls("shell-body");
    c.base.direction = Direction::Column;
    c.base.width = Dim::fill();
    c.base.height = Dim::fill();
    c.base.background = R("surface");
    c.base.radius = 24.0f;
    c.base.overflow = Overflow::Clip;
  }
  {
    ClassStyle& c = sh.cls("shell-bottom");
    c.base.direction = Direction::Row;
    c.base.align = Align::Center;
    c.base.justify = Justify::Center;
    c.base.padding = Insets(18.0f, 6.0f);
    c.base.minHeight = 36.0f;
    c.base.width = Dim::fill();
    c.base.background = R("surfaceContainer");
    c.base.radius = 18.0f;
  }
  {
    ClassStyle& c = sh.cls("shell-nav");
    c.base.direction = Direction::Column;
    c.base.width = Dim::px(84.0f);
    c.base.height = Dim::fill();
    c.base.align = Align::Center;
    c.base.gap = 12.0f;
  }
  {
    ClassStyle& c = sh.cls("shell-nav-header");
    c.base.direction = Direction::Column;
    c.base.align = Align::Center;
    c.base.justify = Justify::Center;
    c.base.minHeight = 56.0f;
  }
  {
    ClassStyle& c = sh.cls("shell-nav-body");
    c.base.direction = Direction::Column;
    c.base.width = Dim::fill();
    c.base.height = Dim::fill();
    c.base.align = Align::Center;
    c.base.justify = Justify::Center;
    c.base.gap = 10.0f;
    c.base.padding = Insets(10.0f);
    c.base.background = R("surfaceContainer");
    c.base.radius = 32.0f;
  }
  {
    ClassStyle& c = sh.cls("shell-drawer");
    c.base.direction = Direction::Column;
    c.base.width = Dim::px(300.0f);
    c.base.height = Dim::fill();
    c.base.background = R("surfaceContainer");
    c.base.radius = 28.0f;
    c.base.padding = Insets(16.0f);
    c.base.gap = 6.0f;
    c.base.color = R("onSurface");
  }
}

}
