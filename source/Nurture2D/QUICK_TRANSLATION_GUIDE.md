# Quick Translation Guide

## ✅ DONE - What I Just Completed

All 32 language files have been:
- ✅ Updated with latest source strings (1,626 total)
- ✅ Compiled to .qm binary format
- ✅ Copied to app/.qm/ directory
- ✅ Ready to use in your app

## 🔄 To Update Translations in the Future

### When you add new tr() strings to code:

```bash
# 1. Extract new strings
lupdate app/app.pro -ts translations/pencil.ts

# 2. Update all language files
for lang in ar bg ca cs da de el en es et fa fr he hu_HU id it ja kab ko nb nl_NL pl pt pt_BR ru sl sv tr vi yue zh_CN zh_TW; do
    lupdate app/app.pro -ts translations/pencil_${lang}.ts
done

# 3. Compile
lrelease translations/pencil_*.ts

# 4. Copy to app
cp translations/*.qm app/.qm/
```

Or just run: `./update_all_translations.sh`

## 🌍 To Translate a Specific Language

### Using Qt Linguist (GUI):
```bash
linguist translations/pencil_de.ts
```
- Mark strings as "finished" when done
- Save the file
- Compile: `lrelease translations/pencil_de.ts`
- Copy: `cp translations/pencil_de.qm app/.qm/`

### Using a Text Editor:
Open `translations/pencil_XX.ts` and change:
```xml
<translation type="unfinished"></translation>
```
to:
```xml
<translation>Your translation here</translation>
```

Then compile and copy as above.

## 📊 Current Status

**Best Languages** (60%+ complete):
- Cantonese (yue): 85% ⭐
- Japanese (ja): 68% ⭐
- Kabyle (kab): 65%
- German, Italian, Portuguese, Turkish, Vietnamese: 60%

**Needs Most Work** (under 30%):
- Indonesian (id): 22%
- Hungarian (hu_HU): 26%
- Norwegian (nb): 28%
- Dutch (nl_NL): 29%
- Hebrew (he): 31%

## 🚀 Quick Test

1. Rebuild your app
2. Go to Preferences → Language
3. Select a language (try German or Japanese)
4. Restart the app
5. Check if UI elements are translated

## 🤖 Want AI Help?

I can create a script to:
1. Extract all unfinished strings
2. Auto-translate using AI (DeepL/GPT)
3. Import back into .ts files
4. Get all languages to 90%+ quickly

Just ask!

## 📁 Important Files

- `translations/pencil.ts` - Source file (English)
- `translations/pencil_XX.ts` - Translation files (editable)
- `translations/pencil_XX.qm` - Compiled files (binary)
- `app/.qm/pencil_XX.qm` - Runtime files (used by app)
- `qmake_qmake_qm_files.qrc` - Resource file (embeds .qm files)

## 🔧 Troubleshooting

**Translation not showing in app?**
1. Check the .qm file exists: `ls app/.qm/pencil_XX.qm`
2. Rebuild the app to embed new .qm files
3. Check language code matches (e.g., "de" not "de_DE")

**New strings not appearing?**
1. Run `lupdate` to extract them
2. Check they appear in pencil.ts
3. Update language files with `lupdate`

**Compilation errors?**
1. Check .ts file is valid XML
2. Look for unclosed tags
3. Use Qt Linguist to validate

## 📞 Need Help?

Check these files:
- `TRANSLATION_UPDATE_COMPLETE.md` - Full report
- `TRANSLATION_GAP_ANALYSIS.md` - Detailed analysis
- `update_all_translations.sh` - Automation script
