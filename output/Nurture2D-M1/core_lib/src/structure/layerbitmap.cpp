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
#include "layerbitmap.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDomDocument>
#include <QColor>
#include "keyframe.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "celllayerstack.h"
#include "innerlayer.h"
#include "util/util.h"

// ── BlendMode serialization helpers ──────────────────────────────────────────

static QString blendModeToString(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:     return QStringLiteral("Normal");
    case BlendMode::Multiply:   return QStringLiteral("Multiply");
    case BlendMode::Screen:     return QStringLiteral("Screen");
    case BlendMode::Overlay:    return QStringLiteral("Overlay");
    case BlendMode::Luminosity: return QStringLiteral("Luminosity");
    }
    return QStringLiteral("Normal");
}

static BlendMode blendModeFromString(const QString& s)
{
    if (s == QLatin1String("Multiply"))   return BlendMode::Multiply;
    if (s == QLatin1String("Screen"))     return BlendMode::Screen;
    if (s == QLatin1String("Overlay"))    return BlendMode::Overlay;
    if (s == QLatin1String("Luminosity")) return BlendMode::Luminosity;
    return BlendMode::Normal;
}

// Per-layer filename: L{layerId:03d}_C{cellId:06d}_L{layerIndex}.png
static QString innerLayerFileName(int layerId, int cellId, int layerIndex)
{
    return QString::asprintf("L%03d_C%06d_L%d.png", layerId, cellId, layerIndex);
}

static QString resolveBitmapDataPath(const QString& src, const QString& dataDirPath)
{
    QString path = validateDataPath(src, dataDirPath);
    if (!path.isEmpty()) {
        return path;
    }

    // Fallback: allow missing files (they may be lazily extracted later).
    // Must still be a safe relative path contained within dataDirPath.
    const QString cleanedSrc = QDir::cleanPath(src).replace('\\', '/');
    if (cleanedSrc.isEmpty()) {
        return QString();
    }
    if (QDir::isAbsolutePath(cleanedSrc) || cleanedSrc.startsWith(QStringLiteral("../")) || cleanedSrc.contains(QStringLiteral("/../")) || cleanedSrc == QStringLiteral("..")) {
        return QString();
    }

    const QString abs = QDir(dataDirPath).absoluteFilePath(cleanedSrc);
    const QString cleanAbs = QDir::cleanPath(abs).replace('\\', '/');
    const QString cleanDataDir = QDir::cleanPath(QDir(dataDirPath).absolutePath()).replace('\\', '/');

    if (cleanAbs == cleanDataDir || cleanAbs.startsWith(cleanDataDir + QStringLiteral("/"))) {
        return abs;
    }
    return QString();
}

LayerBitmap::LayerBitmap(int id) : Layer(id, Layer::BITMAP)
{
    setName(tr("Layer"));
    mCellBank = new CellBank(nullptr);
}

LayerBitmap::~LayerBitmap()
{
    delete mCellBank;
}

BitmapImage* LayerBitmap::getBitmapImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    KeyFrame* kf = getLastKeyFrameAtPosition(frameNumber);
    if (!kf) return nullptr;

    // Use dynamic_cast check if safety is needed, but static for performance if confident.
    // Transition period: KeyFrame might be old BitmapImage? No, we are fully switching.
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
    BitmapImage* img = mCellBank->getCell(bkf->cellId());
    
    // Ensure the image knows which frame it is presenting for (legacy support)
    // Note: This is mutable state on a shared object. Be careful with threads.
    if (img) img->setPos(frameNumber);
    
    return img;
}

BitmapImage* LayerBitmap::getLastBitmapImageAtFrame(int frameNumber, int increment)
{
    Q_ASSERT(frameNumber >= 1);
    KeyFrame* kf = getLastKeyFrameAtPosition(frameNumber + increment);
    if (!kf) return nullptr;
    
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
    BitmapImage* img = mCellBank->getCell(bkf->cellId());
    
    if (img) img->setPos(kf->pos());
    return img;
}

void LayerBitmap::replaceKeyFrame(const KeyFrame* inputKeyFrame)
{
    // replaceKeyFrame is used by undo/redo to restore pixel data.
    // It must write to the active inner layer (not the composite) so the
    // change persists and is reflected on next composite recompute.

    const BitmapImage* inputImg = dynamic_cast<const BitmapImage*>(inputKeyFrame);
    if (!inputImg) return;

    // Find the keyframe at the saved position
    KeyFrame* kf = getKeyFrameAt(inputKeyFrame->pos());
    if (!kf) kf = getLastKeyFrameAtPosition(inputKeyFrame->pos());
    if (!kf) return;

    auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
    if (!bkf || !mCellBank) return;

    BitmapImage* targetImg = mCellBank->getActiveInnerLayerImage(bkf->cellId());
    if (!targetImg) return;

    // Copy pixel data into the active inner layer
    if (inputImg->isLoaded()) {
        *targetImg = *inputImg;
    } else {
        // Input has no pixel data — clear the target instead
        targetImg->clear();
    }

    // Invalidate composite so CanvasPainter recomputes it
    CellLayerStack* stack = mCellBank->getCellLayerStack(bkf->cellId());
    if (stack) stack->invalidateComposite();
}

void LayerBitmap::repositionFrame(QPoint point, int frame)
{
    BitmapImage* image = getBitmapImageAtFrame(frame);
    Q_ASSERT(image);
    image->moveTopLeft(point);
}

QRect LayerBitmap::getFrameBounds(int frame)
{
    BitmapImage* image = getBitmapImageAtFrame(frame);
    if (!image) return QRect();
    return image->bounds();
}

int LayerBitmap::loadImageAtFrame(QString path, QPoint topLeft, int frameNumber, qreal opacity, const QString& cellName, int forcedCellId)
{
    // 1. Create Data
    BitmapImage* image = new BitmapImage(topLeft, path, false);
    image->enableAutoCrop(true);
    image->setOpacity(opacity);
    
    // 2. Add to Bank
    int id = -1;
    if (forcedCellId > 0) {
        id = mCellBank->addCellWithId(forcedCellId, image, cellName);
        if (id < 0) {
            id = mCellBank->addCell(image, cellName);
        }
    } else {
        id = mCellBank->addCell(image, cellName);
    }

    // 3. Create Exposure
    BitmapKeyFrame* kf = new BitmapKeyFrame(id);
    kf->setCellBank(mCellBank);
    kf->setPos(frameNumber);
    
    loadKey(kf);

    return id;
}

Status LayerBitmap::saveKeyFrameFile(KeyFrame* keyframe, QString path)
{
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(keyframe);
    BitmapImage* bitmapImage = mCellBank->getCell(bkf->cellId());
    
    if (!bitmapImage) return Status::FAIL;

    // determine path based on CELL NAME
    QString cellName = mCellBank->getCellName(bkf->cellId());
    // Fallback if empty
    if (cellName.isEmpty()) cellName = QString::number(bkf->cellId());
    
    // Construct filename: layerID.CellName.png 
    // Wait, old format was layerID.framePOS.png
    // User wants: hat_001.png? Or still managed by layer?
    // "Drawings will not be named after the layer... hat_001 regardless of frame"
    // Let's stick to a safe ID based system for the file system to avoid collisions,
    // but the internal logic uses Cell ID.
    
    // We need to implement fileName() properly.
    QString fName = fileName(keyframe);
    QString strFilePath = QDir(path).filePath(fName);

    bool needSave = needSaveFrame(keyframe, strFilePath); // We might need to overload needSaveFrame too as it checks keyframe modification
    
    // Check modification on the IMAGE
    bool isModified = bitmapImage->isModified();

    if (!isModified && QFile::exists(strFilePath) && !bitmapImage->fileName().isEmpty())
    {
        // Already valid
        return Status::SAFE;
    }

    Status st = bitmapImage->writeFile(strFilePath);
    if (!st.ok())
    {
        DebugDetails dd;
        dd << "LayerBitmap::saveKeyFrame";
        dd << QString("  CellId = %1").arg(bkf->cellId());
        dd << QString("  strFilePath = %1").arg(strFilePath);
        dd << QString("Error: Failed to save BitmapImage");
        dd.collect(st.details());
        return Status(Status::FAIL, dd);
    }

    bitmapImage->setModified(false);
    return Status::OK;
}

KeyFrame* LayerBitmap::createKeyFrame(int position)
{
    // Create new blank cell
    BitmapImage* b = new BitmapImage;
    b->enableAutoCrop(true);
    
    // Use layer name or default "Layer"
    QString baseName = name();
    if (baseName.isEmpty()) baseName = "Layer";
    
    // Simple counter. Ideally checks for collision or uses CellBank's next ID.
    // Since ID increases, we can just use ID. 
    // But user wants "hat_001", "hat_002".
    // If we delete "hat_002", next should be "hat_003"? 
    // CellBank ID is always increasing.
    // So "hat_" + ID is safe.
    // Actually, user said: "first keyfraem called hat_001... second keyfraem... hat_002".
    // If I delete hat_001, create new, is it hat_003? Usually yes.
    
    QString cellName = QString("%1_%2").arg(baseName).arg(mCellBank->getAllCellIds().count() + 1, 3, 10, QChar('0'));
    
    int id = mCellBank->addCell(b, cellName);
    
    // Create exposure
    BitmapKeyFrame* kf = new BitmapKeyFrame(id);
    kf->setCellBank(mCellBank);
    kf->setPos(position);
    return kf;
}

// Override addKeyFrame to track cell instances
bool LayerBitmap::addKeyFrame(int position, KeyFrame* pKeyFrame)
{
    qDebug() << "[LayerBitmap::addKeyFrame] Called at position" << position;
    
    // Call base implementation
    bool success = Layer::addKeyFrame(position, pKeyFrame);
    
    qDebug() << "[LayerBitmap::addKeyFrame] Base returned" << success << "cellBank=" << (mCellBank != nullptr);
    
    if (success && mCellBank) {
        // Track the cell instance
        BitmapKeyFrame* bitmapKF = dynamic_cast<BitmapKeyFrame*>(pKeyFrame);
        if (bitmapKF) {
            int cellId = bitmapKF->cellId();
            qDebug() << "[LayerBitmap::addKeyFrame] BitmapKeyFrame cellId=" << cellId;
            if (cellId != -1) {
                mCellBank->addInstance(cellId, id(), position);
            }
        } else {
            qDebug() << "[LayerBitmap::addKeyFrame] Not a BitmapKeyFrame!";
        }
    }
    
    return success;
}

// Override removeKeyFrame to untrack cell instances
bool LayerBitmap::removeKeyFrame(int position)
{
    if (mCellBank) {
        // Untrack the cell instance before removing
        KeyFrame* kf = getKeyFrameAt(position);
        if (kf) {
            BitmapKeyFrame* bitmapKF = dynamic_cast<BitmapKeyFrame*>(kf);
            if (bitmapKF && bitmapKF->cellId() != -1) {
                mCellBank->removeInstance(bitmapKF->cellId(), id(), position);
            }
        }
    }
    
    // Call base implementation
    return Layer::removeKeyFrame(position);
}

// Rebuild instance tracking from existing keyframes
void LayerBitmap::rebuildInstanceTracking()
{
    if (!mCellBank) return;
    
    qDebug() << "[LayerBitmap] Rebuilding instance tracking for layer" << id();
    
    // Scan all keyframes and register their instances
    int count = 0;
    foreachKeyFrame([this, &count](KeyFrame* kf) {
        BitmapKeyFrame* bitmapKF = dynamic_cast<BitmapKeyFrame*>(kf);
        if (bitmapKF && bitmapKF->cellId() != -1) {
            mCellBank->addInstance(bitmapKF->cellId(), id(), kf->pos());
            count++;
        }
    });
    
    qDebug() << "[LayerBitmap] Registered" << count << "instances for layer" << id();
}

Status LayerBitmap::presave(const QString& sDataFolder)
{
    // Need to reimplement presave to handle Cells instead of Frames.
    // Iterating keyframes might visit the same cell multiple times.
    
    QDir dataFolder(sDataFolder);
    QSet<int> processedCells; // To avoid processing same cell twice
    
    foreachKeyFrame([&](KeyFrame* key)
    {
        BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(key);
        if (processedCells.contains(bkf->cellId())) return;
        processedCells.insert(bkf->cellId());
        
        BitmapImage* b = mCellBank->getCell(bkf->cellId());
        if (!b) return;

        QString expectedName = fileName(key); // This uses cell ID now
        QString expectedPath = dataFolder.filePath(expectedName);
        
        if (!b->fileName().isEmpty() && !b->isModified() && b->fileName() != expectedPath)
        {
            // Snapshot existing file into the save folder without mutating the live workspace.
            QFile::remove(expectedPath);
            QFile::copy(b->fileName(), expectedPath);
        }

        // Save each InnerLayer's BitmapImage to its own per-layer PNG file.
        CellLayerStack* stack = mCellBank->getCellLayerStack(bkf->cellId());
        if (!stack) return;

        for (int i = 0; i < stack->layerCount(); ++i) {
            InnerLayer* innerLayer = stack->layerAt(i);
            if (!innerLayer || !innerLayer->image) continue;

            const QString layerFileName = innerLayerFileName(id(), bkf->cellId(), i);
            const QString layerFilePath = dataFolder.filePath(layerFileName);

            BitmapImage* img = innerLayer->image;
            if (!img->isModified() && QFile::exists(layerFilePath) && !img->fileName().isEmpty()) {
                // Already saved; snapshot if needed
                if (img->fileName() != layerFilePath) {
                    QFile::remove(layerFilePath);
                    QFile::copy(img->fileName(), layerFilePath);
                    img->setFileName(layerFilePath);
                }
            } else {
                img->writeFile(layerFilePath);
                img->setFileName(layerFilePath);
                img->setModified(false);
            }
        }
    });

    return Status::OK;
}

QString LayerBitmap::filePath(KeyFrame* key, const QDir& dataFolder) const
{
    return dataFolder.filePath(fileName(key));
}

QString LayerBitmap::fileName(KeyFrame* key) const
{
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(key);

    // Always use a collision-free on-disk name.
    // The user-facing name is stored separately (cellName attribute) and must not be used as a filename,
    // otherwise different layers can overwrite each other when saving into the same data folder.
    return QString::asprintf("L%03d_C%06d.png", id(), bkf->cellId());
}

bool LayerBitmap::needSaveFrame(KeyFrame* key, const QString& savePath)
{
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(key);
    BitmapImage* img = mCellBank->getCell(bkf->cellId());
    if (!img) return false;

    if (img->isModified())
        return true;
    if (QFile::exists(savePath) == false)
        return true;
    if (img->fileName().isEmpty())
        return true;
    return false;
}

QDomElement LayerBitmap::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = createBaseDomElement(doc);

    if (mCellBank) {
        const QList<int> order = mCellBank->getAllCellIds();
        if (!order.isEmpty()) {
            QStringList ids;
            ids.reserve(order.size());
            for (int id : order) {
                ids.append(QString::number(id));
            }
            QDomElement cellOrderTag = doc.createElement("cellOrder");
            cellOrderTag.setAttribute("ids", ids.join(QLatin1Char(',')));
            layerElem.appendChild(cellOrderTag);
        }
    }

    foreachKeyFrame([&](KeyFrame* pKeyFrame)
    {
        BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(pKeyFrame);
        BitmapImage* pImg = mCellBank->getCell(bkf->cellId());
        
        if (pImg) {
            QDomElement imageTag = doc.createElement("image");
            imageTag.setAttribute("frame", pKeyFrame->pos());
            
            // Allow filename override from keyframe if needed, but usually it's the Cell's filename
            QString fName = fileName(pKeyFrame); // This uses the proper naming logic
            imageTag.setAttribute("src", fName);

            imageTag.setAttribute("cellId", bkf->cellId());
            imageTag.setAttribute("cellName", mCellBank->getCellName(bkf->cellId()));
            
            imageTag.setAttribute("topLeftX", pImg->topLeft().x());
            imageTag.setAttribute("topLeftY", pImg->topLeft().y());
            imageTag.setAttribute("opacity", pImg->getOpacity());

            // Write per-inner-layer data
            CellLayerStack* stack = mCellBank->getCellLayerStack(bkf->cellId());
            if (stack && stack->layerCount() > 0) {
                QDomElement layersTag = doc.createElement("layers");
                layersTag.setAttribute("activeIndex", stack->activeIndex());

                for (int i = 0; i < stack->layerCount(); ++i) {
                    const InnerLayer* innerLayer = stack->layerAt(i);
                    if (!innerLayer) continue;

                    QDomElement layerTag = doc.createElement("layer");
                    layerTag.setAttribute("index", i);
                    layerTag.setAttribute("src", innerLayerFileName(id(), bkf->cellId(), i));
                    layerTag.setAttribute("name", innerLayer->name);
                    layerTag.setAttribute("visible", innerLayer->visible ? "true" : "false");
                    layerTag.setAttribute("locked",  innerLayer->locked  ? "true" : "false");
                    layerTag.setAttribute("opacity", innerLayer->opacity);
                    layerTag.setAttribute("blendMode", blendModeToString(innerLayer->blendMode));
                    if (innerLayer->tintColor.isValid()) {
                        layerTag.setAttribute("tintColor", innerLayer->tintColor.name(QColor::HexArgb));
                    }

                    // Per-layer position
                    if (innerLayer->image) {
                        layerTag.setAttribute("topLeftX", innerLayer->image->topLeft().x());
                        layerTag.setAttribute("topLeftY", innerLayer->image->topLeft().y());
                    } else {
                        layerTag.setAttribute("topLeftX", 0);
                        layerTag.setAttribute("topLeftY", 0);
                    }

                    layersTag.appendChild(layerTag);
                }

                imageTag.appendChild(layersTag);
            }

            layerElem.appendChild(imageTag);
    
            if (!pImg->fileName().isEmpty()) {
                // Determine equality of filename
                // Q_ASSERT(QFileInfo(pImg->fileName()).fileName() == fileName(pKeyFrame));
            }
        }
    });

    return layerElem;
}

void LayerBitmap::loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep)
{
    this->loadBaseDomElement(element);

    QList<int> desiredCellOrder;
    bool hasDesiredCellOrder = false;
    
    struct LayerEntry {
        QString src;
        QString path;
        int     index    = 0;
        int     x        = 0;
        int     y        = 0;
        qreal   opacity  = 1.0;
        bool    visible  = true;
        bool    locked   = false;
        BlendMode blendMode = BlendMode::Normal;
        QString name;
        QColor  tintColor; // invalid = no tint
    };

    struct ImgEntry {
        QString src;
        QString path;
        int position = 0;
        int x = 0;
        int y = 0;
        qreal opacity = 1.0;
        int savedCellId = -1;
        QString savedCellName;

        // Inner layer data (present when <layers> child exists)
        bool hasLayers = false;
        int  activeLayerIndex = 0;
        QList<LayerEntry> layers;
    };

    QList<ImgEntry> entries;
    QDomNode imageTag = element.firstChild();
    while (!imageTag.isNull())
    {
        QDomElement imageElement = imageTag.toElement();
        if (!imageElement.isNull() && imageElement.tagName() == "cellOrder")
        {
            const QString ids = imageElement.attribute("ids");
            if (!ids.isEmpty()) {
                const QStringList parts = ids.split(QLatin1Char(','), Qt::SkipEmptyParts);
                desiredCellOrder.clear();
                desiredCellOrder.reserve(parts.size());
                for (const QString& p : parts) {
                    bool ok = false;
                    const int id = p.trimmed().toInt(&ok);
                    if (ok && id > 0) {
                        desiredCellOrder.append(id);
                    }
                }
                hasDesiredCellOrder = !desiredCellOrder.isEmpty();
            }
        }
        else if (!imageElement.isNull() && imageElement.tagName() == "image")
        {
            ImgEntry e;
            e.src = imageElement.attribute("src");
            e.path = resolveBitmapDataPath(e.src, dataDirPath);
            if (!e.path.isEmpty()) {
                e.position = imageElement.attribute("frame").toInt();
                e.x = imageElement.attribute("topLeftX").toInt();
                e.y = imageElement.attribute("topLeftY").toInt();
                e.opacity = imageElement.attribute("opacity", "1.0").toDouble();

                bool okId = false;
                e.savedCellId = imageElement.attribute("cellId").toInt(&okId);
                if (!okId) {
                    e.savedCellId = -1;
                }

                e.savedCellName = imageElement.attribute("cellName");
                if (e.savedCellName.isEmpty()) {
                    e.savedCellName = QFileInfo(e.src).completeBaseName();
                }

                // Check for <layers> child element
                QDomElement layersElem = imageElement.firstChildElement("layers");
                if (!layersElem.isNull()) {
                    e.hasLayers = true;
                    e.activeLayerIndex = layersElem.attribute("activeIndex", "0").toInt();

                    QDomElement layerElem = layersElem.firstChildElement("layer");
                    while (!layerElem.isNull()) {
                        LayerEntry le;
                        le.index     = layerElem.attribute("index", "0").toInt();
                        le.src       = layerElem.attribute("src");
                        le.path      = resolveBitmapDataPath(le.src, dataDirPath);
                        le.name      = layerElem.attribute("name", "Layer 1");
                        le.visible   = (layerElem.attribute("visible", "true") != QLatin1String("false"));
                        le.locked    = (layerElem.attribute("locked",  "false") == QLatin1String("true"));
                        le.opacity   = layerElem.attribute("opacity", "1.0").toDouble();
                        le.blendMode = blendModeFromString(layerElem.attribute("blendMode", "Normal"));
                        le.x         = layerElem.attribute("topLeftX", "0").toInt();
                        le.y         = layerElem.attribute("topLeftY", "0").toInt();
                        const QString tintStr = layerElem.attribute("tintColor");
                        if (!tintStr.isEmpty()) {
                            le.tintColor = QColor(tintStr);
                        }
                        e.layers.append(le);
                        layerElem = layerElem.nextSiblingElement("layer");
                    }

                    // Sort layers by index attribute
                    std::sort(e.layers.begin(), e.layers.end(), [](const LayerEntry& a, const LayerEntry& b) {
                        return a.index < b.index;
                    });
                }

                entries.append(e);
            }

            progressStep();
        }
        imageTag = imageTag.nextSibling();
    }

    std::sort(entries.begin(), entries.end(), [](const ImgEntry& a, const ImgEntry& b) {
        return a.position < b.position;
    });

    QMap<QString, int> loadedFiles; // Path -> CellID
    QMap<int, int> remappedCellIds; // Saved cellId -> actual cellId

    for (const ImgEntry& e : entries)
    {
        if (loadedFiles.contains(e.path))
        {
            int id = loadedFiles.value(e.path);
            BitmapKeyFrame* kf = new BitmapKeyFrame(id);
            kf->setCellBank(mCellBank);
            kf->setPos(e.position);
            loadKey(kf);
        }
        else
        {
            int forcedId = e.savedCellId;
            if (forcedId > 0 && mCellBank->getCell(forcedId) != nullptr) {
                forcedId = -1;
            }

            if (e.hasLayers && !e.layers.isEmpty()) {
                // New format: load each inner layer from its own file.
                // Use the first layer's image to create the cell via addCellWithId for API compat.
                const LayerEntry& firstLayer = e.layers.first();

                BitmapImage* firstImg = nullptr;
                if (!firstLayer.path.isEmpty()) {
                    firstImg = new BitmapImage(QPoint(firstLayer.x, firstLayer.y), firstLayer.path, false);
                    firstImg->enableAutoCrop(true);
                } else {
                    qWarning() << "Inner layer has no resolvable path:" << firstLayer.src << ". Substituting transparent layer.";
                    firstImg = new BitmapImage();
                }

                int cellId;
                if (forcedId > 0) {
                    cellId = mCellBank->addCellWithId(forcedId, firstImg, e.savedCellName);
                    if (cellId < 0) {
                        cellId = mCellBank->addCell(firstImg, e.savedCellName);
                    }
                } else {
                    cellId = mCellBank->addCell(firstImg, e.savedCellName);
                }

                // Now configure the CellLayerStack with all inner layers
                CellLayerStack* stack = mCellBank->getCellLayerStack(cellId);
                if (stack) {
                    // The stack was created with firstImg as InnerLayer[0] named "Layer 1".
                    // Apply the saved properties to layer 0.
                    InnerLayer* layer0 = stack->layerAt(0);
                    if (layer0) {
                        layer0->name      = firstLayer.name;
                        layer0->visible   = firstLayer.visible;
                        layer0->locked    = firstLayer.locked;
                        layer0->setOpacity(firstLayer.opacity);
                        layer0->blendMode = firstLayer.blendMode;
                        layer0->tintColor = firstLayer.tintColor;
                    }

                    // Add remaining layers (index 1+)
                    for (int i = 1; i < e.layers.size(); ++i) {
                        const LayerEntry& le = e.layers.at(i);

                        BitmapImage* img = nullptr;
                        if (!le.path.isEmpty()) {
                            img = new BitmapImage(QPoint(le.x, le.y), le.path, false);
                            img->enableAutoCrop(true);
                        } else {
                            qWarning() << "Inner layer has no resolvable path:" << le.src << ". Substituting transparent layer.";
                            img = new BitmapImage();
                        }

                        // Add a new inner layer to the stack
                        int newIdx = stack->addLayer(le.name, i - 1);
                        InnerLayer* newLayer = stack->layerAt(newIdx);
                        if (newLayer) {
                            // Replace the blank image with the loaded one
                            delete newLayer->image;
                            newLayer->image     = img;
                            newLayer->visible   = le.visible;
                            newLayer->locked    = le.locked;
                            newLayer->setOpacity(le.opacity);
                            newLayer->blendMode = le.blendMode;
                            newLayer->tintColor = le.tintColor;
                        } else {
                            delete img;
                        }
                    }

                    // Restore active index
                    stack->setActiveIndex(e.activeLayerIndex);
                    stack->invalidateComposite();
                }

                // Create the keyframe exposure
                BitmapKeyFrame* kf = new BitmapKeyFrame(cellId);
                kf->setCellBank(mCellBank);
                kf->setPos(e.position);
                loadKey(kf);

                loadedFiles.insert(e.path, cellId);

                if (e.savedCellId > 0 && e.savedCellId != cellId) {
                    remappedCellIds.insert(e.savedCellId, cellId);
                }
            } else {
                // Legacy format: no <layers> child — load as single inner layer.
                // addCellWithId / addCell wraps the image in a CellLayerStack with InnerLayer[0] named "Layer 1".
                const int id = loadImageAtFrame(e.path, QPoint(e.x, e.y), e.position, e.opacity, e.savedCellName, forcedId);
                loadedFiles.insert(e.path, id);

                if (e.savedCellId > 0 && e.savedCellId != id) {
                    remappedCellIds.insert(e.savedCellId, id);
                }
            }
        }
    }
    
    // Rebuild instance tracking for all loaded keyframes
    rebuildInstanceTracking();

    if (mCellBank && hasDesiredCellOrder)
    {
        QList<int> mapped;
        mapped.reserve(desiredCellOrder.size());
        for (int savedId : desiredCellOrder) {
            const int actualId = remappedCellIds.value(savedId, savedId);
            mapped.append(actualId);
        }
        mCellBank->setCellOrder(mapped);
    }
}
