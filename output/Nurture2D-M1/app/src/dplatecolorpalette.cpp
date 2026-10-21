#include "dplatecolorpalette.h"
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QVariant>
#include <QDebug>

static QString styleForButton(bool active) {
    const QString borderCol = active ? "#ffd54f" : "#888";
    const int borderPx = active ? 3 : 2;
    return QString("QToolButton{border:%1px solid %2;border-radius:6px;background:transparent;}")
        .arg(borderPx)
        .arg(borderCol);
}

static QIcon makeSwatchIcon(const QColor& c, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    const int radius = 4;
    p.drawRoundedRect(pm.rect().adjusted(1,1,-1,-1), radius, radius);
    p.end();
    return QIcon(pm);
}

DplateColorPalette::DplateColorPalette(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    if (!layout) {
        qCritical() << "DplateColorPalette: Failed to create layout";
        return;
    }
    layout->setSpacing(6);
    layout->setContentsMargins(0,0,0,0);

    auto add = [&](const QColor& c, bool checked){
        QToolButton* b = new QToolButton(this);
        b->setMinimumSize(28,28);
        b->setMaximumSize(28,28);
        b->setIcon(makeSwatchIcon(c, 18));
        b->setIconSize(QSize(18,18));
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setChecked(checked);
        // Store preset color on the button to reliably rebuild styles later
        b->setProperty("presetColor", c);
        b->setStyleSheet(styleForButton(checked));
        if (checked) {
            auto* eff = new QGraphicsDropShadowEffect(b);
            eff->setBlurRadius(16);
            eff->setOffset(0, 0);
            eff->setColor(QColor(255, 213, 79, 200));
            b->setGraphicsEffect(eff);
        }
        connect(b, &QToolButton::clicked, this, [this, b, c]() {
            try {
                for (auto* child : this->findChildren<QToolButton*>()) {
                    const QColor col = qvariant_cast<QColor>(child->property("presetColor"));
                    const bool isActive = (child == b);
                    child->setChecked(isActive);
                    // Keep icon color fixed to its preset and only change border/glow
                    child->setIcon(makeSwatchIcon(col, 18));
                    child->setStyleSheet(styleForButton(isActive));
                    if (isActive) {
                        auto* eff = new QGraphicsDropShadowEffect(child);
                        eff->setBlurRadius(16);
                        eff->setOffset(0, 0);
                        eff->setColor(QColor(255, 213, 79, 200));
                        child->setGraphicsEffect(eff);
                    } else {
                        child->setGraphicsEffect(nullptr);
                    }
                }
                emit colorChanged(c);
            } catch (const std::exception& e) {
                qCritical() << "DplateColorPalette: Exception in button click handler:" << e.what();
            }
        });
        layout->addWidget(b);
        if (checked) emit colorChanged(c);
    };

    add(Qt::black, true);
    add(Qt::cyan, false);
    add(Qt::magenta, false);

    setLayout(layout);
    // Validate that buttons were created
    QList<QToolButton*> buttons = findChildren<QToolButton*>();
    if (buttons.isEmpty()) {
        qWarning() << "DplateColorPalette: No color buttons created";
    }
}

void DplateColorPalette::setActiveColor(const QColor& c)
{
    if (!c.isValid()) {
        qWarning() << "DplateColorPalette: Invalid color provided";
        return;
    }

    QList<QToolButton*> buttons = findChildren<QToolButton*>();
    if (buttons.isEmpty()) {
        qWarning() << "DplateColorPalette: No buttons found";
        return;
    }

    for (auto* child : buttons) {
        const QColor col = qvariant_cast<QColor>(child->property("presetColor"));
        const bool isActive = (col == c);
        child->setChecked(isActive);
        child->setIcon(makeSwatchIcon(col, 18));
        child->setStyleSheet(styleForButton(isActive));
        if (isActive) {
            auto* eff = new QGraphicsDropShadowEffect(child);
            eff->setBlurRadius(16);
            eff->setOffset(0, 0);
            eff->setColor(QColor(255, 213, 79, 200));
            child->setGraphicsEffect(eff);
            emit colorChanged(c);
        } else {
            child->setGraphicsEffect(nullptr);
        }
    }
}

void DplateColorPalette::updateGlow(QToolButton* active)
{
    if (!active) return;
    for (auto* child : findChildren<QToolButton*>()) {
        const bool isActive = (child == active);
        child->setStyleSheet(styleForButton(isActive));
        if (isActive) {
            auto* eff = new QGraphicsDropShadowEffect(child);
            eff->setBlurRadius(16);
            eff->setOffset(0, 0);
            eff->setColor(QColor(255, 213, 79, 200));
            child->setGraphicsEffect(eff);
        } else {
            child->setGraphicsEffect(nullptr);
        }
    }
} 