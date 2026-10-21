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
#include "penciltool.h"

#include <QPixmap>
#include <QSettings>
#include <QCursor>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <cmath>
#include <limits>

#include "vectorimage.h"
#include "layervector.h"
#include "colormanager.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "graphics/bitmap/bitmapimage.h"
#include "viewmanager.h"
#include "undoredomanager.h"
#include "selectionmanager.h"
#include "editor.h"
#include "scribblearea.h"
#include "blitrect.h"
#include "pointerevent.h"
#include "toolmanager.h"
#include "tile.h"
#include "object.h"
#include "managers/settingsmanager.h"
#include "util/mathutils.h"
#include "bitmapimage.h"
#include "util/pencilsettings.h"

// Initialize static members
QSet<quint64> PencilTool::sInFlightPrerenderKeys;
QImage PencilTool::sTipMaskHardest;
QImage PencilTool::sTipMaskHard;
QImage PencilTool::sTipMaskMedium;
QImage PencilTool::sTipMaskSoft;
bool   PencilTool::sTipMasksReady = false;
constexpr int PencilTool::kRotationDeckSize;

void PencilTool::initTipMasks()
{
    if (sTipMasksReady) return;
    // Tip PNGs are baked at compile time by tools/generate_brush_tips and embedded
    // via app.qrc — just load them, no generation needed at runtime.
    sTipMaskHardest = QImage(":/BrushTips/CosmicDefaultTip_Hardest.png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    sTipMaskHard    = QImage(":/BrushTips/CosmicDefaultTip_Hard.png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    sTipMaskMedium  = QImage(":/BrushTips/CosmicDefaultTip_Medium.png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    sTipMaskSoft    = QImage(":/BrushTips/CosmicDefaultTip_Soft.png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    sTipMasksReady  = true;
}

PencilTool::PencilTool(QObject* parent) : StrokeTool(parent)
{
    // Initialize dab cache (cache up to 200 pre-rendered dabs for better hit rates)
    // Each QImage* counts as 1 cost unit. 200 dabs ~= 2-10MB depending on brush size.
    mDabCache.setMaxCost(50000);
}

QImage PencilTool::renderPresetPreviewStroke(const PreviewPreset& preset,
                                            const QSize& pixelSize,
                                            quint32 seed)
{
    const int w = pixelSize.width();
    const int h = pixelSize.height();
    if (w <= 0 || h <= 0) {
        return QImage();
    }

    PencilTool tool(nullptr);

    // Configure tool properties for preview rendering.
    tool.properties.useAA = 0;
    tool.properties.pressure = true;

    // Map preset parameters into the Pencil tool state.
    tool.setUserSpacingMult(preset.spacingMult);
    tool.setScatterAmount(preset.scatter);
    tool.setRotationJitterRangeDegrees(preset.rotationDeg);
    tool.setSizeJitterAmount(preset.sizeJitter);
    tool.setColorJitterAmount(preset.colorJitter);
    tool.setTextureTileScale(preset.textureTileScale);
    tool.setTextureContrastPercent(preset.textureContrastPercent);
    tool.setThicknessPressureCurve(preset.thicknessCurvePoints);
    tool.setOpacityPressureCurve(preset.opacityCurvePoints);
    tool.setThicknessCurveEnabled(preset.thicknessCurveEnabled);
    tool.setOpacityCurveEnabled(preset.opacityCurveEnabled);

    // Load/apply texture.
    {
        QImage src;
        if (!preset.textureBytes.isEmpty()) {
            src.loadFromData(preset.textureBytes);
        } else {
            src.load(QLatin1String(":/BrushFiles/P1.png"));
        }
        if (!src.isNull()) {
            (void)tool.applyTextureImage(src);
        }
    }

    // Apply tip shape / custom tip mask.
    {
        using TipMode = PreviewPreset::TipMode;
        if (preset.tipMode == TipMode::Soft) {
            tool.setTipShape(CosmicTipShape::Soft);
        } else if (preset.tipMode == TipMode::Custom && !preset.tipBytes.isEmpty()) {
            tool.setTipShape(CosmicTipShape::Custom);
            tool.setCustomTipFromData(preset.tipBytes);
            tool.setTipContrastPercent(preset.tipContrastPercent);
        } else {
            tool.setTipShape(CosmicTipShape::Hard);
        }
    }

    // Deterministic stroke texture seed (mirrors per-stroke init in pointerPressEvent).
    {
        QRandomGenerator gen(seed);
        static const qreal kStrokeTextureAngles[] = { 0.0, 90.0, 180.0, 240.0 };
        const int idx = int(gen.bounded(4u));
        tool.mStrokeTextureRotationDeg = kStrokeTextureAngles[idx];
        tool.mStrokeTextureAnchorCanvas = QPointF(0.0, 0.0);
        if (tool.mTextureLoaded && !tool.mTextureSource.isNull()) {
            const int tw = tool.mTextureSource.width();
            const int th = tool.mTextureSource.height();
            if (tw > 0 && th > 0) {
                tool.mStrokeTextureOffsetCanvas.setX(gen.bounded(tw));
                tool.mStrokeTextureOffsetCanvas.setY(gen.bounded(th));
            }
        }
    }

    // Choose a preview stroke width relative to the available height.
    // Keep it conservative so it fits in the tile and resembles typical pencil presets.
    const qreal baseWidth = qBound<qreal>(1.0, h * 0.30, 64.0);
    tool.properties.width = baseWidth;

    // Build a smooth S-curve across the preview image and render it by composing
    // PencilTool's own ribbon segment bitmaps.
    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    const qreal padX = qMax<qreal>(2.0, w * 0.06);
    const qreal x0 = padX;
    const qreal x1 = qMax<qreal>(x0 + 1.0, w - padX);
    const qreal midY = h * 0.52;
    const qreal curveAmp = h * 0.16;

    const qreal spacingMult = qBound<qreal>(0.05, preset.spacingMult, 10.0);
    const qreal approxSpacing = qMax<qreal>(0.8, baseWidth * 0.45 * spacingMult);
    const int segCount = qBound(40, int(qCeil((x1 - x0) / approxSpacing)), 260);

    auto evalCurve01Local = [](const QVector<QPointF>& pts, qreal x) -> qreal {
        if (pts.isEmpty()) {
            return x;
        }
        x = qBound<qreal>(0.0, x, 1.0);
        if (pts.size() == 1) {
            return qBound<qreal>(0.0, pts[0].y(), 1.0);
        }
        int right = 0;
        while (right < pts.size() && pts[right].x() < x) {
            ++right;
        }
        if (right <= 0) {
            return qBound<qreal>(0.0, pts.front().y(), 1.0);
        }
        if (right >= pts.size()) {
            return qBound<qreal>(0.0, pts.back().y(), 1.0);
        }
        const QPointF a = pts[right - 1];
        const QPointF b = pts[right];
        const qreal dx = (b.x() - a.x());
        if (qFuzzyIsNull(dx)) {
            return qBound<qreal>(0.0, b.y(), 1.0);
        }
        const qreal t = qBound<qreal>(0.0, (x - a.x()) / dx, 1.0);
        return qBound<qreal>(0.0, a.y() + (b.y() - a.y()) * t, 1.0);
    };

    // --- Pass 1: compute all dab positions to determine actual Y extents ---
    // We need to know the full scatter-jittered Y range before drawing so we can
    // normalise the stroke to always fit within the preview rect.
    struct DabInfo {
        QPointF pos;
        qreal   width;
        qreal   alpha;
        qreal   rotationJitterDeg;
    };
    QVector<DabInfo> dabs;
    dabs.reserve(segCount);

    // Save RNG state by re-seeding with the same seed — we'll replay it in pass 2.
    QRandomGenerator genPass1(seed ^ 0xA53C9E21u);

    qreal yMin = std::numeric_limits<qreal>::max();
    qreal yMax = std::numeric_limits<qreal>::lowest();
    qreal maxHalfWidth = 0.0;

    for (int i = 0; i < segCount; ++i) {
        const qreal t = qreal(i) / qMax(1, segCount - 1);
        const qreal tri = (t < 0.5) ? (t * 2.0) : (2.0 - 2.0 * t);
        const qreal tipTaper = qBound<qreal>(0.0, tri, 1.0);

        qreal thicknessCurve = 1.0;
        if (preset.thicknessCurveEnabled && !preset.thicknessCurvePoints.isEmpty()) {
            thicknessCurve = evalCurve01Local(preset.thicknessCurvePoints, t);
        }
        qreal opacityCurve = 1.0;
        if (preset.opacityCurveEnabled && !preset.opacityCurvePoints.isEmpty()) {
            opacityCurve = evalCurve01Local(preset.opacityCurvePoints, t);
        }

        qreal width = baseWidth;
        width *= (0.10 + 0.90 * tipTaper);
        width *= qBound<qreal>(0.35, 0.35 + 1.45 * thicknessCurve, 1.8);
        const qreal sizeJitAmt = qBound<qreal>(0.0, preset.sizeJitter, 0.5);
        if (sizeJitAmt > 0.0) {
            const qreal amount = 0.25 * sizeJitAmt;
            const qreal sizeJit = (genPass1.generateDouble() * 2.0 - 1.0) * amount;
            width *= (1.0 + sizeJit);
        }
        width = qMax<qreal>(0.9, width);

        qreal alpha = tipTaper;
        alpha *= qBound<qreal>(0.0, opacityCurve, 1.0);
        alpha = qBound<qreal>(0.0, alpha, 1.0);

        const qreal x = x0 + t * (x1 - x0);
        const qreal baseY = midY + curveAmp * qSin(t * 3.14159265 * 2.0);
        const qreal scatter = qBound<qreal>(0.0, preset.scatter, 3.0);
        const qreal maxJitterY = scatter * h * 0.28;
        const qreal jitterY = (maxJitterY > 0.0) ? ((genPass1.generateDouble() * 2.0 - 1.0) * maxJitterY) : 0.0;
        const qreal y = baseY + jitterY;

        qreal rotationJitterDeg = 0.0;
        const qreal rotRange = qMax<qreal>(0.0, preset.rotationDeg);
        if (rotRange > 0.0) {
            rotationJitterDeg = (genPass1.generateDouble() * 2.0 - 1.0) * rotRange;
        }

        if (alpha > 0.002) {
            yMin = qMin(yMin, y);
            yMax = qMax(yMax, y);
            maxHalfWidth = qMax(maxHalfWidth, width * 0.5);
        }

        dabs.append({ QPointF(x, y), width, alpha, rotationJitterDeg });
    }

    // --- Compute Y normalisation transform ---
    // Map the stroke's actual Y extent (including brush radius) into the preview rect
    // with a small padding margin so dabs at the extremes aren't clipped.
    const qreal padY = qMax<qreal>(2.0, h * 0.06);
    const qreal availTop    = padY + maxHalfWidth;
    const qreal availBottom = h - padY - maxHalfWidth;
    const qreal availH = qMax<qreal>(1.0, availBottom - availTop);

    qreal yOffset = 0.0;
    qreal yScale  = 1.0;

    if (yMin < std::numeric_limits<qreal>::max() && yMax > std::numeric_limits<qreal>::lowest()) {
        const qreal strokeH = yMax - yMin;
        if (strokeH > 1.0) {
            // Scale so the full jittered range fits in the available height.
            yScale  = qMin<qreal>(1.0, availH / strokeH);
            // Centre the scaled stroke vertically.
            const qreal scaledMid = (yMin + yMax) * 0.5 * yScale;
            yOffset = (h * 0.5) - scaledMid;
        } else {
            // Degenerate (no scatter): just centre it.
            yOffset = (h * 0.5) - (yMin + yMax) * 0.5;
        }
    }

    // --- Pass 2: render using normalised positions ---
    QPointF prev;
    bool hasPrev = false;

    for (const DabInfo& dab : dabs) {
        if (dab.alpha <= 0.002) {
            // Still need to advance prev tracking for connectivity.
            if (!hasPrev) {
                prev = QPointF(dab.pos.x(), dab.pos.y() * yScale + yOffset);
                hasPrev = true;
            }
            continue;
        }

        const QPointF cur(dab.pos.x(), dab.pos.y() * yScale + yOffset);

        if (!hasPrev) {
            prev = cur;
            hasPrev = true;
            continue;
        }

        QRectF segBounds;
        // Use white: preview is masked by texture alpha; color is irrelevant here.
        QImage seg = tool.renderTexturedRibbonSegment(prev, cur, dab.width, dab.alpha, Qt::white, dab.rotationJitterDeg, &segBounds);
        if (!seg.isNull() && segBounds.isValid() && !segBounds.isEmpty()) {
            QPainter p(&out);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.drawImage(segBounds.topLeft(), seg);
            p.end();
        }
        prev = cur;
    }

    return out;
}

void PencilTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyEnabled[WIDTH] = true;
    // Pencil supports a pressure toggle, but pressure only influences opacity now
    mPropertyEnabled[PRESSURE] = true;
    mPropertyEnabled[ANTI_ALIASING] = true;
    mPropertyEnabled[STABILIZATION] = true;
    mPropertyEnabled[DRAW_BEHIND] = true;

    SettingsManager* settings = SettingsManager::instance();

    properties.width = settings->getValue("pencilWidth", 8.0).toDouble();
    // Always enable pressure for the pencil tool; legacy prefs are ignored so
    // pen pressure can drive stroke thickness consistently.
    properties.pressure = true;
    properties.useAA = 0;
    properties.drawBehind = settings->getValue("pencilDrawBehind", false).toBool();

    // Load dual pressure curves for pencil
    // Opacity curve
    QVariant vOpacity = settings->getValue("pencilOpacityCurvePoints", QVariant());
    mOpacityPressureCurve.clear();
    if (vOpacity.isValid() && vOpacity.canConvert<QVariantList>()) {
        QVariantList list = vOpacity.toList();
        for (int i = 0; i + 1 < list.size(); i += 2) {
            qreal x = list[i].toDouble(); qreal y = list[i+1].toDouble();
            mOpacityPressureCurve.append(QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0)));
        }
        if (mOpacityPressureCurve.isEmpty()) {
            mOpacityPressureCurve.append(QPointF(0.5, 0.5));
        }
    } else {
        // Default: linear-ish opacity curve (sketch-style)
        mOpacityPressureCurve.append(QPointF(0.5, 0.5));
    }
    
    // Thickness curve
    QVariant vThickness = settings->getValue("pencilThicknessCurvePoints", QVariant());
    mThicknessPressureCurve.clear();
    if (vThickness.isValid() && vThickness.canConvert<QVariantList>()) {
        QVariantList list = vThickness.toList();
        for (int i = 0; i + 1 < list.size(); i += 2) {
            qreal x = list[i].toDouble(); qreal y = list[i+1].toDouble();
            mThicknessPressureCurve.append(QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0)));
        }
        if (mThicknessPressureCurve.isEmpty()) {
            mThicknessPressureCurve.append(QPointF(0.5, 0.5));
        }
    } else {
        // Default: linear thickness curve
        mThicknessPressureCurve.append(QPointF(0.5, 0.5));
    }

    mOpacityCurveEnabled = settings->getValue("pencilOpacityCurveEnabled", true).toBool();
    mThicknessCurveEnabled = settings->getValue("pencilThicknessCurveEnabled", true).toBool();

    // Initialize LUT caches with loaded curves
    mOpacityCurveLUT.setCurvePoints(mOpacityPressureCurve);
    mThicknessCurveLUT.setCurvePoints(mThicknessPressureCurve);

    // Load pressure pipeline settings
    properties.pressureSmoothingAmount = settings->getValue("pencilPressureSmoothingAmount", 0.3).toDouble();
    properties.pressureTaperingStart = settings->getValue("pencilPressureTaperingStart", 0.0).toDouble();
    properties.pressureTaperingEnd = settings->getValue("pencilPressureTaperingEnd", 0.5).toDouble();

    // Krita-like stabilization modes (Delayed/String). Keep Pencil settings independent
    // from the Pen tool.
    properties.penSmoothingMode = static_cast<PenSmoothingMode>(
        settings->getValue("pencilSmoothingMode", (int)PenSmoothingMode::WEIGHTED).toInt());
    if (properties.penSmoothingMode == PenSmoothingMode::LEGACY ||
        static_cast<int>(properties.penSmoothingMode) == 3 /* was LACE, now removed */) {
        properties.penSmoothingMode = PenSmoothingMode::WEIGHTED;
    }

    // Backward compatibility: pencilStabilizer (legacy) is interpreted as weightedDistance.
    const double legacyWeighted = settings->getValue("pencilStabilizer", 0.0).toDouble();
    properties.weightedDistance = settings->getValue("pencilWeightedDistance", legacyWeighted).toDouble();
    properties.stab2Distance = settings->getValue("pencilStab2Distance", 16.0).toDouble();
    properties.stab2Delay = settings->getValue("pencilStab2Delay", 4.0).toDouble();

    // Load pencil opacity (default 0.8)
    mPencilOpacity = settings->getValue("pencilOpacity", 0.8).toDouble();
    mPencilOpacity = qBound(0.0, mPencilOpacity, 1.0);

    mUserSpacingMult = settings->getValue("pencilUserSpacingMult", 1.0).toDouble();
    mUserSpacingMult = qBound<qreal>(0.05, mUserSpacingMult, 10.0);
    mScatterAmount = settings->getValue("pencilScatterAmount", 0.15).toDouble();
    mScatterAmount = qBound<qreal>(0.0, mScatterAmount, 3.0);
    mRotationJitterRangeDegrees = settings->getValue("pencilRotationJitterRange", 2.2).toDouble();
    mRotationJitterRangeDegrees = qBound<qreal>(0.0, mRotationJitterRangeDegrees, 40.0);
    mSizeJitterAmount = settings->getValue("pencilSizeJitterAmount", 0.10).toDouble();
    mSizeJitterAmount = qBound<qreal>(0.0, mSizeJitterAmount, 0.5);
    mColorJitterAmount = settings->getValue("pencilColorJitterAmount", 0.0).toDouble();
    mColorJitterAmount = qBound<qreal>(0.0, mColorJitterAmount, 1.0);

    mTextureTileScale = settings->getValue("pencilTextureTileScale", 12.0).toDouble();
    mTextureTileScale = qBound<qreal>(0.25, mTextureTileScale, 16.0);
    mTextureMaskGamma = settings->getValue("pencilTextureMaskGamma", 2.2).toDouble();
    mTextureMaskGamma = qBound<qreal>(0.2, mTextureMaskGamma, 4.0);
    mTextureMaskCutoff = settings->getValue("pencilTextureMaskCutoff", 0.25).toDouble();
    mTextureMaskCutoff = qBound<qreal>(0.0, mTextureMaskCutoff, 0.95);

    // Progressive commit behavior for direct-to-bitmap drawing
    {
        QSettings qs(PENCIL2D, PENCIL2D);
        mProgressiveCommitEnabled = qs.value(PENCIL_PROGRESSIVE_COMMIT_ENABLED, true).toBool();
        mProgressiveCommitDabThreshold = qs.value(PENCIL_PROGRESSIVE_COMMIT_DAB_THRESHOLD, 50).toInt();
        mProgressiveCommitDistanceThreshold = qs.value(PENCIL_PROGRESSIVE_COMMIT_DISTANCE_THRESHOLD, 200.0).toDouble();
        // Clamp to sensible ranges
        mProgressiveCommitDabThreshold = qMax(1, mProgressiveCommitDabThreshold);
        mProgressiveCommitDistanceThreshold = qMax<qreal>(10.0, mProgressiveCommitDistanceThreshold);
    }

    // Load texture on first use
    if (!mTextureLoaded) {
        loadTexture();
    }
}

void PencilTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();

    settings->deferredSetValue("pencilWidth", properties.width);
    settings->deferredSetValue("pencilPressure", properties.pressure);
    settings->deferredSetValue("pencilAA", properties.useAA);
    settings->deferredSetValue("pencilDrawBehind", properties.drawBehind);

    // Save opacity curve
    QVariantList flatOpacity;
    for (const QPointF& p : mOpacityPressureCurve) {
        flatOpacity.append(p.x()); flatOpacity.append(p.y());
    }
    settings->deferredSetValue("pencilOpacityCurvePoints", flatOpacity);
    settings->deferredSetValue("pencilOpacityCurveEnabled", mOpacityCurveEnabled);

    // Save thickness curve
    QVariantList flatThickness;
    for (const QPointF& p : mThicknessPressureCurve) {
        flatThickness.append(p.x()); flatThickness.append(p.y());
    }
    settings->deferredSetValue("pencilThicknessCurvePoints", flatThickness);
    settings->deferredSetValue("pencilThicknessCurveEnabled", mThicknessCurveEnabled);

    // Save pressure pipeline settings
    settings->deferredSetValue("pencilPressureSmoothingAmount", properties.pressureSmoothingAmount);
    settings->deferredSetValue("pencilPressureTaperingStart", properties.pressureTaperingStart);
    settings->deferredSetValue("pencilPressureTaperingStart", properties.pressureTaperingStart);
    settings->deferredSetValue("pencilPressureTaperingEnd", properties.pressureTaperingEnd);

    settings->deferredSetValue("pencilSmoothingMode", (int)properties.penSmoothingMode);
    settings->deferredSetValue("pencilWeightedDistance", properties.weightedDistance);
    settings->deferredSetValue("pencilStab2Distance", properties.stab2Distance);
    settings->deferredSetValue("pencilStab2Delay", properties.stab2Delay);
    // Backward compatibility: keep legacy key in sync with weightedDistance.
    settings->deferredSetValue("pencilStabilizer", properties.weightedDistance);

    // Save pencil opacity
    settings->deferredSetValue("pencilOpacity", mPencilOpacity);

    settings->deferredSetValue("pencilUserSpacingMult", mUserSpacingMult);
    settings->deferredSetValue("pencilScatterAmount", mScatterAmount);
    settings->deferredSetValue("pencilRotationJitterRange", mRotationJitterRangeDegrees);
    settings->deferredSetValue("pencilSizeJitterAmount", mSizeJitterAmount);
    settings->deferredSetValue("pencilColorJitterAmount", mColorJitterAmount);

    settings->deferredSetValue("pencilTextureTileScale", mTextureTileScale);
    settings->deferredSetValue("pencilTextureMaskGamma", mTextureMaskGamma);
    settings->deferredSetValue("pencilTextureMaskCutoff", mTextureMaskCutoff);

    clearSettingsDirty();
}

void PencilTool::resetToDefault()
{
    setWidth(8.0);
    setPressure(true);
    setAA(0);
    mOpacityPressureCurve.clear();
    mOpacityPressureCurve.append(QPointF(0.5, 0.5));
    mThicknessPressureCurve.clear();
    mThicknessPressureCurve.append(QPointF(0.5, 0.5));
    properties.pressureSmoothingAmount = 0.3;
    properties.pressureTaperingStart = 0.0;
    properties.pressureTaperingEnd = 0.5;
    properties.weightedDistance = 0.0;
    properties.penSmoothingMode = PenSmoothingMode::WEIGHTED;
    properties.stab2Distance = 16.0;
    properties.stab2Delay = 4.0;
    mUserSpacingMult = 1.0;
    mScatterAmount = 0.15;
    mRotationJitterRangeDegrees = 2.2;
    mSizeJitterAmount = 0.10;
    mColorJitterAmount = 0.0;

    mTextureTileScale = 12.0;
    mTextureMaskGamma = 2.2;
    mTextureMaskCutoff = 0.25;
}

void PencilTool::setWidth(const qreal width)
{
    if (!qFuzzyCompare(properties.width, width)) { properties.width = width; markSettingsDirty(); }
}

void PencilTool::setPressure(const bool pressure)
{
    if (properties.pressure != pressure) { properties.pressure = pressure; markSettingsDirty(); }
}

void PencilTool::setAA(const int AA)
{
    if (properties.useAA != AA) { properties.useAA = AA; markSettingsDirty(); }
}

void PencilTool::setUserSpacingMult(qreal value)
{
    value = qBound<qreal>(0.05, value, 10.0);
    if (!qFuzzyCompare(mUserSpacingMult, value)) { mUserSpacingMult = value; markSettingsDirty(); }
}

void PencilTool::setScatterAmount(qreal value)
{
    value = qBound<qreal>(0.0, value, 3.0);
    if (!qFuzzyCompare(mScatterAmount, value)) { mScatterAmount = value; markSettingsDirty(); }
}

void PencilTool::setRotationJitterRangeDegrees(qreal value)
{
    value = qBound<qreal>(0.0, value, 12.0);
    if (!qFuzzyCompare(mRotationJitterRangeDegrees, value)) { mRotationJitterRangeDegrees = value; markSettingsDirty(); }
}

void PencilTool::setSizeJitterAmount(qreal value)
{
    value = qBound<qreal>(0.0, value, 0.5);
    if (!qFuzzyCompare(mSizeJitterAmount, value)) { mSizeJitterAmount = value; markSettingsDirty(); }
}

void PencilTool::setColorJitterAmount(qreal value)
{
    value = qBound<qreal>(0.0, value, 1.0);
    if (!qFuzzyCompare(mColorJitterAmount, value)) { mColorJitterAmount = value; markSettingsDirty(); }
}
void PencilTool::setOpacityPressureCurve(const QVector<QPointF>& points)
{
    mOpacityPressureCurve.clear();
    for (const QPointF& p : points) {
        mOpacityPressureCurve.append(QPointF(qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0)));
    }
    if (mOpacityPressureCurve.isEmpty()) mOpacityPressureCurve.append(QPointF(0.5, 0.5));
    MathUtils::sortControlPointsByX(mOpacityPressureCurve);
    mOpacityCurveLUT.setCurvePoints(mOpacityPressureCurve);
}

QVector<QPointF> PencilTool::getOpacityPressureCurve() const
{
    return mOpacityPressureCurve;
}

void PencilTool::setThicknessPressureCurve(const QVector<QPointF>& points)
{
    mThicknessPressureCurve.clear();
    for (const QPointF& p : points) {
        mThicknessPressureCurve.append(QPointF(qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0)));
    }
    if (mThicknessPressureCurve.isEmpty()) mThicknessPressureCurve.append(QPointF(0.5, 0.5));
    MathUtils::sortControlPointsByX(mThicknessPressureCurve);
    mThicknessCurveLUT.setCurvePoints(mThicknessPressureCurve);
}

QVector<QPointF> PencilTool::getThicknessPressureCurve() const
{
    return mThicknessPressureCurve;
}

bool PencilTool::loadTexture()
{
    // Load P1.png from BrushFiles folder as greyscale alpha mask
    QString texturePath;
    
    // Try resource path first, then absolute path
    QImage sourceImage;

    // Use embedded resources so packaged builds always find the texture.
    // Prefer the original Pencil dab texture first.
    texturePath = QLatin1String(":/BrushFiles/P1.png");
    if (!sourceImage.load(texturePath)) {
        // Fallback to a bundled BrushTextures asset.
        texturePath = QLatin1String(":/BrushTextures/WeepingFather.jpeg");
        if (!sourceImage.load(texturePath)) {
            qWarning() << "[PencilTool] Failed to load texture from" << texturePath;
            // Create fallback circular texture
            sourceImage = QImage(128, 128, QImage::Format_Grayscale8);
            sourceImage.fill(Qt::black);
            QPainter p(&sourceImage);
            p.setBrush(Qt::white);
            p.setPen(Qt::NoPen);
            p.drawEllipse(0, 0, 128, 128);
            p.end();
        }
    }

    if (!applyTextureImage(sourceImage)) {
        return false;
    }

    if (!mHasDefaultTexture) {
        mDefaultTextureSource = mTextureSource;
        mDefaultTextureBaseSource = mTextureBaseSource;
        mHasDefaultTexture = true;
    }

    qDebug() << "[PencilTool] Loaded texture:" << mTextureSource.size();
    return true;
}

void PencilTool::rebuildWorkingTextureFromBase()
{
    // Write lock: protects mTextureSource from concurrent reads in the prerender worker thread.
    QWriteLocker texLock(&mTextureLock);

    if (mTextureBaseSource.isNull()) {
        mTextureSource = QImage();
        mTextureLoaded = false;
        return;
    }

    // If bake parameters haven't changed, keep the current working texture.
    if (qFuzzyCompare(mBakedGamma, mTextureMaskGamma) && qFuzzyCompare(mBakedCutoff, mTextureMaskCutoff)
        && !mTextureSource.isNull() && mTextureSource.size() == mTextureBaseSource.size()) {
        return;
    }

    // Build alpha LUT (0..255 -> 0..255) incorporating cutoff + gamma.
    const qreal cutoff = qBound<qreal>(0.0, mTextureMaskCutoff, 0.95);
    const qreal gamma = qMax<qreal>(0.0001, mTextureMaskGamma);
    quint8 lut[256];
    for (int i = 0; i < 256; ++i) {
        qreal a = qreal(i) / 255.0;
        if (a <= cutoff) {
            lut[i] = 0;
            continue;
        }
        a = (a - cutoff) / (1.0 - cutoff);
        a = qPow(a, gamma);
        int outA = qBound(0, int(qRound(a * 255.0)), 255);
        lut[i] = quint8(outA);
    }

    // Bake into a working texture with the same size/format as base.
    mTextureSource = mTextureBaseSource;
    for (int y = 0; y < mTextureSource.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(mTextureSource.scanLine(y));
        for (int x = 0; x < mTextureSource.width(); ++x) {
            const int a = qAlpha(line[x]);
            const int outA = lut[a];
            line[x] = qRgba(255, 255, 255, outA);
        }
    }

    mTextureWorkingPixmapDirty = true;

    mBakedGamma = mTextureMaskGamma;
    mBakedCutoff = mTextureMaskCutoff;
}

bool PencilTool::applyTextureImage(const QImage& sourceImage)
{
    if (sourceImage.isNull()) {
        return false;
    }

    // Clamp texture size to avoid excessive memory usage when the user selects
    // very large images. This keeps the brush responsive and prevents
    // exhausting GPU/CPU resources.
    QImage working = sourceImage;
    const int maxDim = 1024; // max width/height in pixels
    if (working.width() > maxDim || working.height() > maxDim) {
        working = working.scaled(QSize(maxDim, maxDim), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // Convert to greyscale alpha mask format (black=transparent, white=opaque)
    mTextureBaseSource = working.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Process to use greyscale as alpha channel.
    // White = opaque (255), Black = transparent (0).
    const int tw = mTextureBaseSource.width();
    const int th = mTextureBaseSource.height();
    for (int y = 0; y < th; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(mTextureBaseSource.scanLine(y));
        for (int x = 0; x < tw; ++x) {
            int gray = qGray(line[x]);
            line[x] = qRgba(255, 255, 255, gray);
        }
    }

    // Bake working texture using current contrast settings.
    mBakedGamma = -1.0;
    mBakedCutoff = -1.0;
    rebuildWorkingTextureFromBase();

    // Set mTextureLoaded under the write lock so the worker thread sees a
    // consistent state (texture data + loaded flag together).
    {
        QWriteLocker texLock(&mTextureLock);
        mTextureLoaded = true;
    }
    clearDabCache();
    return true;
}

void PencilTool::clearDabCache()
{
    QMutexLocker lk(&mDabCacheMutex);
    mDabCache.clear();
    mDabCacheHits = 0;
    mDabCacheMisses = 0;
    sInFlightPrerenderKeys.clear();
}

bool PencilTool::setCustomTextureFromFile(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    QImage sourceImage;
    if (!sourceImage.load(path)) {
        qWarning() << "[PencilTool] Failed to load custom texture from" << path;
        return false;
    }

    return applyTextureImage(sourceImage);
}

bool PencilTool::setCustomTextureFromData(const QByteArray& data)
{
    if (data.isEmpty()) {
        return false;
    }

    QImage sourceImage;
    if (!sourceImage.loadFromData(data)) {
        qWarning() << "[PencilTool] Failed to load custom texture from data";
        return false;
    }

    return applyTextureImage(sourceImage);
}

bool PencilTool::setCustomTipFromData(const QByteArray& data)
{
    if (data.isEmpty()) {
        mCustomTipBase = QImage();
        mCustomTipMask = QImage();
        return false;
    }

    QImage src;
    if (!src.loadFromData(data)) {
        qWarning() << "[PencilTool] setCustomTipFromData: failed to decode image";
        return false;
    }

    // Bake greyscale → alpha once at import time into the base image.
    QImage baked = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = baked.width();
    const int h = baked.height();
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(baked.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int gray    = qGray(line[x]);
            const int srcAlpha = qAlpha(line[x]);
            const int alpha   = (srcAlpha < 255) ? srcAlpha : gray;
            line[x] = qRgba(255, 255, 255, alpha);
        }
    }

    mCustomTipBase  = baked;
    mBakedTipGamma  = -1.0; // force rebuild
    mBakedTipCutoff = -1.0;
    rebuildWorkingTipMask();
    return true;
}

void PencilTool::rebuildWorkingTipMask()
{
    if (mCustomTipBase.isNull()) {
        mCustomTipMask = QImage();
        return;
    }

    // Skip if nothing changed
    if (qFuzzyCompare(mBakedTipGamma, mTipMaskGamma) &&
        qFuzzyCompare(mBakedTipCutoff, mTipMaskCutoff) &&
        !mCustomTipMask.isNull() &&
        mCustomTipMask.size() == mCustomTipBase.size()) {
        return;
    }

    // Build alpha LUT — same formula as rebuildWorkingTextureFromBase
    const qreal cutoff = qBound<qreal>(0.0, mTipMaskCutoff, 0.95);
    const qreal gamma  = qMax<qreal>(0.0001, mTipMaskGamma);
    quint8 lut[256];
    for (int i = 0; i < 256; ++i) {
        qreal a = qreal(i) / 255.0;
        if (a <= cutoff) {
            lut[i] = 0;
        } else {
            const qreal normalized = (a - cutoff) / (1.0 - cutoff);
            lut[i] = quint8(qBound(0.0, qPow(normalized, gamma) * 255.0, 255.0));
        }
    }

    mCustomTipMask = mCustomTipBase.copy();
    for (int y = 0; y < mCustomTipMask.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(mCustomTipMask.scanLine(y));
        for (int x = 0; x < mCustomTipMask.width(); ++x) {
            const int oldAlpha = qAlpha(line[x]);
            line[x] = qRgba(255, 255, 255, lut[oldAlpha]);
        }
    }

    mBakedTipGamma  = mTipMaskGamma;
    mBakedTipCutoff = mTipMaskCutoff;
}

void PencilTool::setTipContrastPercent(int percent)
{
    percent = qBound(0, percent, 100);
    const qreal t      = qreal(percent) / 100.0;
    const qreal gamma  = 1.0 + 2.0 * t;
    const qreal cutoff = 0.45 * t;
    if (!qFuzzyCompare(mTipMaskGamma, gamma) || !qFuzzyCompare(mTipMaskCutoff, cutoff)) {
        mTipMaskGamma  = gamma;
        mTipMaskCutoff = cutoff;
        rebuildWorkingTipMask();
    }
}

int PencilTool::getTipContrastPercent() const
{
    const qreal g = qBound<qreal>(0.0, (mTipMaskGamma  - 1.0) / 2.0, 1.0);
    const qreal c = qBound<qreal>(0.0,  mTipMaskCutoff / 0.45,       1.0);
    return qBound(0, int(qRound((0.6 * g + 0.4 * c) * 100.0)), 100);
}

void PencilTool::clearCustomTexture()
{    if (mHasDefaultTexture && !mDefaultTextureSource.isNull()) {
        mTextureBaseSource = mDefaultTextureBaseSource;
        mBakedGamma = -1.0;
        mBakedCutoff = -1.0;
        rebuildWorkingTextureFromBase();
        {
            QWriteLocker texLock(&mTextureLock);
            mTextureLoaded = true;
        }
        clearDabCache();
    } else {
        loadTexture();
    }
}

void PencilTool::setPlainWhiteTexture()
{
    // Load the plain white 256×256 PNG — all pixels alpha=255, no grain.
    // Used as the default for user presets so strokes are solid until a texture is imported.
    QImage white;
    if (!white.load(QStringLiteral(":/BrushFiles/PlainWhite.png"))) {
        // Fallback: generate programmatically if the resource is missing
        white = QImage(256, 256, QImage::Format_Grayscale8);
        white.fill(Qt::white);
    }
    applyTextureImage(white);
    clearDabCache();
}

quint64 PencilTool::makeCacheKey(const DabCacheKey& key) const
{
    // Combine parameters into 64-bit key
    quint64 k = 0;
    k |= (quint64(key.size & 0xFFFF) << 48);
    k |= (quint64(key.rotation & 0xFFFF) << 32);
    k |= (quint64(key.opacity & 0xFF) << 24);
    k |= (quint64(key.color & 0xFFFFFF));
    return k;
}

PencilTool::DabCacheKey PencilTool::makeDabCacheKey(qreal size, qreal rotation, qreal opacity, const QColor& color)
{
    PencilTool::DabCacheKey key;

    // Size: round(size/0.5)*0.5, stored as bucket index in 0.5 increments
    qreal clampedSize = qBound<qreal>(0.0, size, 10000.0);
    int sizeBucket = qRound(clampedSize / 0.5); // 0.5px steps
    if (sizeBucket < 0) sizeBucket = 0;
    if (sizeBucket > 0xFFFF) sizeBucket = 0xFFFF;
    key.size = sizeBucket;

    // Rotation: round(rotation/15)*15, stored as bucket index
    int rotationBucket = qRound(rotation / 15.0); // 15-degree steps
    // Keep within 16-bit signed range used in packing
    if (rotationBucket < -32768) rotationBucket = -32768;
    if (rotationBucket >  32767) rotationBucket =  32767;
    key.rotation = rotationBucket;

    // Opacity: round(opacity*64)/64, stored as bucket index 0-64
    qreal clampedOpacity = qBound<qreal>(0.0, opacity, 1.0);
    int opacityBucket = qRound(clampedOpacity * 64.0);
    if (opacityBucket < 0) opacityBucket = 0;
    if (opacityBucket > 64) opacityBucket = 64;
    key.opacity = opacityBucket;

    // Color: quantize RGB to 32 levels per channel (5 bits each)
    int rBucket = color.red() / 8;   // 0-31
    int gBucket = color.green() / 8; // 0-31
    int bBucket = color.blue() / 8;  // 0-31
    if (rBucket < 0) rBucket = 0; if (rBucket > 31) rBucket = 31;
    if (gBucket < 0) gBucket = 0; if (gBucket > 31) gBucket = 31;
    if (bBucket < 0) bBucket = 0; if (bBucket > 31) bBucket = 31;
    key.color = (rBucket << 16) | (gBucket << 8) | bBucket;

    return key;
}

QImage PencilTool::prerenderTextureDab(qreal size, qreal rotation, qreal opacity, const QColor& color)
{
    // Thread-safe pre-rendering: creates solid circular brush with texture as opacity noise
    // Texture is NOT scaled to brush size - it's sampled at fixed resolution for grain detail
    // This allows small brushes (4-5px) to still show texture variations like Clip Studio Paint

    // Take a local copy of the texture under the read lock so the main thread can safely
    // replace mTextureSource (e.g. on preset switch) without racing with this worker thread.
    QImage localTexture;
    {
        QReadLocker texLock(&mTextureLock);
        if (!mTextureLoaded || mTextureSource.isNull()) {
            return QImage();
        }
        localTexture = mTextureSource; // QImage is implicitly shared; copy is O(1) here
    }

    // Check cache first (quantized key to improve hit rate)
    DabCacheKey cacheKey = makeDabCacheKey(size, rotation, opacity, color);
    quint64 key = makeCacheKey(cacheKey);
    {
        QMutexLocker lk(&mDabCacheMutex);
        if (mDabCache.contains(key)) {
            ++mDabCacheHits;
            const quint64 total = mDabCacheHits + mDabCacheMisses;
            if (total % 1000 == 0) {
                const double hitRate = total ? (double(mDabCacheHits) / double(total)) : 0.0;
                qDebug() << "[PencilTool] Dab cache hit rate:" << hitRate
                         << "(" << mDabCacheHits << "/" << total << ")";
            }
            return *mDabCache.object(key);
        }
        ++mDabCacheMisses;
    }

    int dabSize = qMax(1, qRound(size));
    qreal radius = dabSize / 2.0;
    
    // Create output image for dab
    QImage dab(dabSize, dabSize, QImage::Format_ARGB32_Premultiplied);
    dab.fill(Qt::transparent);

    // Texture sampling: use fixed-size sampling window for consistent grain
    // Sample from center of texture with random offset for variation
    int texCenterX = localTexture.width() / 2;
    int texCenterY = localTexture.height() / 2;
    int texSampleSize = qMin(localTexture.width(), localTexture.height()) / 2;

    // Apply rotation to texture sampling around dab center so that anisotropic
    // textures visibly change orientation with rotation. We still add a small
    // translational offset plus a random offset to break tiling.
    qreal rotRad = qDegreesToRadians(rotation);
    qreal cosR = qCos(rotRad);
    qreal sinR = qSin(rotRad);

    // Base translational offset along the rotation direction (small radius).
    int baseOffsetX = qRound(cosR * 10.0);
    int baseOffsetY = qRound(sinR * 10.0);

    // Random texture offset (each dab samples different texture region)
    int randomOffsetX = QRandomGenerator::global()->bounded(-30, 30);
    int randomOffsetY = QRandomGenerator::global()->bounded(-30, 30);
    int offsetX = baseOffsetX + randomOffsetX;
    int offsetY = baseOffsetY + randomOffsetY;
    
    // Pre-calculate values outside inner loop for performance
    const int texWidth = localTexture.width();
    const int texHeight = localTexture.height();
    const qreal texScale = texSampleSize / radius;
    const int colorR = color.red();
    const int colorG = color.green();
    const int colorB = color.blue();
    const qreal radiusMinusOne = radius - 1.0;
    
    // Direct buffer access for texture (10x faster than pixel())
    const uchar* texBits = localTexture.constBits();
    const int texBytesPerLine = localTexture.bytesPerLine();
    
    // Build the dab pixel by pixel:
    // 1. Create circular brush shape
    // 2. Apply texture as opacity noise overlay
    // 3. Apply base opacity and color
    const qreal radiusSq        = radius * radius;
    const qreal softEdgeInnerSq = (radiusMinusOne) * (radiusMinusOne);

    for (int y = 0; y < dabSize; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(dab.scanLine(y));
        const qreal dy  = y - radius + 0.5;
        const qreal dy2 = dy * dy;

        // Early-out entire row if it can't intersect the circle
        if (dy2 >= radiusSq) continue;

        for (int x = 0; x < dabSize; ++x) {
            const qreal dx     = x - radius + 0.5;
            const qreal distSq = dx * dx + dy2;

            // Fast reject: outside circle
            if (distSq >= radiusSq) continue;

            // Soft circular falloff — only compute sqrt for the 1-px edge band
            qreal circleMask;
            if (distSq <= softEdgeInnerSq) {
                circleMask = 1.0;
            } else {
                circleMask = radius - qSqrt(distSq); // in [0,1] for the edge band
            }

            // Rotate local dab coordinates into texture space
            const qreal rx = dx * cosR - dy * sinR;
            const qreal ry = dx * sinR + dy * cosR;

            int texX = texCenterX + offsetX + qRound(rx * texScale);
            int texY = texCenterY + offsetY + qRound(ry * texScale);

            // Wrap texture coordinates
            texX = (texX % texWidth + texWidth) % texWidth;
            texY = (texY % texHeight + texHeight) % texHeight;

            // Texture alpha via direct buffer access
            const uchar* texPixel = texBits + texY * texBytesPerLine + texX * 4;
            const qreal textureOpacity = texPixel[3] * (1.0 / 255.0);

            const qreal finalOpacity = circleMask * textureOpacity * opacity;

            const int alpha = qRound(finalOpacity * 255.0);
            if (alpha > 0) {
                const int r = qRound(colorR * finalOpacity);
                const int g = qRound(colorG * finalOpacity);
                const int b = qRound(colorB * finalOpacity);
                line[x] = qRgba(r, g, b, alpha);
            }
        }
    }

    // Cache the result
    {
        QMutexLocker lk(&mDabCacheMutex);
        QImage* cached = new QImage(dab);
        const int cost = qMax(1, cached->byteCount() / 1024);
        mDabCache.insert(key, cached, cost);
    }

    return dab;
}

QImage PencilTool::renderTexturedRibbonSegment(const QPointF& a,
                                               const QPointF& b,
                                               qreal brushSize,
                                               qreal opacity,
                                               const QColor& color,
                                               qreal extraRotationDeg,
                                               QRectF* outCanvasBounds)
{
    if (!outCanvasBounds) {
        return QImage();
    }
    *outCanvasBounds = QRectF();

    if (!mTextureLoaded || mTextureSource.isNull()) {
        return QImage();
    }

    brushSize = qBound<qreal>(1.0, brushSize, 200.0);
    opacity = qBound<qreal>(0.0, opacity, 1.0);

    const qreal half = brushSize * 0.5;
    QRectF bounds = QRectF(a, b).normalized();
    bounds = bounds.adjusted(-half - 2.0, -half - 2.0, half + 2.0, half + 2.0);
    if (!bounds.isValid() || bounds.isEmpty()) {
        return QImage();
    }

    // Hard safety clamp to avoid allocating huge intermediate images.
    const int w = qCeil(bounds.width());
    const int h = qCeil(bounds.height());
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) {
        return QImage();
    }

    auto ensureScratch = [](QImage& img, int w, int h) {
        if (img.width() != w || img.height() != h || img.format() != QImage::Format_ARGB32_Premultiplied) {
            img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        }
        img.fill(Qt::transparent); // always clear — residual pixels from prior segments cause border artifacts
    };

    // Ensure working texture is up-to-date for current contrast and pixmap cache.
    // NOTE: rebuildWorkingTextureFromBase() is called when settings change (not here)
    // to avoid acquiring a write lock on every dab segment.
    if (mTextureWorkingPixmapDirty || mTextureWorkingPixmap.isNull()) {
        mTextureWorkingPixmap = QPixmap::fromImage(mTextureSource);
        mTextureWorkingPixmapDirty = false;
    }
    if (mTextureWorkingPixmap.isNull()) {
        return QImage();
    }

    // Render: draw solid stroke into output, then mask with tiled texture via DestinationIn.
    ensureScratch(mScratchRibbonOut, w, h);
    {
        // --- Texture transform ---
        // Quantize angle to nearest 5° to maximise cache hits across consecutive dabs.
        const qreal segRad  = std::atan2(b.y() - a.y(), b.x() - a.x());
        const qreal segDeg  = segRad * (180.0 / M_PI);
        const qreal angleDeg = mStrokeTextureRotationDeg + segDeg + extraRotationDeg;
        const int   qAngle  = qRound(angleDeg / 5.0) * 5; // 5° quantisation

        const qreal tileScale = qMax<qreal>(0.0001, mTextureTileScale);
        const QPointF boundsTL = bounds.topLeft();
        const QPointF anchor   = mStrokeTextureAnchorCanvas;
        const QPointF offset   = mStrokeTextureOffsetCanvas;

        // Recompute the brush transform only when the quantized angle changes.
        // The translational part (base) depends on boundsTL which shifts every segment,
        // so we always recompute the full transform — but we skip the rotate/scale/invert
        // when the angle hasn't changed, which is the expensive part.
        if (qAngle != mCachedTexXf.quantizedAngle) {
            QTransform r;
            r.rotate(qreal(qAngle));
            QTransform linear = r;
            linear.scale(tileScale, tileScale);

            // Store the angle-dependent part; the translational offset is added below.
            // We store the full inverted transform for the common case where boundsTL
            // is the only thing that changes between consecutive dabs.
            const QPointF base = linear.map(boundsTL - anchor) + r.map(offset);
            const QTransform deviceToPattern = QTransform::fromTranslate(base.x(), base.y()) * linear;
            bool invertOk = false;
            mCachedTexXf.brushXf = deviceToPattern.inverted(&invertOk);
            if (!invertOk) {
                return QImage();
            }
            mCachedTexXf.quantizedAngle = qAngle;
            mCachedTexXf.lastBoundsTL   = boundsTL;
        } else if (boundsTL != mCachedTexXf.lastBoundsTL) {
            // Same angle, different position — only the translation changes.
            // Recompute cheaply without rotate/scale/invert.
            QTransform r;
            r.rotate(qreal(qAngle));
            QTransform linear = r;
            linear.scale(tileScale, tileScale);
            const QPointF base = linear.map(boundsTL - anchor) + r.map(offset);
            const QTransform deviceToPattern = QTransform::fromTranslate(base.x(), base.y()) * linear;
            bool invertOk = false;
            mCachedTexXf.brushXf = deviceToPattern.inverted(&invertOk);
            if (!invertOk) {
                return QImage();
            }
            mCachedTexXf.lastBoundsTL = boundsTL;
        }

        // Pass 1: stamp the tip shape mask using the stroke color directly.
        //         The tip mask alpha gradient defines the dab boundary.
        //         Nothing outside the tip is ever non-zero.
        // Pass 2: texture via DestinationIn — modulates alpha inside the tip.
        //         Outside pixels are already zero so DestinationIn can't affect them.

        const QPointF aLocal = a - boundsTL;
        const QPointF bLocal = b - boundsTL;
        const qreal   segLen = QLineF(aLocal, bLocal).length();

        QColor c = color;
        c.setAlpha(qBound(0, int(qRound(opacity * 255.0)), 255));

        QPainter p(&mScratchRibbonOut);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Apply dab rotation: base rotation aligns tip with stroke direction.
        // extraRotationDeg (random deck) is an offset on top of that.
        // This means value=0 gives stroke-aligned tips, higher values add variation.
        const qreal totalDabRotation = segDeg + extraRotationDeg;
        const bool hasDabRotation = (qAbs(totalDabRotation) > 0.01);

        // Pass 1: draw tip mask tinted with stroke color.
        p.setCompositionMode(QPainter::CompositionMode_Source);
        if (sTipMasksReady) {
            const QImage& tipSrc = (mTipShape == CosmicTipShape::Custom && !mCustomTipMask.isNull())
                                 ? mCustomTipMask
                                 : (mTipShape == CosmicTipShape::Hardest) ? sTipMaskHardest
                                 : (mTipShape == CosmicTipShape::Hard || mTipShape == CosmicTipShape::Custom)
                                 ? sTipMaskHard
                                 : (mTipShape == CosmicTipShape::Soft) ? sTipMaskSoft
                                 : sTipMaskMedium;

            if (hasDabRotation) {
                p.save();
            }

            // Stamp tip at tighter intervals for a seamless ribbon (20% step = no gaps).
            const qreal step = qMax<qreal>(1.0, brushSize * 0.20);
            if (segLen < 0.5) {
                const QPointF center = (segLen < 0.01) ? aLocal : (aLocal + bLocal) * 0.5;
                const QRectF dst(center.x() - half, center.y() - half, brushSize, brushSize);
                if (hasDabRotation) {
                    p.translate(center);
                    p.rotate(totalDabRotation);
                    p.translate(-center);
                }
                p.drawImage(dst, tipSrc);
            } else {
                const QPointF dir = (bLocal - aLocal) / segLen;
                for (qreal t = 0.0; t <= segLen + 0.5; t += step) {
                    const QPointF pt = aLocal + dir * qMin(t, segLen);
                    const QRectF dst(pt.x() - half, pt.y() - half, brushSize, brushSize);
                    if (hasDabRotation) {
                        p.save();
                        p.translate(pt);
                        p.rotate(totalDabRotation);
                        p.translate(-pt);
                    }
                    p.drawImage(dst, tipSrc);
                    if (hasDabRotation) p.restore();
                }
            }

            if (hasDabRotation) p.restore();

            // Recolor: replace the white pixels with stroke color while keeping alpha intact.
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(QRectF(0.0, 0.0, w, h), c);
        } else {
            // Fallback: plain round shape in stroke color
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            if (segLen < 0.01) {
                p.drawEllipse(QRectF(aLocal.x() - half, aLocal.y() - half, brushSize, brushSize));
            } else {
                QPen strokePen(c);
                strokePen.setWidthF(brushSize);
                strokePen.setCapStyle(Qt::RoundCap);
                p.setPen(strokePen);
                p.setBrush(Qt::NoBrush);
                p.drawLine(aLocal, bLocal);
            }
        }

        // Pass 2: paper texture via DestinationIn — modulates stroke alpha by texture luminance.
        // White paper (luminance=255) → alpha unchanged. Dark grain (luminance=0) → alpha=0.
        // Canvas-anchored so grain is consistent across the whole stroke.
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        {
            QBrush brush(mTextureWorkingPixmap);
            brush.setTransform(mCachedTexXf.brushXf);
            p.fillRect(QRectF(0.0, 0.0, w, h), brush);
        }

        // Pass 3: edge-wear — concentrate texture bite at stroke edges, protect the core.
        // Build a radial gradient centered on the stroke axis: center=transparent (no extra
        // modulation), edge=semi-opaque white. Compositing this with DestinationIn on top of
        // the already-textured ribbon makes the grain stronger at the outer edges while the
        // stroke core stays solid — matching CSP/TVPaint edge-wear behaviour.
        if (sTipMasksReady && brushSize >= 4.0) {
            // Use the soft tip mask as the edge-wear weight: soft mask is bright at center,
            // dark at edges. Inverting it gives us bright-at-edges which is what we want.
            // We draw the inverted soft mask with DestinationIn at reduced opacity so it
            // only partially modulates — not a full cut.
            const QImage& softSrc = sTipMaskSoft;
            const qreal edgeWearStrength = 0.35; // 0=no edge wear, 1=full

            // Build edge-wear mask into scratch: invert the soft tip alpha
            auto& ewScratch = mScratchRibbonMask;
            if (ewScratch.width() != w || ewScratch.height() != h ||
                ewScratch.format() != QImage::Format_ARGB32_Premultiplied) {
                ewScratch = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
            }
            ewScratch.fill(Qt::transparent);

            QPainter ep(&ewScratch);
            ep.setRenderHint(QPainter::SmoothPixmapTransform, true);
            ep.setCompositionMode(QPainter::CompositionMode_Source);
            // Stamp soft mask (bright center, dark edge) with same rotation as Pass 1
            const qreal step2 = qMax<qreal>(1.0, brushSize * 0.20);
            if (segLen < 0.5) {
                const QPointF center = (segLen < 0.01) ? aLocal : (aLocal + bLocal) * 0.5;
                if (hasDabRotation) { ep.translate(center); ep.rotate(totalDabRotation); ep.translate(-center); }
                ep.drawImage(QRectF(center.x() - half, center.y() - half, brushSize, brushSize), softSrc);
            } else {
                const QPointF dir = (bLocal - aLocal) / segLen;
                for (qreal t = 0.0; t <= segLen + 0.5; t += step2) {
                    const QPointF pt = aLocal + dir * qMin(t, segLen);
                    if (hasDabRotation) { ep.save(); ep.translate(pt); ep.rotate(totalDabRotation); ep.translate(-pt); }
                    ep.drawImage(QRectF(pt.x() - half, pt.y() - half, brushSize, brushSize), softSrc);
                    if (hasDabRotation) ep.restore();
                }
            }
            ep.end();

            // Invert the alpha: edge-wear weight = 1 - soft_alpha
            // Then blend: final = ribbon * (1 - edgeWearStrength * (1 - soft_alpha))
            //                   = ribbon * (1 - edgeWearStrength + edgeWearStrength * soft_alpha)
            // We achieve this by drawing the inverted mask at edgeWearStrength opacity
            // using DestinationIn, which multiplies ribbon alpha by the mask alpha.
            // Inverted mask alpha at center ≈ 0 (soft_alpha≈1 → invert≈0 → no cut)
            // Inverted mask alpha at edge  ≈ 1 (soft_alpha≈0 → invert≈1 → full cut)
            // But we want partial cut, so we blend: mask = 1 - edgeWearStrength * inverted
            // = 1 - edgeWearStrength + edgeWearStrength * soft_alpha
            // Simplest: draw the soft mask at (1-edgeWearStrength) opacity with DestinationIn.
            // This multiplies ribbon alpha by lerp(1, soft_alpha, edgeWearStrength).
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.setOpacity(edgeWearStrength);
            p.drawImage(0, 0, ewScratch);
            p.setOpacity(1.0);
        }

        p.end();
    }

    *outCanvasBounds = bounds;
    return mScratchRibbonOut;
}

void PencilTool::schedulePrerenderDab(qreal size, qreal rotation, qreal opacity, const QColor& color)
{
    // The ribbon rendering path uses renderTexturedRibbonSegment() directly via QPainter
    // composition, not the per-dab cache. Background prerender threads are therefore
    // unused and only add thread-pool overhead. This function is intentionally a no-op.
    Q_UNUSED(size); Q_UNUSED(rotation); Q_UNUSED(opacity); Q_UNUSED(color);
}

void PencilTool::warmNextDabs(qreal size, qreal opacity, const QColor& color)
{
    // === VELOCITY-BASED PREDICTIVE CACHING (OPTIMIZED) ===
    // Predict future dab positions based on stroke velocity and direction
    // Reduced grid size to minimize thread overhead while maintaining cache hits
    
    // Get current velocity from interpolator
    qreal velocity = mInterpolator.getVelocity();
    if (mEditor && mEditor->view()) {
        const qreal s = mEditor->view()->scaling();
        if (s > 0.0) velocity = velocity / s;  // Convert to canvas space
    }
    
    // Predict pressure changes based on velocity
    // Fast strokes: lighter pressure likely
    // Slow strokes: heavier pressure likely
    qreal velocityFactor = qBound(0.0, velocity / 20.0, 1.0);  // Normalize to 0-1
    qreal predictedPressure = mCurrentPressure * (1.0 - velocityFactor * 0.3);  // Up to 30% lighter at high speed
    
    // Predict size: no longer varies with pressure; keep base size
    qreal predictedSize = size;
    
    // Predict opacity based on pressure curve
    qreal predictedOpacity = opacity;
    if (mOpacityCurveEnabled && !mOpacityPressureCurve.isEmpty()) {
        qreal pressureMult = mOpacityCurveLUT.evaluate(predictedPressure);
        predictedOpacity = mPencilOpacity * pressureMult;
    }
    
    // Pre-render variations around predicted values
    // REDUCED grid: 2x2x2 = 8 dabs (optimized from 27 for performance)
    QVector<qreal> sizes = {
        qBound<qreal>(1.0, predictedSize * 0.9, 200.0),
        qBound<qreal>(1.0, predictedSize * 1.1, 200.0)
    };
    QVector<qreal> rots = {-10.0, 10.0};  // Reduced from 3 to 2 angles
    QVector<qreal> ops = {
        qBound<qreal>(0.0, predictedOpacity * 0.9, 1.0),
        qBound<qreal>(0.0, predictedOpacity * 1.1, 1.0)
    };
    
    // Schedule pre-rendering in background threads (max 8 tasks instead of 27)
    for (qreal s : sizes) {
        for (qreal a : rots) {
            for (qreal op : ops) {
                schedulePrerenderDab(s, a, op, color);
            }
        }
    }
}

qreal PencilTool::calculateAdaptiveSpacing(qreal velocity, qreal curvature)
{
    Q_UNUSED(velocity);
    Q_UNUSED(curvature);

    // Fixed, distance-based spacing: keep dab distance visually constant
    // across stroke speed, curvature, zoom and pressure.
    //
    // We express spacing as a fraction of the brush diameter in *screen space*;
    // drawStroke() already converts this fraction into an actual distance using
    // the current effective width and view scale.
    //
    // A value around 0.4–0.5 gives good overlap (continuous textured line)
    // without overdraw. Users can still scale this with the spacing slider.
    const qreal baseSpacingFraction = 0.45; // 45% of diameter

    // Clamp user spacing multiplier to a sane range so presets/UI cannot
    // accidentally request extreme densities.
    qreal userMult = qBound<qreal>(0.05, mUserSpacingMult, 10.0);

    // Target multiplier is purely user-controlled; no dependence on
    // velocity or curvature so density is uniform along the stroke.
    qreal targetMult = userMult;

    // Light temporal smoothing to avoid visible jumps if spacing is changed
    // live while drawing, but still keep the response snappy.
    const qreal smoothing = 0.2;
    mLastSpacingMult = mLastSpacingMult * (1.0 - smoothing) + targetMult * smoothing;

    qreal finalSpacingFraction = baseSpacingFraction * mLastSpacingMult;

    // Hard safety clamp — upper end allows large gaps (10x diameter).
    return qBound<qreal>(0.05, finalSpacingFraction, 4.5);
}

QCursor PencilTool::cursor()
{
    return Qt::BlankCursor;
}

void PencilTool::pointerPressEvent(PointerEvent *event)
{
    if (mEditor && mEditor->layers()) {
        Layer* layer = mEditor->layers()->currentLayer();
        Q_UNUSED(layer);
    }

    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();
    mLastDabPoint = mLastBrushPoint;
    mAccumulatedDistance = 0.0;
    mLastSegmentPoint = mLastBrushPoint;
    mVisitedDabCells.clear();
    mLastRibbonPoint = mLastBrushPoint;
    mStrokeTextureAnchorCanvas = mLastBrushPoint;

    // Ensure texture and pixmap cache are up-to-date once per stroke,
    // not per dab segment (avoids write lock acquisition in the hot path).
    rebuildWorkingTextureFromBase();
    if (mTextureWorkingPixmapDirty || mTextureWorkingPixmap.isNull()) {
        mTextureWorkingPixmap = QPixmap::fromImage(mTextureSource);
        mTextureWorkingPixmapDirty = false;
    }

    // Invalidate the cached texture transform so the first dab of this stroke
    // always recomputes it with the correct anchor/offset.
    mCachedTexXf.quantizedAngle = INT_MIN;

    mCachedPressureBucket = -1;
    mCachedThicknessFactor = 1.0;
    mCachedOpacityBucket = -1;
    mCachedOpacityFactor = 1.0;

    // Seed per-stroke RNG once — all per-dab random calls use mStrokeRng
    // to avoid the global generator's mutex on every dab.
    mStrokeRng = QRandomGenerator(QRandomGenerator::global()->generate());

    // Build and shuffle the rotation deck for this stroke.
    // mRotationJitterRangeDegrees now stores the active angle count (0–12).
    // Angles are 0°, 30°, 60°, ... up to count-1 steps of 30°.
    // 0 = disabled. Fisher-Yates shuffle using mStrokeRng — zero heap allocation.
    {
        const int count = qBound(0, int(qRound(mRotationJitterRangeDegrees)), kRotationDeckSize);
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                mRotationDeck[i] = i * (360.0 / kRotationDeckSize); // 0, 30, 60, ...
            }
            // Fisher-Yates in-place shuffle of the active subset
            for (int i = count - 1; i > 0; --i) {
                const int j = int(mStrokeRng.bounded(quint32(i + 1)));
                qreal tmp = mRotationDeck[i];
                mRotationDeck[i] = mRotationDeck[j];
                mRotationDeck[j] = tmp;
            }
        }
        mRotationDeckIndex = 0;
    }

    {
        static const qreal kStrokeTextureAngles[] = { 0.0, 90.0, 180.0, 240.0 };
        int idx = QRandomGenerator::global()->bounded(4);
        mStrokeTextureRotationDeg = kStrokeTextureAngles[idx];
        if (mTextureLoaded && !mTextureSource.isNull()) {
            const int tw = mTextureSource.width();
            const int th = mTextureSource.height();
            if (tw > 0 && th > 0) {
                mStrokeTextureOffsetCanvas.setX(QRandomGenerator::global()->bounded(tw));
                mStrokeTextureOffsetCanvas.setY(QRandomGenerator::global()->bounded(th));
            }
        }
    }

    // Initialize pressure pipeline (stroke-level setup only).
    {
        mPressurePipeline.setSmoothingAmount(properties.pressureSmoothingAmount);
        mPressurePipeline.setTaperingStart(properties.pressureTaperingStart);
        mPressurePipeline.setTaperingEnd(properties.pressureTaperingEnd);

        qreal rawPressure = event->isTabletEvent() ? event->pressure() : mInterpolator.getPressure();
        if (rawPressure <= 0.0) rawPressure = 0.5;

        mPressurePipeline.beginStroke(rawPressure);
        mLastRawPressure = qBound<qreal>(0.0, rawPressure, 1.0);
        mCurrentPressure = rawPressure; // initial estimate; refined per dab

        mLastPipelinePoint = getCurrentPoint();
        mPipelineDistance = 0.0;

        // Initialize adaptive spacing
        mLastSpacingMult = 1.0;  // Start at base velocity factor to avoid transient
    }

    // Pen-style stabilization (Delayed/String) for Pencil tool.
    // NOTE: Pencil stores its own settings keys (pencilSmoothingMode, pencilWeightedDistance, pencilStab2Delay).
    PenSmoothingMode mode = properties.penSmoothingMode;
    if (mode == PenSmoothingMode::LEGACY) {
        mode = PenSmoothingMode::WEIGHTED;
    }
    mInterpolator.setPenSmoothingMode(mode);
    mInterpolator.setWeightedDistance(properties.weightedDistance);
    mInterpolator.setWeightedStrokeEnding(0.0);
    mInterpolator.setWeightedSmoothPressure(false);
    mInterpolator.setWeightedScalableDistance(true);
    mInterpolator.setStab2Distance(properties.stab2Distance);
    mInterpolator.setStab2Delay(properties.stab2Delay);
    mInterpolator.setStab2FinishLine(true);
    mInterpolator.setStab2StabilizeSensors(false);
    mInterpolator.setStab2ScalableDistance(true);

    // Initialize Pull cursor tracking state (String mode)
    if (mode == PenSmoothingMode::PULL) {
        mStabilizerCursorActive = true;
        mStabilizerClampedCursorPos = getCurrentPoint();
        mStabilizerPrevDistance = 0.0;
    }

    startStroke(event->inputType());

    // Initialize progressive commit counters and begin tiled stroke for real-time commits
    mDabsSinceLastCommit = 0;
    mLastCommitPoint = getCurrentPoint();
    if (mScribbleArea) {
        // Seed with current width; scribble area will handle batching and updates
        qreal baseWidth = properties.width;
        if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) baseWidth = qMax<qreal>(1.0, gw);
        }
        mScribbleArea->beginDrawingStroke(baseWidth);
    }

}

void PencilTool::pointerMoveEvent(PointerEvent* event)
{
    if (!event) {
        return;
    }

    // Pencil supports only Delayed/String.

    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    // Keep the latest raw pressure updated from the input event.
    // drawStroke() samples the pressure pipeline at dab emission time; if we
    // don't refresh mLastRawPressure here, the stroke will use a constant
    // pressure value (regression: size/opacity dynamics appear broken).
    {
        qreal rawPressure = event->isTabletEvent() ? event->pressure() : mInterpolator.getPressure();
        if (rawPressure <= 0.0) {
            rawPressure = (mLastRawPressure > 0.0) ? mLastRawPressure : 0.5;
        }
        mLastRawPressure = qBound<qreal>(0.0, rawPressure, 1.0);
    }

    const bool isDrawingMove = (event->buttons() & Qt::LeftButton) && (event->inputType() == mCurrentInputType);
    if (!isDrawingMove) {
        // Keep the canvas cursor responsive on hover immediately after tool switch/startup.
        // mCurrentInputType is only initialized on press; without this, we can early-return
        // and never call StrokeTool::pointerMoveEvent(), so the cursor overlay won't update
        // until the first click.
        StrokeTool::pointerMoveEvent(event);
    }

    if (event->inputType() != mCurrentInputType) {
        return;
    }

    // Keep interpolator in sync with live stabilization edits.
    PenSmoothingMode mode = properties.penSmoothingMode;
    if (mode == PenSmoothingMode::LEGACY) {
        mode = PenSmoothingMode::WEIGHTED;
    }
    mInterpolator.setPenSmoothingMode(mode);
    mInterpolator.setWeightedDistance(properties.weightedDistance);
    mInterpolator.setStab2Distance(properties.stab2Distance);
    mInterpolator.setStab2Delay(properties.stab2Delay);

    // Base updates current point/cursor handling.
    // Only run here for the active drawing input type to avoid double-calling.
    StrokeTool::pointerMoveEvent(event);

    if ((event->buttons() & Qt::LeftButton) && event->inputType() == mCurrentInputType) {
        drawStroke();
        // Predictive drawing: extrapolate one segment ahead using current velocity
        // so the visual stroke leads the confirmed input by ~1 frame.
        // This is the primary technique used by Procreate/CSP to feel instant.
        drawPredictiveSegment();
    }
}

void PencilTool::drawPredictiveSegment()
{
    // Predictive drawing: render one extrapolated segment ahead of confirmed input.
    //
    // How it works:
    //   1. Compute the velocity vector from the last two confirmed dab positions.
    //   2. Extrapolate a predicted next point one "dab spacing" ahead.
    //   3. Render a ribbon segment from the current confirmed point to the prediction.
    //   4. On the next real event, drawStroke() will overdraw this prediction with
    //      the actual confirmed segment — so no explicit retraction is needed.
    //
    // This makes the stroke feel instant because the display shows a segment that
    // is ~1 input event ahead of what the hardware has confirmed.

    if (!mEditor || !mScribbleArea) return;

    // Need at least two confirmed points to have a direction.
    const QPointF confirmed = getCurrentPoint();
    const QPointF prev      = mLastBrushPoint;
    const QPointF delta     = confirmed - prev;
    const qreal   deltaLen  = QLineF(prev, confirmed).length();

    // Only predict if we have meaningful movement (avoid jitter on slow strokes).
    if (deltaLen < 0.5) return;

    // Extrapolate: predict the next point by extending the current direction
    // by one dab-spacing distance. This keeps the prediction visually tight
    // and avoids over-shooting on curves.
    const qreal   spacingFraction = calculateAdaptiveSpacing(0.0, 0.0);
    qreal         baseWidth       = (mCurrentWidth > 0.0) ? mCurrentWidth : properties.width;
    const qreal   dabSpacing      = qMax<qreal>(1.0, baseWidth * spacingFraction);

    // Normalise delta and extend by one spacing unit.
    const QPointF dir       = delta / deltaLen;
    const QPointF predicted = confirmed + dir * dabSpacing;

    // Render the predicted segment using current brush parameters.
    // In gap mode (spacing > 1x diameter) skip the prediction fill entirely —
    // drawing a ribbon to the predicted point would create a visible ghost fill
    // between discrete dabs.
    if (mUserSpacingMult > 1.0) return;

    // We use the same opacity/size as the last confirmed dab so it blends seamlessly.
    const qreal opacity  = qBound<qreal>(0.0, mPencilOpacity * mCachedOpacityFactor, 1.0);
    const qreal brushSz  = qMax<qreal>(1.0, baseWidth * mCachedThicknessFactor);

    QColor strokeColor = (mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                             ? mEditor->tools()->strokeColorOverride()
                             : mEditor->color()->frontColor();
    strokeColor.setAlpha(255);

    QRectF segBounds;
    QImage seg = renderTexturedRibbonSegment(confirmed, predicted, brushSz, opacity,
                                             strokeColor, 0.0, &segBounds);
    if (!seg.isNull() && segBounds.isValid()) {
        mScribbleArea->drawImageOverlay(seg, segBounds,
                                        QPainter::CompositionMode_SourceOver, false);
        mPredictedNextPoint = predicted;
        mHasPrediction = true;
    }
}

void PencilTool::pointerReleaseEvent(PointerEvent* event)
{
    if (!event) {
        return;
    }

    // Pencil supports only Delayed/String.

    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        mStabilizerCursorActive = false;
        StrokeTool::pointerReleaseEvent(event);
        return;
    }

    if (event->inputType() != mCurrentInputType) {
        mVisitedDabCells.clear();
        mStabilizerCursorActive = false;
        StrokeTool::pointerReleaseEvent(event);
        return;
    }

    // Final stamp on release.
    qreal distance = QLineF(getCurrentPoint(), mMouseDownPoint).length();
    if (distance < 1.0) {
        // Single-tap: evaluate pressure once for this dab.
        if (properties.pressure) {
            qreal rawPressure = event->isTabletEvent() ? event->pressure() : mLastRawPressure;
            if (rawPressure <= 0.0) rawPressure = (mCurrentPressure > 0.0 ? mCurrentPressure : 0.5);
            mLastRawPressure = qBound<qreal>(0.0, rawPressure, 1.0);
            QPointF delta(0.0, 0.0);
            qreal pipelinePressure = mPressurePipeline.sample(rawPressure, 0.0, mPipelineDistance, delta);
            mCurrentPressure = pipelinePressure;
        }
        paintAt(mMouseDownPoint);
    } else {
        drawStroke();
    }

    // Finalize pressure pipeline
    mPressurePipeline.endStroke();

    // Register hybrid stroke for snapping before endStroke() clears mStrokePoints.
    // mStrokePoints holds canvas-space positions accumulated during drawStroke().
    if (mEditor && mEditor->layers()) {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer && layer->type() == Layer::BITMAP && mStrokePoints.size() >= 2) {
            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            KeyFrame* _kf = bitmapLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
            BitmapKeyFrame* _bkf = _kf ? static_cast<BitmapKeyFrame*>(_kf) : nullptr;
            BitmapImage* cellImg = _bkf ? bitmapLayer->cellBank()->getActiveInnerLayerImage(_bkf->cellId()) : nullptr;
            if (cellImg) {
                qreal baseWidth = properties.width;
                if (mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
                    const qreal gw = mEditor->tools()->globalWidth();
                    if (gw > 0.0) baseWidth = qMax<qreal>(1.0, gw);
                }
                cellImg->addHybridStroke(QVector<QPointF>(mStrokePoints.begin(), mStrokePoints.end()), baseWidth);
            }
        }
    }

    endStroke();
    mVisitedDabCells.clear();

    // Pull mode cleanup
    if (mStabilizerCursorActive && properties.penSmoothingMode == PenSmoothingMode::PULL) {
        mStabilizerCursorActive = false;
        mStabilizerPrevDistance = 0.0;
        mStabilizerClampedCursorPos = QPointF(0, 0);
    }

    StrokeTool::pointerReleaseEvent(event);
}

bool PencilTool::renderStrokeAlongPath(const QPainterPath& path)
{
    return renderStrokeAlongPath(path, 0.0, 0.0);
}

bool PencilTool::renderStrokeAlongPath(const QPainterPath& path, qreal taperStartPx, qreal taperEndPx)
{
    return renderStrokeAlongPathWithBaseWidth(path, -1.0, taperStartPx, taperEndPx);
}

bool PencilTool::renderStrokeAlongPathWithBaseWidth(const QPainterPath& path, qreal baseWidthOverride, qreal taperStartPx, qreal taperEndPx)
{
    if (!mEditor || !mScribbleArea) {
        return false;
    }

    Layer* layer = mEditor->layers() ? mEditor->layers()->currentLayer() : nullptr;
    if (!layer || layer->type() != Layer::BITMAP) {
        return false;
    }

    if (path.isEmpty()) {
        return false;
    }

    // --- Fast path flattening: walk QPainterPath elements directly ---
    // Avoids path.length() and path.pointAtPercent() which are both O(n) per call.
    // We evaluate bezier math ourselves — O(1) per element.
    auto cubicBezier = [](const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, qreal t) -> QPointF {
        const qreal mt = 1.0 - t;
        return mt*mt*mt*p0 + 3.0*mt*mt*t*p1 + 3.0*mt*t*t*p2 + t*t*t*p3;
    };

    // Flatten each element into flatPts with cumulative arc-length in flatDists.
    // Subdivision count per segment is adaptive based on chord length.
    QVector<QPointF> flatPts;
    QVector<qreal>   flatDists;
    flatPts.reserve(1024);
    flatDists.reserve(1024);

    qreal cumLen = 0.0;
    QPointF cur(0, 0);

    auto appendPt = [&](const QPointF& p) {
        if (!flatPts.isEmpty())
            cumLen += QLineF(flatPts.last(), p).length();
        flatPts.append(p);
        flatDists.append(cumLen);
        cur = p;
    };

    const int elemCount = path.elementCount();
    for (int i = 0; i < elemCount; ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.type == QPainterPath::MoveToElement) {
            appendPt(QPointF(e.x, e.y));
        } else if (e.type == QPainterPath::LineToElement) {
            appendPt(QPointF(e.x, e.y));
        } else if (e.type == QPainterPath::CurveToElement) {
            // CurveToElement is followed by 2 CurveToDataElements for cubic,
            // or Qt may emit QuadTo as cubic with repeated control point.
            if (i + 2 < elemCount) {
                const QPainterPath::Element d1 = path.elementAt(i + 1);
                const QPainterPath::Element d2 = path.elementAt(i + 2);
                const QPointF p0 = cur;
                const QPointF p1(e.x, e.y);
                const QPointF p2(d1.x, d1.y);
                const QPointF p3(d2.x, d2.y);
                // Adaptive subdivision: estimate chord vs straight-line deviation
                const qreal chord = QLineF(p0, p3).length();
                const qreal hull  = QLineF(p0, p1).length() + QLineF(p1, p2).length() + QLineF(p2, p3).length();
                const int segs = qBound(4, int((hull + chord) * 0.25) + 1, 64);
                for (int s = 1; s <= segs; ++s)
                    appendPt(cubicBezier(p0, p1, p2, p3, qreal(s) / qreal(segs)));
                i += 2;
            }
        }
    }

    const qreal totalLength = cumLen;
    if (totalLength <= 0.0 || flatPts.size() < 2) {
        return false;
    }

    struct RestoreState {
        PencilTool* self;
        qreal baseWidthOverridePrev;
        explicit RestoreState(PencilTool* s)
            : self(s)
            , baseWidthOverridePrev(s->mBaseWidthOverride)
        {
            // Pressure is set to 1.0 below (no real-time tablet input on a committed path),
            // so the thickness curve will evaluate at full pressure — which is correct.
            // All other preset properties (scatter, spacing, rotation, color jitter, size jitter,
            // etc.) are intentionally preserved so the committed stroke matches what the pencil
            // tool would produce when drawing the same path freehand.
            self->mBaseWidthOverride = -1.0;
        }
        ~RestoreState()
        {
            self->mBaseWidthOverride = baseWidthOverridePrev;
        }
    } restore(this);

    qreal baseWidth = properties.width;
    if (baseWidthOverride > 0.0) {
        baseWidth = baseWidthOverride;
    } else if (mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
        const qreal gw = mEditor->tools()->globalWidth();
        if (gw > 0.0) {
            baseWidth = qMax<qreal>(1.0, gw);
        }
    }
    baseWidth = qBound<qreal>(1.0, baseWidth, 200.0);

    mVisitedDabCells.clear();
    mSkipDedupOnce = false;

    mCurrentInputType = PointerEvent::Mouse;
    mCurrentPressure = 1.0;
    mLastRawPressure = 1.0;

    const QPointF start = flatPts.isEmpty() ? QPointF() : flatPts.first();
    mMouseDownPoint = start;
    mLastBrushPoint = start;
    mLastDabPoint = start;
    mLastRibbonPoint = start;
    mLastSegmentPoint = start;
    mAccumulatedDistance = 0.0;
    mLastPipelinePoint = start;
    mPipelineDistance = 0.0;

    mStrokeTextureAnchorCanvas = start;
    {
        static const qreal kStrokeTextureAngles[] = { 0.0, 90.0, 180.0, 240.0 };
        int idx = QRandomGenerator::global()->bounded(4);
        mStrokeTextureRotationDeg = kStrokeTextureAngles[idx];
        if (mTextureLoaded && !mTextureSource.isNull()) {
            const int tw = mTextureSource.width();
            const int th = mTextureSource.height();
            if (tw > 0 && th > 0) {
                mStrokeTextureOffsetCanvas.setX(QRandomGenerator::global()->bounded(tw));
                mStrokeTextureOffsetCanvas.setY(QRandomGenerator::global()->bounded(th));
            }
        }
    }

    // Build and shuffle rotation deck for this stroke path.
    {
        const int count = qBound(0, int(qRound(mRotationJitterRangeDegrees)), kRotationDeckSize);
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                mRotationDeck[i] = i * (360.0 / kRotationDeckSize);
            }
            for (int i = count - 1; i > 0; --i) {
                const int j = int(QRandomGenerator::global()->bounded(quint32(i + 1)));
                qreal tmp = mRotationDeck[i];
                mRotationDeck[i] = mRotationDeck[j];
                mRotationDeck[j] = tmp;
            }
        }
        mRotationDeckIndex = 0;
    }

    mPressurePipeline.setSmoothingAmount(properties.pressureSmoothingAmount);
    mPressurePipeline.setTaperingStart(properties.pressureTaperingStart);
    mPressurePipeline.setTaperingEnd(properties.pressureTaperingEnd);
    mPressurePipeline.beginStroke(mLastRawPressure);

    mDabsSinceLastCommit = 0;
    mLastCommitPoint = start;
    mScribbleArea->beginDrawingStroke(baseWidth);

    // Fixed-pixel tapering: interpret taperStartPx/taperEndPx as canvas pixels.
    // If taper lengths exceed total length, split proportionally.
    qreal startTaperLen = qMax<qreal>(0.0, taperStartPx);
    qreal endTaperLen = qMax<qreal>(0.0, taperEndPx);
    {
        const qreal totalRequested = startTaperLen + endTaperLen;
        if (totalRequested > 0.0 && totalLength > 0.0 && totalLength < totalRequested) {
            const qreal startRatio = (totalRequested > 0.0) ? (startTaperLen / totalRequested) : 0.5;
            const qreal endRatio = 1.0 - startRatio;
            startTaperLen = totalLength * startRatio;
            endTaperLen = totalLength * endRatio;
        }
    }

    const qreal spacingFraction = calculateAdaptiveSpacing(0.0, 0.0);
    const qreal dabSpacing = qMax<qreal>(0.25, baseWidth * spacingFraction);
    const int maxDabs = 100000;
    int dabCount = 0;

    auto widthAtDistance = [baseWidth, startTaperLen, endTaperLen, totalLength](qreal s) -> qreal {
        if (totalLength <= 0.0) return baseWidth;
        const qreal startRamp = (startTaperLen > 0.0) ? qMin<qreal>(1.0, s / startTaperLen) : 1.0;
        const qreal endRamp   = (endTaperLen   > 0.0) ? qMin<qreal>(1.0, (totalLength - s) / endTaperLen) : 1.0;
        return qMax<qreal>(1.0, 1.0 + (baseWidth - 1.0) * qMin(startRamp, endRamp));
    };

    // Walk the already-flattened polyline (built above via element walker) at
    // dabSpacing intervals. O(1) per dab — no Qt path queries at all.
    int flatIdx = 0;
    const int flatN = flatPts.size();

    for (qreal dist = 0.0; dist <= totalLength && dabCount < maxDabs; dist += dabSpacing) {
        while (flatIdx + 1 < flatN - 1 && flatDists[flatIdx + 1] < dist)
            ++flatIdx;
        QPointF p;
        if (flatIdx + 1 >= flatN) {
            p = flatPts.last();
        } else {
            const qreal d0 = flatDists[flatIdx], d1 = flatDists[flatIdx + 1];
            const qreal span = d1 - d0;
            p = (span < 1e-9) ? flatPts[flatIdx]
                              : flatPts[flatIdx] + ((dist - d0) / span) * (flatPts[flatIdx + 1] - flatPts[flatIdx]);
        }

        if (dabCount > 0) {
            mPipelineDistance += QLineF(mLastPipelinePoint, p).length();
            mLastPipelinePoint = p;
        }

        mBaseWidthOverride = (startTaperLen > 0.0 || endTaperLen > 0.0) ? widthAtDistance(dist) : baseWidth;

        paintAt(p);
        ++dabCount;
    }

    mScribbleArea->flushDrawingStroke();
    mScribbleArea->commitCurrentSegmentToLayer();

    mPressurePipeline.endStroke();
    mVisitedDabCells.clear();

    return true;
}

void PencilTool::paintAt(QPointF point)
{
    // === CRITICAL SAFETY CHECKS ===
    // Prevent crashes from null pointers or invalid state
    if (!mEditor || !mScribbleArea) {
        return;  // Early exit if essential components missing
    }
    
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        return;  // Only paint on bitmap layers
    }

    // Calculate base brush width from tool/global width (before pressure)
    // If renderStrokeAlongPath requested a per-sample override (tapering), honor it.
    qreal baseWidth = (mBaseWidthOverride > 0.0) ? mBaseWidthOverride : properties.width;
    if (mBaseWidthOverride <= 0.0) {
        if (mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) baseWidth = qMax<qreal>(1.0, gw);
        }
    }
    
    // Clamp base width to reasonable range
    baseWidth = qBound(1.0, baseWidth, 200.0);

    // === DUAL PRESSURE ===
    // - Thickness curve (size): controls brush diameter.
    // - Opacity curve (flow): controls per-dab opacity multiplier.
    // Each channel has its own enable flag so size and flow can be controlled
    // independently from the UI.

    const bool sizePressureOn = properties.pressure && mThicknessCurveEnabled;
    const bool flowPressureOn = properties.pressure && mOpacityCurveEnabled;

    qreal pressureValue = (sizePressureOn || flowPressureOn)
            ? qBound<qreal>(0.0, mCurrentPressure, 1.0)
            : 1.0;

    // Quantize pressure so we only recompute curve eval when the bucket changes.
    // 256 buckets gives fine-grained pressure resolution without constant re-evaluation.
    const int bucket = (sizePressureOn || flowPressureOn)
            ? qBound(0, int(qRound(pressureValue * 256.0)), 256)
            : 256;

    qreal thicknessFactor = 1.0;
    qreal opacityFactor = 1.0;

    if (sizePressureOn || flowPressureOn) {
        const qreal qPressure = qreal(bucket) / 256.0;

        // Size / thickness channel
        if (sizePressureOn) {
            if (bucket != mCachedPressureBucket) {
                qreal curveValue = qPressure;
                if (!mThicknessPressureCurve.isEmpty()) {
                    curveValue = mThicknessCurveLUT.evaluate(qPressure);
                }
                // Normalized pressure: maps 0..1 pressure to 0..1 size factor.
                // brushSize = 1.0 + (baseWidth - 1.0) * factor, so light pressure
                // always produces ~1px regardless of base size (no size-relative scaling).
                mCachedThicknessFactor = qBound<qreal>(0.0, curveValue, 1.0);
                mCachedPressureBucket = bucket;
            }
            thicknessFactor = mCachedThicknessFactor;
        }

        // Flow / opacity channel
        if (flowPressureOn) {
            if (bucket != mCachedOpacityBucket) {
                qreal curveValue = qPressure;
                if (!mOpacityPressureCurve.isEmpty()) {
                    curveValue = mOpacityCurveLUT.evaluate(qPressure);
                }
                mCachedOpacityFactor = qBound<qreal>(0.0, curveValue, 1.0);
                mCachedOpacityBucket = bucket;
            }
            opacityFactor = mCachedOpacityFactor;
        }
    }

    // Effective brush size after pressure.
    // Additive mapping: pressure 0 → 1px, pressure 1 → baseWidth.
    // This keeps "light" and "heavy" pressure consistent regardless of base size.
    qreal brushSize = sizePressureOn
            ? qMax<qreal>(1.0, 1.0 + (baseWidth - 1.0) * thicknessFactor)
            : qMax<qreal>(1.0, baseWidth);

    // Overdap avoidance: skip stamping if we've already drawn in this brush-footprint cell.
    // One-shot reset flag (kept for API compatibility but dedup grid removed).
    mSkipDedupOnce = false;
    
    // Base opacity is slider (0..1) with optional pressure curve multiplier.
    qreal opacity = qBound<qreal>(0.0, mPencilOpacity * opacityFactor, 1.0);
    
    mCurrentWidth = brushSize;

    QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                               ? mEditor->tools()->strokeColorOverride()
                               : mEditor->color()->frontColor();
    strokeColor.setAlpha(255);  // Pencil tool handles opacity separately - color should be fully opaque

    // Scatter: in gap mode (discrete dabs) use full 2D random offset — each dab is
    // independent so any direction makes sense, matching TVPaint/CSP behaviour.
    // In ribbon mode (continuous stroke) scatter is applied perpendicular to the
    // stroke direction only, so the ribbon stays on-path and doesn't skew diagonally.
    QPointF jitteredPoint = point;
    if (mScatterAmount > 0.0) {
        const qreal jitterAmount = brushSize * mScatterAmount;
        if (mUserSpacingMult > 1.0) {
            // Gap mode: full 2D scatter
            const qreal jx = (mStrokeRng.generateDouble() * 2.0 - 1.0) * jitterAmount;
            const qreal jy = (mStrokeRng.generateDouble() * 2.0 - 1.0) * jitterAmount;
            jitteredPoint = point + QPointF(jx, jy);
        } else {
            // Ribbon mode: perpendicular-only scatter
            const QPointF strokeDir = (mLastRibbonPoint != point)
                ? (point - mLastRibbonPoint)
                : QPointF(1.0, 0.0);
            const qreal len = std::sqrt(strokeDir.x() * strokeDir.x() + strokeDir.y() * strokeDir.y());
            const QPointF perp = (len > 0.0001)
                ? QPointF(-strokeDir.y() / len, strokeDir.x() / len)
                : QPointF(0.0, 1.0);
            const qreal offset = (mStrokeRng.generateDouble() * 2.0 - 1.0) * jitterAmount;
            jitteredPoint = point + perp * offset;
        }
    }
    
    // Rotation jitter: pick next angle from the shuffled deck.
    // When the deck is exhausted, reshuffle the active subset and restart.
    qreal rotationJitterDeg = 0.0;
    const int rotCount = qBound(0, int(qRound(mRotationJitterRangeDegrees)), kRotationDeckSize);
    if (rotCount > 0) {
        rotationJitterDeg = mRotationDeck[mRotationDeckIndex];
        if (++mRotationDeckIndex >= rotCount) {
            // Reshuffle active subset
            for (int i = rotCount - 1; i > 0; --i) {
                const int j = int(mStrokeRng.bounded(quint32(i + 1)));
                qreal tmp = mRotationDeck[i];
                mRotationDeck[i] = mRotationDeck[j];
                mRotationDeck[j] = tmp;
            }
            mRotationDeckIndex = 0;
        }
    }

    // Color jitter: shift HSV around the current color, bounded by jitter amount.
    // - jitterAmount == 0 => no change
    // - Higher values => broader hue and sat/value variation.
    QColor jitteredColor = strokeColor;
    if (mColorJitterAmount > 0.0) {
        qreal h = 0.0;
        qreal s = 0.0;
        qreal v = 0.0;
        jitteredColor.getHsvF(&h, &s, &v);

        // If the color is achromatic (black/white/grey), getHsvF returns h=-1.
        // Treat that as hue=0 so jitter doesn't produce an out-of-range value.
        if (h < 0.0) h = 0.0;

        const qreal a = qBound<qreal>(0.0, mColorJitterAmount, 1.0);

        // Hue: up to +/- 0.10 (36 degrees) at full jitter.
        qreal dh = (mStrokeRng.generateDouble() * 2.0 - 1.0) * (0.10 * a);

        // Saturation and value: smaller, to avoid wild brightness flicker.
        qreal ds = (mStrokeRng.generateDouble() * 2.0 - 1.0) * (0.20 * a);
        qreal dv = (mStrokeRng.generateDouble() * 2.0 - 1.0) * (0.15 * a);

        // fmod can still produce exactly 1.0 due to floating-point rounding; clamp to be safe.
        h = qBound<qreal>(0.0, std::fmod(h + dh + 1.0, 1.0), 0.9999999);
        s = qBound<qreal>(0.0, s + ds, 1.0);
        v = qBound<qreal>(0.0, v + dv, 1.0);

        jitteredColor.setHsvF(h, s, v);
        jitteredColor.setAlpha(255);
    }

    // Size jitter: each dab gets a random size multiplier within the jitter range.
    // Works in both ribbon and gap mode — in ribbon mode it makes the stroke width
    // pulse organically; in gap mode each stamp is a different size.
    qreal jitteredOpacity = opacity;
    qreal jitteredSize = brushSize;
    if (mSizeJitterAmount > 0.0) {
        const qreal jitter = (mStrokeRng.generateDouble() * 2.0 - 1.0) * mSizeJitterAmount;
        jitteredSize = qMax<qreal>(1.0, brushSize * (1.0 + jitter));
    }

    // Render: continuous ribbon segment when spacing is tight (dabs overlap),
    // or a single stamped dab when spacing is wide enough to create gaps.
    // In gap mode we render from jitteredPoint→jitteredPoint (zero-length ribbon)
    // so the ribbon fills only the dab footprint and leaves empty canvas between stamps.
    {
        const bool gapMode = (mUserSpacingMult > 1.0);
        const QPointF startPoint = gapMode ? jitteredPoint : mLastRibbonPoint;
        const qreal dist = QLineF(startPoint, jitteredPoint).length();
        const qreal maxLen = qMax<qreal>(64.0, 1800.0 - jitteredSize);
        const int parts = (maxLen > 0.0 && dist > maxLen) ? int(qCeil(dist / maxLen)) : 1;
        for (int i = 1; i <= parts; ++i) {
            const qreal t = qreal(i) / qreal(parts);
            const QPointF partEnd = startPoint + t * (jitteredPoint - startPoint);
            QRectF segBounds;
            QImage seg = renderTexturedRibbonSegment(gapMode ? jitteredPoint : mLastRibbonPoint, partEnd, jitteredSize, jitteredOpacity, jitteredColor, rotationJitterDeg, &segBounds);
            if (!seg.isNull() && segBounds.isValid() && !segBounds.isEmpty()) {
                mScribbleArea->drawImageOverlay(seg, segBounds, QPainter::CompositionMode_SourceOver, false);

                mDabsSinceLastCommit++;
                if (mProgressiveCommitEnabled) {
                    qreal distanceSinceCommit = QLineF(mLastCommitPoint, partEnd).length();
                    bool dabThresholdReached = mDabsSinceLastCommit >= mProgressiveCommitDabThreshold;
                    bool distanceThresholdReached = distanceSinceCommit >= mProgressiveCommitDistanceThreshold;
                    if (dabThresholdReached || distanceThresholdReached) {
                        mScribbleArea->flushDrawingStroke();
                        mScribbleArea->commitCurrentSegmentToLayer();
                        mScribbleArea->updateCanvasRegion(segBounds.adjusted(-2.0, -2.0, 2.0, 2.0));
                        mDabsSinceLastCommit = 0;
                        mLastCommitPoint = partEnd;
                    }
                }
            }

            // Only advance the ribbon anchor if we actually emitted the segment.
            if (!seg.isNull()) {
                mLastRibbonPoint = partEnd;
            }
        }
    }
    
    setLastProcessedPoint(point);
}

void PencilTool::drawStroke()
{
    // === CRITICAL SAFETY CHECKS ===
    // Prevent crashes from rapid drawing or invalid state
    if (!mEditor || !mScribbleArea) {
        return;  // Essential components missing
    }
    
    // Ensure hybrid stack update and cursor logic run (live preview + batching)
    StrokeTool::drawStroke();

    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    QPointF currentPoint = getCurrentPoint();
    
    // Validate current point is within reasonable bounds
    if (qAbs(currentPoint.x()) > 100000.0 || qAbs(currentPoint.y()) > 100000.0) {
        return;  // Invalid coordinate, skip this point
    }
    
    // Calculate adaptive spacing based on velocity and curvature
    qreal velocity = mInterpolator.getVelocity();
    if (mEditor->view()) {
        const qreal s = mEditor->view()->scaling();
        if (s > 0.0) velocity = velocity / s;  // Convert to canvas space
    }
    
    // Clamp velocity to reasonable range to prevent spacing issues
    velocity = qBound(0.0, velocity, 10000.0);
    
    // Calculate adaptive spacing (velocity and curvature params unused — spacing is
    // purely distance-based; kept as params for future use without API churn).
    qreal spacingMult = calculateAdaptiveSpacing(0.0, 0.0);
    qreal baseWidth = properties.width;
    if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
        const qreal gw = mEditor->tools()->globalWidth();
        if (gw > 0.0) baseWidth = qMax<qreal>(1.0, gw);
    }
    
    // Dab spacing in canvas pixels: fraction of brush diameter × user multiplier.
    // calculateAdaptiveSpacing() returns a fraction of diameter; multiply by the
    // pressure-scaled brush width to get canvas-space distance between dabs.
    qreal effectiveWidth = (mCurrentWidth > 0.0) ? mCurrentWidth : baseWidth;
    qreal dabSpacing = effectiveWidth * spacingMult;
    
    // Accumulate distance from last dab using a strictly distance-based model
    // that is independent of input event frequency. We walk along the current
    // segment from mLastSegmentPoint to currentPoint and place dabs whenever
    // the travelled distance since the last dab reaches dabSpacing.
    qreal segmentLength = QLineF(mLastSegmentPoint, currentPoint).length();
    qreal remainingSegment = segmentLength;
    QPointF segmentStart = mLastSegmentPoint;

    // Stamp dabs along the path with safety limits
    int maxDabsPerStroke = 1000;  // Prevent infinite loops from bad spacing
    int dabCount = 0;

    while (dabSpacing > 0.0 &&
           remainingSegment > 0.0 &&
           (mAccumulatedDistance + remainingSegment) >= dabSpacing &&
           dabCount < maxDabsPerStroke) {

        qreal distanceToNext = dabSpacing - mAccumulatedDistance;
        if (distanceToNext < 0.0) {
            distanceToNext = 0.0;
        }

        qreal t = (remainingSegment > 0.0) ? (distanceToNext / remainingSegment) : 1.0;
        if (t > 1.0) t = 1.0;

        QPointF dabPoint = segmentStart + t * (currentPoint - segmentStart);

        // Validate dab point before painting
        if (qAbs(dabPoint.x()) < 100000.0 && qAbs(dabPoint.y()) < 100000.0) {
            // Stage B→C: when we actually emit a dab, evaluate pressure once
            // for this dab via the pressure pipeline.
            if (properties.pressure) {
                qreal velocityForDab = velocity; // reuse interpolator velocity
                const QPointF prevPt = mLastPipelinePoint;
                const QPointF delta = dabPoint - prevPt;
                mPipelineDistance += QLineF(prevPt, dabPoint).length();
                const qreal distanceForDab = mPipelineDistance;

                qreal pipelinePressure = mPressurePipeline.sample(mLastRawPressure,
                                                                  velocityForDab,
                                                                  distanceForDab,
                                                                  delta);
                mCurrentPressure = pipelinePressure;
                mLastPipelinePoint = dabPoint;
            }

            // Paint dab at this point
            paintAt(dabPoint);
        }

        // Advance along the segment from the last segmentStart to this dab
        qreal travelled = QLineF(segmentStart, dabPoint).length();
        remainingSegment -= travelled;
        segmentStart = dabPoint;
        mLastDabPoint = dabPoint;
        mAccumulatedDistance = 0.0;
        dabCount++;
    }

    // Any leftover part of the segment contributes to the next spacing step
    mAccumulatedDistance += remainingSegment;
    mLastSegmentPoint = currentPoint;

    // Fallback: for tricky spacing transitions, ensure we don't completely
    // skip stamping. If no dab was placed for this segment at all, force a
    // single dab at the current point. Dedup grid still prevents oversampling.
    //
    // For the PencilTool we keep spacing purely distance-based; do not emit
    // an extra per-event fallback dab here.
    if (dabCount == 0 && dabSpacing > 0.0) {
        // No-op: distance-based spacing only.
    }

    mLastBrushPoint = currentPoint;
}

