/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "curvetool.h"

#include "managers/settingsmanager.h"
#include <QSettings>
#include <QPainterPath>

#include "../../../CUI/cosmicuimetrics.h"
#include "painterutils.h"
#include "editor.h"
#include "scribblearea.h"
#include "pointerevent.h"
#include "layermanager.h"
#include "colormanager.h"
#include "undoredomanager.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "layervector.h"
#include "vectorimage.h"
#include "graphics/bitmap/bitmapimage.h"
#include "viewmanager.h"
#include "graphics/vector/beziercurve.h"
#include "toolmanager.h"
#include "penciltool.h"

namespace {

static inline qreal bmDist2(const QPointF& a, const QPointF& b)
{
    const qreal dx = a.x() - b.x();
    const qreal dy = a.y() - b.y();
    return dx * dx + dy * dy;
}

static bool findHybridSnapCandidate(BitmapImage* bmp, const QPointF& canvasPos, qreal radius, QPointF& outPos)
{
    if (!bmp || radius <= 0.0) {
        return false;
    }

    const auto& strokes = bmp->hybridStrokes();
    if (strokes.isEmpty()) {
        return false;
    }

    const qreal radiusSq = radius * radius;
    qreal bestDistSq = radiusSq;
    bool found = false;

    for (const auto& stroke : strokes) {
        if (stroke.points.size() < 2 && stroke.intersectionPoints.isEmpty()) {
            continue;
        }

        QRectF expanded = stroke.bounds.adjusted(-radius, -radius, radius, radius);
        if (!expanded.contains(canvasPos)) {
            continue;
        }

        for (const QPointF& ip : stroke.intersectionPoints) {
            const qreal d2 = bmDist2(canvasPos, ip);
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                outPos = ip;
                found = true;
            }
        }

        const auto& pts = stroke.points;
        for (int i = 0; i + 1 < pts.size(); ++i) {
            const QPointF& a = pts[i];
            const QPointF& b = pts[i + 1];
            const qreal abx = b.x() - a.x();
            const qreal aby = b.y() - a.y();
            const qreal abLenSq = abx * abx + aby * aby;
            if (abLenSq <= 0.0) {
                continue;
            }

            const qreal apx = canvasPos.x() - a.x();
            const qreal apy = canvasPos.y() - a.y();
            qreal t = (apx * abx + apy * aby) / abLenSq;
            if (t < 0.0) t = 0.0;
            else if (t > 1.0) t = 1.0;

            const QPointF proj(a.x() + abx * t, a.y() + aby * t);
            const qreal d2 = bmDist2(canvasPos, proj);
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                outPos = proj;
                found = true;
            }
        }
    }

    return found;
}

static bool snapCanvasPosToHybrid(BitmapImage* bmp, const QPointF& canvasPos, int snapLevel, qreal viewScaleInv, QPointF& snapped)
{
    static const qreal snapPx[4] = { 0.0, 6.0, 10.0, 16.0 };
    const int level = qBound(0, snapLevel, 3);
    if (level <= 0) {
        return false;
    }
    const qreal radiusCanvas = snapPx[level] * viewScaleInv;
    return findHybridSnapCandidate(bmp, canvasPos, radiusCanvas, snapped);
}

// Sample actual points along a QPainterPath for accurate snap registration.
// Uses adaptive density: one sample per ~4px of path length, clamped to [8, 256].
static QVector<QPointF> samplePathPoints(const QPainterPath& path)
{
    const qreal len = path.length();
    if (len <= 0.0) return {};
    const int n = qBound(8, int(len / 4.0) + 1, 256);
    QVector<QPointF> pts;
    pts.reserve(n + 1);
    for (int i = 0; i <= n; ++i) {
        const qreal t = qreal(i) / qreal(n);
        pts.append(path.pointAtPercent(t));
    }
    return pts;
}

// Returns the active inner layer's BitmapImage for hybrid snap read/write.
// Must NOT use getBitmapImageAtFrame() — that returns the composite, which is
// deleted and recreated on every invalidateComposite() call, losing all stored
// hybrid strokes. The active inner layer image is stable across recomposites.
static BitmapImage* getSnapImage(LayerBitmap* bitmapLayer, int frameNumber)
{
    if (!bitmapLayer || !bitmapLayer->cellBank()) return nullptr;
    KeyFrame* kf = bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
    if (!kf) return nullptr;
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
    return bitmapLayer->cellBank()->getActiveInnerLayerImage(bkf->cellId());
}

} // namespace

CurveTool::CurveTool(QObject* parent) : BaseTool(parent)
{
}

ToolType CurveTool::type()
{
    return CURVE;
}

void CurveTool::setWidth(qreal w)
{
    if (!qFuzzyCompare(properties.width, w)) {
        properties.width = w;
        markSettingsDirty();
    }
}

void CurveTool::loadSettings()
{
    mPropertyEnabled[WIDTH] = true;
    mPropertyEnabled[ANTI_ALIASING] = true;
    mPropertyEnabled[DRAW_BEHIND] = true;
    mPropertyEnabled[CURVE_TAPER_START] = true;
    mPropertyEnabled[CURVE_TAPER_END] = true;
    mPropertyEnabled[HYBRID_SNAP_LEVEL] = true;

    SettingsManager* settings = SettingsManager::instance();
    // Prefer new grouped keys; fall back to legacy flat keys via SettingsManager
    properties.width = settings->getValue("Tool/Curve/Width", settings->getValue("curveWidth", 8.0)).toDouble();
    properties.feather = -1;
    properties.pressure = false;
    properties.invisibility = OFF;
    properties.preserveAlpha = OFF;
    properties.useAA = settings->getValue("unifiedAA", 1).toInt();
    // Draw Behind is global; keep legacy read for migration but actual value comes from manager
    properties.drawBehind = settings->getValue("Tool/Curve/DrawBehind", settings->getValue("curveDrawBehind", false)).toBool();
    
    // Load shared tapering settings (Curve and BSpline share these keys)
    // Fixed Pixels only: 0-200px range
    properties.curveTaperStart = settings->getValue("Tool/Curve/Shared/TaperStart", 0.0).toDouble();
    properties.curveTaperEnd = settings->getValue("Tool/Curve/Shared/TaperEnd", 0.0).toDouble();
    properties.hybridSnapLevel = settings->getValue("Tool/Curve/Shared/HybridSnapLevel", 0).toInt();
    
    // Clamp to fixed pixels range (0-200px)
    if (properties.curveTaperStart < 0.0) properties.curveTaperStart = 0.0;
    if (properties.curveTaperStart > 200.0) properties.curveTaperStart = 200.0;
    if (properties.curveTaperEnd < 0.0) properties.curveTaperEnd = 0.0;
    if (properties.curveTaperEnd > 200.0) properties.curveTaperEnd = 200.0;
    if (properties.hybridSnapLevel < 0) properties.hybridSnapLevel = 0;
    if (properties.hybridSnapLevel > 3) properties.hybridSnapLevel = 3;
    properties.stabilizerLevel = -1;
}

void CurveTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();
    settings->deferredSetValue("Tool/Curve/Width", properties.width);
    settings->deferredSetValue("unifiedAA", properties.useAA);
    // Draw Behind is global; do not persist per-tool anymore (compat no-op)
    settings->deferredSetValue("Tool/Curve/DrawBehind", properties.drawBehind);
    // Save shared tapering settings (Curve and BSpline share these keys)
    settings->deferredSetValue("Tool/Curve/Shared/TaperStart", properties.curveTaperStart);
    settings->deferredSetValue("Tool/Curve/Shared/TaperEnd", properties.curveTaperEnd);
    settings->deferredSetValue("Tool/Curve/Shared/HybridSnapLevel", properties.hybridSnapLevel);
    clearSettingsDirty();
}

QCursor CurveTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void CurveTool::resetToDefault()
{
    setWidth(8.0);
    setAA(1);
}

void CurveTool::clearToolData()
{
    if (mPhase == Phase::Idle) {
        return;
    }
    mPhase = Phase::Idle;
    mIsDragging = false;
    mLivePoint = QPointF();
    mPreviewPath = QPainterPath();
    mScribbleArea->clearDrawingBuffer();
    mScribbleArea->updateFrame();
    emit isActiveChanged(CURVE, false);
}

bool CurveTool::leavingThisTool()
{
    BaseTool::leavingThisTool();
    clearToolData();
    return true;
}

bool CurveTool::isActive() const
{
    return mPhase != Phase::Idle;
}

void CurveTool::pointerPressEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (!(layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR)) {
        event->accept();
        return;
    }

    if (mPhase == Phase::AdjustCurve) {
        // Confirm on click
        commitCurve();
        event->accept();
        return;
    }

    // Start dragging a temporary line
    mScribbleArea->handleDrawingOnEmptyFrame();

    if (layer->type() == Layer::VECTOR) {
        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
        Q_CHECK_PTR(vectorImage);
        vectorImage->deselectAll();
        if (mScribbleArea->makeInvisible() && !mEditor->preference()->isOn(SETTING::INVISIBLE_LINES)) {
            mScribbleArea->toggleThinLines();
        }
    }

    QPointF startPos = event->canvasPos();

    if (layer->type() == Layer::BITMAP && properties.hybridSnapLevel > 0) {
        auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
        BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
        if (cellImg && mEditor && mEditor->view()) {
            QPointF snapped;
            if (snapCanvasPosToHybrid(cellImg, startPos, properties.hybridSnapLevel, mEditor->view()->getScaleInversed(), snapped)) {
                startPos = snapped;
            }
        }
    }

    mStartPoint = mEndPoint = mControlPoint = QPointF();
    mStartPoint = startPos;
    mPhase = Phase::DragLine;
    emit isActiveChanged(CURVE, true);

    event->accept();
}

void CurveTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* layer = mEditor->layers()->currentLayer();
    BitmapImage* cellImg = nullptr;
    if (layer && layer->type() == Layer::BITMAP && properties.hybridSnapLevel > 0) {
        auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
        cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
    }

    if (mPhase == Phase::DragLine) {
        mIsDragging = true;
        QPointF current = event->canvasPos();
        if (cellImg && mEditor && mEditor->view()) {
            QPointF snapped;
            if (snapCanvasPosToHybrid(cellImg, current, properties.hybridSnapLevel, mEditor->view()->getScaleInversed(), snapped)) {
                current = snapped;
            }
        }
        drawTemporaryLineTo(current);
    } else if (mPhase == Phase::AdjustCurve) {
        mIsDragging = true;
        QPointF current = event->canvasPos();
        if (cellImg && mEditor && mEditor->view()) {
            QPointF snapped;
            if (snapCanvasPosToHybrid(cellImg, current, properties.hybridSnapLevel, mEditor->view()->getScaleInversed(), snapped)) {
                current = snapped;
            }
        }
        mControlPoint = current;
        drawTemporaryCurve(mControlPoint);
    }
    event->accept();
}

void CurveTool::pointerReleaseEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }

    // Always reset drag state on release
    mIsDragging = false;

    if (mPhase == Phase::DragLine) {
        QPointF endPos = event->canvasPos();

        Layer* layer = mEditor->layers()->currentLayer();
        if (layer && layer->type() == Layer::BITMAP && properties.hybridSnapLevel > 0) {
            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
            if (cellImg && mEditor && mEditor->view()) {
                QPointF snapped;
                if (snapCanvasPosToHybrid(cellImg, endPos, properties.hybridSnapLevel, mEditor->view()->getScaleInversed(), snapped)) {
                    endPos = snapped;
                }
            }
        }

        mEndPoint = endPos;
        // Enter adjustment phase with midpoint as initial control point
        mControlPoint = (mStartPoint + mEndPoint) / 2.0;
        mPhase = Phase::AdjustCurve;
        drawTemporaryCurve(mControlPoint);
    } else if (mPhase == Phase::AdjustCurve) {
        // Update preview to full quality after adjustment
        drawTemporaryCurve(mControlPoint);
    }

    event->accept();
}

bool CurveTool::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Alt:
        {
            Layer* layer = mEditor->layers()->currentLayer();
            if (layer && layer->type() == Layer::BITMAP) {
                if (mEditor->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_Alt))
                {
                    return true;
                }
            }
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (mPhase == Phase::AdjustCurve) {
            mIsDragging = false; // Reset before commit
            commitCurve();
            return true;
        }
        break;
    case Qt::Key_Escape:
        if (mPhase != Phase::Idle) {
            mIsDragging = false; // Reset before clear
            clearToolData();
            return true;
        }
        break;
    default:
        break;
    }
    return BaseTool::keyPressEvent(event);
}

void CurveTool::drawTemporaryLineTo(const QPointF& endPoint)
{
    mLivePoint = endPoint;
    // Vector preview: store path only
    mPreviewPath = QPainterPath(mStartPoint);
    mPreviewPath.lineTo(endPoint);
    
    // Trigger widget repaint to show vector overlay
    if (mScribbleArea) {
        mScribbleArea->update();
    }
}

void CurveTool::drawTemporaryCurve(const QPointF& controlPoint)
{
    // Vector preview: store quadratic curve path
    mPreviewPath = QPainterPath(mStartPoint);
    mPreviewPath.quadTo(controlPoint, mEndPoint);
    
    // Trigger widget repaint to show vector overlay
    if (mScribbleArea) {
        mScribbleArea->update();
    }
}

void CurveTool::paintOverlay(QPainter& painter)
{
    if (mPhase == Phase::Idle) return;
    if (!mEditor || !mEditor->view()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Switch to screen space so all sizes are zoom-independent,
    // matching how BSplineTool::paint() and PolylineTool::paint() work.
    painter.setWorldMatrixEnabled(false);

    auto toScreen = [&](const QPointF& p) {
        return mEditor->view()->mapCanvasToScreen(p);
    };

    // Preview path — map each element to screen space
    if (!mPreviewPath.isEmpty()) {
        QPainterPath screenPath;
        const int n = mPreviewPath.elementCount();
        for (int i = 0; i < n; ++i) {
            const QPainterPath::Element e = mPreviewPath.elementAt(i);
            const QPointF sp = toScreen(QPointF(e.x, e.y));
            if (e.type == QPainterPath::MoveToElement)
                screenPath.moveTo(sp);
            else if (e.type == QPainterPath::LineToElement)
                screenPath.lineTo(sp);
            else if (e.type == QPainterPath::CurveToElement && i + 2 < n) {
                const QPainterPath::Element d1 = mPreviewPath.elementAt(i + 1);
                const QPainterPath::Element d2 = mPreviewPath.elementAt(i + 2);
                screenPath.cubicTo(sp,
                                   toScreen(QPointF(d1.x, d1.y)),
                                   toScreen(QPointF(d2.x, d2.y)));
                i += 2;
            }
        }
        ToolOverlay::drawPreviewPath(painter, screenPath);
    }

    // Control point dots
    if (mPhase == Phase::DragLine || mPhase == Phase::AdjustCurve)
        ToolOverlay::drawControlDotScreen(painter, toScreen(mStartPoint));

    if (mPhase == Phase::DragLine && !mLivePoint.isNull()) {
        const QPointF mid = (mStartPoint + mLivePoint) / 2.0;
        ToolOverlay::drawControlDotScreen(painter, toScreen(mLivePoint));
        ToolOverlay::drawControlDotScreen(painter, toScreen(mid));
    }

    if (mPhase == Phase::AdjustCurve) {
        // Handle lines in screen space
        ToolOverlay::drawHandleLine(painter, toScreen(mControlPoint), toScreen(mStartPoint));
        ToolOverlay::drawHandleLine(painter, toScreen(mControlPoint), toScreen(mEndPoint));
        ToolOverlay::drawControlDotScreen(painter, toScreen(mEndPoint));
        ToolOverlay::drawControlDotScreen(painter, toScreen(mControlPoint));
    }

    painter.restore();
}

void CurveTool::commitCurve()
{
    if (mPhase == Phase::Idle) { return; }

    const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::VECTOR) {
        // Convert quadratic (P0, C, P2) to cubic
        QPointF p0 = mStartPoint;
        QPointF p2 = mEndPoint;
        QPointF c = mControlPoint;
        QPointF c1 = p0 + (2.0/3.0) * (c - p0);
        QPointF c2 = p2 + (2.0/3.0) * (c - p2);

        BezierCurve curve;
        curve.setOrigin(p0);
        curve.appendCubic(c1, c2, p2, 1.0);
        if (mScribbleArea->makeInvisible()) {
            curve.setWidth(0);
        } else {
            curve.setWidth(properties.width);
        }
        curve.setVariableWidth(false);
        curve.setInvisibility(mScribbleArea->makeInvisible());
        curve.setColorNumber(mEditor->color()->frontColorNumber());

        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
        if (vectorImage != nullptr) {
            if (properties.drawBehind) {
                vectorImage->insertCurve(0, curve, mEditor->view()->scaling(), true);
            } else {
                vectorImage->addCurve(curve, mEditor->view()->scaling());
            }
        }

        // Request raster re-render from newly added vector stroke
        if (mScribbleArea) {
            mScribbleArea->hybridStack()->rerenderRasterFromVectors(mEditor->currentFrame());
        }
    } else if (layer->type() == Layer::BITMAP) {
        const qreal effectiveWidth = [&]() -> qreal {
            qreal w = properties.width;
            if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
                const qreal gw = mEditor->tools()->globalWidth();
                if (gw > 0.0) {
                    w = gw;
                }
            }
            return qMax<qreal>(1.0, w);
        }();

        const bool finalLineActive = []() -> bool {
            QSettings s(PENCIL2D, PENCIL2D);
            s.beginGroup("PencilBrushFactory");
            const bool active = s.value("FinalLineActive", false).toBool();
            const QString presetId = s.value("ActivePresetId", QString()).toString();
            s.endGroup();
            return active || (presetId == QStringLiteral("__final_line__"));
        }();

        if (finalLineActive) {
            const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                       ? mEditor->tools()->strokeColorOverride()
                                       : mEditor->color()->frontColor();

            QPainterPath path(mStartPoint);
            path.quadTo(mControlPoint, mEndPoint);

            QPen pen(strokeColor);
            pen.setWidthF(effectiveWidth);
            pen.setCapStyle(Qt::SquareCap);
            pen.setJoinStyle(Qt::MiterJoin);

            // Final Line: strictly pixelated (no antialiasing)
            if (mScribbleArea) {
                const bool useTapering = (properties.curveTaperStart > 0.0 || properties.curveTaperEnd > 0.0);
                if (useTapering) {
                    mScribbleArea->drawTaperedCurve(path, pen, true, properties.curveTaperStart, properties.curveTaperEnd, false, 0);
                } else {
                    mScribbleArea->drawPathCanvas(path, pen, Qt::NoBrush, QPainter::CompositionMode_Source, false);
                }
                mScribbleArea->commitCurrentSegmentToLayer();
            }

            // Hybrid metadata: sample actual curve points so snap targets the visible stroke
            if (mEditor && mEditor->layers()) {
                auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
                BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
                if (cellImg) {
                    QPainterPath curvePath(mStartPoint);
                    curvePath.quadTo(mControlPoint, mEndPoint);
                    cellImg->addHybridStroke(samplePathPoints(curvePath), effectiveWidth);
                }
            }

            if (mScribbleArea) {
                mScribbleArea->endStroke();
            }
            mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
            if (saveState) { mEditor->undoRedo()->record(saveState, typeName()); }
            clearToolData();
            return;
        }

        if (mEditor && mEditor->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(mEditor->tools()->getTool(PENCIL))) {
                QPainterPath path(mStartPoint);
                path.quadTo(mControlPoint, mEndPoint);
                if (pencil->renderStrokeAlongPathWithBaseWidth(path, effectiveWidth, properties.curveTaperStart, properties.curveTaperEnd)) {
                    if (mEditor && mEditor->layers()) {
                        auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
                        BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
                        if (cellImg) {
                            cellImg->addHybridStroke(samplePathPoints(path), effectiveWidth);
                        }
                    }

                    mScribbleArea->endStroke();
                    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
                    if (saveState) { mEditor->undoRedo()->record(saveState, typeName()); }
                    clearToolData();
                    return;
                }
            }
        }

        const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                   ? mEditor->tools()->strokeColorOverride()
                                   : mEditor->color()->frontColor();
        QPen pen(strokeColor, properties.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QPainterPath path(mStartPoint);
        path.quadTo(mControlPoint, mEndPoint);
        const bool useTapering = (properties.curveTaperStart > 0.0 || properties.curveTaperEnd > 0.0);
        if (useTapering) {
            // Always use mode 0 (FixedPixels) since percentage mode is removed
            mScribbleArea->drawTaperedCurve(path, pen, true, properties.curveTaperStart, properties.curveTaperEnd, false, 0);
            mScribbleArea->endStroke();
        } else {
            // Antialiasing disabled
            mScribbleArea->drawPolyline(path, pen, false);
            mScribbleArea->endStroke();
        }

        // Sample actual curve points so snap targets the visible stroke, not control points
        if (mEditor && mEditor->layers()) {
            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
            if (cellImg) {
                cellImg->addHybridStroke(samplePathPoints(path), properties.width);
            }
        }
    }

    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    // Record undo entry for the committed curve
    if (saveState) { mEditor->undoRedo()->record(saveState, typeName()); }

    clearToolData();
}