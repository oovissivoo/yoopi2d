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

#ifndef BASEDOCKWIDGET_H
#define BASEDOCKWIDGET_H

#include <QDockWidget>

class Editor;
class TitleBarWidget;
class QMouseEvent;
class QWidget;

class BaseDockWidget : public QDockWidget
{
    Q_OBJECT
protected:
    explicit BaseDockWidget(QWidget* pParent);
    virtual  ~BaseDockWidget();

public:
    virtual void initUI() = 0;
    virtual void updateUI() = 0;

    void lock(bool locked);
    void setTitle(const QString& title);
    void setFloatingLock(bool locked);

    bool isLocked() const { return mLocked; }
    bool floatingLock() const { return mFloatingLock; }

    Editor* editor() const { return mEditor; }
    void setEditor( Editor* e ) { mEditor = e; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;

    // Wrap the given content widget in a styled dock container that matches
    // the global dock visuals (dark base background with lighter inner
    // master container and thin inner border gaps).
    void setDockContentWidget(QWidget* contentWidget);

public:
    QWidget* dockContentWidget() const;

private:
    Editor* mEditor = nullptr;

    QWidget* mNoTitleBarWidget = nullptr;
    TitleBarWidget* mTitleBarWidget = nullptr;
    QWidget* mDockContentWidget = nullptr; // Tracks the actual inner content widget

    bool mLocked = false;
    bool mFloatingLock = false;
    Qt::DockWidgetAreas mPrevAllowedAreas = Qt::AllDockWidgetAreas;
};

#endif // BASEDOCKWIDGET_H
