#ifndef COSMICUIMETRICS_H
#define COSMICUIMETRICS_H

#include <QColor>
#include <QSettings>

#include "preferencesdef.h"
#include "pencildef.h"

// Single source of truth for Cosmic color palette.
// Used for active/locked states, selection highlights, scrubber, focus rings, warnings, etc.
namespace CosmicColor {

// ---------------------------------------------------------------------------
// Base UI palette — backgrounds, borders, text
// ---------------------------------------------------------------------------

// cosmicDarkGrey — #080808
// The deepest background level. Reserved for very specific base surfaces that
// need to recede visually. Use sparingly.
// Examples: CosmicMenuBar background strip, timeline track area background,
//           the outermost application window fill.
inline QColor darkGrey() { return QColor(0x08, 0x08, 0x08, 0xff); }

// cosmicGrey — #121212
// The most commonly used background color across the entire UI.
// Use this for any panel, pane, dock, or popup surface.
// Examples: side panel backgrounds, CosmicMenuBar entry popups,
//           context menu backgrounds, brush settings dialog background,
//           layer panel background, color picker panel.
inline QColor grey() { return QColor(0x12, 0x12, 0x12, 0xff); }

// cosmicWidgetControl — #1c1d1c
// Dual purpose: (1) the base fill color for all interactive widget controls,
// and (2) the universal highlight/hover/selection color across the UI.
// If something needs to look "selected" or "hovered", this is the color.
// Examples: button base fill, slider track, input field background,
//           context menu item hover, CosmicMenuBar entry highlight,
//           selected layer highlight, active tool highlight.
inline QColor widgetControl() { return QColor(0x1c, 0x1d, 0x1c, 0xff); }

// cosmicWindowBorder — #4d4d4d
// The border color for any element that has a visible edge or frame.
// Use consistently on anything that is a floating or bounded surface.
// Examples: popup window borders, file browser frame, context menu outline,
//           dialog borders, panel dividers, widget control outlines.
inline QColor windowBorder() { return QColor(0x4d, 0x4d, 0x4d, 0xff); }

// cosmicPopupBorder — #5a5a5a
// Light grey border for all floating windows, popups, and context menus.
// Provides clear visual separation from the dark app background, similar to
// macOS dark mode window borders. Use on CosmicPopupWindow subclasses and
// CosmicMenuWindow instead of the near-black #1f1f1f.
inline QColor popupBorder() { return QColor(0x5a, 0x5a, 0x5a, 0xff); }

// cosmicText / cosmicIconColor — #b3b3b3
// The primary foreground color for both text labels AND icons.
// These are intentionally the same value — if you're looking for "icon color"
// or "label color", this is it. Do not use separate values for text vs icons.
// Examples: panel labels, button text, toolbar icon tint, menu item text,
//           slider value readout, input field text.
inline QColor text() { return QColor(0xb3, 0xb3, 0xb3, 0xff); }

// cosmicDimText — #666666
// Subdued foreground for secondary information, placeholders, and disabled states.
// Use when content should be present but not compete for attention.
// Examples: keyboard shortcut hints in menus, placeholder text in input fields,
//           disabled control labels, metadata/secondary descriptors.
inline QColor dimText() { return QColor(0x66, 0x66, 0x66, 0xff); }

inline QColor accent()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    const QString hex = settings.value(SETTING_COSMIC_ACCENT_COLOR,
                                       QStringLiteral("#6a4cff")).toString();
    return QColor(hex);
}

inline QString accentHex()
{
    return accent().name(); // returns "#rrggbb"
}

// Cosmic Red - used for warnings, destructive actions, close buttons
inline QColor red()
{
    return QColor(0xff, 0x77, 0x33, 0xff); // #ff7733ff
}

// Cosmic Green - used for success states, confirmations
inline QColor green()
{
    return QColor(0x62, 0xcc, 0x29, 0xff); // #62cc29ff
}

// Cosmic Yellow - used for caution, alerts
inline QColor yellow()
{
    return QColor(0xff, 0xb2, 0x1a, 0xff); // #ffb21aff
}

// --- Menu bar strip colors ---
inline QColor menuBarBackground()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuBarBackground"), QStringLiteral("#252525")).toString());
}
inline QColor menuBarHover()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuBarHover"), QStringLiteral("#3a3a3a")).toString());
}
inline QColor menuBarLabel()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuBarLabel"), QStringLiteral("#b3b3b3")).toString());
}
inline QColor menuBarLabelDisabled()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuBarLabelDisabled"), QStringLiteral("#555555")).toString());
}

// --- Menu window popup colors ---
inline QColor menuWindowBackground()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuWindowBackground"), QStringLiteral("#2f2f2f")).toString());
}
inline QColor menuWindowBorder()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuWindowBorder"), QStringLiteral("#5a5a5a")).toString());
}
inline QColor menuWindowSeparator()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuWindowSeparator"), QStringLiteral("#1f1f1f")).toString());
}
inline QColor menuWindowItemText()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuWindowItemText"), QStringLiteral("#e6e6e6")).toString());
}
inline QColor menuWindowItemTextDisabled()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuWindowItemTextDisabled"), QStringLiteral("#555555")).toString());
}
inline QColor menuWindowShortcutText()
{
    QSettings s(PENCIL2D, PENCIL2D);
    return QColor(s.value(QStringLiteral("menuWindowShortcutText"), QStringLiteral("#777777")).toString());
}

} // namespace CosmicColor

namespace CosmicUiMetrics {

// Primary control thickness — the single source of truth for all Cosmic widget
// sizing. Horizontal controls use this as their fixed height, vertical controls
// as their fixed width, and square controls (e.g. checkboxes) as both width and
// height. Reads from QSettings so a future theming system can override it.
inline int controlThickness()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    return qMax(16, settings.value(QStringLiteral("cosmicControlThickness"), 22).toInt());
}

inline int controlCenterThickness()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    return qMax(24, settings.value(QStringLiteral("cosmicControlCenterThickness"), 32).toInt());
}

// cosmicTextSize — the single source of truth for all UI text sizes.
// Default is 9pt (matches the main menu and all cosmic widgets).
// Changing this value in QSettings propagates to every widget that reads it.
inline int cosmicTextSize()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    return qMax(6, settings.value(QStringLiteral("cosmicTextSize"), 9).toInt());
}

inline int cornerRadius()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    return qMax(0, settings.value(SETTING_COSMIC_ROUNDNESS, 6).toInt());
}

inline int windowCornerRadius()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    return qMax(14, settings.value(SETTING_COSMIC_ROUNDNESS, 14).toInt());
}

// Nested rounding: inner element inset by `inset` pixels inside a container
// with `outerRadius`. Mirrors macOS/Blender convention: r_inner = r_outer - inset.
inline int nestedCornerRadius(int outerRadius, int inset)
{
    return qMax(0, outerRadius - inset);
}

} // namespace CosmicUiMetrics

// ---------------------------------------------------------------------------
// Shared inline-edit stylesheet
// Produces a dark rounded field with an accent-colour glow border.
// Usage:  myLineEdit->setStyleSheet(CosmicInlineEdit::stylesheet());
// ---------------------------------------------------------------------------
namespace CosmicInlineEdit {

inline QString stylesheet()
{
    const int r = CosmicUiMetrics::cornerRadius();
    const QString accent = CosmicColor::accentHex();
    return QStringLiteral(
        "QLineEdit {"
        "  background: #1f1f1f;"
        "  border: 1px solid %1;"
        "  border-radius: %2px;"
        "  padding: 0px 4px;"
        "  margin: 0px;"
        "  color: #ffffff;"
        "  selection-background-color: %1;"
        "  selection-color: #ffffff;"
        "}").arg(accent).arg(r);
}

} // namespace CosmicInlineEdit

#endif // COSMICUIMETRICS_H
