/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2025 Pencil2D Contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef SHIFTTRACEWIDGET_H
#define SHIFTTRACEWIDGET_H

#include "basedockwidget.h"

#include <QWidget>
#include <QFrame>
#include <QMap>
#include <QSet>

#include "shifttracemanager.h"

class QScrollArea;
class QHBoxLayout;
class QVBoxLayout;
class QPushButton;
class QLabel;
class QFrame;
class CosmicVSlider;
class CosmicCheckboxToggle;
class PopupColorWheelWidget;
class CosmicButton;

class ShiftTraceManager;

class ShiftTraceCircleToggle : public QWidget
{
    Q_OBJECT
public:
    explicit ShiftTraceCircleToggle(QWidget* parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    bool mChecked = false;
};

class ShiftTraceWidget : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit ShiftTraceWidget(QWidget* parent);
    ~ShiftTraceWidget() override;

    void initUI() override;
    void updateUI() override;

    void setEmbeddedInPopup(bool on);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onEntriesChanged();
    void onEntryChanged(int id);
    void onActiveMoveEntryChanged(int id);

    void onAddFromSelection();
    void onRemoveSelected();
    void onHideAllToggled(bool hideAll);

private:
    struct ItemWidgets {
        QWidget* container = nullptr;
        ShiftTraceCircleToggle* slotToggle = nullptr;
        CosmicVSlider* opacity = nullptr;
        CosmicButton* adjustGhostButton = nullptr;
        CosmicButton* resetGhostButton = nullptr;
        CosmicCheckboxToggle* holdGhostToggle = nullptr;
        int entryId = -1;
        int slotIndex = -1;
        bool isEmpty = false;
    };

    void rebuildItems();
    void createItemForEntry(const ShiftTraceManager::Entry& entry, int slotIndex);
    void createEmptySlot(int slotIndex);
    void refreshItem(ItemWidgets* item, const ShiftTraceManager::Entry& entry);
    void updateActiveMoveVisuals();
    void updateGhostControls();
    void updateButtonStates();
    void validateAndRemoveDeletedFrames();
    void openTintColorPopup(bool isPrev);
    void openHoldGhostTintPopup(int slotIndex, QWidget* anchor);
    void updateFooterTintStrip(int slotIndex, bool held);
    void updateTintUI();

    void applyEmbeddedPopupVisuals(bool on);

    QWidget* mRoot = nullptr;
    QVBoxLayout* mRootLayout = nullptr;
    bool mEmbeddedInPopup = false;
    
    QWidget* mHeaderBar = nullptr;
    CosmicCheckboxToggle* mPrevTintToggle = nullptr;
    QWidget* mLeftColorBox = nullptr;
    CosmicButton* mPrevResetAllButton = nullptr;
    CosmicCheckboxToggle* mPrevChainOpacityToggle = nullptr;
    CosmicCheckboxToggle* mHideAllToggle = nullptr; // Center Shift&Trace toggle
    CosmicCheckboxToggle* mNextChainOpacityToggle = nullptr;
    CosmicButton* mNextResetAllButton = nullptr;
    QWidget* mRightColorBox = nullptr;
    CosmicCheckboxToggle* mNextTintToggle = nullptr;

    QColor mPrevTintColor;
    QColor mNextTintColor;

    QScrollArea* mScroll = nullptr;
    QWidget* mScrollContents = nullptr;
    QHBoxLayout* mItemsLayout = nullptr;

    QWidget* mNumbersBar = nullptr;
    QHBoxLayout* mNumbersLayout = nullptr;

    QWidget* mTintFooterBar = nullptr;
    QHBoxLayout* mTintFooterLayout = nullptr;

    QWidget* mBottomBar = nullptr; // reserved for potential future footer UI (currently unused)
    QPushButton* mAddButton = nullptr; // unused; kept to avoid breaking interface
    QPushButton* mRemoveButton = nullptr; // unused; kept to avoid breaking interface

    QMap<int, ItemWidgets*> mItemsById; // entryId -> widgets
    QVector<ItemWidgets*> mEmptySlots; // placeholder slots
    QSet<int> mSelectedIds;
    bool mHideAll = false;
    bool mApplyingChainedOpacity = false;
};

#endif // SHIFTTRACEWIDGET_H 