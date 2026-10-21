#include "scenesequencemanager.h"

#include "editor.h"
#include "object.h"
#include "playbackmanager.h"

#include "timelinecells.h"

#include <algorithm>
#include <QtGlobal>
#include <QSettings>
#include <QFileInfo>
#include <QCryptographicHash>
 #include <QStandardPaths>
 #include <QDir>
 #include <QUuid>

SceneSequenceManager::SceneSequenceManager(Editor* editor, QObject* parent)
    : QObject(parent)
    , mEditor(editor)
{
}

void SceneSequenceManager::ensureWorkspaceInitialized()
{
    if (!mWorkspaceDir.isEmpty()) {
        return;
    }

    if (mProjectFilePath.isEmpty()) {
        startTransientWorkspace();
    } else {
        ensureWorkspaceForProjectFile(mProjectFilePath);
        mWorkspaceTransient = false;
    }
}

void SceneSequenceManager::startTransientWorkspace()
{
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataRoot.isEmpty()) {
        mWorkspaceDir.clear();
        mWorkspaceTransient = true;
        return;
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(10);
    const QString workspaceDir = QDir(appDataRoot).filePath(QDir(QStringLiteral("workspaces")).filePath(QStringLiteral("untitled_%1").arg(id)));

    QDir dir;
    dir.mkpath(workspaceDir);

    mWorkspaceDir = workspaceDir;
    mWorkspaceTransient = true;
}

void SceneSequenceManager::ensureWorkspaceForProjectFile(const QString& projectFilePath)
{
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataRoot.isEmpty()) {
        return;
    }

    const QFileInfo srcInfo(projectFilePath);
    const QString canonical = srcInfo.absoluteFilePath();
    const QByteArray hash = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString workspaceDir = QDir(appDataRoot).filePath(QDir(QStringLiteral("workspaces")).filePath(QString::fromLatin1(hash)));

    QDir dir;
    dir.mkpath(workspaceDir);

    mWorkspaceDir = workspaceDir;
}

void SceneSequenceManager::setWorkspaceDirectory(const QString& workspaceDir, bool transient)
{
    mWorkspaceDir = workspaceDir;
    mWorkspaceTransient = transient;
}

void SceneSequenceManager::setWorkspaceTransient(bool transient)
{
    mWorkspaceTransient = transient;
}

QString SceneSequenceManager::workspaceAutoSavePath() const
{
    if (mWorkspaceDir.isEmpty()) {
        return QString();
    }
    return QDir(mWorkspaceDir).filePath(QStringLiteral("autosave.n2d"));
}

void SceneSequenceManager::setProjectFilePath(const QString& filePath)
{
    mProjectFilePath = filePath;

    if (!mProjectFilePath.isEmpty()) {
        ensureWorkspaceForProjectFile(mProjectFilePath);
        mWorkspaceTransient = false;
    }

    if (mEditor && mEditor->object()) {
        mEditor->object()->setFilePath(filePath);
    }

    // Propagate the project path to all stored shot objects.
    for (SceneEntry& scene : mScenes)
    {
        for (SequenceEntry& seq : scene.sequences)
        {
            if (seq.object) {
                seq.object->setFilePath(filePath);
            }
        }
    }
}

Status SceneSequenceManager::loadProjectModel(std::vector<SceneEntry>&& scenes,
                                             int activeSceneIndex,
                                             int activeSequenceIndex,
                                             bool sceneLevel,
                                             int browsingSceneIndex)
{
    DebugDetails dd;

    if (!mEditor) {
        dd << QStringLiteral("SceneSequenceManager::loadProjectModel: editor is null");
        return Status(Status::INVALID_ARGUMENT, dd);
    }

    if (scenes.empty()) {
        dd << QStringLiteral("SceneSequenceManager::loadProjectModel: scenes is empty");
        return Status(Status::INVALID_ARGUMENT, dd);
    }

    for (const SceneEntry& s : scenes) {
        if (s.sequences.empty()) {
            dd << QStringLiteral("SceneSequenceManager::loadProjectModel: scene has no shots");
            return Status(Status::INVALID_ARGUMENT, dd);
        }
    }

    // Move current active object back into the existing model (if any).
    // This keeps ownership consistent before we replace the model.
    if (!mScenes.empty())
    {
        ensureSequenceExists(mActiveSceneIndex, mActiveSequenceIndex);
        std::unique_ptr<Object> prev = mEditor->takeObject();
        mScenes[mActiveSceneIndex].sequences[mActiveSequenceIndex].object = std::move(prev);
    }

    mScenes = std::move(scenes);

    // Reindex and compute next UID counters.
    reindexScenes();

    quint64 maxSceneUid = 0;
    quint64 maxSeqUid = 0;
    for (const SceneEntry& s : mScenes) {
        maxSceneUid = qMax(maxSceneUid, s.uid);
        for (const SequenceEntry& q : s.sequences) {
            maxSeqUid = qMax(maxSeqUid, q.uid);
        }
    }
    mNextSceneUid = maxSceneUid + 1;
    mNextSequenceUid = maxSeqUid + 1;

    // Clamp and set browsing/active indices.
    const int sceneCount = static_cast<int>(mScenes.size());
    mActiveSceneIndex = qBound(0, activeSceneIndex, qMax(0, sceneCount - 1));
    const int seqCount = static_cast<int>(mScenes[mActiveSceneIndex].sequences.size());
    mActiveSequenceIndex = qBound(0, activeSequenceIndex, qMax(0, seqCount - 1));

    mSceneLevel = sceneLevel;
    mBrowsingSceneIndex = qBound(0, browsingSceneIndex, qMax(0, sceneCount - 1));

    // Activate the requested shot into the editor.
    mIsActivatingSequenceSwitch = true;

    if (mActiveSceneIndex < 0 || mActiveSceneIndex >= static_cast<int>(mScenes.size())) {
        mIsActivatingSequenceSwitch = false;
        dd << QStringLiteral("SceneSequenceManager::loadProjectModel: active scene index out of range");
        return Status(Status::FAIL, dd);
    }
    if (mActiveSequenceIndex < 0 || mActiveSequenceIndex >= static_cast<int>(mScenes[mActiveSceneIndex].sequences.size())) {
        mIsActivatingSequenceSwitch = false;
        dd << QStringLiteral("SceneSequenceManager::loadProjectModel: active shot index out of range");
        return Status(Status::FAIL, dd);
    }

    SequenceEntry& activeSeq = mScenes[mActiveSceneIndex].sequences[mActiveSequenceIndex];
    if (!activeSeq.object) {
        activeSeq.object = createDefaultObject();
    }
    mEditor->setObject(activeSeq.object.release());
    mIsActivatingSequenceSwitch = false;

    applyTagStateToUIForActiveShot();

    saveUiState();

    emit activeSequenceChanged();
    emit browsingLevelChanged();
    return Status::OK;
}

QString SceneSequenceManager::settingsProjectKey() const
{
    if (!mEditor || !mEditor->object()) {
        return QStringLiteral("Untitled");
    }
    const QString fp = mEditor->object()->filePath();
    if (fp.isEmpty()) {
        return QStringLiteral("Untitled");
    }

    // Avoid storing absolute paths as settings keys.
    const QFileInfo fi(fp);
    const QString base = fi.fileName().isEmpty() ? QStringLiteral("Project") : fi.fileName();
    const QByteArray digest = QCryptographicHash::hash(fp.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("%1_%2").arg(base, QString::fromLatin1(digest.left(10)));
}

void SceneSequenceManager::loadUiState()
{
    QSettings s;
    s.beginGroup(QStringLiteral("ScenesAndShots"));
    s.beginGroup(settingsProjectKey());

    const bool hasActive = s.contains(QStringLiteral("activeScene")) || s.contains(QStringLiteral("activeSequence"));
    const int activeScene = s.value(QStringLiteral("activeScene"), 0).toInt();
    const int activeSeq = s.value(QStringLiteral("activeSequence"), 0).toInt();

    const bool hasExplicitBrowse = s.contains(QStringLiteral("sceneLevel")) || s.contains(QStringLiteral("browsingScene"));
    const bool sceneLevel = s.value(QStringLiteral("sceneLevel"), false).toBool();
    const int browsingScene = s.value(QStringLiteral("browsingScene"), 0).toInt();

    s.endGroup();
    s.endGroup();

    if (hasActive) {
        mActiveSceneIndex = qMax(0, activeScene);
        mActiveSequenceIndex = qMax(0, activeSeq);
    } else {
        mActiveSceneIndex = 0;
        mActiveSequenceIndex = 0;
    }

    if (hasExplicitBrowse) {
        mSceneLevel = sceneLevel;
        mBrowsingSceneIndex = qMax(0, browsingScene);
    } else {
        mSceneLevel = false;
        mBrowsingSceneIndex = mActiveSceneIndex;
    }
}

void SceneSequenceManager::saveUiState() const
{
    if (!mEditor) {
        return;
    }

    QSettings s;
    s.beginGroup(QStringLiteral("ScenesAndShots"));
    s.beginGroup(settingsProjectKey());

    s.setValue(QStringLiteral("activeScene"), mActiveSceneIndex);
    s.setValue(QStringLiteral("activeSequence"), mActiveSequenceIndex);
    s.setValue(QStringLiteral("sceneLevel"), mSceneLevel);
    s.setValue(QStringLiteral("browsingScene"), mBrowsingSceneIndex);

    s.endGroup();
    s.endGroup();
}

void SceneSequenceManager::ensureInitialized()
{
    if (!mScenes.empty()) {
        return;
    }

    ensureWorkspaceInitialized();

    // Establish indices.
    // For unsaved projects (no project file path), do NOT apply persisted UI state.
    // This ensures a brand-new project always starts with Scene 1 / Shot 1 active.
    mActiveSceneIndex = 0;
    mActiveSequenceIndex = 0;
    mBrowsingSceneIndex = 0;
    mSceneLevel = false;
    if (mEditor && mEditor->object()) {
        if (!mEditor->object()->filePath().isEmpty() || !mProjectFilePath.isEmpty()) {
            loadUiState();
        }
    }

    // Build enough scenes/sequences so that the active sequence exists.
    // The active sequence is represented by the Object currently owned by Editor.
    ensureSequenceExists(mActiveSceneIndex, mActiveSequenceIndex);
    ensureSceneExists(mBrowsingSceneIndex);
}

void SceneSequenceManager::onExternalObjectLoaded()
{
    if (mIsActivatingSequenceSwitch) {
        return;
    }

    // External object loads (File/New, preset load, etc.) represent a new project context.
    // Reset project-level file path so Save behaves like Save As until the first save.
    mProjectFilePath.clear();

    startTransientWorkspace();

    mScenes.clear();
    mActiveSceneIndex = 0;
    mActiveSequenceIndex = 0;
    mBrowsingSceneIndex = 0;
    mSceneLevel = false;
    mNextSceneUid = 1;
    mNextSequenceUid = 1;

    ensureInitialized();

    emit browsingLevelChanged();
    emit activeSequenceChanged();
}

void SceneSequenceManager::browseScenes()
{
    ensureInitialized();

    if (!mSceneLevel) {
        mSceneLevel = true;
        saveUiState();
        emit browsingLevelChanged();
    }
}

void SceneSequenceManager::browseSequences(int sceneIndex)
{
    ensureInitialized();
    ensureSceneExists(sceneIndex);

    mSceneLevel = false;
    mBrowsingSceneIndex = sceneIndex;
    saveUiState();
    emit browsingLevelChanged();
}

void SceneSequenceManager::restoreBrowsingState(bool sceneLevel, int browsingSceneIndex)
{
    ensureInitialized();
    ensureSceneExists(browsingSceneIndex);

    mSceneLevel = sceneLevel;
    mBrowsingSceneIndex = browsingSceneIndex;
    saveUiState();

    emit browsingLevelChanged();
}

int SceneSequenceManager::insertSceneAfter(int sceneIndex)
{
    ensureInitialized();

    const int count = static_cast<int>(mScenes.size());
    const int base = qBound(0, sceneIndex, qMax(0, count - 1));
    const int insertPos = qBound(0, base + 1, count);

    SceneEntry scene;
    scene.uid = mNextSceneUid++;
    scene.sceneIndex = 0;

    SequenceEntry seq;
    seq.uid = mNextSequenceUid++;
    seq.sceneIndex = 0;
    seq.sequenceIndex = 0;
    seq.object = createDefaultObject();
    scene.sequences.push_back(std::move(seq));

    mScenes.insert(mScenes.begin() + insertPos, std::move(scene));

    if (insertPos <= mActiveSceneIndex) {
        mActiveSceneIndex++;
    }
    if (insertPos <= mBrowsingSceneIndex) {
        mBrowsingSceneIndex++;
    }

    reindexScenes();

    emit browsingLevelChanged();
    return insertPos;
}

bool SceneSequenceManager::moveScenes(const QVector<int>& sceneIndices, int insertPos)
{
    ensureInitialized();

    if (sceneIndices.isEmpty()) {
        return false;
    }

    QVector<int> indices = sceneIndices;
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    const int count = static_cast<int>(mScenes.size());
    for (int idx : indices) {
        if (idx < 0 || idx >= count) {
            return false;
        }
    }

    int clampedInsert = qBound(0, insertPos, count);

    quint64 activeSceneUid = 0;
    quint64 activeSeqUid = 0;
    if (mActiveSceneIndex >= 0 && mActiveSceneIndex < static_cast<int>(mScenes.size())) {
        activeSceneUid = mScenes[mActiveSceneIndex].uid;
        if (mActiveSequenceIndex >= 0 && mActiveSequenceIndex < static_cast<int>(mScenes[mActiveSceneIndex].sequences.size())) {
            activeSeqUid = mScenes[mActiveSceneIndex].sequences[mActiveSequenceIndex].uid;
        }
    }

    quint64 browsingSceneUid = 0;
    if (mBrowsingSceneIndex >= 0 && mBrowsingSceneIndex < static_cast<int>(mScenes.size())) {
        browsingSceneUid = mScenes[mBrowsingSceneIndex].uid;
    }

    std::vector<SceneEntry> moving;
    moving.reserve(static_cast<size_t>(indices.size()));

    for (int i = indices.size() - 1; i >= 0; --i)
    {
        const int idx = indices[i];
        moving.push_back(std::move(mScenes[idx]));
        mScenes.erase(mScenes.begin() + idx);
    }
    std::reverse(moving.begin(), moving.end());

    for (int idx : indices) {
        if (idx < clampedInsert) {
            clampedInsert--;
        }
    }
    clampedInsert = qBound(0, clampedInsert, static_cast<int>(mScenes.size()));

    mScenes.insert(mScenes.begin() + clampedInsert, std::make_move_iterator(moving.begin()), std::make_move_iterator(moving.end()));

    reindexScenes();

    if (activeSceneUid != 0)
    {
        for (int i = 0; i < static_cast<int>(mScenes.size()); ++i)
        {
            if (mScenes[i].uid == activeSceneUid) {
                mActiveSceneIndex = i;
                if (activeSeqUid != 0) {
                    for (int j = 0; j < static_cast<int>(mScenes[i].sequences.size()); ++j) {
                        if (mScenes[i].sequences[j].uid == activeSeqUid) {
                            mActiveSequenceIndex = j;
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    if (browsingSceneUid != 0)
    {
        for (int i = 0; i < static_cast<int>(mScenes.size()); ++i)
        {
            if (mScenes[i].uid == browsingSceneUid) {
                mBrowsingSceneIndex = i;
                break;
            }
        }
    }

    emit browsingLevelChanged();
    emit activeSequenceChanged();
    return true;
}

bool SceneSequenceManager::removeScene(int sceneIndex)
{
    ensureInitialized();
    if (mScenes.size() <= 1) {
        return false;
    }

    const int count = static_cast<int>(mScenes.size());
    const int idx = qBound(0, sceneIndex, qMax(0, count - 1));

    if (idx == mActiveSceneIndex) {
        const int nextScene = (idx == 0) ? 1 : (idx - 1);
        activateSequence(nextScene, 0);
    }

    if (idx < mActiveSceneIndex) {
        mActiveSceneIndex--;
    }
    if (idx < mBrowsingSceneIndex) {
        mBrowsingSceneIndex--;
    }

    mScenes.erase(mScenes.begin() + idx);

    if (mActiveSceneIndex < 0) mActiveSceneIndex = 0;
    if (mActiveSceneIndex >= static_cast<int>(mScenes.size())) mActiveSceneIndex = static_cast<int>(mScenes.size()) - 1;
    if (mBrowsingSceneIndex < 0) mBrowsingSceneIndex = 0;
    if (mBrowsingSceneIndex >= static_cast<int>(mScenes.size())) mBrowsingSceneIndex = static_cast<int>(mScenes.size()) - 1;

    reindexScenes();

    emit browsingLevelChanged();
    emit activeSequenceChanged();
    return true;
}

int SceneSequenceManager::insertSequenceAfter(int sceneIndex, int sequenceIndex)
{
    ensureInitialized();
    ensureSceneExists(sceneIndex);

    SceneEntry& scene = mScenes[sceneIndex];
    const int count = static_cast<int>(scene.sequences.size());
    const int base = qBound(0, sequenceIndex, qMax(0, count - 1));
    const int insertPos = qBound(0, base + 1, count);

    SequenceEntry seq;
    seq.uid = mNextSequenceUid++;
    seq.sceneIndex = sceneIndex;
    seq.sequenceIndex = 0;
    seq.object = createDefaultObject();

    scene.sequences.insert(scene.sequences.begin() + insertPos, std::move(seq));

    if (sceneIndex == mActiveSceneIndex && insertPos <= mActiveSequenceIndex) {
        mActiveSequenceIndex++;
    }

    reindexSequences(sceneIndex);

    emit browsingLevelChanged();
    return insertPos;
}

bool SceneSequenceManager::removeSequence(int sceneIndex, int sequenceIndex)
{
    ensureInitialized();
    ensureSceneExists(sceneIndex);

    SceneEntry& scene = mScenes[sceneIndex];
    if (scene.sequences.size() <= 1) {
        return false;
    }

    const int count = static_cast<int>(scene.sequences.size());
    const int idx = qBound(0, sequenceIndex, qMax(0, count - 1));

    if (sceneIndex == mActiveSceneIndex && idx == mActiveSequenceIndex) {
        const int nextSeq = (idx == 0) ? 1 : (idx - 1);
        activateSequence(sceneIndex, nextSeq);
    }

    if (sceneIndex == mActiveSceneIndex && idx < mActiveSequenceIndex) {
        mActiveSequenceIndex--;
    }

    scene.sequences.erase(scene.sequences.begin() + idx);

    if (mActiveSceneIndex == sceneIndex) {
        if (mActiveSequenceIndex < 0) mActiveSequenceIndex = 0;
        if (mActiveSequenceIndex >= static_cast<int>(scene.sequences.size())) mActiveSequenceIndex = static_cast<int>(scene.sequences.size()) - 1;
    }

    reindexSequences(sceneIndex);

    emit browsingLevelChanged();
    emit activeSequenceChanged();
    return true;
}

bool SceneSequenceManager::moveSequences(int sceneIndex, const QVector<int>& sequenceIndices, int insertPos)
{
    ensureInitialized();
    ensureSceneExists(sceneIndex);

    if (sequenceIndices.isEmpty()) {
        return false;
    }

    SceneEntry& scene = mScenes[sceneIndex];
    const int count = static_cast<int>(scene.sequences.size());

    QVector<int> indices = sequenceIndices;
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    for (int idx : indices) {
        if (idx < 0 || idx >= count) {
            return false;
        }
    }

    int clampedInsert = qBound(0, insertPos, count);

    quint64 activeSeqUid = 0;
    if (mActiveSceneIndex == sceneIndex && mActiveSequenceIndex >= 0 && mActiveSequenceIndex < static_cast<int>(scene.sequences.size())) {
        activeSeqUid = scene.sequences[mActiveSequenceIndex].uid;
    }

    std::vector<SequenceEntry> moving;
    moving.reserve(static_cast<size_t>(indices.size()));

    for (int i = indices.size() - 1; i >= 0; --i)
    {
        const int idx = indices[i];
        moving.push_back(std::move(scene.sequences[idx]));
        scene.sequences.erase(scene.sequences.begin() + idx);
    }
    std::reverse(moving.begin(), moving.end());

    for (int idx : indices) {
        if (idx < clampedInsert) {
            clampedInsert--;
        }
    }
    clampedInsert = qBound(0, clampedInsert, static_cast<int>(scene.sequences.size()));

    scene.sequences.insert(scene.sequences.begin() + clampedInsert, std::make_move_iterator(moving.begin()), std::make_move_iterator(moving.end()));

    reindexSequences(sceneIndex);

    if (activeSeqUid != 0 && mActiveSceneIndex == sceneIndex)
    {
        for (int i = 0; i < static_cast<int>(scene.sequences.size()); ++i)
        {
            if (scene.sequences[i].uid == activeSeqUid) {
                mActiveSequenceIndex = i;
                break;
            }
        }
    }

    emit browsingLevelChanged();
    emit activeSequenceChanged();
    return true;
}

void SceneSequenceManager::activateSequence(int sceneIndex, int sequenceIndex)
{
    ensureInitialized();
    ensureSequenceExists(sceneIndex, sequenceIndex);

    if (!mEditor) {
        return;
    }

    if (mEditor->playback() && mEditor->playback()->isPlaying()) {
        return;
    }

    if (sceneIndex == mActiveSceneIndex && sequenceIndex == mActiveSequenceIndex) {
        // Already active; just ensure browsing view is on this scene.
        mSceneLevel = false;
        mBrowsingSceneIndex = sceneIndex;
        emit browsingLevelChanged();
        return;
    }

    captureTagStateFromUIForActiveShot();

    mIsActivatingSequenceSwitch = true;

    // Move current active object from the editor into the model.
    ensureSequenceExists(mActiveSceneIndex, mActiveSequenceIndex);
    {
        std::unique_ptr<Object> prev = mEditor->takeObject();
        mScenes[mActiveSceneIndex].sequences[mActiveSequenceIndex].object = std::move(prev);
    }

    // Move the requested sequence object into the editor.
    ensureSequenceExists(sceneIndex, sequenceIndex);
    SequenceEntry& nextSeq = mScenes[sceneIndex].sequences[sequenceIndex];
    if (!nextSeq.object) {
        nextSeq.object = createDefaultObject();
    }

    mEditor->setObject(nextSeq.object.release());

    mActiveSceneIndex = sceneIndex;
    mActiveSequenceIndex = sequenceIndex;

    // UX: after activating a sequence, show that scene's sequences.
    mSceneLevel = false;
    mBrowsingSceneIndex = sceneIndex;

    saveUiState();

    mIsActivatingSequenceSwitch = false;

    applyTagStateToUIForActiveShot();

    emit activeSequenceChanged();
    emit browsingLevelChanged();
}

void SceneSequenceManager::captureTagStateFromUIForActiveShot()
{
    ensureInitialized();
    ensureSequenceExists(mActiveSceneIndex, mActiveSequenceIndex);

    if (mActiveSceneIndex < 0 || mActiveSceneIndex >= static_cast<int>(mScenes.size())) {
        return;
    }
    SceneEntry& scene = mScenes[mActiveSceneIndex];
    if (mActiveSequenceIndex < 0 || mActiveSequenceIndex >= static_cast<int>(scene.sequences.size())) {
        return;
    }

    SequenceEntry& seq = scene.sequences[mActiveSequenceIndex];
    seq.tagState.defaultEnabled = TimeLineCells::isDefaultLayerFilterEnabled();

    const int n = TimeLineCells::layerTagDefCount();
    seq.tagState.tagEnabled.resize(n);
    for (int i = 0; i < n; ++i) {
        seq.tagState.tagEnabled[i] = TimeLineCells::isTagFilterEnabled(i);
    }

    seq.tagState.layerTagByLayerId = TimeLineCells::layerTagByLayerId();
}

void SceneSequenceManager::applyTagStateToUIForActiveShot()
{
    ensureInitialized();
    ensureSequenceExists(mActiveSceneIndex, mActiveSequenceIndex);

    if (mActiveSceneIndex < 0 || mActiveSceneIndex >= static_cast<int>(mScenes.size())) {
        return;
    }
    SceneEntry& scene = mScenes[mActiveSceneIndex];
    if (mActiveSequenceIndex < 0 || mActiveSequenceIndex >= static_cast<int>(scene.sequences.size())) {
        return;
    }

    const SequenceEntry& seq = scene.sequences[mActiveSequenceIndex];

    TimeLineCells::resetTagFilters();
    TimeLineCells::setDefaultLayerFilterEnabled(seq.tagState.defaultEnabled);

    const int n = TimeLineCells::layerTagDefCount();
    for (int i = 0; i < n; ++i) {
        bool enabled = true;
        if (i >= 0 && i < seq.tagState.tagEnabled.size()) {
            enabled = seq.tagState.tagEnabled[i];
        }
        TimeLineCells::setTagFilterEnabled(i, enabled);
    }

    TimeLineCells::setLayerTagByLayerId(seq.tagState.layerTagByLayerId);

    if (mEditor) {
        emit mEditor->updateTimeLine();
    }
}

Object* SceneSequenceManager::objectForSequence(int sceneIndex, int sequenceIndex) const
{
    if (!mEditor) {
        return nullptr;
    }

    if (sceneIndex == mActiveSceneIndex && sequenceIndex == mActiveSequenceIndex) {
        return mEditor->object();
    }

    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(mScenes.size())) {
        return nullptr;
    }

    const SceneEntry& scene = mScenes[sceneIndex];
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(scene.sequences.size())) {
        return nullptr;
    }

    return scene.sequences[sequenceIndex].object.get();
}

quint64 SceneSequenceManager::sequenceUid(int sceneIndex, int sequenceIndex) const
{
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(mScenes.size())) {
        return 0;
    }
    const SceneEntry& scene = mScenes[sceneIndex];
    if (sequenceIndex < 0 || sequenceIndex >= static_cast<int>(scene.sequences.size())) {
        return 0;
    }
    return scene.sequences[sequenceIndex].uid;
}

void SceneSequenceManager::ensureSceneExists(int sceneIndex)
{
    while (static_cast<int>(mScenes.size()) <= sceneIndex) {
        SceneEntry scene;
        scene.uid = mNextSceneUid++;
        scene.sceneIndex = static_cast<int>(mScenes.size());

        // Invariant: every scene has at least 1 sequence.
        SequenceEntry seq;
        seq.uid = mNextSequenceUid++;
        seq.sceneIndex = scene.sceneIndex;
        seq.sequenceIndex = 0;
        seq.tagState.tagEnabled.resize(TimeLineCells::layerTagDefCount());
        for (int i = 0; i < seq.tagState.tagEnabled.size(); ++i) {
            seq.tagState.tagEnabled[i] = true;
        }
        seq.tagState.defaultEnabled = true;
        seq.tagState.layerTagByLayerId.clear();
        if (seq.sceneIndex == mActiveSceneIndex && seq.sequenceIndex == mActiveSequenceIndex) {
            // Active sequence is owned by Editor.
            seq.object.reset();
        } else {
            seq.object = createDefaultObject();
        }
        scene.sequences.push_back(std::move(seq));

        mScenes.push_back(std::move(scene));
    }
}

void SceneSequenceManager::ensureSequenceExists(int sceneIndex, int sequenceIndex)
{
    ensureSceneExists(sceneIndex);

    SceneEntry& scene = mScenes[sceneIndex];
    while (static_cast<int>(scene.sequences.size()) <= sequenceIndex) {
        SequenceEntry seq;
        seq.uid = mNextSequenceUid++;
        seq.sceneIndex = sceneIndex;
        seq.sequenceIndex = static_cast<int>(scene.sequences.size());
        seq.tagState.tagEnabled.resize(TimeLineCells::layerTagDefCount());
        for (int i = 0; i < seq.tagState.tagEnabled.size(); ++i) {
            seq.tagState.tagEnabled[i] = true;
        }
        seq.tagState.defaultEnabled = true;
        seq.tagState.layerTagByLayerId.clear();
        if (seq.sceneIndex == mActiveSceneIndex && seq.sequenceIndex == mActiveSequenceIndex) {
            seq.object.reset();
        } else {
            seq.object = createDefaultObject();
        }
        scene.sequences.push_back(std::move(seq));
    }

    // The active sequence should be represented by the Object currently owned by the editor.
    if (sceneIndex == mActiveSceneIndex && sequenceIndex == mActiveSequenceIndex) {
        scene.sequences[sequenceIndex].object.reset();
    }
}

void SceneSequenceManager::reindexScenes()
{
    for (int i = 0; i < static_cast<int>(mScenes.size()); ++i)
    {
        mScenes[i].sceneIndex = i;
        for (int j = 0; j < static_cast<int>(mScenes[i].sequences.size()); ++j)
        {
            mScenes[i].sequences[j].sceneIndex = i;
            mScenes[i].sequences[j].sequenceIndex = j;
        }
    }
}

void SceneSequenceManager::reindexSequences(int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(mScenes.size())) {
        return;
    }

    SceneEntry& scene = mScenes[sceneIndex];
    for (int i = 0; i < static_cast<int>(scene.sequences.size()); ++i)
    {
        scene.sequences[i].sceneIndex = sceneIndex;
        scene.sequences[i].sequenceIndex = i;
    }
}

std::unique_ptr<Object> SceneSequenceManager::createDefaultObject()
{
    std::unique_ptr<Object> obj(new Object());
    obj->init();

    // Default layers (bitmap-first workflow, keeping camera layer as app expects)
    obj->addNewCameraLayer();
    obj->addNewBitmapLayer();
    obj->data()->setCurrentLayer(1);

    return obj;
}
