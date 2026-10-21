#include "blenderstyleslider.h"
#include <QMouseEvent>
#include <QApplication>
#include <QToolTip>
#include <QPainter>
#include <QStyleOptionSlider>
#include <QDateTime>
#include <QKeyEvent>
#include <QShowEvent>
#include <cmath>
#include <QSettings>
#include <QLayout>
#include <QLabel>
#include "preferencesdef.h"
#include "pencildef.h"
#include "../CUI/cosmicuimetrics.h"

QColor BlenderStyleSlider::defaultSliderColor()
{
    return QColor(255, 170, 64);
}

QColor BlenderStyleSlider::getAccentColor()
{
    // Get the accent color from preferences (stored as ARGB int)
    QSettings settings(PENCIL2D, PENCIL2D);
    const int stored = settings.value(SETTING_ACCENT_COLOR, 0xFF505050).toInt();
    const QColor fromPrefs = QColor::fromRgba(static_cast<QRgb>(stored));
    if (fromPrefs.isValid()) {
        return fromPrefs;
    }
    return QColor(0x50, 0x50, 0x50);
}

void BlenderStyleSlider::updateAccentColor()
{
    // Update the slider color when accent color preference changes
    setSliderColor(getAccentColor());
}

BlenderStyleSlider::BlenderStyleSlider(QWidget *parent)
    : QSlider(parent)
    , isDragging(false)
    , defaultValue(0)
    , valueFormat("%d")
    , showValueTooltip(true)
    , sliderColor(defaultSliderColor())
    , valueLineEdit(nullptr)
    , mDisplayValue(0.0)
    , mUseDisplayValue(false)
    , mDisplayToSliderCallback(nullptr)
{
    setMinimumHeight(22);
}

BlenderStyleSlider::BlenderStyleSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
    , isDragging(false)
    , defaultValue(0)
    , valueFormat("%d")
    , showValueTooltip(true)
    , sliderColor(defaultSliderColor())
    , valueLineEdit(nullptr)
    , mDisplayValue(0.0)
    , mUseDisplayValue(false)
    , mDisplayToSliderCallback(nullptr)
{
    if (orientation == Qt::Horizontal) {
        setMinimumHeight(22);
    } else {
        setMinimumWidth(22);
    }
}

BlenderStyleSlider::~BlenderStyleSlider()
{
    destroyLineEdit();
}

void BlenderStyleSlider::setDefaultValue(int value)
{
    defaultValue = value;
}

void BlenderStyleSlider::setValueDisplayFormat(const QString &format)
{
    valueFormat = format;
}

void BlenderStyleSlider::setShowValueTooltip(bool show)
{
    showValueTooltip = show;
}

void BlenderStyleSlider::setPropertyName(const QString &name)
{
    mPropertyName = name;
    update();
}

void BlenderStyleSlider::setSliderColor(const QColor &color)
{
    sliderColor = color;
    update();  // Repaint with new color
}

void BlenderStyleSlider::setDisplayValue(qreal displayValue)
{
    mDisplayValue = displayValue;
    mUseDisplayValue = true;
    update();
}

void BlenderStyleSlider::setDisplayToSliderCallback(std::function<int(qreal)> callback)
{
    mDisplayToSliderCallback = callback;
}

void BlenderStyleSlider::mousePressEvent(QMouseEvent *event)
{
    // If we're in inline edit mode, keep focus on the editor and ignore slider interactions
    if (valueLineEdit) {
        valueLineEdit->setFocus();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            showValueInputDialog();
            event->accept();
            return;
        }
        isDragging = true;
        setValue(valueFromPosition(event->pos()));
        update();
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void BlenderStyleSlider::mouseMoveEvent(QMouseEvent *event)
{
    // While editing inline value, ignore drags
    if (valueLineEdit) {
        event->accept();
        return;
    }
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        setValue(valueFromPosition(event->pos()));
        if (showValueTooltip) {
            QToolTip::showText(event->globalPos(), QString::number(value()), this);
        }
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void BlenderStyleSlider::mouseReleaseEvent(QMouseEvent *event)
{
    // While editing inline value, ignore releases
    if (valueLineEdit) {
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        QToolTip::hideText();
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

void BlenderStyleSlider::mouseDoubleClickEvent(QMouseEvent *event)
{
    QSlider::mouseDoubleClickEvent(event);
}

void BlenderStyleSlider::paintEvent(QPaintEvent *event)
{
    // Don't call the base class's paintEvent to fully control the appearance
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    
    // Get the slider's bounds - use full widget area without inset for thicker appearance
    QRect sliderRect = rect().adjusted(0, 0, 0, 0);
    
    if (orientation() == Qt::Horizontal) {
        // Calculate the filled portion based on the current value
        double proportion = (value() - minimum()) / static_cast<double>(maximum() - minimum());
        int filledWidth = static_cast<int>(sliderRect.width() * proportion);
        
        // Draw the background (angular)
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 40, 40));  // Dark background like Blender
        painter.drawRect(sliderRect);
        
        // Draw the filled portion in the slider color
        QRect filledRect = sliderRect;
        filledRect.setWidth(filledWidth);
        painter.setBrush(sliderColor);
        painter.drawRect(filledRect);
        
        // Only draw the text if we're not in line edit mode
        if (!valueLineEdit) {
            painter.setPen(Qt::white);
            // Use a smaller font for narrow sliders to prevent crowding
            int narrowFontSize = (sliderRect.width() <= 24) ? 8 : 9;
            painter.setFont(QFont("Arial", narrowFontSize));
            
            // Format the value text
            QString valueText;
            if (mUseDisplayValue) {
                if (valueFormat.contains("%")) {
                    valueText = valueFormat;
                    valueText.replace("%d", QString::number(mDisplayValue, 'f', (mDisplayValue == qRound(mDisplayValue)) ? 0 : 1));
                } else {
                    valueText = QString::number(mDisplayValue, 'f', (mDisplayValue == qRound(mDisplayValue)) ? 0 : 1);
                }
            } else {
                if (valueFormat.contains("%")) {
                    valueText = valueFormat;
                    valueText.replace("%d", QString::number(value()));
                } else {
                    valueText = QString::number(value());
                }
            }
            
            // Draw property name on the left and value on the right
            if (!mPropertyName.isEmpty()) {
                QRect leftRect = sliderRect.adjusted(8, 0, -8, 0);
                QRect rightRect = sliderRect.adjusted(8, 0, -8, 0);
                painter.drawText(leftRect, Qt::AlignLeft | Qt::AlignVCenter, mPropertyName);
                painter.drawText(rightRect, Qt::AlignRight | Qt::AlignVCenter, valueText);
            } else {
                // Fallback to centered text if no property name is set
                painter.drawText(sliderRect, Qt::AlignCenter, valueText);
            }
        }
    } else {
        // Similar logic for vertical sliders
        double proportion = (value() - minimum()) / static_cast<double>(maximum() - minimum());
        int filledHeight = static_cast<int>(sliderRect.height() * proportion);
        
        // Draw the background (angular)
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 40, 40));  // Dark background like Blender
        painter.drawRect(sliderRect);
        
        // Draw the filled portion from bottom up
        QRect filledRect = sliderRect;
        filledRect.setTop(sliderRect.bottom() - filledHeight);
        painter.setBrush(sliderColor);
        painter.drawRect(filledRect);
        
        // Only draw the text if we're not in line edit mode
        if (!valueLineEdit) {
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 9));
            
            // Format the value text
            QString valueText;
            if (mUseDisplayValue) {
                if (valueFormat.contains("%")) {
                    valueText = valueFormat;
                    valueText.replace("%d", QString::number(mDisplayValue, 'f', (mDisplayValue == qRound(mDisplayValue)) ? 0 : 1));
                } else {
                    valueText = QString::number(mDisplayValue, 'f', (mDisplayValue == qRound(mDisplayValue)) ? 0 : 1);
                }
            } else {
                if (valueFormat.contains("%")) {
                    valueText = valueFormat;
                    valueText.replace("%d", QString::number(value()));
                } else {
                    valueText = QString::number(value());
                }
            }
            
            // Draw property name on the left and value on the right
            if (!mPropertyName.isEmpty()) {
                QRect leftRect = sliderRect.adjusted(8, 0, -8, 0);
                QRect rightRect = sliderRect.adjusted(8, 0, -8, 0);
                painter.drawText(leftRect, Qt::AlignLeft | Qt::AlignVCenter, mPropertyName);
                painter.drawText(rightRect, Qt::AlignRight | Qt::AlignVCenter, valueText);
            } else {
                // Fallback to centered text if no property name is set
                painter.drawText(sliderRect, Qt::AlignCenter, valueText);
            }
        }
    }
}

 

void BlenderStyleSlider::showValueInputDialog()
{
    // Create a line edit directly on the slider where the click happened
    createLineEdit();
    
    // Position it appropriately
    positionLineEdit();
    
    // Set focus and select all text to allow immediate typing
    valueLineEdit->setFocus();
    valueLineEdit->selectAll();
    
    // Request a repaint to hide the slider text
    update();
}

void BlenderStyleSlider::createLineEdit()
{
    // Destroy existing line edit if one exists
    destroyLineEdit();
    
    // Create the line edit
    valueLineEdit = new QLineEdit(this);
    valueLineEdit->setAlignment(Qt::AlignCenter);
    valueLineEdit->setText(mUseDisplayValue ? QString::number(mDisplayValue, 'f', (mDisplayValue == qRound(mDisplayValue)) ? 0 : 1) : QString::number(value()));
    valueLineEdit->setMinimumWidth(50);
    valueLineEdit->installEventFilter(this);
    
    // Set up font and styling - transparent background with white text and accent selection
    valueLineEdit->setFrame(false);
    valueLineEdit->setFont(QFont("Arial", 9, QFont::Bold));
    valueLineEdit->setTextMargins(0, 0, 0, 0);
    valueLineEdit->setStyleSheet(CosmicInlineEdit::stylesheet());
    
    // Connect signals
    connect(valueLineEdit, &QLineEdit::editingFinished, this, &BlenderStyleSlider::onLineEditFinished);
}

void BlenderStyleSlider::destroyLineEdit()
{
    if (valueLineEdit) {
        valueLineEdit->removeEventFilter(this);
        disconnect(valueLineEdit, &QLineEdit::editingFinished, this, &BlenderStyleSlider::onLineEditFinished);
        delete valueLineEdit;
        valueLineEdit = nullptr;
        
        // Request a repaint to show the slider text again
        update();
    }
}

void BlenderStyleSlider::positionLineEdit()
{
    if (!valueLineEdit)
        return;
    
    // Calculate the center position of the slider
    QRect sliderRect = rect();
    
    // Height matches the text line, vertically centred in the slider
    const QFontMetrics fm(valueLineEdit->font());
    const int lineEditHeight = fm.height() + 4;
    const int lineEditWidth  = qMax(60, sliderRect.width() / 2);
    const int x = (sliderRect.width()  - lineEditWidth)  / 2;
    const int y = (sliderRect.height() - lineEditHeight) / 2;

    valueLineEdit->setGeometry(x, y, lineEditWidth, lineEditHeight);
    valueLineEdit->show();
}

void BlenderStyleSlider::onLineEditFinished()
{
    if (!valueLineEdit)
        return;
    
    // Get the text and attempt to convert to a value
    bool ok;
    qreal inputValue = valueLineEdit->text().toDouble(&ok);
    
    if (ok) {
        if (mUseDisplayValue && mDisplayToSliderCallback) {
            int sliderValue = mDisplayToSliderCallback(inputValue);
            sliderValue = qBound(minimum(), sliderValue, maximum());
            setValue(sliderValue);
        } else {
            int newValue = qRound(inputValue);
            newValue = qBound(minimum(), newValue, maximum());
            setValue(newValue);
        }
    }
    
    // Clean up
    destroyLineEdit();
}

bool BlenderStyleSlider::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == valueLineEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                // Enter/Return key - apply the value
                onLineEditFinished();
                return true;
            } else if (keyEvent->key() == Qt::Key_Escape) {
                // Escape key - cancel editing
                destroyLineEdit();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            // Commit value on focus loss (consistent with other inline edits)
            onLineEditFinished();
            return true;
        }
    }
    
    return QSlider::eventFilter(watched, event);
}


int BlenderStyleSlider::valueFromPosition(const QPoint &pos) const
{
    int minV = minimum();
    int maxV = maximum();
    if (orientation() == Qt::Horizontal) {
        int x = qBound(0, pos.x(), width());
        double ratio = width() > 0 ? static_cast<double>(x) / static_cast<double>(width()) : 0.0;
        return minV + static_cast<int>(std::round(ratio * (maxV - minV)));
    } else {
        int y = qBound(0, pos.y(), height());
        double ratio = height() > 0 ? static_cast<double>(y) / static_cast<double>(height()) : 0.0;
        // Vertical grows from bottom to top
        return minV + static_cast<int>(std::round((1.0 - ratio) * (maxV - minV)));
    }
}

void BlenderStyleSlider::showEvent(QShowEvent *event)
{
    QSlider::showEvent(event);
    // Auto-detect property name from adjacent label when first shown
    if (mPropertyName.isEmpty()) {
        autoDetectPropertyName();
    }
}

void BlenderStyleSlider::autoDetectPropertyName()
{
    // Try to find a QLabel in the same layout
    QWidget* parent = parentWidget();
    if (!parent) return;
    
    QLayout* layout = parent->layout();
    if (!layout) return;
    
    // Search for a QLabel that appears before this slider in the layout
    QLabel* foundLabel = nullptr;
    bool foundSlider = false;
    
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;
        
        QWidget* widget = item->widget();
        if (!widget) continue;
        
        // Check if this is our slider
        if (widget == this) {
            foundSlider = true;
            break;
        }
        
        // Check if this is a QLabel
        QLabel* label = qobject_cast<QLabel*>(widget);
        if (label && !label->text().isEmpty()) {
            foundLabel = label;
        }
    }
    
    // If we found a label before the slider, use its text and hide it
    if (foundLabel && foundSlider) {
        mPropertyName = foundLabel->text();
        foundLabel->hide();
        update();
    }
}