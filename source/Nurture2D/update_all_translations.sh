#!/bin/bash
# Script to update all translation files with latest source strings

set -e

echo "=== Nurture2D Translation Update Script ==="
echo ""

# Check if we're in the right directory
if [ ! -f "pencil2d.pro" ]; then
    echo "ERROR: Must run from Nurture2D root directory"
    exit 1
fi

# Check if lupdate is available
if ! command -v lupdate &> /dev/null; then
    echo "ERROR: lupdate not found. Install Qt development tools."
    exit 1
fi

echo "Step 1: Updating source translation file (pencil.ts)..."
lupdate app/app.pro -ts translations/pencil.ts
echo "✓ Source file updated"
echo ""

echo "Step 2: Updating all language files..."
LANGUAGES=(
    ar bg ca cs da de el en es et fa fr he hu_HU id it ja kab ko nb nl_NL pl pt pt_BR ru sl sv tr vi yue zh_CN zh_TW
)

for lang in "${LANGUAGES[@]}"; do
    echo "  Updating pencil_${lang}.ts..."
    lupdate app/app.pro -ts translations/pencil_${lang}.ts
done
echo "✓ All language files updated"
echo ""

echo "Step 3: Compiling translations to .qm files..."
if ! command -v lrelease &> /dev/null; then
    echo "WARNING: lrelease not found. Skipping compilation."
    echo "Install Qt development tools and run: lrelease translations/pencil_*.ts"
else
    lrelease translations/pencil_*.ts
    echo "✓ Translations compiled"
fi
echo ""

echo "Step 4: Copying .qm files to app/.qm/ directory..."
mkdir -p app/.qm
cp translations/*.qm app/.qm/
echo "✓ Files copied"
echo ""

echo "=== Summary ==="
echo "Translation files have been updated with the latest source strings."
echo ""
echo "Next steps:"
echo "1. Open Qt Linguist to translate unfinished strings:"
echo "   linguist translations/pencil_de.ts"
echo ""
echo "2. Or push to Transifex for community translation:"
echo "   tx push -s -t"
echo ""
echo "3. Check translation status:"
for lang in "${LANGUAGES[@]}"; do
    unfinished=$(grep -c 'type="unfinished"' translations/pencil_${lang}.ts 2>/dev/null || echo "0")
    total=$(grep -c '<message>' translations/pencil_${lang}.ts 2>/dev/null || echo "0")
    if [ "$total" -gt 0 ]; then
        percent=$((100 - (unfinished * 100 / total)))
        printf "   %-10s: %3d%% complete (%d/%d translated)\n" "$lang" "$percent" "$((total - unfinished))" "$total"
    fi
done
