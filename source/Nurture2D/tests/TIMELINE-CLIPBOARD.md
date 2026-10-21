# Timeline drawing copy and paste

Click a numbered drawing in the timeline and press Command+C (Ctrl+C on other
platforms). Click an empty timeline position and press Command+V. Each paste
creates an independent drawing with all its inner layers. Automatic numeric
names and the cell bank order are updated at the insertion point. Existing
timeline positions remain unchanged. An occupied destination is rejected and
logged. Undo/Redo from the timeline restores the insertion and numbering.

`src/test_timeline_clipboard_integration.cpp` uses the real application window,
mouse click handlers and physical Mac shortcut events with temporary settings
and documents. It restores the system clipboard after the run.

From `<build>/application/app`, after building the app:

```sh
make -f Makefile -f ../../../tests/keyframe_navigation.mk navigation-regression \
  NAVIGATION_SOURCE="$(pwd)/../../../tests/src/test_timeline_clipboard_integration.cpp"
QT_QPA_PLATFORM=offscreen DYLD_FRAMEWORK_PATH=/opt/homebrew/opt/qt@5/lib \
  QT_LOGGING_RULES='*.debug=false' ./navigation-regression-bin
```

Expected: `PASS: 179 timeline clipboard checks`.

Coverage: simple/repeated clicks, Command+C/V with Cyrillic logical keys,
content frozen at copy time, complete inner layers, independent cell identities,
fifth/sixth numbering, repeated paste, Undo/Redo, occupied destinations, text
focus, external clipboard changes and save/reopen.

The Undo regression uses the actual Edit-menu QAction and physical Command+Z /
Command+Shift+Z events. Eight cycles cover `1,2 -> 1,2,3 -> 1,2`, canvas and
timeline focus, removal through Editor::removeKey(), Undo removal then Undo
paste, and pixel Undo/Redo without changing structural history.
