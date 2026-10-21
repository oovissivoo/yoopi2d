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

#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

#include <QObject>
#include <QHash>
#include "basetool.h"
#include "basemanager.h"
#include "camerafieldoption.h"

class ScribbleArea;

class ToolManager : public BaseManager
{
    Q_OBJECT
public:

    explicit ToolManager(Editor* editor);
    ~ToolManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    BaseTool* currentTool() const;
    BaseTool* getTool(ToolType eToolType) const;
    void setDefaultTool();
    void setCurrentTool(ToolType eToolType);
    void tabletSwitchToEraser();
    void tabletRestorePrevTool();

    // New simple temporary tool API
    bool beginTemporaryTool(ToolType eToolType, Qt::Key key = Qt::Key_unknown);
    // Enable sample-only mode for Eyedropper during temporary use
    void setEyedropperSampleOnly(bool enabled);
    // Enable continuous-pick mode for Eyedropper (used by canvas Alt-hold picker)
    void setEyedropperContinuousPick(bool enabled);
    bool endTemporaryTool();
    bool shouldEndTemporaryTool(Qt::Key key) const;  // Check if this key should end temp tool

    // While user is editing properties (e.g., dragging size slider), lock edits to a specific tool
    void beginEditingPropertiesForTool(ToolType toolType);
    void endEditingProperties();

    // Global width accessor
    qreal globalWidth() const { return mGlobalWidth; }
    bool isGlobalWidthEnabled() const { return mUseGlobalWidth; }

    void cleanupAllToolsData();
    bool leavingThisTool();

    int propertySwitch(bool condition, int property);

signals:
    void toolChanged(ToolType);
    void toolPropertyChanged(ToolType, ToolPropertyType);
    void beginHeavyUIEdit();
    void endHeavyUIEdit();

public slots:
    void resetAllTools();

    void setWidth(float);
    void setFeather(float);

    void setUseFeather(bool);
    void setInvisibility(bool);
    void setPreserveAlpha(bool);
    void setVectorMergeEnabled(bool);
    void setBezier(bool);
    void setClosedPath(bool);
    void setPressure(bool);
    void setAA(int);
    void setFillMode(int);
    void setStabilizerLevel(int);
    // Post-stroke correction level
    void setPostCorrectionLevel(int);
    void setTolerance(int);
    void setBucketColorToleranceEnabled(bool enabled);
    void setBucketFillExpandEnabled(bool enabled);
    void setBucketFillReferenceMode(int referenceMode);
    void setBucketFillExpand(int);
    void setUseFillContour(bool);
    void setShowSelectionInfo(bool b);
    void setShowCameraPath(bool);
    void resetCameraPath();
    void setCameraPathDotColor(int);
    void resetCameraTransform(CameraFieldOption option);
    void setDrawBehind(bool);
    // Eraser: lasso mode toggle
    void setEraserLassoMode(bool);

    // Krita-like smoothing (Pen tool)
    void setPenSmoothingMode(int);
    // Weighted
    void setWeightedDistance(double);
    void setWeightedStrokeEnding(double);
    void setWeightedSmoothPressure(bool);
    void setWeightedScalableDistance(bool);
    // Stabilizer2
    void setStab2Distance(double);
    void setStab2Delay(double);
    void setStab2FinishLine(bool);
    void setStab2StabilizeSensors(bool);
    void setStab2ScalableDistance(bool);

    void setPenPressureCurve(const QVector<QPointF>& points);

    // Curve tool tapering (Fixed Pixels only, 0-200px)
    void setCurveTaperStart(qreal);
    void setCurveTaperEnd(qreal);
    void setHybridSnapLevel(int);

    /// Layer mode will be enforced by the the choice the reference mode selected.
    /// @return Returns true if reference mode is ``current layer`, otherwise false.
    bool bucketReferenceModeIsCurrentLayer(int referenceMode) const;

    // New: color replace mode
    void setColorReplaceMode(bool enabled);
    // New: bucket same color only toggle
    void setBucketSameColorOnly(bool enabled);
    // New: bucket multi-keyframe fill toggle
    void setBucketMultiKeyframeFill(bool enabled);
    // New: bucket gap fill strength (Off/Low/Med/High)
    void setBucketGapFillMode(int mode);


private:
    BaseTool* mCurrentTool = nullptr;
    BaseTool* mTabletEraserTool = nullptr;

    // New temporary switching state
    bool mTemporaryActive = false;
    BaseTool* mTemporaryTool = nullptr;
    BaseTool* mPreviousTool = nullptr;
    Qt::Key mTemporaryToolKey = Qt::Key_unknown;  // Track which key is being held for temp tool

    // Target tool for in-progress property editing (e.g., size slider drag)
    BaseTool* mPropertyEditTargetTool = nullptr;

    QHash<ToolType, BaseTool*> mToolSetHash;

    int mOldValue = 0;

    // Global width support (optional): when enabled, all tools that support WIDTH
    // will use this shared width on activation
    qreal mGlobalWidth = -1.0;
    bool  mUseGlobalWidth = true;

    // Optional stroke color override used by stroke tools when a preset swatch is active
    QColor mStrokeColorOverride;
    bool   mHasStrokeColorOverride = false;

public:
    // Stroke color override API (used by palette to set temporary drawing color)
    void setStrokeColorOverride(const QColor& color) { mStrokeColorOverride = color; mHasStrokeColorOverride = true; }
    void clearStrokeColorOverride() { mHasStrokeColorOverride = false; }
    bool hasStrokeColorOverride() const { return mHasStrokeColorOverride; }
    QColor strokeColorOverride() const { return mStrokeColorOverride; }
    
private:
    // Grouped width support: three groups with independent widths
    // Group A: PEN
    // Group B: PENCIL
    // Group C: CURVE + BSPLINE + POLYLINE
    // Group D: ERASER
    qreal mGroupWidthPen = -1.0;
    qreal mGroupWidthPencil = -1.0;
    qreal mGroupWidthCurveFamily = -1.0;
    qreal mGroupWidthEraser = -1.0;

    // Global Draw Behind setting applied consistently across tools
    bool mGlobalDrawBehind = false;

    // Helper to determine group width reference for a tool
    qreal* groupWidthForTool(ToolType type)
    {
        switch (type)
        {
        case PEN:
            return &mGroupWidthPen;
        case PENCIL:
            return &mGroupWidthPencil;
        case CURVE:
        case BSPLINE:
        case POLYLINE:
            return &mGroupWidthCurveFamily;
        case ERASER:
            return &mGroupWidthEraser;
        default:
            // Fallback to pen/pencil group for other drawable tools that use width
            return &mGroupWidthPen;
        }
    }
};

#endif // TOOLMANAGER_H
