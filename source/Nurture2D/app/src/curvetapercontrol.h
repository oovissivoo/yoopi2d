#ifndef CURVETAPERCONTROL_H
#define CURVETAPERCONTROL_H

#include <QWidget>
#include <QColor>

class QLineEdit;

class CurveTaperControl : public QWidget
{
    Q_OBJECT

public:
    explicit CurveTaperControl(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setStartValue(qreal value);
    void setEndValue(qreal value);
    qreal startValue() const { return mStartValue; }
    qreal endValue() const { return mEndValue; }

    void setAccentColor(const QColor& color);

signals:
    void interactionBegun();
    void interactionEnded();
    void startTaperChanged(qreal value);
    void endTaperChanged(qreal value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum ActiveSide {
        None,
        StartSide,
        EndSide
    };

    void updateLayoutRects();
    void applyStartValue(qreal value);
    void applyEndValue(qreal value);
    void updateValueFromPos(const QPoint& pos);
    void beginInlineEdit(ActiveSide side, const QPoint& mousePos);
    void destroyInlineEditor();

    // Current values (0-200 px)
    qreal mStartValue = 0.0;
    qreal mEndValue = 0.0;

    QColor mAccentColor;

    // Geometry cache for hit testing and painting
    QRectF mLeftLaneRect;
    QRectF mRightLaneRect;
    QRectF mLeftCubeRect;
    QRectF mRightCubeRect;
    QRectF mStartCircleRect;
    QRectF mEndCircleRect;

    ActiveSide mActiveSide = None;
    bool mDragging = false;

    QLineEdit* mInlineEditor = nullptr;
    ActiveSide mInlineEditorSide = None;

    // Cached layout parameters
    qreal mCircleRadius = 14.0;
    qreal mCubeWidth = 36.0;
    qreal mCubeHeight = 26.0;
    qreal mHorizontalMargin = 12.0;
    qreal mHandleGapFromCube = 2.0;
};

#endif // CURVETAPERCONTROL_H
