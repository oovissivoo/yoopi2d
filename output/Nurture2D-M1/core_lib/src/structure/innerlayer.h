#ifndef INNERLAYER_H
#define INNERLAYER_H

#include <QString>
#include <QPainter>
#include <QColor>

class BitmapImage;
class QUndoStack;

enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Luminosity
};

QPainter::CompositionMode blendModeToCompositionMode(BlendMode mode);

struct InnerLayer {
    BitmapImage* image     = nullptr;  // owned
    QUndoStack*  undoStack = nullptr;  // owned

    QString   name;
    bool      visible   = true;
    bool      locked    = false;      // when true, drawing tools skip this layer
    qreal     opacity   = 1.0;        // [0.0, 1.0]
    BlendMode blendMode = BlendMode::Normal;
    QColor    tintColor;              // invalid = no tint (non-destructive color overlay)

    // Constructs a new InnerLayer with a blank BitmapImage and a new QUndoStack
    explicit InnerLayer(const QString& layerName = QString());

    // Takes ownership of an existing BitmapImage; creates a new QUndoStack
    InnerLayer(BitmapImage* existingImage, const QString& layerName);

    ~InnerLayer();

    // Non-copyable (owns heap resources)
    InnerLayer(const InnerLayer&)            = delete;
    InnerLayer& operator=(const InnerLayer&) = delete;

    // Movable
    InnerLayer(InnerLayer&& other) noexcept;
    InnerLayer& operator=(InnerLayer&& other) noexcept;

    // Clamps value to [0.0, 1.0]
    void setOpacity(qreal value);
};

#endif // INNERLAYER_H
