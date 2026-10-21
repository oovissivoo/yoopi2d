#ifndef COSMICINTENSITYBOX_H
#define COSMICINTENSITYBOX_H

#include <QWidget>
#include <QVector>
#include <QIcon>
#include <QColor>

// CosmicIntensityBox
//
// A 4-level intensity selector for values 0..3 (Off, Low, Med, High).
// The widget is a horizontal rectangle split into 4 equal boxes.
// - Background per box:
//     * i <= value: accent color (enabled)
//     * i  > value: background color (4d4d4dfe)
// - Each inner box has a border color 1f1f1fff.
// - Icons are drawn centered in each box (off/low/med/high).
// - No hover visuals.
// - Interaction: click or click-and-sweep to change value.
// - Size policy: always expanding horizontally.
class CosmicIntensityBox : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicIntensityBox(QWidget* parent = nullptr);

    int value() const { return mValue; }

public slots:
    void setValue(int v);

    void setFillTintEnabled(bool on) { mFillTintEnabled = on; update(); }
    void setFillTintColor(const QColor& c) { mFillTintColor = c; update(); }

    // Optional visual state: when enabled, draws a horizontal black line
    // through the control to indicate it is logically disabled/ignored
    // (used for guarding Post Correction in certain modes).
    void setStrikeThrough(bool on) { mStrikeThrough = on; update(); }

signals:
    void valueChanged(int v);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    int indexFromPosition(const QPoint& pos) const;

    int mValue = 0;        // 0..3
    bool mDragging = false;

    QVector<QIcon> mIcons; // size 4

    bool mStrikeThrough = false;

    bool mFillTintEnabled = false;
    QColor mFillTintColor;
};

#endif // COSMICINTENSITYBOX_H
