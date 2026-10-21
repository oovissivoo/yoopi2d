/*
 * CosmicText Usage Examples
 * 
 * This file demonstrates how to use the CosmicText system for consistent text styling.
 * 
 * Include the header:
 * #include "../../CUI/cosmictext.h"
 * 
 * Usage Examples:
 */

// Example 1: Apply to a label
QLabel* myLabel = new QLabel("Menu Item");
myLabel->setFont(CosmicText::font());
myLabel->setPalette(CosmicText::palette());

// Example 2: Apply all styling at once
QLabel* anotherLabel = new QLabel("Settings");
CosmicText::applyToWidget(anotherLabel);

// Example 3: Use with stylesheet
QPushButton* button = new QPushButton("Save");
button->setStyleSheet(CosmicText::styleSheet());

// Example 4: Get individual properties
QFont standardFont = CosmicText::font();
QColor textColor = CosmicText::color();
QString colorHex = CosmicText::colorHex(); // "#b3b3b3"
int fontSize = CosmicText::fontSize();     // 9

// Example 5: Custom styling (affects all subsequent calls)
CosmicText::setFontSize(10);
CosmicText::setColor(QColor(200, 200, 200));
// Now all new CosmicText calls will use the custom values

// Example 6: In a widget constructor
class MyWidget : public QWidget
{
public:
    MyWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        auto* label = new QLabel("Cosmic Styled Text", this);
        CosmicText::applyToWidget(label);
        
        auto* button = new QPushButton("Click Me", this);
        button->setStyleSheet(CosmicText::styleSheet());
        
        // Layout setup...
    }
};

// Example 7: For menu items (consistent with main menu)
QAction* action = new QAction("File Action", this);
action->setFont(CosmicText::font());

// Example 8: For status bar text
QStatusBar* statusBar = new QStatusBar(this);
statusBar->setFont(CosmicText::font());
statusBar->setPalette(CosmicText::palette());
