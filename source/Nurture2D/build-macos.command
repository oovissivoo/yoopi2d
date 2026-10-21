#!/bin/bash
# Local Apple Silicon build. Run with: /bin/bash build-macos.command
set -Eeuo pipefail

fail() {
    printf '\nОшибка: %s\n' "$*" >&2
    exit 1
}

[[ "$(uname -s)" == Darwin ]] || fail "Этот скрипт нужно запустить на Mac."
if [[ "$(uname -m)" != arm64 ]]; then
    if [[ "$(sysctl -in hw.optional.arm64 2>/dev/null || true)" == 1 ]]; then
        exec /usr/bin/arch -arm64 /bin/bash "$0" "$@"
    fi
    fail "Нужен Mac с Apple Silicon (M1, M2 и новее)."
fi

MACOS_VERSION="$(sw_vers -productVersion)"
[[ "${MACOS_VERSION%%.*}" -ge 15 ]] || fail "Этот вариант рассчитан на macOS 15 Sequoia и новее."
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$ROOT/logs"
LOG="$ROOT/logs/build-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$LOG") 2>&1
trap 'rc=$?; printf "\nСборка остановилась (код %s, строка %s).\nЖурнал: %s\n" "$rc" "$LINENO" "$LOG"; exit "$rc"' ERR

printf 'Yoopi2D — сборка для Apple Silicon\nmacOS: %s\nЖурнал: %s\n\n' "$MACOS_VERSION" "$LOG"
printf 'Будут установлены Qt 5 и FFmpeg через Homebrew, если их ещё нет.\n'
printf 'Приложение будет создано в папке dist рядом с этим скриптом.\n\n'

if ! xcrun --find clang++ >/dev/null 2>&1; then
    xcode-select --install || true
    fail "Завершите установку Command Line Tools в открывшемся окне и запустите этот скрипт повторно."
fi

export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:$PATH"
if [[ ! -x /opt/homebrew/bin/brew ]]; then
    printf 'Установка Homebrew. Установщик может запросить пароль администратора.\n'
    INSTALLER="$ROOT/logs/homebrew-install-$(date +%Y%m%d-%H%M%S).sh"
    /usr/bin/curl --fail --location --retry 3 \
        https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh -o "$INSTALLER"
    /bin/bash "$INSTALLER"
fi
BREW=/opt/homebrew/bin/brew
[[ -x "$BREW" ]] || fail "Homebrew не установлен в /opt/homebrew."
export HOMEBREW_NO_ANALYTICS=1
"$BREW" install qt@5 ffmpeg
QT_PREFIX="$("$BREW" --prefix qt@5)"
QMAKE="$QT_PREFIX/bin/qmake"
[[ -x "$QMAKE" ]] || fail "Не найден qmake из Qt 5."
[[ "$("$QMAKE" -query QT_VERSION)" == 5.* ]] || fail "Для этой версии форка нужен Qt 5."
/usr/bin/lipo "$QMAKE" -verify_arch arm64
"$QMAKE" -query QT_VERSION
"$BREW" --prefix ffmpeg

# Keep all generated files out of the source tree and retain prior builds.
BUILD="$(mktemp -d "$ROOT/build-macos-arm64.XXXXXX")"
JOBS="$(sysctl -n hw.ncpu)"
if ((JOBS > 4)); then JOBS=4; fi
printf '\nРабочая папка: %s\nПараллельных задач: %s\n' "$BUILD" "$JOBS"

# qmake handles framework linking, SDK paths and C++ flags for this helper.
mkdir -p "$BUILD/brush-generator"
(
    cd "$BUILD/brush-generator"
    "$QMAKE" "$ROOT/tools/generate_brush_tips/generate_brush_tips.pro" \
        -spec macx-clang CONFIG+=release CONFIG-=debug_and_release \
        QMAKE_APPLE_DEVICE_ARCHS=arm64 QMAKE_MACOSX_DEPLOYMENT_TARGET=15.0
    /usr/bin/make -j"$JOBS"
    ./generate_brush_tips "$ROOT/BrushTips"
)
for tip in Hardest Hard Medium Soft; do
    [[ -s "$ROOT/BrushTips/CosmicDefaultTip_${tip}.png" ]] || fail "Не создана кисть $tip."
done

mkdir -p "$BUILD/application"
(
    cd "$BUILD/application"
    "$QMAKE" "$ROOT/pencil2d.pro" -spec macx-clang \
        CONFIG+=release CONFIG-=debug_and_release \
        CONFIG+=NO_TESTS CONFIG+=PENCIL2D_RELEASE \
        QMAKE_APPLE_DEVICE_ARCHS=arm64 QMAKE_MACOSX_DEPLOYMENT_TARGET=15.0 \
        VERSION=2026.0.0
    /usr/bin/make -j"$JOBS"
)

BUILT_APP=""
for candidate in "$BUILD/application/app/"*.app; do
    [[ -d "$candidate" ]] || continue
    [[ -z "$BUILT_APP" ]] || fail "Сборка создала несколько приложений; проверьте журнал."
    BUILT_APP="$candidate"
done
[[ -n "$BUILT_APP" ]] || fail "Сборка не создала пакет .app."
EXECUTABLE="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$BUILT_APP/Contents/Info.plist")"
/usr/bin/lipo "$BUILT_APP/Contents/MacOS/$EXECUTABLE" -verify_arch arm64

# Bundle Qt; FFmpeg remains managed by Homebrew on this Mac.
"$QT_PREFIX/bin/macdeployqt" "$BUILT_APP" -always-overwrite
FRAMEWORKS_DIR="$BUILT_APP/Contents/Frameworks"
for dependency in \
    "$("$BREW" --prefix brotli)/lib/libbrotlicommon.1.dylib" \
    "$("$BREW" --prefix webp)/lib/libsharpyuv.0.dylib"; do
    [[ -f "$dependency" ]] || fail "Не найдена библиотека, нужная Qt: $dependency"
    /bin/cp -L "$dependency" "$FRAMEWORKS_DIR/$(basename "$dependency")"
done
/usr/bin/plutil -lint "$BUILT_APP/Contents/Info.plist"
[[ -f "$BUILT_APP/Contents/PlugIns/platforms/libqcocoa.dylib" ]] ||
    fail "В пакет не попал модуль интерфейса macOS (libqcocoa)."
/usr/bin/codesign --force --deep --sign - "$BUILT_APP"
/usr/bin/codesign --verify --deep --strict "$BUILT_APP"

# Exercise Qt initialization and the command-line parser before copying the app.
"$BUILT_APP/Contents/MacOS/$EXECUTABLE" --version
/opt/homebrew/bin/ffmpeg -version
/opt/homebrew/bin/ffprobe -version

mkdir -p "$ROOT/dist"
OUTPUT="$ROOT/dist/Yoopi2D.app"
if [[ -e "$OUTPUT" ]]; then
    OUTPUT="$ROOT/dist/Yoopi2D-$(date +%Y%m%d-%H%M%S).app"
fi
[[ ! -e "$OUTPUT" ]] || fail "Выходной файл уже существует: $OUTPUT"
/usr/bin/ditto "$BUILT_APP" "$OUTPUT"
/usr/bin/codesign --verify --deep --strict "$OUTPUT"
printf '\nСборка готова: %s\nЖурнал: %s\n' "$OUTPUT" "$LOG"
printf 'Перетащите приложение в «Программы», если хотите установить его.\n'
printf 'FFmpeg для импорта/экспорта видео должен оставаться установленным через Homebrew.\n'
if [[ "${YOOPI2D_NO_OPEN:-0}" != 1 ]]; then
    /usr/bin/open -R "$OUTPUT"
    /usr/bin/open "$OUTPUT"
fi
