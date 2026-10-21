# Adjust Pressure Popup Bug Fix

## Issue
The Adjust Pressure popup was showing up empty/blank on subsequent opens, though the interaction lock mechanism was working correctly.

## Root Cause
The problem had multiple components:

1. **Layout Reuse Conflict**: Qt doesn't allow setting a layout on a widget that already has one. The code was trying to create and set a new layout every time the popup opened, but after the first open, the old layout was still attached to the widget.

2. **Incomplete Cleanup**: The original cleanup code was only deleting child widgets by name, but wasn't properly removing and deleting the layout itself.

3. **Signal Accumulation**: Every time the popup opened, new signal connections were being made to `QDialog::finished`, causing duplicate saves and potential memory issues.

4. **No Guards**: There were no safety checks to detect if widgets failed to create or if the layout failed to set properly.

## Solution

### 1. Proper Layout Cleanup
```cpp
// Properly clear existing layout and content
QLayout* existingLayout = mAdjustPressurePopup->layout();
if (existingLayout) {
    // Delete all widgets in the layout
    while (QLayoutItem* item = existingLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    // Delete the layout itself
    delete existingLayout;
}
```

This ensures:
- All layout items are removed
- All child widgets are scheduled for deletion
- The layout object itself is deleted
- The popup is ready for a fresh layout

### 2. Connection Management
```cpp
// Disconnect all previous connections to avoid duplicates
mAdjustPressurePopup->disconnect();
```

This prevents signal accumulation by clearing all existing connections before creating new ones.

### 3. Comprehensive Guards

Added multiple safety checks throughout the process:

**Check 1: Tool Type**
```cpp
if (currentType != PEN) {
    qWarning() << "Adjust pressure toggle activated but not on PEN tool";
    QSignalBlocker b(mAdjustPressureToggle);
    mAdjustPressureToggle->setChecked(false);
    return;
}
```

**Check 2: Popup Existence**
```cpp
if (!mAdjustPressurePopup) {
    qCritical() << "Adjust pressure popup is null!";
    return;
}
```

**Check 3: Layout Cleared**
```cpp
if (mAdjustPressurePopup->layout()) {
    qCritical() << "Failed to clear popup layout!";
    return;
}
```

**Check 4: Widget Creation**
```cpp
if (!enable || !label || !curveEditor) {
    qCritical() << "Failed to create popup widgets!";
    delete contentWidget;
    return;
}
```

**Check 5: Layout Assignment**
```cpp
if (!mAdjustPressurePopup->layout()) {
    qCritical() << "Failed to set popup layout!";
    delete popupLayout;
    delete contentWidget;
    return;
}
```

**Check 6: Size Validation**
```cpp
QSize popupSize = mAdjustPressurePopup->size();
if (popupSize.width() < 50 || popupSize.height() < 50) {
    qWarning() << "Popup size suspiciously small:" << popupSize;
    mAdjustPressurePopup->resize(250, 300);
}
```

### 4. Improved Visibility Management
```cpp
// Make sure content widget is visible
contentWidget->show();

// Show and verify
mAdjustPressurePopup->show();
mAdjustPressurePopup->raise();
mAdjustPressurePopup->activateWindow();
```

Explicitly ensures the content and popup are visible and brought to front.

### 5. Debug Logging
```cpp
qDebug() << "Adjust pressure popup shown successfully at" << gpos << "size:" << mAdjustPressurePopup->size();
```

Provides diagnostic output to help identify issues during runtime.

## Benefits

1. **Robust Cleanup**: Complete cleanup of all resources before creating new content
2. **Memory Safety**: Proper deletion prevents memory leaks
3. **Error Detection**: Guards catch issues early with clear diagnostic messages
4. **No Signal Leaks**: Disconnecting prevents accumulation of duplicate connections
5. **Visual Verification**: Size checks and explicit show commands ensure the popup is visible
6. **Debugging Support**: Debug output helps diagnose issues in production

## Testing Checklist

- [x] Build succeeds without errors
- [x] No linter warnings
- [ ] First popup open shows content correctly
- [ ] Second popup open shows content correctly
- [ ] Multiple open/close cycles work correctly
- [ ] Settings are saved properly when closing
- [ ] Toggle interaction remains available during popup
- [ ] No console errors in debug output
- [ ] Memory doesn't leak over multiple opens

## Files Modified

- `app/src/tooloptionwidget.cpp`: Complete rewrite of the popup content creation logic

## Technical Details

### Widget Hierarchy
```
BlenderStyleModalPopup (popup)
└── QVBoxLayout (popupLayout) [margins: 0,0,0,0]
    └── QWidget (contentWidget) [margins: 16,16,16,16]
        └── QVBoxLayout (contentLayout) [spacing: 8]
            ├── QCheckBox (enable) ["Enable"]
            ├── QLabel (label) ["Pressure curve"]
            └── BezierCurveEditor (curveEditor) [200x200 minimum]
```

### State Management
Uses `std::shared_ptr<PenCurveState>` to track:
- `QPointF p1, p2`: Bezier curve control points
- `bool enabled`: Whether pressure curve is enabled

State is captured in lambda closures and saved when the popup closes.

### Object Names
All widgets have explicit object names for debugging:
- `popupContent`: Main content widget
- `pressureEnableCheck`: Enable checkbox
- `pressureLabel`: Curve label
- `curvEditor`: Bezier curve editor

## Related Components

- `BlenderStyleModalPopup`: The popup widget with glowing border
- `BlenderStyleToggle`: The toggle button that controls visibility
- `BezierCurveEditor`: The curve editing widget
- `PenTool`: Stores and applies the pressure curve settings

