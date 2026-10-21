#ifndef BRUSHENGINE_H
#define BRUSHENGINE_H

#include <QObject>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QVector>
#include <QMap>
#include <QSharedPointer>
#include <QtMath>

// Defines the way a brush blends with the canvas
enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Dodge,
    Burn,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Color,
    Luminosity,
    Hue,
    Saturation
};

// Brush tip shape types
enum class BrushTip {
    Circle,
    Square,
    Diamond,
    Texture  // Uses an image as a brush tip
};

// Stroke point data
struct StrokePoint {
    QPointF pos;           // Position of the point
    qreal pressure;        // Pressure (0.0 to 1.0)
    qreal tiltX;           // X-axis tilt (for tablet support)
    qreal tiltY;           // Y-axis tilt (for tablet support)
    qreal rotation;        // Brush rotation in radians
    qreal speed;           // Speed factor calculated from distance/time
    
    StrokePoint() : pressure(1.0), tiltX(0.0), tiltY(0.0), rotation(0.0), speed(0.0) {}
    
    StrokePoint(const QPointF &p, qreal pres = 1.0) 
        : pos(p), pressure(pres), tiltX(0.0), tiltY(0.0), rotation(0.0), speed(0.0) {}
};

// Brush settings container
class BrushSettings {
public:
    BrushSettings() 
        : size(10)
        , opacity(1.0)
        , flow(1.0)
        , spacing(0.1)  // 10% of brush diameter
        , hardness(0.8)
        , angle(0.0)
        , roundness(1.0)
        , scatterX(0.0)
        , scatterY(0.0)
        , usePressure(true)
        , useOpacityPressure(true)
        , useSizePressure(true)
        , useScatterPressure(false)
        , useRotationPressure(false)
        , useTiltEnabled(false)
        , tiltSensitivity(0.5)
        , useRotationEnabled(false)
        , rotationSensitivity(0.5)
        , texturePath("")
        , blendMode(BlendMode::Normal)
        , brushTip(BrushTip::Circle)
        , antialias(true)
        , jitter(0.0)
        , pressureCurve(0.5)  // Linear pressure curve by default (0.5 = linear)
    {}
    
    int size;               // Base brush size in pixels
    qreal opacity;          // Master opacity (0.0 to 1.0)
    qreal flow;             // Paint flow per dab (0.0 to 1.0)
    qreal spacing;          // Spacing between dabs (as fraction of diameter)
    qreal hardness;         // Brush hardness (0.0 to 1.0)
    qreal angle;            // Brush rotation angle in radians
    qreal roundness;        // Aspect ratio (1.0 = circle, < 1.0 = ellipse)
    qreal scatterX;         // Horizontal scatter
    qreal scatterY;         // Vertical scatter
    bool usePressure;       // Master pressure switch
    bool useOpacityPressure; // Apply pressure to opacity
    bool useSizePressure;   // Apply pressure to size
    bool useScatterPressure; // Apply pressure to scatter
    bool useRotationPressure; // Apply pressure to rotation
    bool useTiltEnabled;    // Enable tilt sensitivity
    qreal tiltSensitivity;  // Sensitivity to tilt (0.0 to 1.0)
    bool useRotationEnabled; // Enable rotation sensitivity
    qreal rotationSensitivity; // Sensitivity to rotation (0.0 to 1.0)
    QString texturePath;    // Path to brush texture image
    BlendMode blendMode;    // Current blend mode
    BrushTip brushTip;      // Brush tip shape
    bool antialias;         // Enable anti-aliasing
    qreal jitter;           // Random variation (0.0 to 1.0)
    QColor color;           // Brush color
    qreal pressureCurve;    // Pressure curve adjustment (0.0 to 1.0)
                            // Values < 0.5 make pressure more sensitive at light pressure
                            // Values > 0.5 make pressure more sensitive at heavy pressure
                            // 0.5 = linear
    
    // Texture image if using a texture brush tip
    QImage texture;
    
    // Load texture from file
    bool loadTexture(const QString &path) {
        if (texture.load(path)) {
            texturePath = path;
            return true;
        }
        return false;
    }
    
    // Apply pressure curve to raw pressure value
    qreal applyPressureCurve(qreal rawPressure) const {
        if (pressureCurve == 0.5) {
            return rawPressure; // Linear response
        } else if (pressureCurve < 0.5) {
            // More sensitive at lower pressures (power curve with exponent < 1)
            qreal exponent = 0.1 + 1.8 * pressureCurve; // Map 0.0-0.5 to range 0.1-1.0
            return qPow(rawPressure, exponent);
        } else {
            // More sensitive at higher pressures (power curve with exponent > 1)
            qreal exponent = 1.0 + 3.0 * (pressureCurve - 0.5); // Map 0.5-1.0 to range 1.0-2.5
            return qPow(rawPressure, exponent);
        }
    }
};

class BrushEngine : public QObject
{
    Q_OBJECT
    
public:
    explicit BrushEngine(QObject *parent = nullptr);
    ~BrushEngine();
    
    // Apply a single dab to the target image
    void applyDab(QImage &targetImage, const StrokePoint &point);
    
    // Draw a stroke between two points
    void drawStroke(QImage &targetImage, const StrokePoint &start, const StrokePoint &end);
    
    // Start a new stroke at the given point
    void beginStroke(const StrokePoint &point);
    
    // Continue the current stroke to the given point
    void continueStroke(QImage &targetImage, const StrokePoint &point);
    
    // End the current stroke
    void endStroke(QImage &targetImage);
    
    // Set the brush settings
    void setSettings(const BrushSettings &settings);
    
    // Get the current brush settings
    BrushSettings settings() const;
    
    // Generate a brush mask based on current settings (used for preview)
    QImage generateBrushMask(int size = -1, qreal hardness = -1, qreal roundness = -1, qreal angle = -1);
    
    // Apply blend mode to a source color over a destination color
    static QRgb applyBlendMode(QRgb src, QRgb dst, BlendMode mode, qreal opacity, qreal flow);
    
private:
    BrushSettings m_settings;
    StrokePoint m_lastPoint;
    qreal m_lastAngle;
    bool m_strokeInProgress;
    QImage m_brushMask;     // Cached brush mask
    
    // Cache of generated brush masks (key = size:hardness:roundness:angle)
    QMap<QString, QImage> m_brushMaskCache;
    
    // Compute an anti-aliased pixel coverage value (0.0 to 1.0)
    qreal pixelCoverage(qreal x, qreal y, qreal fx, qreal fy, qreal radius, qreal hardness);
    
    // Generate a brush dab image based on current settings
    QImage generateDabImage(const StrokePoint &point);
    
    // Get brush mask for the current settings or specified overrides
    QImage getBrushMask(int size = -1, qreal hardness = -1, qreal roundness = -1, qreal angle = -1);
    
    // Convert QColor to QRgb with applied opacity
    static QRgb colorWithOpacity(const QColor &color, qreal opacity);
    
    // Apply tablet tilt to brush parameters
    void applyTiltEffects(const StrokePoint &point, qreal &effectiveAngle, qreal &effectiveRoundness);
    
    // Utility functions for blend modes
    static inline int clamp(int value, int min = 0, int max = 255) {
        return value < min ? min : (value > max ? max : value);
    }
    
    static inline qreal clampf(qreal value, qreal min = 0, qreal max = 1) {
        return value < min ? min : (value > max ? max : value);
    }
};

#endif // BRUSHENGINE_H 