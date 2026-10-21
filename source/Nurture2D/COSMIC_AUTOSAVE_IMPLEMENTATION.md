# CosmicAutoSave System — Implementation Summary

## Overview
Replaced the legacy modification-count autosave system with a professional time-based autosave matching Clip Studio Paint and TVPaint behavior.

## What Changed

### 1. Time-Based Trigger (Not Modification Count)
- **Old:** Autosave fired every N undo-stack pushes (default 256, conflicting with pencilsettings.cpp default of 15)
- **New:** `QTimer` fires every N minutes (default 15), only saves if document is dirty
- **Cooldown:** Won't fire again within 60 seconds of the last save (prevents rapid-fire saves during playback/scrubbing)

### 2. New Preference: `AUTO_SAVE_INTERVAL_MINUTES`
- Added to `preferencesdef.h`, `pencildef.h`, `preferencemanager.cpp`
- Default: 15 minutes
- Range: 1–120 minutes
- Persisted to QSettings as `"AutosaveIntervalMinutes"`

### 3. Dirty Tracking
- `Editor::markDocumentDirty()` — called automatically via `UndoRedoManager::didUpdateUndoStack` signal
- `Editor::markDocumentClean()` — called by `MainWindow2` after any successful save (manual or auto)
- `mLastSaveTime` — QDateTime guard for the 60-second cooldown

### 4. CosmicAutoSavePopup (Replaces QMessageBox)
- **Old:** Blocking `QMessageBox` with "Yes", "No", "Never ask again"
- **New:** `CosmicAutoSavePopup` with three buttons:
  - **Save Now** (green, default) → saves immediately
  - **Snooze 5 min** → restarts timer for 5 minutes
  - **Snooze 15 min** → restarts timer for 15 minutes
- No "never ask again" — snooze is the user-friendly alternative
- Matches `CosmicConfirmPopup` visual language (question icon, compact layout, entrance animation)

### 5. Preferences UI Update
- **Old label:** "Number of modifications before autosaving:"
- **New label:** "Autosave interval (minutes):"
- **Old range:** 0–9999 (meaningless)
- **New range:** 1–120 minutes
- Wired to `AUTO_SAVE_INTERVAL_MINUTES` instead of `AUTO_SAVE_NUMBER`

### 6. Removed Legacy Cruft
- `mAutosaveNeverAskAgain` — removed (no longer needed)
- `Editor::dontAskAutoSave()` — stubbed for API compatibility
- `Editor::updateAutoSaveCounter()` — removed (replaced by dirty tracking)
- `mAutosaveCounter` — kept as a member for API compatibility but no longer drives saves

## Files Modified

### Core Library
- `core_lib/src/util/preferencesdef.h` — added `AUTO_SAVE_INTERVAL_MINUTES` enum
- `core_lib/src/util/pencildef.h` — added `SETTING_AUTO_SAVE_INTERVAL_MINUTES` key
- `core_lib/src/managers/preferencemanager.cpp` — default 15 min, persistence
- `core_lib/src/interface/editor.h` — new dirty tracking API, `QTimer* mAutosaveTimer`
- `core_lib/src/interface/editor.cpp` — timer setup, dirty tracking, removed counter logic

### App
- `app/src/cosmicautosavepopup.h` — new popup header
- `app/src/cosmicautosavepopup.cpp` — new popup implementation
- `app/src/mainwindow2.cpp` — replaced `QMessageBox` with `CosmicAutoSavePopup`, added `markDocumentClean()` calls
- `app/src/filespage.cpp` — updated UI label and wiring for interval field
- `app/app.pro` — added `cosmicautosavepopup.{h,cpp}` to build

## Behavior

### Already-Saved Files
- Silent background save every N minutes (if dirty)
- No popup, no interruption

### Transient Workspaces
- Silent snapshot to `autosave.n2d` every N minutes (if dirty)
- No popup, no interruption

### Unsaved Projects
- `CosmicAutoSavePopup` appears when timer fires
- User chooses: Save Now, Snooze 5 min, or Snooze 15 min
- Snooze temporarily adjusts the timer; next `markDocumentClean()` resets to normal interval

### Cooldown
- Timer won't fire again within 60 seconds of the last save
- Prevents rapid-fire saves during playback, scrubbing, or rapid undo/redo

## Testing Checklist
- [ ] Preferences → Files → Autosave interval field shows 15 by default
- [ ] Changing interval restarts the timer immediately
- [ ] Disabling autosave stops the timer
- [ ] Re-enabling autosave starts the timer
- [ ] Dirty flag is set on any undo-stack change
- [ ] Dirty flag is cleared after manual save
- [ ] Dirty flag is cleared after autosave
- [ ] Timer doesn't fire if document is clean
- [ ] Timer doesn't fire within 60 seconds of last save
- [ ] Unsaved project shows `CosmicAutoSavePopup` (not `QMessageBox`)
- [ ] "Save Now" button saves and clears dirty flag
- [ ] "Snooze 5 min" restarts timer for 5 minutes
- [ ] "Snooze 15 min" restarts timer for 15 minutes
- [ ] Header close button = Snooze 15 min
- [ ] Escape key = Snooze 15 min
- [ ] Enter key = Save Now

## Migration Notes
- Existing users with `AutosaveNumber=256` will see the new interval field default to 15 minutes
- The old `AutosaveNumber` setting is still read (for API compatibility) but no longer drives saves
- Translation files (`.ts`) have stale "AutoSave Reminder" / "Never ask again" strings — these will be cleaned up on the next `lupdate` run

## Why This Is Better
1. **Predictable:** User knows exactly when autosave will fire (every N minutes), not "after some number of strokes"
2. **Professional:** Matches industry-standard tools (CSP, TVPaint, Photoshop)
3. **No "never ask again" trap:** Snooze is user-friendly and doesn't permanently disable autosave
4. **Robust:** Cooldown prevents re-entrancy, dirty tracking is automatic via undo stack
5. **Lightweight:** Timer-based, no per-operation overhead
6. **Crash-safe:** No `QApplication::processEvents` in the save loop (removed from autosave path)
7. **Consistent UI:** `CosmicAutoSavePopup` matches the rest of the Cosmic design language

---

**Implementation complete. Ready for testing.**
