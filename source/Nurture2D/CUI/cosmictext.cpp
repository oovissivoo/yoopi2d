#include "cosmictext.h"

#include <QWidget>
#include <QApplication>
#include <QSettings>
#include "pencildef.h"

// Static member initialization
int CosmicText::s_fontSize = 9;
QColor CosmicText::s_color = QColor(0xb3, 0xb3, 0xb3); // #b3b3b3
bool CosmicText::s_customized = false;
QString CosmicText::s_fontFamily = QStringLiteral("Inter"); // overwritten at startup

QFont CosmicText::font()
{
    // Always read cosmicTextSize from settings so runtime changes propagate.
    // Fall back to s_fontSize (default 9) if the key is not set.
    QSettings settings(PENCIL2D, PENCIL2D);
    const int size = settings.value(QStringLiteral("cosmicTextSize"), s_fontSize).toInt();

    QFont f;
    f.setFamily(s_fontFamily);
    f.setPointSize(qMax(1, size));
    f.setWeight(QFont::Normal);       // 400 — maps to Inter-Regular.otf
    f.setStyle(QFont::StyleNormal);
    f.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferMatch));
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

QColor CosmicText::color()
{
    return s_color;
}

QPalette CosmicText::palette()
{
    QPalette pal;
    pal.setColor(QPalette::WindowText, s_color);
    pal.setColor(QPalette::ButtonText, s_color);
    pal.setColor(QPalette::Text, s_color);
    return pal;
}

QString CosmicText::styleSheet()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    const int size = settings.value(QStringLiteral("cosmicTextSize"), s_fontSize).toInt();
    return QString("font-family: \"%1\"; color: %2; font-size: %3pt; font-weight: 400;")
           .arg(s_fontFamily)
           .arg(s_color.name())
           .arg(qMax(1, size));
}

void CosmicText::applyToWidget(QWidget* widget)
{
    if (!widget) return;

    // Set the QFont directly — this propagates to all children that don't
    // override their own font, including custom-painted widgets.
    widget->setFont(font());
    widget->setPalette(palette());
}

int CosmicText::fontSize()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    return settings.value(QStringLiteral("cosmicTextSize"), s_fontSize).toInt();
}

QString CosmicText::colorHex()
{
    return s_color.name();
}

void CosmicText::setFontSize(int points)
{
    s_fontSize = qMax(1, points);
    s_customized = true;
}

void CosmicText::setColor(const QColor& color)
{
    s_color = color;
    s_customized = true;
}

void CosmicText::setFontFamily(const QString& family)
{
    if (!family.isEmpty())
        s_fontFamily = family;
}

QString CosmicText::fontFamily()
{
    return s_fontFamily;
}
