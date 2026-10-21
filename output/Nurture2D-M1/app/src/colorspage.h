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

#ifndef COLORSPAGE_H
#define COLORSPAGE_H

#include <QWidget>
#include "preferencesdef.h"

class QPushButton;
class QColor;

class PreferenceManager;

namespace Ui {
class ColorsPage;
}

class ColorsPage : public QWidget
{
Q_OBJECT
public:
    ColorsPage();
    ~ColorsPage() override;
    void setManager(PreferenceManager* p) { mManager = p; }

public slots:
    void updateValues();

private slots:
    void onAccentColorButtonClicked();
    void onToolboxSelectionColorButtonClicked();
    void onToolPaletteSelectionColorButtonClicked();
    void onTitleBarColorButtonClicked();
    void onDockContentColorButtonClicked();
    void onScrollbarColorButtonClicked();

private:
    void refreshColorButtons();
    void setButtonColorSwatch(QPushButton* button, const QColor& color);

    Ui::ColorsPage* ui = nullptr;
    PreferenceManager* mManager = nullptr;
    QPushButton* mAccentColorButton = nullptr;
    QPushButton* mToolboxSelectionColorButton = nullptr;
    QPushButton* mToolPaletteSelectionColorButton = nullptr;
    QPushButton* mTitleBarColorButton = nullptr;
    QPushButton* mDockContentColorButton = nullptr;
    QPushButton* mScrollbarColorButton = nullptr;
};

#endif // COLORSPAGE_H
