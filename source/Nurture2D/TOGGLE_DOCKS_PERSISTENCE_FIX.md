# Toggle Docks Persistence Bug Fix

## Problem

The "Toggle All Docks" feature had a critical bug where the toggle state was not persistent across application restarts. When users:
1. Hid docks using "Toggle All Docks"
2. Closed the application
3. Reopened the application
4. Tried to toggle docks back (show them)

Nothing would happen because the `mPreviouslyVisibleDocks` list was empty after restart.

## Solution

Implemented persistent storage of the toggle state with an override safeguard mechanism.

### Key Changes

#### 1. Persistent State Storage (`readSettings()` and `writeSettings()`)

**In `readSettings()`:**
- Restore `mDocksHidden` flag from QSettings
- Restore `mPreviouslyVisibleDocks` list by matching dock object names
- This ensures the toggle state survives app restarts

**In `writeSettings()`:**
- Save `mDocksHidden` flag to QSettings
- Save list of previously visible dock names
- Only saves the list when docks are hidden to avoid unnecessary data

#### 2. Immediate State Persistence (`toggleDocksVisibility()`)

Modified to save state immediately when toggling:
- When hiding docks: Save the hidden state and dock list to QSettings with `sync()`
- When showing docks: Clear the saved state from QSettings with `sync()`
- This ensures state is persisted even if app crashes or is force-closed

#### 3. Override Safeguard (`onDockVisibilityChanged()`)

Added a new slot that detects manual dock visibility changes:
- Connected to all dock widgets' `visibilityChanged` signal
- When user manually shows/hides a dock via Windows menu while in "hidden" state:
  - Clears `mDocksHidden` flag
  - Clears `mPreviouslyVisibleDocks` list
  - Removes saved state from QSettings
  - Treats current layout as the new baseline

This prevents the toggle feature from interfering with manual dock management.

## Technical Implementation

### Settings Keys

- `ToggleDocks/Hidden` (bool): Whether docks are currently in hidden state
- `ToggleDocks/PreviouslyVisible` (QStringList): Names of docks that were visible before hiding

### Behavior Flow

#### Normal Toggle Flow:
1. User presses toggle shortcut → Docks hide → State saved
2. App closes → State persisted
3. App reopens → State restored
4. User presses toggle shortcut → Docks show from saved list → State cleared

#### Override Safeguard Flow:
1. User presses toggle shortcut → Docks hide → State saved
2. User manually shows a dock via Windows menu → Override triggered
3. State cleared (current layout becomes new baseline)
4. User presses toggle shortcut → Works normally from current state

## Files Modified

1. **`app/src/mainwindow2.h`**
   - Added `onDockVisibilityChanged(bool)` slot declaration
   - Added `mIgnoreDockVisibilityChanges` flag to prevent signal handling during initialization

2. **`app/src/mainwindow2.cpp`**
   - Modified `readSettings()`: Restore toggle state from QSettings, set ignore flag during restoration
   - Modified `writeSettings()`: Save toggle state to QSettings
   - Modified `toggleDocksVisibility()`: Immediate state persistence with sync(), set ignore flag during toggle
   - Added `onDockVisibilityChanged()`: Override safeguard implementation with ignore flag check
   - Connected dock visibility signals in `createDockWidgets()`

## Bug Fix: Crash on Toggle

### Issue
The application was crashing when using the toggle hide all feature due to `onDockVisibilityChanged()` being triggered during `restoreState()` in `readSettings()`.

### Root Cause
When `restoreState()` is called during app initialization, it changes dock visibility which triggers the `visibilityChanged` signal. The signal handler tried to access and modify QSettings during initialization, causing a segmentation fault.

### Solution
Added `mIgnoreDockVisibilityChanges` flag that is set to `true` during:
1. Settings restoration in `readSettings()`
2. Programmatic dock toggling in `toggleDocksVisibility()`

The `onDockVisibilityChanged()` slot now checks this flag and returns early if it's set, preventing the crash and ensuring the override safeguard only triggers for actual user-initiated changes.

## Testing Scenarios

### Scenario 1: Basic Persistence
1. ✅ Hide docks with toggle
2. ✅ Close app
3. ✅ Reopen app (docks remain hidden)
4. ✅ Toggle to show docks (docks reappear)

### Scenario 2: Override Safeguard
1. ✅ Hide docks with toggle
2. ✅ Manually show a dock via Windows menu
3. ✅ Toggle state is cleared (override triggered)
4. ✅ Current layout becomes new baseline
5. ✅ Toggle works normally from this point

### Scenario 3: Selective Toggle with Toggle Taps
1. ✅ Disable some docks in Toggle Taps preferences
2. ✅ Hide docks with toggle (only enabled docks hide)
3. ✅ Close and reopen app
4. ✅ Toggle to show (only previously hidden docks show)
5. ✅ Disabled docks remain in their state

## Benefits

1. **Persistent State**: Toggle state survives app restarts
2. **User Control**: Manual dock changes override toggle state
3. **Intuitive Behavior**: Current layout always becomes the baseline after manual changes
4. **No Data Loss**: Immediate sync prevents state loss on crashes
5. **Respects Preferences**: Works seamlessly with Toggle Taps settings

## Debug Output

Added debug message when override safeguard triggers:
```
"Dock visibility manually changed - clearing toggle state (override safeguard)"
```

This helps track when the override mechanism activates during testing.
