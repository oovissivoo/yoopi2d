#ifndef N2DPROJECTIO_H
#define N2DPROJECTIO_H

#include <functional>
#include <memory>

#include <QString>

#include "pencilerror.h"

class Editor;
class Object;
class SceneSequenceManager;

class N2DProjectIO final
{
public:
    struct ProgressCallbacks
    {
        std::function<void(int)> progressChanged;
        std::function<void(int)> progressRangeChanged;
    };

    static Status saveProject(const QString& filePath,
                             Editor* editor,
                             SceneSequenceManager* sceneSequenceManager,
                             const ProgressCallbacks& callbacks);

    static Status loadProject(const QString& filePath,
                             Editor* editor,
                             SceneSequenceManager* sceneSequenceManager,
                             const ProgressCallbacks& callbacks);

    static Status saveSingleShotProject(const QString& filePath,
                                       const Object* object,
                                       const ProgressCallbacks& callbacks);

    static std::unique_ptr<Object> loadSingleShotProjectAsObject(const QString& filePath,
                                                                 const ProgressCallbacks& callbacks,
                                                                 Status* outStatus);

private:
    N2DProjectIO() = delete;
};

#endif
