# Lasso Tool Bitmap Selection Fix

## Problem
When using the lasso tool on bitmap layers, there were multiple issues:
1. Clicking and dragging a selection multiple times would keep pasting the selection after each release, causing accumulating/duplicated drawing data
2. Original content remained visible when dragging (not cut)
3. Multiple drags would reset the selection position instead of being cumulative/relative

## Root Cause
1. The `commitFloatingMove()` function was being called on **every mouse release**, causing immediate paste to base layer
2. Selection was extracted with `cut=false` (copy only), leaving original content
3. Each drag calculated offset from the original position instead of the current position

## Solution
Implemented a **temporary overlay buffer** approach:

### Key Changes

1. **Cut source content on drag start** (`lassoselecttool.cpp` line 58-64)
   - Changed `extractFloatingFromPath(false)` to `extractFloatingFromPath(true)` to cut (not copy)
   - Original content is removed from base layer when drag starts
   - Ensures source is cut even if already floating via `ensureSourceCutForMove()`

2. **Cumulative drag offsets** (`lassoselecttool.cpp` line 136)
   - Added `applyFloatingOffset()` call on mouse release
   - Bakes the current drag offset into `floatingOrigin` and selection path
   - Next drag is relative to new position, not original position
   - Allows clicking and dragging from anywhere in the selection

3. **New SelectionManager method** (`selectionmanager.cpp` line 643-659)
   - Added `applyFloatingOffset()` to update origin without committing to base layer
   - Translates selection path and updates origin by current offset
   - Resets offset to zero for next drag

4. **Commit only on finalization** - The floating selection is committed when:
   - **Pressing Enter/Escape** - Handled in `keyPressEvent()` (lines 179-209)
   - **Deselecting** (Ctrl+Shift+D or clicking outside) - Handled in `editor.cpp` `deselectAll()` (lines 852-860)
   - **Switching tools** - Handled in `leavingThisTool()` (lines 155-166)

5. **Updated commit logic** (`selectionmanager.cpp` line 533-627)
   - `commitFloatingMove()` now uses `floatingOrigin` directly (already includes all applied offsets)
   - Clears floating state after commit (selection becomes part of base layer)
   - Updated `needsFloatingCommit()` to check `mWasCutFromSource` instead of offset

6. **Proper undo/redo handling**
   - Undo state is captured **before** committing the floating move
   - Undo is recorded **after** the commit completes
   - This ensures undo works correctly for the entire move operation

7. **Auto-commit before new selection** (`lassoselecttool.cpp` line 74-81)
   - When starting a new lasso selection, check if there's existing floating content
   - If `needsFloatingCommit()` returns true, commit it first with undo recording
   - Prevents data loss when creating a new selection while one is already active

8. **Fixed outline offset sync** (`selectionmanager.cpp` line 655)
   - Reset `mOutlineOffset` to zero instead of accumulating
   - Keeps selection outline synchronized with floating content position
   - Allows clicking and dragging from where you released

9. **Clear selection after tool switch** (`lassoselecttool.cpp` line 176)
   - After committing floating content when leaving the tool, clear the selection
   - User starts with a clean slate when switching to a new tool
   - Prevents lingering selection outlines after tool change

## Benefits

1. **No accumulation bug** - The selection remains isolated in the overlay buffer and doesn't interact with the base layer until finalized
2. **Original content removed** - Source is cut when drag starts, no ghosting
3. **Cumulative drags** - Each drag is relative to current position, can click and drag from anywhere in selection
4. **Selection outline stays synced** - Outline follows the content, can click where you released
5. **No data loss** - Existing selections are auto-committed before creating new ones
6. **Clean tool switching** - Selection is cleared after commit when switching tools
7. **Better UX** - Users can drag the selection multiple times to position it perfectly before committing
8. **Cleaner workflow** - Explicit commit actions (Enter/Escape/deselect/tool switch) make the behavior more predictable
9. **Proper undo** - The entire move operation is recorded as a single undo step

## Files Modified

- `core_lib/src/tool/lassoselecttool.cpp` - Cut source on drag, apply offset on release, commit on finalization
- `core_lib/src/managers/selectionmanager.h` - Added `applyFloatingOffset()` method declaration
- `core_lib/src/managers/selectionmanager.cpp` - Implemented `applyFloatingOffset()`, updated commit logic and `needsFloatingCommit()`
- `core_lib/src/interface/editor.cpp` - Added undo recording when deselecting with pending floating commit

## Testing

To test the fix:
1. Create a bitmap layer with some drawing
2. Use the lasso tool to select a region
3. Ctrl+drag the selection multiple times from different positions
4. Verify that:
   - The original content is removed (cut) when you start dragging
   - The selection doesn't accumulate/duplicate
   - Each drag is relative to the current position (cumulative)
   - You can click and drag from anywhere in the selection
5. Press Enter to commit, or Escape to cancel
6. Verify undo works correctly (entire move is one undo step)
