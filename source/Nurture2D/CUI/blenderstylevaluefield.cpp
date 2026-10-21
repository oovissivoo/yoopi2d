#include "blenderstylevaluefield.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QStyle>
#include <QStyleOption>
#include <QPixmap>
#include <QSettings>
#include <qmath.h>

#include "preferencesdef.h"
#include "pencildef.h"
#include "cosmicnumericfieldrenderer.h"

BlenderStyleValueField::BlenderStyleValueField(QWidget *parent)
    : QWidget(parent)
    , mValue(0.0)
    , mMinimum(0.0)
    , mMaximum(100.0)
    , mSingleStep(1.0)
    , mDecimals(3)
    , mLabel("Value")
    , mBackgroundColor(QColor(56, 56, 56))  // Darker gray to match Blender
    , mTextColor(QColor(178, 178, 178))
    , mCornerRadius(0)
    , mButtonWidth(24)
    , mCompactMode(false)
    , mTextlessMode(false)
    , mLegacySidePanels(false)
    , mBorderColor(QColor(0x1f, 0x1f, 0x1f, 0xff))
    , mBorderWidth(1)
    , mHoverRegion(None)
    , mPressedRegion(None)
    , mDragging(false)
    , mClickThreshold(5)
    , mInputField(nullptr)
    , mClickTimer(nullptr)
    , mPotentialClick(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // Make it expand horizontally
    mAccentColor = getAccentColor();

    {
        QSettings settings(PENCIL2D, PENCIL2D);
        const int roundness = settings.value(SETTING_COSMIC_ROUNDNESS, 6).toInt();
        const int borderWidth = settings.value(SETTING_COSMIC_BORDER_WIDTH, 1).toInt();
        const int borderRgba = settings.value(SETTING_COSMIC_BORDER_COLOR, 0xFF1F1F1F).toInt();

        mCornerRadius = qMax(0, roundness);
        mBorderWidth = qMax(0, borderWidth);
        mBorderColor = QColor::fromRgba(static_cast<QRgb>(borderRgba));
    }
    
    // Create input field (hidden by default)
    mInputField = new QLineEdit(this);
    mInputField->hide();
    mInputField->setAlignment(Qt::AlignCenter);
    mInputField->installEventFilter(this); // Install event filter to handle key events
    connect(mInputField, &QLineEdit::editingFinished, this, &BlenderStyleValueField::onInputFinished);
    
    // Create timer for click detection
    mClickTimer = new QTimer(this);
    mClickTimer->setSingleShot(true);
    mClickTimer->setInterval(150); // 150ms threshold for click detection
    connect(mClickTimer, &QTimer::timeout, this, &BlenderStyleValueField::checkForSingleClick);
}

QColor BlenderStyleValueField::getAccentColor()
{
    return BlenderStyleSlider::getAccentColor();
}

void BlenderStyleValueField::setValue(double value)
{
    double clamped = clampValue(value);
    if (qAbs(mValue - clamped) > 1e-9) {
        mValue = clamped;
        emit valueChanged(mValue);
        update();
    }
}

void BlenderStyleValueField::setMinimum(double min)
{
    mMinimum = min;
    if (mValue < mMinimum) {
        setValue(mMinimum);
    }
    update();
}

void BlenderStyleValueField::setMaximum(double max)
{
    mMaximum = max;
    if (mValue > mMaximum) {
        setValue(mMaximum);
    }
    update();
}

void BlenderStyleValueField::setRange(double min, double max)
{
    mMinimum = min;
    mMaximum = max;
    setValue(mValue); // Re-clamp
}

void BlenderStyleValueField::setSingleStep(double step)
{
    mSingleStep = step;
}

void BlenderStyleValueField::setDecimals(int decimals)
{
    mDecimals = qMax(0, decimals);
    update();
}

void BlenderStyleValueField::setLabel(const QString &label)
{
    mLabel = label;
    update();
}

void BlenderStyleValueField::setAccentColor(const QColor &color)
{
    mAccentColor = color;
    update();
}

void BlenderStyleValueField::setBackgroundColor(const QColor &color)
{
    mBackgroundColor = color;
    update();
}

void BlenderStyleValueField::setTextColor(const QColor &color)
{
    mTextColor = color;
    update();
}

void BlenderStyleValueField::setCornerRadius(int radius)
{
    mCornerRadius = radius;
    update();
}

void BlenderStyleValueField::setLegacySidePanels(bool enabled)
{
    if (mLegacySidePanels == enabled)
        return;
    mLegacySidePanels = enabled;
    update();
}

void BlenderStyleValueField::setBorderColor(const QColor& color)
{
    mBorderColor = color;
    update();
}

void BlenderStyleValueField::setBorderWidth(int width)
{
    mBorderWidth = qMax(0, width);
    update();
}

void BlenderStyleValueField::setCompactMode(bool compact)
{
    mCompactMode = compact;
    if (mCompactMode) {
        mButtonWidth = 20;  // Smaller buttons in compact mode
    } else {
        mButtonWidth = 24;  // Normal buttons
    }
    updateGeometry();
    update();
}

void BlenderStyleValueField::setTextlessMode(bool enabled)
{
    if (mTextlessMode == enabled)
        return;
    mTextlessMode = enabled;
    updateGeometry();
    update();
}

BlenderStyleValueField::HitRegion BlenderStyleValueField::hitTest(const QPoint &pos) const
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

QRect BlenderStyleValueField::getLeftButtonRect() const
{
    return QRect(0, 0, mButtonWidth, height());
}

QRect BlenderStyleValueField::getCenterFieldRect() const
{
    return QRect(mButtonWidth, 0, width() - 2 * mButtonWidth, height());
}

QRect BlenderStyleValueField::getRightButtonRect() const
{
    return QRect(width() - mButtonWidth, 0, mButtonWidth, height());
}

void BlenderStyleValueField::increment()
{
    setValue(mValue + mSingleStep);
}

void BlenderStyleValueField::decrement()
{
    setValue(mValue - mSingleStep);
}

void BlenderStyleValueField::showInputField()
{
    QRect centerRect = getCenterFieldRect();
    mInputField->setGeometry(centerRect.adjusted(2, 2, -2, -2));
    mInputField->setText(formatValue(mValue));
    mInputField->selectAll();
    mInputField->show();
    mInputField->setFocus();
}

void BlenderStyleValueField::hideInputField()
{
    mInputField->hide();
    mInputField->clearFocus();
    setFocus(); // Return focus to the value field widget, not to other widgets
}

bool BlenderStyleValueField::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == mInputField && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Manually finish editing and consume the event
            onInputFinished();
            return true; // Event handled, don't propagate
        }
    }
    return QWidget::eventFilter(obj, event);
}

QString BlenderStyleValueField::formatValue(double val) const
{
    return QString::number(val, 'f', mDecimals);
}

double BlenderStyleValueField::clampValue(double val) const
{
    return qBound(mMinimum, val, mMaximum);
}

void BlenderStyleValueField::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);

    QRect leftRect = getLeftButtonRect();
    QRect centerRect = getCenterFieldRect();
    QRect rightRect = getRightButtonRect();

    CosmicNumericFieldRenderer::PaintParams pp;
    pp.widgetRect = rect();
    pp.leftRect = leftRect;
    pp.centerRect = centerRect;
    pp.rightRect = rightRect;
    pp.accentColor = mAccentColor.isValid() ? mAccentColor : getAccentColor();
    pp.borderColor = mBorderColor;
    pp.borderWidth = mBorderWidth;
    pp.cornerRadius = mCornerRadius;
    pp.legacySidePanels = mLegacySidePanels;
    pp.hoverRegion = static_cast<int>(mHoverRegion);
    pp.pressedRegion = static_cast<int>(mPressedRegion);
    pp.dragging = mDragging;
    pp.antialias = true;
    pp.drawArrows = true;
    pp.arrowSize = mCompactMode ? QSize(14, 14) : QSize(18, 18);
    pp.arrowOpacityNormal = 0.7;
    pp.arrowOpacityHighlighted = 1.0;
    pp.tintArrows = false;
    pp.arrowTint = QColor();

    CosmicNumericFieldRenderer::paintField(painter, pp);

    // Draw label/value in center
    if (!mInputField->isVisible()) {
        QFont labelFont = font();
        labelFont.setPointSize(labelFont.pointSize() - 1);
        painter.setFont(labelFont);
        painter.setPen(mTextColor);

        if (mTextlessMode) {
            QString valueText = formatValue(mValue);
            QRect textRect(centerRect.left() + 4, centerRect.top(),
                           centerRect.width() - 8, centerRect.height());
            painter.drawText(textRect, Qt::AlignCenter, valueText);
        } else if (mCompactMode) {
            QString valueText = formatValue(mValue);
            QString combinedText = mLabel + ": " + valueText;
            QRect textRect(centerRect.left() + 4, centerRect.top(),
                           centerRect.width() - 8, centerRect.height());
            painter.drawText(textRect, Qt::AlignCenter, combinedText);
        } else {
            int centerX = centerRect.center().x();
            QRect labelRect(centerRect.left() + 8, centerRect.top(),
                            centerX - centerRect.left() - 12, centerRect.height());
            QRect valueRect(centerX + 4, centerRect.top(),
                            centerRect.right() - centerX - 12, centerRect.height());

            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, mLabel + ":");
            QString valueText = formatValue(mValue);
            painter.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter, valueText);
        }
    }
}

void BlenderStyleValueField::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mPressedRegion = hitTest(event->pos());
        
        if (mPressedRegion == LeftButton) {
            decrement();
        } else if (mPressedRegion == RightButton) {
            increment();
        } else if (mPressedRegion == CenterField) {
            // Start tracking for click vs drag
            mDragStartPos = event->pos();
            mDragStartValue = mValue;
            mDragging = false;
            mPotentialClick = true;
            mClickTimer->start();
        }
        
        update();
        event->accept(); // Accept event to prevent propagation to parent widgets
    } else {
        QWidget::mousePressEvent(event);
    }
}

void BlenderStyleValueField::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mPotentialClick && !mDragging && mPressedRegion == CenterField) {
            // This was a click, not a drag
            mClickTimer->stop();
            showInputField();
        }
        
        mPressedRegion = None;
        mDragging = false;
        mPotentialClick = false;
        mClickTimer->stop();
        update();
        event->accept(); // Accept event to prevent propagation to parent widgets
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void BlenderStyleValueField::mouseMoveEvent(QMouseEvent *event)
{
    // Update hover region
    HitRegion newHover = hitTest(event->pos());
    if (newHover != mHoverRegion) {
        mHoverRegion = newHover;
        update();
    }
    
    // Handle dragging
    if (mPotentialClick && !mDragging) {
        int dragDistance = (event->pos() - mDragStartPos).manhattanLength();
        if (dragDistance > mClickThreshold) {
            // We've moved enough to be considered a drag
            mDragging = true;
            mPotentialClick = false;
            mClickTimer->stop();
        }
    }
    
    if (mDragging) {
        // Calculate value change based on horizontal movement
        int dx = event->pos().x() - mDragStartPos.x();
        double range = mMaximum - mMinimum;
        double valueChange = (dx / (double)getCenterFieldRect().width()) * range;
        
        // Apply sensitivity modifier based on keyboard and value type
        // For integers: default is fine (0.1x), Shift is normal (1.0x)
        // For floats: default is normal (1.0x), Shift is fine (0.1x)
        bool isInteger = (mDecimals == 0);
        
        if (event->modifiers() & Qt::ControlModifier) {
            // Ctrl always means coarse control (10x) for both types
            valueChange *= 10.0;
        } else if (event->modifiers() & Qt::ShiftModifier) {
            // Shift behavior depends on value type
            if (isInteger) {
                // For integers: Shift gives normal speed (1.0x)
                valueChange *= 1.0;
            } else {
                // For floats: Shift gives fine control (0.1x)
                valueChange *= 0.1;
            }
        } else {
            // Default behavior depends on value type
            if (isInteger) {
                // For integers: default is fine control (0.1x)
                valueChange *= 0.1;
            }
            // For floats: default is normal (1.0x) - no multiplication needed
        }
        
        setValue(mDragStartValue + valueChange);
    }
    
    QWidget::mouseMoveEvent(event);
}

void BlenderStyleValueField::leaveEvent(QEvent *event)
{
    mHoverRegion = None;
    update();
    QWidget::leaveEvent(event);
}

void BlenderStyleValueField::wheelEvent(QWheelEvent *event)
{
    QPoint angleDelta = event->angleDelta();
    if (!angleDelta.isNull()) {
        double steps = angleDelta.y() / 120.0; // Standard wheel step
        double change = steps * mSingleStep;
        
        // Apply sensitivity modifier based on keyboard and value type
        // For integers: default is fine (0.1x), Shift is normal (1.0x)
        // For floats: default is normal (1.0x), Shift is fine (0.1x)
        bool isInteger = (mDecimals == 0);
        
        if (event->modifiers() & Qt::ControlModifier) {
            // Ctrl always means coarse control (10x) for both types
            change *= 10.0;
        } else if (event->modifiers() & Qt::ShiftModifier) {
            // Shift behavior depends on value type
            if (isInteger) {
                // For integers: Shift gives normal speed (1.0x)
                change *= 1.0;
            } else {
                // For floats: Shift gives fine control (0.1x)
                change *= 0.1;
            }
        } else {
            // Default behavior depends on value type
            if (isInteger) {
                // For integers: default is fine control (0.1x)
                change *= 0.1;
            }
            // For floats: default is normal (1.0x) - no multiplication needed
        }
        
        setValue(mValue + change);
        event->accept();
    }
    QWidget::wheelEvent(event);
}

QSize BlenderStyleValueField::sizeHint() const
{
    QFontMetrics fm(font());
    int textHeight = fm.height();
    
    if (mCompactMode) {
        // Compact mode: calculate based on text width
        QString sampleText = mLabel + ":999";  // Sample text for width estimation
        int textWidth = fm.horizontalAdvance(sampleText);
        int totalWidth = textWidth + 2 * mButtonWidth + 16;  // buttons + margins
        return QSize(totalWidth, qMax(24, textHeight + 8));
    } else {
        // Normal mode
        return QSize(180, qMax(28, textHeight + 12));
    }
}

QSize BlenderStyleValueField::minimumSizeHint() const
{
    if (mCompactMode) {
        return QSize(80, 24);  // Compact minimum
    } else {
        return QSize(120, 24);  // Normal minimum
    }
}

void BlenderStyleValueField::onInputFinished()
{
    bool ok = false;
    double newValue = mInputField->text().toDouble(&ok);
    if (ok) {
        setValue(newValue);
    }
    hideInputField();
}

void BlenderStyleValueField::checkForSingleClick()
{
    // Timer expired without drag starting - treat as click
    if (mPotentialClick && !mDragging) {
        // Will be handled in mouseReleaseEvent
    }
}

