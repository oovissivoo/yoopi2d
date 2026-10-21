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

#include <QLayout>
#include <QResizeEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QPalette>

#include "basedockwidget.h"
#include "platformhandler.h"
#include "titlebarwidget.h"

BaseDockWidget::BaseDockWidget(QWidget* pParent)
: QDockWidget(pParent, Qt::Tool)
{
    setFocusPolicy(Qt::StrongFocus);

    // Ensure the dock background matches the custom titlebar and base
    // container color. Use a slightly thicker transparent border so the
    // resize hit area around the dock edges is more forgiving without
    // introducing any visible frame.
    setStyleSheet("QDockWidget { background-color: #1f1f1f; border: 4px solid transparent; }");

    mTitleBarWidget = new TitleBarWidget(pParent);
    mNoTitleBarWidget = new QWidget(pParent);

    setTitleBarWidget(mTitleBarWidget);

    connect(mTitleBarWidget, &TitleBarWidget::closeButtonPressed, this, &BaseDockWidget::close);

    connect(mTitleBarWidget, &TitleBarWidget::undockButtonPressed, this, [this] {
       setFloating(!isFloating());
    });

    // Keep titlebar informed about floating state
    connect(this, &QDockWidget::topLevelChanged, mTitleBarWidget, &TitleBarWidget::setIsFloating);
    // Apply/restore docking restriction when floating state changes
    connect(this, &QDockWidget::topLevelChanged, this, [this](bool floating){
        if (mTitleBarWidget) {
            mTitleBarWidget->setIsFloating(floating);
        }
        if (mFloatingLock) {
            if (floating) {
                // Only capture previous allowed areas when they are not already
                // restricted, so we don't overwrite the stored value with
                // Qt::NoDockWidgetArea after restoring from settings.
                Qt::DockWidgetAreas current = allowedAreas();
                if (current != Qt::NoDockWidgetArea) {
                    mPrevAllowedAreas = current;
                }
                setAllowedAreas(Qt::NoDockWidgetArea);
            } else {
                setAllowedAreas(mPrevAllowedAreas);
            }
        }
    });
    connect(this, &QDockWidget::windowTitleChanged, mTitleBarWidget, &TitleBarWidget::setTitle);

    // Sync initial state
    if (mTitleBarWidget) {
        mTitleBarWidget->setIsFloating(isFloating());
        mTitleBarWidget->setFloatingLock(mFloatingLock);
    }

    // Wire floating lock toggle
    connect(mTitleBarWidget, &TitleBarWidget::floatingLockToggled, this, [this](bool on){
        setFloatingLock(on);
    });
}

BaseDockWidget::~BaseDockWidget()
{
}

void BaseDockWidget::lock(bool locked)
{
    // https://doc.qt.io/qt-5/qdockwidget.html#setTitleBarWidget
    // A empty QWidget results in the title bar being hidden.
    // nullptr means removing the custom title bar and restoring the default one

    if (locked) {
        // Ensure no reserved space when hiding the title bar
        if (mNoTitleBarWidget) {
            mNoTitleBarWidget->setFixedHeight(0);
            mNoTitleBarWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
        setTitleBarWidget(mNoTitleBarWidget);
    } else {
        if (mNoTitleBarWidget) {
            mNoTitleBarWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            mNoTitleBarWidget->setMinimumSize(QSize(0, 0));
        }
        setTitleBarWidget(mTitleBarWidget);
    }

    mLocked = locked;
}

void BaseDockWidget::setTitle(const QString& title)
{
    if (!mTitleBarWidget) { return; }
    mTitleBarWidget->setTitle(title);
}

void BaseDockWidget::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);

    if (mTitleBarWidget) {
        mTitleBarWidget->resizeEvent(event);
    }
}

void BaseDockWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    // Intentionally swallow double-clicks on dock widgets to prevent
    // Qt's default dock/undock toggle behavior via title bar double-click.
    // Dock state should be controlled explicitly via drag or menu actions.
}

bool BaseDockWidget::event(QEvent* event)
{
    if (event->type() == QEvent::Enter) {
        // When floating, make this the active window on hover so shortcuts route here
        if (isFloating()) {
            if (!isActiveWindow()) {
                activateWindow();
            }
            raise();
        }
        if (!hasFocus()) setFocus(Qt::OtherFocusReason);
    }
    return QDockWidget::event(event);
}

void BaseDockWidget::setDockContentWidget(QWidget* contentWidget)
{
    if (!contentWidget) {
        QDockWidget::setWidget(nullptr);
        mDockContentWidget = nullptr;
        return;
    }

    mDockContentWidget = contentWidget; // Track the inner content

    // Base dock container: uniform dark background that matches the
    // titlebar color and provides the visible inner border in the gaps.
    QWidget* baseContainer = new QWidget(this);
    baseContainer->setObjectName(QStringLiteral("DockBaseContainer"));
    baseContainer->setAutoFillBackground(true);

    QPalette basePal = baseContainer->palette();
    basePal.setColor(QPalette::Window, QColor(31, 31, 31));          // BaseDockColor = 1f1f1fff
    basePal.setColor(QPalette::WindowText, QColor(179, 179, 179));  // General text color = b3b3b3ff
    basePal.setColor(QPalette::ButtonText, QColor(179, 179, 179));
    baseContainer->setPalette(basePal);
    baseContainer->setStyleSheet("#DockBaseContainer { background-color: #1f1f1f; color: #b3b3b3; }");

    // Master container: holds the actual dock UI content. Slightly lighter
    // background so it reads as a panel inside the dock, with thin gaps
    // around it where the base color shows through.
    const int borderGap = 4; // small gap on all four sides around master container

    QVBoxLayout* outerLayout = new QVBoxLayout(baseContainer);
    // Apply the gap on all four sides so the dark base color forms
    // a continuous inner frame around the lighter master container.
    outerLayout->setContentsMargins(borderGap, borderGap, borderGap, borderGap);
    outerLayout->setSpacing(0);

    QWidget* masterContainer = new QWidget(baseContainer);
    masterContainer->setObjectName(QStringLiteral("DockMasterContainer"));
    masterContainer->setAutoFillBackground(true);

    QPalette masterPal = masterContainer->palette();
    masterPal.setColor(QPalette::Window, QColor(47, 47, 47));        // MasterContainerColor = 2f2f2fff
    masterPal.setColor(QPalette::WindowText, QColor(179, 179, 179));
    masterPal.setColor(QPalette::ButtonText, QColor(179, 179, 179));
    masterContainer->setPalette(masterPal);
    // Keep stylesheet in sync with the palette so the visible panel color
    // is the new master container color 2f2f2fff instead of 303030.
    masterContainer->setStyleSheet("#DockMasterContainer { background-color: #2f2f2f; color: #b3b3b3; }");

    QVBoxLayout* innerLayout = new QVBoxLayout(masterContainer);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(0);

    // Re-parent content into the master container so its lifetime is tied
    // to the dock, but without altering its own internal layout.
    contentWidget->setParent(masterContainer);
    innerLayout->addWidget(contentWidget);

    outerLayout->addWidget(masterContainer);

    QDockWidget::setWidget(baseContainer);
}

QWidget* BaseDockWidget::dockContentWidget() const
{
    return mDockContentWidget;
}

void BaseDockWidget::setFloatingLock(bool locked)
{
    mFloatingLock = locked;
    if (mTitleBarWidget) {
        mTitleBarWidget->setFloatingLock(locked);
    }
    if (isFloating()) {
        if (locked) {
            // When enabling the floating lock while floating, remember the last
            // non-restricted docking areas so that unlocking can properly
            // restore dockability even after a restored session.
            Qt::DockWidgetAreas current = allowedAreas();
            if (current != Qt::NoDockWidgetArea) {
                mPrevAllowedAreas = current;
            }
            setAllowedAreas(Qt::NoDockWidgetArea);
        } else {
            setAllowedAreas(mPrevAllowedAreas);
        }
    } else {
        // If not floating, ensure allowed areas are default/restored
        if (!locked) {
            setAllowedAreas(mPrevAllowedAreas);
        }
    }
}
