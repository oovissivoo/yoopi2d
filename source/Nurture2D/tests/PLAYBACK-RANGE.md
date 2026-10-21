# Numeric playback range regression

`src/test_playback_range_integration.cpp` opens the real window with temporary
INI settings and a disposable document. It checks visible Start/End fields,
typed input, the marker setter path, inclusive timed playback, crossing bounds,
values above the currently allocated timeline length, save/reopen, switching
documents and disabling/enabling the range. It does not read user settings.

Reuse the application objects after building. From `<build>/application/app`:

```sh
make -f Makefile -f ../../../tests/keyframe_navigation.mk navigation-regression \
  NAVIGATION_SOURCE="$(pwd)/../../../tests/src/test_playback_range_integration.cpp"
QT_QPA_PLATFORM=offscreen DYLD_FRAMEWORK_PATH=/opt/homebrew/opt/qt@5/lib \
  QT_LOGGING_RULES='*.debug=false' ./navigation-regression-bin
```

Expected: `PASS: 26 playback range checks`.
An offscreen preview is saved to `/private/tmp/yoopi2d-playback-range-preview.png`.
