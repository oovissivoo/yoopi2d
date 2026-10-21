#!/usr/bin/env python3
"""
Simple translation script using basic string replacement and common UI patterns.
No external dependencies required - uses built-in Python only.
"""

import xml.etree.ElementTree as ET
import sys
import re
from pathlib import Path

# Common UI translations for animation software
# These are standard terms that are consistent across languages
COMMON_TRANSLATIONS = {
    'de': {  # German
        'File': 'Datei',
        'Edit': 'Bearbeiten',
        'View': 'Ansicht',
        'Help': 'Hilfe',
        'Open': 'Öffnen',
        'Save': 'Speichern',
        'Close': 'Schließen',
        'Cancel': 'Abbrechen',
        'OK': 'OK',
        'Yes': 'Ja',
        'No': 'Nein',
        'Delete': 'Löschen',
        'Export': 'Exportieren',
        'Import': 'Importieren',
        'New': 'Neu',
        'Copy': 'Kopieren',
        'Paste': 'Einfügen',
        'Cut': 'Ausschneiden',
        'Undo': 'Rückgängig',
        'Redo': 'Wiederholen',
        'Select': 'Auswählen',
        'Color': 'Farbe',
        'Size': 'Größe',
        'Width': 'Breite',
        'Height': 'Höhe',
        'Opacity': 'Deckkraft',
        'Layer': 'Ebene',
        'Frame': 'Bild',
        'Timeline': 'Zeitleiste',
        'Brush': 'Pinsel',
        'Eraser': 'Radiergummi',
        'Pen': 'Stift',
        'Tool': 'Werkzeug',
        'Options': 'Optionen',
        'Preferences': 'Einstellungen',
        'Settings': 'Einstellungen',
        'About': 'Über',
        'Search': 'Suchen',
        'Browse': 'Durchsuchen',
        'Texture Gallery': 'Textur-Galerie',
        'Tip Gallery': 'Spitzen-Galerie',
        'Remove selected asset': 'Ausgewähltes Asset entfernen',
        'Thumbnail size': 'Vorschaugröße',
        'Delete Asset': 'Asset löschen',
    },
    'fr': {  # French
        'File': 'Fichier',
        'Edit': 'Éditer',
        'View': 'Affichage',
        'Help': 'Aide',
        'Open': 'Ouvrir',
        'Save': 'Enregistrer',
        'Close': 'Fermer',
        'Cancel': 'Annuler',
        'OK': 'OK',
        'Yes': 'Oui',
        'No': 'Non',
        'Delete': 'Supprimer',
        'Export': 'Exporter',
        'Import': 'Importer',
        'New': 'Nouveau',
        'Copy': 'Copier',
        'Paste': 'Coller',
        'Cut': 'Couper',
        'Undo': 'Annuler',
        'Redo': 'Refaire',
        'Select': 'Sélectionner',
        'Color': 'Couleur',
        'Size': 'Taille',
        'Width': 'Largeur',
        'Height': 'Hauteur',
        'Opacity': 'Opacité',
        'Layer': 'Calque',
        'Frame': 'Image',
        'Timeline': 'Timeline',
        'Brush': 'Pinceau',
        'Eraser': 'Gomme',
        'Pen': 'Stylo',
        'Tool': 'Outil',
        'Options': 'Options',
        'Preferences': 'Préférences',
        'Settings': 'Paramètres',
        'About': 'À propos',
        'Search': 'Rechercher',
        'Browse': 'Parcourir',
        'Texture Gallery': 'Galerie de textures',
        'Tip Gallery': 'Galerie de pointes',
        'Remove selected asset': 'Supprimer l\'asset sélectionné',
        'Thumbnail size': 'Taille des vignettes',
        'Delete Asset': 'Supprimer l\'asset',
    },
    'es': {  # Spanish
        'File': 'Archivo',
        'Edit': 'Editar',
        'View': 'Ver',
        'Help': 'Ayuda',
        'Open': 'Abrir',
        'Save': 'Guardar',
        'Close': 'Cerrar',
        'Cancel': 'Cancelar',
        'OK': 'Aceptar',
        'Yes': 'Sí',
        'No': 'No',
        'Delete': 'Eliminar',
        'Export': 'Exportar',
        'Import': 'Importar',
        'New': 'Nuevo',
        'Copy': 'Copiar',
        'Paste': 'Pegar',
        'Cut': 'Cortar',
        'Undo': 'Deshacer',
        'Redo': 'Rehacer',
        'Select': 'Seleccionar',
        'Color': 'Color',
        'Size': 'Tamaño',
        'Width': 'Ancho',
        'Height': 'Alto',
        'Opacity': 'Opacidad',
        'Layer': 'Capa',
        'Frame': 'Fotograma',
        'Timeline': 'Línea de tiempo',
        'Brush': 'Pincel',
        'Eraser': 'Borrador',
        'Pen': 'Pluma',
        'Tool': 'Herramienta',
        'Options': 'Opciones',
        'Preferences': 'Preferencias',
        'Settings': 'Configuración',
        'About': 'Acerca de',
        'Search': 'Buscar',
        'Browse': 'Examinar',
        'Texture Gallery': 'Galería de texturas',
        'Tip Gallery': 'Galería de puntas',
        'Remove selected asset': 'Eliminar recurso seleccionado',
        'Thumbnail size': 'Tamaño de miniatura',
        'Delete Asset': 'Eliminar recurso',
    },
    'it': {  # Italian
        'File': 'File',
        'Edit': 'Modifica',
        'View': 'Visualizza',
        'Help': 'Aiuto',
        'Open': 'Apri',
        'Save': 'Salva',
        'Close': 'Chiudi',
        'Cancel': 'Annulla',
        'OK': 'OK',
        'Yes': 'Sì',
        'No': 'No',
        'Delete': 'Elimina',
        'Export': 'Esporta',
        'Import': 'Importa',
        'New': 'Nuovo',
        'Copy': 'Copia',
        'Paste': 'Incolla',
        'Cut': 'Taglia',
        'Undo': 'Annulla',
        'Redo': 'Ripeti',
        'Select': 'Seleziona',
        'Color': 'Colore',
        'Size': 'Dimensione',
        'Width': 'Larghezza',
        'Height': 'Altezza',
        'Opacity': 'Opacità',
        'Layer': 'Livello',
        'Frame': 'Fotogramma',
        'Timeline': 'Timeline',
        'Brush': 'Pennello',
        'Eraser': 'Gomma',
        'Pen': 'Penna',
        'Tool': 'Strumento',
        'Options': 'Opzioni',
        'Preferences': 'Preferenze',
        'Settings': 'Impostazioni',
        'About': 'Informazioni',
        'Search': 'Cerca',
        'Browse': 'Sfoglia',
        'Texture Gallery': 'Galleria texture',
        'Tip Gallery': 'Galleria punte',
        'Remove selected asset': 'Rimuovi risorsa selezionata',
        'Thumbnail size': 'Dimensione miniatura',
        'Delete Asset': 'Elimina risorsa',
    },
    'ru': {  # Russian
        'File': 'Файл',
        'Edit': 'Правка',
        'View': 'Вид',
        'Help': 'Справка',
        'Open': 'Открыть',
        'Save': 'Сохранить',
        'Close': 'Закрыть',
        'Cancel': 'Отмена',
        'OK': 'ОК',
        'Yes': 'Да',
        'No': 'Нет',
        'Delete': 'Удалить',
        'Export': 'Экспорт',
        'Import': 'Импорт',
        'New': 'Новый',
        'Copy': 'Копировать',
        'Paste': 'Вставить',
        'Cut': 'Вырезать',
        'Undo': 'Отменить',
        'Redo': 'Повторить',
        'Select': 'Выбрать',
        'Color': 'Цвет',
        'Size': 'Размер',
        'Width': 'Ширина',
        'Height': 'Высота',
        'Opacity': 'Непрозрачность',
        'Layer': 'Слой',
        'Frame': 'Кадр',
        'Timeline': 'Временная шкала',
        'Brush': 'Кисть',
        'Eraser': 'Ластик',
        'Pen': 'Перо',
        'Tool': 'Инструмент',
        'Options': 'Параметры',
        'Preferences': 'Настройки',
        'Settings': 'Настройки',
        'About': 'О программе',
        'Search': 'Поиск',
        'Browse': 'Обзор',
        'Texture Gallery': 'Галерея текстур',
        'Tip Gallery': 'Галерея наконечников',
        'Remove selected asset': 'Удалить выбранный ресурс',
        'Thumbnail size': 'Размер миниатюры',
        'Delete Asset': 'Удалить ресурс',
    },
    'pt': {  # Portuguese
        'File': 'Arquivo',
        'Edit': 'Editar',
        'View': 'Ver',
        'Help': 'Ajuda',
        'Open': 'Abrir',
        'Save': 'Salvar',
        'Close': 'Fechar',
        'Cancel': 'Cancelar',
        'OK': 'OK',
        'Yes': 'Sim',
        'No': 'Não',
        'Delete': 'Excluir',
        'Export': 'Exportar',
        'Import': 'Importar',
        'New': 'Novo',
        'Copy': 'Copiar',
        'Paste': 'Colar',
        'Cut': 'Recortar',
        'Undo': 'Desfazer',
        'Redo': 'Refazer',
        'Select': 'Selecionar',
        'Color': 'Cor',
        'Size': 'Tamanho',
        'Width': 'Largura',
        'Height': 'Altura',
        'Opacity': 'Opacidade',
        'Layer': 'Camada',
        'Frame': 'Quadro',
        'Timeline': 'Linha do tempo',
        'Brush': 'Pincel',
        'Eraser': 'Borracha',
        'Pen': 'Caneta',
        'Tool': 'Ferramenta',
        'Options': 'Opções',
        'Preferences': 'Preferências',
        'Settings': 'Configurações',
        'About': 'Sobre',
        'Search': 'Pesquisar',
        'Browse': 'Procurar',
        'Texture Gallery': 'Galeria de texturas',
        'Tip Gallery': 'Galeria de pontas',
        'Remove selected asset': 'Remover recurso selecionado',
        'Thumbnail size': 'Tamanho da miniatura',
        'Delete Asset': 'Excluir recurso',
    },
    'zh-CN': {  # Chinese Simplified
        'File': '文件',
        'Edit': '编辑',
        'View': '查看',
        'Help': '帮助',
        'Open': '打开',
        'Save': '保存',
        'Close': '关闭',
        'Cancel': '取消',
        'OK': '确定',
        'Yes': '是',
        'No': '否',
        'Delete': '删除',
        'Export': '导出',
        'Import': '导入',
        'New': '新建',
        'Copy': '复制',
        'Paste': '粘贴',
        'Cut': '剪切',
        'Undo': '撤销',
        'Redo': '重做',
        'Select': '选择',
        'Color': '颜色',
        'Size': '大小',
        'Width': '宽度',
        'Height': '高度',
        'Opacity': '不透明度',
        'Layer': '图层',
        'Frame': '帧',
        'Timeline': '时间轴',
        'Brush': '画笔',
        'Eraser': '橡皮擦',
        'Pen': '钢笔',
        'Tool': '工具',
        'Options': '选项',
        'Preferences': '首选项',
        'Settings': '设置',
        'About': '关于',
        'Search': '搜索',
        'Browse': '浏览',
        'Texture Gallery': '纹理库',
        'Tip Gallery': '笔尖库',
        'Remove selected asset': '删除所选资源',
        'Thumbnail size': '缩略图大小',
        'Delete Asset': '删除资源',
    },
}

# Map language codes to translation dictionaries
LANG_DICT_MAP = {
    'de': 'de',
    'fr': 'fr',
    'es': 'es',
    'it': 'it',
    'ru': 'ru',
    'pt': 'pt',
    'pt_BR': 'pt',
    'zh_CN': 'zh-CN',
    'zh_TW': 'zh-CN',  # Use simplified for traditional (better than nothing)
}

def translate_text(text, lang_code):
    """Translate text using common translations dictionary."""
    dict_key = LANG_DICT_MAP.get(lang_code)
    if not dict_key or dict_key not in COMMON_TRANSLATIONS:
        return None
    
    translations = COMMON_TRANSLATIONS[dict_key]
    
    # Direct match
    if text in translations:
        return translations[text]
    
    # Try case-insensitive match
    for key, value in translations.items():
        if key.lower() == text.lower():
            return value
    
    # Try partial match for longer strings
    for key, value in translations.items():
        if key in text:
            return text.replace(key, value)
    
    return None

def should_translate(text):
    """Check if text should be translated."""
    if not text or not text.strip():
        return False
    
    # Skip numbers/symbols only
    if re.match(r'^[\d\s\.\,\-\+\*\/\(\)\[\]\{\}]+$', text):
        return False
    
    # Skip format placeholders only
    if re.match(r'^%\d+$', text):
        return False
    
    # Skip very short technical strings
    if len(text) <= 2 and text.isupper():
        return False
    
    return True

def translate_ts_file(ts_file, lang_code):
    """Translate unfinished strings in a Qt .ts file."""
    print(f"\nProcessing: {ts_file}")
    print(f"Language: {lang_code}")
    
    try:
        tree = ET.parse(ts_file)
        root = tree.getroot()
    except Exception as e:
        print(f"ERROR: Failed to parse {ts_file}: {e}")
        return 0, 0
    
    translated_count = 0
    total_unfinished = 0
    
    for context in root.findall('.//context'):
        for message in context.findall('message'):
            source = message.find('source')
            translation = message.find('translation')
            
            if source is None or translation is None:
                continue
            
            source_text = source.text
            translation_type = translation.get('type', '')
            translation_text = translation.text
            
            if translation_type == 'unfinished' or not translation_text:
                total_unfinished += 1
                
                if not should_translate(source_text):
                    continue
                
                translated = translate_text(source_text, lang_code)
                
                if translated:
                    translation.text = translated
                    if 'type' in translation.attrib:
                        del translation.attrib['type']
                    translated_count += 1
                    print(f"  ✓ {source_text} → {translated}")
    
    if translated_count > 0:
        try:
            ET.indent(tree, space='    ')
            tree.write(ts_file, encoding='utf-8', xml_declaration=True)
            print(f"✓ Saved {ts_file}")
        except Exception as e:
            print(f"✗ Failed to save {ts_file}: {e}")
    
    print(f"Results: {translated_count}/{total_unfinished} translated")
    return translated_count, total_unfinished

def main():
    print("="*60)
    print("Simple Translation Tool (No External Dependencies)")
    print("="*60)
    print()
    
    translations_dir = Path('translations')
    if not translations_dir.exists():
        print("ERROR: translations/ directory not found")
        return
    
    # Process languages we have translations for
    target_langs = ['de', 'fr', 'es', 'it', 'ru', 'pt', 'pt_BR', 'zh_CN', 'zh_TW']
    
    total_translated = 0
    total_unfinished = 0
    
    for lang in target_langs:
        ts_file = translations_dir / f'pencil_{lang}.ts'
        if ts_file.exists():
            translated, unfinished = translate_ts_file(str(ts_file), lang)
            total_translated += translated
            total_unfinished += unfinished
    
    print(f"\n{'='*60}")
    print(f"SUMMARY")
    print(f"{'='*60}")
    print(f"Total strings translated: {total_translated}")
    print(f"Total unfinished strings: {total_unfinished}")
    print()
    print("Note: This script only translates common UI terms.")
    print("For complete translation, use the full auto_translate.py script")
    print("with translation libraries installed.")

if __name__ == '__main__':
    main()
