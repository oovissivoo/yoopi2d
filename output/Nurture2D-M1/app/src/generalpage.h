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

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include <QWidget>
#include <QStringList>
#include <QMap>

class QPushButton;
class QColor;
class QCheckBox;
class QGroupBox;
class QLabel;

class QAbstractButton;
class PreferenceManager;
class BlenderStyleValueField;
class CosmicDropDown;
class CosmicInputField;
class CosmicCheckboxToggle;

namespace Ui {
class GeneralPage;
}

class GeneralPage : public QWidget
{
Q_OBJECT
public:
    GeneralPage();
    ~GeneralPage() override;
    void setManager(PreferenceManager* p) { mManager = p; }
    void updateAccentColors();

public slots:
    void updateValues();
    void gridWidthChanged(int value);
    void gridHeightChanged(int value);
    void actionSafeCheckBoxStateChanged(int b);
    void actionSafeAreaChanged(int value);
    void titleSafeCheckBoxStateChanged(int b);
    void titleSafeAreaChanged(int value);
    void SafeAreaHelperTextCheckBoxStateChanged(int b);

signals:
    void windowOpacityChange(int value);

protected:
    void changeEvent(QEvent* e) override;

private slots:
    void languageChanged(int i);
    void shadowsCheckboxStateChanged(int b);
    void antiAliasCheckboxStateChanged(int b);
    void toolCursorsCheckboxStateChanged(int b);
    void canvasCursorCheckboxStateChanged(int b);
    void cursorStyleChanged(int index);
    void alwaysShowCircleChanged(int b);
    void cursorPressureSizeChanged(int b);
    void rememberCursorSizeChanged(int b);
    void highResCheckboxStateChanged(int b);
    void gridCheckBoxStateChanged(int b);
    void curveSmoothingChanged(int value);
    void backgroundChanged(QAbstractButton* button);
    void frameCacheNumberChanged(int value);
    void invertScrollDirectionBoxStateChanged(int b);
    void undoRedoMaxStepsChanged();

private:
    void retranslatePageStrings();
    void updateSafeHelperTextEnabledState();
    void refreshGapCheckColorButtons();
    void setButtonColorSwatch(QPushButton* button, const QColor& color);

    Ui::GeneralPage* ui = nullptr;
    PreferenceManager* mManager = nullptr;
    QPushButton* mGapCheckBgButton = nullptr;
    QPushButton* mGapCheckDrawButton = nullptr;
    CosmicDropDown* mLanguageDropDown = nullptr;
    CosmicDropDown* mCursorStyleDropDown = nullptr;
    CosmicInputField* mWindowOpacityField = nullptr;
    CosmicInputField* mCurveSmoothingField = nullptr;
    CosmicInputField* mGridWidthField = nullptr;
    CosmicInputField* mGridHeightField = nullptr;
    CosmicInputField* mActionSafeField = nullptr;
    CosmicInputField* mTitleSafeField = nullptr;
    CosmicInputField* mFramePoolSizeField = nullptr;
    CosmicInputField* mUndoStepsField = nullptr;
    QStringList mLanguageLocaleCodes;
    bool mIsApplyingLanguageChange = false;

    // Maps each hidden QCheckBox to its visible CosmicCheckboxToggle replacement,
    // so we can sync translated text and tooltips on language change.
    QMap<QCheckBox*, CosmicCheckboxToggle*> mCheckboxToggleMap;

    // Section header labels inserted dynamically into each group box.
    // Stored so we can retranslate them without searching the widget tree.
    QLabel* mHeaderLanguage    = nullptr;
    QLabel* mHeaderAppearance  = nullptr;
    QLabel* mHeaderCursors     = nullptr;
    QLabel* mHeaderCanvas      = nullptr;
    QLabel* mHeaderGrid        = nullptr;
    QLabel* mHeaderOverlays    = nullptr;
    QLabel* mHeaderAdvanced    = nullptr;
    QLabel* mHeaderUndoRedo    = nullptr;

    // Dynamically-created labels in the Appearance card for gap-check color pickers.
    QLabel* mGapCheckBgLabel   = nullptr;
    QLabel* mGapCheckDrawLabel = nullptr;
};

#endif // GENERALPAGE_H
