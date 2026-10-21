#include "cosmictappersliders.h"

#include "cosmichslider.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QtMath>

CosmicTapperSliders::CosmicTapperSliders(QWidget* parent)
    : QWidget(parent)
{
    mLeftSlider = new CosmicHSlider(this);
    mRightSlider = new CosmicHSlider(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(0);

    mLeftSlider->setLabelMode(CosmicHSlider::Textless);
    mRightSlider->setLabelMode(CosmicHSlider::Textless);

    updateSliderRanges();
    syncSlidersFromValues();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(mLeftSlider);
    layout->addWidget(mRightSlider);

    mLeftSlider->installEventFilter(this);
    mRightSlider->installEventFilter(this);

    connect(mLeftSlider, &QSlider::valueChanged, this, &CosmicTapperSliders::applyStartFromSlider);
    connect(mRightSlider, &QSlider::valueChanged, this, &CosmicTapperSliders::applyEndFromSlider);
}

QSize CosmicTapperSliders::minimumSizeHint() const
{
    // Two cosmic sliders side by side; height aligned with other cosmic controls.
    return QSize(0, 22);
}

QSize CosmicTapperSliders::sizeHint() const
{
    return QSize(220, 22);
}

void CosmicTapperSliders::setStartValue(qreal value)
{
    value = qBound(0.0, value, 200.0);
    if (qFuzzyCompare(mStartValue, value)) {
        return;
    }
    mStartValue = value;
    syncSlidersFromValues();
}

void CosmicTapperSliders::setEndValue(qreal value)
{
    value = qBound(0.0, value, 200.0);
    if (qFuzzyCompare(mEndValue, value)) {
        return;
    }
    mEndValue = value;
    syncSlidersFromValues();
}

void CosmicTapperSliders::setAccentColor(const QColor& color)
{
    mAccentColor = color;
    // Accent color is kept for potential future use (e.g. outlines or
    // background effects). The core cosmic spec requires slider handles
    // to remain at 0x808080, so we intentionally do NOT recolor the
    // internal CosmicHSlider handles here.
}

void CosmicTapperSliders::updateSliderRanges()
{
    if (mLeftSlider) {
        mLeftSlider->setRange(0, 200);
    }
    if (mRightSlider) {
        mRightSlider->setRange(0, 200);
    }
}

void CosmicTapperSliders::syncSlidersFromValues()
{
    updateSliderRanges();

    if (mLeftSlider) {
        QSignalBlocker b(mLeftSlider);
        int leftVal = qRound(qBound(0.0, mStartValue, 200.0));
        mLeftSlider->setValue(leftVal);
    }

    if (mRightSlider) {
        QSignalBlocker b(mRightSlider);
        int maxVal = mRightSlider->maximum();
        int clamped = qRound(qBound(0.0, mEndValue, 200.0));
        int rightSliderVal = maxVal - clamped;
        mRightSlider->setValue(rightSliderVal);
    }
}

void CosmicTapperSliders::applyStartFromSlider(int sliderValue)
{
    qreal v = qBound(0.0, static_cast<qreal>(sliderValue), 200.0);
    if (qFuzzyCompare(mStartValue, v)) {
        return;
    }
    mStartValue = v;
    emit startTaperChanged(mStartValue);
}

void CosmicTapperSliders::applyEndFromSlider(int sliderValue)
{
    int maxVal = mRightSlider ? mRightSlider->maximum() : 200;
    int inverted = maxVal - sliderValue;
    qreal v = qBound(0.0, static_cast<qreal>(inverted), 200.0);
    if (qFuzzyCompare(mEndValue, v)) {
        return;
    }
    mEndValue = v;
    emit endTaperChanged(mEndValue);
}

bool CosmicTapperSliders::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == mLeftSlider || watched == mRightSlider) &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {

        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton) {
            return QWidget::eventFilter(watched, event);
        }

        if (event->type() == QEvent::MouseButtonPress) {
            bool wasDragging = mDraggingLeft || mDraggingRight;
            if (watched == mLeftSlider) {
                mDraggingLeft = true;
            } else if (watched == mRightSlider) {
                mDraggingRight = true;
            }
            if (!wasDragging && (mDraggingLeft || mDraggingRight)) {
                emit interactionBegun();
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (watched == mLeftSlider) {
                mDraggingLeft = false;
            } else if (watched == mRightSlider) {
                mDraggingRight = false;
            }
            if (!mDraggingLeft && !mDraggingRight) {
                emit interactionEnded();
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}
