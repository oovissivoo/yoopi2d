#!/usr/bin/env python3
"""
Auto-translate unfinished strings in Qt .ts files using AI translation.
This script uses multiple translation backends with fallbacks.
"""

import xml.etree.ElementTree as ET
import sys
import os
import time
from pathlib import Path
import re

# Language code mapping for translation services
LANG_MAP = {
    'ar': 'ar',      # Arabic
    'bg': 'bg',      # Bulgarian
    'ca': 'ca',      # Catalan
    'cs': 'cs',      # Czech
    'da': 'da',      # Danish
    'de': 'de',      # German
    'el': 'el',      # Greek
    'en': 'en',      # English
    'es': 'es',      # Spanish
    'et': 'et',      # Estonian
    'fa': 'fa',      # Persian/Farsi
    'fr': 'fr',      # French
    'he': 'he',      # Hebrew
    'hu_HU': 'hu',   # Hungarian
    'id': 'id',      # Indonesian
    'it': 'it',      # Italian
    'ja': 'ja',      # Japanese
    'kab': 'kab',    # Kabyle (may not be supported)
    'ko': 'ko',      # Korean
    'nb': 'no',      # Norwegian Bokmål
    'nl_NL': 'nl',   # Dutch
    'pl': 'pl',      # Polish
    'pt': 'pt',      # Portuguese
    'pt_BR': 'pt',   # Portuguese (Brazil)
    'ru': 'ru',      # Russian
    'sl': 'sl',      # Slovenian
    'sv': 'sv',      # Swedish
    'tr': 'tr',      # Turkish
    'vi': 'vi',      # Vietnamese
    'yue': 'zh-TW',  # Cantonese (use Traditional Chinese)
    'zh_CN': 'zh-CN',# Chinese Simplified
    'zh_TW': 'zh-TW',# Chinese Traditional
}

def translate_text(text, target_lang, source_lang='en'):
    """
    Translate text using available translation service.
    Falls back through multiple methods.
    """
    # Method 1: Try googletrans (free, no API key)
    try:
        from googletrans import Translator
        translator = Translator()
        result = translator.translate(text, src=source_lang, dest=target_lang)
        if result and result.text:
            return result.text
    except Exception as e:
        pass
    
    # Method 2: Try deep-translator (free, no API key)
    try:
        from deep_translator import GoogleTranslator
        translator = GoogleTranslator(source=source_lang, target=target_lang)
        result = translator.translate(text)
        if result:
            return result
    except Exception as e:
        pass
    
    # Method 3: Try translate library (free, no API key)
    try:
        from translate import Translator as TranslateTranslator
        translator = TranslateTranslator(from_lang=source_lang, to_lang=target_lang)
        result = translator.translate(text)
        if result:
            return result
    except Exception as e:
        pass
    
    # If all methods fail, return original text
    return None

def should_translate(text):
    """Check if text should be translated (skip special cases)."""
    if not text or not text.strip():
        return False
    
    # Skip if it's just numbers or symbols
    if re.match(r'^[\d\s\.\,\-\+\*\/\(\)\[\]\{\}]+$', text):
        return False
    
    # Skip if it's a format string placeholder only
    if re.match(r'^%\d+$', text):
        return False
    
    # Skip very short technical strings
    if len(text) <= 2 and text.isupper():
        return False
    
    return True

def preserve_qt_formatting(original, translated):
    """
    Preserve Qt-specific formatting in translated text.
    Handles %1, %2, &shortcuts, HTML tags, etc.
    """
    if not translated:
        return original
    
    # Find all Qt placeholders (%1, %2, etc.)
    placeholders = re.findall(r'%\d+', original)
    
    # Find all keyboard shortcuts (&X)
    shortcuts = re.findall(r'&\w', original)
    
    # Find HTML tags
    html_tags = re.findall(r'<[^>]+>', original)
    
    result = translated
    
    # Ensure placeholders are preserved
    for placeholder in placeholders:
        if placeholder not in result:
            # Try to add it back at the end
            result = result + ' ' + placeholder
    
    # Preserve shortcuts (usually at start of words)
    for shortcut in shortcuts:
        if shortcut not in result:
            # Try to add to first letter of first word
            words = result.split()
            if words:
                words[0] = shortcut[0] + words[0]
                result = ' '.join(words)
    
    # Preserve HTML tags
    for tag in html_tags:
        if tag not in result:
            result = tag + result
    
    return result

def translate_ts_file(ts_file, lang_code, dry_run=False):
    """
    Translate unfinished strings in a Qt .ts file.
    """
    print(f"\n{'='*60}")
    print(f"Processing: {ts_file}")
    print(f"Language: {lang_code} -> {LANG_MAP.get(lang_code, lang_code)}")
    print(f"{'='*60}")
    
    # Parse XML
    try:
        tree = ET.parse(ts_file)
        root = tree.getroot()
    except Exception as e:
        print(f"ERROR: Failed to parse {ts_file}: {e}")
        return 0, 0
    
    target_lang = LANG_MAP.get(lang_code, lang_code)
    translated_count = 0
    skipped_count = 0
    total_unfinished = 0
    
    # Find all unfinished translations
    for context in root.findall('.//context'):
        context_name = context.find('name').text if context.find('name') is not None else 'Unknown'
        
        for message in context.findall('message'):
            source = message.find('source')
            translation = message.find('translation')
            
            if source is None or translation is None:
                continue
            
            source_text = source.text
            translation_type = translation.get('type', '')
            translation_text = translation.text
            
            # Check if unfinished
            if translation_type == 'unfinished' or not translation_text:
                total_unfinished += 1
                
                if not should_translate(source_text):
                    skipped_count += 1
                    continue
                
                # Translate
                print(f"  [{context_name}] Translating: {source_text[:60]}...")
                
                if not dry_run:
                    translated = translate_text(source_text, target_lang, 'en')
                    
                    if translated and translated != source_text:
                        # Preserve Qt formatting
                        translated = preserve_qt_formatting(source_text, translated)
                        
                        # Update XML
                        translation.text = translated
                        if 'type' in translation.attrib:
                            del translation.attrib['type']
                        
                        translated_count += 1
                        print(f"    ✓ {translated[:60]}")
                        
                        # Rate limiting to avoid API blocks
                        time.sleep(0.1)
                    else:
                        print(f"    ✗ Translation failed, keeping original")
                        skipped_count += 1
                else:
                    print(f"    [DRY RUN] Would translate")
    
    # Save updated file
    if not dry_run and translated_count > 0:
        try:
            # Preserve XML formatting
            ET.indent(tree, space='    ')
            tree.write(ts_file, encoding='utf-8', xml_declaration=True)
            print(f"\n✓ Saved {ts_file}")
        except Exception as e:
            print(f"\n✗ Failed to save {ts_file}: {e}")
    
    print(f"\nResults:")
    print(f"  Total unfinished: {total_unfinished}")
    print(f"  Translated: {translated_count}")
    print(f"  Skipped: {skipped_count}")
    
    return translated_count, total_unfinished

def install_dependencies():
    """Install required translation libraries."""
    print("Installing translation libraries...")
    print("This may take a moment...\n")
    
    libraries = [
        'googletrans==4.0.0rc1',
        'deep-translator',
        'translate',
    ]
    
    for lib in libraries:
        try:
            print(f"Installing {lib}...")
            os.system(f"{sys.executable} -m pip install -q {lib}")
        except:
            pass
    
    print("\n✓ Dependencies installed\n")

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Auto-translate Qt .ts files')
    parser.add_argument('files', nargs='*', help='Translation files to process')
    parser.add_argument('--all', action='store_true', help='Process all translation files')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be translated without making changes')
    parser.add_argument('--install-deps', action='store_true', help='Install required dependencies')
    parser.add_argument('--lang', help='Process specific language code (e.g., de, fr, es)')
    
    args = parser.parse_args()
    
    if args.install_deps:
        install_dependencies()
        return
    
    # Find translation files
    ts_files = []
    
    if args.all:
        translations_dir = Path('translations')
        if translations_dir.exists():
            ts_files = list(translations_dir.glob('pencil_*.ts'))
    elif args.lang:
        ts_file = Path(f'translations/pencil_{args.lang}.ts')
        if ts_file.exists():
            ts_files = [ts_file]
        else:
            print(f"ERROR: File not found: {ts_file}")
            return
    elif args.files:
        ts_files = [Path(f) for f in args.files]
    else:
        parser.print_help()
        return
    
    if not ts_files:
        print("ERROR: No translation files found")
        return
    
    print(f"\n{'='*60}")
    print(f"Auto-Translation Tool for Nurture2D")
    print(f"{'='*60}")
    print(f"Files to process: {len(ts_files)}")
    if args.dry_run:
        print("MODE: DRY RUN (no changes will be made)")
    print()
    
    # Check if translation libraries are available
    try:
        import googletrans
    except ImportError:
        print("⚠️  Translation libraries not found!")
        print("Run: python3 auto_translate.py --install-deps")
        print()
        response = input("Install now? (y/n): ")
        if response.lower() == 'y':
            install_dependencies()
        else:
            return
    
    total_translated = 0
    total_unfinished = 0
    
    # Process each file
    for ts_file in sorted(ts_files):
        # Extract language code from filename
        lang_code = ts_file.stem.replace('pencil_', '')
        
        # Skip if already well-translated (>85%)
        try:
            tree = ET.parse(ts_file)
            root = tree.getroot()
            unfinished = len(root.findall('.//translation[@type="unfinished"]'))
            total_msgs = len(root.findall('.//message'))
            
            if total_msgs > 0:
                completion = ((total_msgs - unfinished) / total_msgs) * 100
                if completion > 85:
                    print(f"\n✓ Skipping {lang_code} - already {completion:.1f}% complete")
                    continue
        except:
            pass
        
        translated, unfinished = translate_ts_file(str(ts_file), lang_code, args.dry_run)
        total_translated += translated
        total_unfinished += unfinished
    
    print(f"\n{'='*60}")
    print(f"SUMMARY")
    print(f"{'='*60}")
    print(f"Total strings translated: {total_translated}")
    print(f"Total unfinished strings: {total_unfinished}")
    
    if not args.dry_run and total_translated > 0:
        print(f"\n✓ Translation complete!")
        print(f"\nNext steps:")
        print(f"1. Review translations in Qt Linguist (optional)")
        print(f"2. Compile: lrelease translations/pencil_*.ts")
        print(f"3. Copy: cp translations/*.qm app/.qm/")
        print(f"4. Rebuild your app")
    elif args.dry_run:
        print(f"\nThis was a dry run. Run without --dry-run to apply changes.")

if __name__ == '__main__':
    main()
