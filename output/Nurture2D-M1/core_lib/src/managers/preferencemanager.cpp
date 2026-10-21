/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "preferencemanager.h"

#include <QSettings>
#include <QDebug>

const QString SETTING_CURSOR_STYLE = "CursorStyle";
const QString SETTING_CURSOR_PRESSURE_SIZE = "CursorPressureSize";
const QString SETTING_REMEMBER_CURSOR_SIZE = "RememberCursorSize";

PreferenceManager::PreferenceManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

PreferenceManager::~PreferenceManager()
{
}

bool PreferenceManager::init()
{
    loadPrefs();
    return true;
}

Status PreferenceManager::load(Object*)
{
    return Status::OK;
}

Status PreferenceManager::save(Object*)
{
    return Status::OK;
}

void PreferenceManager::loadPrefs()
{
    QSettings settings( PENCIL2D, PENCIL2D );

    // Display
    set(SETTING::GRID,                     settings.value(SETTING_SHOW_GRID,              false).toBool());
    set(SETTING::INVISIBLE_LINES,          settings.value(SETTING_INVISIBLE_LINES,        false).toBool());
    set(SETTING::OUTLINES,                 settings.value(SETTING_OUTLINES,               false).toBool());
    set(SETTING::OVERLAY_CENTER,           settings.value(SETTING_OVERLAY_CENTER,         false).toBool());
    set(SETTING::OVERLAY_THIRDS,           settings.value(SETTING_OVERLAY_THIRDS,         false).toBool());
    set(SETTING::OVERLAY_GOLDEN,           settings.value(SETTING_OVERLAY_GOLDEN,         false).toBool());
    set(SETTING::OVERLAY_SAFE,             settings.value(SETTING_OVERLAY_SAFE,           false).toBool());
    set(SETTING::OVERLAY_PERSPECTIVE1,     settings.value(SETTING_OVERLAY_PERSPECTIVE1,   false).toBool());
    set(SETTING::OVERLAY_PERSPECTIVE2,     settings.value(SETTING_OVERLAY_PERSPECTIVE2,   false).toBool());
    set(SETTING::OVERLAY_PERSPECTIVE3,     settings.value(SETTING_OVERLAY_PERSPECTIVE3,   false).toBool());
    set(SETTING::OVERLAY_ANGLE,            settings.value(SETTING_OVERLAY_ANGLE,          15).toInt());
    set(SETTING::ACTION_SAFE,              settings.value(SETTING_ACTION_SAFE,            5).toInt());
    set(SETTING::ACTION_SAFE_ON,           settings.value(SETTING_ACTION_SAFE_ON,         true).toBool());
    set(SETTING::OVERLAY_SAFE_HELPER_TEXT_ON, settings.value(SETTING_OVERLAY_SAFE_HELPER_TEXT_ON, true).toBool());
    set(SETTING::TITLE_SAFE,               settings.value(SETTING_TITLE_SAFE,             10).toInt());
    set(SETTING::TITLE_SAFE_ON,            settings.value(SETTING_TITLE_SAFE_ON,          true).toBool());

    // Grid
    set(SETTING::GRID_SIZE_W,              settings.value(SETTING_GRID_SIZE_W,            100).toInt());
    set(SETTING::GRID_SIZE_H,              settings.value(SETTING_GRID_SIZE_H,            100).toInt());

    // General
    set(SETTING::ANTIALIAS,                settings.value(SETTING_ANTIALIAS,              true).toBool());
    set(SETTING::TOOL_CURSOR,              settings.value(SETTING_TOOL_CURSOR,            true).toBool());
    set(SETTING::CANVAS_CURSOR,            settings.value(SETTING_CANVAS_CURSOR,          true).toBool());
    set(SETTING::CURSOR_STYLE,             settings.value(SETTING_CURSOR_STYLE,           0).toInt());
    set(SETTING::ALWAYS_SHOW_CIRCLE,       false);
    set(SETTING::CURSOR_PRESSURE_SIZE,     false);
    set(SETTING::REMEMBER_CURSOR_SIZE,     false);
    set(SETTING::HIGH_RESOLUTION,          settings.value(SETTING_HIGH_RESOLUTION,        true).toBool());
    set(SETTING::SHADOW,                   false);
    set(SETTING::QUICK_SIZING,             false);
    set(SETTING::SHOW_SELECTION_INFO,      settings.value(SETTING_SHOW_SELECTION_INFO,    false).toBool());

    set(SETTING::INVERT_DRAG_ZOOM_DIRECTION,   settings.value(SETTING_INVERT_DRAG_ZOOM_DIRECTION,  false).toBool());
    set(SETTING::INVERT_SCROLL_ZOOM_DIRECTION, settings.value(SETTING_INVERT_SCROLL_ZOOM_DIRECTION,  false).toBool());
    set(SETTING::ZOOM_HORIZONTAL_DRAG,         settings.value(SETTING_ZOOM_HORIZONTAL_DRAG, true).toBool());
    set(SETTING::LOW_RES_NAVIGATION,           settings.value(SETTING_LOW_RES_NAVIGATION, false).toBool());

    set(SETTING::ROTATION_INCREMENT,       settings.value(SETTING_ROTATION_INCREMENT,     15).toInt());

    set(SETTING::WINDOW_OPACITY,           settings.value(SETTING_WINDOW_OPACITY,         0).toInt());
    set(SETTING::SHOW_STATUS_BAR,          settings.value(SETTING_SHOW_STATUS_BAR,        true).toBool());
    set(SETTING::CURVE_SMOOTHING,          settings.value(SETTING_CURVE_SMOOTHING,        20).toInt());

    set(SETTING::BACKGROUND_STYLE,         settings.value(SETTING_BACKGROUND_STYLE,       "white").toString());

    set(SETTING::CANVAS_COLOR,             settings.value(SETTING_CANVAS_COLOR,           0xFFFFFFFF).toInt());

    set(SETTING::LAYOUT_LOCK,              settings.value(SETTING_LAYOUT_LOCK,            false).toBool());
    set(SETTING::FRAME_POOL_SIZE,          settings.value(SETTING_FRAME_POOL_SIZE,        1024).toInt());
    set(SETTING::UNDO_REDO_MAX_STEPS,      settings.value(SETTING_UNDO_REDO_MAX_STEPS,    100).toInt());

    set(SETTING::FPS,                      settings.value(SETTING_FPS,                    12).toInt());
    set(SETTING::FIELD_W,                  settings.value(SETTING_FIELD_W,                1920).toInt());
    set(SETTING::FIELD_H,                  settings.value(SETTING_FIELD_H,                1080).toInt());

    const int fieldW = settings.value(SETTING_FIELD_W, 1920).toInt();
    const int fieldH = settings.value(SETTING_FIELD_H, 1080).toInt();

    int extraStyle = settings.value(SETTING_EXTRA_SPACE_STYLE, 0).toInt();
    if (extraStyle != 0 && extraStyle != 1) extraStyle = 0;
    set(SETTING::EXTRA_SPACE_STYLE, extraStyle);

    int extraScale = settings.value(SETTING_EXTRA_SPACE_SCALE_PERCENT, 100).toInt();
    extraScale = qBound(100, extraScale, 200);
    set(SETTING::EXTRA_SPACE_SCALE_PERCENT, extraScale);

    int extraW = settings.value(SETTING_EXTRA_SPACE_MANUAL_W, fieldW).toInt();
    int extraH = settings.value(SETTING_EXTRA_SPACE_MANUAL_H, fieldH).toInt();
    extraW = qBound(fieldW, extraW, fieldW * 2);
    extraH = qBound(fieldH, extraH, fieldH * 2);
    set(SETTING::EXTRA_SPACE_MANUAL_W, extraW);
    set(SETTING::EXTRA_SPACE_MANUAL_H, extraH);
    // Default timestamp mode: SFF (FPS display: seconds:frames)
    set(SETTING::TIMECODE_TEXT,            settings.value(SETTING_TIMECODE_TEXT,          3).toInt());

    // Files
    set(SETTING::AUTO_SAVE,                settings.value(SETTING_AUTO_SAVE,              false).toBool());
    set(SETTING::AUTO_SAVE_NUMBER,         settings.value(SETTING_AUTO_SAVE_NUMBER,       15).toInt());
    set(SETTING::AUTO_SAVE_INTERVAL_MINUTES, settings.value(SETTING_AUTO_SAVE_INTERVAL_MINUTES, 15).toInt());
    set(SETTING::ASK_FOR_PRESET,           settings.value(SETTING_ASK_FOR_PRESET,         false).toBool());
    set(SETTING::LOAD_MOST_RECENT,         settings.value(SETTING_LOAD_MOST_RECENT,       false).toBool());
    set(SETTING::LOAD_DEFAULT_PRESET,      settings.value(SETTING_LOAD_DEFAULT_PRESET,    true).toBool());
    set(SETTING::DEFAULT_PRESET,           settings.value(SETTING_DEFAULT_PRESET,         0).toInt());

    // Timeline
    set(SETTING::SHORT_SCRUB,              settings.value(SETTING_SHORT_SCRUB,            false ).toBool());
    set(SETTING::FRAME_SIZE,               settings.value(SETTING_FRAME_SIZE,             55).toInt());
    set(SETTING::TIMELINE_SIZE,            settings.value(SETTING_TIMELINE_SIZE,          240).toInt());
    set(SETTING::DRAW_LABEL,               settings.value(SETTING_DRAW_LABEL,             false ).toBool());
    set(SETTING::LABEL_FONT_SIZE,          settings.value(SETTING_LABEL_FONT_SIZE,        12).toInt());

    set( SETTING::DRAW_ON_EMPTY_FRAME_ACTION, settings.value( SETTING_DRAW_ON_EMPTY_FRAME_ACTION,
                                                              KEEP_DRAWING_ON_PREVIOUS_KEY).toInt() );

    // Onion Skin
    // Force disabled by default on launch (user can enable later)
    set(SETTING::PREV_ONION,               false);
    set(SETTING::NEXT_ONION,               false);
    set(SETTING::ONION_MUTLIPLE_LAYERS,    settings.value(SETTING_MULTILAYER_ONION,       false).toBool());
    set(SETTING::ONION_BLUE,               settings.value(SETTING_ONION_BLUE,             false).toBool());
    set(SETTING::ONION_RED,                settings.value(SETTING_ONION_RED,              false).toBool());

    set(SETTING::ONION_MAX_OPACITY,        settings.value(SETTING_ONION_MAX_OPACITY,      50).toInt());
    set(SETTING::ONION_MIN_OPACITY,        settings.value(SETTING_ONION_MIN_OPACITY,      20).toInt());
    set(SETTING::ONION_PREV_FRAMES_NUM,    settings.value(SETTING_ONION_PREV_FRAMES_NUM,  5).toInt());
    set(SETTING::ONION_NEXT_FRAMES_NUM,    settings.value(SETTING_ONION_NEXT_FRAMES_NUM,  5).toInt());
    set(SETTING::ONION_WHILE_PLAYBACK,     settings.value(SETTING_ONION_WHILE_PLAYBACK,   0).toInt());
    set(SETTING::ONION_TYPE,               settings.value(SETTING_ONION_TYPE,             "relative").toString());
    set(SETTING::LAYER_VISIBILITY,         settings.value(SETTING_LAYER_VISIBILITY,       2).toInt());
    set(SETTING::LAYER_VISIBILITY_THRESHOLD, settings.value(SETTING_LAYER_VISIBILITY_THRESHOLD, 0.5f).toFloat());
    set(SETTING::DIM_INACTIVE_LAYERS,      settings.value(SETTING_DIM_INACTIVE_LAYERS,    true).toBool());

    set(SETTING::CELL_BANK_VISIBLE,        settings.value(SETTING_CELL_BANK_VISIBLE,      false).toBool());

    set(SETTING::SHIFT_TRACE_WINDOW_VISIBLE, settings.value(SETTING_SHIFT_TRACE_WINDOW_VISIBLE, false).toBool());

    set(SETTING::FLIP_ROLL_MSEC,           settings.value(SETTING_FLIP_ROLL_MSEC,         100).toInt());
    set(SETTING::FLIP_ROLL_DRAWINGS,       settings.value(SETTING_FLIP_ROLL_DRAWINGS,     5).toInt());
    set(SETTING::FLIP_INBETWEEN_MSEC,      settings.value(SETTING_FLIP_INBETWEEN_MSEC,    100).toInt());
    set(SETTING::SOUND_SCRUB_ACTIVE,       settings.value(SETTING_SOUND_SCRUB_ACTIVE,     false).toBool());
    set(SETTING::SOUND_SCRUB_MSEC,         settings.value(SETTING_SOUND_SCRUB_MSEC,       100).toInt());

    set(SETTING::LANGUAGE,                 settings.value(SETTING_LANGUAGE).toString());

    // Tools
    set(SETTING::TEMP_TOOL_HOLD_MSEC,      settings.value(SETTING_TEMP_TOOL_HOLD_MSEC, 375.0f).toFloat());

    // Pencil performance defaults
    set(SETTING::PENCIL_MAX_DAB_CACHE_SIZE, settings.value(SETTING_PENCIL_MAX_DAB_CACHE_SIZE, 512).toInt());
    set(SETTING::PENCIL_MAX_TEXTURE_CACHE_SIZE, settings.value(SETTING_PENCIL_MAX_TEXTURE_CACHE_SIZE, 64).toInt());
    set(SETTING::PENCIL_MAX_TEXTURE_RESOLUTION, settings.value(SETTING_PENCIL_MAX_TEXTURE_RESOLUTION, 256).toInt());
    set(SETTING::PENCIL_PERFORMANCE_LOGGING, settings.value(SETTING_PENCIL_PERFORMANCE_LOGGING, false).toBool());

    // Gap Checking: default off, cyan background
    set(SETTING::GAP_CHECK_MODE,           settings.value(SETTING_GAP_CHECK_MODE,        false).toBool());
    // Default cyan color (ARGB: 0xFF00FFFF)
    set(SETTING::GAP_CHECK_BG_COLOR,       settings.value(SETTING_GAP_CHECK_BG_COLOR,    0xFF00FFFF).toInt());
    // Default medium gray for drawing (ARGB: 0xFFD3D3D3)
    set(SETTING::GAP_CHECK_DRAW_COLOR,     settings.value(SETTING_GAP_CHECK_DRAW_COLOR,  0xFFD3D3D3).toInt());

    // Color preferences: cosmic accent and derived selection colors
    const int kNewAccent = 0xFF505050;      // #505050 with full alpha
    const int kOldAccentBlue = 0xFF4B8AFF;  // legacy blue
    const int kOldAccentCosmic = 0xFF40555C; // previous cosmic

    int storedAccent = settings.value(SETTING_ACCENT_COLOR, kNewAccent).toInt();
    if (storedAccent == kOldAccentBlue || storedAccent == kOldAccentCosmic) {
        storedAccent = kNewAccent;
        settings.setValue(SETTING_ACCENT_COLOR, storedAccent);
    }
    set(SETTING::ACCENT_COLOR, storedAccent);

    int storedToolbox = settings.value(SETTING_TOOLBOX_SELECTION_COLOR, storedAccent).toInt();
    if (storedToolbox == kOldAccentBlue || storedToolbox == kOldAccentCosmic) {
        storedToolbox = storedAccent;
        settings.setValue(SETTING_TOOLBOX_SELECTION_COLOR, storedToolbox);
    }
    set(SETTING::TOOLBOX_SELECTION_COLOR, storedToolbox);

    int storedToolPalette = settings.value(SETTING_TOOL_PALETTE_SELECTION_COLOR, storedAccent).toInt();
    if (storedToolPalette == kOldAccentBlue || storedToolPalette == kOldAccentCosmic) {
        storedToolPalette = storedAccent;
        settings.setValue(SETTING_TOOL_PALETTE_SELECTION_COLOR, storedToolPalette);
    }
    set(SETTING::TOOL_PALETTE_SELECTION_COLOR, storedToolPalette);

    // Cosmic theming defaults
    set(SETTING::COSMIC_ROUNDNESS,
        settings.value(SETTING_COSMIC_ROUNDNESS, 6).toInt());          // more pronounced radius
    set(SETTING::COSMIC_BORDER_WIDTH,
        settings.value(SETTING_COSMIC_BORDER_WIDTH, 1).toInt());      // thin border
    set(SETTING::COSMIC_BORDER_COLOR,
        settings.value(SETTING_COSMIC_BORDER_COLOR, 0xFF1F1F1F).toInt()); // #1f1f1f

    // UI Theme colors: #2b2b2b, #353535, #606060
    set(SETTING::UI_TITLE_BAR_COLOR,       settings.value(SETTING_UI_TITLE_BAR_COLOR,    0xFF2B2B2B).toInt());
    set(SETTING::UI_DOCK_CONTENT_COLOR,    settings.value(SETTING_UI_DOCK_CONTENT_COLOR, 0xFF353535).toInt());
    set(SETTING::UI_SCROLLBAR_COLOR,       settings.value(SETTING_UI_SCROLLBAR_COLOR,    0xFF606060).toInt());

    {
        int mode = settings.value(SETTING_PLAYBACK_PERFORMANCE_MODE, 0).toInt();
        if (mode != 0 && mode != 1) {
            mode = 0;
        }
        set(SETTING::PLAYBACK_PERFORMANCE_MODE, mode);
    }

    // Always enable canvas cursor regardless of stored value
    set(SETTING::CANVAS_CURSOR, true);

    // Windows & Scrollbars
    set(SETTING::WINDOW_CLOSE_STYLE,   settings.value("WindowCloseStyle",   0).toInt());
    set(SETTING::WINDOW_CLOSE_ON_LEFT, settings.value("WindowCloseOnLeft",  true).toBool());
    // Timeline scrollbars (0=Bottom/Left, 1=Top/Right, 2=Both)
    set(SETTING::TIMELINE_H_SCROLLBAR_MODE, settings.value("TimelineHScrollbarMode", 0).toInt());
    set(SETTING::TIMELINE_V_SCROLLBAR_MODE, settings.value("TimelineVScrollbarMode", 2).toInt());
}

void PreferenceManager::turnOn(SETTING option)
{
    set(option, true);
}

void PreferenceManager::turnOff(SETTING option)
{
    set(option, false);
}

bool PreferenceManager::isOn(SETTING option)
{
    int optionId = static_cast<int>(option);
    return mBooleanSet.value(optionId, false);
}

int PreferenceManager::getInt(SETTING option)
{
    int optionId = static_cast<int>(option);
    return mIntegerSet.value(optionId, -1);
}

float PreferenceManager::getFloat(SETTING option)
{
    int optionId = static_cast<int>(option);
    return mFloatingPointSet.value(optionId, -1);
}

QString PreferenceManager::getString(SETTING option)
{
    int optionId = static_cast<int>(option);
    if (mIntegerSet.contains(optionId))
    {
        return QString::number(mIntegerSet.value(optionId, -1));
    }
    else if (mBooleanSet.contains(optionId))
    {
        if (mBooleanSet.value(optionId, false))
        {
            return "true";
        }
        else
        {
            return "false";
        }
    }
    return mStringSet.value(optionId);
}

void PreferenceManager::set(SETTING option, QString value)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    switch (option)
    {
    case SETTING::BACKGROUND_STYLE:
        settings.setValue(SETTING_BACKGROUND_STYLE, value);
        break;
    case SETTING::ONION_TYPE:
        settings.setValue(SETTING_ONION_TYPE, value);
        break;
    case SETTING::LANGUAGE:
        settings.setValue(SETTING_LANGUAGE, value);
        break;
    default:
        break;
    }

    int optionId = static_cast<int>(option);
    if (mStringSet[optionId] != value)
    {
        mStringSet[optionId] = value;
        emit optionChanged(option);
    }
}

void PreferenceManager::set(SETTING option, float value)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    switch(option)
    {
    case SETTING::LAYER_VISIBILITY_THRESHOLD:
        settings.setValue(SETTING_LAYER_VISIBILITY_THRESHOLD, value);
        break;
    case SETTING::TEMP_TOOL_HOLD_MSEC:
        settings.setValue(SETTING_TEMP_TOOL_HOLD_MSEC, value);
        break;
    default:
        // allow storing other float settings without assertion
        break;
    }


    int optionId = static_cast<int>(option);

    if (qFuzzyCompare(mFloatingPointSet[optionId], value) == false)
    {
        mFloatingPointSet[optionId] = value;
        emit optionChanged(option);
    }
}

void PreferenceManager::set(SETTING option, int value)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    switch (option)
    {
    case SETTING::OVERLAY_ANGLE:
        settings.setValue(SETTING_OVERLAY_ANGLE, value);
        break;
    case SETTING::ACTION_SAFE:
        settings.setValue(SETTING_ACTION_SAFE, value);
        break;
    case SETTING::TITLE_SAFE:
        settings.setValue(SETTING_TITLE_SAFE, value);
        break;
    case SETTING::CURVE_SMOOTHING:
        settings.setValue(SETTING_CURVE_SMOOTHING, value);
        break;
    case SETTING::WINDOW_OPACITY:
        settings.setValue(SETTING_WINDOW_OPACITY, value);
        break;
    case SETTING::FRAME_POOL_SIZE:
        settings.setValue(SETTING_FRAME_POOL_SIZE, value);
        break;
    case SETTING::UNDO_REDO_MAX_STEPS:
        settings.setValue(SETTING_UNDO_REDO_MAX_STEPS, value);
        break;
    case SETTING::FRAME_SIZE:
        settings.setValue(SETTING_FRAME_SIZE, value);
        break;
    case SETTING::GAP_CHECK_BG_COLOR:
        settings.setValue(SETTING_GAP_CHECK_BG_COLOR, value);
        break;
    case SETTING::GAP_CHECK_DRAW_COLOR:
        settings.setValue(SETTING_GAP_CHECK_DRAW_COLOR, value);
        break;
    case SETTING::ACCENT_COLOR:
        settings.setValue(SETTING_ACCENT_COLOR, value);
        break;
    case SETTING::TOOLBOX_SELECTION_COLOR:
        settings.setValue(SETTING_TOOLBOX_SELECTION_COLOR, value);
        break;
    case SETTING::TOOL_PALETTE_SELECTION_COLOR:
        settings.setValue(SETTING_TOOL_PALETTE_SELECTION_COLOR, value);
        break;
    case SETTING::UI_TITLE_BAR_COLOR:
        settings.setValue(SETTING_UI_TITLE_BAR_COLOR, value);
        break;
    case SETTING::UI_DOCK_CONTENT_COLOR:
        settings.setValue(SETTING_UI_DOCK_CONTENT_COLOR, value);
        break;
    case SETTING::UI_SCROLLBAR_COLOR:
        settings.setValue(SETTING_UI_SCROLLBAR_COLOR, value);
        break;
    case SETTING::PLAYBACK_PERFORMANCE_MODE:
        settings.setValue(SETTING_PLAYBACK_PERFORMANCE_MODE, value);
        break;
    case SETTING::COSMIC_ROUNDNESS:
        settings.setValue(SETTING_COSMIC_ROUNDNESS, value);
        break;
    case SETTING::COSMIC_BORDER_WIDTH:
        settings.setValue(SETTING_COSMIC_BORDER_WIDTH, value);
        break;
    case SETTING::COSMIC_BORDER_COLOR:
        settings.setValue(SETTING_COSMIC_BORDER_COLOR, value);
        break;
    case SETTING::CURSOR_STYLE:
        settings.setValue(SETTING_CURSOR_STYLE, value);
        break;
    case SETTING::FIELD_W:
        settings.setValue(SETTING_FIELD_W, value);
        break;
    case SETTING::FIELD_H:
        settings.setValue(SETTING_FIELD_H, value);
        break;
    case SETTING::CANVAS_COLOR:
        settings.setValue(SETTING_CANVAS_COLOR, value);
        break;
    case SETTING::EXTRA_SPACE_STYLE:
        settings.setValue(SETTING_EXTRA_SPACE_STYLE, value);
        break;
    case SETTING::EXTRA_SPACE_SCALE_PERCENT:
        settings.setValue(SETTING_EXTRA_SPACE_SCALE_PERCENT, value);
        break;
    case SETTING::EXTRA_SPACE_MANUAL_W:
        settings.setValue(SETTING_EXTRA_SPACE_MANUAL_W, value);
        break;
    case SETTING::EXTRA_SPACE_MANUAL_H:
        settings.setValue(SETTING_EXTRA_SPACE_MANUAL_H, value);
        break;
    case SETTING::WINDOW_CLOSE_STYLE:
        settings.setValue("WindowCloseStyle", value);
        break;
    case SETTING::TIMELINE_H_SCROLLBAR_MODE:
        settings.setValue("TimelineHScrollbarMode", value);
        break;
    case SETTING::TIMELINE_V_SCROLLBAR_MODE:
        settings.setValue("TimelineVScrollbarMode", value);
        break;
    case SETTING::TIMECODE_TEXT:
        settings.setValue(SETTING_TIMECODE_TEXT, value);
        break;
    case SETTING::AUTO_SAVE_INTERVAL_MINUTES:
        settings.setValue(SETTING_AUTO_SAVE_INTERVAL_MINUTES, value);
        break;
    default:
        break;
    }

    int optionId = static_cast<int>(option);
    if (mIntegerSet[optionId] != value)
    {
        mIntegerSet[optionId] = value;
        emit optionChanged(option);
    }
}

// Set bool value
//
void PreferenceManager::set(SETTING option, bool value)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    switch (option)
    {
    case SETTING::GRID:
        settings.setValue(SETTING_SHOW_GRID, value);
        break;
    case SETTING::INVISIBLE_LINES:
        settings.setValue(SETTING_INVISIBLE_LINES, value);
        break;
    case SETTING::OUTLINES:
        settings.setValue(SETTING_OUTLINES, value);
        break;
    case SETTING::OVERLAY_CENTER:
        settings.setValue(SETTING_OVERLAY_CENTER, value);
        break;
    case SETTING::OVERLAY_THIRDS:
        settings.setValue(SETTING_OVERLAY_THIRDS, value);
        break;
    case SETTING::OVERLAY_GOLDEN:
        settings.setValue(SETTING_OVERLAY_GOLDEN, value);
        break;
    case SETTING::OVERLAY_SAFE:
        settings.setValue(SETTING_OVERLAY_SAFE, value);
        break;
    case SETTING::OVERLAY_PERSPECTIVE1:
        settings.setValue(SETTING_OVERLAY_PERSPECTIVE1, value);
        break;
    case SETTING::OVERLAY_PERSPECTIVE2:
        settings.setValue(SETTING_OVERLAY_PERSPECTIVE2, value);
        break;
    case SETTING::OVERLAY_PERSPECTIVE3:
        settings.setValue(SETTING_OVERLAY_PERSPECTIVE3, value);
        break;
    case SETTING::ACTION_SAFE_ON:
        settings.setValue(SETTING_ACTION_SAFE_ON, value);
        break;
    case SETTING::OVERLAY_SAFE_HELPER_TEXT_ON:
        settings.setValue(SETTING_OVERLAY_SAFE_HELPER_TEXT_ON, value);
        break;
    case SETTING::TITLE_SAFE_ON:
        settings.setValue(SETTING_TITLE_SAFE_ON, value);
        break;
    case SETTING::ANTIALIAS:
        settings.setValue(SETTING_ANTIALIAS, value);
        break;
    case SETTING::TOOL_CURSOR:
        settings.setValue(SETTING_TOOL_CURSOR, value);
        break;
    case SETTING::CANVAS_CURSOR:
        settings.setValue(SETTING_CANVAS_CURSOR, value);
        break;
    case SETTING::ALWAYS_SHOW_CIRCLE:
        settings.setValue(SETTING_ALWAYS_SHOW_CIRCLE, value);
        break;
    case SETTING::CURSOR_PRESSURE_SIZE:
        settings.setValue(SETTING_CURSOR_PRESSURE_SIZE, value);
        break;
    case SETTING::REMEMBER_CURSOR_SIZE:
        settings.setValue(SETTING_REMEMBER_CURSOR_SIZE, value);
        break;
    case SETTING::HIGH_RESOLUTION:
        settings.setValue(SETTING_HIGH_RESOLUTION, value);
        break;
    case SETTING::SHADOW:
        settings.setValue(SETTING_SHADOW, value);
        break;
    case SETTING::QUICK_SIZING:
        settings.setValue(SETTING_QUICK_SIZING, value);
        break;
    case SETTING::SHOW_SELECTION_INFO:
        settings.setValue(SETTING_SHOW_SELECTION_INFO, value);
        break;
    case SETTING::INVERT_DRAG_ZOOM_DIRECTION:
        settings.setValue(SETTING_INVERT_DRAG_ZOOM_DIRECTION, value);
        break;
    case SETTING::INVERT_SCROLL_ZOOM_DIRECTION:
        settings.setValue(SETTING_INVERT_SCROLL_ZOOM_DIRECTION, value);
        break;
    case SETTING::ZOOM_HORIZONTAL_DRAG:
        settings.setValue(SETTING_ZOOM_HORIZONTAL_DRAG, value);
        break;
    case SETTING::LOW_RES_NAVIGATION:
        settings.setValue(SETTING_LOW_RES_NAVIGATION, value);
        break;
    case SETTING::DIM_INACTIVE_LAYERS:
        settings.setValue(SETTING_DIM_INACTIVE_LAYERS, value);
        break;
    case SETTING::CELL_BANK_VISIBLE:
        settings.setValue(SETTING_CELL_BANK_VISIBLE, value);
        break;
    case SETTING::SHIFT_TRACE_WINDOW_VISIBLE:
        settings.setValue(SETTING_SHIFT_TRACE_WINDOW_VISIBLE, value);
        break;
    case SETTING::SHOW_STATUS_BAR:
        settings.setValue(SETTING_SHOW_STATUS_BAR, value);
        break;
    case SETTING::LAYOUT_LOCK:
        settings.setValue(SETTING_LAYOUT_LOCK, value);
        break;
    
    case SETTING::GAP_CHECK_MODE:
        settings.setValue(SETTING_GAP_CHECK_MODE, value);
        break;
    case SETTING::WINDOW_CLOSE_ON_LEFT:
        settings.setValue("WindowCloseOnLeft", value);
        break;
    default:
        break;
    }

    int optionId = static_cast<int>(option);
    if (mBooleanSet[optionId] != value)
    {
        mBooleanSet[optionId] = value;
        emit optionChanged(option);
    }
}
