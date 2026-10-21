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

#ifndef PENCILDEF_H
#define PENCILDEF_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define S__GIT_TIMESTAMP TOSTRING(GIT_TIMESTAMP)
#define S__GIT_COMMIT_HASH TOSTRING(GIT_CURRENT_SHA1)

enum ToolType : int
{
    INVALID_TOOL = -1,
    ERASER,
    SELECT,
    MOVE,
    HAND,
    CAMERA,
    PEN,
    PENCIL,
    POLYLINE,
    CURVE,
    BUCKET,
    EYEDROPPER,
    BSPLINE,
    // New tools
    RECT_GAP_FILL,
    LASSO_GAP_FILL,
    LASSO_SELECT,
    DPLATE,
    TOOL_TYPE_COUNT
};

enum ToolPropertyType
{
    WIDTH,
    FEATHER,
    PRESSURE,
    INVISIBILITY,
    PRESERVEALPHA,
    BEZIER,
    CLOSEDPATH,
    USEFEATHER,
    VECTORMERGE,
    ANTI_ALIASING,
    FILL_MODE,
    STABILIZATION,
    // Pen post-stroke correction level (Off/Low/Med/High)
    POST_CORRECTION_LEVEL,
    TOLERANCE,
    FILLCONTOUR,
    SHOWSELECTIONINFO,
    USETOLERANCE,
    BUCKETFILLEXPAND,            // deprecated; no-op
    USEBUCKETFILLEXPAND,         // deprecated; no-op
    BUCKETFILLLAYERREFERENCEMODE,
    CAMERAPATH,
    DRAW_BEHIND,
    // Krita-like smoothing controls (Pen tool)
    PEN_SMOOTHING_MODE,
    // Weighted
    WEIGHTED_DISTANCE,
    WEIGHTED_STROKE_ENDING,
    WEIGHTED_SMOOTH_PRESSURE,
    WEIGHTED_SCALABLE_DISTANCE,
    // Stabilizer
    STAB2_DISTANCE,
    STAB2_DELAY,
    STAB2_FINISH_LINE,
    STAB2_STABILIZE_SENSORS,
    STAB2_SCALABLE_DISTANCE,
    // Pressure pipeline (always-on pressure smoothing and tapering)
    PRESSURE_SMOOTHING_AMOUNT,
    PRESSURE_TAPERING_START,
    PRESSURE_TAPERING_END,
    CURVE_TAPER_START,
    CURVE_TAPER_END,
    HYBRID_SNAP_LEVEL,
    BUCKET_GAP_FILL_MODE,
    EYEDROPPER_REFERENCE_MODE,
};

// Eyedropper reference options
enum class EyedropperReferenceMode
{
    FROM_VIEW = 0,
    CURRENT_LAYER = 1,
};

// Pen smoothing modes modeled after Krita
enum class PenSmoothingMode
{
    LEGACY = 0, // use existing NONE/SIMPLE/STRONG levels
    WEIGHTED = 1,
    PULL = 2,
};

enum class DotColorType {
    RED,
    BLUE,
    GREEN,
    BLACK,
    WHITE
};

enum BackgroundStyle
{

};

enum StabilizationLevel
{
    NONE,
    SIMPLE,
    STRONG
};

enum TimecodeTextLevel
{
    NOTEXT,
    FRAMES, // FF
    SMPTE,  // HH:MM:SS:FF
    SFF     // S:FF
};

enum class LayerVisibility
{
    CURRENTONLY = 0,
    RELATED = 1,
    ALL = 2,
    // If you are adding new enum values here, be sure to update the ++/-- operators below
};

inline LayerVisibility& operator++(LayerVisibility& vis)
{
    return vis = (vis == LayerVisibility::ALL) ? LayerVisibility::CURRENTONLY : static_cast<LayerVisibility>(static_cast<int>(vis)+1);
}

inline LayerVisibility& operator--(LayerVisibility& vis)
{
    return vis = (vis == LayerVisibility::CURRENTONLY) ? LayerVisibility::ALL : static_cast<LayerVisibility>(static_cast<int>(vis)-1);
}

// Max frames that can be imported and loaded onto the timeline
const static int MaxFramesBound = 9999;

// shortcuts command code
#define CMD_NEW_FILE  "CmdNewFile"
#define CMD_OPEN_FILE "CmdOpenFile"
#define CMD_SAVE_FILE "CmdSaveFile"
#define CMD_SAVE_AS "CmdSaveAs"
#define CMD_IMPORT_IMAGE "CmdImportImage"
#define CMD_IMPORT_IMAGE_SEQ "CmdImportImageSequence"
#define CMD_IMPORT_IMAGE_PREDEFINED_SET "CmdImportImagePredefinedSet"
#define CMD_IMPORT_MOVIE_VIDEO "CmdImportMovieVideo"
#define CMD_IMPORT_ANIMATED_IMAGE "CmdImportAnimatedImage"
#define CMD_IMPORT_LAYERS "CmdImportLayers"
#define CMD_IMPORT_SOUND "CmdImportSound"
#define CMD_IMPORT_MOVIE_AUDIO "CmdImportMovieAudio"
#define CMD_IMPORT_PALETTE "CmdImportPalette"
#define CMD_IMPORT_PALETTE_REPLACE "CmdImportPaletteReplace"
#define CMD_EXPORT_IMAGE_SEQ "CmdExportImageSequence"
#define CMD_EXPORT_IMAGE "CmdExportImage"
#define CMD_EXPORT_MOVIE "CmdExportMovie"
#define CMD_EXPORT_GIF "CmdExportGIF"
#define CMD_EXPORT_SPRITE "CmdExportSprite"
#define CMD_EXPORT_PALETTE "CmdExportPalette"
// Add Export Animation command id
#define CMD_EXPORT_ANIMATION "CmdExportAnimation"
#define CMD_UNDO "CmdUndo"
#define CMD_REDO "CmdRedo"
#define CMD_CUT "CmdCut"
#define CMD_COPY "CmdCopy"
#define CMD_PASTE "CmdPaste"
#define CMD_PASTE_FROM_PREVIOUS "CmdPasteFromPrevious"
#define CMD_SELECT_ALL "CmdSelectAll"
#define CMD_DESELECT_ALL "CmdDeselectAll"
#define CMD_CLEAR_FRAME "CmdClearFrame"
#define CMD_PEGBAR_ALIGNMENT "CmdPegBarAlignment"
#define CMD_PREFERENCE "CmdPreferences"
#define CMD_RESET_WINDOWS "CmdResetWindows"
#define CMD_LOCK_WINDOWS "CmdLockWindows"
#define CMD_ZOOM_IN "CmdZoomIn"
#define CMD_ZOOM_OUT "CmdZoomOut"
#define CMD_ROTATE_CLOCK "CmdRotateClockwise"
#define CMD_ROTATE_ANTI_CLOCK "CmdRotateAntiClockwise"
#define CMD_RESET_ROTATION "CmdResetRotation"
#define CMD_RESET_ZOOM_ROTATE "CmdResetZoomRotate"
#define CMD_CENTER_VIEW "CmdCenterView"
#define CMD_ZOOM_400 "CmdZoom400"
#define CMD_ZOOM_300 "CmdZoom300"
#define CMD_ZOOM_200 "CmdZoom200"
#define CMD_ZOOM_100 "CmdZoom100"
#define CMD_ZOOM_50 "CmdZoom50"
#define CMD_ZOOM_33 "CmdZoom33"
#define CMD_ZOOM_25 "CmdZoom25"
#define CMD_FLIP_HORIZONTAL "CmdFlipHorizontal"
#define CMD_FLIP_VERTICAL "CmdFlipVertical"
#define CMD_GRID "CmdGrid"
#define CMD_OVERLAY_CENTER "CmdOverlayCenter"
#define CMD_OVERLAY_THIRDS "CmdOverlayThirds"
#define CMD_OVERLAY_GOLDEN_RATIO "CmdOverlayGoldenRatio"
#define CMD_OVERLAY_SAFE_AREAS "CmdOverlaySafeAreas"
#define CMD_OVERLAY_ONE_POINT_PERSPECTIVE "CmdOverlayOnePointPerspective"
#define CMD_OVERLAY_TWO_POINT_PERSPECTIVE "CmdOverlayTwoPointPerspective"
#define CMD_OVERLAY_THREE_POINT_PERSPECTIVE "CmdOverlayThreePointPerspective"
#define CMD_ONIONSKIN_PREV "CmdOnionSkinPrevious"
#define CMD_ONIONSKIN_NEXT "CmdOnionSkinNext"
#define CMD_TOGGLE_STATUS_BAR "CmdToggleStatusBar"
#define CMD_PLAY "CmdPlay"
#define CMD_LOOP "CmdLoop"
#define CMD_LOOP_CONTROL "CmdLoopControl"
#define CMD_FLIP_INBETWEEN "CmdFlipInBetween"
#define CMD_FLIP_ROLLING "CmdFlipRolling"
#define CMD_GOTO_NEXT_FRAME "CmdGotoNextFrame"
#define CMD_GOTO_PREV_FRAME "CmdGotoPreviousFrame"
#define CMD_GOTO_NEXT_KEY_FRAME "CmdGotoNextKeyFrame"
#define CMD_GOTO_PREV_KEY_FRAME "CmdGotoPreviousKeyFrame"
#define CMD_ADD_FRAME "CmdAddFrame"
#define CMD_DUPLICATE_FRAME "CmdDuplicateFrame"
#define CMD_REMOVE_FRAME "CmdRemoveFrame"
#define CMD_REVERSE_SELECTED_FRAMES "CmdReverseSelectedFrames"
#define CMD_REMOVE_SELECTED_FRAMES "CmdRemoveSelectedFrames"
#define CMD_SELECTION_REPOSITION_FRAMES "CmdSelectionRepositionFrames"
#define CMD_SELECTION_ADD_FRAME_EXPOSURE "CmdSelectionAddFrameExposure"
#define CMD_SELECTION_SUBTRACT_FRAME_EXPOSURE "CmdSelectionSubtractFrameExposure"
#define CMD_SELECTION_FLIP_HORIZONTAL "CmdSelectionFlipHorizontal"
#define CMD_SELECTION_FLIP_VERTICAL "CmdSelectionFlipVertical"
#define CMD_SELECTION_FREE_TRANSFORM "CmdSelectionFreeTransform"
#define CMD_MOVE_FRAME_BACKWARD "CmdMoveFrameBackward"
#define CMD_MOVE_FRAME_FORWARD "CmdMoveFrameForward"

// Timeline: retime selected frames to 1s/2s/3s spacing (no ripple). Defaults: none.
#define CMD_RETIME_1S "CmdRetime1s"
#define CMD_RETIME_2S "CmdRetime2s"
#define CMD_RETIME_3S "CmdRetime3s"
// Timeline: retime selected frames + ripple trailing frames. Defaults: none.
#define CMD_RETIME_1S_RIPPLE "CmdRetime1sRipple"
#define CMD_RETIME_2S_RIPPLE "CmdRetime2sRipple"
#define CMD_RETIME_3S_RIPPLE "CmdRetime3sRipple"
// Timeline: make current drawing unique (break shared exposure). Default: none.
#define CMD_MAKE_UNIQUE_DRAWING "CmdMakeUniqueDrawing"
// Timeline / Papers pane: select the layer above or below the active one. Defaults: none.
#define CMD_SELECT_LAYER_ABOVE "CmdSelectLayerAbove"
#define CMD_SELECT_LAYER_BELOW "CmdSelectLayerBelow"

// Papers pane: add / remove / import inner layers. Defaults: none.
#define CMD_PAPERS_NEW    "CmdPapersNew"
#define CMD_PAPERS_REMOVE "CmdPapersRemove"
#define CMD_PAPERS_IMPORT "CmdPapersImport"
// Papers pane: navigate active inner layer up / down. Defaults: none.
#define CMD_PAPERS_SELECT_ABOVE "CmdPapersSelectAbove"
#define CMD_PAPERS_SELECT_BELOW "CmdPapersSelectBelow"
#define CMD_TOOL_MOVE "CmdToolMove"
#define CMD_TOOL_SELECT "CmdToolSelect"
#define CMD_TOOL_BRUSH "CmdToolBrush"
#define CMD_TOOL_POLYLINE "CmdToolPolyline"
#define CMD_TOOL_CURVE "CmdToolCurve"
#define CMD_TOOL_BSPLINE "CmdToolBSpline"
#define CMD_TOOL_SMUDGE "CmdToolSmudge"
#define CMD_TOOL_PEN "CmdToolPen"
#define CMD_TOOL_PENCIL "CmdToolPencil"
#define CMD_TOOL_HAND "CmdToolHand"
#define CMD_TOOL_BUCKET "CmdToolBucket"
#define CMD_TOOL_EYEDROPPER "CmdToolEyedropper"
#define CMD_TOOL_ERASER "CmdToolEraser"
// Lasso Select tool command (default: none)
#define CMD_TOOL_LASSO_SELECT "CmdToolLassoSelect"
// New gap fill tool commands
#define CMD_TOOL_RECT_GAP_FILL "CmdToolRectGapFill"
#define CMD_TOOL_LASSO_GAP_FILL "CmdToolLassoGapFill"
// New Dplate tool command
#define CMD_TOOL_DPLATE "CmdToolDplate"
#define CMD_RESET_ALL_TOOLS "CmdResetAllTools"
#define CMD_NEW_BITMAP_LAYER "CmdNewBitmapLayer"
#define CMD_NEW_VECTOR_LAYER "CmdNewVectorLayer"
#define CMD_NEW_SOUND_LAYER "CmdNewSoundLayer"
#define CMD_NEW_CAMERA_LAYER "CmdNewCameraLayer"
#define CMD_DELETE_CUR_LAYER "CmdDeleteCurrentLayer"
#define CMD_CHANGE_LINE_COLOR_KEYFRAME "CmdChangeLineColorKeyframe"
#define CMD_CHANGE_LINE_COLOR_LAYER "CmdChangeLineColorLayer"
#define CMD_CHANGE_LAYER_OPACITY "CmdChangeLayerOpacity"
#define CMD_CURRENT_LAYER_VISIBILITY "CmdLayerVisibilityCurrentOnly"
#define CMD_RELATIVE_LAYER_VISIBILITY "CmdLayerVisibilityRelative"
#define CMD_ALL_LAYER_VISIBILITY "CmdLayerVisibilityAll"
#define CMD_HELP "CmdHelp"
#define CMD_TOGGLE_TOOLBOX "CmdToggleToolBox"
#define CMD_TOGGLE_TOOL_OPTIONS "CmdToggleToolOptions"
#define CMD_TOGGLE_COLOR_WHEEL "CmdToggleColorWheel"
#define CMD_TOGGLE_COLOR_INSPECTOR "CmdToggleColorInspector"
#define CMD_TOGGLE_COLOR_LIBRARY "CmdToggleColorLibrary"
#define CMD_POPUP_COLOR_WHEEL "CmdPopupColorWheel"
#define CMD_TOGGLE_ONION_SKIN "CmdToggleOnionSkin"
#define CMD_TOGGLE_TIMELINE "CmdToggleTimeline"
// Add toggle commands for Reference Window, Shift & Trace dock, and Cell Stack dock
#define CMD_TOGGLE_REFERENCE_WINDOW "CmdToggleReferenceWindow"
#define CMD_TOGGLE_SHIFT_TRACE "CmdToggleShiftTrace"
#define CMD_TOGGLE_CELL_STACK "CmdToggleCellStack"
// Toggle all dock widgets visibility
#define CMD_TOGGLE_DOCKS "CmdToggleDocks"
// Toggle Color Gap Checking mode
#define CMD_TOGGLE_GAP_CHECK "CmdToggleGapCheck"

// Cosmic side panels (left/right toolpanels + per-pane toggles). Defaults: none.
#define CMD_COSMIC_LEFT_LOCK_TOGGLE "CmdCosmicLeftLockToggle"
#define CMD_COSMIC_RIGHT_LOCK_TOGGLE "CmdCosmicRightLockToggle"

// Cosmic bottom timeline panel (CosmicBottomToolsBar) lock toggle. Defaults: none.
#define CMD_COSMIC_BOTTOM_TIMELINE_LOCK_TOGGLE "CmdCosmicBottomTimelineLockToggle"

#define CMD_COSMIC_PANE_TOGGLE_TOOLS "CmdCosmicPaneToggleTools"
#define CMD_COSMIC_PANE_TOGGLE_TOOLS_OPTIONS "CmdCosmicPaneToggleToolsOptions"
#define CMD_COSMIC_PANE_TOGGLE_BRUSHES "CmdCosmicPaneToggleBrushes"

#define CMD_COSMIC_PANE_TOGGLE_COLOR_WHEEL "CmdCosmicPaneToggleColorWheel"
#define CMD_COSMIC_PANE_TOGGLE_PALETTES "CmdCosmicPaneTogglePalettes"
#define CMD_COSMIC_PANE_TOGGLE_REFERENCE "CmdCosmicPaneToggleReference"

// Cosmic ControlCenter shortcuts (top bar groups). Defaults: none.
// Pen palette: 6 buttons.
#define CMD_CONTROL_CENTER_PEN_PALETTE_1 "CmdControlCenterPenPalette1"
#define CMD_CONTROL_CENTER_PEN_PALETTE_2 "CmdControlCenterPenPalette2"
#define CMD_CONTROL_CENTER_PEN_PALETTE_3 "CmdControlCenterPenPalette3"
#define CMD_CONTROL_CENTER_PEN_PALETTE_4 "CmdControlCenterPenPalette4"
#define CMD_CONTROL_CENTER_PEN_PALETTE_5 "CmdControlCenterPenPalette5"
#define CMD_CONTROL_CENTER_PEN_PALETTE_6 "CmdControlCenterPenPalette6"

// Auto-fill: 1 master + 5 individual color toggles.
#define CMD_CONTROL_CENTER_AUTOFILL_MASTER "CmdControlCenterAutoFillMaster"
#define CMD_CONTROL_CENTER_AUTOFILL_COLOR_1 "CmdControlCenterAutoFillColor1"
#define CMD_CONTROL_CENTER_AUTOFILL_COLOR_2 "CmdControlCenterAutoFillColor2"
#define CMD_CONTROL_CENTER_AUTOFILL_COLOR_3 "CmdControlCenterAutoFillColor3"
#define CMD_CONTROL_CENTER_AUTOFILL_COLOR_4 "CmdControlCenterAutoFillColor4"
#define CMD_CONTROL_CENTER_AUTOFILL_COLOR_5 "CmdControlCenterAutoFillColor5"

// Toolbars menu: toggle visibility of each group in the CosmicControlCenter bar.
#define CMD_CC_TOGGLE_FILE_OPS    "CmdCCToggleFileOps"
#define CMD_CC_TOGGLE_VIEW        "CmdCCToggleView"
#define CMD_CC_TOGGLE_OVERLAYS    "CmdCCToggleOverlays"
#define CMD_CC_TOGGLE_UNDO_REDO   "CmdCCToggleUndoRedo"
#define CMD_CC_TOGGLE_CLEAR_FRAME "CmdCCToggleClearFrame"

#define CMD_INCREASE_SIZE "CmdIncreaseSize"
#define CMD_DECREASE_SIZE "CmdDecreaseSize"
#define CMD_INCREASE_OPACITY "CmdIncreaseOpacity"
#define CMD_DECREASE_OPACITY "CmdDecreaseOpacity"
#define CMD_INCREASE_SMOOTH "CmdIncreaseSmooth"
#define CMD_DECREASE_SMOOTH "CmdDecreaseSmooth"
#define CMD_EXIT "CmdExit"

// Canvas menu actions (user-assignable; default empty)
#define CMD_CANVAS_SIZE          "CmdCanvasSize"
#define CMD_CANVAS_COLOR         "CmdCanvasColor"
#define CMD_CANVAS_EXTRA_SPACE   "CmdCanvasExtraSpace"
#define CMD_CANVAS_SHOW_PALETTE  "CmdCanvasShowPalette"

// Canvas navigation
#define CMD_PAN_VIEW "CmdPanView"
#define CMD_TEMP_PAN_HOLD "CmdTempPanHold"
#define CMD_TEMP_ZOOM_HOLD "CmdTempZoomHold"
#define CMD_TEMP_ROTATE_HOLD "CmdTempRotateHold"
// Generic per-tool temporary hold commands (user-assignable; default empty)
#define CMD_TEMP_TOOL_PEN "CmdTempToolPen"
#define CMD_TEMP_TOOL_PENCIL "CmdTempToolPencil"
#define CMD_TEMP_TOOL_ERASER "CmdTempToolEraser"
#define CMD_TEMP_TOOL_SELECT "CmdTempToolSelect"
#define CMD_TEMP_TOOL_MOVE "CmdTempToolMove"
#define CMD_TEMP_TOOL_POLYLINE "CmdTempToolPolyline"
#define CMD_TEMP_TOOL_CURVE "CmdTempToolCurve"
#define CMD_TEMP_TOOL_BSPLINE "CmdTempToolBSpline"
#define CMD_TEMP_TOOL_BUCKET "CmdTempToolBucket"
#define CMD_TEMP_TOOL_EYEDROPPER "CmdTempToolEyedropper"
#define CMD_TEMP_TOOL_DPLATE "CmdTempToolDplate"
// Additional tool holds
#define CMD_TEMP_TOOL_HAND "CmdTempToolHand"
#define CMD_TEMP_TOOL_CAMERA "CmdTempToolCamera"
#define CMD_TEMP_TOOL_LASSO_SELECT "CmdTempToolLassoSelect"

// Temporary holds for gap-fill tools (user-assignable; default empty)
// Note: these strings must match the keys in resources/kb.ini (no trailing "Hold")
#define CMD_TEMP_RECT_GAP_FILL "CmdTempRectGapFill"
#define CMD_TEMP_LASSO_GAP_FILL "CmdTempLassoGapFill"

// Save / Export
#define LAST_PCLX_PATH          "LastFilePath"

// Import
#define IMPORT_REPOSITION_TYPE      "ImportRepositionType"

// Settings Group/Key Name
#define PENCIL2D "Nurture2D"
#define SHORTCUTS_GROUP             "Shortcuts"
#define SETTING_AUTO_SAVE                  "AutoSave"
#define SETTING_AUTO_SAVE_NUMBER           "AutosaveNumber"
#define SETTING_AUTO_SAVE_INTERVAL_MINUTES "AutosaveIntervalMinutes"
#define SETTING_TOOL_CURSOR         "ToolCursors"
#define SETTING_CANVAS_CURSOR       "DottedCursors"
#define SETTING_ALWAYS_SHOW_CIRCLE  "AlwaysShowCircle"
#define SETTING_HIGH_RESOLUTION     "HighResPosition"
#define SETTING_BACKGROUND_STYLE    "Background"
#define SETTING_CANVAS_COLOR        "CanvasColor"
#define SETTING_WINDOW_OPACITY      "WindowOpacity"
#define SETTING_WINDOW_GEOMETRY     "WindowGeometry"
#define SETTING_WINDOW_STATE        "WindowState"
#define SETTING_SHOW_STATUS_BAR     "ShowStatusBar"
#define SETTING_CURVE_SMOOTHING     "CurveSmoothing"
#define SETTING_DISPLAY_EFFECT      "RenderEffect"
#define SETTING_SHORT_SCRUB         "ShortScrub"
#define SETTING_FPS                 "Fps"
#define SETTING_FIELD_W             "FieldW"
#define SETTING_FIELD_H             "FieldH"
#define SETTING_EXTRA_SPACE_STYLE          "CanvasExtraSpaceStyle"
#define SETTING_EXTRA_SPACE_SCALE_PERCENT  "CanvasExtraSpaceScalePercent"
#define SETTING_EXTRA_SPACE_MANUAL_W       "CanvasExtraSpaceManualW"
#define SETTING_EXTRA_SPACE_MANUAL_H       "CanvasExtraSpaceManualH"
#define SETTING_FRAME_SIZE          "FrameSize"
#define SETTING_TIMELINE_SIZE       "TimelineSize"
#define SETTING_LABEL_FONT_SIZE     "LabelFontSize"
#define SETTING_DRAW_LABEL          "DrawLabel"
#define SETTING_QUICK_SIZING        "QuickSizing"
#define SETTING_LAYOUT_LOCK         "LayoutLock"
#define SETTING_ROTATION_INCREMENT  "RotationIncrement"
#define SETTING_SHOW_SELECTION_INFO "ShowSelectionInfo"
#define SETTING_ASK_FOR_PRESET      "AskForPreset"
#define SETTING_LOAD_MOST_RECENT    "LoadMostRecent"
#define SETTING_LOAD_DEFAULT_PRESET "LoadDefaultPreset"
#define SETTING_DEFAULT_PRESET      "DefaultPreset"

#define SETTING_INVERT_DRAG_ZOOM_DIRECTION   "InvertDragZoomDirection"
#define SETTING_INVERT_SCROLL_ZOOM_DIRECTION "InvertScrollZoomDirection"
#define SETTING_ZOOM_HORIZONTAL_DRAG         "ZoomHorizontalDrag"
#define SETTING_LOW_RES_NAVIGATION           "LowResNavigation"

#define SETTING_ANTIALIAS        "Antialiasing"
#define SETTING_SHOW_GRID        "ShowGrid"
#define SETTING_COUNT            "Count"
#define SETTING_SHADOW           "Shadow"
#define SETTING_PREV_ONION       "PrevOnion"
#define SETTING_NEXT_ONION       "NextOnion"
#define SETTING_MULTILAYER_ONION "MultilayerOnion"
#define SETTING_AXIS             "Axis"
#define SETTING_CAMERABORDER     "CameraBorder"
#define SETTING_INVISIBLE_LINES  "InvisibleLines"
#define SETTING_OUTLINES         "Outlines"
#define SETTING_ONION_BLUE       "OnionBlue"
#define SETTING_ONION_RED        "OnionRed"

#define SETTING_FRAME_POOL_SIZE  "FramePoolSizeInMB"
#define SETTING_GRID_SIZE_W      "GridSizeW"
#define SETTING_GRID_SIZE_H      "GridSizeH"
#define SETTING_OVERLAY_CENTER   "OverlayCenter"
#define SETTING_OVERLAY_THIRDS   "OverlayThirds"
#define SETTING_OVERLAY_GOLDEN   "OverlayGolden"
#define SETTING_OVERLAY_SAFE     "OverlaySafe"
#define SETTING_OVERLAY_PERSPECTIVE1 "OverlayPerspective1"
#define SETTING_OVERLAY_PERSPECTIVE2 "OverlayPerspective2"
#define SETTING_OVERLAY_PERSPECTIVE3 "OverlayPerspective3"
#define SETTING_OVERLAY_ANGLE    "OverlayAngle"
#define SETTING_TITLE_SAFE_ON    "TitleSafeOn"
#define SETTING_TITLE_SAFE       "TitleSafe"
#define SETTING_ACTION_SAFE_ON   "ActionSafeOn"
#define SETTING_ACTION_SAFE      "ActionSafe"
#define SETTING_OVERLAY_SAFE_HELPER_TEXT_ON "OverlaySafeHelperTextOn"
#define SETTING_TIMECODE_TEXT    "TimecodeText"

#define SETTING_ONION_MAX_OPACITY       "OnionMaxOpacity"
#define SETTING_ONION_MIN_OPACITY       "OnionMinOpacity"
#define SETTING_ONION_PREV_FRAMES_NUM   "OnionPrevFramesNum"
#define SETTING_ONION_NEXT_FRAMES_NUM   "OnionNextFramesNum"
#define SETTING_ONION_WHILE_PLAYBACK    "OnionWhilePlayback"
#define SETTING_ONION_TYPE              "OnionType"
#define SETTING_FLIP_ROLL_MSEC          "FlipRoll"
#define SETTING_FLIP_ROLL_DRAWINGS      "FlipRollDrawings"
#define SETTING_FLIP_INBETWEEN_MSEC     "FlipInbetween"
#define SETTING_SOUND_SCRUB_ACTIVE      "SoundScrubActive"
#define SETTING_SOUND_SCRUB_MSEC        "SoundScrubMsec"
// Legacy preference key removed: NewUndoRedoOn
#define SETTING_UNDO_REDO_MAX_STEPS     "UndoRedoMaxSteps"

// Gap Checking mode settings
#define SETTING_GAP_CHECK_MODE          "GapCheckMode"
#define SETTING_GAP_CHECK_BG_COLOR      "GapCheckBgColor"
#define SETTING_GAP_CHECK_DRAW_COLOR    "GapCheckDrawColor"

// Color preferences
#define SETTING_ACCENT_COLOR            "AccentColor"
#define SETTING_TOOLBOX_SELECTION_COLOR "ToolboxSelectionColor"
#define SETTING_TOOL_PALETTE_SELECTION_COLOR "ToolPaletteSelectionColor"
#define SETTING_COSMIC_ROUNDNESS        "CosmicRoundness"
#define SETTING_COSMIC_BORDER_WIDTH     "CosmicBorderWidth"
#define SETTING_COSMIC_BORDER_COLOR     "CosmicBorderColor"
#define SETTING_COSMIC_ACCENT_COLOR     "CosmicAccentColor"

// UI Theme colors
#define SETTING_UI_TITLE_BAR_COLOR      "UITitleBarColor"
#define SETTING_UI_DOCK_CONTENT_COLOR   "UIDockContentColor"
#define SETTING_UI_SCROLLBAR_COLOR      "UIScrollbarColor"

#define SETTING_PLAYBACK_PERFORMANCE_MODE "PlaybackPerformanceMode"

#define SETTING_SHIFT_TRACE_WINDOW_VISIBLE "ShiftTraceWindowVisible"

#define SETTING_BUCKET_TOLERANCE "Tolerance"
#define SETTING_BUCKET_TOLERANCE_ON "BucketToleranceEnabled"
// #define SETTING_BUCKET_FILL_EXPAND "BucketFillExpand"         // removed
// #define SETTING_BUCKET_FILL_EXPAND_ON "BucketFillExpandEnabled" // removed
#define SETTING_BUCKET_FILL_REFERENCE_MODE "BucketFillReferenceMode"

#define SETTING_FILL_MODE "FillMode"
#define SETTING_BUCKET_GAP_FILL_MODE "BucketGapFillMode"

#define SETTING_LAYER_VISIBILITY "LayerVisibility"
#define SETTING_LAYER_VISIBILITY_THRESHOLD "LayerVisibilityThreshold"
#define SETTING_DIM_INACTIVE_LAYERS "DimInactiveLayers"

#define SETTING_CELL_BANK_VISIBLE "CellBankVisible"

#define SETTING_DRAW_ON_EMPTY_FRAME_ACTION  "DrawOnEmptyFrameAction"

#define SETTING_LANGUAGE        "Language"

// Tool shortcut hold preference (in milliseconds)
#define SETTING_TEMP_TOOL_HOLD_MSEC "TempToolHoldMsec"

// Pencil performance settings keys
#define SETTING_PENCIL_MAX_DAB_CACHE_SIZE    "PencilMaxDabCacheSize"
#define SETTING_PENCIL_MAX_TEXTURE_CACHE_SIZE "PencilMaxTextureCacheSize"
#define SETTING_PENCIL_MAX_TEXTURE_RESOLUTION "PencilMaxTextureResolution"
#define SETTING_PENCIL_PERFORMANCE_LOGGING    "PencilPerformanceLogging"

// New pencil optimization settings
#define SETTING_PENCIL_USE_LIGHTWEIGHT_PREVIEW "PencilUseLightweightPreview"
#define SETTING_PENCIL_TILE_BATCHING_ENABLED   "PencilTileBatchingEnabled"
#define SETTING_PENCIL_MAX_RANDOM_SAMPLES      "PencilMaxRandomSamples"
#define SETTING_PENCIL_PRESSURE_LUT_SIZE       "PencilPressureLutSize"

#endif // PENCILDEF_H
