#include "cosmicintensitybox.h"

#include <QPainter>
#include <QMouseEvent>
#include <QApplication>

#include "cosmicuimetrics.h"

namespace {
static const int kSegmentCount = 4;
static const QColor kBorderColor(0x1f, 0x1f, 0x1f, 0xff);           // 1f1f1fff
static const QColor kBgHighlighted(0x50, 0x50, 0x50, 0xff);        // 505050ff
static const QColor kBgUnhighlighted = kBorderColor;               // 1f1f1fff
static const QColor kIconActiveColor(0x1f, 0x1f, 0x1f, 0xff);      // 1f1f1fff
static const QColor kIconInactiveColor(0x80, 0x80, 0x80, 0xff);    // 808080ff
}

CosmicIntensityBox::CosmicIntensityBox(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(CosmicUiMetrics::controlThickness());
    // Enable tracking so we get smooth sweep updates while the mouse is held
    // down and moved across the segments.
    setMouseTracking(true);

    mIcons.reserve(kSegmentCount);
    mIcons << QIcon(":/Interaction/IntensityOff.svg")
           << QIcon(":/Interaction/IntensityLow.svg")
           << QIcon(":/Interaction/IntensityMedium.svg")
           << QIcon(":/Interaction/IntensityHigh.svg");
}

void CosmicIntensityBox::setValue(int v)
{
    int clamped = qBound(0, v, kSegmentCount - 1);
    if (clamped == mValue)
        return;
    mValue = clamped;
    update();
    emit valueChanged(mValue);
}

void CosmicIntensityBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = rect();
    if (r.width() <= 0 || r.height() <= 0)
        return;

    int baseW = r.width() / kSegmentCount;
    int remainder = r.width() % kSegmentCount;

    int x = r.x();
    for (int i = 0; i < kSegmentCount; ++i) {
        int w = baseW + (i == kSegmentCount - 1 ? remainder : 0);
        QRect boxRect(x, r.y(), w, r.height());

        // Fill background based on intensity value
        const bool highlighted = (i <= mValue);
        QColor fill = highlighted ? kBgHighlighted : kBgUnhighlighted;
        if (mFillTintEnabled && mFillTintColor.isValid()) {
            // Tint the inner fill while leaving borders untouched.
            // Stronger tint for the highlighted segments.
            const qreal a = highlighted ? 0.75 : 0.55;
            fill.setRedF(fill.redF() * (1.0 - a) + mFillTintColor.redF() * a);
            fill.setGreenF(fill.greenF() * (1.0 - a) + mFillTintColor.greenF() * a);
            fill.setBlueF(fill.blueF() * (1.0 - a) + mFillTintColor.blueF() * a);
            fill.setAlpha(255);
        }
        p.fillRect(boxRect, fill);

        // Draw border
        p.setPen(kBorderColor);
        p.drawRect(boxRect.adjusted(0, 0, -1, -1));

        // Draw icon centered in box (larger, with minimal margins), tinted per state
        if (i < mIcons.size() && !mIcons[i].isNull()) {
            int margin = 1;
            int iconSide = qMin(boxRect.width() - 2 * margin, boxRect.height() - 2 * margin);
            if (iconSide > 0) {
                QRect iconRect(
                    boxRect.x() + (boxRect.width() - iconSide) / 2,
                    boxRect.y() + (boxRect.height() - iconSide) / 2,
                    iconSide,
                    iconSide);

                // Obtain the base pixmap and tint it for better contrast
                QPixmap pixmap = mIcons[i].pixmap(iconSide, iconSide);
                if (!pixmap.isNull()) {
                    QPainter iconPainter(&pixmap);
                    iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    const QColor iconColor = highlighted ? kIconActiveColor : kIconInactiveColor;
                    iconPainter.fillRect(pixmap.rect(), iconColor);
                    iconPainter.end();

                    p.drawPixmap(iconRect, pixmap);
                }
            }
        }

        x += w;
    }

    // Optional strike-through overlay used to indicate that the control
    // is logically disabled/ignored while still showing its last value.
    if (mStrikeThrough) {
        // Darken the entire box to visually "mute" it.
        QColor overlay(0, 0, 0, 160); // semi-opaque black tint
        p.fillRect(r.adjusted(1, 1, -1, -1), overlay);

        // Draw a bold red diagonal line across the control.
        QPen pen(Qt::red);
        pen.setWidth(2);
        p.setPen(pen);
        QPoint start = r.topLeft() + QPoint(2, 2);
        QPoint end   = r.bottomRight() - QPoint(2, 2);
        p.drawLine(start, end);
    }
}

void CosmicIntensityBox::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mDragging = true;
        grabMouse();
        setFocus(Qt::MouseFocusReason);
        int idx = indexFromPosition(event->pos());
        setValue(idx);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void CosmicIntensityBox::mouseMoveEvent(QMouseEvent* event)
{
    // While we are in a drag that began with a left-button press on this
    // widget, continuously update the value as the cursor sweeps across
    // the segments. After mouseReleaseEvent, mDragging is cleared so
    // further moves no longer affect the value.
    if (mDragging) {
        int idx = indexFromPosition(event->pos());
        setValue(idx);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void CosmicIntensityBox::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && mDragging) {
        mDragging = false;
        releaseMouse();
        int idx = indexFromPosition(event->pos());
        setValue(idx);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QSize CosmicIntensityBox::sizeHint() const
{
    const int t = CosmicUiMetrics::controlThickness();
    return QSize(4 * t, t);
}

QSize CosmicIntensityBox::minimumSizeHint() const
{
    const int t = CosmicUiMetrics::controlThickness();
    return QSize(4 * (t - 4), t - 4);
}

int CosmicIntensityBox::indexFromPosition(const QPoint& pos) const
{
    int w = width();
    if (w <= 0)
        return 0;

    int segmentWidth = w / kSegmentCount;
    if (segmentWidth <= 0)
        return 0;

    int idx = pos.x() / segmentWidth;
    if (idx < 0) idx = 0;
    if (idx >= kSegmentCount) idx = kSegmentCount - 1;
    return idx;
}
