#!/bin/bash
# Build a portable AppImage for Nurture2D
# Usage: ./create_appimage.sh [build_dir]
set -e

ARCH="$(uname -m)"
BUILD_DIR="${1:-build-release}"
BINARY="$BUILD_DIR/app/nurture2d"
APPDIR="Nurture2D.AppDir"
OUTPUT="Nurture2D-$ARCH.AppImage"

QT_LIBS="/usr/lib/x86_64-linux-gnu"
QT_PLUGINS="$QT_LIBS/qt5/plugins"

# ── 1. Build if needed ────────────────────────────────────────────────────────
if [ ! -f "$BINARY" ]; then
    echo "==> Binary not found — building release first..."
    bash build_release.sh "$BUILD_DIR"
fi
echo "==> Binary: $BINARY ($(du -h "$BINARY" | cut -f1))"

# ── 2. Download tools ─────────────────────────────────────────────────────────
LINUXDEPLOY="./linuxdeploy-$ARCH.AppImage"
APPIMAGETOOL="./appimagetool-$ARCH.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "==> Downloading linuxdeploy..."
    wget -q --show-progress "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage" -O "$LINUXDEPLOY"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$APPIMAGETOOL" ]; then
    echo "==> Downloading appimagetool..."
    wget -q --show-progress "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-$ARCH.AppImage" -O "$APPIMAGETOOL"
    chmod +x "$APPIMAGETOOL"
fi

# ── 3. Create AppDir structure ────────────────────────────────────────────────
echo "==> Building AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/plugins/platforms"
mkdir -p "$APPDIR/usr/plugins/imageformats"
mkdir -p "$APPDIR/usr/plugins/iconengines"
mkdir -p "$APPDIR/usr/plugins/platforminputcontexts"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# ── 4. Copy binary ────────────────────────────────────────────────────────────
cp "$BINARY" "$APPDIR/usr/bin/nurture2d"

# ── 5. Copy Qt libraries ──────────────────────────────────────────────────────
echo "==> Copying Qt libs..."
for lib in \
    libQt5Core.so.5 libQt5Gui.so.5 libQt5Widgets.so.5 \
    libQt5Xml.so.5 libQt5Svg.so.5 libQt5Network.so.5 \
    libQt5Multimedia.so.5 libQt5MultimediaWidgets.so.5 \
    libQt5Concurrent.so.5 libQt5DBus.so.5 libQt5XcbQpa.so.5 \
    libQt5PrintSupport.so.5; do
    src=$(find "$QT_LIBS" /lib/x86_64-linux-gnu -name "$lib" 2>/dev/null | head -1)
    [ -n "$src" ] && cp -L "$src" "$APPDIR/usr/lib/" && echo "  $lib"
done

# ── 6. Copy Qt plugins ────────────────────────────────────────────────────────
echo "==> Copying Qt plugins..."
# Platform: xcb only (wayland optional)
cp -L "$QT_PLUGINS/platforms/libqxcb.so" "$APPDIR/usr/plugins/platforms/"
[ -f "$QT_PLUGINS/platforms/libqwayland-generic.so" ] && \
    cp -L "$QT_PLUGINS/platforms/libqwayland-generic.so" "$APPDIR/usr/plugins/platforms/" 2>/dev/null || true

# Image formats (skip libdsvg.so — broken Debian stub)
for plugin in libqgif.so libqico.so libqjpeg.so libqpng.so libqsvg.so libqtiff.so libqwebp.so; do
    src="$QT_PLUGINS/imageformats/$plugin"
    [ -f "$src" ] && cp -L "$src" "$APPDIR/usr/plugins/imageformats/" && echo "  $plugin"
done

# Icon engines
[ -f "$QT_PLUGINS/iconengines/libqsvgicon.so" ] && \
    cp -L "$QT_PLUGINS/iconengines/libqsvgicon.so" "$APPDIR/usr/plugins/iconengines/"

# Input contexts
for plugin in "$QT_PLUGINS/platforminputcontexts/"*.so; do
    [ -f "$plugin" ] && cp -L "$plugin" "$APPDIR/usr/plugins/platforminputcontexts/" 2>/dev/null || true
done

# ── 7. Bundle all binary dependencies via linuxdeploy ────────────────────────
echo "==> Resolving shared library dependencies..."
"$LINUXDEPLOY" --appdir "$APPDIR" --executable "$BINARY" 2>&1 | grep -E "Deploying shared|ERROR" || true

# Also resolve deps for all bundled plugins
for f in "$APPDIR/usr/plugins"/**/*.so "$APPDIR/usr/lib"/*.so*; do
    [ -f "$f" ] || continue
    ldd "$f" 2>/dev/null | awk '/=> \// {print $3}' | while read dep; do
        name=$(basename "$dep")
        # Skip system/blacklisted libs
        case "$name" in
            libc.so*|libm.so*|libpthread*|libdl.so*|librt.so*|libstdc++*|libgcc_s*) continue ;;
            libGL.so*|libGLX*|libGLdispatch*|libEGL*) continue ;;
            libX11*|libxcb.so*|libfontconfig*|libfreetype*|libexpat*) continue ;;
            libz.so*|libuuid*|libSM*|libICE*|libharfbuzz*) continue ;;
        esac
        dest="$APPDIR/usr/lib/$name"
        [ ! -f "$dest" ] && [ -f "$dep" ] && cp -L "$dep" "$dest" 2>/dev/null || true
    done
done

# ── 8. Desktop file and icon ──────────────────────────────────────────────────
cp "app/data/org.nurture2d.Nurture2D.desktop" "$APPDIR/usr/share/applications/nurture2d.desktop"
cp "app/data/icons/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/nurture2d.png"
# AppImage root symlinks
cp "app/data/icons/icon.png" "$APPDIR/nurture2d.png"
cp "app/data/org.nurture2d.Nurture2D.desktop" "$APPDIR/nurture2d.desktop"

# ── 9. AppRun ─────────────────────────────────────────────────────────────────
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export PATH="$HERE/usr/bin:$PATH"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/usr/plugins/platforms"
# Prefer xcb; fall back to offscreen if no display
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/usr/bin/nurture2d" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# ── 10. Pack into AppImage ────────────────────────────────────────────────────
echo "==> Packing AppImage..."
ARCH="$ARCH" "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo ""
echo "==> Done: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
echo "    chmod +x $OUTPUT && ./$OUTPUT"
