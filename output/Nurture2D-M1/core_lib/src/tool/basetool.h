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

#ifndef BASETOOL_H
#define BASETOOL_H

#include <QObject>
#include <QString>
#include <QCursor>
#include <QPainter>
#include <QHash>
#include <QEvent>
#include "pencildef.h"

class QPixmap;
class Editor;
class ScribbleArea;
class QEnterEvent;
class QKeyEvent;
class QMouseEvent;
class QTabletEvent;
class PointerEvent;

class Properties
{
public:
    qreal width = 1.0;
    qreal feather = 1.0;
    bool  pressure = true;
    int   invisibility = 0;
    int   preserveAlpha = 0;
    bool  vectorMergeEnabled = false;
    bool  bezier_state = false;
    bool  closedPolylinePath = false;
    bool  useFeather = true;
    int   useAA = 0;
    int   fillMode = 0;
    int   stabilizerLevel = 0;
    // Pen post-stroke correction: 0=Off, 1=Low, 2=Med, 3=High
    int   postCorrectionLevel = 0;
    qreal tolerance = 0;
    bool toleranceEnabled = false;
    int bucketFillExpand = 0;
    bool bucketFillExpandEnabled = 0;
    int bucketFillReferenceMode = 0;
    bool  useFillContour = false;
    bool  showSelectionInfo = true;
    bool  cameraShowPath = true;
    DotColorType cameraPathDotColorType = DotColorType::RED;
    bool  drawBehind = false;
    // Krita-like smoothing (Pen only)
    PenSmoothingMode penSmoothingMode = PenSmoothingMode::LEGACY;
    // Weighted smoothing
    qreal weightedDistance = 12.0;
    qreal weightedStrokeEnding = 0.0; // 0..1 fraction
    bool  weightedSmoothPressure = false;
    bool  weightedScalableDistance = true;
    // Stabilizer smoothing (Krita-style)
    qreal stab2Distance = 350.0; // hardcoded distance in algorithm; UI removed
    qreal stab2Delay = 4.0;
    bool  stab2FinishLine = true;
    bool  stab2StabilizeSensors = false;
    bool  stab2ScalableDistance = true;

    // Pressure pipeline (always-on pressure smoothing and tapering)
    qreal pressureSmoothingAmount = 0.3;  // Range 0.0 (minimal smoothing) to 1.0 (max smoothing), always applied
    qreal pressureTaperingStart = 0.0;    // Range 0.0 (no start taper) to 1.0 (full start fade) - CHANGED from 0.5 to 0.0
    qreal pressureTaperingEnd = 0.5;      // Range 0.0 (no end taper) to 1.0 (full end fade, tail aggressiveness)

    // Curve tool distance-based tapering (Fixed Pixels only: 0-200px)
    // Shared between Curve and BSpline tools
    qreal curveTaperStart = 0.0;  // Start taper distance in pixels (0.0 to 200.0)
    qreal curveTaperEnd = 0.0;    // End taper distance in pixels (0.0 to 200.0)

    int   hybridSnapLevel = 0;

    // Eyedropper reference selection: 0=From View (composited), 1=Current Layer
    int   eyedropperReferenceMode = static_cast<int>(EyedropperReferenceMode::FROM_VIEW);

    // Bucket tool specific options
    bool colorReplaceMode = false; // When true, replace all pixels matching clicked color across reference image
    // When true, restrict drag-to-fill to exactly the initially clicked color
    bool bucketSameColorOnly = false;
    // Multi-Keyframe Fill: when enabled, bucket can fill across a frame range using a movable gizmo
    bool bucketMultiKeyframeFill = false;
    int  bucketGapFillMode = 0;

    // Eraser-specific options
    bool eraserLassoMode = false; // When true, Eraser acts as lasso delete selector
};

const int ON = 1;
const int OFF = 0;
const int DISABLED = -1;


class BaseTool : public QObject
{
    Q_OBJECT
protected:
    explicit BaseTool(QObject* parent);
public:
    static QString TypeName(ToolType);
    QString typeName() { return TypeName(type()); }

    void initialize(Editor* editor);

    virtual ToolType type() = 0;
    virtual void loadSettings() = 0;
    virtual void saveSettings() = 0;
    virtual QCursor cursor();

    virtual void pointerPressEvent(PointerEvent*) = 0;
    virtual void pointerMoveEvent(PointerEvent*) = 0;
    virtual void pointerReleaseEvent(PointerEvent*) = 0;
    virtual void pointerDoubleClickEvent(PointerEvent*);

    // return true if handled
    virtual bool keyPressEvent(QKeyEvent*) { return false; }
    virtual bool keyReleaseEvent(QKeyEvent*) { return false; }

    // Optional hook for tools that need custom deselect-all behavior
    // (e.g. to commit selection moves with custom undo handling)
    virtual bool handleDeselectAll() { return false; }

    virtual bool enterEvent(QEnterEvent*) { return false; }
    virtual bool leaveEvent(QEvent*) { return false; }

    virtual void clearToolData() {}
    virtual void resetToDefault() {}

    /** Check if the tool is active.
     *
     *  An active tool is definied as one which is actively modifying the buffer.
     *  This is used to check if an full frame cache can be used instead of redrawing with CanvasPainter.
     *
     * @return Returns true if the tool is currently active, else returns false.
     */
    virtual bool isActive() const;

    virtual void setWidth(const qreal width);
    virtual void setFeather(const qreal feather);

    virtual void setInvisibility(const bool invisibility);
    virtual void setBezier(const bool bezier_state);
    virtual void setClosedPath(const bool closed);
    virtual void setPressure(const bool pressure);
    virtual void setUseFeather(const bool usingFeather);
    virtual void setPreserveAlpha(const bool preserveAlpha);
    virtual void setVectorMergeEnabled(const bool vectorMergeEnabled);
    virtual void setAA(const int useAA);
    virtual void setFillMode(const int mode);
    virtual void setStabilizerLevel(const int level);
    virtual void setTolerance(const int tolerance);
    virtual void setToleranceEnabled(const bool enabled);
    virtual void setFillExpand(const int fillExpandValue);
    virtual void setFillExpandEnabled(const bool enabled);
    virtual void setFillReferenceMode(int referenceMode);
    virtual void setUseFillContour(const bool useFillContour);
    virtual void setShowSelectionInfo(const bool b);
    virtual void setShowCameraPath(const bool showCameraPath);
    virtual void setPathDotColorType(const DotColorType dotColorType);
    virtual void resetCameraPath();
    virtual void setDrawBehind(const bool drawBehind);

    // New: color replace mode toggle
    virtual void setColorReplaceMode(const bool enable) { properties.colorReplaceMode = enable; }

    // Eraser-specific toggles
    virtual void setEraserLassoMode(const bool enable) { properties.eraserLassoMode = enable; }

    virtual void paint(QPainter& painter, const QRect& blitRect) { Q_UNUSED(painter) Q_UNUSED(blitRect) }
    
    /** Paint tool-specific overlay for vector preview (e.g. curve paths before commit).
     *  Called during ScribbleArea::paintEvent() after bitmap content but before UI overlays.
     *  Painter is already configured with view transform.
     */
    virtual void paintOverlay(QPainter& painter) { Q_UNUSED(painter) }

    // Called by the app after showing the context menu requested via requestContextMenu().
    // index is the 0-based chosen action index, or -1 if dismissed.
    virtual void onContextMenuResult(int index) { Q_UNUSED(index) }

    /// Will clean up `active` connections
    virtual bool leavingThisTool();

    /// Setup `active` connections here that should only emit while tool is active
    /// `leavingThisTool` will handle the cleanup of `active` connections
    virtual bool enteringThisTool() { loadSettings(); return true; }

    Properties properties;

    bool isPropertyEnabled(ToolPropertyType t) { return mPropertyEnabled[t]; }
    bool isDrawingTool();

signals:
    bool isActiveChanged(ToolType, bool);
    // Emitted when the tool wants a context menu shown at screenPos.
    // items: list of action labels. enabledFlags: per-item enabled state.
    // The receiver should show the menu and call onContextMenuResult(chosenIndex)
    // with the 0-based index of the chosen action, or -1 if dismissed.
    void requestContextMenu(QPoint screenPos, QStringList items, QList<bool> enabledFlags);

protected:
    Editor* editor() { return mEditor; }

    QHash<ToolPropertyType, bool> mPropertyEnabled;

    Editor* mEditor = nullptr;
    ScribbleArea* mScribbleArea = nullptr;
    QList<QMetaObject::Connection> mActiveConnections;

    // Deferred settings persistence helpers
public:
    virtual void saveSettingsIfDirty() { if (areSettingsDirty()) { saveSettings(); clearSettingsDirty(); } }

    // Public notifier for external managers to mark settings as changed
    void notifySettingsChanged() { markSettingsDirty(); }

protected:
    void markSettingsDirty() { mSettingsDirty = true; }
    bool areSettingsDirty() const { return mSettingsDirty; }
    void clearSettingsDirty() { mSettingsDirty = false; }

    bool mSettingsDirty = false;
};

#endif // BASETOOL_H
