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
#ifndef AUTOLINECOLORTOGGLE_H
#define AUTOLINECOLORTOGGLE_H

#include <QWidget>
#include <QColor>
#include <QVector>
#include <QIcon>
#include <QBoxLayout>
#include <QList>

class Editor;
class CosmicCheckboxToggle;

class AutoLineColorToggle : public QWidget
{
    Q_OBJECT
public:
    explicit AutoLineColorToggle(Editor* editor, QWidget* parent = nullptr);
    
    void setColorEnabled(int index, bool enabled);
    bool isColorEnabled(int index) const;
    void setMasterEnabled(bool enabled);
    bool isMasterEnabled() const;
    void setVerticalLayoutEnabled(bool vertical);

    CosmicCheckboxToggle* masterToggle() const { return mMasterToggle; }
    QList<CosmicCheckboxToggle*> colorButtons() const { return mColorButtons.toList(); }
    QWidget* masterSeparatorWidget() const { return mMasterSeparator; }
    void detachForExternalLayout();

signals:
    void colorToggled(int index, bool enabled);

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override;

private:
    void updateEnabledColors();
    void onMasterToggled(bool enabled);
    
    Editor* mEditor = nullptr;
    CosmicCheckboxToggle* mMasterToggle = nullptr;
    QVector<QColor> mColors;
    QVector<bool> mEnabled;
    QVector<QRgb> mColorValues;
    QVector<CosmicCheckboxToggle*> mColorButtons;

    QIcon mOpenBucketIcon;
    QIcon mClosedBucketIcon;
    
    const int mCircleSize = 24;
    const int mSpacing = 8;
    const int mBorderWidth = 3;
    QBoxLayout* mLayout = nullptr;
    QWidget* mMasterSeparator = nullptr;
};

#endif // AUTOLINECOLORTOGGLE_H
