#include <QApplication>
#include <QAction>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "mainwindow2.h"
#include "editor.h"
#include "object.h"
#include "layercamera.h"
#include "movieexporter.h"
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    auto* window = new MainWindow2;
    auto* action = window->findChild<QAction*>("exportH264Animation");
    if (!action || !action->isEnabled()) return 1;
    auto cameras = window->mEditor->object()->getLayersByType<LayerCamera>();
    if (cameras.empty()) return 2;
    ExportMovieDesc desc;
    desc.strFileName = "/private/tmp/yoopi-h264-export-test.mp4";
    desc.strCameraName = cameras.front()->name();
    desc.startFrame = 1; desc.endFrame = 6; desc.fps = 12;
    desc.exportSize = QSize(128, 72);
    MovieExporter exporter;
    Status status = exporter.run(window->mEditor->object(), desc,
        [](float, float){}, [](float){}, [](QString){});
    if (!status.ok() || !QFile::exists(desc.strFileName)) {
        std::fprintf(stderr,"FAIL: real H264 movie export\n"); return 3;
    }
    desc.startFrame = desc.endFrame = 3;
    desc.strFileName = "/private/tmp/yoopi-h264-single-frame.mp4";
    bool finite = true;
    MovieExporter single;
    status = single.run(window->mEditor->object(), desc, [](float, float){},
                        [&](float value){ finite &= std::isfinite(value); }, [](QString){});
    if (!status.ok() || !finite) return 4;
    for (QSize size : {QSize(0,72), QSize(127,72), QSize(128,-1)}) {
        desc.exportSize = size;
        MovieExporter invalid;
        if (invalid.run(window->mEditor->object(), desc, [](float,float){}, [](float){}, [](QString){}).ok()) return 5;
    }
    desc.exportSize = QSize(128,72);
    desc.strCameraName = "missing camera";
    MovieExporter noCamera;
    if (noCamera.run(window->mEditor->object(), desc, [](float,float){}, [](float){}, [](QString){}).ok()) return 6;
    std::puts("PASS: MP4 six frames, single frame finite progress, invalid sizes and missing camera");
    std::fflush(stdout); std::_Exit(0);
}
