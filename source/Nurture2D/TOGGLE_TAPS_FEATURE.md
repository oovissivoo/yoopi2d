# Toggle Taps Feature Implementation

## Overview
Added a new "Toggle Taps" preferences page that allows users to control which dock widgets remain visible when using the "Toggle All Docks" feature.

## Changes Made

### New Files Created

1. **`app/src/toggletapspage.h`** - Header file for the Toggle Taps preferences page
2. **`app/src/toggletapspage.cpp`** - Implementation of the Toggle Taps preferences page
3. **`app/ui/toggletapspage.ui`** - UI definition for the Toggle Taps preferences page

### Modified Files

1. **`app/ui/preferencesdialog.ui`**
   - Added "Toggle Taps" item to the preferences navigation list
   - Added ToggleTapsPage widget to the stacked widget
   - Registered ToggleTapsPage as a custom widget

2. **`app/src/preferencesdialog.cpp`**
   - Added initialization for the Toggle Taps page in `init()` method
   - Added accent color update support for Toggle Taps page in `onColorPreferenceChanged()`

3. **`app/src/mainwindow2.cpp`**
   - Modified `toggleDocksVisibility()` to respect Toggle Taps preferences
   - Docks with their toggle enabled (default) will be hidden when "Toggle All Docks" is activated
   - Docks with their toggle disabled will remain visible

4. **`app/app.pro`**
   - Added toggletapspage.h to HEADERS
   - Added toggletapspage.cpp to SOURCES
   - Added toggletapspage.ui to FORMS

## Feature Details

### Toggle Taps Preferences Page

The new preferences page displays a list of all dock widgets in the application using BlenderStyleToggle widgets:

- **TimeLine**
- **Color Box**
- **Color Inspector**
- **ColorPalette**
- **Onion Skin**
- **Shift & Trace**
- **ToolOption**
- **ToolBox**
- **Reference Window**

Each dock has a toggle button that can be enabled or disabled:
- **Enabled (checked)**: The dock will be hidden when "Toggle All Docks" is used
- **Disabled (unchecked)**: The dock will remain visible when "Toggle All Docks" is used

### Settings Storage

Toggle states are stored in QSettings under the "ToggleTaps/" prefix:
- Key format: `ToggleTaps/<DockObjectName>`
- Default value: `true` (enabled - dock will be toggled)
- Example: `ToggleTaps/TimeLine = true`

### User Experience

1. User opens Preferences (Edit → Preferences)
2. Navigates to "Toggle Taps" page
3. Toggles individual docks on/off based on preference
4. Settings are saved automatically when toggled
5. When user presses the "Toggle All Docks" shortcut:
   - Enabled docks will be hidden/shown
   - Disabled docks will remain in their current state

## Technical Implementation

### BlenderStyleToggle Integration

The page uses the existing `BlenderStyleToggle` widget for consistent UI appearance:
- Matches the Blender-style aesthetic of the application
- Supports accent color updates
- Provides visual feedback on hover and click

### Preference Management

Settings are managed using Qt's QSettings API:
- Organization: "Pencil2D"
- Application: "Pencil2D"
- Group: "ToggleTaps"

### Toggle Logic

The `toggleDocksVisibility()` function now:
1. Reads the toggle state for each dock from QSettings
2. Only hides/shows docks that have their toggle enabled
3. Maintains the previous behavior for docks not explicitly disabled

## Testing

The application has been built and tested successfully:
- Build completed without errors
- Application launches correctly
- New preferences page is accessible
- All dock widgets are listed with toggles

## Future Enhancements

Potential improvements:
- Add "Select All" / "Deselect All" buttons
- Group docks by category (e.g., Color, Timeline, Tools)
- Add tooltips explaining each dock's purpose
- Remember per-project toggle settings
