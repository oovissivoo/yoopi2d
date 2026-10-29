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
#include "toolmanager.h"
#include "editor.h"
#include "scribblearea.h"

#include <cmath>
#include <QDebug>
#include "pentool.h"
#include "penciltool.h"
#include "buckettool.h"
#include "erasertool.h"
#include "eyedroppertool.h"
#include "handtool.h"
#include "movetool.h"
#include "polylinetool.h"
#include "curvetool.h"
#include "bsplinetool.h"
#include "selecttool.h"
#include "cameratool.h"
#include "editor.h"
#include "rectgapfilltool.h"
#include "lassogapfilltool.h"
#include "dplatetool.h"
#include "lassoselecttool.h"
#include <QSettings>
#include "managers/settingsmanager.h"


ToolManager::ToolManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

ToolManager::~ToolManager()
{
    foreach(BaseTool* tool, mToolSetHash)
    {
        tool->saveSettingsIfDirty();
    }
}

bool ToolManager::init()
{
    mToolSetHash.insert(PEN, new PenTool(this));
    mToolSetHash.insert(PENCIL, new PencilTool(this));
    mToolSetHash.insert(ERASER, new EraserTool(this));
    mToolSetHash.insert(BUCKET, new BucketTool(this));
    mToolSetHash.insert(EYEDROPPER, new EyedropperTool(this));
    mToolSetHash.insert(HAND, new HandTool(this));
    mToolSetHash.insert(MOVE, new MoveTool(this));
    mToolSetHash.insert(POLYLINE, new PolylineTool(this));
    mToolSetHash.insert(CURVE, new CurveTool(this));
    mToolSetHash.insert(BSPLINE, new BSplineTool(this));
    mToolSetHash.insert(SELECT, new SelectTool(this));
    mToolSetHash.insert(CAMERA, new CameraTool(this));
    // New tools
    mToolSetHash.insert(RECT_GAP_FILL, new RectGapFillTool(this));
    mToolSetHash.insert(LASSO_GAP_FILL, new LassoGapFillTool(this));
    // Enable Lasso Select tool
    mToolSetHash.insert(LASSO_SELECT, new LassoSelectTool(this));
    mToolSetHash.insert(DPLATE, new DplateTool(this));

    foreach(BaseTool* pTool, mToolSetHash.values())
    {
        pTool->initialize(editor());
    }

    // Ensure SettingsManager is initialized early and load persisted grouped widths
    // before selecting the default tool so the group values are applied correctly
    // during tool initialization.
    SettingsManager* settings = SettingsManager::instance();

    // Migration: older builds stored a single width shared by Pen+Pencil.
    // If new keys are missing but legacy exists, seed both tools with the legacy value.
    const QVariant legacyPenPencil = settings->getValue("widthGroupPenPencil", QVariant());
    const QVariant penWidthVar = settings->getValue("widthGroupPen", QVariant());
    const QVariant pencilWidthVar = settings->getValue("widthGroupPencil", QVariant());
    if (legacyPenPencil.isValid() && (!penWidthVar.isValid() || !pencilWidthVar.isValid())) {
        const qreal legacy = legacyPenPencil.toDouble();
        if (!penWidthVar.isValid()) { settings->deferredSetValue("widthGroupPen", legacy); }
        if (!pencilWidthVar.isValid()) { settings->deferredSetValue("widthGroupPencil", legacy); }
    }

    mGroupWidthPen = settings->getValue("widthGroupPen", -1.0).toDouble();
    mGroupWidthPencil = settings->getValue("widthGroupPencil", -1.0).toDouble();
    mGroupWidthCurveFamily = settings->getValue("widthGroupCurveFamily", -1.0).toDouble();
    mGroupWidthEraser = settings->getValue("widthGroupEraser", -1.0).toDouble();
    mGlobalDrawBehind = settings->getValue("globalDrawBehind", false).toBool();

    // Now set default tool; with group widths loaded, the group's width will be
    // applied when the tool becomes active.
    setDefaultTool();

    // Initialize grouped widths and global draw-behind from the active tool if
    // values were not present in settings (keep persisted values otherwise).
    if (currentTool()) {
        if (mUseGlobalWidth && currentTool()->isPropertyEnabled(WIDTH)) {
            qreal* gw = groupWidthForTool(currentTool()->type());
            if (gw && *gw <= 0.0) { *gw = currentTool()->properties.width; }
        }
        if (!settings->getValue("globalDrawBehind", QVariant()).isValid()) {
            mGlobalDrawBehind = currentTool()->properties.drawBehind;
        }
    }

    return true;
}

Status ToolManager::load(Object*)
{
    // Reset temporary/transient tool state (tablet eraser, temp key hold, etc.)
    // but do NOT switch the active tool — the brush preset system manages that
    // at the app level and must not be clobbered every time an object loads.
    mTabletEraserTool = nullptr;
    mTemporaryActive = false;
    mTemporaryTool = nullptr;
    mPreviousTool = nullptr;
    mTemporaryToolKey = Qt::Key_unknown;

    // Ensure grouped widths and draw-behind are seeded after loading a project/session as well
    if (currentTool()) {
        if (mUseGlobalWidth && currentTool()->isPropertyEnabled(WIDTH)) {
            qreal* gw = groupWidthForTool(currentTool()->type());
            if (gw && *gw <= 0.0) { *gw = currentTool()->properties.width; }
        }
        mGlobalDrawBehind = currentTool()->properties.drawBehind;
    }
    return Status::OK;
}

Status ToolManager::save(Object*)
{
    return Status::OK;
}

BaseTool* ToolManager::currentTool() const
{
    if (mTemporaryActive && mTemporaryTool != nullptr)
    {
        return mTemporaryTool;
    }
    else if (mTabletEraserTool != nullptr)
    {
        return mTabletEraserTool;
    }
    return mCurrentTool;
}

BaseTool* ToolManager::getTool(ToolType eToolType) const
{
    return mToolSetHash.value(eToolType, nullptr);
}

void ToolManager::setDefaultTool()
{
    // Set default tool
    // (called by the main window init)
    ToolType defaultToolType = PEN;

    setCurrentTool(defaultToolType);
    mTabletEraserTool = nullptr;
    // Clear temporary state
    mTemporaryActive = false;
    mTemporaryTool = nullptr;
    mPreviousTool = nullptr;
    mTemporaryToolKey = Qt::Key_unknown;
}

void ToolManager::setCurrentTool(ToolType eToolType)
{
    // Commit any in-progress property edits before switching tools. This ensures
    // that quick tool switches (eg. via keyboard shortcuts) do not lose pending
    // slider/spinbox edits that haven't been ended via sliderReleased/editingFinished.
    if (mPropertyEditTargetTool) {
        qDebug() << "[ToolManager] setCurrentTool: pending edit; committing" << BaseTool::TypeName(mPropertyEditTargetTool->type());
        endEditingProperties();
    }

    // We're already using this tool
    if (mCurrentTool == getTool(eToolType)) { return; }

    if (currentTool())
    {
        // Persist the active tool's settings (covers temporary/eraser overrides)
        // and use the manager wrapper so Move tool transforms are committed
        // before the actual switch.
        qDebug() << "[ToolManager] setCurrentTool: leaving tool" << BaseTool::TypeName(currentTool()->type());
        currentTool()->saveSettingsIfDirty();
        leavingThisTool();
    }

    if (eToolType == PENCIL || eToolType == PEN || eToolType == POLYLINE ||
        eToolType == CURVE || eToolType == BSPLINE) {
        mLastDrawingTool = eToolType;
    }
    mCurrentTool = getTool(eToolType);
    // Ensure the tool refreshes its in-memory properties from saved settings upon entry,
    // unless an explicit property edit is in progress (prevents flicker/mid-gesture reset)
    if (mCurrentTool && mPropertyEditTargetTool == nullptr) { mCurrentTool->loadSettings(); }
    qDebug() << "[ToolManager] setCurrentTool" << BaseTool::TypeName(eToolType);
    mCurrentTool->enteringThisTool();
    // Apply grouped width if enabled and applicable
    if (mUseGlobalWidth && mCurrentTool && mCurrentTool->isPropertyEnabled(WIDTH)) {
        if (qreal* gw = groupWidthForTool(mCurrentTool->type())) {
            if (*gw > 0.0) {
            mCurrentTool->setWidth(*gw);
            mCurrentTool->saveSettingsIfDirty();
                emit toolPropertyChanged(mCurrentTool->type(), WIDTH);
            }
        }
    }
    // Apply global Draw Behind consistently
    if (mCurrentTool && mCurrentTool->isPropertyEnabled(DRAW_BEHIND)) {
        mCurrentTool->setDrawBehind(mGlobalDrawBehind);
        mCurrentTool->saveSettingsIfDirty();
        emit toolPropertyChanged(mCurrentTool->type(), DRAW_BEHIND);
    }
    // If global width is enabled and group width is loaded, apply group width now
    if (mUseGlobalWidth && mCurrentTool && mCurrentTool->isPropertyEnabled(WIDTH)) {
        if (qreal* gw = groupWidthForTool(mCurrentTool->type())) {
            if (*gw > 0.0) {
                mCurrentTool->setWidth(*gw);
                mCurrentTool->saveSettingsIfDirty();
                emit toolPropertyChanged(mCurrentTool->type(), WIDTH);
            }
        }
    }
    if (!mTemporaryActive && mTabletEraserTool == nullptr)
    {
        emit toolChanged(eToolType);
    }
}

bool ToolManager::leavingThisTool()
{
    // If leaving from Move tool, commit any pending selection transform before switch
    if (currentTool()->type() == ToolType::MOVE) {
        editor()->getScribbleArea()->applyTransformedSelection();
    }
    return currentTool()->leavingThisTool();
}

void ToolManager::cleanupAllToolsData()
{
    foreach(BaseTool* tool, mToolSetHash)
    {
        tool->clearToolData();
    }
}

void ToolManager::resetAllTools()
{
    // Reset can be useful to solve some pencil settings problems.
    // Beta-testers should be recommended to reset before sending tool related issues.
    // This can prevent from users to stop working on their project.

    foreach(BaseTool* tool, mToolSetHash)
    {
        tool->resetToDefault();
    }
    qDebug("tools restored to default settings");
}


void ToolManager::setPenPressureCurve(const QVector<QPointF>& points)
{
    if (auto* pen = qobject_cast<PenTool*>(getTool(PEN))) {
        pen->setPressureCurve(points);
        pen->saveSettings();
        emit toolPropertyChanged(PEN, PRESSURE);
    } else {
        Q_UNUSED(points);
    }
}

// Performance setting slots removed; PencilTool loads settings automatically

void ToolManager::setWidth(float newWidth)
{
    if (std::isnan(newWidth) || newWidth < 0)
    {
        newWidth = 1.f;
    }

    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setWidth(static_cast<qreal>(newWidth));
    // Update grouped width reference for this tool family and persist it
    if (mUseGlobalWidth && target && target->isPropertyEnabled(WIDTH))
    {
        if (qreal* gw = groupWidthForTool(target->type())) {
            *gw = static_cast<qreal>(newWidth);
            // Persist group width via SettingsManager to keep families in sync immediately
            SettingsManager* settings = SettingsManager::instance();
            switch (target->type()) {
            case PEN:
                settings->deferredSetValue("widthGroupPen", *gw); break;
            case PENCIL:
                settings->deferredSetValue("widthGroupPencil", *gw); break;
            case CURVE:
            case BSPLINE:
            case POLYLINE:
                settings->deferredSetValue("widthGroupCurveFamily", *gw);
                // Also push the new width to the other two inactive curve-family tools
                // so their in-memory state stays in sync and doesn't revert on next load.
                for (ToolType siblingType : { CURVE, BSPLINE, POLYLINE }) {
                    if (BaseTool* sibling = getTool(siblingType)) {
                        if (sibling != target) {
                            sibling->setWidth(static_cast<qreal>(newWidth));
                            sibling->notifySettingsChanged();
                        }
                    }
                }
                break;
            case ERASER:
                settings->deferredSetValue("widthGroupEraser", *gw); break;
            default:
                settings->deferredSetValue("widthGroupPen", *gw); break;
            }
        }
    }
    // Mark settings dirty so saveSettingsIfDirty() will persist if a tool switch occurs
    target->notifySettingsChanged();
    // Defer persistence to endEditingProperties() to keep slider drags responsive
    emit toolPropertyChanged(target->type(), WIDTH);
}

void ToolManager::setFeather(float newFeather)
{
    if (std::isnan(newFeather) || newFeather < 0)
    {
        newFeather = 0.f;
    }

    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setFeather(static_cast<qreal>(newFeather));
    // Mark settings dirty so saveSettingsIfDirty() will persist if a tool switch occurs
    target->notifySettingsChanged();
    // Do not persist on every change; commit on release/endEditing
    emit toolPropertyChanged(target->type(), FEATHER);
}

void ToolManager::setUseFeather(bool usingFeather)
{
    int usingAA = currentTool()->properties.useAA;
    int value = propertySwitch(usingFeather, usingAA);

    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setAA(value);
    target->setUseFeather(usingFeather);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), USEFEATHER);
    emit toolPropertyChanged(target->type(), ANTI_ALIASING);
}

void ToolManager::setInvisibility(bool isInvisible)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setInvisibility(isInvisible);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), INVISIBILITY);
}

void ToolManager::setPreserveAlpha(bool isPreserveAlpha)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setPreserveAlpha(isPreserveAlpha);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), PRESERVEALPHA);
}

void ToolManager::setVectorMergeEnabled(bool isVectorMergeEnabled)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setVectorMergeEnabled(isVectorMergeEnabled);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), VECTORMERGE);
}

void ToolManager::setBezier(bool isBezierOn)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setBezier(isBezierOn);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), BEZIER);
}

void ToolManager::setClosedPath(bool isPathClosed)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setClosedPath(isPathClosed);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), CLOSEDPATH);
}

void ToolManager::setPressure(bool isPressureOn)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setPressure(isPressureOn);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), PRESSURE);
}

void ToolManager::setAA(int usingAA)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setAA(usingAA);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), ANTI_ALIASING);
}

void ToolManager::setFillMode(int mode)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setFillMode(mode);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), FILL_MODE);
}

void ToolManager::setStabilizerLevel(int level)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setStabilizerLevel(level);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), STABILIZATION);
}

void ToolManager::setPostCorrectionLevel(int level)
{
    // Only meaningful for Pen tool, but we update current tool properties for consistency
    if (auto* pen = qobject_cast<PenTool*>(getTool(PEN))) {
        pen->setPostCorrectionLevel(level);
        pen->saveSettings();
    } else {
        currentTool()->properties.postCorrectionLevel = qBound(0, level, 3);
    }
    emit toolPropertyChanged(PEN, POST_CORRECTION_LEVEL);
}

void ToolManager::setTolerance(int newTolerance)
{
    newTolerance = qMax(0, newTolerance);

    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setTolerance(newTolerance);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), TOLERANCE);
}

void ToolManager::setBucketColorToleranceEnabled(bool enabled)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setToleranceEnabled(enabled);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), USETOLERANCE);
}

void ToolManager::setBucketFillExpandEnabled(bool expandValue)
{
    Q_UNUSED(expandValue);
    // Expand Fill removed: no-op
}

void ToolManager::setBucketFillExpand(int expandValue)
{
    Q_UNUSED(expandValue);
    // Expand Fill removed: no-op
}

void ToolManager::setBucketFillReferenceMode(int referenceMode)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setFillReferenceMode(referenceMode);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), BUCKETFILLLAYERREFERENCEMODE);
}

void ToolManager::setUseFillContour(bool useFillContour)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setUseFillContour(useFillContour);
    target->saveSettings();
    emit toolPropertyChanged(target->type(), FILLCONTOUR);
}

void ToolManager::setShowSelectionInfo(bool b)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setShowSelectionInfo(b);
    target->saveSettings();
}

void ToolManager::setShowCameraPath(bool enabled)
{
    CameraTool* cameraTool = static_cast<CameraTool*>(getTool(CAMERA));
    cameraTool->setShowCameraPath(enabled);
    emit toolPropertyChanged(cameraTool->type(), CAMERAPATH);
}

void ToolManager::resetCameraPath()
{
    CameraTool* cameraTool = static_cast<CameraTool*>(getTool(CAMERA));
    cameraTool->resetCameraPath();
    emit toolPropertyChanged(cameraTool->type(), CAMERAPATH);
}

void ToolManager::resetCameraTransform(CameraFieldOption option)
{
    CameraTool* cameraTool = static_cast<CameraTool*>(getTool(CAMERA));
    cameraTool->resetTransform(option);
}

void ToolManager::setCameraPathDotColor(int dotColorNum)
{
    CameraTool* cameraTool = static_cast<CameraTool*>(getTool(CAMERA));
    cameraTool->setPathDotColorType(static_cast<DotColorType>(dotColorNum));
    emit toolPropertyChanged(cameraTool->type(), CAMERAPATH);
}

void ToolManager::setDrawBehind(bool drawBehind)
{
    // Update the global draw-behind state and propagate to all tools that support it
    mGlobalDrawBehind = drawBehind;
    for (auto* tool : mToolSetHash) {
        if (tool && tool->isPropertyEnabled(DRAW_BEHIND)) {
            tool->setDrawBehind(drawBehind);
            tool->saveSettings();
            emit toolPropertyChanged(tool->type(), DRAW_BEHIND);
        }
    }
    // Persist global draw-behind via SettingsManager (deferred write)
    SettingsManager::instance()->deferredSetValue("globalDrawBehind", mGlobalDrawBehind);
}

void ToolManager::setColorReplaceMode(bool enabled)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->setColorReplaceMode(enabled);
    // Reuse FILL_MODE notification family is not ideal; we could add a new enum if needed
    emit toolPropertyChanged(target->type(), FILL_MODE);
}

void ToolManager::setBucketSameColorOnly(bool enabled)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.bucketSameColorOnly = enabled;
    target->saveSettings();
    // Reuse FILL_MODE property channel to notify bucket UI; alternatively define a new enum
    emit toolPropertyChanged(target->type(), FILL_MODE);
}

void ToolManager::setBucketMultiKeyframeFill(bool enabled)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.bucketMultiKeyframeFill = enabled;
    target->saveSettings();
    // Notify via FILL_MODE channel for now to refresh bucket UI
    emit toolPropertyChanged(target->type(), FILL_MODE);
}


void ToolManager::setEraserLassoMode(bool enabled)
{
    // Apply to Eraser tool only
    if (auto* eraser = qobject_cast<EraserTool*>(getTool(ERASER))) {
        eraser->setEraserLassoMode(enabled);
        eraser->saveSettings();
        // Reuse PRESSURE property signal family for UI refresh, or define new if needed
        emit toolPropertyChanged(ERASER, PRESSURE);
    } else {
        currentTool()->setEraserLassoMode(enabled);
        emit toolPropertyChanged(currentTool()->type(), PRESSURE);
    }
}

void ToolManager::setBucketGapFillMode(int mode)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    // Clamp to supported range: 0=Off, 1=Low, 2=Medium, 3=High
    int clamped = qBound(0, mode, 3);
    target->properties.bucketGapFillMode = clamped;
    target->saveSettings();
    emit toolPropertyChanged(target->type(), BUCKET_GAP_FILL_MODE);
}

bool ToolManager::bucketReferenceModeIsCurrentLayer(int referenceMode) const
{
    return referenceMode == 0;
}

// Switches on/off two actions
// eg. if x = true, then y = false
int ToolManager::propertySwitch(bool condition, int tool)
{
    int value = 0;
    int newValue = 0;

    if (condition == true)
    {
        value = -1;
        newValue = mOldValue;
        mOldValue = tool;
    }
    else if (condition == false)
    {
        value = (newValue == 1) ? 1 : mOldValue;
    }
    return value;
}

void ToolManager::tabletSwitchToEraser()
{
    mTabletEraserTool = getTool(ERASER);

    if (!mTemporaryActive)
    {
        emit toolChanged(ERASER);
    }
}

void ToolManager::tabletRestorePrevTool()
{
    if (!mTemporaryActive && mTabletEraserTool != nullptr)
    {
        mTabletEraserTool = nullptr;
        emit toolChanged(currentTool()->type());
    }
}

bool ToolManager::beginTemporaryTool(ToolType eToolType, Qt::Key key)
{
    // If already in a temp tool, ignore to prevent conflicts
    if (mTemporaryActive)
    {
        qDebug() << "[ToolManager] beginTemporaryTool ignored; already active";
        return false;
    }
    // Do not allow while using tablet eraser override
    mPreviousTool = mCurrentTool;
    mTemporaryTool = getTool(eToolType);
    mTemporaryActive = true;
    mTemporaryToolKey = key;  // Store the key that activated this temp tool

    if (mTemporaryTool)
    {
        qDebug() << "[ToolManager] beginTemporaryTool" << BaseTool::TypeName(eToolType)
                 << "prev" << (mPreviousTool ? BaseTool::TypeName(mPreviousTool->type()) : QString("<none>"))
                 << "key" << key;
        mTemporaryTool->enteringThisTool();
        emit toolChanged(eToolType);
        return true;
    }
    qDebug() << "[ToolManager] beginTemporaryTool failed (null tool)";
    return false;
}

bool ToolManager::endTemporaryTool()
{
    if (!mTemporaryActive)
    {
        // benign
        return false;
    }

    if (mTemporaryTool)
    {
        qDebug() << "[ToolManager] endTemporaryTool; restoring prev"
                 << (mPreviousTool ? BaseTool::TypeName(mPreviousTool->type()) : QString("<none>"));
        // Reset any eyedropper-specific temporary modes when a temp tool ends
        if (mTemporaryTool->type() == ToolType::EYEDROPPER) {
            if (auto* dropper = qobject_cast<EyedropperTool*>(mTemporaryTool)) {
                dropper->setSampleOnly(false);
                dropper->setContinuousPick(false);
            }
        }
        mTemporaryTool->leavingThisTool();
    }
    mTemporaryTool = nullptr;
    mTemporaryActive = false;
    mTemporaryToolKey = Qt::Key_unknown;  // Clear the key

    // Restore previous tool and notify UI
    if (mPreviousTool)
    {
        emit toolChanged(mPreviousTool->type());
    }
    return true;
}

bool ToolManager::shouldEndTemporaryTool(Qt::Key key) const
{
    // Only end temporary tool if this is the key that activated it
    return mTemporaryActive && mTemporaryToolKey == key;
}

void ToolManager::setEyedropperSampleOnly(bool enabled)
{
    if (auto* dropper = qobject_cast<EyedropperTool*>(getTool(EYEDROPPER))) {
        dropper->setSampleOnly(enabled);
    }
}

void ToolManager::setEyedropperContinuousPick(bool enabled)
{
    if (auto* dropper = qobject_cast<EyedropperTool*>(getTool(EYEDROPPER))) {
        dropper->setContinuousPick(enabled);
    }
}

void ToolManager::setPenSmoothingMode(int mode)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.penSmoothingMode = static_cast<PenSmoothingMode>(mode);
    qDebug() << "[ToolManager] setPenSmoothingMode mode=" << mode << "tool=" << BaseTool::TypeName(target->type());
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), PEN_SMOOTHING_MODE);
}

void ToolManager::setWeightedDistance(double d)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.weightedDistance = qMax(0.0, d);
    qDebug() << "[ToolManager] setWeightedDistance d=" << d << "tool=" << BaseTool::TypeName(target->type())
             << "editTarget?" << (mPropertyEditTargetTool != nullptr);
    // Mark the tool settings dirty so saveSettingsIfDirty will persist them if needed.
    // Use public notifier to avoid touching protected internals.
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), WEIGHTED_DISTANCE);
}

void ToolManager::setWeightedStrokeEnding(double s)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.weightedStrokeEnding = qBound(0.0, s, 1.0);
    // No UI signal; legacy retained for completeness
}

void ToolManager::setWeightedSmoothPressure(bool b)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.weightedSmoothPressure = b;
    target->notifySettingsChanged();
}

void ToolManager::setWeightedScalableDistance(bool b)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.weightedScalableDistance = b;
    target->notifySettingsChanged();
}

void ToolManager::setStab2Distance(double d)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.stab2Distance = qMax(0.0, d);
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), STAB2_DISTANCE);
}

void ToolManager::setStab2Delay(double d)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.stab2Delay = qMax(0.0, d);
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), STAB2_DELAY);
}

void ToolManager::setStab2FinishLine(bool b)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.stab2FinishLine = b;
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), STAB2_FINISH_LINE);
}

void ToolManager::setStab2StabilizeSensors(bool b)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.stab2StabilizeSensors = b;
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), STAB2_STABILIZE_SENSORS);
}

void ToolManager::setStab2ScalableDistance(bool b)
{
    BaseTool* target = mPropertyEditTargetTool ? mPropertyEditTargetTool : currentTool();
    target->properties.stab2ScalableDistance = b;
    target->notifySettingsChanged();
    emit toolPropertyChanged(target->type(), STAB2_SCALABLE_DISTANCE);
}

void ToolManager::setCurveTaperStart(qreal value)
{
    const qreal clamped = qBound(0.0, value, 200.0);
    // Propagate to all three curve-family tools so they stay in sync regardless
    // of which one is currently active (mirrors setHybridSnapLevel pattern).
    for (ToolType type : { CURVE, BSPLINE, POLYLINE }) {
        if (BaseTool* tool = getTool(type)) {
            tool->properties.curveTaperStart = clamped;
            tool->notifySettingsChanged();
            emit toolPropertyChanged(type, CURVE_TAPER_START);
        }
    }
}

void ToolManager::setCurveTaperEnd(qreal value)
{
    const qreal clamped = qBound(0.0, value, 200.0);
    // Propagate to all three curve-family tools so they stay in sync regardless
    // of which one is currently active (mirrors setHybridSnapLevel pattern).
    for (ToolType type : { CURVE, BSPLINE, POLYLINE }) {
        if (BaseTool* tool = getTool(type)) {
            tool->properties.curveTaperEnd = clamped;
            tool->notifySettingsChanged();
            emit toolPropertyChanged(type, CURVE_TAPER_END);
        }
    }
}

void ToolManager::setHybridSnapLevel(int level)
{
    const int clamped = qBound(0, level, 3);
    auto applyToCurveFamily = [this, clamped](ToolType type) {
        if (BaseTool* tool = getTool(type)) {
            tool->properties.hybridSnapLevel = clamped;
            tool->notifySettingsChanged();
            emit toolPropertyChanged(type, HYBRID_SNAP_LEVEL);
        }
    };

    applyToCurveFamily(CURVE);
    applyToCurveFamily(BSPLINE);
    applyToCurveFamily(POLYLINE);
}

void ToolManager::beginEditingPropertiesForTool(ToolType toolType)
{
    BaseTool* target = getTool(toolType);
    if (mPropertyEditTargetTool == target) {
        return;
    }
    mPropertyEditTargetTool = target;
    qDebug() << "[ToolManager] beginEditingPropertiesForTool" << BaseTool::TypeName(toolType);
}

void ToolManager::endEditingProperties()
{
    if (mPropertyEditTargetTool) {
        // Commit persisted settings for the tool that was being edited
        qDebug() << "[ToolManager] endEditingProperties commit" << BaseTool::TypeName(mPropertyEditTargetTool->type())
                 << "weightedDistance=" << mPropertyEditTargetTool->properties.weightedDistance;
        mPropertyEditTargetTool->saveSettings();
        // Persist grouped width to settings once per interaction if applicable
        if (mUseGlobalWidth && mPropertyEditTargetTool->isPropertyEnabled(WIDTH)) {
            if (qreal* gw = groupWidthForTool(mPropertyEditTargetTool->type())) {
                *gw = mPropertyEditTargetTool->properties.width;
                SettingsManager* settings = SettingsManager::instance();
                switch (mPropertyEditTargetTool->type()) {
                case PEN:
                    settings->deferredSetValue("widthGroupPen", *gw); break;
                case PENCIL:
                    settings->deferredSetValue("widthGroupPencil", *gw); break;
                case CURVE:
                case BSPLINE:
                case POLYLINE:
                    settings->deferredSetValue("widthGroupCurveFamily", *gw); break;
                case ERASER:
                    settings->deferredSetValue("widthGroupEraser", *gw); break;
                default:
                    settings->deferredSetValue("widthGroupPen", *gw); break;
                }
            }
        }
        mPropertyEditTargetTool = nullptr;
    }
}
