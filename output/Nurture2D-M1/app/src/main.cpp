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

#include "log.h"
#include "pencil2d.h"
#include "pencilerror.h"
#include "platformhandler.h"
#include <QSplashScreen>
#include <QPixmap>
#include <QFile>
#include <QMessageBox>
#include "managers/settingsmanager.h"
#include "tool/penciltool.h"

#ifdef Q_OS_LINUX
#include <cstdlib>
#endif

/**
 * This is the entrypoint of the program. It performs basic initialization, then
 * boots the actual application (@ref Pencil2D).
 */
int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
    // Force XCB (X11) platform on Linux to ensure correct behaviour under Wayland sessions.
    // Wayland has known issues with Qt drag-and-drop window positioning that break docking.
    // XWayland provides full compatibility transparently, so this is safe on both X11 and Wayland.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif

    Q_INIT_RESOURCE(core_lib);

    // Validate critical resources are available
    if (!QFile::exists(":/Splash.png")) {
        qWarning() << "Warning: Splash screen resource not found";
    }
    if (!QFile::exists(":/icons/themes/playful/tools/tool-pen.svg")) {
        qCritical() << "Critical: tool-pen.svg resource missing! App may fail to start.";
    }

    PlatformHandler::initialise();
    Pencil2D app(argc, argv);
    initCategoryLogging();
    PencilTool::initTipMasks(); // bake CosmicDefaultTip Hard/Medium/Soft masks once

    // Show splash immediately before any heavy initialization
    QPixmap splashPixmap(":/Splash.png");
    if (splashPixmap.isNull()) {
        qWarning() << "Failed to load splash screen, continuing without it";
    }
    QSplashScreen splash(splashPixmap);
    splash.show();
    app.processEvents(); // ensure splash is painted before we do anything else

    // Now do the heavy init (style, theme, translators) while splash is visible
    app.initialize();

    try {
        switch (app.handleCommandLineOptions(&splash).code())
        {
            case Status::OK:
                QObject::connect(&app, &QCoreApplication::aboutToQuit, [](){
                    SettingsManager::instance()->forceSyncNow();
                });
                return Pencil2D::exec();
            case Status::SAFE:
                splash.close();
                return EXIT_SUCCESS;
            default:
                splash.close();
                return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        qCritical() << "Fatal exception during startup:" << e.what();
        splash.close();
        QMessageBox::critical(nullptr, "Startup Error", 
            QString("Nurture2D failed to start: %1").arg(e.what()));
        return EXIT_FAILURE;
    }
}
