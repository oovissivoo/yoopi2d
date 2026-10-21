#include "cosmicvslider.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QStyleOptionSlider>

#include "cosmicuimetrics.h"

namespace {
// Cosmic palette borrowed from CosmicDropDown
static const QColor kCosmicBackground(0x1f, 0x1f, 0x1f);   // 1f1f1fff
static const QColor kCosmicHandle(0x80, 0x80, 0x80);       // 808080ff

}

CosmicVSlider::CosmicVSlider(QWidget* parent)
    : QSlider(Qt::Vertical, parent)
    , mBackgroundColor(kCosmicBackground)
    , mHandleColor(kCosmicHandle)
    , mHandleHoverColor(kCosmicHandle.lighter(115))
{
    setMouseTracking(true);
    setOrientation(Qt::Vertical);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setMinimumWidth(CosmicUiMetrics::controlThickness());
    setMaximumWidth(CosmicUiMetrics::controlThickness());
    setRange(0, 100);
}

CosmicVSlider::CosmicVSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
    , mBackgroundColor(kCosmicBackground)
    , mHandleColor(kCosmicHandle)
    , mHandleHoverColor(kCosmicHandle.lighter(115))
{
    // Force vertical orientation regardless of argument to keep the API clear
    setOrientation(Qt::Vertical);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setMinimumWidth(CosmicUiMetrics::controlThickness());
    setMaximumWidth(CosmicUiMetrics::controlThickness());
    setRange(0, 100);
}

CosmicVSlider::~CosmicVSlider() = default;

void CosmicVSlider::setBackgroundColor(const QColor& color)
{
    mBackgroundColor = color;
    update();
}

void CosmicVSlider::setHandleColor(const QColor& color)
{
    mHandleColor = color;
    mHandleHoverColor = color.lighter(115);
    update();
}

QSize CosmicVSlider::sizeHint() const
{
    return QSize(CosmicUiMetrics::controlThickness(), 120);
}

QSize CosmicVSlider::minimumSizeHint() const
{
    return QSize(CosmicUiMetrics::controlThickness(), 80);
}

void CosmicVSlider::enterEvent(QEvent* event)
{
    mHover = true;
    QSlider::enterEvent(event);
    update();
}

void CosmicVSlider::leaveEvent(QEvent* event)
{
    mHover = false;
    mDragging = false;
    QSlider::leaveEvent(event);
    update();
}

void CosmicVSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        mDragging = true;
        setValue(valueFromPosition(event->pos()));
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void CosmicVSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (mDragging && (event->buttons() & Qt::LeftButton) && isEnabled()) {
        setValue(valueFromPosition(event->pos()));
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void CosmicVSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && mDragging) {
        mDragging = false;
        setValue(valueFromPosition(event->pos()));
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

int CosmicVSlider::valueFromPosition(const QPoint& pos) const
{
    const QRect trackRect = rect();

    if (trackRect.height() <= 0) {
        return value();
    }

    int y = pos.y();
    if (y < trackRect.top()) y = trackRect.top();
    if (y > trackRect.bottom()) y = trackRect.bottom();

    int vMin = minimum();
    int vMax = maximum();
    if (vMax == vMin) {
        return vMin;
    }

    // Ensure edges map exactly to min/max (top=max, bottom=min).
    if (y <= trackRect.top()) {
        return vMax;
    }
    if (y >= trackRect.bottom()) {
        return vMin;
    }

    // Map y (top=minimum, bottom=maximum) to slider range
    const int denom = qMax(1, trackRect.height() - 1);
    double t = 1.0 - (static_cast<double>(y - trackRect.top()) / denom);
    int v = vMin + static_cast<int>(t * (vMax - vMin));
    if (v < vMin) v = vMin;
    if (v > vMax) v = vMax;
    return v;
}

void CosmicVSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect fullRect = rect();
    const int radius = CosmicUiMetrics::cornerRadius();

    int vMin = minimum();
    int vMax = maximum();
    if (vMax <= vMin) {
        return;
    }

    QColor bg = mBackgroundColor;
    QColor handleColor = mHover ? mHandleHoverColor : mHandleColor;
    if (!isEnabled()) {
        bg.setAlphaF(0.55);
        handleColor.setAlphaF(0.35);
    }

    // Draw track: no border, thinner than handle (75% of handle width), centered
    const int handleW_pre = fullRect.width() - 4;
    const int trackW = qMax(2, handleW_pre * 65 / 100);
    const int trackX = fullRect.left() + (fullRect.width() - trackW) / 2;
    const QRectF trackRectF = QRectF(trackX + 0.5, fullRect.top() + 0.5,
                                     trackW - 1, fullRect.height() - 1);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    const qreal trackRadius = trackW / 2.0;
    p.drawRoundedRect(trackRectF, trackRadius, trackRadius);

    // Draw handle: horizontal rectangle (perpendicular to V direction)
    const double t = (value() - vMin) / static_cast<double>(vMax - vMin);
    const int handleH = qMax(4, fullRect.width() / 2);   // narrow horizontal pill
    const int handleW = fullRect.width() - 4;            // nearly full width
    // top=max, bottom=min: invert t
    const int handleY = fullRect.top() + static_cast<int>((1.0 - t) * (fullRect.height() - handleH));
    const int handleX = fullRect.left() + (fullRect.width() - handleW) / 2;
    const QRectF handleRect = QRectF(handleX + 0.5, handleY + 0.5, handleW - 1, handleH - 1);

    p.setPen(Qt::NoPen);
    p.setBrush(handleColor);
    // Handle radius: proportional — half the shorter dimension
    const int handleRadius = qMax(1, qMin(handleW, handleH) / 2);
    p.drawRoundedRect(handleRect, handleRadius, handleRadius);
}
