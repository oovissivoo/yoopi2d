# CosmicText - Centralized Text Styling System

## Overview

CosmicText is a centralized text styling system for Nurture2D that provides consistent font and color appearance across all UI components. It ensures all text uses the same styling as the main menu (File, Edit, View, Canvas, etc.).

## Default Values

- **Font Size**: 9pt (consistent with main menu)
- **Color**: `#b3b3b3` (light gray, same as cosmic components)
- **Weight**: 400 (normal)

## Usage

### Basic Usage

```cpp
#include "../../CUI/cosmictext.h"

// Apply all styling at once
QLabel* label = new QLabel("Menu Item");
CosmicText::applyToWidget(label);

// Individual properties
label->setFont(CosmicText::font());
label->setPalette(CosmicText::palette());

// Using stylesheet
button->setStyleSheet(CosmicText::styleSheet());
```

### Advanced Usage

```cpp
// Get individual properties
QFont standardFont = CosmicText::font();
QColor textColor = CosmicText::color();
QString colorHex = CosmicText::colorHex(); // "#b3b3b3"
int fontSize = CosmicText::fontSize();     // 9

// Custom styling (affects all subsequent calls)
CosmicText::setFontSize(10);
CosmicText::setColor(QColor(200, 200, 200));
```

## API Reference

### Static Methods

- `QFont font()` - Returns the standard cosmic font
- `QColor color()` - Returns the standard cosmic text color
- `QPalette palette()` - Returns a palette with cosmic text color
- `QString styleSheet()` - Returns a stylesheet string for cosmic text
- `void applyToWidget(QWidget* widget)` - Applies cosmic text styling to any widget
- `int fontSize()` - Returns current font size in points
- `QString colorHex()` - Returns color as hex string
- `void setFontSize(int points)` - Sets custom font size
- `void setColor(const QColor& color)` - Sets custom color

## Integration with Existing Components

All existing cosmic components already use the same text color (`#b3b3b3`):
- CosmicButton
- CosmicInputField
- CosmicDropDown
- CosmicHSlider
- CosmicCheckboxToggle
- And more...

CosmicText provides a convenient way to access these same values for any widget.

## File Structure

```
CUI/
├── cosmictext.h                    - Header file with class declaration
├── cosmictext.cpp                  - Implementation file
└── COSMICTEXT_USAGE_EXAMPLES.cpp   - Usage examples and demonstrations
```

## Benefits

1. **Consistency**: All text uses the same styling across the application
2. **Maintainability**: Change text appearance in one place
3. **Convenience**: Easy to apply consistent styling to any widget
4. **Flexibility**: Can be customized when needed
5. **Integration**: Works seamlessly with existing cosmic components

## Examples in the Codebase

Look for these patterns in the existing code:

```cpp
// Old way (manual styling)
QLabel* label = new QLabel("Text");
label->setFont(QFont("Arial", 9));
label->setStyleSheet("color: #b3b3b3;");

// New way (using CosmicText)
QLabel* label = new QLabel("Text");
CosmicText::applyToWidget(label);
```

## Future Enhancements

- Add font family customization
- Support for different text variants (header, body, small)
- Integration with theme system
- Runtime font size adjustment from preferences
