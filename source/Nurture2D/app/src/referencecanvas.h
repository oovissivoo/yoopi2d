#ifndef REFERENCECANVAS_H
#define REFERENCECANVAS_H

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QTimer>
#include <QKeyEvent>

class ReferenceCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ReferenceCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setZoomScale(double scale); // scale = 1.0 = 100%
    double zoomScale() const { return mZoomScale; }
    void fitToView();
    QSize imageSize() const { return mImage.size(); }

    void setColorSampleEnabled(bool enabled) { mColorSampleEnabled = enabled; }
    void updateEyedropperCursor(const QColor& color);

signals:
    void colorSampled(const QColor& c);
    void requestZoomIn();
    void requestZoomOut();
    void requestZoomPreset(int percent);
    void requestScaleBy(double factor);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;

private:
    QImage mImage;
    double mZoomScale = 1.0;
    bool mIsPanning = false;
    QPoint mPanLastPos;
    bool mColorSampleEnabled = true;
    bool mIsSampling = false;
};

#endif // REFERENCECANVAS_H


