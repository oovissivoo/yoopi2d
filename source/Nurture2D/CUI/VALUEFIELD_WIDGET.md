# BlenderStyleValueField Widget

## Overview

`BlenderStyleValueField` is a sophisticated numeric input control that combines multiple interaction methods in a seamless, Blender-style interface. It provides three ways to adjust values:

1. **Click and drag** on the center field (slider-like behavior)
2. **Single click** on the center field to manually type a value
3. **Click** the arrow buttons to increment/decrement by steps

## Design Features

### Visual Components

```
┌─────┬─────────────────────────────────────┬─────┐
│  ←  │  Label:              Value          │  →  │
└─────┴─────────────────────────────────────┴─────┘
 Button         Center Field                Button
```

- **Left Button**: Decrement arrow (rounded left corners)
- **Center Field**: Shows label and value, interactive
- **Right Button**: Increment arrow (rounded right corners)
- **Seamless Design**: Single unified widget with connected sections
- **Rounded Corners**: Only on far left and right edges

### States

1. **Normal**: Standard background color
2. **Hover** (per section): Slightly lighter background
3. **Pressed** (buttons): More pronounced highlight
4. **Dragging** (center): Accent color tint showing active interaction
5. **Input Mode**: QLineEdit overlay for manual text entry

## Basic Usage

```cpp
#include "../../CUI/blenderstylevaluefield.h"

// Create widget
auto* rotationField = new BlenderStyleValueField(this);
rotationField->setLabel("Rotation");
rotationField->setRange(0.0, 360.0);
rotationField->setValue(0.0);
rotationField->setDecimals(1);
rotationField->setSingleStep(1.0);

// Connect to changes
connect(rotationField, &BlenderStyleValueField::valueChanged, 
        this, [](double value) {
    qDebug() << "Rotation changed to:" << value;
});

// Add to layout
layout->addWidget(rotationField);
```

## Properties

### Value Properties

- **value**: Current numeric value (double)
- **minimum**: Minimum allowed value (default: 0.0)
- **maximum**: Maximum allowed value (default: 100.0)
- **singleStep**: Amount to increment/decrement (default: 1.0)
- **decimals**: Number of decimal places to display (default: 3)

### Display Properties

- **label**: Text label shown on the left side of the center field
- **accentColor**: Color used during drag interaction
- **backgroundColor**: Base background color
- **textColor**: Color for label and value text
- **cornerRadius**: Radius for rounded corners (default: 3)

## Interaction Methods

### 1. Drag to Adjust

Click and drag horizontally on the center field:
- **Drag right**: Increase value
- **Drag left**: Decrease value
- **Hold Shift**: Fine control (10% speed)
- **Hold Ctrl**: Coarse control (10× speed)

The value change is proportional to:
```
valueChange = (dragDistance / fieldWidth) × (max - min)
```

### 2. Click to Type

Single click on the center field (without dragging) to open an input field:
- Type the desired value
- Press Enter or click away to confirm
- Invalid input is ignored, previous value retained

**Click Detection**: 
- Must move less than 5 pixels
- Timer-based threshold (150ms)

### 3. Increment/Decrement Buttons

- **Left button**: Decreases value by `singleStep`
- **Right button**: Increases value by `singleStep`
- Uses Qt's standard arrow icons (`SP_ArrowLeft`, `SP_ArrowRight`)

### 4. Mouse Wheel

Scroll while hovering over the widget:
- **Scroll up**: Increase by `singleStep`
- **Scroll down**: Decrease by `singleStep`
- **Shift + Scroll**: Fine control (10%)
- **Ctrl + Scroll**: Coarse control (10×)

## Customization

### Styling

```cpp
// Match accent color with other Blender widgets
field->setAccentColor(BlenderStyleSlider::getAccentColor());

// Custom colors
field->setBackgroundColor(QColor(50, 50, 55));
field->setTextColor(QColor(220, 220, 220));

// Adjust corner radius
field->setCornerRadius(5);
```

### Range Configuration

```cpp
// Rotation (0-360 degrees)
field->setRange(0.0, 360.0);
field->setSingleStep(1.0);
field->setDecimals(1);

// Opacity (0.0-1.0)
field->setRange(0.0, 1.0);
field->setSingleStep(0.01);
field->setDecimals(3);

// Large range with fine control
field->setRange(-1000.0, 1000.0);
field->setSingleStep(0.1);
field->setDecimals(2);
```

## Real-World Example: Camera Rotation

```cpp
void ToolOptionWidget::initCameraRotationField()
{
    // Create rotation value field
    mRotationField = new BlenderStyleValueField(this);
    mRotationField->setLabel("Rotation");
    mRotationField->setRange(0.0, 360.0);
    mRotationField->setValue(0.0);
    mRotationField->setDecimals(1);
    mRotationField->setSingleStep(1.0);
    mRotationField->setAccentColor(BlenderStyleSlider::getAccentColor());
    
    // Connect to camera property
    connect(mRotationField, &BlenderStyleValueField::valueChanged,
            this, [this](double value) {
        // Update camera rotation
        if (auto* camera = mEditor->currentCamera()) {
            camera->setRotation(value);
            emit needUpdateUI();
        }
    });
    
    // Add to layout
    ui->verticalLayout_2->addWidget(mRotationField);
}

void ToolOptionWidget::updateUI()
{
    // Sync UI with current camera state
    if (auto* camera = mEditor->currentCamera()) {
        QSignalBlocker blocker(mRotationField);
        mRotationField->setValue(camera->rotation());
    }
}
```

## Real-World Example: Background Opacity

```cpp
void LayerPanel::initBackgroundOpacityField()
{
    // Create opacity value field
    mBgOpacityField = new BlenderStyleValueField(this);
    mBgOpacityField->setLabel("Background");
    mBgOpacityField->setRange(0.0, 1.0);
    mBgOpacityField->setValue(0.5);
    mBgOpacityField->setDecimals(3);
    mBgOpacityField->setSingleStep(0.01);
    mBgOpacityField->setAccentColor(BlenderStyleSlider::getAccentColor());
    
    // Fine-tune button width for compact layout
    // (button width is internal but can be adjusted if needed)
    
    // Connect to layer opacity
    connect(mBgOpacityField, &BlenderStyleValueField::valueChanged,
            this, [this](double opacity) {
        if (auto* layer = mEditor->layers()->currentLayer()) {
            layer->setBackgroundOpacity(opacity);
        }
    });
    
    layout->addWidget(mBgOpacityField);
}
```

## Technical Details

### Hit Testing

The widget is divided into three regions:
```cpp
enum HitRegion {
    None,           // Outside widget
    LeftButton,     // Decrement button
    CenterField,    // Main interactive area
    RightButton     // Increment button
};
```

Each region has independent hover and press states.

### Click vs Drag Detection

Two mechanisms work together:
1. **Distance threshold**: If mouse moves > 5 pixels, it's a drag
2. **Time threshold**: 150ms timer for delayed click detection

```cpp
// On mouse press
mDragStartPos = event->pos();
mPotentialClick = true;
mClickTimer->start(150);

// On mouse move
if (distance > 5 pixels) {
    mDragging = true;
    mPotentialClick = false;
}

// On mouse release
if (mPotentialClick && !mDragging) {
    showInputField(); // Was a click
}
```

### Input Field Overlay

When single-clicked, a QLineEdit is shown:
- Positioned over the center field
- Pre-filled with current value
- All text pre-selected for easy replacement
- Accepts numeric input only (validated on finish)
- Automatically hidden and cleared on focus loss

### Value Clamping

All value changes are clamped to the [minimum, maximum] range:
```cpp
double clampValue(double val) const {
    return qBound(mMinimum, val, mMaximum);
}
```

### Painting Strategy

Three separate paths are painted:
1. **Left button path**: Rounded left corners only
2. **Center field rect**: Straight rectangle
3. **Right button path**: Rounded right corners only

Separator lines between sections create visual distinction without breaking the unified appearance.

## Size Hints

```cpp
QSize sizeHint() const {
    return QSize(180, 28);  // Width: 180px, Height: 28px
}

QSize minimumSizeHint() const {
    return QSize(120, 24);  // Minimum viable size
}
```

Button width is fixed at 24px, center field takes remaining space.

## Keyboard Modifiers

| Modifier | Drag Effect | Wheel Effect |
|----------|-------------|--------------|
| None     | Normal      | Normal       |
| Shift    | 10% speed   | 10% step     |
| Ctrl     | 10× speed   | 10× step     |

This allows precise control when needed and fast changes for large adjustments.

## Signals

```cpp
signals:
    void valueChanged(double value);
```

Emitted whenever the value changes through any interaction method (drag, buttons, input, wheel).

## Best Practices

1. **Label Length**: Keep labels short (5-12 characters) for optimal layout
2. **Decimal Places**: Use appropriate decimals for the value range:
   - Angles: 1 decimal (0.0-360.0)
   - Percentages: 0 decimals (0-100)
   - Ratios: 3 decimals (0.000-1.000)
3. **Step Size**: Set step to match the precision users need
4. **Signal Blocking**: Use `QSignalBlocker` when programmatically updating to avoid feedback loops
5. **Accent Color**: Always use `BlenderStyleSlider::getAccentColor()` for consistency

## Integration with Other Blender Widgets

The `BlenderStyleValueField` follows the same design philosophy as:
- `BlenderStyleComboBox`: Seamless segmented appearance
- `BlenderStyleToggle`: Rounded corners on edges
- `BlenderStyleSlider`: Drag interaction and accent colors

All widgets share:
- Consistent color scheme
- Similar hover/press states
- Same corner radius defaults
- Unified visual language

## Future Enhancements

Potential improvements:
- [ ] Suffix support (e.g., "°" for degrees, "%" for percentages)
- [ ] Prefix support (e.g., "+" for positive values)
- [ ] Exponential dragging for very large ranges
- [ ] Visual feedback bar showing position in range
- [ ] Custom button icons instead of Qt standard icons
- [ ] Animation on value change
- [ ] Right-click context menu for reset/copy/paste

## Widget Hierarchy

```
BlenderStyleValueField (QWidget)
├── QLineEdit (mInputField) [hidden overlay]
└── QTimer (mClickTimer) [single-shot]
```

The widget paints everything custom; no child widgets are visible in normal operation.

