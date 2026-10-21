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
#ifndef CAMERAOPTIONSWIDGET_H
#define CAMERAOPTIONSWIDGET_H

#include <QWidget>

#include "pencildef.h"
#include "camera.h"

class Editor;
class CameraTool;
class CosmicDropDown;
class BlenderStyleToggle;

namespace Ui {
class CameraOptionsWidget;
}

class CameraOptionsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraOptionsWidget(Editor* editor, QWidget *parent = nullptr);
    ~CameraOptionsWidget();

    void updateUI();

    void setShowCameraPath(bool showCameraPath);
    void setPathDotColorType(DotColorType index);
    void onToolPropertyChanged(ToolType, ToolPropertyType ePropertyType);

protected:
    void changeEvent(QEvent* e) override;

private:
    Ui::CameraOptionsWidget *ui;
    Editor* mEditor = nullptr;

    CameraTool* mCameraTool = nullptr;
    // New cosmic dropdown replacing ui->pathColorComboBox
    CosmicDropDown* mPathColorDropDown = nullptr;
    // New Blender-style toggle replacing ui->showCameraPathCheckBox
    BlenderStyleToggle* mShowCameraPathToggle = nullptr;
};

#endif // CAMERAOPTIONSWIDGET_H
