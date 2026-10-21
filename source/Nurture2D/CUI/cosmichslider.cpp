#include "cosmichslider.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QLineEdit>
#include <QIntValidator>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleOptionSlider>

#include "cosmicuimetrics.h"

namespace {
// Cosmic palette borrowed from CosmicDropDown / CosmicVSlider
static const QColor kCosmicBackground(0x1f, 0x1f, 0x1f);   // 1f1f1fff
static const QColor kCosmicHandle(0x80, 0x80, 0x80);       // 808080ff
static const QColor kCosmicText(0xb3, 0xb3, 0xb3);         // b3b3b3ff

static int fixedLabelWidthForSlider(const QFont& font, int min, int max, const QString& currentLabel)
{
    Q_UNUSED(min); Q_UNUSED(max); Q_UNUSED(currentLabel);
    // Fixed reserved width — every slider gets the same label column so tracks
    // are always identical width regardless of label content or suffix.
    const QFontMetrics fm(font);
    // Reserve enough for "16.00x" (the widest label we expect) plus padding.
    return fm.horizontalAdvance(QStringLiteral("16.00x")) + 16;
}

static QRect labelRectForSlider(const QRect& fullRect,
                                const QFont& font,
                                int min,
                                int max,
                                CosmicHSlider::LabelMode mode,
                                const QString& label)
{
    if (mode != CosmicHSlider::WithLabel || label.isEmpty()) {
        return QRect();
    }

    const int labelGap = 6;
    const int labelWidth = fixedLabelWidthForSlider(font, min, max, label);
    QRect trackOuterRect = fullRect;
    trackOuterRect.setRight(fullRect.right() - (labelGap + labelWidth));
    return QRect(trackOuterRect.right() + labelGap, fullRect.top(), labelWidth, fullRect.height());
}

static QRect trackRectForSlider(const QRect& fullRect,
                                const QFont& font,
                                int min,
                                int max,
                                CosmicHSlider::LabelMode mode,
                                const QString& label)
{
    const QRect labelRect = labelRectForSlider(fullRect, font, min, max, mode, label);
    if (!labelRect.isValid()) {
        return fullRect;
    }

    QRect trackRect = fullRect;
    trackRect.setRight(labelRect.left() - 6);
    return trackRect;
}
}

CosmicHSlider::CosmicHSlider(QWidget* parent)
    : QSlider(Qt::Horizontal, parent)
    , mBackgroundColor(kCosmicBackground)
    , mHandleColor(kCosmicHandle)
    , mHandleHoverColor(kCosmicHandle.lighter(115))
    , mTextColor(kCosmicText)
{
    setMouseTracking(true);
    setOrientation(Qt::Horizontal);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(CosmicUiMetrics::controlThickness());
    setMaximumHeight(CosmicUiMetrics::controlThickness());
    setRange(0, 100);
}

CosmicHSlider::CosmicHSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
    , mBackgroundColor(kCosmicBackground)
    , mHandleColor(kCosmicHandle)
    , mHandleHoverColor(kCosmicHandle.lighter(115))
    , mTextColor(kCosmicText)
{
    // Force horizontal orientation regardless of argument to keep the API clear
    setOrientation(Qt::Horizontal);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(CosmicUiMetrics::controlThickness());
    setMaximumHeight(CosmicUiMetrics::controlThickness());
    setRange(0, 100);
}

CosmicHSlider::~CosmicHSlider() = default;

void CosmicHSlider::setBackgroundColor(const QColor& color)
{
    mBackgroundColor = color;
    update();
}

void CosmicHSlider::setHandleColor(const QColor& color)
{
    mHandleColor = color;
    mHandleHoverColor = color.lighter(115);
    update();
}

void CosmicHSlider::setTextColor(const QColor& color)
{
    mTextColor = color;
    update();
}

void CosmicHSlider::setLabel(const QString& text)
{
    if (mLabel == text) {
        return;
    }
    mLabel = text;
    update();
}

void CosmicHSlider::setLabelMode(CosmicHSlider::LabelMode mode)
{
    if (mLabelMode == mode) {
        return;
    }
    mLabelMode = mode;
    update();
}

QSize CosmicHSlider::sizeHint() const
{
    // Match cosmic capsules and standard horizontal control heights
    return QSize(140, CosmicUiMetrics::controlThickness());
}

QSize CosmicHSlider::minimumSizeHint() const
{
    return QSize(80, CosmicUiMetrics::controlThickness());
}

void CosmicHSlider::enterEvent(QEvent* event)
{
    mHover = true;
    QSlider::enterEvent(event);
    update();
}

void CosmicHSlider::leaveEvent(QEvent* event)
{
    mHover = false;
    mDragging = false;
    QSlider::leaveEvent(event);
    update();
}

void CosmicHSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        const QRect fullRect = rect();
        const QRect labelRect = labelRectForSlider(fullRect, font(), minimum(), maximum(), mLabelMode, mLabel);
        const QRect trackRect = trackRectForSlider(fullRect, font(), minimum(), maximum(), mLabelMode, mLabel);

        mPressOnTrack = trackRect.contains(event->pos());
        if (mPressOnTrack) {
            mDragging = true;
            setValue(valueFromPosition(event->pos()));
            event->accept();
            return;
        }

        // Clicks on the label (or outside the track) should not affect the value.
        if (labelRect.contains(event->pos())) {
            event->accept();
            return;
        }
    }
    QSlider::mousePressEvent(event);
}

void CosmicHSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (mDragging && mPressOnTrack && (event->buttons() & Qt::LeftButton) && isEnabled()) {
        setValue(valueFromPosition(event->pos()));
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void CosmicHSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && mDragging) {
        mDragging = false;
        mPressOnTrack = false;
        setValue(valueFromPosition(event->pos()));
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

void CosmicHSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        const QRect labelRect = labelRectForSlider(rect(), font(), minimum(), maximum(), mLabelMode, mLabel);
        if (labelRect.isValid() && labelRect.contains(event->pos())) {
            beginInlineEdit();
            event->accept();
            return;
        }
    }
    QSlider::mouseDoubleClickEvent(event);
}

void CosmicHSlider::beginInlineEdit()
{
    if (mLabelMode != WithLabel) {
        return;
    }

    const QRect labelRect = labelRectForSlider(rect(), font(), minimum(), maximum(), mLabelMode, mLabel);
    if (!labelRect.isValid()) {
        return;
    }

    if (!mInlineEditor) {
        mInlineEditor = new QLineEdit(this);
        mInlineEditor->setFrame(false);
        mInlineEditor->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mInlineEditor->setValidator(new QIntValidator(minimum(), maximum(), mInlineEditor));
        mInlineEditor->installEventFilter(this);
        mInlineEditor->setStyleSheet(CosmicInlineEdit::stylesheet());
        mInlineEditor->setTextMargins(0, 0, 0, 0);
        mInlineEditor->hide();

        connect(mInlineEditor, &QLineEdit::editingFinished, this, [this]() {
            if (!mInlineEditor) {
                return;
            }

            if (!mInlineEditor->isVisible()) {
                return;
            }

            if (mInlineEditCancelled) {
                mInlineEditor->hide();
                setFocus(Qt::MouseFocusReason);
                return;
            }

            bool ok = false;
            const int v = mInlineEditor->text().toInt(&ok);
            mInlineEditor->hide();
            setFocus(Qt::MouseFocusReason);

            if (!ok) {
                return;
            }
            setValue(qBound(minimum(), v, maximum()));
        });

        connect(mInlineEditor, &QLineEdit::returnPressed, this, [this]() {
            if (!mInlineEditor || !mInlineEditor->isVisible() || mInlineEditCancelled) {
                return;
            }
            bool ok = false;
            const int v = mInlineEditor->text().toInt(&ok);
            mInlineEditor->hide();
            setFocus(Qt::MouseFocusReason);
            if (!ok) {
                return;
            }
            setValue(qBound(minimum(), v, maximum()));
        });
    }

    mInlineEditOriginalValue = value();
    mInlineEditCancelled = false;
    const QString current = QString::number(value());
    mInlineEditor->setFont(font());
    mInlineEditor->setText(current);
    mInlineEditor->selectAll();
    // Height matches the text line, vertically centred in the label column
    const QFontMetrics fm(font());
    const int editH = fm.height() + 4;
    const QRect editRect(labelRect.left(),
                         labelRect.top() + (labelRect.height() - editH) / 2,
                         labelRect.width(),
                         editH);
    mInlineEditor->setGeometry(editRect);
    mInlineEditor->show();
    mInlineEditor->setFocus(Qt::MouseFocusReason);
}

bool CosmicHSlider::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == mInlineEditor && event && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            mInlineEditCancelled = true;
            if (mInlineEditor) {
                mInlineEditor->hide();
            }
            setFocus(Qt::MouseFocusReason);
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            // Let returnPressed handler commit.
            return false;
        }
    }
    return QSlider::eventFilter(obj, event);
}

int CosmicHSlider::valueFromPosition(const QPoint& pos) const
{
    const QRect fullRect = rect();

    const int labelGap = 6;
    int labelWidth = 0;
    if (mLabelMode == WithLabel && !mLabel.isEmpty()) {
        labelWidth = fixedLabelWidthForSlider(font(), minimum(), maximum(), mLabel);
    }

    QRect trackRect = fullRect;
    if (labelWidth > 0) {
        trackRect.setRight(fullRect.right() - (labelGap + labelWidth));
    }

    if (trackRect.width() <= 0) {
        return value();
    }

    int x = pos.x();
    if (x < trackRect.left()) x = trackRect.left();
    if (x > trackRect.right()) x = trackRect.right();

    int vMin = minimum();
    int vMax = maximum();
    if (vMax == vMin) {
        return vMin;
    }

    // Ensure edges map exactly to min/max.
    if (x <= trackRect.left()) {
        return vMin;
    }
    if (x >= trackRect.right()) {
        return vMax;
    }

    // Map x (left=minimum, right=maximum) to slider range
    const int denom = qMax(1, trackRect.width() - 1);
    double t = static_cast<double>(x - trackRect.left()) / denom;
    int v = vMin + static_cast<int>(t * (vMax - vMin));
    if (v < vMin) v = vMin;
    if (v > vMax) v = vMax;
    return v;
}

QRect CosmicHSlider::leftLabelRect(const QRect& trackRect) const
{
    // Full track width with symmetric horizontal padding.
    // Side selection is handled via text alignment, not by shrinking the rect.
    const int padding = 6;
    return trackRect.adjusted(padding, 0, -padding, 0);
}

QRect CosmicHSlider::rightLabelRect(const QRect& trackRect) const
{
    // Same as left: use the full padded track rect; alignment picks the side.
    const int padding = 6;
    return trackRect.adjusted(padding, 0, -padding, 0);
}

void CosmicHSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect fullRect = rect();
    const int radius = CosmicUiMetrics::cornerRadius();
    const int labelGap = 6;
    const int labelPadding = 8;

    int labelWidth = 0;
    if (mLabelMode == WithLabel && !mLabel.isEmpty()) {
        labelWidth = fixedLabelWidthForSlider(font(), minimum(), maximum(), mLabel);
    }

    QRect trackRect = fullRect;
    QRect labelRect;
    if (labelWidth > 0) {
        trackRect.setRight(fullRect.right() - (labelGap + labelWidth));
        labelRect = QRect(trackRect.right() + labelGap,
                          fullRect.top(),
                          labelWidth,
                          fullRect.height());
    }

    if (trackRect.width() <= 0 || trackRect.height() <= 0) {
        return;
    }

    int vMin = minimum();
    int vMax = maximum();
    if (vMax <= vMin) {
        return;
    }

    QColor bg = mBackgroundColor;
    QColor text = mTextColor;
    QColor handleColor = mHover ? mHandleHoverColor : mHandleColor;
    if (!isEnabled()) {
        bg.setAlphaF(0.55);
        handleColor.setAlphaF(0.35);
        text.setAlphaF(0.45);
    }

    // Draw track: no border, thinner than handle (75% of handle height), centered
    const int handleH_pre = trackRect.height() - 4;
    const int trackH = qMax(2, handleH_pre * 65 / 100);
    const int trackY = trackRect.top() + (trackRect.height() - trackH) / 2;
    const QRectF trackRectF = QRectF(trackRect.left() + 0.5, trackY + 0.5,
                                     trackRect.width() - 1, trackH - 1);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    const qreal trackRadius = trackH / 2.0;
    p.drawRoundedRect(trackRectF, trackRadius, trackRadius);

    // Draw handle: vertical rectangle (perpendicular to H direction)
    const double t = (value() - vMin) / static_cast<double>(vMax - vMin);
    const int handleW = qMax(4, trackRect.height() / 2);  // narrow vertical pill
    const int handleH = trackRect.height() - 4;           // nearly full height
    const int handleX = trackRect.left() + static_cast<int>(t * (trackRect.width() - handleW));
    const int handleY = trackRect.top() + (trackRect.height() - handleH) / 2;
    const QRectF handleRect = QRectF(handleX + 0.5, handleY + 0.5, handleW - 1, handleH - 1);

    p.setPen(Qt::NoPen);
    p.setBrush(handleColor);
    // Handle radius: proportional — half the shorter dimension, giving a fully rounded end
    const int handleRadius = qMax(1, qMin(handleW, handleH) / 2);
    p.drawRoundedRect(handleRect, handleRadius, handleRadius);

    if (labelWidth > 0) {
        // Hide the painted label text while the inline editor is active
        if (!mInlineEditor || !mInlineEditor->isVisible()) {
            p.setPen(text);
            p.drawText(labelRect.adjusted(labelPadding, 0, -labelPadding, 0), Qt::AlignVCenter | Qt::AlignLeft, mLabel);
        }
    }
}
