#ifndef COSMICVSLIDER_H
#define COSMICVSLIDER_H

#include <QSlider>
#include <QColor>

/**
 * CosmicVSlider - Vertical cosmic-style opacity slider.
 *
 * Visual design:
 * - Orientation is always vertical.
 * - Background: solid rectangle using color #1f1f1f (1f1f1fff).
 * - Handle: horizontal bar spanning almost full width, color #808080.
 * - No numeric text; the control is purely visual, optimized for compact slot layouts.
 *
 * Behavior:
 * - Inherits QSlider value/range API and signals.
 * - Click + drag anywhere on the control to adjust the value.
 */
class CosmicVSlider : public QSlider
{
    Q_OBJECT

public:
    explicit CosmicVSlider(QWidget* parent = nullptr);
    explicit CosmicVSlider(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~CosmicVSlider() override;

    void setBackgroundColor(const QColor& color);
    void setHandleColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    int valueFromPosition(const QPoint& pos) const;

    QColor mBackgroundColor;
    QColor mHandleColor;
    QColor mHandleHoverColor;

    bool mDragging = false;
    bool mHover = false;
};

#endif // COSMICVSLIDER_H
