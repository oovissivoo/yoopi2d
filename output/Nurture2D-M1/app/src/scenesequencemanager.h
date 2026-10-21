#ifndef SCENESEQUENCEMANAGER_H
#define SCENESEQUENCEMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <vector>
#include <memory>

 #include "object.h"
 #include "pencilerror.h"
class Editor;
class Object;

struct SequenceEntry
{
    struct TagState
    {
        QVector<bool> tagEnabled;
        bool defaultEnabled = true;
        QHash<int, int> layerTagByLayerId;
    };

    quint64 uid = 0;
    int sceneIndex = 0;
    int sequenceIndex = 0;
    std::unique_ptr<Object> object;

    TagState tagState;
};

struct SceneEntry
{
    quint64 uid = 0;
    int sceneIndex = 0;
    std::vector<SequenceEntry> sequences;
};

class SceneSequenceManager : public QObject
{
    Q_OBJECT

public:
    explicit SceneSequenceManager(Editor* editor, QObject* parent = nullptr);

    void ensureInitialized();

    void onExternalObjectLoaded();

    QString projectFilePath() const { return mProjectFilePath; }
    void setProjectFilePath(const QString& filePath);

    bool isTransientProject() const { return mProjectFilePath.isEmpty(); }

    bool hasWorkspace() const { return !mWorkspaceDir.isEmpty(); }
    QString workspaceDir() const { return mWorkspaceDir; }
    bool workspaceIsTransient() const { return mWorkspaceTransient; }
    QString workspaceAutoSavePath() const;

    void setWorkspaceDirectory(const QString& workspaceDir, bool transient);
    void setWorkspaceTransient(bool transient);

    int sceneCount() const { return static_cast<int>(mScenes.size()); }
    const std::vector<SceneEntry>& scenes() const { return mScenes; }

     Status loadProjectModel(std::vector<SceneEntry>&& scenes,
                            int activeSceneIndex,
                            int activeSequenceIndex,
                            bool sceneLevel,
                            int browsingSceneIndex);

    int activeSceneIndex() const { return mActiveSceneIndex; }
    int activeSequenceIndex() const { return mActiveSequenceIndex; }

    bool isAtSceneLevel() const { return mSceneLevel; }
    int browsingSceneIndex() const { return mBrowsingSceneIndex; }

    bool isActivatingSequenceSwitch() const { return mIsActivatingSequenceSwitch; }

    void browseScenes();
    void browseSequences(int sceneIndex);

    void activateSequence(int sceneIndex, int sequenceIndex);

    void restoreBrowsingState(bool sceneLevel, int browsingSceneIndex);

    void captureTagStateFromUIForActiveShot();
    void applyTagStateToUIForActiveShot();

    Object* objectForSequence(int sceneIndex, int sequenceIndex) const;
    quint64 sequenceUid(int sceneIndex, int sequenceIndex) const;

    int insertSceneAfter(int sceneIndex);
    bool removeScene(int sceneIndex);

    bool moveScenes(const QVector<int>& sceneIndices, int insertPos);

    int insertSequenceAfter(int sceneIndex, int sequenceIndex);
    bool removeSequence(int sceneIndex, int sequenceIndex);

    bool moveSequences(int sceneIndex, const QVector<int>& sequenceIndices, int insertPos);

signals:
    void browsingLevelChanged();
    void activeSequenceChanged();

private:
    QString settingsProjectKey() const;
    void loadUiState();
    void saveUiState() const;

    void ensureWorkspaceInitialized();
    void startTransientWorkspace();
    void ensureWorkspaceForProjectFile(const QString& projectFilePath);

    void ensureSceneExists(int sceneIndex);
    void ensureSequenceExists(int sceneIndex, int sequenceIndex);

    void reindexScenes();
    void reindexSequences(int sceneIndex);

    static std::unique_ptr<Object> createDefaultObject();

    Editor* mEditor = nullptr;

    QString mProjectFilePath;

    QString mWorkspaceDir;
    bool mWorkspaceTransient = false;

    std::vector<SceneEntry> mScenes;

    quint64 mNextSceneUid = 1;

    quint64 mNextSequenceUid = 1;

    bool mSceneLevel = true;
    int mBrowsingSceneIndex = 0;

    int mActiveSceneIndex = 0;
    int mActiveSequenceIndex = 0;

    bool mIsActivatingSequenceSwitch = false;
};

#endif
