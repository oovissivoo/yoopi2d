#ifndef COSMICTAPPERSLIDERS_H
#define COSMICTAPPERSLIDERS_H

#include <QWidget>
#include <QColor>

class CosmicHSlider;

/**
 * CosmicTapperSliders - dual mirrored horizontal sliders for curve taper.
 *
 * Pure visual replacement for CurveTaperControl while preserving its
 * public API and semantics (0-200 px start/end taper values).
 */
class CosmicTapperSliders : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicTapperSliders(QWidget* parent = nullptr);

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
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateSliderRanges();
    void syncSlidersFromValues();
    void applyStartFromSlider(int sliderValue);
    void applyEndFromSlider(int sliderValue);

    CosmicHSlider* mLeftSlider = nullptr;
    CosmicHSlider* mRightSlider = nullptr;

    // 0-200 px, same semantics as CurveTaperControl
    qreal mStartValue = 0.0;
    qreal mEndValue = 0.0;

    QColor mAccentColor;

    bool mDraggingLeft = false;
    bool mDraggingRight = false;
};

#endif // COSMICTAPPERSLIDERS_H
