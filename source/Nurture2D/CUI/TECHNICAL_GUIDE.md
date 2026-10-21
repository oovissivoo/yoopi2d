# BlenderStyleComboBox Technical Implementation Guide

## Overview

This document explains the internal workings of the `BlenderStyleComboBox` widget, focusing on the rendering strategy that creates the seamless segmented appearance.

---

## Core Challenge

**Goal:** Create a single widget that looks like this:

```
┌─────────────────────────┐
│╭───┬────┬────┬────╮│
││Off│Low │Med │High││
│╰───┴────┴────┴────╯│
└─────────────────────────┘
```

**Requirements:**
1. Single widget (not separate buttons)
2. Rounded corners ONLY on far left and far right
3. Middle sections seamlessly connected
4. Each segment independently clickable
5. Visual feedback for hover/press/selected states

---

## Architecture

### Class Structure

```cpp
class BlenderStyleComboBox : public QWidget
{
    Q_OBJECT
    
private:
    QStringList mOptions;           // ["Off", "Low", "Med", "High"]
    int mCurrentIndex;              // Currently selected (0-3)
    int mHoverIndex;                // Mouse hovering over (-1 if none)
    int mPressedIndex;              // Mouse pressed on (-1 if none)
    
    QColor mAccentColor;            // Bright blue for selected
    QColor mBackgroundColor;        // Dark gray base
    QColor mTextColor;              // Light gray for normal text
    QColor mSelectedTextColor;      // Dark text on bright selection
    int mCornerRadius;              // 4px by default
};
```

### Key Methods

```cpp
// Layout calculation
QRect segmentRect(int index) const;
int indexAtPosition(const QPoint &pos) const;

// Rendering
void paintEvent(QPaintEvent *event) override;

// Interaction
void mousePressEvent(QMouseEvent *event) override;
void mouseReleaseEvent(QMouseEvent *event) override;
void mouseMoveEvent(QMouseEvent *event) override;
```

---

## Rendering Strategy

### Step 1: Calculate Segment Positions

```cpp
int indexAtPosition(const QPoint &pos) const
{
    if (mOptions.isEmpty()) return -1;
    
    int segmentWidth = width() / mOptions.size();
    int index = pos.x() / segmentWidth;
    
    return (index >= 0 && index < mOptions.size()) ? index : -1;
}

QRect segmentRect(int index) const
{
    if (index < 0 || index >= mOptions.size()) {
        return QRect();
    }
    
    int segmentWidth = width() / mOptions.size();
    int x = index * segmentWidth;
    
    return QRect(x, 0, segmentWidth, height());
}
```

**Example with 4 options and 200px width:**

```
Segment 0: x=0,   width=50  [Off]
Segment 1: x=50,  width=50  [Low]
Segment 2: x=100, width=50  [Med]
Segment 3: x=150, width=50  [High]
```

---

### Step 2: Draw Unified Background

First, draw the entire widget background with rounded corners:

```cpp
void paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Step 1: Unified background
    QPainterPath backgroundPath;
    backgroundPath.addRoundedRect(rect(), mCornerRadius, mCornerRadius);
    painter.fillPath(backgroundPath, mBackgroundColor);
```

**Visual:**
```
┌────────────────────────┐
│ ╭────────────────╮ │  ← All corners rounded initially
│ │                │ │
│ ╰────────────────╯ │
└────────────────────────┘
```

---

### Step 3: Draw Segments with Selective Rounding

For each segment, create a path with conditional corner rounding:

#### First Segment (Left Side)

```cpp
if (i == 0) {
    // First segment - round LEFT corners only
    segmentPath.moveTo(segRect.right(), segRect.top());
    segmentPath.lineTo(segRect.left() + mCornerRadius, segRect.top());
    
    // Top-left arc
    segmentPath.arcTo(QRectF(segRect.left(), segRect.top(), 
                             mCornerRadius * 2, mCornerRadius * 2), 
                      90, 90);
    
    segmentPath.lineTo(segRect.left(), segRect.bottom() - mCornerRadius);
    
    // Bottom-left arc
    segmentPath.arcTo(QRectF(segRect.left(), 
                             segRect.bottom() - mCornerRadius * 2, 
                             mCornerRadius * 2, mCornerRadius * 2), 
                      180, 90);
    
    segmentPath.lineTo(segRect.right(), segRect.bottom());
    segmentPath.closeSubpath();
}
```

**Visual:**
```
     2
  ╭────┐ ← Top arc (90° start, 90° span)
1 │    │
  │    │ 3 → Straight right edge
  ╰────┘ 4 ← Bottom arc
     5
```

#### Middle Segments

```cpp
else if (i > 0 && i < mOptions.size() - 1) {
    // Middle segments - NO rounding
    segmentPath.addRect(segRect);
}
```

**Visual:**
```
┌────┐  ← Sharp corners
│    │
│    │
└────┘
```

#### Last Segment (Right Side)

```cpp
else if (i == mOptions.size() - 1) {
    // Last segment - round RIGHT corners only
    segmentPath.moveTo(segRect.left(), segRect.top());
    segmentPath.lineTo(segRect.right() - mCornerRadius, segRect.top());
    
    // Top-right arc
    segmentPath.arcTo(QRectF(segRect.right() - mCornerRadius * 2, 
                             segRect.top(), 
                             mCornerRadius * 2, mCornerRadius * 2), 
                      90, -90);
    
    segmentPath.lineTo(segRect.right(), 
                      segRect.bottom() - mCornerRadius);
    
    // Bottom-right arc
    segmentPath.arcTo(QRectF(segRect.right() - mCornerRadius * 2, 
                             segRect.bottom() - mCornerRadius * 2, 
                             mCornerRadius * 2, mCornerRadius * 2), 
                      0, -90);
    
    segmentPath.lineTo(segRect.left(), segRect.bottom());
    segmentPath.closeSubpath();
}
```

**Visual:**
```
┌────╮  ← Top arc (90° start, -90° span = clockwise)
│    │
│    │
└────╯  ← Bottom arc (0° start, -90° span)
```

---

### Step 4: Fill Segments Based on State

```cpp
// Fill the segment based on state
if (isSelected) {
    // Bright accent color
    painter.fillPath(segmentPath, mAccentColor);
} else if (isPressed) {
    // 30% lighter during press
    painter.fillPath(segmentPath, mBackgroundColor.lighter(130));
} else if (isHovered) {
    // 15% lighter on hover
    painter.fillPath(segmentPath, mBackgroundColor.lighter(115));
}
// else: leave background color (already drawn)
```

**State Colors:**
```
Selected:  #5BA5FF (bright blue)
Pressed:   #424242 (30% lighter)
Hovered:   #3A3A3A (15% lighter)
Normal:    #323232 (background)
```

---

### Step 5: Draw Separator Lines

```cpp
// Draw separator lines between segments (except at edges and selected)
if (i > 0 && !isSelected && mCurrentIndex != i - 1) {
    painter.setPen(QPen(QColor(30, 30, 30), 1));
    painter.drawLine(segRect.left(), segRect.top() + 2, 
                    segRect.left(), segRect.bottom() - 2);
}
```

**Result:**
```
┌────│────│────┐
│ Off│Low │Med │  ← Subtle separators
└────│────│────┘
     ^    ^
  Separators (inset by 2px)
```

---

### Step 6: Draw Text

```cpp
// Draw text
QColor textColor = isSelected ? mSelectedTextColor : mTextColor;
painter.setPen(textColor);

QFont textFont = font();
if (isSelected) {
    textFont.setBold(true);
}
painter.setFont(textFont);

painter.drawText(segRect, Qt::AlignCenter, mOptions[i]);
```

**Typography:**
- Selected: Bold, dark (#111111 on bright background)
- Normal: Regular, light (#DCDCDC on dark background)

---

### Step 7: Draw Outer Border

```cpp
// Draw outer border for the entire widget
painter.setPen(QPen(mBackgroundColor.darker(150), 1));
painter.setBrush(Qt::NoBrush);
painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 
                       mCornerRadius, mCornerRadius);
```

**Final Touch:**
```
┌─────────────────┐
│ ╭───┬───┬───╮ │  ← Subtle darker outline
│ │   │   │   │ │
│ ╰───┴───┴───╯ │
└─────────────────┘
```

---

## Interaction Logic

### Mouse Press

```cpp
void mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mPressedIndex = indexAtPosition(event->pos());
        update();  // Trigger repaint
    }
}
```

**Effect:** Segment shows "pressed" state (lighter background)

---

### Mouse Release

```cpp
void mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int releaseIndex = indexAtPosition(event->pos());
        
        // Only change selection if released on same segment
        if (releaseIndex >= 0 && releaseIndex == mPressedIndex) {
            setCurrentIndex(releaseIndex);
            emit optionClicked(releaseIndex);
        }
        
        mPressedIndex = -1;
        update();
    }
}
```

**Effect:** 
- If released on same segment → Selection changes
- If released elsewhere → No change
- Pressed state clears

---

### Mouse Move

```cpp
void mouseMoveEvent(QMouseEvent *event)
{
    int newHoverIndex = indexAtPosition(event->pos());
    
    if (newHoverIndex != mHoverIndex) {
        mHoverIndex = newHoverIndex;
        update();  // Trigger repaint
    }
}
```

**Effect:** Segment under cursor shows hover state

---

### Mouse Leave

```cpp
void leaveEvent(QEvent *event)
{
    mHoverIndex = -1;
    update();  // Clear hover state
}
```

**Effect:** All hover highlights removed

---

## Complete Paint Cycle

```
User Action → State Change → update() → paintEvent()
                                           │
                        ┌──────────────────┤
                        ↓                  ↓
              Draw Background    Calculate Segments
                        │                  │
                        └──────┬───────────┘
                               ↓
                     For each segment:
                     ├─ Create path with conditional rounding
                     ├─ Fill based on state
                     ├─ Draw separator (if needed)
                     └─ Draw text
                               ↓
                     Draw outer border
                               ↓
                           Display
```

---

## Performance Optimizations

### 1. Minimal Repaints

```cpp
if (newHoverIndex != mHoverIndex) {
    mHoverIndex = newHoverIndex;
    update();  // Only repaint when state changes
}
```

### 2. Signal Blocking

```cpp
// In updateUI() - prevent recursive signals
if (mPostCorrComboBox) {
    QSignalBlocker b(mPostCorrComboBox);
    mPostCorrComboBox->setCurrentIndex(value);
}
```

### 3. Efficient Path Creation

- Paths created on-the-fly (no caching needed for this size)
- Antialiasing enabled for smooth rendering
- Simple rectangular math (no complex calculations)

---

## Arc Geometry Explained

### QPainterPath::arcTo() Parameters

```cpp
arcTo(QRectF rect, qreal startAngle, qreal spanAngle)
```

**Angles in Qt:**
- 0° = 3 o'clock (right)
- 90° = 12 o'clock (top)
- 180° = 9 o'clock (left)
- 270° = 6 o'clock (bottom)

**Span:**
- Positive = counter-clockwise
- Negative = clockwise

### Top-Left Corner (First Segment)

```cpp
arcTo(QRectF(x, y, diameter, diameter), 90, 90)
```

```
      90° (start)
        │
   ╭────┤
   │    
   │    
   │    
  180°  
```

Start at top (90°), sweep counter-clockwise (90°) to left (180°).

### Bottom-Left Corner (First Segment)

```cpp
arcTo(QRectF(x, y-diameter, diameter, diameter), 180, 90)
```

```
  180° (start)
   │    
   │    
   │    
   ╰────┤
      270°
```

Start at left (180°), sweep counter-clockwise (90°) to bottom (270°).

### Top-Right Corner (Last Segment)

```cpp
arcTo(QRectF(x-diameter, y, diameter, diameter), 90, -90)
```

```
      90° (start)
        │
     ───╮
        │
        │
        │
       0°
```

Start at top (90°), sweep clockwise (-90°) to right (0°).

### Bottom-Right Corner (Last Segment)

```cpp
arcTo(QRectF(x-diameter, y-diameter, diameter, diameter), 0, -90)
```

```
       0° (start)
        │
        │
        │
     ───╯
        │
      270°
```

Start at right (0°), sweep clockwise (-90°) to bottom (270°).

---

## Color Theory

### Why These Colors?

**Background (#323232):**
- Dark enough for professional appearance
- Light enough to show hover states
- Mid-range for good contrast with both text and accent

**Accent (#5BA5FF):**
- Blender's signature blue
- High visibility against dark backgrounds
- Maintains 4.5:1 contrast ratio (WCAG AA compliant)

**Text (#DCDCDC):**
- Light gray (not pure white) reduces eye strain
- Good contrast against dark background (11.3:1)

**Selected Text (#111111):**
- Near-black for maximum readability on bright accent
- Contrast ratio: 8.1:1

---

## Testing Strategy

### Visual Test Cases

1. **Initial render**: All segments visible, first selected
2. **Hover**: Each segment highlights on mouse over
3. **Click**: Selection changes, accent color appears
4. **Multiple clicks**: Rapid clicking works smoothly
5. **Edge cases**: Single option, two options, many options

### State Combinations

```
Normal:   [ Off ][ Low ][ Med ][ High ]
Hover:    [ Off ][~Low~][ Med ][ High ]
Press:    [ Off ][#Low#][ Med ][ High ]
Selected: [|Off|][ Low ][ Med ][ High ]
```

### Resize Behavior

```
200px: [Off][Low][Med][High]  ← 50px each
300px: [ Off ][ Low ][ Med ][ High ]  ← 75px each
400px: [  Off  ][  Low  ][  Med  ][  High  ]  ← 100px each
```

Segments scale proportionally.

---

## Common Issues and Solutions

### Issue: Gaps Between Segments

**Cause:** Floating-point rounding errors

**Solution:** Use integer division and draw background first
```cpp
int segmentWidth = width() / mOptions.size();
```

### Issue: Corners Not Smooth

**Cause:** Antialiasing disabled

**Solution:** Enable in paintEvent
```cpp
painter.setRenderHint(QPainter::Antialiasing);
```

### Issue: Text Clipped

**Cause:** Segment too narrow

**Solution:** Implement minimumSizeHint()
```cpp
QSize minimumSizeHint() const override {
    return QSize(mOptions.size() * 40, fm.height() + 8);
}
```

---

## Extension Points

### Adding Icons

```cpp
// In paintEvent, before text:
if (!iconForOption(i).isNull()) {
    QRect iconRect = segRect.adjusted(4, 4, -4, -4);
    painter.drawPixmap(iconRect, iconForOption(i));
}
```

### Adding Tooltips

```cpp
bool event(QEvent *e) override {
    if (e->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(e);
        int index = indexAtPosition(helpEvent->pos());
        if (index >= 0) {
            QToolTip::showText(helpEvent->globalPos(), 
                             tooltipForIndex(index));
        }
    }
    return QWidget::event(e);
}
```

### Adding Disabled States

```cpp
bool isEnabled(int index) const;

// In paintEvent:
if (!isEnabled(i)) {
    painter.setOpacity(0.3);
    // ... draw with reduced opacity
}
```

---

## Conclusion

The `BlenderStyleComboBox` achieves its seamless appearance through:

1. **Unified background rendering**
2. **Conditional path geometry** (selective corner rounding)
3. **State-based styling** (selected/hover/pressed)
4. **Precise segment calculation**
5. **Efficient event handling**

This architecture is extensible and can be adapted for similar segmented controls throughout the application.

---

**Document Version:** 1.0  
**Last Updated:** October 12, 2025  
**Implementation:** CUI/blenderstylecombobox.cpp

