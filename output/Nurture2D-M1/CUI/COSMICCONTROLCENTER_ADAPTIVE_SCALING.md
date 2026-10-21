# CosmicControlCenter Adaptive Scaling

## Overview

The CosmicControlCenter now features intelligent adaptive scaling that ensures all 50 slots remain visible regardless of window width. Instead of clipping slots when the window is resized, the control center dynamically scales down content, spacing, and hit boxes to fit the available space.

## How It Works

### 1. Dynamic Cell Width Calculation

```cpp
int CosmicControlCenter::cellWidth() const
{
    const int w = width();
    if (w > 0) {
        // Always divide by total cells to ensure all slots fit
        return qMax(12, w / CosmicControlCenterLayout::kCells); // 12px absolute minimum
    }
    return CosmicUiMetrics::controlCenterThickness() + 6;
}
```

- **No fixed minimum**: Removed the previous constraint that enforced a minimum cell width based on `controlCenterThickness()`
- **Proportional scaling**: Cell width = `window_width / 50`
- **Absolute minimum**: 12px minimum prevents cells from becoming unusably small
- **Always visible**: All 50 slots will always be visible within the window width

### 2. Adaptive Padding System

The padding and spacing now scale proportionally with cell width:

```cpp
const int outerPad = qMax(1, cw / 10);  // Outer padding: 10% of cell width
const int innerPad = qMax(1, cw / 8);   // Inner padding: 12.5% of cell width
```

**Benefits:**
- Large cells (wide window): More generous spacing for comfortable interaction
- Small cells (narrow window): Minimal padding to maximize icon visibility
- Maintains visual balance at all sizes

### 3. Scaled Corner Radius

```cpp
const int radius = qMax(2, qMin(CosmicUiMetrics::cornerRadius(), cw / 6));
```

- Scales down with cell size to maintain proportions
- Minimum 2px ensures corners remain visible
- Maximum capped at configured corner radius

### 4. Adaptive Font Sizing

For text-based actions (when no icon is available):

```cpp
QFont f = p.font(); 
f.setPointSize(qMax(5, qMin(7, cw / 5))); 
p.setFont(f);
```

- Font size scales between 5pt and 7pt based on cell width
- Ensures text remains readable at smaller sizes

## Scaling Behavior Examples

| Window Width | Cell Width | Outer Pad | Inner Pad | Radius | Result |
|--------------|------------|-----------|-----------|--------|--------|
| 2500px       | 50px       | 5px       | 6px       | 6px    | Spacious, comfortable |
| 1500px       | 30px       | 3px       | 3px       | 5px    | Standard desktop |
| 1000px       | 20px       | 2px       | 2px       | 3px    | Compact but usable |
| 600px        | 12px       | 1px       | 1px       | 2px    | Minimum viable size |

## Key Features

### ✅ Always Visible
All 50 slots remain visible at any window width above the absolute minimum (600px)

### ✅ Proportional Scaling
Icons, padding, and spacing scale together maintaining visual harmony

### ✅ Smart Minimum
12px cell width provides a reasonable lower bound for usability

### ✅ Responsive Hit Boxes
Click targets scale with cell size - larger cells = easier to click

### ✅ Consistent Behavior
Drag-and-drop, hover states, and edit mode all respect the adaptive scaling

## Implementation Details

### Modified Functions

1. **`cellWidth()`** - Removed fixed minimum, uses proportional calculation
2. **`drawCell()`** - Uses adaptive padding and radius calculations
3. **`drawGhost()`** - Drag preview respects scaled dimensions
4. **`paintEvent()`** - Empty cell placeholders use adaptive padding

### Backward Compatibility

- Existing layouts load correctly
- Saved positions remain valid
- No changes to persistence format
- Works with all existing features (groups, widgets, spacers)

## Usage Notes

- **Recommended minimum window width**: 800px for comfortable use
- **Absolute minimum**: 600px (50 cells × 12px)
- **Optimal range**: 1200px - 2000px
- **Maximum**: No upper limit, cells will continue to grow proportionally

## Future Enhancements

Possible improvements for even better responsiveness:

1. **Icon size scaling**: Dynamically adjust icon rendering size based on cell width
2. **Multi-row layout**: Wrap to multiple rows on very narrow windows
3. **Collapsible groups**: Hide less-used groups automatically on narrow windows
4. **Zoom control**: User-adjustable scaling factor independent of window size
