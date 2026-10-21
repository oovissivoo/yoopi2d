# Color Sync Fix Test

## Problem Fixed
The new project logic needed to be consistent with application startup behavior - drawing with black color but with the hue wheel positioned at red (hue=0).

## Changes Made

### 1. Fixed `MainWindow2::newObject()` in `app/src/mainwindow2.cpp`
- **Restored**: `mColorBox->setColor(QColor::fromHsv(0, 0, 0))` to set black with red hue
- **Purpose**: Ensures File/New behaves the same as app startup - black drawing color with red hue

### 2. Fixed `ColorPaletteWidget::initUI()` in `app/src/colorpalettewidget.cpp`
- **Restored**: `mEditor->color()->setFrontColor(QColor("#000000"))` to set black front color
- **Purpose**: Maintains consistency with app startup behavior

### 3. Fixed `ColorPaletteWidget::applyDefaultPreset()` and `applyPreset()` in `app/src/colorpalettewidget.cpp`
- **Added**: `mEditor->color()->setFrontColor(QColor("#000000"))` after `setColorNumber(0)`
- **Purpose**: Ensures drawing color is black even after applying palette presets

## How It Works Now

1. When creating a new project (`File > New`):
   - A new Object is created with default layers
   - The ColorPaletteWidget's `updateUI()` is called and applies the default preset (Genga)
   - The ColorBox (color wheel) is set to black with red hue for consistency with app launch
   - The drawing color is black, but the hue wheel is positioned at red (hue=0)
   - This matches the application startup behavior exactly

## Expected Behavior
- New project should start with the Genga preset loaded in the palette dock
- The color wheel should show black color with red hue (hue=0)
- **Drawing with any tool should use black color** (this was the main issue)
- The hue wheel should be positioned at red (hue=0) for consistency with app launch
- Users can select colors from the palette dock, and the color wheel will update accordingly
- The drawing color should be black, not the first color from the palette

## Test Steps
1. Start the application
2. Create a new project (File > New)
3. Check that the color palette dock shows the Genga preset
4. Verify the color wheel shows black color with red hue (hue=0)
5. Draw on the canvas and confirm the color is black
6. Select different colors in the palette and verify the color wheel updates accordingly
7. Verify this behavior matches the application startup behavior
