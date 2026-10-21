
#pragma once

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPixmap>
#include <QPen>
#include <QVector>
#include <QPoint>
#include <QPointF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QTabletEvent>

// Structure to hold point position and pressure
struct StrokePoint {
    QPointF pos;
    qreal pressure;
    
    StrokePoint() : pos(), pressure(1.0) {}
    StrokePoint(const QPointF &p, qreal pr = 1.0) : pos(p), pressure(pr) {}
};

class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CanvasView(QWidget *parent = nullptr);
    void setSmoothFactor(float factor);
    void setSmoothIterations(int iterations);

    void setBrushSize(int size);
    void setPenColor(const QColor &color);
    void clearCanvas();

    bool canUndo() const;
    bool canRedo() const;

public slots:
    void undo();
    void redo();

signals:
    void undoAvailabilityChanged(bool available);
    void redoAvailabilityChanged(bool available);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void tabletEvent(QTabletEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void initializeScene();
    void refreshBasePixmap(const QRect &dirtyRegion = QRect());
    QPointF mapToCanvas(const QPoint &viewPoint) const;
    void pushUndoState();
    void updateUndoRedoState();
    void beginStroke();
    void updateVectorPreview(const StrokePoint &pt);
    void commitVectorStroke();
    QVector<StrokePoint> smoothPoints(const QVector<StrokePoint> &points, float factor, int iterations) const;
    QVector<StrokePoint> generateCurvePoints(const QVector<StrokePoint> &controlPoints) const;
    void drawRasterStroke(QPainter &painter, const QVector<StrokePoint> &points, int basePenWidth);

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_baseItem;
    QGraphicsPixmapItem *m_previewItem; // Raster preview layer
    QImage m_baseImage;
    QPixmap m_basePixmap;
    QImage m_previewImage; // Live drawing preview
    QPen m_pen;
    bool m_isDrawing;
    bool m_spacePressed;
    bool m_isPanning;
    QPoint m_lastMousePos;
    QPointF m_lastCanvasPos;
    QVector<StrokePoint> m_vectorPoints;
    QVector<StrokePoint> m_rawStrokePoints; // Raw input points with pressure
    QPointF m_lastAddedPoint; // Last point actually added to stroke (for distance filtering)
    qreal m_currentPressure;
    bool m_tabletInUse;
    QVector<QImage> m_undoStack;
    QVector<QImage> m_redoStack;
    qreal m_currentScale;
    float m_smoothFactor = 0.0f; // 0.0 = disabled, 1.0 = strong
    int m_smoothIterations = 0; // 0-10 smoothing passes
    float m_tabletSmoothMultiplier = 4.0f; // Multiply smooth factor for tablets
    int m_tabletIterationsBoost = 6; // Add extra iterations for tablets
};





