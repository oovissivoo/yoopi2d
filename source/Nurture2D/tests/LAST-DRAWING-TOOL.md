# Return to the last drawing tool

`src/test_last_drawing_tool.cpp` opens a real window with isolated settings.
It checks return from Eraser, Hand, Eyedropper and Select to each supported
freehand/line drawing tool, the physical B position with a Cyrillic logical key,
and exclusion of text input. It does not alter user preferences or drawings.

Reuse application objects from the application/app build directory:

```sh
make -f Makefile -f ../../../tests/keyframe_navigation.mk navigation-regression \
  NAVIGATION_SOURCE="$(pwd)/../../../tests/src/test_last_drawing_tool.cpp"
QT_QPA_PLATFORM=offscreen DYLD_FRAMEWORK_PATH=/opt/homebrew/opt/qt@5/lib \
  ./navigation-regression-bin
```
