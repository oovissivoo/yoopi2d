#ifndef COSMICTEXT_H
#define COSMICTEXT_H

#include <QColor>
#include <QFont>
#include <QString>
#include <QPalette>

// CosmicText - Centralized text styling system for Nurture2D
//
// Provides consistent text appearance across all UI components.
// Use this class to ensure all text uses the same font, size, and color.
//
// Usage examples:
//   label->setFont(CosmicText::font());           // Set font
//   label->setPalette(CosmicText::palette());     // Set color palette
//   label->setStyleSheet(CosmicText::styleSheet()); // Apply full style
//   CosmicText::applyToWidget(label);             // Apply all at once
//
// Default values:
//   - Font family: "Inter" (verified at startup via QFontDatabase)
//   - Font size: 9pt (consistent with main menu)
//   - Color: #b3b3b3 (light gray, same as cosmic components)
//   - Weight: Normal (400)

class CosmicText
{
public:
    // Get the standard cosmic font
    static QFont font();
    
    // Get the standard cosmic text color
    static QColor color();
    
    // Get a palette with cosmic text color
    static QPalette palette();
    
    // Get a stylesheet string for cosmic text styling
    static QString styleSheet();
    
    // Apply cosmic text styling to any widget
    static void applyToWidget(QWidget* widget);
    
    // Get font size in points
    static int fontSize();
    
    // Get color as hex string
    static QString colorHex();
    
    // Set custom font size (affects all subsequent calls)
    static void setFontSize(int points);
    
    // Set custom color (affects all subsequent calls)
    static void setColor(const QColor& color);

    // Set the verified font family name (called once at startup after font loading)
    static void setFontFamily(const QString& family);

    // Get the current font family name
    static QString fontFamily();

private:
    static int     s_fontSize;
    static QColor  s_color;
    static bool    s_customized;
    static QString s_fontFamily;
};

#endif // COSMICTEXT_H
