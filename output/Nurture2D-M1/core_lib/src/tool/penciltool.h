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

#ifndef PENCILTOOL_H
#define PENCILTOOL_H

#include "stroketool.h"
#include "pressurepipeline.h"
#include "pressurecurvelut.h"
#include <QImage>
#include <QCache>
#include <QMutex>
#include <QReadWriteLock>
#include <QRandomGenerator>
#include <QByteArray>
#include <QSet>
#include <QtGlobal>
#include <QPixmap>
 #include <QVector>
 #include <QPointF>

class Layer;

class PencilTool : public StrokeTool
{
    Q_OBJECT
public:
    PencilTool(QObject* parent = 0);
    ToolType type() override { return PENCIL; }
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void resetToDefault() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    bool renderStrokeAlongPath(const QPainterPath& path);
    bool renderStrokeAlongPath(const QPainterPath& path, qreal taperStartPx, qreal taperEndPx);
    bool renderStrokeAlongPathWithBaseWidth(const QPainterPath& path, qreal baseWidthOverride, qreal taperStartPx, qreal taperEndPx);

    void drawStroke();
    void paintAt(QPointF point);

    void setWidth(const qreal width) override;
    void setPressure(const bool pressure) override;
    void setAA(const int AA) override;

    // Pencil-specific pressure curve (dual curves: one for opacity, one for thickness)
    void setOpacityPressureCurve(const QVector<QPointF>& points);
    QVector<QPointF> getOpacityPressureCurve() const;
    void setThicknessPressureCurve(const QVector<QPointF>& points);
    QVector<QPointF> getThicknessPressureCurve() const;
    
    bool isOpacityCurveEnabled() const { return mOpacityCurveEnabled; }
    void setOpacityCurveEnabled(bool enabled) { mOpacityCurveEnabled = enabled; }
    bool isThicknessCurveEnabled() const { return mThicknessCurveEnabled; }
    void setThicknessCurveEnabled(bool enabled) { mThicknessCurveEnabled = enabled; }
    
    // Opacity slider value (0-1 range)
    qreal getPencilOpacity() const { return mPencilOpacity; }
    void setPencilOpacity(qreal value) { 
        value = qBound(0.0, value, 1.0);
        if (!qFuzzyCompare(mPencilOpacity, value)) {
            mPencilOpacity = value; 
            markSettingsDirty(); 
        }
    }

    qreal getTextureTileScale() const { return mTextureTileScale; }
    void setTextureTileScale(qreal value) {
        value = qBound<qreal>(0.25, value, 16.0);
        if (!qFuzzyCompare(mTextureTileScale, value)) {
            mTextureTileScale = value;
            markSettingsDirty();
        }
    }

    int getTextureContrastPercent() const {
        const qreal g = qBound<qreal>(0.0, (mTextureMaskGamma - 1.0) / 2.0, 1.0);
        const qreal c = qBound<qreal>(0.0, mTextureMaskCutoff / 0.45, 1.0);
        return qBound(0, int(qRound((0.6 * g + 0.4 * c) * 100.0)), 100);
    }
    void setTextureContrastPercent(int percent) {
        percent = qBound(0, percent, 100);
        const qreal t = qreal(percent) / 100.0;
        const qreal gamma  = 1.0 + 2.0 * t;  // 1.0 → 3.0
        const qreal cutoff = 0.45 * t;         // 0.0 → 0.45
        if (!qFuzzyCompare(mTextureMaskGamma, gamma) || !qFuzzyCompare(mTextureMaskCutoff, cutoff)) {
            mTextureMaskGamma = gamma;
            mTextureMaskCutoff = cutoff;
            markSettingsDirty();
            rebuildWorkingTextureFromBase();
            clearDabCache();
        }
    }

    qreal getUserSpacingMult() const { return mUserSpacingMult; }
    void setUserSpacingMult(qreal value);
    qreal getScatterAmount() const { return mScatterAmount; }
    void setScatterAmount(qreal value);
    qreal getRotationJitterRangeDegrees() const { return mRotationJitterRangeDegrees; }
    void setRotationJitterRangeDegrees(qreal value);
    qreal getSizeJitterAmount() const { return mSizeJitterAmount; }
    void setSizeJitterAmount(qreal value);
    qreal getColorJitterAmount() const { return mColorJitterAmount; }
    void setColorJitterAmount(qreal value);

    bool loadDefaultSettings();

    bool loadTexture();
    bool setCustomTextureFromFile(const QString& path);
    bool setCustomTextureFromData(const QByteArray& data);
    void clearCustomTexture();
    QImage currentTextureForPreview() const { return mTextureSource; }
    QImage defaultTextureForPreview() const { return mDefaultTextureSource; }

    struct PreviewPreset
    {
        qreal spacingMult = 1.0;
        qreal scatter = 0.15;
        qreal rotationDeg = 40.0;
        qreal sizeJitter = 0.10;
        qreal colorJitter = 0.0;
        qreal textureTileScale = 12.0;
        int textureContrastPercent = 50;
        QString textureAssetId;   // informational only
        QByteArray textureBytes;  // pre-loaded by caller via BrushAssetStore
        // Tip
        enum class TipMode { Hard, Soft, Custom };
        TipMode tipMode = TipMode::Hard;
        QByteArray tipBytes;      // pre-loaded by caller via BrushAssetStore (empty = built-in)
        int tipContrastPercent = 50;
        QVector<QPointF> thicknessCurvePoints;
        QVector<QPointF> opacityCurvePoints;
        bool thicknessCurveEnabled = true;
        bool opacityCurveEnabled = true;
    };

    static QImage renderPresetPreviewStroke(const PreviewPreset& preset,
                                            const QSize& pixelSize,
                                            quint32 seed);

    // Built-in tip shape masks — baked radial gradients, generated once at startup.
    // Hard: tight falloff, crisp edge. Medium: balanced (default for pencil). Soft: wide feather.
    enum class CosmicTipShape { Hardest, Hard, Medium, Soft, Custom };
    static void initTipMasks();  // call once at app startup
    void setTipShape(CosmicTipShape shape) { mTipShape = shape; }
    CosmicTipShape getTipShape() const { return mTipShape; }

    // Import a custom tip mask from raw file bytes.
    // Processes greyscale→alpha at import time (black=transparent, white=opaque).
    // Returns false if the data can't be decoded.
    bool setCustomTipFromData(const QByteArray& data);

    // Apply contrast to the custom tip mask (0–100, same mapping as texture contrast).
    void setTipContrastPercent(int percent);
    int  getTipContrastPercent() const;

    // Load the plain white texture (no grain) — used as default for user presets.
    void setPlainWhiteTexture();

private:
    void rebuildWorkingTipMask(); // applies gamma/cutoff LUT to mCustomTipBase → mCustomTipMask

    // Tip shape mask applied as Pass 3 in ribbon rendering to round off square texture edges.
    // Masks are baked once at startup and scaled per-segment — zero runtime cost.
    static QImage sTipMaskHardest;
    static QImage sTipMaskHard;
    static QImage sTipMaskMedium;
    static QImage sTipMaskSoft;
    static bool   sTipMasksReady;
    QImage         mCustomTipBase;   // raw greyscale→alpha bake, never modified after import
    QImage         mCustomTipMask;   // contrast-adjusted working copy used in rendering
    qreal          mTipMaskGamma  { 2.2 };
    qreal          mTipMaskCutoff { 0.25 };
    qreal          mBakedTipGamma  { -1.0 };
    qreal          mBakedTipCutoff { -1.0 };
    CosmicTipShape mTipShape { CosmicTipShape::Medium };

    // Load and cache the default texture as alpha mask
    bool applyTextureImage(const QImage& sourceImage);
    void rebuildWorkingTextureFromBase();
    void clearDabCache();
    
    // Pre-render texture dab in worker thread (thread-safe)
    QImage prerenderTextureDab(qreal size, qreal rotation, qreal opacity, const QColor& color);
    // Schedule pre-render in QtConcurrent thread pool (non-blocking). Populates cache.
    void schedulePrerenderDab(qreal size, qreal rotation, qreal opacity, const QColor& color);
    // Warm a small set of likely-to-be-needed dabs around current params
    void warmNextDabs(qreal baseSize, qreal opacity, const QColor& color);

    QImage renderTexturedRibbonSegment(const QPointF& a,
                                      const QPointF& b,
                                      qreal brushSize,
                                      qreal opacity,
                                      const QColor& color,
                                      qreal extraRotationDeg,
                                      QRectF* outCanvasBounds);
    
    // Calculate adaptive spacing based on velocity and curvature
    qreal calculateAdaptiveSpacing(qreal velocity, qreal curvature);

    // Predictive drawing: render one extrapolated segment ahead of confirmed input
    void drawPredictiveSegment();

    QPointF mLastBrushPoint;
    QPointF mMouseDownPoint;
    QPointF mLastDabPoint;  // Last stamped position

    // Texture source (P1.png loaded as greyscale alpha mask)
    // mTextureLock guards mTextureSource and mTextureLoaded against concurrent
    // access from the background dab prerender thread.
    mutable QReadWriteLock mTextureLock;
    QImage mTextureSource;             // working texture used for sampling (alpha already baked)
    QImage mTextureBaseSource;         // base texture (alpha mask, un-contrasted)
    QImage mDefaultTextureSource;      // working default
    QImage mDefaultTextureBaseSource;  // base default
    bool mTextureLoaded;
    bool mHasDefaultTexture = false;

    qreal mBakedGamma { -1.0 };
    qreal mBakedCutoff { -1.0 };

    // Scratch buffers to avoid per-segment allocations
    QImage mScratchRibbonMask;
    QImage mScratchRibbonOut;

    // Cached pixmap form of the baked working texture (updated only when the
    // working texture changes). Used by QPainter composition to avoid manual
    // per-pixel loops.
    QPixmap mTextureWorkingPixmap;
    bool mTextureWorkingPixmapDirty { true };

    // Cached texture brush transform — recomputed only when the quantized angle changes.
    // Avoids QTransform::inverted() + rotate() + scale() on every dab segment.
    struct CachedTexXf {
        int   quantizedAngle = INT_MIN; // degrees * 10, rounded to nearest 5°
        QTransform brushXf;
        QPointF lastBoundsTL;           // last bounds.topLeft() used — offset changes per segment
    };
    CachedTexXf mCachedTexXf;

    // Predictive drawing: last confirmed velocity vector (canvas space px/event)
    // used to extrapolate one segment ahead for reduced perceived latency.
    QPointF mPredictedNextPoint;
    bool    mHasPrediction { false };
    
    // Dual pressure curves for pencil
    QVector<QPointF> mOpacityPressureCurve;
    QVector<QPointF> mThicknessPressureCurve;
    bool mOpacityCurveEnabled { true };
    bool mThicknessCurveEnabled { true };
    
    // Lookup table caches for fast pressure curve evaluation
    PressureCurveLUT mOpacityCurveLUT;
    PressureCurveLUT mThicknessCurveLUT;
    
    // Opacity slider value (0-1 range, used as max when curve enabled, constant when disabled)
    qreal mPencilOpacity { 0.8 };  // Default 80%

    qreal mUserSpacingMult { 1.0 };
    qreal mScatterAmount { 0.15 };
    qreal mRotationJitterRangeDegrees { 40.0 };
    qreal mSizeJitterAmount { 0.10 };
    qreal mColorJitterAmount { 0.0 };

    QSet<quint64> mVisitedDabCells;

    // Stabilizer (String/Pull) cursor tracking (mirrors Pen tool behavior)
    QPointF mStabilizerClampedCursorPos { 0, 0 };
    qreal   mStabilizerPrevDistance { 0.0 };
    bool    mStabilizerCursorActive { false };

    // One-shot flag: when true, the next dab will bypass the per-stroke
    // dedup grid in paintAt() so we can force a dab even if the grid cell
    // was already visited (used for curvature/spacing fallbacks).
    bool mSkipDedupOnce { false };

    // Per-stroke shuffled rotation sequence.
    // 12 angles evenly distributed across [0, 360), shuffled at stroke start.
    // Exhausted in order before repeating — no clustering of the same angle.
    static constexpr int kRotationDeckSize = 12;
    int  mRotationDeckIndex { 0 };
    qreal mRotationDeck[kRotationDeckSize] {}; // pre-scaled angles for current stroke

    qreal mStrokeTextureRotationDeg { 0.0 };

    QPointF mStrokeTextureOffsetCanvas { 0.0, 0.0 };
    QPointF mStrokeTextureAnchorCanvas { 0.0, 0.0 };
    QPointF mLastRibbonPoint;

    // Pressure quantization cache (avoid recomputing thickness curve every dab)
    int mCachedPressureBucket { -1 };
    qreal mCachedThicknessFactor { 1.0 };

    int mCachedOpacityBucket { -1 };
    qreal mCachedOpacityFactor { 1.0 };

    qreal mTextureTileScale { 12.0 };
    qreal mTextureMaskGamma { 2.2 };
    qreal mTextureMaskCutoff { 0.25 };

    // Per-stroke random generator — seeded once at stroke start to avoid
    // the global generator's mutex overhead on every dab.
    QRandomGenerator mStrokeRng;

    // Adaptive spacing state
    qreal mLastSpacingMult = 0.5;
    qreal mAccumulatedDistance = 0.0;
    QPointF mLastSegmentPoint;
    
    // Progressive commit settings (direct-to-bitmap while drawing)
    bool mProgressiveCommitEnabled { true };
    int  mProgressiveCommitDabThreshold { 6 };
    qreal mProgressiveCommitDistanceThreshold { 180.0 }; // in canvas pixels
    // Progressive commit counters/state
    int mDabsSinceLastCommit { 0 };
    QPointF mLastCommitPoint { 0, 0 };

    // Optional override used by renderStrokeAlongPath() to force a per-sample base width
    // (e.g. for curve/polyline tapering). When <= 0, paintAt() uses normal width logic.
    qreal mBaseWidthOverride { -1.0 };
    
    // Pressure pipeline (always-on pressure smoothing and tapering)
    PressurePipeline mPressurePipeline;
    qreal mLastRawPressure { 0.5 };
    QPointF mLastPipelinePoint;
    qreal mPipelineDistance;
    
    // Dab cache for performance (caches pre-rendered texture dabs)
    struct DabCacheKey {
        int size;
        int rotation;
        int opacity;
        QRgb color;
        
        bool operator==(const DabCacheKey& other) const {
            return size == other.size && rotation == other.rotation && 
                   opacity == other.opacity && color == other.color;
        }
    };
    
    // Cache pre-rendered dabs (improves performance during drawing)
    QCache<quint64, QImage> mDabCache;
    QMutex mDabCacheMutex;
    quint64 mDabCacheHits { 0 };
    quint64 mDabCacheMisses { 0 };
    
    // Helper to generate cache key
    quint64 makeCacheKey(const DabCacheKey& key) const;
    static DabCacheKey makeDabCacheKey(qreal size, qreal rotation, qreal opacity, const QColor& color);
    
    // Static tracking of in-flight prerender tasks (shared across all tool instances)
    static QSet<quint64> sInFlightPrerenderKeys;
};

// Hash function for cache key
inline uint qHash(const quint64& key) {
    return static_cast<uint>(key ^ (key >> 32));
}

#endif // PENCILTOOL_H
