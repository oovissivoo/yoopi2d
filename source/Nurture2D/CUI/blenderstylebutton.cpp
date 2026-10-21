#include "blenderstylebutton.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QApplication>
#include <QIcon>
#include <QTimer>
#include <QSettings>

#include "preferencesdef.h"
#include "pencildef.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

BlenderStyleButton::BlenderStyleButton(QWidget *parent)
    : QWidget(parent)
    , mBorderless(false)
    , mHovered(false)
    , mPressed(false)
    , mEnabled(true)
    , mFlashing(false)
    , mCheckable(false)
    , mChecked(false)
    , mCornerRadius(0)
    , mCornerMode(AllCorners)
    , mBorderSides(BorderAll)
    , mFlashTimer(new QTimer(this))
    , mFlashDuration(150)
{
    initializeColors();
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    mFlashTimer->setSingleShot(true);
    connect(mFlashTimer, &QTimer::timeout, this, &BlenderStyleButton::onFlashTimeout);
}

BlenderStyleButton::~BlenderStyleButton()
{
}

void BlenderStyleButton::initializeColors()
{
    // Blender-style dark theme defaults
    mAccentColor = getAccentColor();
    mBackgroundColor = QColor(50, 50, 50);      // Dark gray background
    mTextColor = QColor(220, 220, 220);         // Light gray text
}

QColor BlenderStyleButton::getAccentColor()
{
    // Read the global accent color via BlenderStyleSlider helper
    return BlenderStyleSlider::getAccentColor();
}

void BlenderStyleButton::setBorderless(bool borderless)
{
    if (mBorderless != borderless) {
        mBorderless = borderless;
        update();
    }
}

void BlenderStyleButton::setIcon(const QPixmap &icon)
{
    mIcon = icon;
    update();
    updateGeometry();
}

void BlenderStyleButton::setIcon(const QIcon &icon, const QSize &size)
{
    mIcon = icon.pixmap(size);
    update();
    updateGeometry();
}

void BlenderStyleButton::setText(const QString &text)
{
    if (mText != text) {
        mText = text;
        update();
        updateGeometry();
    }
}

void BlenderStyleButton::setCheckable(bool checkable)
{
    mCheckable = checkable;
    update();
}

void BlenderStyleButton::setChecked(bool checked)
{
    if (mCheckable && mChecked != checked) {
        mChecked = checked;
        emit toggled(checked);
        update();
    }
}

void BlenderStyleButton::setEnabled(bool enabled)
{
    if (mEnabled != enabled) {
        mEnabled = enabled;
        update();
    }
    QWidget::setEnabled(enabled);
}

void BlenderStyleButton::setAccentColor(const QColor &color)
{
    mAccentColor = color;
    update();
}

void BlenderStyleButton::setBackgroundColor(const QColor &color)
{
    mBackgroundColor = color;
    update();
}

void BlenderStyleButton::setTextColor(const QColor &color)
{
    mTextColor = color;
    update();
}

void BlenderStyleButton::setCornerRadius(int radius)
{
    mCornerRadius = radius;
    update();
}

void BlenderStyleButton::setCornerMode(CornerMode mode)
{
    if (mCornerMode != mode) {
        mCornerMode = mode;
        update();
    }
}

void BlenderStyleButton::setBorderSides(BorderSides sides)
{
    if (mBorderSides != sides) {
        mBorderSides = sides;
        update();
    }
}

QSize BlenderStyleButton::sizeHint() const
{
    int width = 0;
    int height = 24;  // Default height
    
    if (!mIcon.isNull()) {
        width += mIcon.width() + 4;  // Icon + padding
        height = qMax(height, mIcon.height() + 4);
    }
    
    if (!mText.isEmpty()) {
        QFontMetrics fm(font());
        width += fm.horizontalAdvance(mText) + 8;
        height = qMax(height, fm.height() + 8);
    }
    
    // Minimum size for icon-only buttons
    if (mIcon.isNull() && mText.isEmpty()) {
        width = 24;
    }
    
    return QSize(width, height);
}

QSize BlenderStyleButton::minimumSizeHint() const
{
    return QSize(24, 24);
}

void BlenderStyleButton::startFlash()
{
    mFlashing = true;
    mFlashTimer->start(mFlashDuration);
    update();
}

void BlenderStyleButton::onFlashTimeout()
{
    mFlashing = false;
    update();
}

void BlenderStyleButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect buttonRect = rect();
    
    // Determine button state colors
    QColor bgColor = mBackgroundColor;
    bool showBackground = true;
    
    if (!mEnabled) {
        bgColor = mBackgroundColor.darker(120);
    } else if (mFlashing) {
        // Flash with accent color on click
        bgColor = mAccentColor;
    } else if (mCheckable && mChecked) {
        // Checked state shows accent color
        bgColor = mAccentColor;
    } else if (mPressed) {
        bgColor = mBackgroundColor.lighter(130);
    } else if (mHovered) {
        if (mBorderless) {
            // In borderless mode, show accent-colored highlight on hover
            bgColor = getAccentColor();
        } else {
            bgColor = mBackgroundColor.lighter(115);
        }
    } else {
        if (mBorderless) {
            // In borderless mode with no hover, don't show background
            showBackground = false;
        }
    }
    
    // Draw background as an angular rectangle regardless of corner mode
    if (showBackground) {
        painter.fillRect(buttonRect, bgColor);
    }

    // Draw border if not borderless or if we have specific border sides
    if (!mBorderless && mBorderSides != BorderNone) {
        painter.setPen(QPen(mBackgroundColor.darker(150), 1));
        painter.setBrush(Qt::NoBrush);

        if (mBorderSides == BorderAll) {
            // Simple full rectangle border
            painter.drawRect(buttonRect.adjusted(0, 0, -1, -1));
        } else {
            // Draw only specified border sides
            if (mBorderSides & BorderLeft) {
                painter.drawLine(buttonRect.left(), buttonRect.top(),
                                 buttonRect.left(), buttonRect.bottom());
            }
            if (mBorderSides & BorderTop) {
                painter.drawLine(buttonRect.left(), buttonRect.top(),
                                 buttonRect.right(), buttonRect.top());
            }
            if (mBorderSides & BorderRight) {
                painter.drawLine(buttonRect.right(), buttonRect.top(),
                                 buttonRect.right(), buttonRect.bottom());
            }
            if (mBorderSides & BorderBottom) {
                painter.drawLine(buttonRect.left(), buttonRect.bottom(),
                                 buttonRect.right(), buttonRect.bottom());
            }
        }
    }
    
    // Draw content (icon and/or text)
    int contentX = buttonRect.left();
    int contentY = buttonRect.top();
    int contentWidth = buttonRect.width();
    int contentHeight = buttonRect.height();
    
    // Calculate total content width for centering
    int totalContentWidth = 0;
    if (!mIcon.isNull()) {
        totalContentWidth += mIcon.width();
    }
    if (!mText.isEmpty()) {
        QFontMetrics fm(font());
        if (totalContentWidth > 0) totalContentWidth += 4; // spacing
        totalContentWidth += fm.horizontalAdvance(mText);
    }
    
    // Center content
    int startX = contentX + (contentWidth - totalContentWidth) / 2;
    int currentX = startX;
    
    // Draw icon
    if (!mIcon.isNull()) {
        int iconY = contentY + (contentHeight - mIcon.height()) / 2;
        
        // Apply opacity for disabled state
        if (!mEnabled) {
            painter.setOpacity(0.5);
        }
        
        painter.drawPixmap(currentX, iconY, mIcon);
        
        if (!mEnabled) {
            painter.setOpacity(1.0);
        }
        
        currentX += mIcon.width() + 4;
    }
    
    // Draw text
    if (!mText.isEmpty()) {
        QColor textColor = mTextColor;
        
        // Determine text color based on state
        if (!mEnabled) {
            textColor = textColor.darker(150);
        } else if (mCheckable && mChecked) {
            // Use contrasting color for checked state
            textColor = (mAccentColor.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);
        } else if (mFlashing) {
            textColor = (mAccentColor.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);
        }
        
        painter.setPen(textColor);
        
        QFontMetrics fm(font());
        int textY = contentY + (contentHeight + fm.ascent() - fm.descent()) / 2;
        
        painter.drawText(currentX, textY, mText);
    }
}

void BlenderStyleButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mEnabled) {
        mPressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void BlenderStyleButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mEnabled) {
        if (mPressed && rect().contains(event->pos())) {
            // Button was clicked
            startFlash();
            
            if (mCheckable) {
                setChecked(!mChecked);
            }
            
            emit clicked();
        }
        
        mPressed = false;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void BlenderStyleButton::mouseMoveEvent(QMouseEvent *event)
{
    bool wasHovered = mHovered;
    mHovered = rect().contains(event->pos());
    
    if (wasHovered != mHovered) {
        update();
    }
    
    QWidget::mouseMoveEvent(event);
}

void BlenderStyleButton::leaveEvent(QEvent *event)
{
    mHovered = false;
    mPressed = false;
    update();
    QWidget::leaveEvent(event);
}
