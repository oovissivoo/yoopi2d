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
#include "importlayersdialog.h"
#include "ui_importlayersdialog.h"

#include <QProgressDialog>

#include "app_util.h"
#include "n2dprojectio.h"
#include "filedialog.h"
#include "fileformat.h"
#include "layermanager.h"
#include "soundmanager.h"
#include "layer.h"
#include "layersound.h"
#include "layervector.h"
#include "layerbitmap.h"
#include "cellbank.h"
#include "bitmapimage.h"
#include "soundclip.h"


ImportLayersDialog::ImportLayersDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImportLayersDialog)
{
    ui->setupUi(this);
    connect(ui->btnSelectFile, &QPushButton::clicked, this, &ImportLayersDialog::getFileName);
    connect(ui->btnImportLayers, &QPushButton::clicked, this, &ImportLayersDialog::importLayers);
    connect(ui->lwLayers, &QListWidget::itemSelectionChanged, this, &ImportLayersDialog::listWidgetChanged);
    connect(ui->btnClose, &QPushButton::clicked, this, &ImportLayersDialog::cancel);
    ui->lwLayers->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->btnImportLayers->setEnabled(false);

    hideQuestionMark(*this);
}

ImportLayersDialog::~ImportLayersDialog()
{
    delete ui;
}

void ImportLayersDialog::setCore(Editor *editor)
{
    mEditor = editor;
}

void ImportLayersDialog::getFileName()
{
    mFileName.clear();
    ui->lwLayers->clear();
    mFileName = FileDialog::getOpenFileName(this, FileType::ANIMATION, tr("Choose file"));
    if (mFileName.isEmpty()) { return; }
    getLayers();
}

void ImportLayersDialog::listWidgetChanged()
{
    mItemsSelected.clear();
    for (int i = 0; i < ui->lwLayers->count(); i++)
        if (ui->lwLayers->item(i)->isSelected())
            mItemsSelected.append(i);

    if (!mItemsSelected.isEmpty())
        ui->btnImportLayers->setEnabled(true);
    else
        ui->btnImportLayers->setEnabled(false);
}

void ImportLayersDialog::importLayers()
{
    Object* object = mEditor->object();
    int currentFrame = mEditor->currentFrame();
    Q_ASSERT(ui->lwLayers->count() == mImportObject->getLayerCount());

    QMap<int, int> importedColors;

    for (const QListWidgetItem* item : ui->lwLayers->selectedItems())
    {
        mImportLayer = mImportObject->takeLayer(item->data(Qt::UserRole).toInt());
        mImportLayer->setName(mEditor->layers()->nameSuggestLayer(item->text()));
        loadKeyFrames(mImportLayer); // all keyframes of this layer must be in memory

        object->addLayer(mImportLayer);

        if (mImportLayer->type() == Layer::VECTOR)
        {
            LayerVector* layerVector = static_cast<LayerVector*>(mImportLayer);
            for (int i = 0; i < mImportObject->getColorCount(); i++) {
                if (!layerVector->usesColor(i)) {
                    continue;
                }

                if (!importedColors.contains(i)) {
                    const ColorRef color = mImportObject->getColor(i);
                    object->addColor(color);
                    importedColors[i] = object->getColorCount() - 1;
                }

                layerVector->moveColor(i, importedColors[i]);
            }
        }

        if (mImportLayer->type() == Layer::SOUND)
        {
            LayerSound* layerSound = static_cast<LayerSound*>(mImportLayer);
            layerSound->foreachKeyFrame([this](KeyFrame* key)
            {
                SoundClip* clip = dynamic_cast<SoundClip*>(key);
                Status st = mEditor->sound()->loadSound(clip, clip->fileName());
                Q_ASSERT(st.ok());
            });
        }
    }
    mEditor->object()->modification();

    mImportObject.reset();
    getLayers();
    // Return to the frame the user was on and refresh the canvas so imported layers are visible
    mEditor->scrubTo(currentFrame);
    mEditor->updateFrame();

    // Close the dialog so the main window can refresh timeline UI
    accept();
}

void ImportLayersDialog::cancel()
{
    close();
}

void ImportLayersDialog::getLayers()
{
    QProgressDialog progress(tr("Opening document..."), tr("Abort"), 0, 100, this);

    // Don't show progress bar if running without a GUI (aka. when rendering from command line)
    if (isVisible())
    {
        hideQuestionMark(progress);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
    }

    Status st = Status::OK;
    const N2DProjectIO::ProgressCallbacks cb{
        [&progress](int p) {
            progress.setValue(p);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        },
        [&progress](int max) {
            progress.setRange(0, max + 3);
        }
    };
    mImportObject = N2DProjectIO::loadSingleShotProjectAsObject(mFileName, cb, &st);
    if (!st.ok() || !mImportObject) {
        ui->lwLayers->clear();
        return;
    }

    ui->lwLayers->clear();
    for (int i = 0; i < mImportObject->getLayerCount(); i++)
    {
        Layer* layer = mImportObject->getLayer(i);
        if (layer && layer->type() == Layer::CAMERA)
        {
            // Skip camera layers from being listed/imported
            continue;
        }

        const QString layerName = layer->name();
        const int layerId = layer->id();

        // Store the layer name as well as layer ID cuz two layers could have the same name
        QListWidgetItem* item = new QListWidgetItem(layerName);
        item->setData(Qt::UserRole, layerId);
        ui->lwLayers->addItem(item);
    }
}

void ImportLayersDialog::loadKeyFrames(Layer* importedLayer)
{
    if (!importedLayer) {
        return;
    }

    // Pencil2D only keeps a small portion of keyframes (and associated images) in memory.
    // When importing from another project, the source Object's temporary data directory
    // is deleted once mImportObject is reset. For bitmap layers using the CellBank
    // system, we must explicitly load each cell image into memory so that drawings
    // remain available after the source files are gone.

    if (importedLayer->type() == Layer::BITMAP)
    {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(importedLayer);
        CellBank* bank = bitmapLayer->cellBank();
        if (bank)
        {
            const QList<int> cellIds = bank->getAllCellIds();
            for (int id : cellIds)
            {
                if (BitmapImage* img = bank->getCell(id))
                {
                    // Load pixel data into memory from the original file
                    img->loadFile();

                    // Detach from the original on-disk path so we don't depend on
                    // the source project's temporary data directory after import.
                    img->setFileName(QString());

                    // Mark as modified so the current project will write these
                    // cells into its own data folder on the next save.
                    img->setModified(true);
                }

                // Reset per-cell undo history so imported content starts with a
                // clean stack in this project, just like freshly created cells.
                if (QUndoStack* stack = bank->getOrCreateUndoStack(id)) {
                    stack->clear();
                }
            }
        }
    }

    // For non-bitmap layers (or legacy cases), still force-load any keyframe data.
    importedLayer->foreachKeyFrame([](KeyFrame* k)
    {
        k->loadFile();
    });
}
