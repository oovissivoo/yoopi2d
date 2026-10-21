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
#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include "cosmicpopupwindow.h"
#include "preferencesdef.h"

class PreferenceManager;
class QListWidgetItem;

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent);
    ~PreferencesDialog() override;

    void init(PreferenceManager* m);
    void updateRecentListBtn(bool isEmpty);

public slots:
    void changePage(QListWidgetItem* current, QListWidgetItem* previous);
    void onColorPreferenceChanged(SETTING setting);

signals:
    void windowOpacityChange(int);
    void soundScrubChanged(bool b);
    void soundScrubMsecChanged(int mSec);
    void curveOpacityChange(int);
    void clearRecentList();
    void updateRecentFileListBtn();

protected:
    void closeEvent(QCloseEvent*) override;

private:
    void applyCosmicTextToAllWidgets();
    void applyCosmicTextRecursively(QWidget* widget);
    Ui::PreferencesDialog* ui = nullptr;

    PreferenceManager* mPrefManager = nullptr;

    QPointer<QWidget> mCosmicHeaderHost;
};

#endif // PREFERENCESDIALOG_H
