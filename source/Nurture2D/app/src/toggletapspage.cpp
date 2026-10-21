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

#include "toggletapspage.h"
#include "ui_toggletapspage.h"
#include "preferencemanager.h"
#include "../../CUI/cosmiccheckboxtoggle.h"
#include "../../CUI/cosmicuimetrics.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QScrollBar>
#include <QGroupBox>

ToggleTapsPage::ToggleTapsPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ToggleTapsPage)
{
    ui->setupUi(this);

    if (ui->titleLabel) {
        ui->titleLabel->hide();
    }

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
            "QWidget#scrollAreaWidgetContents{background:transparent;}"
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
    
    // Apply cosmic scrollbar styling to match timeline scrollbars
    if (ui->scrollArea && ui->scrollArea->verticalScrollBar()) {
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
    }
    
    setupDockToggles();
}

ToggleTapsPage::~ToggleTapsPage()
{
    delete ui;
}

void ToggleTapsPage::setupDockToggles()
{
    // List of all dock widgets in the application
    // settingsKey must match the objectName set in mainwindow2.cpp
    // label is the user-facing name shown in the UI
    struct DockToggleDesc {
        QString settingsKey;
        QString label;
    };

    const QVector<DockToggleDesc> dockDescs = {
        { QStringLiteral("CellStack"),       QStringLiteral("Cell Stack") },
        { QStringLiteral("ColorWheel"),      QStringLiteral("Color Box") },
        { QStringLiteral("Color Inspector"), QStringLiteral("Color Inspector") },
        { QStringLiteral("colorSlidersDock"), QStringLiteral("Color Sliders") },
        { QStringLiteral("ColorPalette"),    QStringLiteral("ColorPalette") },
        { QStringLiteral("Onion Skin"),      QStringLiteral("Onion Skin") },
        { QStringLiteral("Reference Window"), QStringLiteral("Reference Window") },
        { QStringLiteral("Shift & Trace"),   QStringLiteral("Shift & Trace") },
        { QStringLiteral("TimeLine"),        QStringLiteral("TimeLine") },
        { QStringLiteral("ToolBox"),         QStringLiteral("ToolBox") },
        { QStringLiteral("ToolOption"),      QStringLiteral("ToolOption") }
    };

    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    if (!mainLayout) {
        mainLayout = new QVBoxLayout(ui->scrollAreaWidgetContents);
        ui->scrollAreaWidgetContents->setLayout(mainLayout);
    }
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    auto* card = new QGroupBox(ui->scrollAreaWidgetContents);
    card->setTitle(QString());
    card->setFlat(true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(10);

    auto* header = new QLabel(tr("Toggle Taps"), card);
    header->setObjectName(QStringLiteral("cosmicPrefSectionHeader"));
    cardLayout->addWidget(header);

    QLabel* descLabel = new QLabel(
        tr("Select which docks should remain visible when using the 'Toggle All Docks' feature.\n"
           "Enabled docks will stay on screen, while disabled docks will be hidden."),
        card
    );
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("QLabel { color: #aaaaaa; }");
    cardLayout->addWidget(descLabel);

    // Create toggle for each dock
    for (const DockToggleDesc& desc : dockDescs) {
        CosmicCheckboxToggle* toggle = new CosmicCheckboxToggle(card);
        toggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        toggle->setText(desc.label);
        toggle->setFixedHeight(CosmicUiMetrics::controlThickness());
        
        // Connect toggle signal
        const QString settingsKey = desc.settingsKey;
        connect(toggle, &CosmicCheckboxToggle::toggled, this, [this, settingsKey](bool checked) {
            dockToggleChanged(settingsKey, checked);
        });
        
        mDockToggles[settingsKey] = toggle;
        cardLayout->addWidget(toggle);
    }

    mainLayout->addWidget(card);
    mainLayout->addStretch();
}

void ToggleTapsPage::updateValues()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    
    // Default state: only TimeLine is enabled, all other docks are disabled
    const QMap<QString, bool> defaultStates = {
        { QStringLiteral("CellStack"),       false },
        { QStringLiteral("ColorWheel"),      false },
        { QStringLiteral("Color Inspector"), false },
        { QStringLiteral("colorSlidersDock"), false },
        { QStringLiteral("ColorPalette"),    false },
        { QStringLiteral("Onion Skin"),      false },
        { QStringLiteral("Reference Window"), false },
        { QStringLiteral("Shift & Trace"),   false },
        { QStringLiteral("TimeLine"),        true  },
        { QStringLiteral("ToolBox"),         false },
        { QStringLiteral("ToolOption"),      false }
    };

    // Load saved toggle states, falling back to defaults when no setting exists yet
    for (auto it = mDockToggles.begin(); it != mDockToggles.end(); ++it) {
        const QString& dockName = it.key();
        CosmicCheckboxToggle* toggle = it.value();

        const QString key = "ToggleTaps/" + dockName;
        QVariant stored = settings.value(key);
        bool enabled;

        if (!stored.isValid()) {
            // First run or new dock: use default state and persist it
            enabled = defaultStates.value(dockName, false);
            settings.setValue(key, enabled);
        } else {
            enabled = stored.toBool();
        }

        toggle->setChecked(enabled);
    }
}

void ToggleTapsPage::updateAccentColors()
{
    Q_UNUSED(mManager)
}

void ToggleTapsPage::dockToggleChanged(const QString& dockName, bool enabled)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("ToggleTaps/" + dockName, enabled);
    settings.sync();
}
