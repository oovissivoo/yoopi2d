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

#ifndef EXPORTIMAGEDIALOG_H
#define EXPORTIMAGEDIALOG_H

#include "importexportdialog.h"

namespace Ui {
class ExportImageOptions;
}

class CosmicDropDown;

class ExportImageDialog : public ImportExportDialog
{
    Q_OBJECT

public:
    explicit ExportImageDialog(QWidget* parent, FileType eType);
    ~ExportImageDialog();

    bool getTransparency() const;
    QString getExportFormat() const;

    // Export Specific controls
    enum class ExportSpecificMode {
        Keyframes = 0,
        CellsOnly = 1
    };

    bool isExportSpecificEnabled() const;
    ExportSpecificMode getExportSpecificMode() const;

private slots:
    void formatChanged(const QString& format);


private:
    Ui::ExportImageOptions* ui = nullptr;
    CosmicDropDown* mFormatDropDown = nullptr;
    CosmicDropDown* mExportSpecificModeDropDown = nullptr;

    void setTransparencyOptionVisibility(const QString& format);
};

#endif // EXPORTIMAGEDIALOG_H
