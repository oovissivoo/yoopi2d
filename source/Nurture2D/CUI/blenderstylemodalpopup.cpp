#include "blenderstylemodalpopup.h"
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QApplication>
#include <qmath.h>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"

BlenderStyleModalPopup::BlenderStyleModalPopup(QWidget *parent)
    : QDialog(parent)
    , mGlowIntensity(0.8)
    , mBorderWidth(3)
    , mGlowPhase(0.0)
    , mControllingToggle(nullptr)
{
    // Frameless with custom border
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // NOT modal - we need the toggle to remain interactive!
    // Instead, we'll use event filtering to restrict interaction
    setModal(false);
    
    mAccentColor = getAccentColor();
    
    // Timer for pulsing glow animation
    mGlowTimer = new QTimer(this);
    connect(mGlowTimer, &QTimer::timeout, this, &BlenderStyleModalPopup::updateGlow);
    
    // Install event filter on application to restrict interactions
    // This will allow clicks on the toggle but block everything else
    qApp->installEventFilter(this);
}

BlenderStyleModalPopup::~BlenderStyleModalPopup()
{
    // Remove event filter when destroyed
    qApp->removeEventFilter(this);
}

QColor BlenderStyleModalPopup::getAccentColor()
{
    // Use the global accent color from the shared Blender-style helper
    return BlenderStyleSlider::getAccentColor();
}

void BlenderStyleModalPopup::setAccentColor(const QColor &color)
{
    mAccentColor = color;
    update();
}

void BlenderStyleModalPopup::setGlowIntensity(qreal intensity)
{
    mGlowIntensity = qBound(0.0, intensity, 1.0);
    update();
}

void BlenderStyleModalPopup::setBorderWidth(int width)
{
    mBorderWidth = qMax(1, width);
    update();
}

void BlenderStyleModalPopup::setControllingToggle(QWidget* toggle)
{
    mControllingToggle = toggle;
}

void BlenderStyleModalPopup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // Start glow animation
    mGlowTimer->start(50); // 20 FPS for smooth animation
}

void BlenderStyleModalPopup::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    // Stop glow animation
    mGlowTimer->stop();
}

bool BlenderStyleModalPopup::eventFilter(QObject *obj, QEvent *event)
{
    // Only filter events when this popup is visible
    if (!isVisible()) {
        return QDialog::eventFilter(obj, event);
    }
    
    // Allow interaction with:
    // 1. This popup itself
    // 2. The controlling toggle
    // 3. Children of this popup
    
    QWidget* widget = qobject_cast<QWidget*>(obj);
    if (!widget) {
        return QDialog::eventFilter(obj, event);
    }
    
    // Check if this widget is the popup, its children, or the controlling toggle
    bool isPopupOrChild = (widget == this) || isAncestorOf(widget);
    bool isControlToggle = (mControllingToggle && (widget == mControllingToggle || mControllingToggle->isAncestorOf(widget)));
    
    if (isPopupOrChild || isControlToggle) {
        // Allow this event
        return QDialog::eventFilter(obj, event);
    }
    
    // Block mouse and keyboard events for everything else
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick ||
        event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease) {
        // Block the event
        return true;
    }
    
    return QDialog::eventFilter(obj, event);
}

void BlenderStyleModalPopup::updateGlow()
{
    // Pulse the glow phase
    mGlowPhase += 0.1;
    if (mGlowPhase > 2.0 * M_PI) {
        mGlowPhase -= 2.0 * M_PI;
    }
    update();
}

void BlenderStyleModalPopup::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate glow brightness (pulsing effect)
    qreal glowPulse = 0.7 + 0.3 * qSin(mGlowPhase);
    qreal currentIntensity = mGlowIntensity * glowPulse;
    
    // Dark background
    QColor bgColor(45, 45, 45);
    painter.fillRect(rect(), bgColor);
    
    // Draw multiple glow layers for depth
    QRect contentRect = rect().adjusted(mBorderWidth * 2, mBorderWidth * 2, 
                                       -mBorderWidth * 2, -mBorderWidth * 2);
    
    // Outer glow (softer, wider)
    for (int i = mBorderWidth * 3; i > 0; --i) {
        qreal alpha = (currentIntensity * 0.4) * (1.0 - static_cast<qreal>(i) / (mBorderWidth * 3));
        QColor glowColor = mAccentColor;
        glowColor.setAlphaF(alpha);
        
        QPen glowPen(glowColor, 1);
        painter.setPen(glowPen);
        painter.setBrush(Qt::NoBrush);
        
        QRect glowRect = contentRect.adjusted(-i, -i, i, i);
        painter.drawRoundedRect(glowRect, 4, 4);
    }
    
    // Main bold border
    for (int i = 0; i < mBorderWidth; ++i) {
        qreal alpha = currentIntensity * (1.0 - static_cast<qreal>(i) / mBorderWidth);
        QColor borderColor = mAccentColor;
        borderColor.setAlphaF(alpha);
        
        QPen borderPen(borderColor, 2);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        
        QRect borderRect = contentRect.adjusted(-mBorderWidth + i, -mBorderWidth + i, 
                                                mBorderWidth - i, mBorderWidth - i);
        painter.drawRoundedRect(borderRect, 4, 4);
    }
    
    // Inner shadow for depth
    QColor shadowColor(0, 0, 0, 60);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadowColor);
    QRect shadowRect = contentRect.adjusted(0, 0, 0, 2);
    painter.drawRect(shadowRect);
}

