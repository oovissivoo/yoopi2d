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
#ifndef TOOLBOXWIDGET_H
#define TOOLBOXWIDGET_H

#include <QObject>
#include <QWidget>

#include "flowlayout.h"
#include "pencildef.h"
#include "toolboxlayout.h"
#include "preferencesdef.h"

class Editor;
class PenColorPalette;
class QAbstractButton;
class CosmicCheckboxToggle;

namespace Ui {
class ToolBoxWidget;
}

class ToolBoxWidget : public QWidget
{
    Q_OBJECT
public:
    ToolBoxWidget(QWidget* parent = nullptr);
    ~ToolBoxWidget() override;

    void setEditor(Editor* editor) { mEditor = editor; }
    void initUI();
    void updateUI();
    void updateToolboxStyles();
    void onColorPreferenceChanged(SETTING setting);

public slots:
    void setActiveTool(ToolType toolType);

public:

    void eraserOn();
    void selectOn();
    void moveOn();
    void penOn();
    void pencilOn();
    void handOn();
    void polylineOn();
    void curveOn();
    void bsplineOn();
    void bucketOn();
    void eyedropperOn();
    void dplateOn();
    void lassoSelectOn();

    void updateLayoutAlignment();
    void deselectAllTools();
    void toolOn(ToolType toolType, QAbstractButton* toolButton);

protected:
    int getMinHeightForWidth(int width) const;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void retranslateTooltips();
    FlowLayout* mFlowlayout = nullptr;

    Ui::ToolBoxWidget* ui = nullptr;
    Editor* mEditor = nullptr;
    PenColorPalette* mPenColorPalette = nullptr;

    CosmicCheckboxToggle* mEraserToggle = nullptr;
    CosmicCheckboxToggle* mLassoSelectToggle = nullptr;
    CosmicCheckboxToggle* mPenToggle = nullptr;
    CosmicCheckboxToggle* mPencilToggle = nullptr;
    CosmicCheckboxToggle* mHandToggle = nullptr;
    CosmicCheckboxToggle* mPolylineToggle = nullptr;
    CosmicCheckboxToggle* mCurveToggle = nullptr;
    CosmicCheckboxToggle* mBsplineToggle = nullptr;
    CosmicCheckboxToggle* mBucketToggle = nullptr;
    CosmicCheckboxToggle* mRectGapFillToggle = nullptr;
    CosmicCheckboxToggle* mLassoGapFillToggle = nullptr;
    CosmicCheckboxToggle* mEyedropperToggle = nullptr;
    CosmicCheckboxToggle* mDplateToggle = nullptr;
};

#endif // TOOLBOXWIDGET_H

