/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2009 Mj Mendoza IV
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "platformhandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QSettings>

#include "pencildef.h"

// X11 compositor detection via _NET_WM_CM_Sn selection owner
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#    include <QNativeInterface>
#  endif
#  include <xcb/xcb.h>
#  include <cstdio>
#  include <cstring>
static bool xcbCompositorRunning()
{
    // Try to get the XCB connection Qt is using
    xcb_connection_t* conn = nullptr;
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (auto* x11app = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
        conn = x11app->connection();
    }
#  else
    // Qt5: use QX11Info if available, otherwise fall back to xcb_connect
    conn = nullptr; // will open our own below
#  endif
    bool ownsConn = false;
    if (!conn) {
        conn = xcb_connect(nullptr, nullptr);
        ownsConn = true;
    }
    if (!conn || xcb_connection_has_error(conn)) {
        if (ownsConn && conn) xcb_disconnect(conn);
        return false;
    }

    // Check _NET_WM_CM_S<screen> selection — set by any EWMH compositor
    int screenNum = 0;
    char atomName[32];
    std::snprintf(atomName, sizeof(atomName), "_NET_WM_CM_S%d", screenNum);

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, static_cast<uint16_t>(strlen(atomName)), atomName);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, nullptr);
    if (!reply) {
        if (ownsConn) xcb_disconnect(conn);
        return false;
    }
    xcb_atom_t cmAtom = reply->atom;
    free(reply);

    xcb_get_selection_owner_cookie_t ownerCookie = xcb_get_selection_owner(conn, cmAtom);
    xcb_get_selection_owner_reply_t* ownerReply = xcb_get_selection_owner_reply(conn, ownerCookie, nullptr);
    bool hasCompositor = ownerReply && ownerReply->owner != XCB_NONE;
    free(ownerReply);

    if (ownsConn) xcb_disconnect(conn);
    return hasCompositor;
}
#endif // Q_OS_LINUX

namespace PlatformHandler
{
    void configurePlatformSpecificSettings() {}

    bool isDarkMode() { return false; }

    bool isCompositorActive()
    {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        // Only X11 needs the check; Wayland always composites
        const QString platform = QGuiApplication::platformName();
        if (platform.startsWith(QLatin1String("wayland"))) {
            return true;
        }
        static int cached = -1; // -1 = not yet checked
        if (cached == -1) {
            cached = xcbCompositorRunning() ? 1 : 0;
            if (cached == 0) {
                qWarning() << "PlatformHandler: no X11 compositor detected — "
                              "disabling WA_TranslucentBackground to prevent rendering glitches";
            }
        }
        return cached == 1;
#else
        return true;
#endif
    }

    void initialise()
    {
        /* If running as an AppImage, sets GStreamer environment variables to ensure
         * the plugins contained in the AppImage are found
         */
        QString appDir = QString::fromLocal8Bit(qgetenv("APPDIR"));
        if (!appDir.isEmpty())
        {
            bool success = qputenv("GST_PLUGIN_SYSTEM_PATH_1_0",
                                   QString("%1/usr/lib/gstreamer-1.0:%2")
                                       .arg(appDir, QString::fromLocal8Bit(qgetenv("GST_PLUGIN_SYSTEM_PATH_1_0")))
                                       .toLocal8Bit());
            success = qputenv("GST_PLUGIN_SCANNER_1_0",
                              QString("%1/usr/lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner")
                                 .arg(appDir).toLocal8Bit()) && success;
            if (!success)
            {
                qWarning() << "Unable to set up GStreamer environment";
            }
        }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        // Temporary solution for high DPI displays
        // EnableHighDpiScaling is a just in case mechanism in the event that we
        // want to disable this without recompiling, see #922
        QSettings settings(PENCIL2D, PENCIL2D);
        if (settings.value("EnableHighDpiScaling", "true").toBool())
        {
            // Enable auto screen scaling on high dpi display, for example, a 4k monitor
            // This attr has to be set before the QApplication is constructed
            // Only works on Windows & X11
            QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        }
#endif // QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    }
}
