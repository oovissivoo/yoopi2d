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
#ifndef IMPORTPOSITIONDIALOG_H
#define IMPORTPOSITIONDIALOG_H

// Note: This dialog is still used by ActionCommands::importAnimatedImage()
// for positioning animated image imports.
// It is no longer used by MainWindow2 import methods.

#include <QDialog>

#include "importimageconfig.h"

namespace Ui {
class ImportPositionDialog;
}

class Editor;

class ImportPositionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportPositionDialog(Editor* editor, QWidget *parent = nullptr);
    ~ImportPositionDialog();

    ImportImageConfig importConfig() const { return mImportConfig; }

private slots:
    void didChangeComboBoxIndex(const int index);
    void changeImportView();

private:
    static ImportImageConfig::PositionType getTypeFromIndex(int index) {
        switch (index) {
        case 0: return ImportImageConfig::CenterOfView;
        case 1: return ImportImageConfig::CenterOfCanvas;
        case 2: return ImportImageConfig::CenterOfCamera;
        case 3: return ImportImageConfig::CenterOfCameraFollowed;
        default: return ImportImageConfig::None;
        }
    }
    static int indexFromType(ImportImageConfig::PositionType type, bool hasCamera) {
        switch (type) {
        case ImportImageConfig::CenterOfView: return 0;
        case ImportImageConfig::CenterOfCanvas: return 1;
        case ImportImageConfig::CenterOfCamera: return hasCamera ? 2 : 0;
        case ImportImageConfig::CenterOfCameraFollowed: return hasCamera ? 3 : 0;
        default: return 0;
        }
    }

    Ui::ImportPositionDialog *ui;

    ImportImageConfig mImportConfig;
    Editor* mEditor = nullptr;
    // Track availability of camera layer to filter options
    bool mHasCameraLayer = false;
    class CosmicDropDown* mPositionDropDown = nullptr;
};

#endif // IMPORTPOSITIONDIALOG_H
