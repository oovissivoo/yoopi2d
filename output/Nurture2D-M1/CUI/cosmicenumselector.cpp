#include "cosmicenumselector.h"
#include "cosmicuimetrics.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>

static constexpr int   kAnimDurationMs = 180;
static constexpr qreal kPad            = 2.0; // inset from outer border

CosmicEnumSelector::CosmicEnumSelector(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_Hover, true);

    mSlideAnim = new QVariantAnimation(this);
    mSlideAnim->setDuration(kAnimDurationMs);
    mSlideAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(mSlideAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        mHighlightRect.setX(v.toRectF().x());
        mHighlightRect.setWidth(v.toRectF().width());
        update();
    });
}

// ── Public API ────────────────────────────────────────────────────────────────

void CosmicEnumSelector::setOptions(const QStringList& options)
{
    mOptions      = options;
    mItemEnabled  = QVector<bool>(options.size(), true);
    mCurrentIndex = qBound(0, mCurrentIndex, qMax(0, options.size() - 1));
    mHoverIndex   = -1;
    mPressIndex   = -1;
    updateGeometry();
    snapHighlightToIndex(mCurrentIndex);
    update();
}

void CosmicEnumSelector::setItemEnabled(int index, bool enabled)
{
    if (index < 0 || index >= mItemEnabled.size()) return;
    if (mItemEnabled[index] == enabled) return;
    mItemEnabled[index] = enabled;
    update();
}

bool CosmicEnumSelector::isItemEnabled(int index) const
{
    if (index < 0 || index >= mItemEnabled.size()) return false;
    return mItemEnabled[index];
}

void CosmicEnumSelector::setCurrentIndex(int index, bool animate)
{
    if (index < 0 || index >= mOptions.size()) return;
    if (index == mCurrentIndex) return;

    mCurrentIndex = index;
    emit currentIndexChanged(mCurrentIndex);
    emit currentOptionChanged(currentOption());

    if (animate)
        animateTo(index);
    else
        snapHighlightToIndex(index);

    update();
}

void CosmicEnumSelector::setCurrentOption(const QString& option)
{
    const int idx = mOptions.indexOf(option);
    if (idx >= 0)
        setCurrentIndex(idx, true);
}

void CosmicEnumSelector::setShowBorder(bool show)
{
    if (mShowBorder == show) return;
    mShowBorder = show;
    update();
}

void CosmicEnumSelector::setShowBackground(bool show)
{
    if (mShowBackground == show) return;
    mShowBackground = show;
    update();
}

QString CosmicEnumSelector::currentOption() const{
    if (mCurrentIndex >= 0 && mCurrentIndex < mOptions.size())
        return mOptions[mCurrentIndex];
    return {};
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

QRectF CosmicEnumSelector::itemRect(int index) const
{
    if (mOptions.isEmpty()) return {};
    const int    n      = mOptions.size();
    const qreal  innerH = height() - kPad * 2;
    const qreal  totalW = width()  - kPad * 2;
    const qreal  itemW  = totalW / n;
    return QRectF(kPad + index * itemW, kPad, itemW, innerH);
}

int CosmicEnumSelector::indexAtPos(const QPoint& pos) const
{
    for (int i = 0; i < mOptions.size(); ++i) {
        if (itemRect(i).contains(pos))
            return i;
    }
    return -1;
}

void CosmicEnumSelector::snapHighlightToIndex(int index)
{
    if (mSlideAnim->state() == QAbstractAnimation::Running)
        mSlideAnim->stop();
    mHighlightRect = itemRect(index);
}

void CosmicEnumSelector::animateTo(int newIndex)
{
    const QRectF target = itemRect(newIndex);
    if (mSlideAnim->state() == QAbstractAnimation::Running)
        mSlideAnim->stop();

    // Animate from current highlight position to target
    mSlideAnim->setStartValue(mHighlightRect);
    mSlideAnim->setEndValue(target);
    mSlideAnim->start();
}

// ── Events ────────────────────────────────────────────────────────────────────

void CosmicEnumSelector::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Re-snap so the highlight rect stays in the right place after resize
    if (mSlideAnim->state() != QAbstractAnimation::Running)
        snapHighlightToIndex(mCurrentIndex);
}

void CosmicEnumSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int idx = indexAtPos(event->pos());
        if (idx >= 0 && isItemEnabled(idx))
            mPressIndex = idx;
        update();
    }
    event->accept();
}

void CosmicEnumSelector::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int idx = indexAtPos(event->pos());
        if (idx >= 0 && idx == mPressIndex && isItemEnabled(idx)) {
            if (idx != mCurrentIndex) {
                mCurrentIndex = idx;
                emit currentIndexChanged(mCurrentIndex);
                emit currentOptionChanged(currentOption());
                animateTo(idx);
            }
        }
        mPressIndex = -1;
        update();
    }
    event->accept();
}

void CosmicEnumSelector::mouseMoveEvent(QMouseEvent* event)
{
    const int idx = indexAtPos(event->pos());
    if (idx != mHoverIndex) {
        mHoverIndex = idx;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CosmicEnumSelector::leaveEvent(QEvent* event)
{
    mHoverIndex = -1;
    mPressIndex = -1;
    update();
    QWidget::leaveEvent(event);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void CosmicEnumSelector::paintEvent(QPaintEvent*)
{
    if (mOptions.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const qreal  r          = CosmicUiMetrics::cornerRadius();
    const QColor accent     = CosmicColor::accent();
    const QRectF outerRect  = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // Outer container background
    p.setPen(Qt::NoPen);
    if (mShowBackground) {
        p.setBrush(QColor(0x1a, 0x1a, 0x1a, 0xff));
        p.drawRoundedRect(outerRect, r, r);
    }

    // Outer border (thin, accent color)
    if (mShowBorder) {
        p.setPen(QPen(accent, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(outerRect, r, r);
    }

    // Sliding accent highlight (drawn before text so text renders on top)
    if (!mHighlightRect.isEmpty() && isItemEnabled(mCurrentIndex)) {
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(mHighlightRect, r, r);
    }

    // Text for each item
    p.setFont(font());
    for (int i = 0; i < mOptions.size(); ++i) {
        const QRectF ir       = itemRect(i);
        const bool   selected = (i == mCurrentIndex);
        const bool   disabled = !isItemEnabled(i);
        const bool   hovered  = (i == mHoverIndex) && !selected && !disabled;
        const bool   pressed  = (i == mPressIndex)  && !selected && !disabled;

        if (disabled) {
            p.setPen(QColor(0x55, 0x55, 0x55, 0xff));
        } else if (selected) {
            p.setPen(QColor(0xff, 0xff, 0xff, 0xff));
        } else if (pressed) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x30, 0x30, 0x30, 0xff));
            p.drawRoundedRect(ir, r, r);
            p.setPen(QColor(0xff, 0xff, 0xff, 0xdd));
        } else if (hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x25, 0x25, 0x25, 0xff));
            p.drawRoundedRect(ir, r, r);
            p.setPen(QColor(0xff, 0xff, 0xff, 0xcc));
        } else {
            p.setPen(QColor(0xaa, 0xaa, 0xaa, 0xff));
        }

        p.drawText(ir, Qt::AlignCenter, mOptions[i]);
    }
}

QSize CosmicEnumSelector::sizeHint() const
{
    // Enum selector is slightly taller than a standard control to give the
    // sliding pill visual breathing room. Use thickness + a proportional inset.
    const int t = CosmicUiMetrics::controlThickness();
    const int h = t + qMax(4, t / 4);
    const int minW = mOptions.size() * 80;
    return QSize(minW, h);
}

QSize CosmicEnumSelector::minimumSizeHint() const
{
    const int t = CosmicUiMetrics::controlThickness();
    return QSize(mOptions.size() * 60, t + 4);
}
