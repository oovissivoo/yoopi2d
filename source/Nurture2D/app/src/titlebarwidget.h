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
#ifndef TITLEBARWIDGET_H
#define TITLEBARWIDGET_H

#include <QWidget>

#include "appearance.h"

class QLabel;
class QToolButton;
class QMouseEvent;
class QHBoxLayout;

class TitleBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBarWidget(QWidget* parent = nullptr);
    ~TitleBarWidget();

    void resizeEvent(QResizeEvent* resizeEvent) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void setTitle(const QString& title);
    void paintEvent(QPaintEvent*) override;

    void setIsFloating(bool floating) { mIsFloating = floating; updateFloatingLockVisibility(); }
    void setFloatingLock(bool locked);
    void setDockControlsVisible(bool showDockButton, bool showFloatLockButton);
    void setCloseButtonStyle(int style, bool closeOnLeft);  // 0=default X, 1=mac circle

    // Apply to every live TitleBarWidget in the app
    static void applyStyleToAll(int style, bool closeOnLeft);

signals:
    void closeButtonPressed();
    void undockButtonPressed();
    void floatingLockToggled(bool locked);

private:
    QString flatButtonStylesheet() const;
    void showEvent(QShowEvent* event) override;
    void hideButtons(bool hide);
    void hideButtonsIfNeeded(int width);

    QWidget* createCustomTitleBarWidget(QWidget* parent);
    void updateFloatingLockIcon();
    void updateFloatingLockVisibility();

    QLabel* mTitleLabel = nullptr;
    QToolButton* mCloseButton = nullptr;
    QToolButton* mDockButton = nullptr;
    QToolButton* mFloatLockButton = nullptr;
    QHBoxLayout* mContainerLayout = nullptr;  // layout holding the buttons in order

    bool mIsFloating = false;
    bool mFloatingLock = false;

    bool mShowDockButton = true;
    bool mShowFloatLockButton = true;

    int mWidthOfFullLayout = 0;
};

#endif // TITLEBARWIDGET_H
