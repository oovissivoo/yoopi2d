# Nurture2D AppImage

Self-contained portable build of Nurture2D for any Linux x86_64 distro — no installation required.

## Running the AppImage

```bash
chmod +x Nurture2D-x86_64.AppImage
./Nurture2D-x86_64.AppImage
```

That's it. No Qt, no dependencies, no root access needed.

## Building the AppImage yourself

### Step 1 — Build the release binary

```bash
./build_release.sh
```

This compiles an optimised release build into `build-release/app/nurture2d`.

### Step 2 — Package into an AppImage

```bash
./create_appimage.sh
```

The script will:
1. Auto-download `linuxdeploy` and `linuxdeploy-plugin-qt` if not present
2. Bundle the binary with all Qt libraries and plugins
3. Output `Nurture2D-x86_64.AppImage`

You can also do both steps in one go:

```bash
./create_appimage.sh          # builds first if binary is missing
```

Or point it at an existing build directory:

```bash
./create_appimage.sh my-build-dir
```

## System requirements

- Linux x86_64
- Kernel 3.2+ (covers Ubuntu 16.04+, Fedora 24+, Arch, Debian 9+, etc.)
- X11 or XWayland

## Troubleshooting

**App won't start — run from terminal to see errors:**
```bash
./Nurture2D-x86_64.AppImage
```

**Extract and inspect contents:**
```bash
./Nurture2D-x86_64.AppImage --appimage-extract
```

**Force software rendering (if GPU issues):**
```bash
LIBGL_ALWAYS_SOFTWARE=1 ./Nurture2D-x86_64.AppImage
```

## License

Nurture2D is licensed under GPL v2. See `LICENSE.TXT`.
