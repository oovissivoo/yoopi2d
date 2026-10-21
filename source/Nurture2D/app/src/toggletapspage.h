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

#ifndef TOGGLETAPSPAGE_H
#define TOGGLETAPSPAGE_H

#include <QWidget>
#include <QMap>

class PreferenceManager;
class CosmicCheckboxToggle;

namespace Ui {
class ToggleTapsPage;
}

class ToggleTapsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ToggleTapsPage(QWidget* parent = nullptr);
    ~ToggleTapsPage() override;

    void setManager(PreferenceManager* p) { mManager = p; }
    void updateValues();
    void updateAccentColors();

private slots:
    void dockToggleChanged(const QString& dockName, bool enabled);

private:
    void setupDockToggles();

    Ui::ToggleTapsPage* ui = nullptr;
    PreferenceManager* mManager = nullptr;
    
    // Map of dock object names to their toggle widgets
    QMap<QString, CosmicCheckboxToggle*> mDockToggles;
};

#endif // TOGGLETAPSPAGE_H
