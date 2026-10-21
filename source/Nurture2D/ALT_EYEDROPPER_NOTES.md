# Alt Eyedropper Temporary Tool – Issue & Fix Notes

## What was going wrong

- Holding or tapping **Alt** on the canvas is supposed to:
  - Temporarily switch to the **Eyedropper** tool.
  - Continuously pick colors while Alt is held.
  - Return to the previous tool when Alt is released.
- In practice, two bad things were happening:
  - **Alt sometimes activated the File menu** (Qt/OS menubar accelerator), stealing focus from the canvas.
  - When Alt was released after the menu stole focus (or during fast tapping), the **key release event never reached the canvas**, so our temporary tool system didn’t see the release and the Eyedropper could get **stuck**.

So the root problem was a mismatch between **Qt’s default Alt menubar behavior** and our **temporary tool system** that expects to see both press and release for the same key.

## High‑level fix

We did three main things:

1. **Made the Eyedropper’s Alt behavior explicitly continuous and temporary.**
   - Added a `mContinuousPick` flag to `EyedropperTool` and a setter.
   - When `mContinuousPick` is true, `pointerMoveEvent` continuously updates the front color as the cursor moves.
   - This flag is used **only** for the Alt-based temporary picker; the normal Eyedropper tool still picks on LMB release.

2. **Hardened the temporary-tool lifecycle so it always cleans up.**
   - `ToolManager::endTemporaryTool()` now:
     - Resets Eyedropper-specific temporary modes: `setSampleOnly(false)` and `setContinuousPick(false)`.
     - Restores the previous tool and emits `toolChanged`.
   - `ScribbleArea::event()` now handles `QEvent::FocusOut` by calling `editor()->tools()->endTemporaryTool()`, so if focus leaves the canvas (e.g. menu grabs focus), any temp tool is force‑cancelled.

3. **Stopped Qt from treating plain Alt as a menubar accelerator.**
   - In `MainWindow2::eventFilter`, we intercept `QEvent::ShortcutOverride` for **plain Alt** and `accept()` it.
   - This prevents Qt from toggling menubar focus on Alt, while still allowing the actual `KeyPress` / `KeyRelease` events for Alt to reach the canvas.

Together, these changes:
- Prevent the File menu from reacting to Alt while the app is focused.
- Ensure the temp eyedropper cannot remain stuck after Alt is released or focus changes.
- Provide a clean, continuous Alt-hold color picker that is logically separate from the normal Eyedropper tool.

## Key implementation details

### 1. Eyedropper continuous pick

**File:** `core_lib/src/tool/eyedroppertool.h`

- Added a flag and setter:
  - `bool mContinuousPick = false;`
  - `void setContinuousPick(bool enabled) { mContinuousPick = enabled; }`

**File:** `core_lib/src/tool/eyedroppertool.cpp`

- In `EyedropperTool::pointerMoveEvent`:
  - When `mContinuousPick` is true and the sampled color is valid, we call `mEditor->color()->setFrontColor(...)` on every move.
  - Cursor color preview behavior is unchanged.
  - If `mContinuousPick` is false, behavior is the same as the original tool (no continuous update, only preview).

Result: the Alt-based picker continuously changes the active color as you move, while the normal Eyedropper (selected as a tool) still picks on LMB release only.

### 2. ToolManager temporary tool cleanup

**File:** `core_lib/src/managers/toolmanager.h`

- Added:
  - `void setEyedropperContinuousPick(bool enabled);`

**File:** `core_lib/src/managers/toolmanager.cpp`

- `ToolManager::setEyedropperContinuousPick(bool enabled)` forwards to `EyedropperTool::setContinuousPick`.
- `ToolManager::endTemporaryTool()` now checks if the temp tool is `EYEDROPPER` and, if so, resets:
  - `dropper->setSampleOnly(false);`
  - `dropper->setContinuousPick(false);`

This guarantees that any eyedropper temp session ends in a clean state, even if something odd happens during the interaction.

### 3. ScribbleArea Alt temp- eyedropper handling

**File:** `core_lib/src/interface/scribblearea.cpp`

- When the `CMD_TEMP_TOOL_EYEDROPPER` shortcut matches and `event->key() == Qt::Key_Alt`:
  - We enable continuous pick and disable sample-only for the duration of the temp tool:
    - `editor()->tools()->setEyedropperSampleOnly(false);`
    - `editor()->tools()->setEyedropperContinuousPick(true);`
  - We call `beginTemporaryTool(EYEDROPPER, event->key())` to enter the temp tool.
  - We `event->accept()` to prevent further propagation.
- For non‑Alt temp eyedropper shortcuts (if any are configured), we keep the original sample-only behavior.
- In `ScribbleArea::event`, we added a `QEvent::FocusOut` handler that calls `endTemporaryTool()` to cancel any active temp tool when the canvas loses focus.

### 4. Alt menubar accelerator suppression

**File:** `app/src/mainwindow2.cpp`

- `MainWindow2` already installs a global event filter:
  - `qApp->installEventFilter(this);`
- In `MainWindow2::eventFilter(QObject* obj, QEvent* event)` we added:

```cpp
// Block Qt's default "Alt focuses menubar" accelerator at the shortcut
// override stage, so the File menu doesn't react to plain Alt. Normal
// key events (KeyPress/KeyRelease) still reach ScribbleArea and other
// widgets, preserving the Alt-based canvas eyedropper behavior.
if (event->type() == QEvent::ShortcutOverride) {
    if (auto* ke = dynamic_cast<QKeyEvent*>(event)) {
        const bool isPlainAlt = (ke->key() == Qt::Key_Alt) &&
                                (ke->modifiers() == Qt::AltModifier);
        if (isPlainAlt) {
            // Accept the shortcut override so Qt does not treat plain Alt
            // as a menubar accelerator, but do not filter out the event.
            // This allows the subsequent KeyPress/KeyRelease events for
            // Alt to still reach widgets like ScribbleArea.
            event->accept();
        }
    }
}
```

This is the key piece that stops the File menu from lighting up due to Qt’s default accelerator, while still letting our canvas receive Alt press/release events.

## Behavioral summary

After these changes:

- **Alt-hold on canvas**
  - Temporarily switches to Eyedropper.
  - Continuously updates the front color under the cursor while held.
  - Returns cleanly to the previous tool on Alt release, even after rapid taps.

- **Normal Eyedropper tool**
  - Unchanged: user must click (LMB release) to pick a color.

- **Menu bar / File menu**
  - Plain Alt no longer toggles or highlights the File menu while Nurture2D is focused.
  - If focus does leave the canvas for any reason, `FocusOut` will still force-end any active temporary tool as a safety net.

## If a similar issue appears in the future

1. **Symptoms to look for**
   - Temp tools (especially Eyedropper) staying active after you release a key.
   - Menus reacting to Alt or other modifiers when they shouldn’t.

2. **First checks**
   - Confirm that `KeyPress` and `KeyRelease` for the modifier are reaching `ScribbleArea::keyPressEvent` / `keyReleaseEvent`.
   - Check whether `eventFilter` (on `MainWindow2` or elsewhere) is consuming those events.
   - Verify `ToolManager::beginTemporaryTool` / `endTemporaryTool` and `shouldEndTemporaryTool` logic.

3. **General approach**
   - Prefer handling menu accelerator suppression at the **`QEvent::ShortcutOverride`** stage, not by blocking raw `KeyPress`/`KeyRelease`.
   - Keep temporary-tool cleanup robust: always reset any per-tool temporary flags and provide a focus-loss escape hatch (like `FocusOut`).
   - Keep the core tool behavior (e.g. normal Eyedropper) and the temporary modifier-based behavior **decoupled** via explicit runtime flags, not global settings.
