/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2024-2099 Oliver S. Larsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef PENCOLORPALETTE_H
#define PENCOLORPALETTE_H

#include <QWidget>
#include <QBoxLayout>
#include <QColor>
#include "preferencesdef.h"
#include <QList>

class PreferenceManager;
class CosmicCheckboxToggle;

class PenColorPalette : public QWidget
{
    Q_OBJECT

public:
    explicit PenColorPalette(QWidget* parent = nullptr);
    void setCurrentColor(const QColor& color);
    QColor getCurrentColor() const;
    int getActiveButtonIndex() const;
    void activateFirstButtonAndSetColor(const QColor& color);
    bool isPresetColor(const QColor& color) const;
    void activatePresetButton(const QColor& color);
    void setVerticalLayoutEnabled(bool vertical);

    QList<CosmicCheckboxToggle*> colorButtons() const { return mColorButtons; }
    QWidget* firstSeparatorWidget() const { return mFirstSeparator; }
    void detachForExternalLayout();

signals:
    void colorChanged(const QColor& color);

private slots:
    void onColorButtonClicked();

public:
    void onColorPreferenceChanged(SETTING setting);
    void connectToPreferenceManager(PreferenceManager* prefs);

private:
    void setupUI();
    void createColorButton(const QColor& color, bool isDefault = false);
    void setActiveButton(CosmicCheckboxToggle* button);
    void updateButtonStyle(CosmicCheckboxToggle* button, const QColor& color, bool isActive = false, bool isDefault = false);

    QBoxLayout* mLayout;
    QList<CosmicCheckboxToggle*> mColorButtons;
    QList<QColor> mColors;
    CosmicCheckboxToggle* mActiveButton;
    QColor mCurrentColor;
    QColor mSavedColor;  // Stores the last used color for the first button
    QWidget* mFirstSeparator = nullptr;

    // Default colors: changeable drawing color + 5 fill tool colors
    const QList<QColor> FILL_COLORS = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::magenta};
};

#endif // PENCOLORPALETTE_H