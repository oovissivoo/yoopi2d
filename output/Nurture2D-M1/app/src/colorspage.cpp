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

#include "colorspage.h"
#include "ui_colorspage.h"

#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QScrollBar>

#include "../CUI/cosmicuimetrics.h"

#include "preferencemanager.h"
#include "preferencesdef.h"
#include "pencildef.h"
#include "popupcolorwheelwidget.h"

ColorsPage::ColorsPage() : ui(new Ui::ColorsPage)
{
    ui->setupUi(this);

    if (ui->scrollArea) {
        ui->scrollArea->setFrameShape(QFrame::NoFrame);
        ui->scrollArea->setWidgetResizable(true);
        ui->scrollArea->viewport()->setAutoFillBackground(false);
        ui->scrollArea->setAutoFillBackground(false);
    }

    {
        const int cr = qMax(0, CosmicUiMetrics::cornerRadius());
        const QString cardStyle = QString(
            "QScrollArea{background:transparent;border:none;}"
            "QWidget#contents{background:transparent;}"
            "QLabel{background:transparent;}"
            "QGroupBox{"
            "  background:#2b2b2b;"
            "  border:none;"
            "  border-radius:%1px;"
            "  margin-top:10px;"
            "  padding:12px;"
            "}"
            "QGroupBox::title{height:0px; padding:0px; margin:0px; border:none; color:transparent;}"
            "QLabel#cosmicPrefSectionHeader{"
            " background:transparent;"
            " color:#b3b3b3;"
            " font-weight:600;"
            " padding:0px;"
            " margin:0px;"
            "}"
        ).arg(QString::number(cr));

        setStyleSheet(styleSheet() + cardStyle);
    }

    if (ui->colorGroupBox) {
        ui->colorGroupBox->setTitle(QString());
        ui->colorGroupBox->setFlat(true);
        if (QLayout* l = ui->colorGroupBox->layout()) {
            if (auto* grid = qobject_cast<QGridLayout*>(l)) {
                auto* header = new QLabel(tr("Colors"), ui->colorGroupBox);
                header->setObjectName(QStringLiteral("cosmicPrefSectionHeader"));
                grid->addWidget(header, 0, 0, 1, 2);

                struct Item { QLayoutItem* item; int row; int col; int rs; int cs; };
                QList<Item> items;
                for (int i = 0; i < grid->count(); ++i) {
                    int row = -1, col = -1, rs = 1, cs = 1;
                    grid->getItemPosition(i, &row, &col, &rs, &cs);
                    QLayoutItem* it = grid->itemAt(i);
                    if (!it) continue;
                    if (it->widget() == header) continue;
                    items.push_back({it, row, col, rs, cs});
                }
                for (int i = grid->count() - 1; i >= 0; --i) {
                    QLayoutItem* it = grid->itemAt(i);
                    if (!it) continue;
                    if (it->widget() == header) continue;
                    grid->takeAt(i);
                }
                for (const Item& rec : items) {
                    const int newRow = (rec.row >= 0) ? (rec.row + 1) : 1;
                    if (QWidget* w = rec.item->widget()) {
                        grid->addWidget(w, newRow, qMax(0, rec.col), rec.rs, rec.cs);
                    } else if (QLayout* lay = rec.item->layout()) {
                        grid->addLayout(lay, newRow, qMax(0, rec.col), rec.rs, rec.cs);
                    } else {
                        grid->addItem(rec.item, newRow, qMax(0, rec.col), rec.rs, rec.cs);
                    }
                }
            }
        }
    }
    
    // Apply cosmic scrollbar styling to match timeline scrollbars
    ui->scrollArea->verticalScrollBar()->setFixedWidth(12);
    ui->scrollArea->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 26px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Get references to the UI elements
    mAccentColorButton = ui->accentColorButton;
    mToolboxSelectionColorButton = ui->toolboxSelectionColorButton;
    mToolPaletteSelectionColorButton = ui->toolPaletteSelectionColorButton;
    mTitleBarColorButton = ui->titleBarColorButton;
    mDockContentColorButton = ui->dockContentColorButton;
    mScrollbarColorButton = ui->scrollbarColorButton;

    // Set cursor for color buttons
    mAccentColorButton->setCursor(Qt::PointingHandCursor);
    mToolboxSelectionColorButton->setCursor(Qt::PointingHandCursor);
    mToolPaletteSelectionColorButton->setCursor(Qt::PointingHandCursor);
    mTitleBarColorButton->setCursor(Qt::PointingHandCursor);
    mDockContentColorButton->setCursor(Qt::PointingHandCursor);
    mScrollbarColorButton->setCursor(Qt::PointingHandCursor);

    mAccentColorButton->setEnabled(false);

    // Connect signals
    connect(mAccentColorButton, &QPushButton::clicked, this, &ColorsPage::onAccentColorButtonClicked);
    connect(mToolboxSelectionColorButton, &QPushButton::clicked, this, &ColorsPage::onToolboxSelectionColorButtonClicked);
    connect(mToolPaletteSelectionColorButton, &QPushButton::clicked, this, &ColorsPage::onToolPaletteSelectionColorButtonClicked);
    connect(mTitleBarColorButton, &QPushButton::clicked, this, &ColorsPage::onTitleBarColorButtonClicked);
    connect(mDockContentColorButton, &QPushButton::clicked, this, &ColorsPage::onDockContentColorButtonClicked);
    connect(mScrollbarColorButton, &QPushButton::clicked, this, &ColorsPage::onScrollbarColorButtonClicked);
}

ColorsPage::~ColorsPage()
{
    delete ui;
}

void ColorsPage::updateValues()
{
    refreshColorButtons();
}

void ColorsPage::onAccentColorButtonClicked()
{
    const int rgbaPref = mManager->getInt(SETTING::ACCENT_COLOR);
    const QColor initial = QColor::fromRgba(rgbaPref);
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mManager);
    if (initial.isValid()) popup->setColor(initial);
    
    // Only update visual swatch during dragging (colorChanged)
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            setButtonColorSwatch(mAccentColorButton, c);
        }
    });
    
    // Save preference only on mouse release (colorSelected)
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mManager->set(SETTING::ACCENT_COLOR, static_cast<int>(c.rgba()));
        }
    });
    
    popup->showAt(QCursor::pos());
}

void ColorsPage::onToolboxSelectionColorButtonClicked()
{
    const int rgbaPref = mManager->getInt(SETTING::TOOLBOX_SELECTION_COLOR);
    const QColor initial = QColor::fromRgba(rgbaPref);
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mManager);
    if (initial.isValid()) popup->setColor(initial);
    
    // Only update visual swatch during dragging (colorChanged)
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            setButtonColorSwatch(mToolboxSelectionColorButton, c);
        }
    });
    
    // Save preference only on mouse release (colorSelected)
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mManager->set(SETTING::TOOLBOX_SELECTION_COLOR, static_cast<int>(c.rgba()));
        }
    });
    
    popup->showAt(QCursor::pos());
}

void ColorsPage::onToolPaletteSelectionColorButtonClicked()
{
    const int rgbaPref = mManager->getInt(SETTING::TOOL_PALETTE_SELECTION_COLOR);
    const QColor initial = QColor::fromRgba(rgbaPref);
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mManager);
    if (initial.isValid()) popup->setColor(initial);
    
    // Only update visual swatch during dragging (colorChanged)
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            setButtonColorSwatch(mToolPaletteSelectionColorButton, c);
        }
    });
    
    // Save preference only on mouse release (colorSelected)
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mManager->set(SETTING::TOOL_PALETTE_SELECTION_COLOR, static_cast<int>(c.rgba()));
        }
    });
    
    popup->showAt(QCursor::pos());
}

void ColorsPage::onTitleBarColorButtonClicked()
{
    const int rgbaPref = mManager->getInt(SETTING::UI_TITLE_BAR_COLOR);
    const QColor initial = QColor::fromRgba(rgbaPref);
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mManager);
    if (initial.isValid()) popup->setColor(initial);
    
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            setButtonColorSwatch(mTitleBarColorButton, c);
        }
    });
    
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mManager->set(SETTING::UI_TITLE_BAR_COLOR, static_cast<int>(c.rgba()));
        }
    });
    
    popup->showAt(QCursor::pos());
}

void ColorsPage::onDockContentColorButtonClicked()
{
    const int rgbaPref = mManager->getInt(SETTING::UI_DOCK_CONTENT_COLOR);
    const QColor initial = QColor::fromRgba(rgbaPref);
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mManager);
    if (initial.isValid()) popup->setColor(initial);
    
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            setButtonColorSwatch(mDockContentColorButton, c);
        }
    });
    
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mManager->set(SETTING::UI_DOCK_CONTENT_COLOR, static_cast<int>(c.rgba()));
        }
    });
    
    popup->showAt(QCursor::pos());
}

void ColorsPage::onScrollbarColorButtonClicked()
{
    const int rgbaPref = mManager->getInt(SETTING::UI_SCROLLBAR_COLOR);
    const QColor initial = QColor::fromRgba(rgbaPref);
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mManager);
    if (initial.isValid()) popup->setColor(initial);
    
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            setButtonColorSwatch(mScrollbarColorButton, c);
        }
    });
    
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mManager->set(SETTING::UI_SCROLLBAR_COLOR, static_cast<int>(c.rgba()));
        }
    });
    
    popup->showAt(QCursor::pos());
}

void ColorsPage::refreshColorButtons()
{
    if (!mManager) return;

    // Update accent color button
    const int accentRgba = mManager->getInt(SETTING::ACCENT_COLOR);
    const QColor accentColor = QColor::fromRgba(accentRgba);
    setButtonColorSwatch(mAccentColorButton, accentColor);

    // Update toolbox selection color button
    const int toolboxRgba = mManager->getInt(SETTING::TOOLBOX_SELECTION_COLOR);
    const QColor toolboxColor = QColor::fromRgba(toolboxRgba);
    setButtonColorSwatch(mToolboxSelectionColorButton, toolboxColor);

    // Update tool palette selection color button
    const int toolPaletteRgba = mManager->getInt(SETTING::TOOL_PALETTE_SELECTION_COLOR);
    const QColor toolPaletteColor = QColor::fromRgba(toolPaletteRgba);
    setButtonColorSwatch(mToolPaletteSelectionColorButton, toolPaletteColor);

    // Update UI theme color buttons
    const int titleBarRgba = mManager->getInt(SETTING::UI_TITLE_BAR_COLOR);
    const QColor titleBarColor = QColor::fromRgba(titleBarRgba);
    setButtonColorSwatch(mTitleBarColorButton, titleBarColor);

    const int dockContentRgba = mManager->getInt(SETTING::UI_DOCK_CONTENT_COLOR);
    const QColor dockContentColor = QColor::fromRgba(dockContentRgba);
    setButtonColorSwatch(mDockContentColorButton, dockContentColor);

    const int scrollbarRgba = mManager->getInt(SETTING::UI_SCROLLBAR_COLOR);
    const QColor scrollbarColor = QColor::fromRgba(scrollbarRgba);
    setButtonColorSwatch(mScrollbarColorButton, scrollbarColor);
}

void ColorsPage::setButtonColorSwatch(QPushButton* button, const QColor& color)
{
    if (!button) return;
    
    const QString style = QString("QPushButton { background-color: %1; border: 1px solid %2; }")
                         .arg(color.name(), color.darker(150).name());
    button->setStyleSheet(style);
}
