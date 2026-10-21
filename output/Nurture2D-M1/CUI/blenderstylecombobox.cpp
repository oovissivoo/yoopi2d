#include "blenderstylecombobox.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QApplication>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"

BlenderStyleComboBox::BlenderStyleComboBox(QWidget *parent)
    : QWidget(parent)
    , mCurrentIndex(-1)
    , mHoverIndex(-1)
    , mPressedIndex(-1)
    , mCornerRadius(0)
{
    initializeColors();
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

BlenderStyleComboBox::~BlenderStyleComboBox()
{
}

void BlenderStyleComboBox::initializeColors()
{
    // Blender-style dark theme defaults
    mAccentColor = getAccentColor();
    mBackgroundColor = QColor(50, 50, 50);      // Dark gray background
    mTextColor = QColor(220, 220, 220);         // Light gray text
    // Dynamic text color based on accent color lightness
    QColor accent = getAccentColor();
    mSelectedTextColor = (accent.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);
}

QColor BlenderStyleComboBox::getAccentColor()
{
    // Use the global accent color from the shared Blender-style helper
    return BlenderStyleSlider::getAccentColor();
}

void BlenderStyleComboBox::setOptions(const QStringList &options)
{
    mOptions = options;
    mEnabledStates.resize(options.size());
    mEnabledStates.fill(true); // All options enabled by default
    
    if (mCurrentIndex >= mOptions.size()) {
        mCurrentIndex = mOptions.isEmpty() ? -1 : 0;
    }
    update();
    updateGeometry();
}

void BlenderStyleComboBox::setCurrentIndex(int index)
{
    if (index < 0 || index >= mOptions.size()) {
        return;
    }
    
    if (mCurrentIndex != index) {
        mCurrentIndex = index;
        emit currentIndexChanged(index);
        update();
    }
}

QString BlenderStyleComboBox::currentText() const
{
    if (mCurrentIndex >= 0 && mCurrentIndex < mOptions.size()) {
        return mOptions[mCurrentIndex];
    }
    return QString();
}

void BlenderStyleComboBox::setOptionEnabled(int index, bool enabled)
{
    if (index >= 0 && index < mEnabledStates.size()) {
        mEnabledStates[index] = enabled;
        update();
    }
}

bool BlenderStyleComboBox::isOptionEnabled(int index) const
{
    if (index >= 0 && index < mEnabledStates.size()) {
        return mEnabledStates[index];
    }
    return false;
}

void BlenderStyleComboBox::setAccentColor(const QColor &color)
{
    mAccentColor = color;
    // Recalculate text color based on new accent color lightness
    mSelectedTextColor = (color.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);
    update();
}

void BlenderStyleComboBox::setBackgroundColor(const QColor &color)
{
    mBackgroundColor = color;
    update();
}

void BlenderStyleComboBox::setTextColor(const QColor &color)
{
    mTextColor = color;
    update();
}

void BlenderStyleComboBox::setSelectedTextColor(const QColor &color)
{
    mSelectedTextColor = color;
    update();
}

void BlenderStyleComboBox::setCornerRadius(int radius)
{
    mCornerRadius = radius;
    update();
}

QSize BlenderStyleComboBox::sizeHint() const
{
    QFontMetrics fm(font());
    int height = fm.height() + 8; // padding
    return QSize(200, height);
}

QSize BlenderStyleComboBox::minimumSizeHint() const
{
    QFontMetrics fm(font());
    int height = fm.height() + 8;
    int width = mOptions.size() * 40; // minimum 40px per option
    return QSize(width, height);
}

int BlenderStyleComboBox::indexAtPosition(const QPoint &pos) const
{
    if (mOptions.isEmpty()) return -1;
    
    int segmentWidth = width() / mOptions.size();
    int index = pos.x() / segmentWidth;
    
    if (index >= 0 && index < mOptions.size()) {
        return index;
    }
    return -1;
}

QRect BlenderStyleComboBox::segmentRect(int index) const
{
    if (index < 0 || index >= mOptions.size()) {
        return QRect();
    }
    
    int segmentWidth = width() / mOptions.size();
    int x = index * segmentWidth;
    
    return QRect(x, 0, segmentWidth, height());
}

void BlenderStyleComboBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    if (mOptions.isEmpty()) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QFontMetrics fm(font());
    
    // Determine widget-level enabled state
    bool widgetEnabled = isEnabled();
    
    // Draw the unified widget background (angular, no rounded corners)
    QPainterPath backgroundPath;
    backgroundPath.addRect(rect());
    
    // Apply disabled styling to entire widget if disabled
    QColor bgColor = mBackgroundColor;
    if (!widgetEnabled) {
        bgColor = mBackgroundColor.darker(110);
        bgColor.setRed(qMin(255, bgColor.red() + 20));
    }
    painter.fillPath(backgroundPath, bgColor);
    
    // Draw each segment
    for (int i = 0; i < mOptions.size(); ++i) {
        QRect segRect = segmentRect(i);
        bool isSelected = (i == mCurrentIndex);
        bool isHovered = (i == mHoverIndex);
        bool isPressed = (i == mPressedIndex);
        bool isEnabled = isOptionEnabled(i);
        
        // Create angular path for this segment (no rounded corners)
        QPainterPath segmentPath;
        segmentPath.addRect(segRect);
        
        // Fill the segment based on state
        if (!widgetEnabled || !isEnabled) {
            // Disabled state - darker and show with red tint
            QColor disabledBg = mBackgroundColor.darker(110);
            disabledBg.setRed(qMin(255, disabledBg.red() + 20));
            painter.fillPath(segmentPath, disabledBg);
        } else if (isSelected) {
            // Selected state - bright accent color
            painter.fillPath(segmentPath, mAccentColor);
        } else if (isPressed) {
            // Pressed state - slightly lighter than background
            painter.fillPath(segmentPath, mBackgroundColor.lighter(130));
        } else if (isHovered) {
            // Hover state - slightly lighter than background
            painter.fillPath(segmentPath, mBackgroundColor.lighter(115));
        }
        
        // Draw separator lines between segments (except at edges and selected segments)
        if (i > 0 && !isSelected && mCurrentIndex != i - 1) {
            painter.setPen(QPen(QColor(30, 30, 30), 1));
            painter.drawLine(segRect.left(), segRect.top() + 2, 
                           segRect.left(), segRect.bottom() - 2);
        }
        
        // Draw text
        QColor textColor;
        if (!widgetEnabled || !isEnabled) {
            // Disabled text - red tint
            textColor = QColor(211, 47, 47); // Red color for disabled
        } else {
            textColor = isSelected ? mSelectedTextColor : mTextColor;
        }
        painter.setPen(textColor);
        
        QFont textFont = font();
        if (isSelected && isEnabled) {
            textFont.setBold(true);
        }
        painter.setFont(textFont);
        
        painter.drawText(segRect, Qt::AlignCenter, mOptions[i]);
    }
    
    // Draw outer border for the entire widget
    painter.setPen(QPen(mBackgroundColor.darker(150), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), mCornerRadius, mCornerRadius);
}

void BlenderStyleComboBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int idx = indexAtPosition(event->pos());
        // Only allow pressing enabled options
        if (idx >= 0 && isOptionEnabled(idx)) {
            mPressedIndex = idx;
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void BlenderStyleComboBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int releaseIndex = indexAtPosition(event->pos());
        
        // Only trigger if released on the same enabled segment that was pressed
        if (releaseIndex >= 0 && releaseIndex == mPressedIndex && isOptionEnabled(releaseIndex)) {
            setCurrentIndex(releaseIndex);
            emit optionClicked(releaseIndex);
        }
        
        mPressedIndex = -1;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void BlenderStyleComboBox::mouseMoveEvent(QMouseEvent *event)
{
    int newHoverIndex = indexAtPosition(event->pos());
    
    // Only show hover for enabled options
    if (newHoverIndex >= 0 && !isOptionEnabled(newHoverIndex)) {
        newHoverIndex = -1;
    }
    
    if (newHoverIndex != mHoverIndex) {
        mHoverIndex = newHoverIndex;
        update();
    }
    
    QWidget::mouseMoveEvent(event);
}

void BlenderStyleComboBox::leaveEvent(QEvent *event)
{
    mHoverIndex = -1;
    update();
    QWidget::leaveEvent(event);
}

