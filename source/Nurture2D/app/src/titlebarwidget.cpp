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
#include "titlebarwidget.h"

#include "pencildef.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QToolButton>
#include <QResizeEvent>
#include <QStyle>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QSettings>
#include <QApplication>

#include "platformhandler.h"
#include "buttonappearancewatcher.h"

TitleBarWidget::TitleBarWidget(QWidget* parent)
    : QWidget(parent)
{

    QVBoxLayout* vLayout = new QVBoxLayout();

    vLayout->setContentsMargins(3,4,3,4);
    vLayout->setSpacing(0);

    vLayout->addWidget(createCustomTitleBarWidget(this));

    setLayout(vLayout);
}

void TitleBarWidget::updateFloatingLockIcon()
{
    if (!mFloatLockButton) return;
    bool isDarkmode = PlatformHandler::isDarkMode();
    IconResource lockRes;
    IconResource unlockRes;
    lockRes.lightMode = QIcon(":/Interaction/LockFloatingState.svg");
    lockRes.darkMode = lockRes.lightMode;
    unlockRes.lightMode = QIcon(":/Interaction/UnlockFloatingState.svg");
    unlockRes.darkMode = unlockRes.lightMode;
    mFloatLockButton->setIcon(mFloatingLock ? lockRes.iconForMode(isDarkmode)
                                            : unlockRes.iconForMode(isDarkmode));
    mFloatLockButton->setToolTip(mFloatingLock ? tr("Floating lock: Prevent docking")
                                               : tr("Floating free: Allow docking"));
}

void TitleBarWidget::updateFloatingLockVisibility()
{
    if (!mFloatLockButton) return;

    bool lockVisible = mIsFloating && mShowFloatLockButton;
    mFloatLockButton->setVisible(lockVisible);

    if (mDockButton) {
        // Hide the dock/undock button when floating & locked, or when
        // explicitly disabled via setDockControlsVisible.
        bool dockVisible = mShowDockButton && !(mIsFloating && mFloatingLock);
        mDockButton->setVisible(dockVisible);
    }
}

void TitleBarWidget::setFloatingLock(bool locked)
{
    mFloatingLock = locked;
    if (mFloatLockButton) mFloatLockButton->setChecked(locked);
    updateFloatingLockIcon();
    updateFloatingLockVisibility();
}

TitleBarWidget::~TitleBarWidget()
{
}

QWidget* TitleBarWidget::createCustomTitleBarWidget(QWidget* parent)
{
    bool isDarkmode = PlatformHandler::isDarkMode();
    QWidget* containerWidget = new QWidget(parent);

    QHBoxLayout* containerLayout = new QHBoxLayout(parent);
    mContainerLayout = containerLayout;

    mCloseButton = new QToolButton(parent);

    mCloseButton->setStyleSheet(flatButtonStylesheet());

    QSize iconSize = QSize(14,14);
    QSize padding = QSize(2,2);

    // Read saved style preference
    QSettings s(PENCIL2D, PENCIL2D);
    const int closeStyle   = s.value(QStringLiteral("WindowCloseStyle"),  0).toInt();
    const bool closeOnLeft = s.value(QStringLiteral("WindowCloseOnLeft"), true).toBool();

    const QString closeIconPath = (closeStyle == 1)
        ? QStringLiteral(":/Windows&AppIcons/MacCloseButton.svg")
        : QStringLiteral(":/Interaction/CloseWindow.svg");

    IconResource closeButtonRes;
    closeButtonRes.lightMode = QIcon(closeIconPath);
    closeButtonRes.darkMode  = closeButtonRes.lightMode;

    QIcon closeIcon = closeButtonRes.iconForMode(isDarkmode);

    IconResource closeHoverButtonRes;
    closeHoverButtonRes.lightMode = QIcon(closeIconPath);
    closeHoverButtonRes.darkMode  = closeHoverButtonRes.lightMode;

    mCloseButton->setIcon(closeIcon);
    mCloseButton->setIconSize(iconSize);
    mCloseButton->setFixedSize(iconSize + padding);
    mCloseButton->installEventFilter(new ButtonAppearanceWatcher(closeButtonRes,
                                                                 closeHoverButtonRes,
                                                            this));

    connect(mCloseButton, &QToolButton::clicked, this, &TitleBarWidget::closeButtonPressed);

    IconResource dockButtonRes;
    dockButtonRes.lightMode = QIcon(":/Interaction/undockWindow.svg");
    dockButtonRes.darkMode = dockButtonRes.lightMode;

    IconResource dockHoverButtonRes;
    dockHoverButtonRes.lightMode = QIcon(":/Interaction/undockWindow.svg");
    dockHoverButtonRes.darkMode = dockHoverButtonRes.lightMode;

    mDockButton = new QToolButton(parent);

    QIcon dockIcon = dockButtonRes.iconForMode(isDarkmode);
    mDockButton->setIcon(dockIcon);
    mDockButton->setStyleSheet(flatButtonStylesheet());

    mDockButton->setIconSize(iconSize);
    mDockButton->setFixedSize(iconSize + padding);
    mDockButton->installEventFilter(new ButtonAppearanceWatcher(dockButtonRes, dockHoverButtonRes, this));

    connect(mDockButton, &QToolButton::clicked, this, &TitleBarWidget::undockButtonPressed);

    // Floating lock toggle (only visible when dock is floating)
    mFloatLockButton = new QToolButton(parent);
    mFloatLockButton->setCheckable(true);
    mFloatLockButton->setStyleSheet(flatButtonStylesheet());
    mFloatLockButton->setIconSize(iconSize);
    mFloatLockButton->setFixedSize(iconSize + padding);
    updateFloatingLockIcon();
    updateFloatingLockVisibility();
    connect(mFloatLockButton, &QToolButton::toggled, this, [this](bool on){
        mFloatingLock = on;
        updateFloatingLockIcon();
        emit floatingLockToggled(on);
    });

    mTitleLabel = new QLabel(parent);
    mTitleLabel->setAlignment(Qt::AlignVCenter);
    mTitleLabel->setStyleSheet("QLabel { background: transparent; color: #b3b3b3; }");

    // Position close button based on saved preference (not hardcoded to platform)
    if (closeOnLeft) {
        containerLayout->addWidget(mCloseButton);
        containerLayout->addWidget(mDockButton);
        containerLayout->addWidget(mFloatLockButton);
        containerLayout->addWidget(mTitleLabel);
    } else {
        containerLayout->addWidget(mTitleLabel);
        containerLayout->addWidget(mFloatLockButton);
        containerLayout->addWidget(mDockButton);
        containerLayout->addWidget(mCloseButton);
    }

    containerLayout->setSpacing(3);
    containerLayout->setContentsMargins(0,0,0,0);

    containerWidget->setLayout(containerLayout);
    containerWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    containerWidget->setMinimumSize(QSize(1,1));

    return containerWidget;
}

void TitleBarWidget::setDockControlsVisible(bool showDockButton, bool showFloatLockButton)
{
    mShowDockButton = showDockButton;
    mShowFloatLockButton = showFloatLockButton;
    updateFloatingLockVisibility();
}

void TitleBarWidget::setCloseButtonStyle(int style, bool closeOnLeft)
{
    if (!mCloseButton) return;

    const QString closeIconPath = (style == 1)
        ? QStringLiteral(":/Windows&AppIcons/MacCloseButton.svg")
        : QStringLiteral(":/Interaction/CloseWindow.svg");

    mCloseButton->setIcon(QIcon(closeIconPath));

    // Rebuild button order using the stored container layout
    if (!mContainerLayout) return;

    // Remove all items without deleting widgets
    while (mContainerLayout->count() > 0)
        mContainerLayout->takeAt(0);

    if (closeOnLeft) {
        mContainerLayout->addWidget(mCloseButton);
        mContainerLayout->addWidget(mDockButton);
        mContainerLayout->addWidget(mFloatLockButton);
        mContainerLayout->addWidget(mTitleLabel);
    } else {
        mContainerLayout->addWidget(mTitleLabel);
        mContainerLayout->addWidget(mFloatLockButton);
        mContainerLayout->addWidget(mDockButton);
        mContainerLayout->addWidget(mCloseButton);
    }

    update();
}

QString TitleBarWidget::flatButtonStylesheet() const
{
    return "QToolButton { border: 0; padding: 0; margin: 0; background: transparent; }";
}

void TitleBarWidget::setTitle(const QString &title)
{
    mTitleLabel->setText(title);
}

void TitleBarWidget::hideButtons(bool hide)
{
    mCloseButton->setHidden(hide);
    // Also hide dock button when floating lock is enabled
    if (mDockButton) {
        mDockButton->setHidden(hide || !mShowDockButton || (mIsFloating && mFloatingLock));
    }
    if (mFloatLockButton) {
        mFloatLockButton->setHidden(hide || !mShowFloatLockButton || !mIsFloating);
    }
}

void TitleBarWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    // Swallow all double-clicks on the dock title bar so Qt does not toggle
    // dock/undock state via double-click. Docking is now controlled only by
    // explicit drag or menu actions, not by double-click behavior.
}

void TitleBarWidget::resizeEvent(QResizeEvent *resizeEvent)
{
    QWidget::resizeEvent(resizeEvent);

    hideButtonsIfNeeded(resizeEvent->size().width());
}

void TitleBarWidget::hideButtonsIfNeeded(int width)
{
    if (width <= mWidthOfFullLayout) {
        hideButtons(true);
    } else {
        hideButtons(false);
    }
}

void TitleBarWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // This is to ensure that after the titlebar has been hidden with buttons hidden
    // the layout width is smaller, so we enable them again briefly to get the correct width.
    hideButtons(false);

    mWidthOfFullLayout = layout()->sizeHint().width();
    hideButtonsIfNeeded(size().width());
}

void TitleBarWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.save();
    // Base dock color for the title header area
    QColor bg = QColor(31, 31, 31); // 1f1f1fff
    painter.setBrush(bg);
    painter.setPen(Qt::NoPen);
    painter.drawRect(this->rect());
    painter.restore();
}

// static
void TitleBarWidget::applyStyleToAll(int style, bool closeOnLeft)
{
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* top : tops) {
        const QList<TitleBarWidget*> bars = top->findChildren<TitleBarWidget*>();
        for (TitleBarWidget* bar : bars) {
            bar->setCloseButtonStyle(style, closeOnLeft);
        }
    }
}
