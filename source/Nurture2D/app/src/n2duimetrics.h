#pragma once

namespace N2DUiMetrics {

// Unified toolbar sizing. Reduced by ~40% from legacy 22px toolbar icons.
static constexpr int TOOLBAR_ICON_PX = 18;

// Used by embedded toolbar widgets (Pen Palette / Auto Fill Controls).
static constexpr int TOOLBAR_EMBEDDED_ICON_PX = TOOLBAR_ICON_PX;

// Toolbar button hitbox styling. This controls actual toolbar thickness.
static constexpr int TOOLBAR_BUTTON_PADDING_PX = 4;
static constexpr int TOOLBAR_BUTTON_RADIUS_PX = 12;

// Thin divider bars used inside toolbar-embedded widgets.
static constexpr int TOOLBAR_SEPARATOR_THICKNESS_PX = 2;

} // namespace N2DUiMetrics
