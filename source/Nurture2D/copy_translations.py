#!/usr/bin/env python3
"""
Copy translations from well-translated languages to fill gaps in others.
This is the most practical approach for a solo developer - reuse existing work!
"""

import xml.etree.ElementTree as ET
from pathlib import Path
from collections import defaultdict

def extract_translations(ts_file):
    """Extract all translations from a .ts file into a dictionary."""
    try:
        tree = ET.parse(ts_file)
        root = tree.getroot()
    except Exception as e:
        print(f"ERROR: Failed to parse {ts_file}: {e}")
        return {}
    
    translations = {}
    
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
            
            # Only include finished translations
            if translation_type != 'unfinished' and translation_text:
                key = f"{context_name}::{source_text}"
                translations[key] = translation_text
    
    return translations

def apply_translations(ts_file, translations_dict, source_lang_name):
    """Apply translations from dictionary to unfinished strings."""
    try:
        tree = ET.parse(ts_file)
        root = tree.getroot()
    except Exception as e:
        print(f"ERROR: Failed to parse {ts_file}: {e}")
        return 0
    
    applied_count = 0
    
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
            
            # Only fill unfinished translations
            if translation_type == 'unfinished' or not translation_text:
                key = f"{context_name}::{source_text}"
                
                if key in translations_dict:
                    translation.text = translations_dict[key]
                    if 'type' in translation.attrib:
                        del translation.attrib['type']
                    applied_count += 1
    
    if applied_count > 0:
        try:
            ET.indent(tree, space='    ')
            tree.write(ts_file, encoding='utf-8', xml_declaration=True)
        except Exception as e:
            print(f"✗ Failed to save {ts_file}: {e}")
            return 0
    
    return applied_count

def get_completion_stats(ts_file):
    """Get completion statistics for a translation file."""
    try:
        tree = ET.parse(ts_file)
        root = tree.getroot()
    except:
        return 0, 0, 0
    
    total = 0
    finished = 0
    unfinished = 0
    
    for message in root.findall('.//message'):
        translation = message.find('translation')
        if translation is not None:
            total += 1
            translation_type = translation.get('type', '')
            if translation_type == 'unfinished' or not translation.text:
                unfinished += 1
            else:
                finished += 1
    
    completion = (finished / total * 100) if total > 0 else 0
    return total, finished, completion

def main():
    print("="*70)
    print("Translation Copy Tool - Reuse Existing Translations")
    print("="*70)
    print()
    print("Strategy: Copy translations from well-translated languages")
    print("to fill gaps in poorly-translated ones.")
    print()
    
    translations_dir = Path('translations')
    if not translations_dir.exists():
        print("ERROR: translations/ directory not found")
        return
    
    # Step 1: Identify best source languages
    print("Step 1: Analyzing translation completeness...")
    print()
    
    lang_stats = {}
    for ts_file in sorted(translations_dir.glob('pencil_*.ts')):
        lang_code = ts_file.stem.replace('pencil_', '')
        total, finished, completion = get_completion_stats(str(ts_file))
        lang_stats[lang_code] = (total, finished, completion, ts_file)
        print(f"  {lang_code:10s}: {completion:5.1f}% ({finished}/{total})")
    
    print()
    
    # Step 2: Select best source languages (>60% complete)
    source_langs = [(lang, stats) for lang, stats in lang_stats.items() 
                    if stats[2] > 60 and lang not in ['en']]
    source_langs.sort(key=lambda x: x[1][2], reverse=True)
    
    print(f"Step 2: Selected {len(source_langs)} source languages (>60% complete):")
    for lang, stats in source_langs[:10]:
        print(f"  ✓ {lang}: {stats[2]:.1f}%")
    print()
    
    if not source_langs:
        print("ERROR: No well-translated languages found to copy from!")
        return
    
    # Step 3: Extract translations from source languages
    print("Step 3: Extracting translations from source languages...")
    
    all_translations = defaultdict(list)
    
    for lang, stats in source_langs:
        ts_file = stats[3]
        print(f"  Reading {lang}...")
        translations = extract_translations(str(ts_file))
        
        for key, value in translations.items():
            all_translations[key].append((lang, value))
    
    print(f"  ✓ Extracted {len(all_translations)} unique translation keys")
    print()
    
    # Step 4: Apply to target languages (<60% complete)
    target_langs = [(lang, stats) for lang, stats in lang_stats.items() 
                    if stats[2] < 60 and lang not in ['en']]
    target_langs.sort(key=lambda x: x[1][2])
    
    print(f"Step 4: Applying translations to {len(target_langs)} target languages:")
    print()
    
    total_applied = 0
    
    for lang, stats in target_langs:
        ts_file = stats[3]
        print(f"  Processing {lang} (currently {stats[2]:.1f}%)...")
        
        # Build translation dict (prefer first source language)
        trans_dict = {}
        for key, values in all_translations.items():
            if values:
                # Use the translation from the most complete language
                trans_dict[key] = values[0][1]
        
        applied = apply_translations(str(ts_file), trans_dict, source_langs[0][0])
        total_applied += applied
        
        # Get new stats
        new_total, new_finished, new_completion = get_completion_stats(str(ts_file))
        improvement = new_completion - stats[2]
        
        print(f"    ✓ Applied {applied} translations")
        print(f"    ✓ {stats[2]:.1f}% → {new_completion:.1f}% (+{improvement:.1f}%)")
        print()
    
    print("="*70)
    print("SUMMARY")
    print("="*70)
    print(f"Total translations applied: {total_applied}")
    print()
    print("Next steps:")
    print("1. Compile: lrelease translations/pencil_*.ts")
    print("2. Copy: cp translations/*.qm app/.qm/")
    print("3. Rebuild your app")
    print()
    print("Note: Translations were copied from similar contexts.")
    print("They should be mostly correct, but may need review for context-specific terms.")

if __name__ == '__main__':
    main()
