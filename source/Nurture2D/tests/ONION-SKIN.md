# Проверка Onion Skin

`src/test_onion_skin_integration.cpp` создаёт три независимых чёрных рисунка в разных местах, включает слоты соседних кадров и одновременно выставляет старые настройки Onion Skin. Пользовательские анимации не нужны, настройки теста находятся во временной папке.

На коде до исправления проверка завершается с `FAIL: previous ghost has exactly the configured blue tint and opacity, without a black duplicate`: старый проход добавляет непрозрачный чёрный силуэт под синий.

После исправления ожидается `PASS: 98 onion skin integration checks`.

Проверяются:

- точный цвет и прозрачность предыдущего/следующего рисунка при DPR 1 и 2;
- отсутствие чёрных дублей даже при включённых старых настройках;
- настоящая кнопка лампочки и настоящие QAction Previous/Next;
- повторные переходы между рисунками с выключенной лампочкой;
- совпадение пикселей реального холста после включения и выключения подсветки;
- согласованность лампочки и меню, отдельное отключение направления;
- удержания кадров, отсутствие подсветки во время воспроизведения и сохранность исходных пикселей.

Сборка из `<build>/application/app`, где уже собраны объекты приложения:

```sh
make -f Makefile -f '../../../tests/keyframe_navigation.mk' navigation-regression \
  NAVIGATION_SOURCE='<абсолютный путь к tests/src/test_onion_skin_integration.cpp>'
QT_QPA_PLATFORM=offscreen DYLD_FRAMEWORK_PATH=/opt/homebrew/opt/qt@5/lib \
  QT_LOGGING_RULES='*.debug=false' ./navigation-regression-bin
```

Результат от 2026-10-22: 98 проверок PASS. Логи находятся в `output/Nurture2D-M1/logs/*onion*20261022.log`.
