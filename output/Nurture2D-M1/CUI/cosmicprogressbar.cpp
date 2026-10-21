#include "cosmicprogressbar.h"
#include "cosmicuimetrics.h"

#include <QPainter>
#include <QPainterPath>

CosmicProgressBar::CosmicProgressBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(4);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CosmicProgressBar::setRange(int min, int max)
{
    mMin = min;
    mMax = max;
    update();
}

void CosmicProgressBar::setValue(int v)
{
    mValue = v;
    update();
}

void CosmicProgressBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0, 0, width(), height());
    const qreal r = height() / 2.0;

    // Track
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2a, 0x2a, 0x2a));
    p.drawRoundedRect(track, r, r);

    // Fill
    const int range = (mMax > mMin) ? (mMax - mMin) : 1;
    const qreal t = qBound(0.0, static_cast<qreal>(mValue - mMin) / range, 1.0);
    if (t > 0.0) {
        const QRectF fill(0, 0, track.width() * t, height());
        p.setBrush(CosmicColor::accent());
        p.drawRoundedRect(fill, r, r);
    }
}
