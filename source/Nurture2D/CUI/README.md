# Blender-Style UI Widget Templates

This directory contains custom Qt widget templates designed to match Blender's modern UI aesthetic. These widgets are immediately usable throughout the Nurture2D application.

## Available Widgets

### 0. BlenderStyleDropDown

**Location:** `CUI/blenderstyledropdown.h` and `CUI/blenderstyledropdown.cpp`

Blender-like dropdown with a capsule button (label left, arrow right) and a popup list whose hover/selection highlight uses the accent color with rounded corners.

#### Basic Usage

```cpp
#include "../../CUI/blenderstyledropdown.h"

auto* dd = new BlenderStyleDropDown(this);
dd->setOptions({tr("Eevee"), tr("Cycles"), tr("Workbench")});
dd->setCurrentIndex(0);
dd->setAccentColor(BlenderStyleSlider::getAccentColor());
layout->addWidget(dd);

connect(dd, &BlenderStyleDropDown::currentIndexChanged, this, [](int i){
    qDebug() << "Dropdown index:" << i;
});
```

#### Visual Details
- **Capsule**: rounded rectangle with dark background
- **Arrow**: Qt built-in down arrow icon
- **Popup**: rounded accent hover/selection highlight, high-contrast selected text

---

### 1. BlenderStyleComboBox

**Location:** `CUI/blenderstylecombobox.h` and `CUI/blenderstylecombobox.cpp`

A segmented control widget that displays multiple mutually-exclusive options in a unified horizontal bar.

#### Design Features
- **Single unified widget** - Not separate buttons, giving a seamless appearance
- **Rounded corners** - Only the far left and far right corners are rounded (4px radius by default)
- **Seamless middle sections** - Center segments connect cleanly with no gaps
- **Visual states**:
  - Selected: Bright accent color background (default: blue #5BA5FF)
  - Hover: Slightly lighter than background
  - Pressed: Visual feedback during interaction
- **Dark theme optimized** - Default background #323232, text #DCDCDC
- **Accent color integration** - Respects user's accent color preferences

#### Basic Usage

```cpp
#include "../../CUI/blenderstylecombobox.h"

// Create the widget
auto* combo = new BlenderStyleComboBox(this);

// Set the available options
combo->setOptions({"Off", "Low", "Med", "High"});

// Set initial selection
combo->setCurrentIndex(0);

// Connect to changes
connect(combo, &BlenderStyleComboBox::currentIndexChanged, [](int index){
    qDebug() << "Selected index:" << index;
});

// Add to layout
layout->addWidget(combo);
```

#### Customization

```cpp
// Set custom accent color
combo->setAccentColor(QColor(255, 100, 50));

// Set custom background
combo->setBackgroundColor(QColor(40, 40, 40));

// Set text colors
combo->setTextColor(QColor(220, 220, 220));
combo->setSelectedTextColor(QColor(17, 17, 17));

// Adjust corner radius
combo->setCornerRadius(6);
```

#### Getting Current Selection

```cpp
// Get selected index (0-based)
int index = combo->currentIndex();

// Get selected text
QString text = combo->currentText();
```

#### Disabling Options

```cpp
// Disable a specific option (shown in red, not clickable)
combo->setOptionEnabled(1, false);

// Check if an option is enabled
bool enabled = combo->isOptionEnabled(1);

// Re-enable the option
combo->setOptionEnabled(1, true);
```

**Visual appearance when disabled:**
- Background: Darker with red tint
- Text: Red color (#d32f2f) 
- Interaction: No hover state, not clickable

#### Signals

- `currentIndexChanged(int index)` - Emitted when selection changes
- `optionClicked(int index)` - Emitted on every click, even if same option

#### Real-World Examples

**Example 1: Post-Stroke Correction** (lines 142-166)

See `app/src/tooloptionwidget.cpp` for the post-stroke correction implementation:

```cpp
// Add Post-Stroke Correction combo box (Off/Low/Med/High)
if (!mPostCorrComboBox) {
    mPostCorrComboBox = new BlenderStyleComboBox(this);
    mPostCorrComboBox->setObjectName("postCorrComboBox");
    mPostCorrComboBox->setOptions({tr("Off"), tr("Low"), tr("Med"), tr("High")});
    mPostCorrComboBox->setCurrentIndex(0);
    mPostCorrComboBox->setAccentColor(BlenderStyleSlider::getAccentColor());
    
    ui->verticalLayout_2->insertWidget(0, mPostCorrComboBox);

    connect(mPostCorrComboBox, &BlenderStyleComboBox::currentIndexChanged, this, [this](int id){
        editor()->tools()->setPostCorrectionLevel(id);
        updateUI();
    });
}
```

**Example 2: Pen Smoothing Mode with Dynamic Disabling** (lines 125-140)

```cpp
// Pen Smoothing: Weighted / Stabiliser
if (!mPenSmoothingComboBox) {
    mPenSmoothingComboBox = new BlenderStyleComboBox(this);
    mPenSmoothingComboBox->setOptions({tr("Weighted"), tr("Stabiliser")});
    mPenSmoothingComboBox->setCurrentIndex(0);
    mPenSmoothingComboBox->setAccentColor(BlenderStyleSlider::getAccentColor());
    
    ui->verticalLayout_2->insertWidget(1, mPenSmoothingComboBox);
}

// When post-correction is enabled, disable Stabiliser option
if (postCorrectionLevel > 0) {
    mPenSmoothingComboBox->setOptionEnabled(1, false); // Disable Stabiliser
    mPenSmoothingComboBox->setCurrentIndex(0); // Force to Weighted
} else {
    mPenSmoothingComboBox->setOptionEnabled(1, true); // Enable Stabiliser
}
```

---

### 2. BlenderStyleValueField

**Location:** `CUI/blenderstylevaluefield.h` and `CUI/blenderstylevaluefield.cpp`

A sophisticated numeric input control combining drag interaction, increment/decrement buttons, and direct text input.

#### Design Features
- **Three interaction methods**:
  1. Drag on center field to adjust like a slider
  2. Single click on center field to type value directly
  3. Click arrow buttons to increment/decrement
- **Unified widget** - Seamless appearance with three functional sections
- **Rounded corners** - Only far left (decrement) and far right (increment) corners rounded
- **Visual feedback**:
  - Hover states per section
  - Accent color tint during drag
  - Button press feedback
- **Label and value display** - Shows property name and current value side by side
- **Keyboard modifiers** - Shift for fine control (10%), Ctrl for coarse (10×)
- **Mouse wheel support** - Scroll to adjust with modifier support

#### Basic Usage

```cpp
#include "../../CUI/blenderstylevaluefield.h"

// Create the widget
auto* rotationField = new BlenderStyleValueField(this);
rotationField->setLabel("Rotation");
rotationField->setRange(0.0, 360.0);
rotationField->setValue(0.0);
rotationField->setDecimals(1);
rotationField->setSingleStep(1.0);

// Connect to changes
connect(rotationField, &BlenderStyleValueField::valueChanged, 
        this, [](double value) {
    qDebug() << "Rotation:" << value;
});

// Add to layout
layout->addWidget(rotationField);
```

#### Real-World Example: Camera Rotation

```cpp
// In ToolOptionWidget::initUI()
mRotationField = new BlenderStyleValueField(this);
mRotationField->setLabel("Rotation");
mRotationField->setRange(0.0, 360.0);
mRotationField->setValue(0.0);
mRotationField->setDecimals(1);
mRotationField->setSingleStep(1.0);
mRotationField->setAccentColor(BlenderStyleSlider::getAccentColor());

connect(mRotationField, &BlenderStyleValueField::valueChanged,
        this, [this](double angle) {
    if (auto* camera = mEditor->currentCamera()) {
        camera->setRotation(angle);
    }
});

ui->verticalLayout_2->addWidget(mRotationField);
```

#### Customization

```cpp
// Set value range and precision
field->setRange(-100.0, 100.0);
field->setDecimals(2);         // Show 2 decimal places
field->setSingleStep(0.5);     // Buttons increment by 0.5

// Styling
field->setAccentColor(QColor(91, 165, 255));
field->setBackgroundColor(QColor(45, 45, 48));
field->setTextColor(QColor(200, 200, 200));
field->setCornerRadius(4);
```

#### Signals

```cpp
// Emitted when value changes via any method (drag, buttons, input, wheel)
void valueChanged(double value);
```

**See `CUI/VALUEFIELD_WIDGET.md` for complete documentation.**

---

### 3. BlenderStyleToggle

**Location:** `CUI/blenderstyletoggle.h` and `CUI/blenderstyletoggle.cpp`

A toggleable button widget that displays a single option that can be turned on/off.

#### Design Features
- **Rounded left and right edges** - Clean button appearance with 4px corner radius
- **Visual states**:
  - Checked: Bright accent color background (default: blue #5BA5FF)
  - Unchecked: Dark background (#323232)
  - Hover: Slightly lighter than background
  - Disabled: Darker, grayed out
- **Optional icon support** - Can include an icon alongside text
- **Dark theme optimized** - Matches Blender aesthetic

#### Basic Usage

```cpp
#include "../../CUI/blenderstyletoggle.h"

// Create the widget
auto* toggle = new BlenderStyleToggle(this);

// Set the text
toggle->setText("Draw Behind");

// Set initial state
toggle->setChecked(false);

// Connect to changes
connect(toggle, &BlenderStyleToggle::toggled, [](bool checked){
    qDebug() << "Toggle is now:" << (checked ? "ON" : "OFF");
});

// Add to layout
layout->addWidget(toggle);
```

#### Customization

```cpp
// Set custom accent color
toggle->setAccentColor(QColor(255, 100, 50));

// Set custom background
toggle->setBackgroundColor(QColor(40, 40, 40));

// Set text colors
toggle->setTextColor(QColor(220, 220, 220));
toggle->setSelectedTextColor(QColor(17, 17, 17));

// Adjust corner radius
toggle->setCornerRadius(6);

// Add an icon
toggle->setIcon(QPixmap(":/icons/paint.png"));
```

#### Getting/Setting State

```cpp
// Get checked state
bool isOn = toggle->isChecked();

// Set checked state (triggers toggled signal)
toggle->setChecked(true);

// Enable/disable
toggle->setEnabled(false);
```

#### Signals

- `toggled(bool checked)` - Emitted when state changes
- `clicked()` - Emitted on every click

#### Real-World Example

See `app/src/tooloptionwidget.cpp` lines 143-152 for the Draw Behind implementation:

```cpp
// Add Draw Behind toggle
if (!mDrawBehindToggle) {
    mDrawBehindToggle = new BlenderStyleToggle(this);
    mDrawBehindToggle->setObjectName("drawBehindToggle");
    mDrawBehindToggle->setText(tr("Draw Behind"));
    mDrawBehindToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
    
    ui->verticalLayout_2->addWidget(mDrawBehindToggle);
}

// Connect to tool manager
connect(mDrawBehindToggle, &BlenderStyleToggle::toggled, 
        toolManager, &ToolManager::setDrawBehind);
```

---

### 3. BlenderStyleSlider

**Location:** `paintlyFillAlgorithm/blenderstyleslider.h` and `blenderstyleslider.cpp`

A custom slider widget with Blender-style interaction patterns.

#### Features
- Infinite drag interaction
- Context menu with reset/copy/paste
- Inline value editing
- Custom accent colors
- Fine control mode (Shift + drag)

---

## Adding New Template Widgets

When creating new Blender-style UI templates:

1. **Create header and implementation files** in the `CUI/` directory
2. **Add to build system** in `app/app.pro`:
   ```
   HEADERS += \
       $$PWD/../CUI/yourwidget.h \
   
   SOURCES += \
       $$PWD/../CUI/yourwidget.cpp \
   ```
3. **Follow the design principles**:
   - Dark theme by default
   - Accent color integration
   - Consistent padding and sizing
   - Proper hover/pressed states
   - Antialiased rendering
   - QWidget base for maximum flexibility

4. **Document your widget** in this README

---

## Design Philosophy

These widgets follow Blender's UI philosophy:

- **Unified visual language** - Consistent look and feel
- **Efficient screen space** - Compact, minimal chrome
- **Clear visual hierarchy** - Selected states are obvious
- **Professional appearance** - Modern, flat design
- **Accessibility** - Good contrast, clear interaction states

---

## Color Palette

Default Blender-style colors used:

| Element | Color | Hex |
|---------|-------|-----|
| Accent (Blue) | ![#5BA5FF](https://via.placeholder.com/15/5BA5FF/000000?text=+) | `#5BA5FF` |
| Background | ![#323232](https://via.placeholder.com/15/323232/000000?text=+) | `#323232` |
| Text | ![#DCDCDC](https://via.placeholder.com/15/DCDCDC/000000?text=+) | `#DCDCDC` |
| Selected Text | ![#111111](https://via.placeholder.com/15/111111/000000?text=+) | `#111111` |
| Separator | ![#1E1E1E](https://via.placeholder.com/15/1E1E1E/000000?text=+) | `#1E1E1E` |

---

## Future Widgets

Completed template widgets:
- [x] **BlenderStyleComboBox** - Segmented control for multiple options
- [x] **BlenderStyleValueField** - Numeric input with drag, buttons, and direct entry
- [x] **BlenderStyleToggle** - Toggleable button for on/off states

Planned template widgets:
- [ ] BlenderStyleButton - Standard push button
- [ ] BlenderStyleCheckBox - Custom checkbox with icon states
- [ ] BlenderStyleTabBar - Horizontal tab control
- [ ] BlenderStylePanel - Collapsible panel/group box
- [ ] BlenderStyleMenu - Custom dropdown menu
- [ ] BlenderStyleTooltip - Enhanced tooltip display

---

## Migration Strategy

To gradually replace legacy UI:

1. Create template widget in `CUI/`
2. Test thoroughly in a single location
3. Document usage patterns
4. Gradually replace old widgets across codebase
5. Update theme/preference system integration
6. Ensure accessibility standards are met

---

Last updated: October 12, 2025

