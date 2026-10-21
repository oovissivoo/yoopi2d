# Bug Fix: Mouse Interaction Broken in Reference Window, Gallery, and Brush Preset Settings

## Issue

When `CosmicControlCenter` was added to `mainwindow2.cpp`, a deferred `QTimer::singleShot` scan was introduced to neutralize phantom Qt-internal toolbar-area widgets that overlap the custom `CosmicMenuBar`. The scan used `findChildren<QWidget*>()`, which recursively walks the entire widget tree.

At the time the timer fires (next event loop tick after construction), many widgets haven't been laid out yet and sit at position `(0, 0)` — which overlaps the menu bar rect at the top of the screen. Any unnamed widget at that position got permanently stamped with `WA_TransparentForMouseEvents = true`. The same logic existed in the global `eventFilter` on every `MouseButtonPress`.

`ReferenceCanvas`, scroll area viewports, the `CanvasGalleryOverlay`, and other deep unnamed widgets were all affected.

## What It Broke

- Reference window: clicking and dragging to pan an image worked once, then all subsequent mouse interaction (panning, color picking via Alt) was dead.
- Gallery overlay: close button, resize, and all interaction except window dragging were non-functional.
- Brush preset settings window: same — close button and resize non-functional (separate but related cause, see below).

## Fix

**`app/src/mainwindow2.cpp`** — Both the deferred scan and the `eventFilter` check were restricted to **direct children of `this`** only (`w->parent() == this` / iterating `children()` instead of `findChildren`). Qt's phantom toolbar-area widgets are always direct children of `QMainWindow`, never deep descendants, so the fix is precise.

**`app/src/brushpresetlistwidget.cpp` / `.h`** — The `BrushPresetSettingsPopup` was parented to the embedded `BrushPresetListWidget` panel instead of the top-level window. A `QDialog` parented to an embedded widget is constrained to that widget's geometry, breaking close button clicks, resize hit-testing, and all non-drag interaction. Fixed by lazy-initializing the popup via `ensureSettingsPopup()`, which parents it to `window()` at first use.
