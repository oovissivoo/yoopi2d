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
#include "bitmapbucket.h"

#include <QtMath>
#include <QDebug>
#include <QVector>
#include <QPen>
#include <algorithm>

#include "editor.h"
#include "layermanager.h"

#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "structure/celllayerstack.h"
#include "managers/toolmanager.h"

BitmapBucket::BitmapBucket()
{
}


BitmapBucket::BitmapBucket(Editor* editor,
                           QColor color,
                           QRect maxFillRegion,
                           QPointF fillPoint,
                           Properties properties,
                           bool autoLineFill,
                           const QVector<QRgb>& lineColors,
                           QRgb ignoreColor,
                           bool ignoreInFillEnabled):
    mEditor(editor),
    mMaxFillRegion(maxFillRegion),
    mProperties(properties),
    mAutoLineFill(autoLineFill),
    mLineColors(lineColors),
    mIgnoreColor(ignoreColor),
    mIgnoreInFillEnabled(ignoreInFillEnabled)

{
    Layer* initialLayer = editor->layers()->currentLayer();
    int initialLayerIndex = mEditor->currentLayerIndex();
    int frameIndex = mEditor->currentFrame();

    mBucketColor = qPremultiply(color.rgba());

    mTargetFillToLayer = initialLayer;
    mTargetFillToLayerIndex = initialLayerIndex;

    mTolerance = mProperties.toleranceEnabled ? static_cast<int>(mProperties.tolerance) : 0;
    const QPoint& point = QPoint(qFloor(fillPoint.x()), qFloor(fillPoint.y()));

    Q_ASSERT(mTargetFillToLayer);

    auto* bitmapInitialLayer = static_cast<LayerBitmap*>(initialLayer);
    BitmapImage* keyImg = bitmapInitialLayer->getBitmapImageAtFrame(frameIndex);
    if (!keyImg) {
        // Handle error e.g. return or throw
        return; 
    }

    if (properties.bucketFillReferenceMode == 1) // All layers — composite of all inner layers across all visible layers
    {
        mReferenceImage = flattenBitmapLayersToImage();
    }
    else // Current — only the active inner layer (paper) of the current layer
    {
        // Retrieve the active inner layer image so the reference is limited to the
        // single paper the user is drawing on, not the composite of all papers.
        BitmapImage* activeInnerImg = nullptr;
        {
            KeyFrame* kf = bitmapInitialLayer->keyExists(frameIndex)
                           ? bitmapInitialLayer->getKeyFrameAt(frameIndex)
                           : bitmapInitialLayer->getLastKeyFrameAtPosition(frameIndex);
            if (kf) {
                BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
                CellBank* bank = bitmapInitialLayer->cellBank();
                activeInnerImg = bank->getActiveInnerLayerImage(bkf->cellId());
            }
        }
        if (activeInnerImg) {
            mReferenceImage = *activeInnerImg;
        } else {
            // Fallback to composite if inner layer API is unavailable
            mReferenceImage = *keyImg;
        }
    }

    // singleLayerImage is still needed for canUseDragToFill (uses composite for drag detection)
    BitmapImage singleLayerImage = *keyImg;
    mStartReferenceColor = mReferenceImage.constScanLine(point.x(), point.y());
    mUseDragToFill = canUseDragToFill(point, color, singleLayerImage);

    mPixelCache = new QHash<QRgb, bool>();
}

bool BitmapBucket::canUseDragToFill(const QPoint& fillPoint, const QColor& bucketColor, const BitmapImage& referenceImage)
{
    QRgb pressReferenceColorSingleLayer = referenceImage.constScanLine(fillPoint.x(), fillPoint.y());
    QRgb startRef = qUnpremultiply(pressReferenceColorSingleLayer);

    // Check if Same Color is disabled - if so, allow continuous fill even when colors match
    bool sameColorOnly = mProperties.bucketSameColorOnly;
    if (mEditor && mEditor->tools() && mEditor->tools()->currentTool()) {
        ToolType type = mEditor->tools()->currentTool()->type();
        if (type == BUCKET || type == RECT_GAP_FILL || type == LASSO_GAP_FILL) {
            sameColorOnly = mEditor->tools()->currentTool()->properties.bucketSameColorOnly;
        }
    }

    if (!sameColorOnly) {
        // When Same Color is disabled, allow continuous fill regardless of color matching
        // Only check for alpha=0 case which would make fill invisible
        if (bucketColor.alpha() == 0) {
            return false;
        }
        return true;
    }

    // Use live fill mode from tool properties when available
    int fillMode = mProperties.fillMode;
    if (mEditor && mEditor->tools() && mEditor->tools()->currentTool()) {
        ToolType type = mEditor->tools()->currentTool()->type();
        if (type == BUCKET || type == RECT_GAP_FILL || type == LASSO_GAP_FILL) {
            fillMode = mEditor->tools()->currentTool()->properties.fillMode;
        }
    }

    // When Same Color is enabled, use original logic
    if (fillMode == 0 && ((QColor(qRed(startRef), qGreen(startRef), qBlue(startRef)) == bucketColor.rgb() && qAlpha(startRef) == 255) || bucketColor.alpha() == 0)) {
        // In overlay mode: When the reference pixel matches the bucket color and the reference is fully opaque
        // Otherwise when the bucket alpha is zero.
        return false;
    } else if (fillMode == 2 && qAlpha(startRef) == 255) {
        // In behind mode: When the reference pixel is already fully opaque, the output will be invisible.
        return false;
    }

    return true;
}

bool BitmapBucket::allowFill(const QPoint& checkPoint, const QRgb& checkColor) const
{
    // A normal click to fill should happen unconditionally, because the alternative is utterly confusing.
    if (!mFilledOnce) {
        return true;
    }

    return allowContinuousFill(checkPoint, checkColor);
}

bool BitmapBucket::allowContinuousFill(const QPoint& checkPoint, const QRgb& checkColor) const
{
    if (!mUseDragToFill) {
        return false;
    }

    const QRgb& colorOfReferenceImage = mReferenceImage.constScanLine(checkPoint.x(), checkPoint.y());

    // If Same Color toggle is enabled, enforce exact match with the initially clicked reference color
    // Read the flag live from the tool properties to reflect UI changes during a drag gesture
    bool sameColorOnly = mProperties.bucketSameColorOnly;
    if (mEditor && mEditor->tools() && mEditor->tools()->currentTool()) {
        // Fallback to stored value if tool manager is unavailable
        ToolType type = mEditor->tools()->currentTool()->type();
        if (type == BUCKET || type == RECT_GAP_FILL || type == LASSO_GAP_FILL) {
            sameColorOnly = mEditor->tools()->currentTool()->properties.bucketSameColorOnly;
        }
    }
    
    // Use live fill mode from tool properties when available
    int fillMode = mProperties.fillMode;
    if (mEditor && mEditor->tools() && mEditor->tools()->currentTool()) {
        ToolType type = mEditor->tools()->currentTool()->type();
        if (type == BUCKET || type == RECT_GAP_FILL || type == LASSO_GAP_FILL) {
            fillMode = mEditor->tools()->currentTool()->properties.fillMode;
        }
    }

    if (sameColorOnly) {
        // When Same Color is enabled, avoid filling if target pixel color matches fill color
        // to avoid creating numerous seemingly useless undo operations
        if (checkColor == mBucketColor && (fillMode == 1 || qAlpha(checkColor) == 255))
        {
            return false;
        }
        
        // Strict compare on reference image color only; target pixel check keeps ignore rules
        bool sameRef = (colorOfReferenceImage == mStartReferenceColor);
        bool targetOk = (checkColor == 0 || checkColor == mStartReferenceColor);
        return sameRef && targetOk;
    }

    // When Same Color is disabled, only fill if the color under the cursor is different from bucket color
    // This check runs continuously during drag operations to prevent useless undo operations
    if (checkColor == mBucketColor && (fillMode == 1 || qAlpha(checkColor) == 255))
    {
        return false;
    }

    // When Same Color is disabled, allow continuous fill as long as the target pixel
    // is different from the bucket color (checked above).
    return true;
}

void BitmapBucket::paint(const QPointF& updatedPoint, std::function<void(BucketState, int, int)> state)
{
    const int currentFrameIndex = mEditor->currentFrame();

    // Task 8.2 (Req 3.4, 8.4): Fill results write to the Active_Inner_Layer only.
    // The reference image (mReferenceImage) already uses the composite for gap detection
    // (set in the constructor). The write target must be the active inner layer so that
    // other inner layers remain unchanged.
    auto* bitmapLayer = static_cast<LayerBitmap*>(mTargetFillToLayer);
    BitmapImage* targetImage = nullptr;
    CellLayerStack* stack = nullptr;
    {
        KeyFrame* kf = bitmapLayer->keyExists(currentFrameIndex)
                       ? bitmapLayer->getKeyFrameAt(currentFrameIndex)
                       : bitmapLayer->getLastKeyFrameAtPosition(currentFrameIndex);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            CellBank* bank = bitmapLayer->cellBank();
            targetImage = bank->getActiveInnerLayerImage(bkf->cellId());
            stack = bank->getCellLayerStack(bkf->cellId());
        }
    }
    if (!targetImage) {
        // Fallback to composite (legacy path) if inner layer API unavailable
        targetImage = bitmapLayer->getLastBitmapImageAtFrame(currentFrameIndex, 0);
    }
    if (targetImage == nullptr || !targetImage->isLoaded()) { return; } // Can happen if the first frame is deleted while drawing

    QPoint point = QPoint(qFloor(updatedPoint.x()), qFloor(updatedPoint.y()));
    if (!mReferenceImage.contains(point))
    {
        // If point is outside the our max known fill area, move the fill point anywhere within the bounds
        point = mReferenceImage.topLeft();
    }

    const QRgb& targetPixelColor = targetImage->constScanLine(point.x(), point.y());

    // Prevent fill if starting pixel matches ignore color, only if enabled
    if (mIgnoreInFillEnabled && mIgnoreColor != 0 && targetPixelColor == mIgnoreColor) {
        return;
    }

    if (!allowFill(point, targetPixelColor)) {
        return;
    }

    // Respect live UI state: read current tool properties for colorReplaceMode/fillMode when available
    bool colorReplaceMode = mProperties.colorReplaceMode;
    int fillMode = mProperties.fillMode;
    if (mEditor && mEditor->tools() && mEditor->tools()->currentTool()) {
        ToolType type = mEditor->tools()->currentTool()->type();
        if (type == BUCKET || type == RECT_GAP_FILL || type == LASSO_GAP_FILL) {
            const auto& tp = mEditor->tools()->currentTool()->properties;
            colorReplaceMode = tp.colorReplaceMode;
            fillMode = tp.fillMode;
        }
    }

    // Apply virtual gap-fill bridges on the reference image before computing the fill region
    applyGapFillBridges(point, targetImage);

    // If color replace mode is enabled: replace all pixels matching the reference color across the reference image
    if (colorReplaceMode)
    {
        const QRgb referenceColor = mReferenceImage.constScanLine(point.x(), point.y());
        // Build a mask image for all matching pixels per tolerance
        QRect bounds = mReferenceImage.bounds();
        BitmapImage mask(bounds, QColor(0,0,0,0));
        QHash<QRgb, bool> cache;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                const QRgb refPx = mReferenceImage.constScanLine(x, y);
                if (BitmapImage::compareColor(refPx, referenceColor, mTolerance, &cache)) {
                    mask.scanLine(x, y, qRgba(255,255,255,255));
                }
            }
        }

        state(BucketState::WillFillTarget, mTargetFillToLayerIndex, currentFrameIndex);

        // Create color image with desired bucket color but preserve original alpha per pixel on target
        BitmapImage colorImg(bounds, QColor::fromRgba(mBucketColor));
        // Use mask as alpha for color image
        colorImg.paste(&mask, QPainter::CompositionMode_DestinationIn);

        // If filling on current layer only, apply on that; otherwise behavior already uses flattened reference but writes to current
        // First clear matched pixels (DestinationOut with mask)
        targetImage->paste(&mask, QPainter::CompositionMode_DestinationOut);
        // Then paint color preserving alpha shape
        targetImage->paste(&colorImg, QPainter::CompositionMode_SourceOver);

        if (mAutoLineFill && !mLineColors.isEmpty())
        {
            fillLinePixels(targetImage, &mask);
        }

        targetImage->modification();
        // Invalidate composite so CanvasPainter recomputes it on next render (Req 3.4, 8.4)
        if (stack) stack->invalidateComposite();
        state(BucketState::DidFillTarget, mTargetFillToLayerIndex, currentFrameIndex);
        mFilledOnce = true;
        return;
    }

    QRgb fillColor = mBucketColor;
    if (mProperties.fillMode == 1)
    {
        // Pass a fully opaque version of the new color to floodFill
        // This is required so we can fully mask out the existing data before
        // writing the new color.
        QColor tempColor;
        tempColor.setRgba(fillColor);
        tempColor.setAlphaF(1);
        fillColor = tempColor.rgba();
    }

    BitmapImage* replaceImage = nullptr;

    int expandValue = 0;
    bool didFloodFill = BitmapImage::floodFill(&replaceImage,
                           &mReferenceImage,
                           mMaxFillRegion,
                           point,
                           fillColor,
                           mTolerance,
                           expandValue);

    if (!didFloodFill) {
        delete replaceImage;
        return;
    }
    Q_ASSERT(replaceImage != nullptr);

    state(BucketState::WillFillTarget, mTargetFillToLayerIndex, currentFrameIndex);

    if (fillMode == 0)
    {
        targetImage->paste(replaceImage);
    }
    else if (fillMode == 2)
    {
        targetImage->paste(replaceImage, QPainter::CompositionMode_DestinationOver);
    }
    else
    {
        // fill mode replace
        targetImage->paste(replaceImage, QPainter::CompositionMode_DestinationOut);
        // Reduce the opacity of the fill to match the new color
        BitmapImage properColor(replaceImage->bounds(), QColor::fromRgba(mBucketColor));
        properColor.paste(replaceImage, QPainter::CompositionMode_DestinationIn);
        // Write reduced-opacity fill image on top of target image
        targetImage->paste(&properColor);

    }

    if (mAutoLineFill && !mLineColors.isEmpty())
    {
        fillLinePixels(targetImage, replaceImage);
    }

    targetImage->modification();
    delete replaceImage;

    // Invalidate composite so CanvasPainter recomputes it on next render (Req 3.4, 8.4)
    if (stack) stack->invalidateComposite();

    state(BucketState::DidFillTarget, mTargetFillToLayerIndex, currentFrameIndex);
    mFilledOnce = true;
}

void BitmapBucket::fillLinePixels(BitmapImage* targetImage, BitmapImage* filledRegion)
{
    // Flood-fill adjacent outline pixels of allowed colours using an 8-way BFS that keeps growing
    // until no further connected outline pixels remain.  The search is limited only by the bitmap
    // bounds, so even "thick" outlines will be completely consumed.
    if (!targetImage || !filledRegion) return;

    // When filling with "All layers" reference mode, line pixels may live on other layers.
    // Use mReferenceImage (the composite) for line color detection so Auto Line Fill works
    // regardless of which layer the lines are on. We still write the fill color to targetImage.
    const BitmapImage* lineSourceImage = (mProperties.bucketFillReferenceMode == 1)
                                         ? &mReferenceImage
                                         : targetImage;

    // Use the union of target and reference bounds so we don't miss any line pixels
    QRect targetBounds = targetImage->bounds();
    if (mProperties.bucketFillReferenceMode == 1 && !mReferenceImage.bounds().isEmpty()) {
        targetBounds = targetBounds.united(mReferenceImage.bounds());
    }
    int left = targetBounds.left();
    int right = targetBounds.right();
    int top = targetBounds.top();
    int bottom = targetBounds.bottom();
    int width = targetBounds.width();
    int height = targetBounds.height();

    QVector<bool> visited(width * height, false);
    auto index = [&](int x, int y){ return (y - top) * width + (x - left); };

    QVector<QPoint> stack;
    stack.reserve(width * height / 10);

    auto isLineColor = [&](int x, int y) {
        return mLineColors.contains(lineSourceImage->constScanLine(x, y));
    };
    QRgb fillColor = mBucketColor;

    // Initialize the line pixel stack by scanning ONLY the filled region
    QRect filledBounds = filledRegion->bounds();
    for (int y = filledBounds.top(); y <= filledBounds.bottom(); ++y) {
        for (int x = filledBounds.left(); x <= filledBounds.right(); ++x) {
            // Check if this pixel was just filled (non-transparent in filledRegion)
            if (qAlpha(filledRegion->constScanLine(x, y)) == 0) continue;

            // This pixel was just filled, so check its 8 neighbors for line colors
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < left || nx > right || ny < top || ny > bottom) continue;
                    if (isLineColor(nx, ny)) {
                        int idx = index(nx, ny);
                        if (!visited[idx]) {
                            visited[idx] = true;
                            stack.append(QPoint(nx, ny));
                        }
                    }
                }
            }
        }
    }

    // Process the stack with BFS to fill connected line pixels
    while (!stack.isEmpty()) {
        QPoint p = stack.takeLast();
        int x = p.x();
        int y = p.y();
        targetImage->scanLine(x, y, fillColor);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                int ny = y + dy;
                if (nx < left || nx > right || ny < top || ny > bottom) continue;
                int idx = index(nx, ny);
                if (!visited[idx] && isLineColor(nx, ny)) {
                    visited[idx] = true;
                    stack.append(QPoint(nx, ny));
                }
            }
        }
    }
}

void BitmapBucket::applyGapFillBridges(const QPoint& fillPoint, BitmapImage* cellImage)
{
    // Off/Low/Medium/High mapped to radii in pixels
    if (mProperties.bucketGapFillMode <= 0) {
        return;
    }

    qreal gapRadius = 0.0;
    switch (mProperties.bucketGapFillMode) {
    case 1: gapRadius = 10.0; break;  // Low (raised from 5, old Medium)
    case 2: gapRadius = 25.0; break;  // Medium (old High)
    case 3: gapRadius = 40.0; break;  // High (new, larger than old High)
    default: return;
    }

    const auto& strokes = (cellImage ? cellImage->hybridStrokes() : mReferenceImage.hybridStrokes());
    if (strokes.isEmpty()) {
        return;
    }

    struct Endpoint {
        QPointF pos;
        int strokeIndex = -1;
        bool isStart = false;
        qreal angle = 0.0;
        bool paired = false;
    };

    QVector<Endpoint> endpoints;
    endpoints.reserve(strokes.size() * 2);
    for (int si = 0; si < strokes.size(); ++si) {
        const auto& s = strokes[si];
        if (s.points.size() < 2) {
            continue;
        }
        Endpoint eStart;
        eStart.pos = s.points.first();
        eStart.strokeIndex = si;
        eStart.isStart = true;
        endpoints.push_back(eStart);

        Endpoint eEnd;
        eEnd.pos = s.points.last();
        eEnd.strokeIndex = si;
        eEnd.isStart = false;
        endpoints.push_back(eEnd);
    }

    if (endpoints.size() < 2) {
        return;
    }

    const qreal gapRadiusSq = gapRadius * gapRadius;

    // Compute polar angle around the fill point for each endpoint so we can walk them
    // in a circular order similar to an expanding balloon.
    const QPointF origin(fillPoint);
    for (Endpoint& e : endpoints) {
        const qreal dx = e.pos.x() - origin.x();
        const qreal dy = e.pos.y() - origin.y();
        e.angle = qAtan2(dy, dx);
    }

    std::sort(endpoints.begin(), endpoints.end(), [](const Endpoint& a, const Endpoint& b){
        return a.angle < b.angle;
    });

    // Walk endpoints in angular order; for each unpaired endpoint, try to pair it with
    // the next unpaired neighbor in this circular ordering, provided they are within
    // the configured gap radius. This approximates a circular balloon whose perimeter
    // touches the outline and uses adjacent contacts to define gaps.
    const int n = endpoints.size();
    for (int i = 0; i < n; ++i) {
        Endpoint& e = endpoints[i];
        if (e.paired) {
            continue;
        }

        // Search forward for the next unpaired endpoint within gap radius.
        int bestIndex = -1;
        qreal bestD2 = gapRadiusSq;
        for (int offset = 1; offset < n; ++offset) {
            int j = i + offset;
            if (j >= n) break; // do not wrap to avoid huge cross-scene bridges
            Endpoint& f = endpoints[j];
            if (f.paired) {
                continue;
            }

            const qreal dx = f.pos.x() - e.pos.x();
            const qreal dy = f.pos.y() - e.pos.y();
            const qreal d2 = dx * dx + dy * dy;
            if (d2 <= bestD2) {
                bestD2 = d2;
                bestIndex = j;
                // we keep looking in case there is an even closer neighbor ahead,
                // but still bounded by angular order.
            }
        }

        if (bestIndex < 0) {
            continue;
        }

        Endpoint& f = endpoints[bestIndex];
        e.paired = true;
        f.paired = true;

        // Snap endpoints to integer pixel positions and draw a single-pixel bridge.
        // This ensures the gap is truly closed in raster space, matching the debug overlay.
        QPoint a = e.pos.toPoint();
        QPoint b = f.pos.toPoint();

        // Sample color from reference image at the first endpoint, if available
        QColor bridgeColor = Qt::black;
        if (mReferenceImage.contains(a)) {
            QRgb c = mReferenceImage.constScanLine(a.x(), a.y());
            bridgeColor = QColor::fromRgba(c);
        }

        QPen pen(bridgeColor, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);

        // Draw into the reference image used by floodFill so the virtual bridge
        // participates in region connectivity.
        mReferenceImage.drawLine(a, b, pen, QPainter::CompositionMode_SourceOver, false);

        // Also draw into the live cell image (if provided) *behind* existing strokes so
        // the artwork remains visually clean but future fills still see a closed outline.
        if (cellImage) {
            cellImage->drawLine(a, b, pen, QPainter::CompositionMode_DestinationOver, false);
        }
    }
}

BitmapImage BitmapBucket::flattenBitmapLayersToImage()
{
    BitmapImage flattenImage = BitmapImage();
    int currentFrame = mEditor->currentFrame();
    auto layerMan = mEditor->layers();
    for (int i = 0; i < layerMan->count(); i++)
    {
        Layer* layer = layerMan->getLayer(i);
        Q_ASSERT(layer);
        if (layer->type() == Layer::BITMAP && layer->visible())
        {
            BitmapImage* image = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(currentFrame);
            if (image) {
                flattenImage.paste(image);
            }
        }
    }
    return flattenImage;
}
