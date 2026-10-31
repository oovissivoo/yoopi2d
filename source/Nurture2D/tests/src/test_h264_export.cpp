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
#include "cosmicexportmoviepopup.h"
#include "cosmicdropdown.h"
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
    CosmicExportMoviePopup dialog(window, FileType::MOVIE);
    const QString camera = cameras.front()->name();
    dialog.setCamerasInfo({{camera, QSize(128,72)}});
    if (dialog.selectedCameraName() != camera || dialog.exportSize() != QSize(128,72)) return 7;
    dialog.setCamerasInfo({{camera, QSize(128,72)}, {"Other camera", QSize(256,144)}});
    auto* drop = dialog.findChild<CosmicDropDown*>();
    if (!drop) return 8;
    drop->setCurrentIndex(1);
    if (dialog.selectedCameraName() != "Other camera" || dialog.exportSize() != QSize(256,144)) return 9;
    dialog.setCamerasInfo({{camera, QSize(128,72)}});
    CosmicExportMoviePopup gifDialog(window, FileType::GIF);
    gifDialog.setCamerasInfo({{camera, QSize(128,72)}});
    if (gifDialog.selectedCameraName() != camera) return 10;
    desc.strCameraName = dialog.selectedCameraName();
    desc.startFrame = 1; desc.endFrame = 6; desc.fps = 12;
    desc.exportSize = dialog.exportSize();
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
    const Status noCameraResult = noCamera.run(window->mEditor->object(), desc, [](float,float){}, [](float){}, [](QString){});
    if (noCameraResult.ok() || noCameraResult.description().isEmpty() || noCameraResult.details().str().isEmpty()) return 6;
    std::puts("PASS: dialog default/switch/reset camera, GIF camera, MP4 6/1 frames, invalid input and nonempty diagnostics");
    std::fflush(stdout); std::_Exit(0);
}
