/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef COLOR_PALETTE_WIDGET_H
#define COLOR_PALETTE_WIDGET_H

#include "basedockwidget.h"
#include "preferencesdef.h"

class QToolButton;
class QListWidget;
class QListWidgetItem;
class Object;
class Editor;
class ColorBox;
class QActionGroup;
class CosmicPopupContextMenu;
class QString;
class CosmicDropDown;
class CosmicButton;
class QAbstractItemDelegate;

namespace Ui
{
    class ColorPalette;
}

class ColorPaletteWidget : public BaseDockWidget
{
    Q_OBJECT

public:

    explicit ColorPaletteWidget(QWidget* parent);
    ~ColorPaletteWidget() override;

    void initUI() override;
    void updateUI() override;
    void setCore(Editor* editor);
    void updateHighlightColor();

    int currentColorNumber();

    void selectColorNumber(int) const;
    void refreshColorList();

    void adjustSwatches();



    // Preset combo reset API for external callers (e.g., MainWindow2 palette ops)
    // Clears the preset selection to indicate the palette has been modified
    void resetPresetSelection();

signals:
    void colorNumberChanged(int);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void clickColorListItem(QListWidgetItem*);
    void changeColorName(QListWidgetItem*);
    void onItemChanged(QListWidgetItem* item);
    void onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);
    void clickAddColorButton();
    void clickColorDialogButton();
    void clickRemoveColorButton();
    void palettePreferences();
    void setListMode();
    void setGridMode();
    void setSwatchSizeSmall();
    void setSwatchSizeMedium();
    void setSwatchSizeLarge();
    void fitSwatchSize();
    void addItem();
    void replaceItem();
    void removeItem();
    void showPaletteReminder();
    void showPrefsMenu();

    bool showPaletteWarning();

    // Preset management slots
    void onPresetSelected(int index);
    void saveCurrentPaletteAsPreset();
    void importPresetFromFile();
    void exportPresetToFile();
    void deleteCurrentPreset();
    void newPreset();
    void renameCurrentPreset();

private:
    void updateItemColor(int, QColor);
    void updateGridUI();
    void addSwatch(int colorIndex) const;

    // Preset helpers
    void initializePresetSystem();
    void loadPresetList();
    void applyPreset(const QString& presetFilePath);
    QString getPresetFilePath(const QString& presetName);
    bool savePresetMetadata(const QString& presetName, const QString& fileName);
    bool removePresetMetadata(const QString& presetName);
    QMap<QString, QString> loadPresetMetadata();
    bool isPresetNameDuplicate(const QString& presetName, const QString& excludeName = QString());
    bool savePresetWithName(const QString& presetName, bool refreshUi = true);
    void autoSaveCurrentPreset();
    bool checkCanModifyCurrentPreset();

    Ui::ColorPalette* ui = nullptr;
    QActionGroup* mLayoutModeActionGroup = nullptr;
    QAction* mListModeAction = nullptr;
    QAction* mGridModeAction = nullptr;
    QActionGroup* mIconSizesActionGroup = nullptr;
    QAction* mSmallSwatchAction = nullptr;
    QAction* mMediumSwatchAction = nullptr;
    QAction* mLargeSwatchAction = nullptr;
    QAction* mSeparator = nullptr;

    // Preset UI/actions
    CosmicDropDown* mPresetComboBox = nullptr;
    CosmicButton* mAddColorButtonCosmic = nullptr;
    CosmicButton* mReplaceColorButtonCosmic = nullptr;
    CosmicButton* mRemoveColorButtonCosmic = nullptr;
    CosmicButton* mPalettePrefButtonCosmic = nullptr;
    QAction* mImportPresetAction = nullptr;
    QAction* mExportPresetAction = nullptr;
    QAction* mDeletePresetAction = nullptr;
    QAction* mPresetSeparator = nullptr;
    QAction* mNewPresetAction = nullptr;
    QAction* mRenamePresetAction = nullptr;

    QSize mIconSize{ 34, 34 };
    int stepper = 0;

    QAbstractItemDelegate* mDefaultItemDelegate = nullptr;
    QAbstractItemDelegate* mGridItemDelegate = nullptr;
    QAbstractItemDelegate* mListItemDelegate = nullptr;

    QString buttonStylesheet;

    bool mIsColorDialog = false;
    bool mMultipleSelected = false;
    bool mFitSwatches = false;
    // When true, updateUI() is a layout-only refresh and should not reset preset selection
    bool mLayoutRefreshOnly = false;
    // When true, refreshes should not reset the preset combo box selection
    bool mIsApplyingPreset = false;
    // When true during initialization, applying presets should not change the
    // tool's current color (we want to preserve/restore the tool color manually)
    bool mSuppressToolColorChangeDuringInit = false;
    // Tracks whether the preset combo box signal has been connected
    bool mPresetComboConnected = false;

    const int MIN_ICON_SIZE = 19;
    const int MEDIUM_ICON_SIZE = 26;
    const int MAX_ICON_SIZE = 36;

    Editor* mEditor = nullptr;
    Object* mObject = nullptr;

    // Preset storage paths
    QString mPresetsDirectory;
    QString mPresetsMetadataFile;

    // Built-in preset identifiers and color definitions
    static const QString BUILTIN_PRESET_GENGA;
    static const QString BUILTIN_PRESET_GENGA_FLUSHED;
    static const QStringList GENGA_COLORS;
    static const QStringList GENGA_FLUSHED_COLORS;

    // Built-in preset helpers
    bool isBuiltInPreset(const QString& presetName) const;
    void ensureBuiltInPresetsExist();
    void createBuiltInPresetFile(const QString& presetName, const QStringList& hexColors);
    void applyDefaultPreset();
    void onAccentColorChanged(SETTING setting);
};

#endif

