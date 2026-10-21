#include "cosmicinputfield.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QStyle>
#include <QStyleOption>
#include <QPixmap>
#include <QtGlobal>
#include <QtMath>
#include <QSettings>

#include "preferencesdef.h"
#include "pencildef.h"
#include "cosmicnumericfieldrenderer.h"
#include "cosmicuimetrics.h"

namespace {
static const QColor kCosmicText(0xb3, 0xb3, 0xb3, 0xff);       // #b3b3b3ff
static const QColor kCosmicAccent(0x50, 0x50, 0x50, 0xff);     // #40555cff
static const QColor kLegacySide(0x1f, 0x1f, 0x1f, 0xff);       // #1f1f1fff
}

CosmicInputField::CosmicInputField(QWidget* parent)
    : QWidget(parent)
    , mValue(0.0)
    , mMinimum(0.0)
    , mMaximum(100.0)
    , mSingleStep(1.0)
    , mDecimals(0)
    , mSuffix(QString())
    , mAccentColor(kCosmicAccent)
    , mTextColor(kCosmicText)
    , mVariant(Minimal)
    , mButtonWidth(24)
    , mCornerRadius(0)
    , mBorderColor(QColor(0x1f, 0x1f, 0x1f, 0xff))
    , mBorderWidth(1)
    , mHoverRegion(None)
    , mPressedRegion(None)
    , mDragging(false)
    , mClickThreshold(5)
    , mInputField(nullptr)
    , mClickTimer(nullptr)
    , mPotentialClick(false)
    , mInlineEditing(false)
{
    // Unified cosmic metrics
    mCornerRadius = CosmicUiMetrics::cornerRadius();
    mBorderWidth = 0;
    mBorderColor = QColor();

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(0);
    setMinimumHeight(CosmicUiMetrics::controlThickness());
    setMaximumHeight(CosmicUiMetrics::controlThickness());
    mInputField = new QLineEdit(this);
    mInputField->hide();
    mInputField->setAlignment(Qt::AlignCenter);
    mInputField->setFrame(false);
    mInputField->setAutoFillBackground(false);
    mInputField->installEventFilter(this);

    connect(mInputField, &QLineEdit::editingFinished,
            this, &CosmicInputField::onInputFinished);

    // Timer for distinguishing click vs drag
    mClickTimer = new QTimer(this);
    mClickTimer->setSingleShot(true);
    mClickTimer->setInterval(150); // ms
    connect(mClickTimer, &QTimer::timeout,
            this, &CosmicInputField::checkForSingleClick);
}

void CosmicInputField::setValue(double value)
{
    double clamped = clampValue(value);
    if (!qFuzzyCompare(1.0 + mValue, 1.0 + clamped)) {
        mValue = clamped;
        emit valueChanged(mValue);
        update();
    }
}

void CosmicInputField::setMinimum(double min)
{
    mMinimum = min;
    if (mValue < mMinimum) {
        setValue(mMinimum);
    }
    update();
}

void CosmicInputField::setButtonWidth(int width)
{
    const int clamped = qBound(6, width, 64);
    if (mButtonWidth == clamped)
        return;
    mButtonWidth = clamped;

    // If currently editing, keep the inline editor aligned with the new center rect.
    if (mInputField && mInputField->isVisible()) {
        QRect centerRect = getCenterFieldRect();
        mInputField->setGeometry(centerRect.adjusted(2, 2, -2, -2));
    }
    updateGeometry();
    update();
}

void CosmicInputField::setMaximum(double max)
{
    mMaximum = max;
    if (mValue > mMaximum) {
        setValue(mMaximum);
    }
    update();
}

void CosmicInputField::setRange(double min, double max)
{
    mMinimum = min;
    mMaximum = max;
    setValue(mValue); // re-clamp
}

void CosmicInputField::setSingleStep(double step)
{
    mSingleStep = step;
}

void CosmicInputField::setDecimals(int decimals)
{
    mDecimals = qMax(0, decimals);
    update();
}

void CosmicInputField::setSuffix(const QString& suffix)
{
    mSuffix = suffix;
    update();
}

void CosmicInputField::setAccentColor(const QColor& color)
{
    mAccentColor = color;
    update();
}

void CosmicInputField::setTextColor(const QColor& color)
{
    mTextColor = color;

    if (mInputField) {
        QPalette pal = mInputField->palette();
        pal.setColor(QPalette::Text, mTextColor);
        mInputField->setPalette(pal);
    }

    update();
}

void CosmicInputField::setVariant(CosmicInputField::Variant variant)
{
    if (mVariant == variant)
        return;
    mVariant = variant;
    update();
}

//---------------- Painting ----------------

void CosmicInputField::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    // When in inline edit mode, draw the full-widget inline-edit background
    // (dark fill + accent border) so the QLineEdit sits seamlessly on top.
    if (mInlineEditing) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor accent = CosmicColor::accent();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setBrush(QColor(0x1f, 0x1f, 0x1f, 0xff));
        painter.setPen(QPen(accent, 1));
        painter.drawRoundedRect(r, mCornerRadius, mCornerRadius);
        return;
    }

    QRect leftRect = getLeftButtonRect();
    QRect centerRect = getCenterFieldRect();
    QRect rightRect = getRightButtonRect();

    CosmicNumericFieldRenderer::PaintParams pp;
    pp.widgetRect = rect();
    pp.leftRect = leftRect;
    pp.centerRect = centerRect;
    pp.rightRect = rightRect;
    pp.accentColor = mAccentColor.isValid() ? mAccentColor : kCosmicAccent;
    pp.borderColor = mBorderColor;
    pp.borderWidth = mBorderWidth;
    pp.cornerRadius = mCornerRadius;
    pp.legacySidePanels = (mVariant == Legacy);
    pp.hoverRegion = static_cast<int>(mHoverRegion);
    pp.pressedRegion = static_cast<int>(mPressedRegion);
    pp.dragging = mDragging;
    pp.antialias = true;
    pp.drawArrows = true;
    pp.arrowSize = QSize(18, 18);
    pp.arrowOpacityNormal = 0.7;
    pp.arrowOpacityHighlighted = 1.0;
    pp.tintArrows = true;
    pp.arrowTint = mBorderColor.isValid() ? mBorderColor : QColor(0x1f, 0x1f, 0x1f, 0xff);

    CosmicNumericFieldRenderer::paintField(painter, pp);

    // Draw value text in center (when not editing inline)
    if (!mInputField->isVisible()) {
        QFont f = font();
        painter.setFont(f);
        painter.setPen(mTextColor);

        QString valueText = formatDisplayValue(mValue);
        QRect textRect(centerRect.left() + 4, centerRect.top(),
                       centerRect.width() - 8, centerRect.height());
        painter.drawText(textRect, Qt::AlignCenter, valueText);
    }
}

//---------------- Hit testing helpers ----------------

CosmicInputField::HitRegion CosmicInputField::hitTest(const QPoint& pos) const
{
    if (getLeftButtonRect().contains(pos)) {
        return LeftButton;
    } else if (getRightButtonRect().contains(pos)) {
        return RightButton;
    } else if (getCenterFieldRect().contains(pos)) {
        return CenterField;
    }
    return None;
}

QRect CosmicInputField::getLeftButtonRect() const
{
    return QRect(0, 0, mButtonWidth, height());
}

QRect CosmicInputField::getCenterFieldRect() const
{
    return QRect(mButtonWidth, 0, width() - 2 * mButtonWidth, height());
}

QRect CosmicInputField::getRightButtonRect() const
{
    return QRect(width() - mButtonWidth, 0, mButtonWidth, height());
}

void CosmicInputField::increment()
{
    // Snap up to the next whole number
    double next = qFloor(mValue) + 1.0;
    // If already on a whole number, step up by 1
    if (qFuzzyCompare(mValue, qFloor(mValue)))
        next = mValue + 1.0;
    setValue(next);
}

void CosmicInputField::decrement()
{
    // Snap down to the previous whole number
    double prev = qCeil(mValue) - 1.0;
    // If already on a whole number, step down by 1
    if (qFuzzyCompare(mValue, qCeil(mValue)))
        prev = mValue - 1.0;
    setValue(prev);
}

//---------------- Inline edit helpers ----------------

void CosmicInputField::showInputField()
{
    mInlineEditing = true;
    // Fill the entire widget so the styled QLineEdit covers it completely
    mInputField->setGeometry(rect());
    mInputField->setStyleSheet(CosmicInlineEdit::stylesheet());
    mInputField->setText(formatNumericValue(mValue));
    mInputField->selectAll();
    mInputField->show();
    mInputField->setFocus();
    update();
}

void CosmicInputField::hideInputField()
{
    mInlineEditing = false;
    mInputField->hide();
    mInputField->clearFocus();
    mInputField->setStyleSheet(QString());
    setFocus(); // return focus to the widget itself
    update();
}

QString CosmicInputField::formatDisplayValue(double val) const
{
    QString numeric = formatNumericValue(val);
    if (!mSuffix.isEmpty())
        return numeric + mSuffix;
    return numeric;
}

QString CosmicInputField::formatNumericValue(double val) const
{
    return QString::number(val, 'f', mDecimals);
}

double CosmicInputField::clampValue(double val) const
{
    return qBound(mMinimum, val, mMaximum);
}

//---------------- Events ----------------

void CosmicInputField::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mPressedRegion = hitTest(event->pos());

        if (mPressedRegion == LeftButton) {
            decrement();
            emit valueCommitted(mValue);
        } else if (mPressedRegion == RightButton) {
            increment();
            emit valueCommitted(mValue);
        } else if (mPressedRegion == CenterField) {
            // Start tracking for click vs drag
            mDragStartPos = event->pos();
            mDragStartValue = mValue;
            mDragging = false;
            mPotentialClick = true;
            mClickTimer->start();
        }

        update();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void CosmicInputField::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (mPotentialClick && !mDragging && mPressedRegion == CenterField) {
            // This was a click, not a drag
            mClickTimer->stop();
            showInputField();
        }

        // Commit the final value after drag-based editing.
        if (mDragging && mPressedRegion == CenterField) {
            emit valueCommitted(mValue);
        }

        mPressedRegion = None;
        mDragging = false;
        mPotentialClick = false;
        mClickTimer->stop();
        update();
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void CosmicInputField::mouseMoveEvent(QMouseEvent* event)
{
    // Update hover region
    HitRegion newHover = hitTest(event->pos());
    if (newHover != mHoverRegion) {
        mHoverRegion = newHover;
        update();
    }

    // Handle drag start threshold
    if (mPotentialClick && !mDragging) {
        int dragDistance = (event->pos() - mDragStartPos).manhattanLength();
        if (dragDistance > mClickThreshold) {
            mDragging = true;
            mPotentialClick = false;
            mClickTimer->stop();
        }
    }

    // Dragging to adjust value
    if (mDragging) {
        int dx = event->pos().x() - mDragStartPos.x();

        // Fixed pixels-per-unit so sensitivity is independent of widget width.
        // 8px of drag = 1 unit change. Feels deliberate for both mouse and pen tablet.
        static const double kPixelsPerUnit = 8.0;
        double rawChange = dx / kPixelsPerUnit;
        double rawValue = mDragStartValue + rawChange;

        double snapped;
        if (event->modifiers() & Qt::ShiftModifier) {
            // Snap to nearest 5
            snapped = qRound(rawValue / 5.0) * 5.0;
        } else if (event->modifiers() & Qt::ControlModifier) {
            // Snap to nearest 0.5
            snapped = qRound(rawValue * 2.0) / 2.0;
        } else {
            // Snap to nearest whole number
            snapped = qRound(rawValue);
        }

        setValue(snapped);
    }

    QWidget::mouseMoveEvent(event);
}

void CosmicInputField::leaveEvent(QEvent* event)
{
    mHoverRegion = None;
    update();
    QWidget::leaveEvent(event);
}

void CosmicInputField::wheelEvent(QWheelEvent* event)
{
    QPoint angleDelta = event->angleDelta();
    if (!angleDelta.isNull()) {
        double steps = angleDelta.y() / 120.0;
        double rawValue = mValue + steps;

        double snapped;
        if (event->modifiers() & Qt::ShiftModifier) {
            snapped = qRound(rawValue / 5.0) * 5.0;
        } else if (event->modifiers() & Qt::ControlModifier) {
            snapped = qRound(rawValue * 2.0) / 2.0;
        } else {
            snapped = qRound(rawValue);
        }

        setValue(snapped);
        event->accept();
    }

    QWidget::wheelEvent(event);
}

bool CosmicInputField::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == mInputField && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            onInputFinished();
            return true; // consume event
        }
    }
    return QWidget::eventFilter(obj, event);
}

QSize CosmicInputField::sizeHint() const
{
    QFontMetrics fm(font());
    const int t = CosmicUiMetrics::controlThickness();
    QString sample = QString("0000") + mSuffix;
    int textWidth = fm.horizontalAdvance(sample);
    int totalWidth = textWidth + 2 * mButtonWidth + 20;
    return QSize(totalWidth, t);
}

QSize CosmicInputField::minimumSizeHint() const
{
    return QSize(0, CosmicUiMetrics::controlThickness());
}

//---------------- Slots ----------------

void CosmicInputField::onInputFinished()
{
    QString text = mInputField->text().trimmed();

    // Allow user to type with or without suffix
    if (!mSuffix.isEmpty() && text.endsWith(mSuffix)) {
        text.chop(mSuffix.size());
        text = text.trimmed();
    }

    bool ok = false;
    double newValue = text.toDouble(&ok);
    if (ok) {
        // Snap to nearest 0.5
        newValue = qRound(newValue * 2.0) / 2.0;
        setValue(newValue);
    }

    emit valueCommitted(mValue);

    hideInputField();
}

void CosmicInputField::checkForSingleClick()
{
    // Timer expired without drag starting - actual click handling
    // happens in mouseReleaseEvent when mPotentialClick is still true.
}

