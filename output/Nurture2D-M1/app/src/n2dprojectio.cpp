#include "n2dprojectio.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>

#include "editor.h"
#include "object.h"
#include "objectdata.h"
#include "qminiz.h"
#include "miniz.h"
#include "layer.h"
#include "layercamera.h"
#include "pencildef.h"
#include "scenesequencemanager.h"

namespace
{
static bool ensureDirExists(const QString& dirPath)
{
    QDir d(dirPath);
    if (d.exists()) {
        return true;
    }
    return d.mkpath(QStringLiteral("."));
}

static Status saveObjectToShotFolder(const Object* object,
                                    const QString& shotRoot,
                                    const N2DProjectIO::ProgressCallbacks& callbacks,
                                    int& progress);

static Status zipFolderToN2D(const QString& outputFilePath,
                            const QString& rootFolder,
                            const QString& mimetype);

static Status loadObjectFromShotFolder(const QString& shotRoot,
                                      const QString& projectFilePath,
                                      Object* object,
                                      const N2DProjectIO::ProgressCallbacks& callbacks,
                                      int& progress);

static QDomElement saveProjectDataXml(const ObjectData* data, QDomDocument& xmlDoc)
{
    QDomElement rootTag = xmlDoc.createElement(QStringLiteral("projectdata"));

    QDomElement currentFrameTag = xmlDoc.createElement(QStringLiteral("currentFrame"));
    currentFrameTag.setAttribute(QStringLiteral("value"), data->getCurrentFrame());
    rootTag.appendChild(currentFrameTag);

    QDomElement currentColorTag = xmlDoc.createElement(QStringLiteral("currentColor"));
    const QColor color = data->getCurrentColor();
    currentColorTag.setAttribute(QStringLiteral("r"), color.red());
    currentColorTag.setAttribute(QStringLiteral("g"), color.green());
    currentColorTag.setAttribute(QStringLiteral("b"), color.blue());
    currentColorTag.setAttribute(QStringLiteral("a"), color.alpha());
    rootTag.appendChild(currentColorTag);

    QDomElement currentLayerTag = xmlDoc.createElement(QStringLiteral("currentLayer"));
    currentLayerTag.setAttribute(QStringLiteral("value"), data->getCurrentLayer());
    rootTag.appendChild(currentLayerTag);

    QDomElement currentViewTag = xmlDoc.createElement(QStringLiteral("currentView"));
    const QTransform view = data->getCurrentView();
    currentViewTag.setAttribute(QStringLiteral("m11"), view.m11());
    currentViewTag.setAttribute(QStringLiteral("m12"), view.m12());
    currentViewTag.setAttribute(QStringLiteral("m21"), view.m21());
    currentViewTag.setAttribute(QStringLiteral("m22"), view.m22());
    currentViewTag.setAttribute(QStringLiteral("dx"), view.dx());
    currentViewTag.setAttribute(QStringLiteral("dy"), view.dy());
    rootTag.appendChild(currentViewTag);

    QDomElement fpsTag = xmlDoc.createElement(QStringLiteral("fps"));
    fpsTag.setAttribute(QStringLiteral("value"), data->getFrameRate());
    rootTag.appendChild(fpsTag);

    QDomElement isLoopTag = xmlDoc.createElement(QStringLiteral("isLoop"));
    isLoopTag.setAttribute(QStringLiteral("value"), data->isLooping() ? QStringLiteral("true") : QStringLiteral("false"));
    rootTag.appendChild(isLoopTag);

    QDomElement isRangeTag = xmlDoc.createElement(QStringLiteral("isRangedPlayback"));
    isRangeTag.setAttribute(QStringLiteral("value"), data->isRangedPlayback() ? QStringLiteral("true") : QStringLiteral("false"));
    rootTag.appendChild(isRangeTag);

    QDomElement markInTag = xmlDoc.createElement(QStringLiteral("markInFrame"));
    markInTag.setAttribute(QStringLiteral("value"), data->getMarkInFrameNumber());
    rootTag.appendChild(markInTag);

    QDomElement markOutTag = xmlDoc.createElement(QStringLiteral("markOutFrame"));
    markOutTag.setAttribute(QStringLiteral("value"), data->getMarkOutFrameNumber());
    rootTag.appendChild(markOutTag);

    return rootTag;
}

static void extractProjectData(const QDomElement& element, ObjectData& data)
{
    const QString strName = element.tagName();
    if (strName == QStringLiteral("currentFrame")) {
        data.setCurrentFrame(element.attribute(QStringLiteral("value")).toInt());
    } else if (strName == QStringLiteral("currentColor")) {
        const int r = element.attribute(QStringLiteral("r"), QStringLiteral("255")).toInt();
        const int g = element.attribute(QStringLiteral("g"), QStringLiteral("255")).toInt();
        const int b = element.attribute(QStringLiteral("b"), QStringLiteral("255")).toInt();
        const int a = element.attribute(QStringLiteral("a"), QStringLiteral("255")).toInt();
        data.setCurrentColor(QColor(r, g, b, a));
    } else if (strName == QStringLiteral("currentLayer")) {
        data.setCurrentLayer(element.attribute(QStringLiteral("value"), QStringLiteral("0")).toInt());
    } else if (strName == QStringLiteral("currentView")) {
        const double m11 = element.attribute(QStringLiteral("m11"), QStringLiteral("1")).toDouble();
        const double m12 = element.attribute(QStringLiteral("m12"), QStringLiteral("0")).toDouble();
        const double m21 = element.attribute(QStringLiteral("m21"), QStringLiteral("0")).toDouble();
        const double m22 = element.attribute(QStringLiteral("m22"), QStringLiteral("1")).toDouble();
        const double dx = element.attribute(QStringLiteral("dx"), QStringLiteral("0")).toDouble();
        const double dy = element.attribute(QStringLiteral("dy"), QStringLiteral("0")).toDouble();
        data.setCurrentView(QTransform(m11, m12, m21, m22, dx, dy));
    } else if (strName == QStringLiteral("fps") || strName == QStringLiteral("currentFps")) {
        data.setFrameRate(element.attribute(QStringLiteral("value"), QStringLiteral("12")).toInt());
    } else if (strName == QStringLiteral("isLoop")) {
        data.setLooping(element.attribute(QStringLiteral("value"), QStringLiteral("false")) == QStringLiteral("true"));
    } else if (strName == QStringLiteral("isRangedPlayback")) {
        data.setRangedPlayback(element.attribute(QStringLiteral("value"), QStringLiteral("false")) == QStringLiteral("true"));
    } else if (strName == QStringLiteral("markInFrame")) {
        data.setMarkInFrameNumber(element.attribute(QStringLiteral("value"), QStringLiteral("0")).toInt());
    } else if (strName == QStringLiteral("markOutFrame")) {
        data.setMarkOutFrameNumber(element.attribute(QStringLiteral("value"), QStringLiteral("15")).toInt());
    }
}

static ObjectData loadProjectDataXml(const QDomElement& docElem)
{
    ObjectData data;
    if (docElem.isNull()) {
        return data;
    }

    QDomNode tag = docElem.firstChild();
    while (!tag.isNull())
    {
        QDomElement element = tag.toElement();
        if (!element.isNull()) {
            extractProjectData(element, data);
        }
        tag = tag.nextSibling();
    }
    return data;
}

static Status writeMainXmlFile(const Object* object, const QString& mainXmlPath)
{
    DebugDetails dd;
    QFile file(mainXmlPath);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        dd << QString("Failed to open: %1, reason: %2").arg(mainXmlPath, file.errorString());
        return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
    }

    QDomDocument xmlDoc(QStringLiteral("Nurture2DDocument"));
    QDomElement root = xmlDoc.createElement(QStringLiteral("document"));
    QDomProcessingInstruction encoding = xmlDoc.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\""));
    xmlDoc.appendChild(encoding);
    xmlDoc.appendChild(root);

    root.appendChild(saveProjectDataXml(object->data(), xmlDoc));
    root.appendChild(object->saveXML(xmlDoc));

    QDomElement versionElem = xmlDoc.createElement(QStringLiteral("version"));
    versionElem.appendChild(xmlDoc.createTextNode(QString(APP_VERSION)));
    root.appendChild(versionElem);

    QTextStream out(&file);
    xmlDoc.save(out, 2);
    out.flush();

    file.close();
    return Status::OK;
}

static Status saveObjectToShotFolder(const Object* object,
                                    const QString& shotRoot,
                                    const N2DProjectIO::ProgressCallbacks& callbacks,
                                    int& progress)
{
    DebugDetails dd;

    const QString mainXmlPath = QDir(shotRoot).filePath(QStringLiteral("main.xml"));
    const QString dataFolder = QDir(shotRoot).filePath(QStringLiteral("data"));

    if (!ensureDirExists(shotRoot) || !ensureDirExists(dataFolder)) {
        dd << QString("Failed to create directories under: %1").arg(shotRoot);
        return Status(Status::FAIL, dd);
    }

    const int numLayers = object->getLayerCount();
    for (int i = 0; i < numLayers; ++i) {
        Layer* layer = object->getLayer(i);
        if (layer) {
            layer->presave(dataFolder);
        }
    }

    QStringList attachedFiles;
    for (int i = 0; i < numLayers; ++i)
    {
        Layer* layer = object->getLayer(i);
        if (!layer) {
            continue;
        }

        Status st = layer->save(dataFolder, attachedFiles, [&]() {
            progress++;
            if (callbacks.progressChanged) {
                callbacks.progressChanged(progress);
            }
        });
        if (!st.ok()) {
            dd.collect(st.details());
            return Status(Status::FAIL, dd);
        }
    }

    const QString paletteFile = object->savePalette(dataFolder);
    if (paletteFile.isEmpty()) {
        dd << QString("Failed to save palette");
        return Status(Status::FAIL, dd);
    }

    Status stXml = writeMainXmlFile(object, mainXmlPath);
    if (!stXml.ok()) {
        dd.collect(stXml.details());
        return Status(Status::FAIL, dd);
    }

    progress++;
    if (callbacks.progressChanged) {
        callbacks.progressChanged(progress);
    }

    return Status::OK;
}

static QStringList listAllFilesRecursive(const QString& rootDirPath)
{
    QStringList files;
    QDirIterator it(rootDirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files.append(it.next());
    }
    return files;
}

static Status zipFolderToN2D(const QString& outputFilePath,
                            const QString& rootFolder,
                            const QString& mimetype)
{
    DebugDetails dd;

    mz_zip_archive mz;
    mz_zip_zero_struct(&mz);

    if (!mz_zip_writer_init_heap(&mz, 0, 0)) {
        dd << QString("mz_zip_writer_init_heap failed");
        return Status(Status::ERROR_MINIZ_FAIL, dd);
    }

    const QByteArray mimeData = mimetype.toUtf8();
    if (!mz_zip_writer_add_mem(&mz, "mimetype", mimeData.constData(), static_cast<size_t>(mimeData.size()), MZ_NO_COMPRESSION)) {
        mz_zip_writer_end(&mz);
        dd << QString("Failed to add mimetype");
        return Status(Status::ERROR_MINIZ_FAIL, dd);
    }

    const QDir root(rootFolder);
    const QStringList files = listAllFilesRecursive(rootFolder);
    for (const QString& absPath : files)
    {
        QString rel = root.relativeFilePath(absPath);
        if (rel == QStringLiteral("mimetype")) {
            continue;
        }

        rel = rel.replace('\\', '/');

        if (!mz_zip_writer_add_file(&mz,
                                   rel.toUtf8().constData(),
                                   absPath.toUtf8().constData(),
                                   "", 0,
                                   MZ_BEST_SPEED))
        {
            mz_zip_writer_end(&mz);
            dd << QString("Failed to add file to archive: %1").arg(rel);
            return Status(Status::ERROR_MINIZ_FAIL, dd);
        }
    }

    void* outBuf = nullptr;
    size_t outSize = 0;
    if (!mz_zip_writer_finalize_heap_archive(&mz, &outBuf, &outSize)) {
        mz_zip_writer_end(&mz);
        dd << QString("Failed to finalize heap archive");
        return Status(Status::ERROR_MINIZ_FAIL, dd);
    }

    mz_zip_writer_end(&mz);

    QSaveFile out(outputFilePath);
    if (!out.open(QIODevice::WriteOnly)) {
        mz_free(outBuf);
        dd << QString("Failed to open output file: %1").arg(outputFilePath);
        return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
    }

    const qint64 wrote = out.write(static_cast<const char*>(outBuf), static_cast<qint64>(outSize));
    mz_free(outBuf);

    if (wrote != static_cast<qint64>(outSize)) {
        dd << QString("Failed to write full archive to disk");
        out.cancelWriting();
        return Status(Status::FAIL, dd);
    }

    if (!out.commit()) {
        dd << QString("Failed to commit archive");
        return Status(Status::FAIL, dd);
    }

    return Status::OK;
}

static Status loadObjectFromShotFolder(const QString& shotRoot,
                                      const QString& projectFilePath,
                                      Object* object,
                                      const N2DProjectIO::ProgressCallbacks& callbacks,
                                      int& progress)
{
    DebugDetails dd;

    QDir shotDir(shotRoot);
    if (!shotDir.exists()) {
        dd << QString("Missing directory: %1").arg(shotRoot);
        return Status(Status::FILE_NOT_FOUND, dd);
    }

    const QString mainXmlPath = QDir(shotRoot).filePath(QStringLiteral("main.xml"));
    const QString dataFolder = QDir(shotRoot).filePath(QStringLiteral("data"));

    if (!QFileInfo::exists(mainXmlPath)) {
        dd << QString("Missing main.xml: %1").arg(mainXmlPath);
        return Status(Status::ERROR_INVALID_XML_FILE, dd);
    }

    object->setFilePath(projectFilePath);
    object->setWorkingDir(shotRoot);
    object->setMainXMLFile(mainXmlPath);
    object->setDataDir(dataFolder);

    const QString palettePath = QDir(dataFolder).filePath(QStringLiteral("palette.xml"));
    if (QFileInfo::exists(palettePath)) {
        object->importPalette(palettePath);
    }

    QFile file(mainXmlPath);
    if (!file.open(QFile::ReadOnly)) {
        dd << QString("Unable to read main.xml: %1").arg(mainXmlPath);
        return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
    }

    QDomDocument xmlDoc;
    if (!xmlDoc.setContent(&file)) {
        dd << QString("Unable to parse main.xml: %1").arg(mainXmlPath);
        return Status(Status::ERROR_INVALID_XML_FILE, dd);
    }

    const QDomElement root = xmlDoc.documentElement();
    if (root.isNull() || root.tagName() != QStringLiteral("document")) {
        dd << QString("Invalid main.xml root");
        return Status(Status::ERROR_INVALID_PENCIL_FILE, dd);
    }

    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        const QDomElement element = node.toElement();
        if (element.isNull()) {
            continue;
        }

        if (element.tagName() == QStringLiteral("object")) {
            const bool ok = object->loadXML(element, [&]() {
                progress++;
                if (callbacks.progressChanged) {
                    callbacks.progressChanged(progress);
                }
            });
            if (!ok) {
                dd << QString("Failed to load object XML");
                return Status(Status::FAIL, dd);
            }
        } else if (element.tagName() == QStringLiteral("projectdata") || element.tagName() == QStringLiteral("editor")) {
            object->setData(loadProjectDataXml(element));
        }
    }

    if (object->data()->getCurrentLayer() >= object->getLayerCount()) {
        object->data()->setCurrentLayer(object->getLayerCount() - 1);
    }

    std::vector<LayerCamera*> camLayers = object->getLayersByType<LayerCamera>();
    if (camLayers.empty()) {
        object->addNewCameraLayer();
    }

    return Status::OK;
} // <--- Added closing brace here

static Status saveShotTagsFile(const QString& shotRoot, const SequenceEntry& seq)
{
    DebugDetails dd;

    QJsonObject root;
    root.insert(QStringLiteral("defaultEnabled"), seq.tagState.defaultEnabled);

    QJsonArray enabledArr;
    for (bool b : seq.tagState.tagEnabled) {
        enabledArr.append(b);
    }
    root.insert(QStringLiteral("tagEnabled"), enabledArr);

    QJsonArray mapArr;
    for (auto it = seq.tagState.layerTagByLayerId.constBegin(); it != seq.tagState.layerTagByLayerId.constEnd(); ++it)
    {
        QJsonObject e;
        e.insert(QStringLiteral("layerId"), it.key());
        e.insert(QStringLiteral("tagIndex"), it.value());
        mapArr.append(e);
    }
    root.insert(QStringLiteral("layerTagByLayerId"), mapArr);

    const QString outPath = QDir(shotRoot).filePath(QStringLiteral("tags.json"));
    QSaveFile f(outPath);
    if (!f.open(QIODevice::WriteOnly)) {
        dd << QStringLiteral("Failed to open tags.json for write");
        return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        dd << QStringLiteral("Failed to commit tags.json");
        return Status(Status::FAIL, dd);
    }
    return Status::OK;
}

static void loadShotTagsFileIfPresent(const QString& shotRoot, SequenceEntry& seq)
{
    const QString inPath = QDir(shotRoot).filePath(QStringLiteral("tags.json"));
    if (!QFileInfo::exists(inPath)) {
        return;
    }

    QFile f(inPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError jerr;
    const QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &jerr);
    if (jerr.error != QJsonParseError::NoError || !jdoc.isObject()) {
        return;
    }

    const QJsonObject root = jdoc.object();
    seq.tagState.defaultEnabled = root.value(QStringLiteral("defaultEnabled")).toBool(true);

    const QJsonArray enabledArr = root.value(QStringLiteral("tagEnabled")).toArray();
    if (!enabledArr.isEmpty()) {
        seq.tagState.tagEnabled.resize(enabledArr.size());
        for (int i = 0; i < enabledArr.size(); ++i) {
            seq.tagState.tagEnabled[i] = enabledArr.at(i).toBool(true);
        }
    }

    const QJsonArray mapArr = root.value(QStringLiteral("layerTagByLayerId")).toArray();
    if (!mapArr.isEmpty()) {
        seq.tagState.layerTagByLayerId.clear();
        for (const QJsonValue& v : mapArr) {
            const QJsonObject e = v.toObject();
            const int layerId = e.value(QStringLiteral("layerId")).toInt();
            const int tagIndex = e.value(QStringLiteral("tagIndex")).toInt();
            if (layerId > 0 && tagIndex >= 0) {
                seq.tagState.layerTagByLayerId.insert(layerId, tagIndex);
            }
        }
    }
}

}

Status N2DProjectIO::saveSingleShotProject(const QString& filePath,
                                          const Object* object,
                                          const ProgressCallbacks& callbacks)
{
    DebugDetails dd;
    if (!object) {
        dd << QStringLiteral("N2DProjectIO::saveSingleShotProject: object is null");
        return Status(Status::INVALID_ARGUMENT, dd);
    }

    if (callbacks.progressRangeChanged) {
        callbacks.progressRangeChanged(object->totalKeyFrameCount() + 20);
    }

    QTemporaryDir staging;
    if (!staging.isValid()) {
        dd << QStringLiteral("N2DProjectIO::saveSingleShotProject: failed to create staging dir");
        return Status(Status::FAIL, dd);
    }

    const QString root = staging.path();
    const quint64 sceneUid = 1;
    const quint64 shotUid = 1;

    QJsonObject manifest;
    manifest.insert(QStringLiteral("format"), QStringLiteral("n2d"));
    manifest.insert(QStringLiteral("version"), 1);
    manifest.insert(QStringLiteral("projectId"), QStringLiteral("preset"));
    manifest.insert(QStringLiteral("activeSceneIndex"), 0);
    manifest.insert(QStringLiteral("activeShotIndex"), 0);
    manifest.insert(QStringLiteral("sceneLevel"), false);
    manifest.insert(QStringLiteral("browsingSceneIndex"), 0);

    QJsonObject sceneObj;
    sceneObj.insert(QStringLiteral("uid"), QString::number(sceneUid));
    QJsonArray shotsArr;
    {
        QJsonObject shotObj;
        shotObj.insert(QStringLiteral("uid"), QString::number(shotUid));
        shotObj.insert(QStringLiteral("sceneIndex"), 0);
        shotObj.insert(QStringLiteral("shotIndex"), 0);
        shotsArr.append(shotObj);
    }
    sceneObj.insert(QStringLiteral("shots"), shotsArr);

    QJsonArray scenesArr;
    scenesArr.append(sceneObj);
    manifest.insert(QStringLiteral("scenes"), scenesArr);

    const QString manifestPath = QDir(root).filePath(QStringLiteral("manifest.json"));
    {
        QSaveFile mf(manifestPath);
        if (!mf.open(QIODevice::WriteOnly)) {
            dd << QStringLiteral("N2DProjectIO::saveSingleShotProject: failed to write manifest.json");
            return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
        }
        mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
        if (!mf.commit()) {
            dd << QStringLiteral("N2DProjectIO::saveSingleShotProject: failed to commit manifest.json");
            return Status(Status::FAIL, dd);
        }
    }

    int progress = 0;
    if (callbacks.progressChanged) {
        callbacks.progressChanged(progress);
    }

    const QString shotRoot = QDir(root).filePath(QDir(QStringLiteral("shots")).filePath(QString::number(shotUid)));
    Status st = saveObjectToShotFolder(object, shotRoot, callbacks, progress);
    if (!st.ok()) {
        dd.collect(st.details());
        return Status(Status::FAIL, dd);
    }

    Status zipSt = zipFolderToN2D(filePath, root, QStringLiteral("application/x-nurture2d-n2d"));
    if (!zipSt.ok()) {
        dd.collect(zipSt.details());
        return zipSt;
    }

    return Status::OK;
}

std::unique_ptr<Object> N2DProjectIO::loadSingleShotProjectAsObject(const QString& filePath,
                                                                     const ProgressCallbacks& callbacks,
                                                                     Status* outStatus)
{
    DebugDetails dd;

    if (outStatus) {
        *outStatus = Status::OK;
    }

    const QFileInfo srcInfo(filePath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        dd << QStringLiteral("Project file does not exist: %1").arg(filePath);
        if (outStatus) *outStatus = Status(Status::FILE_NOT_FOUND, dd);
        return nullptr;
    }

    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataRoot.isEmpty()) {
        dd << QStringLiteral("Failed to resolve AppLocalDataLocation");
        if (outStatus) *outStatus = Status(Status::FAIL, dd);
        return nullptr;
    }

    const QString canonical = srcInfo.absoluteFilePath();
    const QByteArray hash = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha1).toHex();

    const QString workspaceDir = QDir(appDataRoot).filePath(QDir(QStringLiteral("workspaces")).filePath(QString::fromLatin1(hash)));
    const QString destRoot = QDir(workspaceDir).filePath(QStringLiteral("root/"));
    const QString stampPath = QDir(workspaceDir).filePath(QStringLiteral("source_stamp"));
    const QString sourcePathPath = QDir(workspaceDir).filePath(QStringLiteral("source_path"));
    const QString manifestPath = QDir(destRoot).filePath(QStringLiteral("manifest.json"));

    if (!ensureDirExists(workspaceDir)) {
        dd << QStringLiteral("Failed to create workspace directory: %1").arg(workspaceDir);
        if (outStatus) *outStatus = Status(Status::FAIL, dd);
        return nullptr;
    }
    ensureDirExists(destRoot);

    const qint64 srcMtime = srcInfo.lastModified().toMSecsSinceEpoch();
    const qint64 srcSize = srcInfo.size();
    bool needsExtract = true;
    if (QFileInfo::exists(stampPath) && QFileInfo::exists(manifestPath))
    {
        QFile stampFile(stampPath);
        if (stampFile.open(QIODevice::ReadOnly)) {
            const QByteArray b = stampFile.readAll();
            stampFile.close();
            const QList<QByteArray> parts = b.simplified().split(' ');
            if (parts.size() >= 2) {
                bool ok1 = false;
                bool ok2 = false;
                const qint64 oldMtime = parts.at(0).toLongLong(&ok1);
                const qint64 oldSize = parts.at(1).toLongLong(&ok2);
                if (ok1 && ok2 && oldMtime == srcMtime && oldSize == srcSize) {
                    needsExtract = false;
                }
            }
        }
    }

    if (needsExtract)
    {
        QDir existing(destRoot);
        if (existing.exists()) {
            existing.removeRecursively();
        }
        if (!ensureDirExists(destRoot)) {
            dd << QStringLiteral("Failed to recreate extraction root: %1").arg(destRoot);
            if (outStatus) *outStatus = Status(Status::FAIL, dd);
            return nullptr;
        }

        Status stUnzip = MiniZ::uncompressFolderSelective(filePath, destRoot, [&](const QString& relPath) {
            const QString p = relPath.toLower();
            if (p.contains(QStringLiteral("/data/")) && (p.endsWith(QStringLiteral(".png")) || p.endsWith(QStringLiteral(".hyb")))) {
                return false;
            }
            return true;
        });
        if (!stUnzip.ok()) {
            dd.collect(stUnzip.details());
            if (outStatus) *outStatus = Status(Status::ERROR_MINIZ_FAIL, dd);
            return nullptr;
        }

        QSaveFile stampOut(stampPath);
        if (stampOut.open(QIODevice::WriteOnly)) {
            stampOut.write(QByteArray::number(srcMtime));
            stampOut.write(" ");
            stampOut.write(QByteArray::number(srcSize));
            stampOut.commit();
        }

        QSaveFile srcOut(sourcePathPath);
        if (srcOut.open(QIODevice::WriteOnly)) {
            srcOut.write(srcInfo.absoluteFilePath().toUtf8());
            srcOut.write("\n");
            srcOut.commit();
        }
    }
    else
    {
        if (!QFileInfo::exists(sourcePathPath)) {
            QSaveFile srcOut(sourcePathPath);
            if (srcOut.open(QIODevice::WriteOnly)) {
                srcOut.write(srcInfo.absoluteFilePath().toUtf8());
                srcOut.write("\n");
                srcOut.commit();
            }
        }
    }

    QFile mf(manifestPath);
    if (!mf.open(QIODevice::ReadOnly)) {
        dd << QStringLiteral("Missing manifest.json");
        if (outStatus) *outStatus = Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
        return nullptr;
    }

    const QByteArray manifestBytes = mf.readAll();
    mf.close();

    QJsonParseError jerr;
    const QJsonDocument jdoc = QJsonDocument::fromJson(manifestBytes, &jerr);
    if (jerr.error != QJsonParseError::NoError || !jdoc.isObject()) {
        dd << QStringLiteral("Failed to parse manifest.json");
        if (outStatus) *outStatus = Status(Status::ERROR_INVALID_XML_FILE, dd);
        return nullptr;
    }

    const QJsonObject manifest = jdoc.object();
    const QJsonArray scenesArr = manifest.value(QStringLiteral("scenes")).toArray();
    if (scenesArr.isEmpty()) {
        dd << QStringLiteral("manifest.json has no scenes");
        if (outStatus) *outStatus = Status(Status::FAIL, dd);
        return nullptr;
    }

    const int activeSceneIndex = manifest.value(QStringLiteral("activeSceneIndex")).toInt(0);
    const int activeShotIndex = manifest.value(QStringLiteral("activeShotIndex")).toInt(0);
    const int sceneIndex = qBound(0, activeSceneIndex, qMax(0, scenesArr.size() - 1));

    const QJsonObject sceneObj = scenesArr.at(sceneIndex).toObject();
    const QJsonArray shotsArr = sceneObj.value(QStringLiteral("shots")).toArray();
    if (shotsArr.isEmpty()) {
        dd << QStringLiteral("manifest.json has no shots");
        if (outStatus) *outStatus = Status(Status::FAIL, dd);
        return nullptr;
    }

    const int shotIndex = qBound(0, activeShotIndex, qMax(0, shotsArr.size() - 1));
    const QJsonObject shotObj = shotsArr.at(shotIndex).toObject();
    const quint64 shotUid = shotObj.value(QStringLiteral("uid")).toString().toULongLong();

    const QString shotExtractRoot = QDir(destRoot).filePath(QDir(QStringLiteral("shots")).filePath(QString::number(shotUid)));
    std::unique_ptr<Object> obj(new Object());

    obj->setOwnsWorkingDir(false);

    int progress = 0;
    Status stLoad = loadObjectFromShotFolder(shotExtractRoot, filePath, obj.get(), callbacks, progress);
    if (!stLoad.ok()) {
        dd.collect(stLoad.details());
        if (outStatus) *outStatus = Status(Status::FAIL, dd);
        return nullptr;
    }

    return obj;
}

Status N2DProjectIO::saveProject(const QString& filePath,
                                Editor* editor,
                                SceneSequenceManager* sceneSequenceManager,
                                const ProgressCallbacks& callbacks)
{
    DebugDetails dd;
    if (!editor || !sceneSequenceManager) {
        dd << QStringLiteral("Missing editor or sceneSequenceManager");
        return Status(Status::INVALID_ARGUMENT, dd);
    }

    sceneSequenceManager->ensureInitialized();

    sceneSequenceManager->captureTagStateFromUIForActiveShot();

    const std::vector<SceneEntry>& scenes = sceneSequenceManager->scenes();

    int totalProgress = 0;
    for (const SceneEntry& scene : scenes) {
        for (const SequenceEntry& seq : scene.sequences) {
            Object* obj = sceneSequenceManager->objectForSequence(seq.sceneIndex, seq.sequenceIndex);
            if (obj) {
                totalProgress += obj->totalKeyFrameCount();
            }
            totalProgress += 2;
        }
    }

    if (callbacks.progressRangeChanged) {
        callbacks.progressRangeChanged(totalProgress + 10);
    }

    QTemporaryDir staging;
    if (!staging.isValid()) {
        dd << QStringLiteral("Failed to create staging directory");
        return Status(Status::FAIL, dd);
    }

    const QString root = staging.path();

    const QString projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonObject manifest;
    manifest.insert(QStringLiteral("format"), QStringLiteral("n2d"));
    manifest.insert(QStringLiteral("version"), 1);
    manifest.insert(QStringLiteral("projectId"), projectId);
    manifest.insert(QStringLiteral("activeSceneIndex"), sceneSequenceManager->activeSceneIndex());
    manifest.insert(QStringLiteral("activeShotIndex"), sceneSequenceManager->activeSequenceIndex());
    manifest.insert(QStringLiteral("sceneLevel"), sceneSequenceManager->isAtSceneLevel());
    manifest.insert(QStringLiteral("browsingSceneIndex"), sceneSequenceManager->browsingSceneIndex());

    QJsonArray scenesArr;
    for (const SceneEntry& scene : scenes)
    {
        QJsonObject sceneObj;
        sceneObj.insert(QStringLiteral("uid"), QString::number(scene.uid));
        QJsonArray shotsArr;
        for (const SequenceEntry& seq : scene.sequences)
        {
            QJsonObject shotObj;
            shotObj.insert(QStringLiteral("uid"), QString::number(seq.uid));
            shotObj.insert(QStringLiteral("sceneIndex"), seq.sceneIndex);
            shotObj.insert(QStringLiteral("shotIndex"), seq.sequenceIndex);
            shotsArr.append(shotObj);
        }
        sceneObj.insert(QStringLiteral("shots"), shotsArr);
        scenesArr.append(sceneObj);
    }
    manifest.insert(QStringLiteral("scenes"), scenesArr);

    const QString manifestPath = QDir(root).filePath(QStringLiteral("manifest.json"));
    {
        QSaveFile mf(manifestPath);
        if (!mf.open(QIODevice::WriteOnly)) {
            dd << QStringLiteral("Failed to write manifest.json");
            return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
        }
        const QByteArray bytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
        mf.write(bytes);
        if (!mf.commit()) {
            dd << QStringLiteral("Failed to commit manifest.json");
            return Status(Status::FAIL, dd);
        }
    }

    int progress = 0;
    if (callbacks.progressChanged) {
        callbacks.progressChanged(progress);
    }

    for (const SceneEntry& scene : scenes)
    {
        for (const SequenceEntry& seq : scene.sequences)
        {
            Object* obj = sceneSequenceManager->objectForSequence(seq.sceneIndex, seq.sequenceIndex);
            if (!obj) {
                dd << QString("Missing object for scene %1 shot %2").arg(seq.sceneIndex).arg(seq.sequenceIndex);
                return Status(Status::FAIL, dd);
            }

            const QString shotRoot = QDir(root).filePath(QDir(QStringLiteral("shots")).filePath(QString::number(seq.uid)));
            Status st = saveObjectToShotFolder(obj, shotRoot, callbacks, progress);
            if (!st.ok()) {
                dd.collect(st.details());
                return Status(Status::FAIL, dd);
            }

            Status stTags = saveShotTagsFile(shotRoot, seq);
            if (!stTags.ok()) {
                dd.collect(stTags.details());
                return Status(Status::FAIL, dd);
            }
        }
    }

    Status zipSt = zipFolderToN2D(filePath, root, QStringLiteral("application/x-nurture2d-n2d"));
    if (!zipSt.ok()) {
        dd.collect(zipSt.details());
        return zipSt;
    }

    if (editor->object()) {
        editor->object()->setFilePath(filePath);
    }

    return Status::OK;
}

Status N2DProjectIO::loadProject(const QString& filePath,
                                Editor* editor,
                                SceneSequenceManager* sceneSequenceManager,
                                const ProgressCallbacks& callbacks)
{
    DebugDetails dd;
    if (!editor || !sceneSequenceManager) {
        dd << QStringLiteral("Missing editor or sceneSequenceManager");
        return Status(Status::INVALID_ARGUMENT, dd);
    }

    const QFileInfo srcInfo(filePath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        dd << QStringLiteral("Project file does not exist: %1").arg(filePath);
        return Status(Status::FILE_NOT_FOUND, dd);
    }

    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataRoot.isEmpty()) {
        dd << QStringLiteral("Failed to resolve AppLocalDataLocation");
        return Status(Status::FAIL, dd);
    }

    const QString canonical = srcInfo.absoluteFilePath();
    const QByteArray hash = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha1).toHex();

    const QString workspaceDir = QDir(appDataRoot).filePath(QDir(QStringLiteral("workspaces")).filePath(QString::fromLatin1(hash)));
    const QString destRoot = QDir(workspaceDir).filePath(QStringLiteral("root/"));
    const QString stampPath = QDir(workspaceDir).filePath(QStringLiteral("source_stamp"));
    const QString sourcePathPath = QDir(workspaceDir).filePath(QStringLiteral("source_path"));
    const QString manifestPath = QDir(destRoot).filePath(QStringLiteral("manifest.json"));

    sceneSequenceManager->setWorkspaceDirectory(workspaceDir, false);

    if (!ensureDirExists(workspaceDir)) {
        dd << QStringLiteral("Failed to create workspace directory: %1").arg(workspaceDir);
        return Status(Status::FAIL, dd);
    }
    ensureDirExists(destRoot);

    const qint64 srcMtime = srcInfo.lastModified().toMSecsSinceEpoch();
    const qint64 srcSize = srcInfo.size();
    bool needsExtract = true;
    if (QFileInfo::exists(stampPath) && QFileInfo::exists(manifestPath))
    {
        QFile stampFile(stampPath);
        if (stampFile.open(QIODevice::ReadOnly)) {
            const QByteArray b = stampFile.readAll();
            stampFile.close();
            const QList<QByteArray> parts = b.simplified().split(' ');
            if (parts.size() >= 2) {
                bool ok1 = false;
                bool ok2 = false;
                const qint64 oldMtime = parts.at(0).toLongLong(&ok1);
                const qint64 oldSize = parts.at(1).toLongLong(&ok2);
                if (ok1 && ok2 && oldMtime == srcMtime && oldSize == srcSize) {
                    needsExtract = false;
                }
            }
        }
    }

    if (needsExtract)
    {
        QDir existing(destRoot);
        if (existing.exists()) {
            existing.removeRecursively();
        }
        if (!ensureDirExists(destRoot)) {
            dd << QStringLiteral("Failed to recreate extraction root: %1").arg(destRoot);
            return Status(Status::FAIL, dd);
        }

        Status stUnzip = MiniZ::uncompressFolderSelective(filePath, destRoot, [&](const QString& relPath) {
            const QString p = relPath.toLower();
            if (p.contains(QStringLiteral("/data/")) && (p.endsWith(QStringLiteral(".png")) || p.endsWith(QStringLiteral(".hyb")))) {
                return false;
            }
            return true;
        });
        if (!stUnzip.ok()) {
            dd.collect(stUnzip.details());
            return Status(Status::ERROR_MINIZ_FAIL, dd);
        }

        QSaveFile stampOut(stampPath);
        if (stampOut.open(QIODevice::WriteOnly)) {
            stampOut.write(QByteArray::number(srcMtime));
            stampOut.write(" ");
            stampOut.write(QByteArray::number(srcSize));
            stampOut.commit();
        }

        QSaveFile srcOut(sourcePathPath);
        if (srcOut.open(QIODevice::WriteOnly)) {
            srcOut.write(srcInfo.absoluteFilePath().toUtf8());
            srcOut.write("\n");
            srcOut.commit();
        }
    }
    else
    {
        if (!QFileInfo::exists(sourcePathPath)) {
            QSaveFile srcOut(sourcePathPath);
            if (srcOut.open(QIODevice::WriteOnly)) {
                srcOut.write(srcInfo.absoluteFilePath().toUtf8());
                srcOut.write("\n");
                srcOut.commit();
            }
        }
    }

    QFile mf(manifestPath);
    if (!mf.open(QIODevice::ReadOnly)) {
        dd << QString("Missing manifest.json");
        return Status(Status::ERROR_FILE_CANNOT_OPEN, dd);
    }

    const QByteArray manifestBytes = mf.readAll();
    mf.close();

    QJsonParseError jerr;
    const QJsonDocument jdoc = QJsonDocument::fromJson(manifestBytes, &jerr);
    if (jerr.error != QJsonParseError::NoError || !jdoc.isObject()) {
        dd << QString("Failed to parse manifest.json");
        return Status(Status::ERROR_INVALID_XML_FILE, dd);
    }

    const QJsonObject manifest = jdoc.object();
    const int activeSceneIndex = manifest.value(QStringLiteral("activeSceneIndex")).toInt(0);
    const int activeShotIndex = manifest.value(QStringLiteral("activeShotIndex")).toInt(0);
    const bool sceneLevel = manifest.value(QStringLiteral("sceneLevel")).toBool(false);
    const int browsingSceneIndex = manifest.value(QStringLiteral("browsingSceneIndex")).toInt(0);

    const QJsonArray scenesArr = manifest.value(QStringLiteral("scenes")).toArray();

    int estimatedProgress = 5;
    for (const QJsonValue& vScene : scenesArr) {
        const QJsonObject sceneObj = vScene.toObject();
        estimatedProgress += sceneObj.value(QStringLiteral("shots")).toArray().size() * 20;
    }

    if (callbacks.progressRangeChanged) {
        callbacks.progressRangeChanged(estimatedProgress);
    }

    int progress = 0;
    if (callbacks.progressChanged) {
        callbacks.progressChanged(progress);
    }

    std::vector<SceneEntry> scenes;
    scenes.reserve(static_cast<size_t>(scenesArr.size()));

    for (int si = 0; si < scenesArr.size(); ++si)
    {
        const QJsonObject sceneObj = scenesArr.at(si).toObject();
        SceneEntry scene;
        scene.uid = sceneObj.value(QStringLiteral("uid")).toString().toULongLong();
        scene.sceneIndex = si;

        const QJsonArray shotsArr = sceneObj.value(QStringLiteral("shots")).toArray();
        scene.sequences.reserve(static_cast<size_t>(shotsArr.size()));

        for (int qi = 0; qi < shotsArr.size(); ++qi)
        {
            const QJsonObject shotObj = shotsArr.at(qi).toObject();
            SequenceEntry seq;
            seq.uid = shotObj.value(QStringLiteral("uid")).toString().toULongLong();
            seq.sceneIndex = si;
            seq.sequenceIndex = qi;

            const QString shotExtractRoot = QDir(destRoot).filePath(QDir(QStringLiteral("shots")).filePath(QString::number(seq.uid)));

            loadShotTagsFileIfPresent(shotExtractRoot, seq);

            std::unique_ptr<Object> obj(new Object());

            obj->setOwnsWorkingDir(false);

            Status stLoad = loadObjectFromShotFolder(shotExtractRoot, filePath, obj.get(), callbacks, progress);
            if (!stLoad.ok()) {
                dd.collect(stLoad.details());
                return Status(Status::FAIL, dd);
            }

            seq.object = std::move(obj);
            scene.sequences.push_back(std::move(seq));
        }

        scenes.push_back(std::move(scene));
    }

    Status stApply = sceneSequenceManager->loadProjectModel(std::move(scenes), activeSceneIndex, activeShotIndex, sceneLevel, browsingSceneIndex);
    if (!stApply.ok()) {
        dd.collect(stApply.details());
        return Status(Status::FAIL, dd);
    }

    return Status::OK;
}
