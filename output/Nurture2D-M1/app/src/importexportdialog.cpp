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

#include "importexportdialog.h"
#include "ui_importexportdialog.h"
#include <QFileInfo>
#include <QDialogButtonBox>
#include "filedialog.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QFile>
#include <QIODevice>

ImportExportDialog::ImportExportDialog(QWidget* parent, Mode eMode, FileType eFileType) : QDialog(parent)
{
    mMode = eMode;
    mFileType = eFileType;

    ui = new Ui::ImportExportDialog;
    ui->setupUi(this);
    hidePreviewGroupBox(true);
    // Make dialog resizable and set a reasonable minimum size
    setMinimumSize(455, 400);

    // Ensure window flags allow resizing
    Qt::WindowFlags eFlags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
    setWindowFlags(eFlags);

    // Configure scroll area (defined in .ui as scrollArea)
    mScrollArea = findChild<QScrollArea*>("scrollArea");
    if (mScrollArea) {
        mScrollArea->setWidgetResizable(true);
        mScrollArea->setFrameShape(QFrame::NoFrame);
        mScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
    // Apply dark theme stylesheet to this dialog so dialog QGroupBox rules take effect
    QFile darkQss(":/styles/dark_theme.qss");
    if (darkQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString qss = QString::fromUtf8(darkQss.readAll());
        setStyleSheet(qss);
    }
    connect(ui->browseButton, &QPushButton::clicked, this, &ImportExportDialog::browse);
}

ImportExportDialog::~ImportExportDialog()
{
    delete ui;
}

QDialogButtonBox* ImportExportDialog::getDialogButtonBox()
{
    return ui->buttonBox;
}

QString ImportExportDialog::getFilePath() const
{
    return m_filePaths.isEmpty() ? QString() : m_filePaths.first();
}

QStringList ImportExportDialog::getFilePaths()
{
    return m_filePaths;
}

QString ImportExportDialog::getAbsolutePath()
{
    QFileInfo info(m_filePaths.first());
    return info.absolutePath();
}

void ImportExportDialog::hideInstructionsLabel(bool hide)
{
    ui->instructionsLabel->setHidden(hide);
}

void ImportExportDialog::setInstructionsLabel(const QString& text)
{
    ui->instructionsLabel->setText(text);
}

void ImportExportDialog::setOkButtonEnabled(const bool enabled)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

void ImportExportDialog::init()
{
    switch (mMode)
    {
        case Import:
            m_filePaths = QStringList(FileDialog::getLastOpenPath(mFileType));
            break;
        case Export:
            m_filePaths = QStringList(FileDialog::getLastSavePath(mFileType));
            break;
        default:
            Q_ASSERT(false);
    }
    ui->fileEdit->setText("\"" + m_filePaths.first() + "\"");

    emit filePathsChanged(m_filePaths);

    adjustDialogSize();
}

QGroupBox* ImportExportDialog::getOptionsGroupBox()
{
    return ui->optionsGroupBox;
}

void ImportExportDialog::hideOptionsGroupBox(bool hide)
{
    ui->optionsGroupBox->setHidden(hide);
}

void ImportExportDialog::hidePreviewGroupBox(bool hide)
{
    ui->previewGroupBox->setHidden(hide);
    adjustDialogSize();
}

QGroupBox* ImportExportDialog::getPreviewGroupBox()
{
    return ui->previewGroupBox;
}

void ImportExportDialog::setFileExtension(const QString& extension)
{
    for (int i = 0; i < m_filePaths.size(); i++)
    {
        QFileInfo info(m_filePaths.at(i));
        m_filePaths.replace(i, info.path() + "/" + info.completeBaseName() + "." + extension);
    }
    ui->fileEdit->setText("\"" + m_filePaths.join("\" \"") + "\"");

    emit filePathsChanged(m_filePaths);
}

void ImportExportDialog::browse()
{
    QStringList filePaths;
    switch (mMode)
    {
        case Import:
            if (mFileType == FileType::IMAGE_SEQUENCE)
            {
                filePaths = FileDialog::getOpenFileNames(this, FileType::IMAGE_SEQUENCE);
                break;
            }
            filePaths = QStringList(FileDialog::getOpenFileName(this, mFileType));
            break;
        case Export:
            filePaths = QStringList(FileDialog::getSaveFileName(this, mFileType));
            break;
        default:
            Q_ASSERT(false);
    }

    if (filePaths.isEmpty() || filePaths.first().isEmpty())
    {
        return;
    }

    m_filePaths = filePaths;
    ui->fileEdit->setText("\"" + filePaths.join("\" \"") + "\"");

    emit filePathsChanged(m_filePaths);
}

// NEW: hide the top file group box
void ImportExportDialog::hideFileGroupBox(bool hide)
{
    ui->fileGroupBox->setHidden(hide);
    adjustDialogSize();
}

void ImportExportDialog::adjustDialogSize()
{
    // Let Qt compute a good size based on current content
    adjustSize();

    // Clamp to available screen geometry to avoid oversizing on small displays
    QRect availableGeometry;
    if (QWindow* win = windowHandle()) {
        if (QScreen* scr = QGuiApplication::screenAt(win->geometry().center())) {
            availableGeometry = scr->availableGeometry();
        }
    }
    if (availableGeometry.isNull()) {
        if (QScreen* primary = QGuiApplication::primaryScreen()) {
            availableGeometry = primary->availableGeometry();
        }
    }
    if (!availableGeometry.isNull()) {
        const int maxW = availableGeometry.width();
        const int maxH = availableGeometry.height();
        int newW = qMin(width(), maxW);
        int newH = qMin(height(), maxH);
        resize(newW, newH);
    }
}
