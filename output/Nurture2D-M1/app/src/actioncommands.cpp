/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "actioncommands.h"
#include "timelinecommands.h"

#include <cmath>
#include <QImage>
#include <QPainter>

#include <QInputDialog>
#include <QMessageBox>
#include "cosmicprogresspopup.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <QDebug>
#include <QPropertyAnimation>
#include <QLabel>
#include <QPointer>
#include <QShowEvent>
#include <QVBoxLayout>

#include "pencildef.h"
#include "editor.h"
#include "object.h"
#include "viewmanager.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "soundmanager.h"
#include "playbackmanager.h"
#include "colormanager.h"
#include "preferencemanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "undoredocommand.h"
#include "util.h"
#include "app_util.h"

#include "layercamera.h"
#include "layersound.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "vectorimage.h"
#include "soundclip.h"
#include "camera.h"

#include "importimageseqdialog.h"
#include "importpositiondialog.h"
#include "movieimporter.h"
#include "movieexporter.h"
#include "filedialog.h"
#include "exportmoviedialog.h"
#include "exportimagedialog.h"
#include "aboutdialog.h"
#include "doubleprogressdialog.h"
#include "checkupdatesdialog.h"
#include "errordialog.h"
#include "exportanimationdialog.h"
// Cosmic redesigned dialogs
#include "cosmicexportimagepopup.h"
#include "cosmicexportmoviepopup.h"
#include "cosmicexportanimationpopup.h"
#include "cosmicexportspritepopup.h"
#include "cosmicaboutpopup.h"
#include "cosmiccheckupdatespopup.h"
#include "cosmicexportgifpopup.h"
#include "gifexporter.h"
#include "gifexportdesc.h"

#include "cosmicpopupwindow.h"
#include "cosmicwindowheader.h"
#include "../CUI/cosmicbutton.h"
#include "cosmicwarningpopup.h"
#include "cosmicconfirmpopup.h"


ActionCommands::ActionCommands(QWidget* parent) : QObject(parent)
{
    mParent = parent;
}

ActionCommands::~ActionCommands() {}

Status ActionCommands::importAnimatedImage()
{
    ImportImageSeqDialog fileDialog(mParent, ImportExportDialog::Import, FileType::ANIMATED_IMAGE);
    fileDialog.exec();
    if (fileDialog.result() != QDialog::Accepted)
    {
        return Status::CANCELED;
    }
    int frameSpacing = fileDialog.getSpace();
    QString strImgFileLower = fileDialog.getFilePath();

    ImportPositionDialog positionDialog(mEditor, mParent);
    positionDialog.exec();
    if (positionDialog.result() != QDialog::Accepted)
    {
        return Status::CANCELED;
    }

    // Show a progress dialog, as this could take a while if the gif is huge
    CosmicProgressPopup progressDialog(mParent);
    progressDialog.setTitle(tr("Importing Animated Imageâ¦"));
    progressDialog.setRange(0, 100);
    progressDialog.setCancelEnabled(true);
    progressDialog.showCentered();

    Status st = mEditor->importAnimatedImage(strImgFileLower, frameSpacing, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
    }, [&progressDialog]() {
        return progressDialog.wasCanceled();
    });

    progressDialog.setValue(100);
    progressDialog.close();

    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html());
        errorDialog.exec();
        return Status::SAFE;
    }

    return Status::OK;
}

Status ActionCommands::importMovieVideo()
{
    QString filePath = FileDialog::getOpenFileName(mParent, FileType::MOVIE);
    if (filePath.isEmpty())
    {
        return Status::FAIL;
    }

    // Show a progress dialog, as this can take a while if you have lots of images.
    CosmicProgressPopup progressDialog(mParent);
    progressDialog.setTitle(tr("Importing movieâ¦"));
    progressDialog.setRange(0, 100);
    progressDialog.setCancelEnabled(true);
    progressDialog.showCentered();

    QMessageBox information(mParent);
    information.setIcon(QMessageBox::Warning);
    information.setText(tr("You are importing a lot of frames, beware this could take some time. Are you sure you want to proceed?"));
    information.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    information.setDefaultButton(QMessageBox::Yes);

    MovieImporter importer(this);
    importer.setCore(mEditor);

    connect(&progressDialog, &CosmicProgressPopup::canceled, &importer, &MovieImporter::cancel);

    Status st = importer.run(filePath, mEditor->playback()->fps(), FileType::MOVIE, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [&progressDialog](QString progMessage) {
        progressDialog.setStatus(progMessage);
    }, [&information]() {

        int ret = information.exec();
        return ret == QMessageBox::Yes;
    });

    if (!st.ok() && st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
        return Status::SAFE;
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();

    progressDialog.setValue(100);
    progressDialog.close();

    return Status::OK;
}

Status ActionCommands::importSound(FileType type)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        Q_ASSERT(layer);
        return Status::FAIL;
    }

    if (layer->type() != Layer::SOUND)
    {
        QMessageBox msg;
        msg.setText(tr("No sound layer exists as a destination for your import. Create a new sound layer?"));
        msg.addButton(tr("Create sound layer"), QMessageBox::AcceptRole);
        msg.addButton(tr("Don't create layer"), QMessageBox::RejectRole);

        int buttonClicked = msg.exec();
        if (buttonClicked != QMessageBox::AcceptRole)
        {
            return Status::SAFE;
        }

        // Create new sound layer.
        bool ok = false;
        QString strLayerName = QInputDialog::getText(mParent, tr("Layer Properties", "Dialog title on creating a sound layer"),
                                                     tr("Layer name:"), QLineEdit::Normal,
                                                     mEditor->layers()->nameSuggestLayer(tr("Sound Layer", "Default name on creating a sound layer")), &ok);
        if (ok && !strLayerName.isEmpty())
        {
            Layer* newLayer = mEditor->layers()->createSoundLayer(strLayerName);
            mEditor->layers()->setCurrentLayer(newLayer);
        }
        else
        {
            return Status::SAFE;
        }
    }

    layer = mEditor->layers()->currentLayer();
    Q_ASSERT(layer->type() == Layer::SOUND);

    // Adding key before getting file name just to make sure the keyframe can be insterted
    SoundClip* key = static_cast<SoundClip*>(mEditor->addNewKey());

    if (key == nullptr)
    {
        // Probably tried to modify a hidden layer or something like that
        // Let Editor handle the warnings
        return Status::SAFE;
    }

    QString strSoundFile = FileDialog::getOpenFileName(mParent, type);

    Status st = Status::FAIL;

    if (strSoundFile.isEmpty())
    {
        st = Status::CANCELED;
    }
    else
    {
        // Convert even if it already is a WAV file to strip metadata that the
        // DirectShow media player backend on Windows can't handle
        st = convertSoundToWav(strSoundFile);
    }

    if (!st.ok())
    {
        mEditor->removeKey();
        emit mEditor->layers()->currentLayerChanged(mEditor->layers()->currentLayerIndex()); // trigger timeline repaint.
    } else {
        showSoundClipWarningIfNeeded();
    }

    return st;
}

Status ActionCommands::convertSoundToWav(const QString& filePath)
{
    CosmicProgressPopup progressDialog(mParent);
    progressDialog.setTitle(tr("Importing soundâ¦"));
    progressDialog.setRange(0, 100);
    progressDialog.setCancelEnabled(true);
    progressDialog.showCentered();

    MovieImporter importer(this);
    importer.setCore(mEditor);

    Status st = importer.run(filePath, mEditor->playback()->fps(), FileType::SOUND, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [](QString progressMessage) {
        Q_UNUSED(progressMessage)
        // Not needed
    }, []() {
        return true;
    });

    connect(&progressDialog, &CosmicProgressPopup::canceled, &importer, &MovieImporter::cancel);

    if (!st.ok() && st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }
    return st;
}

Status ActionCommands::exportGif()
{
    // 1. Gather camera and animation info
    LayerCamera* cameraLayer = nullptr;
    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer && currLayer->type() == Layer::CAMERA)
    {
        cameraLayer = static_cast<LayerCamera*>(currLayer);
    }
    else
    {
        auto cameraLayers = mEditor->object()->getLayersByType<LayerCamera>();
        if (!cameraLayers.empty())
            cameraLayer = cameraLayers.front();
    }

    if (!cameraLayer)
    {
        QMessageBox::warning(mParent, tr("Warning"), tr("No camera layer found."), QMessageBox::Ok);
        return Status::FAIL;
    }

    const QSize cameraSize = cameraLayer->getViewSize();
    const int   animLength = mEditor->layers()->animationLength(false);
    const int   fps        = mEditor->playback()->fps();

    // 2. Create and exec CosmicExportGifPopup
    auto* dialog = new CosmicExportGifPopup(mParent);
    OnScopeExit(dialog->deleteLater());

    dialog->setResolution(cameraSize);
    dialog->setDefaultRange(1, animLength, fps);
    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
        return Status::SAFE;

    // 3. Build GifExportDesc from popup result
    GifExportDesc desc;
    desc.outputPath   = dialog->filePath();
    desc.size         = dialog->exportSize();
    desc.startFrame   = dialog->startFrame();
    desc.endFrame     = dialog->endFrame();
    desc.fps          = fps;
    desc.loop         = dialog->loop();
    desc.transparency = dialog->transparency();

    // 4. Show CosmicProgressPopup
    CosmicProgressPopup progress(mParent);
    progress.setTitle(tr("Exporting GIF\u2026"));
    progress.setRange(0, 100);
    progress.setCancelEnabled(true);
    progress.showCentered();

    // 5. Run GifExporter (synchronous; UI stays responsive via processEvents in callbacks)
    GifExporter exporter;
    connect(&progress, &CosmicProgressPopup::canceled, &exporter, &GifExporter::cancel);

    Status st = exporter.run(
        mEditor->object(),
        desc,
        cameraLayer,
        [&progress](int percent)
        {
            progress.setValue(percent);
            QApplication::processEvents();
        },
        [&progress]() -> bool
        {
            return progress.wasCanceled();
        }
    );

    progress.close();

    if (st.ok())
    {
        // 6. Persist last save path
        FileDialog::setLastSavePath(FileType::GIF, desc.outputPath);

        // 7. Offer to open file location
        CosmicConfirmPopup confirm(mParent);
        confirm.setBodyText(tr("GIF exported successfully. Open the file location?"));
        confirm.setConfirmText(tr("Open Folder"));
        confirm.setCancelText(tr("Close"));
        if (confirm.exec() == QDialog::Accepted)
        {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(desc.outputPath).absolutePath()));
        }
    }
    else if (st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }

    return st;
}

Status ActionCommands::exportMovie(bool isGif)
{
    FileType fileType = (isGif) ? FileType::GIF : FileType::MOVIE;

    int clipCount = mEditor->sound()->soundClipCount();
    if (fileType == FileType::MOVIE && clipCount >= MovieExporter::MAX_SOUND_FRAMES)
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.").arg(clipCount).arg(MovieExporter::MAX_SOUND_FRAMES), QString(), mParent);
        errorDialog.exec();
        return Status::FAIL;
    }

    CosmicExportMoviePopup* dialog = new CosmicExportMoviePopup(mParent, fileType);
    OnScopeExit(dialog->deleteLater());

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        if (it != camerasInfo.end())
            std::swap(camerasInfo[0], *it);
    }

    dialog->setCamerasInfo(camerasInfo);

    int lengthWithSounds = mEditor->layers()->animationLength(true);
    int length = mEditor->layers()->animationLength(false);

    if (!isGif && mEditor->playback()->isRangedPlaybackOn()) {
        dialog->setDefaultRange(mEditor->playback()->startFrame(),
                                mEditor->playback()->endFrame(), lengthWithSounds);
    } else {
        dialog->setDefaultRange(1, length, lengthWithSounds);
    }
    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }
    QString strMoviePath = dialog->filePath();

    ExportMovieDesc desc;
    desc.strFileName = strMoviePath;
    desc.startFrame = dialog->startFrame();
    desc.endFrame = dialog->endFrame();
    desc.fps = mEditor->playback()->fps();
    desc.exportSize = dialog->exportSize();
    desc.strCameraName = dialog->selectedCameraName();
    desc.loop = dialog->loop();
    desc.alpha = dialog->transparency();

    DoubleProgressDialog progressDlg(mParent);
    progressDlg.setWindowModality(Qt::WindowModal);
    progressDlg.setWindowTitle(tr("Exporting movie"));
    Qt::WindowFlags eFlags = Qt::Dialog | Qt::WindowTitleHint;
    progressDlg.setWindowFlags(eFlags);
    progressDlg.show();

    MovieExporter ex;

    connect(&progressDlg, &DoubleProgressDialog::canceled, [&ex]
    {
        ex.cancel();
    });

    // The start points and length for the current minor operation segment on the major progress bar
    float minorStart, minorLength;

    Status st = ex.run(mEditor->object(), desc,
        [&progressDlg, &minorStart, &minorLength](float f, float final)
        {
            progressDlg.major->setValue(f);

            minorStart = f;
            minorLength = qMax(0.f, final - minorStart);

            QApplication::processEvents();
        },
        [&progressDlg, &minorStart, &minorLength](float f) {
            progressDlg.minor->setValue(f);

            progressDlg.major->setValue(minorStart + f * minorLength);

            QApplication::processEvents();
        },
        [&progressDlg](QString s) {
            progressDlg.setStatus(s);
            QApplication::processEvents();
        }
    );

    if (st.ok())
    {
        if (QFile::exists(strMoviePath))
        {
            if (isGif) {
                auto btn = QMessageBox::question(mParent, "Nurture2D",
                                                 tr("Finished. Open file location?"));

                if (btn == QMessageBox::Yes)
                {
                    QString path = QFileInfo(dialog->filePath()).absolutePath();
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                }
                return Status::OK;
            }
            auto btn = QMessageBox::question(mParent, "Nurture2D",
                                             tr("Finished. Open movie now?", "When movie export done."));
            if (btn == QMessageBox::Yes)
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(strMoviePath));
            }
        }
        else
        {
            ErrorDialog errorDialog(tr("Unknown export error"), tr("The export did not produce any errors, however we can't find the output file. Your export may not have completed successfully."), QString(), mParent);
            errorDialog.exec();
        }
    }
    else if(st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }

    return st;
}

Status ActionCommands::exportImageSequence()
{
    // Create dialog with simplified UI (format, transparency, frame range only)
    auto dialog = new CosmicExportImagePopup(mParent, FileType::IMAGE_SEQUENCE);
    OnScopeExit(dialog->deleteLater());

    dialog->exec();
    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }

    // Read dialog settings
    const QString strFilePath = dialog->filePath();
    const QString exportFormat = dialog->exportFormat();
    const bool useTranparency = dialog->transparency();

    // Always export full timeline range (animation length without sounds)
    const int startFrame = 1;
    const int endFrame  = mEditor->layers()->animationLength(false);

    // Validate active layer
    Layer* activeLayer = mEditor->layers()->currentLayer();
    if (!activeLayer)
    {
        QMessageBox::warning(mParent, tr("Warning"), tr("No active layer."), QMessageBox::Ok);
        return Status::FAIL;
    }
    if (activeLayer->type() != Layer::BITMAP && activeLayer->type() != Layer::VECTOR)
    {
        QMessageBox::warning(mParent,
                             tr("Warning"),
                             tr("Cannot export animation for this layer type. Only bitmap and vector layers can be exported."),
                             QMessageBox::Ok);
        return Status::FAIL;
    }
    const QString layerName = activeLayer->name();

    // Determine camera layer and export size from camera view size
    LayerCamera* cameraLayer = nullptr;
    if (activeLayer->type() == Layer::CAMERA)
    {
        cameraLayer = static_cast<LayerCamera*>(activeLayer);
    }
    else
    {
        auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
        if (!cameraLayers.empty())
        {
            cameraLayer = cameraLayers.front();
        }
    }
    if (!cameraLayer)
    {
        QMessageBox::warning(mParent, tr("Warning"), tr("No camera layer found."), QMessageBox::Ok);
        return Status::FAIL;
    }
    const QSize exportSize = cameraLayer->getViewSize();

    CosmicProgressPopup progress(mParent);
    progress.setTitle(tr("Exporting image sequenceâ¦"));
    progress.setRange(0, 100);
    progress.setCancelEnabled(true);
    progress.showCentered();

    const bool exportSpecific = dialog->isExportSpecificEnabled();
    const auto mode = dialog->exportSpecificMode();

    if (!exportSpecific || activeLayer->type() != Layer::BITMAP)
    {
        // Default behavior or non-bitmap: full or keyframes-only export based on mode
        const bool exportKeyframesOnly = exportSpecific && (mode == CosmicExportImagePopup::ExportSpecificMode::Keyframes);

        mEditor->object()->exportFramesForLayer(startFrame,
                                                endFrame,
                                                cameraLayer,
                                                exportSize,
                                                strFilePath,
                                                exportFormat,
                                                useTranparency,
                                                exportKeyframesOnly,
                                                layerName,
                                                true,
                                                [&progress](int v){ progress.setValue(v); },
                                                [&progress](){ return progress.wasCanceled(); },
                                                100);
    }
    else
    {
        // Cells Only for active bitmap layer
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(activeLayer);
        CellBank* bank = bitmapLayer->cellBank();

        if (!bank)
        {
            progress.close();
            return Status::OK;
        }

        // Derive output directory and base name from file path
        QFileInfo info(strFilePath);
        QDir outDir(info.absolutePath());
        const QString baseName = layerName;

        QList<int> cellIds = bank->getAllCellIds();
        if (cellIds.isEmpty())
        {
            progress.close();
            return Status::OK;
        }

        const int totalCells = cellIds.size();
        int exported = 0;

        // Iterate keyframes to find first frame where each cellId appears
        QSet<int> exportedIds;
        bitmapLayer->foreachKeyFrame([&](KeyFrame* kf){
            BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
            if (!bkf)
            {
                return;
            }

            const int cellId = bkf->cellId();
            if (cellId <= 0 || exportedIds.contains(cellId))
            {
                return;
            }

            exportedIds.insert(cellId);

            const int cellIndex = bank->indexForCellId(cellId);
            QString indexStr;
            if (cellIndex > 0)
            {
                indexStr = QString::number(cellIndex).rightJustified(3, QLatin1Char('0'));
            }
            else
            {
                indexStr = QString::number(cellId);
            }

            QString filePrefix = outDir.filePath(baseName + QLatin1Char('_') + indexStr);

            const int framePos = kf->pos();
            QTransform view = cameraLayer->getViewAtFrame(framePos);
            const QSize camSize = cameraLayer->getViewSize();

            // Render this layer at the first occurrence frame for this cellId
            // Reuse Object::exportImForLayer-like behavior
            QImage imageToExport(exportSize, QImage::Format_ARGB32_Premultiplied);
            QColor bgColor = Qt::white;
            if (useTranparency)
            {
                bgColor.setAlpha(0);
            }
            imageToExport.fill(bgColor);

            QTransform centralizeCamera;
            centralizeCamera.translate(camSize.width() / 2, camSize.height() / 2);

            QPainter painter(&imageToExport);
            painter.setWorldTransform(view * centralizeCamera);
            painter.setWindow(QRect(0, 0, camSize.width(), camSize.height()));

            // Draw only this bitmap layer at the given frame
            BitmapImage* bitmap = bitmapLayer->getLastBitmapImageAtFrame(framePos);
            if (bitmap)
            {
                painter.setOpacity(bitmap->getOpacity());
                bitmap->paintImage(painter);
            }

            QString fmt = exportFormat;
            QString ext = ".png";
            if (fmt.compare("PNG", Qt::CaseInsensitive) == 0) { fmt = "PNG"; ext = ".png"; }
            else if (fmt.compare("JPG", Qt::CaseInsensitive) == 0 || fmt.compare("JPEG", Qt::CaseInsensitive) == 0) { fmt = "JPG"; ext = ".jpg"; }
            else if (fmt.compare("TIFF", Qt::CaseInsensitive) == 0 || fmt.compare("TIF", Qt::CaseInsensitive) == 0) { fmt = "TIFF"; ext = ".tiff"; }
            else if (fmt.compare("BMP", Qt::CaseInsensitive) == 0) { fmt = "BMP"; ext = ".bmp"; }
            else if (fmt.compare("WEBP", Qt::CaseInsensitive) == 0) { fmt = "WEBP"; ext = ".webp"; }

            const QString outPath = filePrefix + ext;
            imageToExport.save(outPath, fmt.toStdString().c_str());

            ++exported;
            int progressValue = (exported * 100) / totalCells;
            progress.setValue(progressValue);
            QApplication::processEvents();
            if (progress.wasCanceled())
            {
                return;
            }
        });
    }

    progress.close();

    return Status::OK;
}

Status ActionCommands::exportImage()
{
    // Options
    auto dialog = new CosmicExportImagePopup(mParent, FileType::IMAGE);
    OnScopeExit(dialog->deleteLater())

    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }

    QString filePath = dialog->filePath();
    QString exportFormat = dialog->exportFormat();
    bool useTranparency = dialog->transparency();

    // Export
    LayerCamera* cameraLayer = nullptr;
    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer && currLayer->type() == Layer::CAMERA) {
        cameraLayer = static_cast<LayerCamera*>(currLayer);
    } else {
        auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
        if (!cameraLayers.empty()) {
            cameraLayer = cameraLayers.front();
        }
    }
    if (!cameraLayer) {
        QMessageBox::warning(mParent,
                             tr("Warning"),
                             tr("No camera layer found."),
                             QMessageBox::Ok);
        return Status::FAIL;
    }

    QTransform view = cameraLayer->getViewAtFrame(mEditor->currentFrame());

    bool bOK = mEditor->object()->exportIm(mEditor->currentFrame(),
                                           view,
                                           cameraLayer->getViewSize(),
                                           cameraLayer->getViewSize(),
                                           filePath,
                                           exportFormat,
                                           true,
                                           useTranparency);

    if (!bOK)
    {
        QMessageBox::warning(mParent,
                             tr("Warning"),
                             tr("Unable to export image."),
                             QMessageBox::Ok);
        return Status::FAIL;
    }
    return Status::OK;
}

Status ActionCommands::exportAnimation()
{
    // Create and show dialog
    auto dialog = new CosmicExportAnimationPopup(mParent);
    OnScopeExit(dialog->deleteLater());

    // Determine camera layer (use current if camera, otherwise first available)
    LayerCamera* cameraLayer = nullptr;
    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer && currLayer->type() == Layer::CAMERA) {
        cameraLayer = static_cast<LayerCamera*>(currLayer);
    } else {
        auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
        if (!cameraLayers.empty()) {
            cameraLayer = cameraLayers.front();
        }
    }

    // Set resolution to current camera resolution when the menu opens
    if (cameraLayer) {
        dialog->setResolution(cameraLayer->getViewSize());
    }

    dialog->exec();
    if (dialog->result() == QDialog::Rejected) {
        return Status::SAFE;
    }

    // Read settings
    const QString baseFolderName = dialog->parentFolderName().trimmed();
    const QString destRoot = dialog->destDir();
    const QString exportFormat = dialog->exportFormat();
    const bool useTransparency = dialog->transparency();
    // Always export full timeline range (without sounds)
    const int startFrame = 1;
    const int endFrame = mEditor->layers()->animationLength(false);
    const QSize exportSize = dialog->exportSize();
    const bool isActiveLayerMode = dialog->isActiveLayerMode();
    const bool createParentFolder = dialog->shouldCreateParentFolder();
    const QString parentFolderName = dialog->parentFolderName().trimmed();

    // Ensure we have a camera layer
    if (!cameraLayer) {
        QMessageBox::warning(mParent,
                             tr("Warning"),
                             tr("No camera layer found."),
                             QMessageBox::Ok);
        return Status::FAIL;
    }

    // Prepare output directories based on toggle states
    QDir outRoot(destRoot);
    if (!outRoot.exists()) { outRoot.mkpath("."); }
    QString baseOutPath;
    if (createParentFolder && !parentFolderName.isEmpty()) {
        baseOutPath = outRoot.filePath(parentFolderName);
    } else if (createParentFolder && parentFolderName.isEmpty() && !baseFolderName.isEmpty()) {
        // Fallback: use legacy baseFolderName if provided
        baseOutPath = outRoot.filePath(baseFolderName);
    } else {
        baseOutPath = destRoot;
    }
    QDir().mkpath(baseOutPath);

    CosmicProgressPopup progress(mParent);
    progress.setTitle(tr("Exporting animationâ¦"));
    progress.setRange(0, 100);
    progress.setCancelEnabled(true);
    progress.showCentered();

    // Collect layers to export (bitmap only for new export modes; vectors allowed only for full/keyframe modes)
    QList<Layer*> layers;
    Layer* activeLayer = nullptr;
    if (isActiveLayerMode) {
        activeLayer = mEditor->layers()->currentLayer();
        if (!activeLayer || activeLayer->type() != Layer::BITMAP) {
            QMessageBox::warning(mParent,
                                 tr("Warning"),
                                 tr("Cannot export: Active layer must be a bitmap layer."),
                                 QMessageBox::Ok);
            return Status::FAIL;
        }
        layers.append(activeLayer);
    } else {
        for (int i = 0; i < mEditor->object()->getLayerCount(); ++i) {
            Layer* layer = mEditor->object()->getLayer(i);
            if (!layer) continue;
            if (layer->type() == Layer::BITMAP) {
                layers.append(layer);
            }
        }
    }

    const int perLayerMax = layers.isEmpty() ? 100 : 100 / layers.size();
    int accumulated = 0;

    const bool exportSpecific = dialog->isExportSpecificEnabled();
    const auto mode = dialog->exportSpecificMode();

    for (Layer* layer : layers) {
        const QString layerFolderName = layer->name();
        const QString layerFolderPath = QDir(baseOutPath).filePath(layerFolderName);
        QDir().mkpath(layerFolderPath);
        const QString filePrefix = QDir(layerFolderPath).filePath(layerFolderName + "_");

        if (!exportSpecific)
        {
            // Full range export for this bitmap layer
            mEditor->object()->exportFramesForLayer(startFrame,
                                                    endFrame,
                                                    cameraLayer,
                                                    exportSize,
                                                    filePrefix,
                                                    exportFormat,
                                                    useTransparency,
                                                    false,
                                                    layerFolderName,
                                                    true,
                                                    [&progress](int v){ progress.setValue(v); },
                                                    [&progress](){ return progress.wasCanceled(); },
                                                    perLayerMax);
        }
        else if (mode == CosmicExportAnimationPopup::ExportSpecificMode::Keyframes)
        {
            // Keyframes-only export (one image per keyframe frame, named by frame index)
            mEditor->object()->exportFramesForLayer(startFrame,
                                                    endFrame,
                                                    cameraLayer,
                                                    exportSize,
                                                    filePrefix,
                                                    exportFormat,
                                                    useTransparency,
                                                    true,
                                                    layerFolderName,
                                                    true,
                                                    [&progress](int v){ progress.setValue(v); },
                                                    [&progress](){ return progress.wasCanceled(); },
                                                    perLayerMax);
        }
        else
        {
            // Cells Only for this bitmap layer
            LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
            CellBank* bank = bitmapLayer->cellBank();
            if (!bank)
            {
                accumulated += perLayerMax;
                progress.setValue(accumulated);
                QApplication::processEvents();
                if (progress.wasCanceled()) break;
                continue;
            }

            QList<int> cellIds = bank->getAllCellIds();
            if (cellIds.isEmpty())
            {
                accumulated += perLayerMax;
                progress.setValue(accumulated);
                QApplication::processEvents();
                if (progress.wasCanceled()) break;
                continue;
            }

            const int totalCells = cellIds.size();
            int exported = 0;

            QSet<int> exportedIds;
            bitmapLayer->foreachKeyFrame([&](KeyFrame* kf){
                if (progress.wasCanceled())
                {
                    return;
                }

                BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
                if (!bkf)
                {
                    return;
                }

                const int cellId = bkf->cellId();
                if (cellId <= 0 || exportedIds.contains(cellId))
                {
                    return;
                }

                exportedIds.insert(cellId);

                const int cellIndex = bank->indexForCellId(cellId);
                QString indexStr;
                if (cellIndex > 0)
                {
                    indexStr = QString::number(cellIndex).rightJustified(3, QLatin1Char('0'));
                }
                else
                {
                    indexStr = QString::number(cellId);
                }

                QString cellFilePrefix = QDir(layerFolderPath).filePath(layerFolderName + QLatin1Char('_') + indexStr);

                const int framePos = kf->pos();
                QTransform view = cameraLayer->getViewAtFrame(framePos);
                const QSize camSize = cameraLayer->getViewSize();

                QImage imageToExport(exportSize, QImage::Format_ARGB32_Premultiplied);
                QColor bgColor = Qt::white;
                if (useTransparency)
                {
                    bgColor.setAlpha(0);
                }
                imageToExport.fill(bgColor);

                QTransform centralizeCamera;
                centralizeCamera.translate(camSize.width() / 2, camSize.height() / 2);

                QPainter painter(&imageToExport);
                painter.setWorldTransform(view * centralizeCamera);
                painter.setWindow(QRect(0, 0, camSize.width(), camSize.height()));

                BitmapImage* bitmap = bitmapLayer->getLastBitmapImageAtFrame(framePos);
                if (bitmap)
                {
                    painter.setOpacity(bitmap->getOpacity());
                    bitmap->paintImage(painter);
                }

                QString fmt = exportFormat;
                QString ext = ".png";
                if (fmt.compare("PNG", Qt::CaseInsensitive) == 0) { fmt = "PNG"; ext = ".png"; }
                else if (fmt.compare("JPG", Qt::CaseInsensitive) == 0 || fmt.compare("JPEG", Qt::CaseInsensitive) == 0) { fmt = "JPG"; ext = ".jpg"; }
                else if (fmt.compare("TIFF", Qt::CaseInsensitive) == 0 || fmt.compare("TIF", Qt::CaseInsensitive) == 0) { fmt = "TIFF"; ext = ".tiff"; }
                else if (fmt.compare("BMP", Qt::CaseInsensitive) == 0) { fmt = "BMP"; ext = ".bmp"; }
                else if (fmt.compare("WEBP", Qt::CaseInsensitive) == 0) { fmt = "WEBP"; ext = ".webp"; }

                const QString outPath = cellFilePrefix + ext;
                imageToExport.save(outPath, fmt.toStdString().c_str());

                ++exported;
                int localProgress = (exported * perLayerMax) / totalCells;
                progress.setValue(accumulated + localProgress);
                QApplication::processEvents();
            });

            accumulated += perLayerMax;
        }

        progress.setValue(accumulated);
        QApplication::processEvents();
        if (progress.wasCanceled()) break;
    }

    progress.close();

    return Status::OK;
}

void ActionCommands::flipSelectionX()
{
    bool flipVertical = false;
    mEditor->flipSelection(flipVertical);
}

void ActionCommands::flipSelectionY()
{
    bool flipVertical = true;
    mEditor->flipSelection(flipVertical);
}

void ActionCommands::selectAll()
{
    mEditor->selectAll();
}

void ActionCommands::deselectAll()
{
    mEditor->deselectAll();
}

void ActionCommands::freeTransformSelectionMode()
{
    auto sel = mEditor->select();
    if (!sel->somethingSelected()) {
        return;
    }
    // Set pivot to center and enable rotation mode by default
    sel->setTransformAnchor(sel->mySelectionRect().center());
    sel->setMoveMode(MoveMode::ROTATION);
    sel->calculateSelectionTransformation();
    emit mEditor->frameModified(mEditor->currentFrame());
}

void ActionCommands::ZoomIn()
{
    mEditor->view()->scaleUp();
}

void ActionCommands::ZoomOut()
{
    mEditor->view()->scaleDown();
}

void ActionCommands::rotateClockwise()
{
    // Rotation direction is inverted if view is flipped either vertically or horizontally
    const float delta = mEditor->view()->isFlipHorizontal() == !mEditor->view()->isFlipVertical() ? -15.f : 15.f;
    mEditor->view()->rotateRelative(delta);
}

void ActionCommands::rotateCounterClockwise()
{
    // Rotation direction is inverted if view is flipped either vertically or horizontally
    const float delta = mEditor->view()->isFlipHorizontal() == !mEditor->view()->isFlipVertical() ? 15.f : -15.f;
    mEditor->view()->rotateRelative(delta);
}

void ActionCommands::PlayStop()
{
    PlaybackManager* playback = mEditor->playback();
    if (playback->isPlaying())
    {
        playback->stop();
    }
    else
    {
        playback->play();
    }
}

void ActionCommands::GotoNextFrame()
{
    mEditor->scrubNextKeyFrame();
}

void ActionCommands::GotoPrevFrame()
{
    mEditor->scrubPreviousKeyFrame();
}

void ActionCommands::GotoNextKeyFrame()
{
    mEditor->scrubNextKeyFrame();
}

void ActionCommands::GotoPrevKeyFrame()
{
    mEditor->scrubPreviousKeyFrame();
}

Status ActionCommands::addNewKey()
{
    // Sound keyframes should not be empty, so we try to import a sound instead
    if (mEditor->layers()->currentLayer()->type() == Layer::SOUND)
    {
        return importSound(FileType::SOUND);
    }

    KeyFrame* key = mEditor->addNewKey();
    Camera* cam = dynamic_cast<Camera*>(key);
    if (cam)
    {
        mEditor->view()->forceUpdateViewTransform();
    }

    return Status::OK;
}

void ActionCommands::exposeSelectedFrames(int offset)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();

    bool hasSelectedFrames = currentLayer->hasAnySelectedFrames();

    // Functionality to be able to expose the current frame without selecting
    // A:
    KeyFrame* key = currentLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    if (!hasSelectedFrames) {

        if (key == nullptr) { return; }
        currentLayer->setFrameSelected(key->pos(), true);
    }

    currentLayer->setExposureForSelectedFrames(offset);
    emit mEditor->updateTimeLine();
    emit mEditor->framesModified();
    mEditor->updateUndoContext();

    // Remember to deselect frame again so we don't show it being visually selected.
    // B:
    if (!hasSelectedFrames) {
        currentLayer->setFrameSelected(key->pos(), false);
    }
}

void ActionCommands::addExposureToSelectedFrames()
{
    exposeSelectedFrames(1);
}

void ActionCommands::subtractExposureFromSelectedFrames()
{
    exposeSelectedFrames(-1);
}

Status ActionCommands::insertKeyFrameAtCurrentPosition()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    int currentPosition = mEditor->currentFrame();

    // CSP-style spacing: only when the playhead is sitting exactly on a keyframe.
    // Compute the interval between that keyframe and its predecessor, then place
    // the new key that same distance ahead.  Falls back to currentPosition when
    // on a hold frame (no exact key) so the user can opt out simply by scrubbing
    // to a hold frame before pressing Add.
    int targetPosition = currentPosition;
    if (currentLayer->keyExists(currentPosition))
    {
        const int prev = currentLayer->getPreviousKeyFramePosition(currentPosition);
        if (prev > 0 && prev < currentPosition)
        {
            const int interval = currentPosition - prev;
            targetPosition = currentPosition + interval;
        }
    }

    currentLayer->insertExposureAt(currentPosition);
    mEditor->updateUndoContext();

    // Temporarily scrub to the computed target so addNewKey places the frame there.
    const int savedFrame = mEditor->currentFrame();
    if (targetPosition != savedFrame)
        mEditor->scrubTo(targetPosition);

    Status st = addNewKey();

    // addNewKey/addKeyFrame scrubs to the new frame itself, so no need to restore.
    return st;
}

void ActionCommands::removeSelectedFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();

    // Guard: Check if we have a valid layer and selected frames
    if (!currentLayer || !currentLayer->hasAnySelectedFrames()) { return; }

    // Get selected positions before removal
    QList<int> selectedPositions = currentLayer->selectedKeyFramesPositions();
    
    // Use macro to group multiple removals into single undo action
    // Begin macro if multiple selections
    if (mEditor->undoRedo() && selectedPositions.size() > 1) {
        mEditor->undoRedo()->undoStack()->beginMacro(tr("Remove Frames"));
    }

    // Iterate backwards safely
    for (int i = selectedPositions.size() - 1; i >= 0; --i)
    {
        int pos = selectedPositions[i];
        if (currentLayer->keyExists(pos))
        {
            if (mEditor->undoRedo()) {
                RemoveKeyFrameCommand* removeCommand = new RemoveKeyFrameCommand(
                    pos,
                    currentLayer->id(),
                    mEditor
                );
                mEditor->undoRedo()->pushCommand(removeCommand);
            } else {
                 currentLayer->removeKeyFrame(pos);
            }
        }
    }

    if (mEditor->undoRedo() && selectedPositions.size() > 1)
    {
        mEditor->undoRedo()->undoStack()->endMacro();
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    mEditor->layers()->notifyLayerChanged(currentLayer);
    emit mEditor->updateTimeLine();
}

void ActionCommands::reverseSelectedFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();

    if (!currentLayer->reverseOrderOfSelection()) {
        return;
    }

    if (currentLayer->type() == Layer::CAMERA) {
        mEditor->view()->forceUpdateViewTransform();
    }
    emit mEditor->framesModified();
};

void ActionCommands::removeKey()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    
    // Guard: Check if we have a valid layer
    if (!currentLayer) {
        return;
    }
    
    // If multiple keyframes are selected, remove all of them
    if (currentLayer->hasAnySelectedFrames()) {
        removeSelectedFrames();
        return;
    }
    
    // Otherwise, remove the keyframe at the current position
    mEditor->removeKey();
}

void ActionCommands::duplicateLayer()
{
    LayerManager* layerMgr = mEditor->layers();
    Layer* fromLayer = layerMgr->currentLayer();
    int currFrame = mEditor->currentFrame();

    Layer* toLayer = layerMgr->createLayer(fromLayer->type(), tr("%1 (copy)", "Default duplicate layer name").arg(fromLayer->name()));
    fromLayer->foreachKeyFrame([&] (KeyFrame* key) {
        key = key->clone();
        toLayer->addOrReplaceKeyFrame(key->pos(), key);
        if (toLayer->type() == Layer::SOUND)
        {
            mEditor->sound()->processSound(static_cast<SoundClip*>(key));
        }
    });
    if (!fromLayer->keyExists(1)) {
        toLayer->removeKeyFrame(1);
    }
    mEditor->scrubTo(currFrame);
}

void ActionCommands::duplicateKey()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) return;
    if (!layer->visible())
    {
        mEditor->getScribbleArea()->showLayerNotVisibleWarning();
        return;
    }

    KeyFrame* key = layer->getKeyFrameAt(mEditor->currentFrame());
    if (key == nullptr) return;

    // Duplicating a selected keyframe is not handled properly.
    // The desired behavior is to clear selection anyway so we just do that.
    deselectAll();

    int nextEmptyFrame = mEditor->currentFrame() + 1;
    while (layer->keyExistsWhichCovers(nextEmptyFrame))
    {
        nextEmptyFrame += 1;
    }

    if (mEditor->undoRedo()) {
        DuplicateKeyFrameCommand* cmd = new DuplicateKeyFrameCommand(mEditor->currentFrame(), nextEmptyFrame, layer->id(), mEditor);
        mEditor->undoRedo()->pushCommand(cmd);
    } else {
        // Fallback
        KeyFrame* dupKey = key->clone();
        layer->addKeyFrame(nextEmptyFrame, dupKey);
        mEditor->scrubTo(nextEmptyFrame);
        emit mEditor->frameModified(nextEmptyFrame);
    }

    if (layer->type() == Layer::SOUND)
    {
        // Sound processing might strictly need the key to exist first if done this way?
        // Command execution (redo) happens immediately on push, so it's safe.
        KeyFrame* newKey = layer->getKeyFrameAt(nextEmptyFrame);
        if (newKey) {
             mEditor->sound()->processSound(dynamic_cast<SoundClip*>(newKey));
             showSoundClipWarningIfNeeded();
        }
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->layers()->currentLayerChanged(mEditor->layers()->currentLayerIndex()); // trigger timeline repaint.
}

void ActionCommands::makeUniqueDrawing()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    if (!layer->visible())
    {
        mEditor->getScribbleArea()->showLayerNotVisibleWarning();
        return;
    }

    const int frame = mEditor->currentFrame();

    // Find the keyframe that covers the current position
    // (could be exactly at frame, or the last key before it for an exposure)
    int keyPos = -1;
    if (layer->keyExists(frame)) {
        keyPos = frame;
    } else {
        keyPos = layer->getPreviousKeyFramePosition(frame);
    }
    if (keyPos < 1) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(layer->getKeyFrameAt(keyPos));
    if (!bkf) return;

    const int cellId = bkf->cellId();

    // Check if this cell is already unique (only one instance)
    int instanceCount = 0;
    layer->foreachKeyFrame([&](KeyFrame* kf) {
        BitmapKeyFrame* b = dynamic_cast<BitmapKeyFrame*>(kf);
        if (b && b->cellId() == cellId) instanceCount++;
    });

    if (instanceCount <= 1)
    {
        // Already unique — nothing to do, but don't silently fail
        // (could show a tooltip/status message; for now just return)
        return;
    }

    if (mEditor->undoRedo()) {
        MakeUniqueDrawingCommand* cmd = new MakeUniqueDrawingCommand(keyPos, layer->id(), mEditor);
        mEditor->undoRedo()->pushCommand(cmd);
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->layers()->currentLayerChanged(mEditor->layers()->currentLayerIndex());
}

void ActionCommands::moveFrameForward(){
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer)
    {
        if (layer->moveKeyFrame(mEditor->currentFrame(), 1))
        {
            mEditor->scrubForward();
        }
    }
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
}

void ActionCommands::moveFrameBackward()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer)
    {
        if (layer->moveKeyFrame(mEditor->currentFrame(), -1))
        {
            mEditor->scrubBackward();
        }
    }
    emit mEditor->framesModified();
}

Status ActionCommands::addNewBitmapLayer()
{
    // Create without prompting for a name; use suggested default
    const QString name = mEditor->layers()->nameSuggestLayer(tr("Layer"));
    mEditor->layers()->createBitmapLayer(name);
    return Status::OK;
}

Status ActionCommands::addNewVectorLayer()
{
    bool ok;
    QString text = QInputDialog::getText(mParent, tr("Layer Properties"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         mEditor->layers()->nameSuggestLayer(tr("Vector Layer")), &ok);
    if (ok && !text.isEmpty())
    {
        mEditor->layers()->createVectorLayer(text);
    }
    return Status::OK;
}

Status ActionCommands::addNewCameraLayer()
{
    bool ok;
    QString text = QInputDialog::getText(mParent, tr("Layer Properties", "A popup when creating a new layer"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         mEditor->layers()->nameSuggestLayer(tr("Camera Layer")), &ok);
    if (ok && !text.isEmpty())
    {
        mEditor->layers()->createCameraLayer(text);
    }
    return Status::OK;
}

Status ActionCommands::addNewSoundLayer()
{
    bool ok = false;
    QString strLayerName = QInputDialog::getText(mParent, tr("Layer Properties"),
                                                 tr("Layer name:"), QLineEdit::Normal,
                                                 mEditor->layers()->nameSuggestLayer(tr("Sound Layer")), &ok);
    if (ok && !strLayerName.isEmpty())
    {
        Layer* layer = mEditor->layers()->createSoundLayer(strLayerName);
        mEditor->layers()->setCurrentLayer(layer);
   }
    return Status::OK;
}

Status ActionCommands::deleteCurrentLayer()
{
    LayerManager* layerMgr = mEditor->layers();
    QString strLayerName = layerMgr->currentLayer()->name();

    if (!layerMgr->canDeleteLayer(mEditor->currentLayerIndex())) {
        qDebug() << "[DeleteLayer] canDeleteLayer=false idx=" << mEditor->currentLayerIndex();
        return Status::CANCELED;
    }

    qDebug() << "[DeleteLayer] showing warning popup for layer=" << strLayerName;
    CosmicConfirmPopup popup(mParent);
    popup.setTitleText(tr("Delete Layer"));
    popup.setBodyText(tr("Are you sure you want to delete layer: %1? This cannot be undone.").arg(strLayerName));
    popup.setConfirmText(tr("Yes"));
    popup.setCancelText(tr("No"));
    popup.exec();
    qDebug() << "[DeleteLayer] popup result=" << popup.result();
    if (popup.result() == QDialog::Accepted)
    {
        Status st = layerMgr->deleteLayer(mEditor->currentLayerIndex());
        if (st == Status::ERROR_NEED_AT_LEAST_ONE_CAMERA_LAYER)
        {
            QMessageBox::information(mParent, "",
                                     tr("Please keep at least one camera layer in project", "text when failed to delete camera layer"));
        }
    }
    return Status::OK;
}

void ActionCommands::setLayerVisibilityIndex(int index)
{
    mEditor->setLayerVisibility(static_cast<LayerVisibility>(index));
}

void ActionCommands::changeKeyframeLineColor()
{
    if (mEditor->layers()->currentLayer()->type() == Layer::BITMAP &&
            mEditor->layers()->currentLayer()->keyExists(mEditor->currentFrame()))
    {
        QRgb color = mEditor->color()->frontColor().rgb();
        LayerBitmap* layer = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
        layer->getBitmapImageAtFrame(mEditor->currentFrame())->fillNonAlphaPixels(color);
        mEditor->updateFrame();
    }
}

void ActionCommands::changeallKeyframeLineColor()
{
    if (mEditor->layers()->currentLayer()->type() == Layer::BITMAP)
    {
        QRgb color = mEditor->color()->frontColor().rgb();
        LayerBitmap* layer = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
        for (int i = layer->firstKeyFramePosition(); i <= layer->getMaxKeyFramePosition(); i++)
        {
            if (layer->keyExists(i))
                layer->getBitmapImageAtFrame(i)->fillNonAlphaPixels(color);
        }
        mEditor->updateFrame();
    }
}


void ActionCommands::resetAllTools()
{
    mEditor->tools()->resetAllTools();
}

void ActionCommands::help()
{
    QString url = "http://www.pencil2d.org/doc/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::quickGuide()
{
    QString sDocPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString sCopyDest = QDir(sDocPath).filePath("pencil2d_quick_guide.pdf");

    QFile quickGuideFile(":/app/pencil2d_quick_guide.pdf");
    quickGuideFile.copy(sCopyDest);

    QDesktopServices::openUrl(QUrl::fromLocalFile(sCopyDest));
}

void ActionCommands::website()
{
    QString url = "https://www.pencil2d.org/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::forum()
{
    QString url = "https://discuss.pencil2d.org/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::discord()
{
    QString url = "https://discord.gg/8FxdV2g";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::reportbug()
{
    QString url = "https://github.com/pencil2d/pencil/issues";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::checkForUpdates()
{
    CosmicCheckUpdatesPopup popup(mParent);
    popup.startChecking();
    popup.exec();
}

// This action is a temporary measure until we have an automated recover mechanism in place
void ActionCommands::openTemporaryDirectory()
{
    int ret = QMessageBox::warning(mParent, tr("Warning"), tr("The temporary directory is meant to be used only by Pencil2D. Do not modify it unless you know what you are doing."), QMessageBox::Cancel, QMessageBox::Ok);
    if (ret == QMessageBox::Ok)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::temp().filePath("Pencil2D")));
    }
}

void ActionCommands::about()
{
    CosmicAboutPopup* popup = new CosmicAboutPopup(mParent);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->init();
    popup->exec();
}

void ActionCommands::showSoundClipWarningIfNeeded()
{
    int clipCount = mEditor->sound()->soundClipCount();
    if (clipCount >= MovieExporter::MAX_SOUND_FRAMES && !mSuppressSoundWarning) {
        QMessageBox::warning(mParent, tr("Warning"), tr("You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.").arg(clipCount).arg(MovieExporter::MAX_SOUND_FRAMES));
        mSuppressSoundWarning = true;
    } else {
        mSuppressSoundWarning = false;
    }
}

Status ActionCommands::exportSprite()
{
    auto* dialog = new CosmicExportSpritePopup(mParent);
    OnScopeExit(dialog->deleteLater());

    // Set resolution from camera layer
    LayerCamera* cameraLayer = nullptr;
    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer && currLayer->type() == Layer::CAMERA) {
        cameraLayer = static_cast<LayerCamera*>(currLayer);
    } else {
        auto cameraLayers = mEditor->object()->getLayersByType<LayerCamera>();
        if (!cameraLayers.empty())
            cameraLayer = cameraLayers.front();
    }

    if (cameraLayer)
        dialog->setResolution(cameraLayer->getViewSize());

    dialog->exec();
    if (dialog->result() == QDialog::Rejected)
        return Status::SAFE;

    if (!cameraLayer) {
        QMessageBox::warning(mParent, tr("Warning"), tr("No camera layer found."), QMessageBox::Ok);
        return Status::FAIL;
    }

    const QString destDir    = dialog->destDir().trimmed();
    const QString baseName   = dialog->fileName().trimmed().isEmpty()
                                   ? QStringLiteral("spritesheet")
                                   : dialog->fileName().trimmed();
    const QString format     = dialog->exportFormat();
    const QSize   frameSize  = dialog->frameSize();
    const bool    useTransparency = dialog->transparency();
    const CosmicExportSpritePopup::Layout layout = dialog->spriteLayout();

    const int startFrame = 1;
    const int endFrame   = mEditor->layers()->animationLength(false);
    const int frameCount = endFrame - startFrame + 1;
    if (frameCount <= 0) {
        QMessageBox::warning(mParent, tr("Warning"), tr("No frames to export."), QMessageBox::Ok);
        return Status::FAIL;
    }

    // Determine sprite sheet dimensions
    int cols = 1, rows = 1;
    if (layout == CosmicExportSpritePopup::Layout::Horizontal) {
        cols = frameCount;
        rows = 1;
    } else if (layout == CosmicExportSpritePopup::Layout::Vertical) {
        cols = 1;
        rows = frameCount;
    } else { // Grid — square-ish
        cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(frameCount))));
        rows = (frameCount + cols - 1) / cols;
    }

    const QSize sheetSize(frameSize.width() * cols, frameSize.height() * rows);
    QImage sheet(sheetSize, QImage::Format_ARGB32_Premultiplied);
    sheet.fill(useTransparency ? Qt::transparent : Qt::white);

    QPainter sheetPainter(&sheet);

    const QSize camSize = cameraLayer->getViewSize();

    for (int i = 0; i < frameCount; ++i) {
        const int frame = startFrame + i;
        const int col   = i % cols;
        const int row   = i / cols;

        QImage frameImg(frameSize, QImage::Format_ARGB32_Premultiplied);
        frameImg.fill(useTransparency ? Qt::transparent : Qt::white);

        QTransform view = cameraLayer->getViewAtFrame(frame);
        QTransform centralizeCamera;
        centralizeCamera.translate(camSize.width() / 2.0, camSize.height() / 2.0);

        QPainter framePainter(&frameImg);
        framePainter.setRenderHint(QPainter::Antialiasing);
        framePainter.setWorldTransform(view * centralizeCamera);
        framePainter.setWindow(QRect(0, 0, camSize.width(), camSize.height()));
        mEditor->object()->paintImage(framePainter, frame, false, true);
        framePainter.end();

        sheetPainter.drawImage(col * frameSize.width(), row * frameSize.height(), frameImg);
    }

    sheetPainter.end();

    // Build extension
    QString ext = format.toLower();
    if (ext == QStringLiteral("jpg")) ext = QStringLiteral("jpg");
    const QString outPath = QDir(destDir).filePath(baseName + QLatin1Char('.') + ext);

    if (!sheet.save(outPath, format.toUtf8().constData())) {
        QMessageBox::warning(mParent, tr("Export Failed"),
                             tr("Could not save sprite sheet to:\n%1").arg(outPath),
                             QMessageBox::Ok);
        return Status::FAIL;
    }

    return Status::OK;
}
