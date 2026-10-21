#include "brushengine.h"
#include <QDebug>
#include <QRandomGenerator>

BrushEngine::BrushEngine(QObject *parent)
    : QObject(parent)
    , m_lastAngle(0.0)
    , m_strokeInProgress(false)
{
}

BrushEngine::~BrushEngine()
{
}

void BrushEngine::setSettings(const BrushSettings &settings)
{
    m_settings = settings;
    // Clear cached brush mask when settings change
    m_brushMask = QImage();
}

BrushSettings BrushEngine::settings() const
{
    return m_settings;
}

void BrushEngine::beginStroke(const StrokePoint &point)
{
    m_lastPoint = point;
    m_strokeInProgress = true;
}

void BrushEngine::continueStroke(QImage &targetImage, const StrokePoint &point)
{
    if (!m_strokeInProgress) {
        beginStroke(point);
        applyDab(targetImage, point);
        return;
    }

    // Calculate distance between current and last point
    qreal distance = QLineF(m_lastPoint.pos, point.pos).length();
    
    // Calculate the diameter based on size and pressure
    int baseDiameter = m_settings.size;
    if (m_settings.usePressure && m_settings.useSizePressure) {
        // Apply pressure curve to get more natural pressure response
        qreal adjustedPressure = m_settings.applyPressureCurve(point.pressure);
        baseDiameter = qRound(m_settings.size * (0.3 + 0.7 * adjustedPressure));
    }
    
    // Determine spacing in pixels
    qreal spacing = qMax(1.0, m_settings.spacing * baseDiameter);
    
    // If distance is less than spacing, just apply a single dab
    if (distance < spacing) {
        // Mix pressure with last point for smoother transitions
        StrokePoint mixedPoint = point;
        mixedPoint.pressure = (m_lastPoint.pressure + point.pressure) * 0.5;
        mixedPoint.tiltX = (m_lastPoint.tiltX + point.tiltX) * 0.5;
        mixedPoint.tiltY = (m_lastPoint.tiltY + point.tiltY) * 0.5;
        mixedPoint.rotation = (m_lastPoint.rotation + point.rotation) * 0.5;
        applyDab(targetImage, mixedPoint);
    } else {
        // Number of dabs to apply along the line
        int numDabs = qCeil(distance / spacing);
        
        // Apply dabs along the line
        for (int i = 0; i < numDabs; ++i) {
            qreal t = (i + 1.0) / numDabs;
            
            // Interpolate position and pressure
            QPointF pos = m_lastPoint.pos + (point.pos - m_lastPoint.pos) * t;
            qreal pressure = m_lastPoint.pressure + (point.pressure - m_lastPoint.pressure) * t;
            
            // Create interpolated point
            StrokePoint interpPoint(pos, pressure);
            interpPoint.tiltX = m_lastPoint.tiltX + (point.tiltX - m_lastPoint.tiltX) * t;
            interpPoint.tiltY = m_lastPoint.tiltY + (point.tiltY - m_lastPoint.tiltY) * t;
            interpPoint.rotation = m_lastPoint.rotation + (point.rotation - m_lastPoint.rotation) * t;
            
            // Calculate speed for use in dynamics
            interpPoint.speed = distance / qMax(0.001, static_cast<qreal>(numDabs));
            
            // Apply dab at interpolated position
            applyDab(targetImage, interpPoint);
        }
    }
    
    m_lastPoint = point;
}

void BrushEngine::endStroke(QImage &targetImage)
{
    // Apply final dab if needed
    if (m_strokeInProgress) {
        applyDab(targetImage, m_lastPoint);
        m_strokeInProgress = false;
    }
}

void BrushEngine::drawStroke(QImage &targetImage, const StrokePoint &start, const StrokePoint &end)
{
    beginStroke(start);
    continueStroke(targetImage, end);
    endStroke(targetImage);
}

void BrushEngine::applyDab(QImage &targetImage, const StrokePoint &point)
{
    // Generate a dab image based on the current settings
    QImage dabImage = generateDabImage(point);
    int radius = dabImage.width() / 2;
    
    // Calculate the top-left position to paint the dab
    QPoint topLeft = QPoint(
        qRound(point.pos.x() - radius),
        qRound(point.pos.y() - radius)
    );
    
    // Apply scatter effect if enabled
    if (m_settings.scatterX > 0 || m_settings.scatterY > 0) {
        qreal scatterFactorX = m_settings.scatterX;
        qreal scatterFactorY = m_settings.scatterY;
        
        // Reduce scatter with pressure if enabled
        if (m_settings.usePressure && m_settings.useScatterPressure) {
            qreal adjustedPressure = m_settings.applyPressureCurve(point.pressure);
            scatterFactorX *= (1.0 - adjustedPressure * 0.8);
            scatterFactorY *= (1.0 - adjustedPressure * 0.8);
        }
        
        // Apply scatter with random offset - only generate when needed
        int maxScatterX = qRound(scatterFactorX * radius * 2);
        int maxScatterY = qRound(scatterFactorY * radius * 2);
        
        if (maxScatterX > 0 || maxScatterY > 0) {
            // Use a single random generator call for performance
            if (maxScatterX > 0) {
                topLeft.rx() += QRandomGenerator::global()->bounded(-maxScatterX, maxScatterX + 1);
            }
            if (maxScatterY > 0) {
                topLeft.ry() += QRandomGenerator::global()->bounded(-maxScatterY, maxScatterY + 1);
            }
        }
    }
    
    // Apply jitter - this is expensive, so only do it if jitter is enabled
    if (m_settings.jitter > 0) {
        int jitterAmount = qRound(m_settings.jitter * radius);
        if (jitterAmount > 0) {
            // Create a more efficient jittering approach
            QImage jitteredDab(dabImage.size(), QImage::Format_ARGB32);
            jitteredDab.fill(Qt::transparent);
            
            // Precompute the random jitter offsets for performance 
            QVector<QPoint> jitterOffsets;
            jitterOffsets.reserve(dabImage.width() * dabImage.height());
            
            for (int i = 0; i < dabImage.width() * dabImage.height(); ++i) {
                jitterOffsets.append(QPoint(
                    QRandomGenerator::global()->bounded(-jitterAmount, jitterAmount + 1),
                    QRandomGenerator::global()->bounded(-jitterAmount, jitterAmount + 1)
                ));
            }
            
            // Apply jitter in a more efficient way
            int index = 0;
            for (int y = 0; y < dabImage.height(); ++y) {
                for (int x = 0; x < dabImage.width(); ++x, ++index) {
                    QRgb pixel = dabImage.pixel(x, y);
                    if (qAlpha(pixel) > 0) {
                        QPoint offset = jitterOffsets[index];
                        int newX = x + offset.x();
                        int newY = y + offset.y();
                        
                        if (newX >= 0 && newX < jitteredDab.width() && 
                            newY >= 0 && newY < jitteredDab.height()) {
                            jitteredDab.setPixel(newX, newY, pixel);
                        }
                    }
                }
            }
            dabImage = jitteredDab;
        }
    }
    
    // Clip the portion of the dab that is visible on the target image
    QRect targetRect = QRect(topLeft, dabImage.size()).intersected(targetImage.rect());
    
    if (targetRect.isEmpty()) {
        return;
    }
    
    // Calculate source rectangle in the dab image
    QRect srcRect = QRect(targetRect.topLeft() - topLeft, targetRect.size());
    
    // For better performance, precalculate opacity based on pressure
    qreal finalOpacity = m_settings.opacity;
    if (m_settings.usePressure && m_settings.useOpacityPressure) {
        qreal adjustedPressure = m_settings.applyPressureCurve(point.pressure);
        finalOpacity = m_settings.opacity * (0.2 + 0.8 * adjustedPressure);
    }
    
    // Apply the dab to the target image with blending
    // Use scanlines for better performance
    for (int y = 0; y < targetRect.height(); ++y) {
        QRgb *targetLine = reinterpret_cast<QRgb*>(targetImage.scanLine(targetRect.y() + y));
        const QRgb *srcLine = reinterpret_cast<const QRgb*>(dabImage.constScanLine(srcRect.y() + y));
        
        for (int x = 0; x < targetRect.width(); ++x) {
            QRgb srcPixel = srcLine[srcRect.x() + x];
            
            // Only blend if the source pixel has some opacity
            if (qAlpha(srcPixel) > 0) {
                QRgb dstPixel = targetLine[targetRect.x() + x];
                
                // Apply the blending mode with opacity and flow
                targetLine[targetRect.x() + x] = applyBlendMode(srcPixel, dstPixel, 
                                                   m_settings.blendMode, 
                                                   finalOpacity, m_settings.flow);
            }
        }
    }
}

QImage BrushEngine::generateDabImage(const StrokePoint &point)
{
    // Calculate effective size based on pressure
    int effectiveSize = m_settings.size;
    if (m_settings.usePressure && m_settings.useSizePressure) {
        qreal adjustedPressure = m_settings.applyPressureCurve(point.pressure);
        effectiveSize = qMax(1, qRound(m_settings.size * (0.3 + 0.7 * adjustedPressure)));
    }
    
    // Apply tilt and rotation effects to angle and roundness
    qreal effectiveAngle = m_settings.angle;
    qreal effectiveRoundness = m_settings.roundness;
    
    // Apply tablet tilt effects if enabled
    if (m_settings.useTiltEnabled || m_settings.useRotationEnabled) {
        applyTiltEffects(point, effectiveAngle, effectiveRoundness);
    }
    
    // Generate brush mask for the current settings
    QImage mask = getBrushMask(effectiveSize, m_settings.hardness, effectiveRoundness, 
                              effectiveAngle + point.rotation);
    
    // Create a colored dab based on the brush color
    QImage dabImage(mask.size(), QImage::Format_ARGB32_Premultiplied);
    dabImage.fill(Qt::transparent);
    
    QColor brushColor = m_settings.color;
    
    // Adjust color based on the brush type
    if (m_settings.brushTip == BrushTip::Texture && !m_settings.texture.isNull()) {
        // For texture brushes, use the mask as an alpha channel for the texture
        int width = mask.width();
        int height = mask.height();
        
        // Scale the texture to match the brush size
        QImage scaledTexture = m_settings.texture.scaled(width, height, Qt::KeepAspectRatio, 
                                                       m_settings.antialias ? Qt::SmoothTransformation : Qt::FastTransformation);
        
        // Center the texture if it's not the same size as the mask
        int offsetX = (width - scaledTexture.width()) / 2;
        int offsetY = (height - scaledTexture.height()) / 2;
        
        // Apply the texture with the mask as alpha
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int alpha = qAlpha(mask.pixel(x, y));
                if (alpha > 0) {
                    // Get color from the texture if within bounds, otherwise use brush color
                    QRgb color;
                    int texX = x - offsetX;
                    int texY = y - offsetY;
                    
                    if (texX >= 0 && texX < scaledTexture.width() && 
                        texY >= 0 && texY < scaledTexture.height()) {
                        color = scaledTexture.pixel(texX, texY);
                    } else {
                        color = brushColor.rgba();
                    }
                    
                    // Apply the alpha from the mask
                    dabImage.setPixel(x, y, qRgba(qRed(color), qGreen(color), qBlue(color), 
                                              (qAlpha(color) * alpha) / 255));
                }
            }
        }
    } else {
        // For standard brushes, just use the mask with the brush color
        for (int y = 0; y < mask.height(); ++y) {
            for (int x = 0; x < mask.width(); ++x) {
                int alpha = qAlpha(mask.pixel(x, y));
                if (alpha > 0) {
                    dabImage.setPixel(x, y, qRgba(brushColor.red(), brushColor.green(), brushColor.blue(), alpha));
                }
            }
        }
    }
    
    return dabImage;
}

QImage BrushEngine::getBrushMask(int size, qreal hardness, qreal roundness, qreal angle)
{
    // If parameters are default (-1), use the current settings
    if (size < 0) size = m_settings.size;
    if (hardness < 0) hardness = m_settings.hardness;
    if (roundness < 0) roundness = m_settings.roundness;
    if (angle < 0) angle = m_settings.angle;
    
    // Create a cache key based on parameters
    // Round values to reduce cache size while maintaining quality
    int sizeKey = size;
    int hardnessKey = qRound(hardness * 20);  // 20 hardness steps (0.05 increments)
    int roundnessKey = qRound(roundness * 10); // 10 roundness steps (0.1 increments)
    int angleKey = qRound(angle * 36 / (2 * M_PI)); // 36 angle steps (10 degrees)
    
    QString cacheKey = QString("%1:%2:%3:%4:%5")
        .arg(sizeKey)
        .arg(hardnessKey)
        .arg(roundnessKey)
        .arg(angleKey)
        .arg(static_cast<int>(m_settings.brushTip));
    
    // Check if this mask is already in the cache
    if (m_brushMaskCache.contains(cacheKey)) {
        return m_brushMaskCache.value(cacheKey);
    }
    
    // Limit cache size to prevent memory issues (keep 50 most recent masks)
    constexpr int MAX_CACHE_SIZE = 50;
    if (m_brushMaskCache.size() >= MAX_CACHE_SIZE) {
        // Remove the first (oldest) item
        m_brushMaskCache.remove(m_brushMaskCache.begin().key());
    }
    
    // Generate the mask
    int diameter = size * 2;
    QImage mask(diameter, diameter, QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    
    // Center point
    qreal centerX = diameter / 2.0;
    qreal centerY = diameter / 2.0;
    
    // Maximum radius (half of size)
    qreal radius = size / 2.0;
    
    // Create transformation for rotation and shape
    QTransform transform;
    transform.translate(centerX, centerY);
    transform.rotate(qRadiansToDegrees(angle));
    transform.scale(1.0, roundness);  // Apply roundness (vertical scale)
    transform.translate(-centerX, -centerY);
    
    // Apply brush shape
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            // Transform point to apply rotation and roundness
            QPointF original(x, y);
            QPointF transformed = transform.map(original);
            
            // Calculate distance from center, accounting for the transformation
            qreal dx = transformed.x() - centerX;
            qreal dy = transformed.y() - centerY;
            qreal distance = qSqrt(dx * dx + dy * dy);
            
            // Calculate the alpha based on distance and hardness
            int alpha = 0;
            
            if (distance <= radius) {
                if (m_settings.brushTip == BrushTip::Circle) {
                    if (m_settings.antialias) {
                        // Anti-aliased circle with hardness
                        qreal t = distance / radius;
                        if (t <= hardness) {
                            // Inside the hardness radius - full opacity
                            alpha = 255;
                        } else {
                            // Transition from hardness edge to full radius
                            qreal fadeDistance = 1.0 - hardness;
                            if (fadeDistance > 0) {
                                qreal fadeT = (t - hardness) / fadeDistance;
                                alpha = qRound(255 * (1.0 - fadeT));
                            }
                        }
                    } else {
                        // Non-antialiased (pixelated) circle
                        alpha = (distance <= radius * hardness) ? 255 : 0;
                    }
                } else if (m_settings.brushTip == BrushTip::Square) {
                    // Square brush tip (based on max distance)
                    qreal maxDist = qMax(qAbs(dx), qAbs(dy)) / radius;
                    if (maxDist <= hardness) {
                        alpha = 255;
                    } else if (maxDist <= 1.0 && m_settings.antialias) {
                        qreal fadeDistance = 1.0 - hardness;
                        if (fadeDistance > 0) {
                            qreal fadeT = (maxDist - hardness) / fadeDistance;
                            alpha = qRound(255 * (1.0 - fadeT));
                        }
                    }
                } else if (m_settings.brushTip == BrushTip::Diamond) {
                    // Diamond brush tip (based on Manhattan distance)
                    qreal manhattanDist = (qAbs(dx) + qAbs(dy)) / (radius * 2);
                    if (manhattanDist <= hardness) {
                        alpha = 255;
                    } else if (manhattanDist <= 1.0 && m_settings.antialias) {
                        qreal fadeDistance = 1.0 - hardness;
                        if (fadeDistance > 0) {
                            qreal fadeT = (manhattanDist - hardness) / fadeDistance;
                            alpha = qRound(255 * (1.0 - fadeT));
                        }
                    }
                }
            }
            
            if (alpha > 0 && x >= 0 && x < diameter && y >= 0 && y < diameter) {
                mask.setPixel(x, y, qRgba(255, 255, 255, alpha));
            }
        }
    }
    
    // Cache the mask for future use
    m_brushMaskCache.insert(cacheKey, mask);
    
    return mask;
}

// Static method to apply blend mode to colors
QRgb BrushEngine::applyBlendMode(QRgb src, QRgb dst, BlendMode mode, qreal opacity, qreal flow)
{
    // Apply flow - reduces the intensity of the color change
    int srcA = qAlpha(src);
    int srcR = qRed(src);
    int srcG = qGreen(src);
    int srcB = qBlue(src);
    
    // Apply flow by adjusting alpha
    srcA = qRound(srcA * flow);
    
    // Early exit if source is transparent
    if (srcA <= 0) {
        return dst;
    }
    
    int dstA = qAlpha(dst);
    int dstR = qRed(dst);
    int dstG = qGreen(dst);
    int dstB = qBlue(dst);
    
    // For transparent destination, just return source with opacity
    if (dstA <= 0) {
        return qRgba(srcR, srcG, srcB, qRound(srcA * opacity));
    }
    
    // Results for the blending operation
    int resultR, resultG, resultB, resultA;
    
    // Apply different blend modes
    switch (mode) {
        case BlendMode::Normal:
            resultR = srcR;
            resultG = srcG;
            resultB = srcB;
            break;
            
        case BlendMode::Multiply:
            resultR = (srcR * dstR) / 255;
            resultG = (srcG * dstG) / 255;
            resultB = (srcB * dstB) / 255;
            break;
            
        case BlendMode::Screen:
            resultR = 255 - ((255 - srcR) * (255 - dstR)) / 255;
            resultG = 255 - ((255 - srcG) * (255 - dstG)) / 255;
            resultB = 255 - ((255 - srcB) * (255 - dstB)) / 255;
            break;
            
        case BlendMode::Overlay:
            resultR = (dstR < 128) ? (2 * dstR * srcR) / 255 : 255 - 2 * ((255 - dstR) * (255 - srcR)) / 255;
            resultG = (dstG < 128) ? (2 * dstG * srcG) / 255 : 255 - 2 * ((255 - dstG) * (255 - srcG)) / 255;
            resultB = (dstB < 128) ? (2 * dstB * srcB) / 255 : 255 - 2 * ((255 - dstB) * (255 - srcB)) / 255;
            break;
            
        case BlendMode::Dodge:
            resultR = (srcR == 255) ? 255 : qMin(255, (dstR * 255) / (255 - srcR));
            resultG = (srcG == 255) ? 255 : qMin(255, (dstG * 255) / (255 - srcG));
            resultB = (srcB == 255) ? 255 : qMin(255, (dstB * 255) / (255 - srcB));
            break;
            
        case BlendMode::Burn:
            resultR = (srcR == 0) ? 0 : qMax(0, 255 - ((255 - dstR) * 255) / srcR);
            resultG = (srcG == 0) ? 0 : qMax(0, 255 - ((255 - dstG) * 255) / srcG);
            resultB = (srcB == 0) ? 0 : qMax(0, 255 - ((255 - dstB) * 255) / srcB);
            break;
            
        case BlendMode::Darken:
            resultR = qMin(srcR, dstR);
            resultG = qMin(srcG, dstG);
            resultB = qMin(srcB, dstB);
            break;
            
        case BlendMode::Lighten:
            resultR = qMax(srcR, dstR);
            resultG = qMax(srcG, dstG);
            resultB = qMax(srcB, dstB);
            break;
            
        case BlendMode::Difference:
            resultR = qAbs(srcR - dstR);
            resultG = qAbs(srcG - dstG);
            resultB = qAbs(srcB - dstB);
            break;
            
        case BlendMode::Exclusion:
            resultR = srcR + dstR - ((srcR * dstR) / 128);
            resultG = srcG + dstG - ((srcG * dstG) / 128);
            resultB = srcB + dstB - ((srcB * dstB) / 128);
            break;
            
        // For the following modes, we'd need to convert to/from HSL/HSV space
        // Simplified implementations:
        case BlendMode::Color:
        case BlendMode::Hue:
        case BlendMode::Saturation:
        case BlendMode::Luminosity:
            // For now, default to normal blending for these complex modes
            resultR = srcR;
            resultG = srcG;
            resultB = srcB;
            break;
            
        default:
            // Fallback to normal blend mode
            resultR = srcR;
            resultG = srcG;
            resultB = srcB;
    }
    
    // Clamp results
    resultR = clamp(resultR);
    resultG = clamp(resultG);
    resultB = clamp(resultB);
    
    // Alpha compositing formula
    qreal alphaFactor = (opacity * srcA) / 255.0;
    
    // Calculate final alpha
    resultA = dstA + qRound((255 - dstA) * alphaFactor);
    
    // Calculate final colors with alpha blending
    resultR = qRound(dstR * (1.0 - alphaFactor) + resultR * alphaFactor);
    resultG = qRound(dstG * (1.0 - alphaFactor) + resultG * alphaFactor);
    resultB = qRound(dstB * (1.0 - alphaFactor) + resultB * alphaFactor);
    
    return qRgba(resultR, resultG, resultB, resultA);
}

// Generate a brush mask for preview
QImage BrushEngine::generateBrushMask(int size, qreal hardness, qreal roundness, qreal angle)
{
    return getBrushMask(size, hardness, roundness, angle);
}

// Helper function to compute pixel coverage for anti-aliasing
qreal BrushEngine::pixelCoverage(qreal x, qreal y, qreal fx, qreal fy, qreal radius, qreal hardness)
{
    // Calculate distance from center
    qreal dx = x - fx;
    qreal dy = y - fy;
    qreal distance = qSqrt(dx*dx + dy*dy);
    
    // Return coverage value based on distance and hardness
    if (distance <= radius * hardness) {
        return 1.0;  // Full coverage within hardness radius
    } else if (distance <= radius) {
        // Smooth transition from hardness edge to full radius
        qreal t = (distance - radius * hardness) / (radius * (1.0 - hardness));
        return 1.0 - t;
    }
    
    return 0.0;  // No coverage outside radius
}

// Helper function to convert QColor to QRgb with applied opacity
QRgb BrushEngine::colorWithOpacity(const QColor &color, qreal opacity)
{
    int alpha = qRound(color.alpha() * opacity);
    return qRgba(color.red(), color.green(), color.blue(), alpha);
}

// Apply tablet tilt and rotation to brush parameters
void BrushEngine::applyTiltEffects(const StrokePoint &point, qreal &effectiveAngle, qreal &effectiveRoundness)
{
    // If tilt is not enabled, no changes needed
    if (!m_settings.useTiltEnabled) {
        return;
    }
    
    // Apply tilt if it's significant enough
    if (qAbs(point.tiltX) > 1.0 || qAbs(point.tiltY) > 1.0) {
        // Scale by the tilt sensitivity setting
        qreal tiltFactor = m_settings.tiltSensitivity;
        
        // Adjust the angle based on tilt direction
        qreal tiltAngle = qAtan2(point.tiltY, point.tiltX);
        
        // Blend between the base angle and the tilt angle
        qreal tiltMagnitude = qSqrt(point.tiltX * point.tiltX + point.tiltY * point.tiltY) / 60.0; // Normalize to 0-1 range
        tiltMagnitude = qMin(tiltMagnitude, 1.0);
        
        // Apply the tilt effect to the angle
        effectiveAngle = effectiveAngle * (1.0 - tiltFactor * tiltMagnitude) + 
                        tiltAngle * tiltFactor * tiltMagnitude;
        
        // Adjust roundness based on tilt (more tilt = more elliptical)
        effectiveRoundness = effectiveRoundness * (1.0 - tiltFactor * tiltMagnitude) +
                            effectiveRoundness * 0.5 * tiltFactor * tiltMagnitude;
    }
    
    // Apply tablet rotation if enabled
    if (m_settings.useRotationEnabled && point.rotation != 0.0) {
        // Combine with any existing angle change
        effectiveAngle += point.rotation * m_settings.rotationSensitivity;
    }
    
    // Apply pressure to rotation if enabled
    if (m_settings.usePressure && m_settings.useRotationPressure) {
        qreal adjustedPressure = m_settings.applyPressureCurve(point.pressure);
        effectiveAngle += adjustedPressure * M_PI * 2.0 * m_settings.rotationSensitivity;
    }
    
    // Normalize angle to 0-2π range
    while (effectiveAngle < 0) effectiveAngle += 2.0 * M_PI;
    while (effectiveAngle > 2.0 * M_PI) effectiveAngle -= 2.0 * M_PI;
} 