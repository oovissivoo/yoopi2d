#include "blenderstyletoggle.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"

BlenderStyleToggle::BlenderStyleToggle(QWidget *parent)
    : QWidget(parent)
    , mChecked(false)
    , mHovered(false)
    , mPressed(false)
    , mEnabled(true)
    , mCornerRadius(0)
    , mCornerMode(AllCorners)
    , mBorderSides(BorderNone)
    , mBorderlessMode(false)
{
    initializeColors();
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

BlenderStyleToggle::~BlenderStyleToggle()
{
}

void BlenderStyleToggle::initializeColors()
{
    // Blender-style dark theme defaults
    mBackgroundColor = QColor(31, 31, 31);      // Dark background for OFF state (0x1f1f1fff)
    // Use app text color for consistency (0xb2b2b2ff)
    mTextColor = QColor(178, 178, 178);
    // Initialize accent and selected-text color via setter for consistent logic
    setAccentColor(getAccentColor());
}

QColor BlenderStyleToggle::getAccentColor()
{
    // Use the global accent color from the shared Blender-style helper
    return BlenderStyleSlider::getAccentColor();
}

void BlenderStyleToggle::setBorderlessMode(bool enabled)
{
    if (mBorderlessMode != enabled) {
        mBorderlessMode = enabled;
        update();
    }
}

void BlenderStyleToggle::setChecked(bool checked)
{
    if (mChecked != checked) {
        mChecked = checked;
        emit toggled(checked);
        update();
    }
}

void BlenderStyleToggle::setText(const QString &text)
{
    mText = text;
    update();
    updateGeometry();
}

void BlenderStyleToggle::setIcon(const QPixmap &icon)
{
    mIcon = icon;
    update();
    updateGeometry();
}

void BlenderStyleToggle::clearIcon()
{
    mIcon = QPixmap();
    update();
    updateGeometry();
}

void BlenderStyleToggle::setEnabled(bool enabled)
{
    mEnabled = enabled;
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
    update();
}

void BlenderStyleToggle::setAccentColor(const QColor &color)
{
    mAccentColor = color;

    // Adjust selected text color for readability against the accent background
    const int lightness = mAccentColor.lightness();
    if (lightness > 140) {
        // Light accent color -> use dark text
        mSelectedTextColor = QColor("#111111");
    } else {
        // Dark accent color -> use light text
        mSelectedTextColor = QColor("#ffffff");
    }

    update();
}

void BlenderStyleToggle::setBackgroundColor(const QColor &color)
{
    mBackgroundColor = color;
    update();
}

void BlenderStyleToggle::setTextColor(const QColor &color)
{
    mTextColor = color;
    update();
}

void BlenderStyleToggle::setSelectedTextColor(const QColor &color)
{
    mSelectedTextColor = color;
    update();
}

void BlenderStyleToggle::setCornerRadius(int radius)
{
    mCornerRadius = radius;
    update();
}

void BlenderStyleToggle::setCornerMode(CornerMode mode)
{
    mCornerMode = mode;
    update();
}

void BlenderStyleToggle::setBorderSides(BorderSides sides)
{
    mBorderSides = sides;
    update();
}

QSize BlenderStyleToggle::sizeHint() const
{
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(mText);
    int iconWidth = mIcon.isNull() ? 0 : mIcon.width() + 4;
    int width = textWidth + iconWidth + 16; // 16px padding
    int height = fm.height() + 8; // 8px padding
    return QSize(width, height);
}

QSize BlenderStyleToggle::minimumSizeHint() const
{
    QFontMetrics fm(font());
    int height = fm.height() + 8;
    return QSize(60, height);
}

void BlenderStyleToggle::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect buttonRect = rect();
    
    // Angular button: simple rectangle path regardless of corner mode
    QPainterPath buttonPath;
    buttonPath.addRect(buttonRect);
    
    // Background:
    // Normal mode:
    //   - Inactive: neutral light gray background
    //   - Active (checked): accent-colored background
    //   - Hovered (inactive): slightly lightened background
    // Borderless mode:
    //   - Inactive: no background at all (icon/text only)
    //   - Active (checked): accent-colored background
    QColor bgColor = mBackgroundColor;
    const bool drawBackground = (!mBorderlessMode && mEnabled) || (mBorderlessMode && mChecked && mEnabled);

    if (mChecked && mEnabled) {
        bgColor = mAccentColor;
    } else if (!mBorderlessMode && mHovered && mEnabled) {
        bgColor = mBackgroundColor.lighter(115);
    }

    if (drawBackground) {
        painter.fillPath(buttonPath, bgColor);
    }

    // Optional borders for grouped layouts; default is no border.
    if (!mBorderlessMode && mBorderSides != BorderNone) {
        painter.setPen(QPen(bgColor.darker(150), 1));
        painter.setBrush(Qt::NoBrush);

        // Draw only the specified border sides
        if (mBorderSides & BorderLeft) {
            painter.drawLine(buttonRect.left(), buttonRect.top(), buttonRect.left(), buttonRect.bottom());
        }
        if (mBorderSides & BorderTop) {
            painter.drawLine(buttonRect.left(), buttonRect.top(), buttonRect.right(), buttonRect.top());
        }
        if (mBorderSides & BorderRight) {
            painter.drawLine(buttonRect.right(), buttonRect.top(), buttonRect.right(), buttonRect.bottom());
        }
        if (mBorderSides & BorderBottom) {
            painter.drawLine(buttonRect.left(), buttonRect.bottom(), buttonRect.right(), buttonRect.bottom());
        }
    }

    // Determine text color
    QColor textColor;
    if (!mEnabled) {
        textColor = mTextColor.darker(150);
    } else if (mChecked) {
        // Active: text color chosen for contrast with accent background
        textColor = mSelectedTextColor;
    } else if (!mBorderlessMode && mHovered) {
        // Hovered but inactive in normal mode: slightly brighter than default
        textColor = mTextColor.lighter(120);
    } else {
        // Inactive text: keep it dark for readability on light gray, or as-is in borderless mode
        textColor = mTextColor;
    }

    painter.setPen(textColor);

    // Set font (always normal weight)
    QFont textFont = font();
    painter.setFont(textFont);
    
    // Calculate content position (icon + text centered)
    QFontMetrics fm(textFont);
    int contentWidth = 0;
    int iconWidth = 0;
    
    if (!mIcon.isNull()) {
        iconWidth = mIcon.width();
        contentWidth += iconWidth + 4; // 4px spacing between icon and text
    }
    
    if (!mText.isEmpty()) {
        contentWidth += fm.horizontalAdvance(mText);
    }
    
    int xOffset = (buttonRect.width() - contentWidth) / 2;
    int yCenter = buttonRect.center().y();
    
    // Draw icon if present
    if (!mIcon.isNull()) {
        QRect iconRect(xOffset, yCenter - mIcon.height() / 2, mIcon.width(), mIcon.height());
        painter.drawPixmap(iconRect, mIcon);
        xOffset += iconWidth + 4;
    }
    
    // Draw text centered (supports multi-line vertical labels)
    if (!mText.isEmpty()) {
        QRect textRect(buttonRect);
        painter.drawText(textRect, Qt::AlignCenter, mText);
    }
}

void BlenderStyleToggle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mEnabled) {
        mPressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void BlenderStyleToggle::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mEnabled) {
        if (mPressed && rect().contains(event->pos())) {
            // Toggle the state
            setChecked(!mChecked);
            emit clicked();
        }
        mPressed = false;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void BlenderStyleToggle::mouseMoveEvent(QMouseEvent *event)
{
    bool nowHovered = rect().contains(event->pos()) && mEnabled;
    
    if (nowHovered != mHovered) {
        mHovered = nowHovered;
        update();
    }
    
    QWidget::mouseMoveEvent(event);
}

void BlenderStyleToggle::leaveEvent(QEvent *event)
{
    mHovered = false;
    update();
    QWidget::leaveEvent(event);
}

