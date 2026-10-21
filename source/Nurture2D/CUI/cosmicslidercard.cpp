#include "cosmicslidercard.h"
#include "cosmichslider.h"
#include "cosmicuimetrics.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QLabel>

CosmicSliderCard::CosmicSliderCard(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);

    mLabel = new QLabel(label, this);
    mLabel->setStyleSheet(
        "QLabel { color: #cccccc; font-size: 13px; font-weight: 400;"
        " background: transparent; }");

    mSlider = new CosmicHSlider(this);
    mSlider->setSingleStep(1);
    mSlider->setPageStep(1);
    mSlider->setTracking(true);
    mSlider->setLabelMode(CosmicHSlider::WithLabel);

    layout->addWidget(mLabel);
    layout->addWidget(mSlider);

    connect(mSlider, &QSlider::valueChanged, this, &CosmicSliderCard::valueChanged);
}

void CosmicSliderCard::setRange(int min, int max)
{
    mSlider->setRange(min, max);
}

void CosmicSliderCard::setValue(int value)
{
    mSlider->setValue(value);
    mSlider->setLabel(QString::number(value));
}

int CosmicSliderCard::value() const
{
    return mSlider->value();
}

void CosmicSliderCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal r = CosmicUiMetrics::cornerRadius();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1a, 0x1a, 0x1a, 0xff));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
}
