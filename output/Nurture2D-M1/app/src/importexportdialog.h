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

#ifndef IMPORTEXPORTDIALOG_H
#define IMPORTEXPORTDIALOG_H

#include <QDialog>
#include <QGroupBox>
#include "filetype.h"

namespace Ui {
class ImportExportDialog;
}

class QDialogButtonBox;
class QScrollArea;

class ImportExportDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Import, Export };

    explicit ImportExportDialog(QWidget* parent, Mode eMode, FileType eFileType);
    ~ImportExportDialog() override;

    void init();
    QString getFilePath() const;
    QString getAbsolutePath();
    QStringList getFilePaths();
    int getPosIndex() { return mPosIndex; }

    // NEW: hide the top file group
    void hideFileGroupBox(bool hide);

signals:
    void filePathsChanged(QStringList filePaths);

protected:
    QGroupBox* getOptionsGroupBox();
    QGroupBox* getPreviewGroupBox();
    QDialogButtonBox* getDialogButtonBox();

    // Helper accessors for child dialogs if needed
    QScrollArea* getScrollArea() const { return mScrollArea; }
    void adjustDialogSize();

    void setFileExtension(const QString& extension);
    void hideOptionsGroupBox(bool hide);
    void hidePreviewGroupBox(bool hide);
    void hideInstructionsLabel(bool hide);

    void setInstructionsLabel(const QString& text);
    void setOkButtonEnabled(bool enabled);

private slots:
    void browse();
    void setPosIndex(int index) { mPosIndex = index; }

private:
    Ui::ImportExportDialog* ui = nullptr;

    QStringList m_filePaths;

    FileType mFileType = FileType::ANIMATION;
    Mode mMode = Import;
    int mPosIndex = 0;

    // Scroll area holding the main content to avoid clipping
    QScrollArea* mScrollArea = nullptr;
};

#endif // IMPORTEXPORTDIALOG_H
