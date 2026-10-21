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

#include "shortcutfilter.h"
#include <QAction>
#include <QDateTime>
#include <QDebug>
#include "scribblearea.h"
#include "editor.h"
#include "toolmanager.h"
#include "preferencemanager.h"
#include "preferencesdef.h"

ShortcutFilter::ShortcutFilter(ScribbleArea* scribbleArea , QObject* parent) :
    QObject(parent)
{
    mScribbleArea = scribbleArea;
    if (mScribbleArea) {
        mScribbleArea->installEventFilter(this);
    }
    mThresholdTimer.setSingleShot(true);
    connect(&mThresholdTimer, &QTimer::timeout, this, [this]{
        // Timer fired: if still tracking, mark exceeded
        if (mTracking) mExceededThreshold = true;
    });
}

bool ShortcutFilter::eventFilter(QObject* obj, QEvent* event)
{
    if (!mScribbleArea) return QObject::eventFilter(obj, event);

    // Intercept shortcut events for actions to start tracking
    if (event->type() == QEvent::Shortcut) {
        beginTrack();
        // If a property edit is in progress, end it now to avoid misrouting the final change
        if (mScribbleArea->editor() && mScribbleArea->editor()->tools()) {
            mScribbleArea->editor()->tools()->endEditingProperties();
        }
        return QObject::eventFilter(obj, event);
    }

    // On the canvas, react to key/mouse releases
    if (obj == mScribbleArea) {
        switch (event->type()) {
        case QEvent::KeyRelease:
            maybeRevertOnRelease();
            break;
        case QEvent::MouseButtonRelease:
            if (mShouldRevertAfterPointer) {
                maybeRevertOnRelease();
            }
            break;
        case QEvent::WindowDeactivate:
            // Cancel tracking when losing focus
            mTracking = false;
            mShouldRevertAfterPointer = false;
            mExceededThreshold = false;
            mThresholdTimer.stop();
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(obj, event);
}

bool ShortcutFilter::isToolAction(QObject* obj) const
{
    auto act = qobject_cast<QAction*>(obj);
    if (!act) return false;
    // The filter is only installed on tool actions in mainwindow, so accept any QAction here
    return true;
}

bool ShortcutFilter::isNavigationTool(ToolType toolType) const
{
    // Only HAND and CAMERA tools should use temporary switching behavior
    // All other tools (drawing, coloring, selection, etc.) should switch permanently
    return toolType == HAND || toolType == CAMERA;
}

void ShortcutFilter::beginTrack()
{
    if (!mScribbleArea || !mScribbleArea->editor()) return;
    ToolManager* tm = mScribbleArea->editor()->tools();
    if (!tm) return;
    
    // Only apply temporary switching behavior to navigation tools
    // Drawing and coloring tools should switch permanently
    ToolType currentToolType = tm->currentTool()->type();
    if (!isNavigationTool(currentToolType)) {
        qDebug() << "[ShortcutFilter] beginTrack skipped for non-navigation tool=" << BaseTool::TypeName(currentToolType);
        return;
    }
    
    mPrevToolType = tm->currentTool()->type();
    mPressMs = QDateTime::currentMSecsSinceEpoch();
    mTracking = true;
    mShouldRevertAfterPointer = false;
    mExceededThreshold = false;
    // Start threshold countdown now. If still pressed when it fires, we flip exceeded to true.
    PreferenceManager* prefs = mScribbleArea->editor()->preference();
    qreal threshold = prefs ? prefs->getFloat(SETTING::TEMP_TOOL_HOLD_MSEC) : 375.0;
    if (threshold < 0) threshold = 250.0;
    qDebug() << "[ShortcutFilter] beginTrack prevTool=" << BaseTool::TypeName(mPrevToolType)
             << "thresholdMs=" << threshold;
    mThresholdTimer.start(static_cast<int>(threshold));
}

void ShortcutFilter::maybeRevertOnRelease()
{
    if (!mTracking || !mScribbleArea || !mScribbleArea->editor()) return;
    // Timer-based: exceededThreshold tells if we surpassed the threshold while the key was down
    mThresholdTimer.stop();
    if (mExceededThreshold) {
        if (mScribbleArea->isMouseInUse()) {
            mShouldRevertAfterPointer = true;
            qDebug() << "[ShortcutFilter] hold-exceeded; deferring revert until pointer up";
        } else {
            ToolManager* tm = mScribbleArea->editor()->tools();
            if (tm) {
                qDebug() << "[ShortcutFilter] hold-exceeded; reverting to prevTool=" << BaseTool::TypeName(mPrevToolType);
                tm->setCurrentTool(mPrevToolType);
            }
        }
    } else {
        qDebug() << "[ShortcutFilter] tap; keep new tool";
    }
    mTracking = false;
    mExceededThreshold = false;
}
