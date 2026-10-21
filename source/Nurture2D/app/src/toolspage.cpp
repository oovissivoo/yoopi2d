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

#include "toolspage.h"

#include <QtMath>

#include "preferencemanager.h"

#include "ui_toolspage.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "spinslider.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QLabel>
#include <QEvent>
#include <QCheckBox>

#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"

ToolsPage::ToolsPage() : ui(new Ui::ToolsPage)
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
            "QWidget#scrollAreaWidgetContents{background:transparent;}"
            "QLabel{background:transparent;}"
            "QCheckBox{background:transparent;}"
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

    if (ui->verticalLayout_4) {
        ui->verticalLayout_4->setContentsMargins(12, 12, 12, 12);
        ui->verticalLayout_4->setSpacing(10);
    }

    if (ui->brushBox) { ui->brushBox->setTitle(QString()); ui->brushBox->setFlat(true); }
    if (ui->moveBox) { ui->moveBox->setTitle(QString()); ui->moveBox->setFlat(true); }
    if (ui->handBox) { ui->handBox->setTitle(QString()); ui->handBox->setFlat(true); }

    auto insertHeader = [](QGroupBox* gb, const QString& title) -> QLabel* {
        if (!gb) return nullptr;
        QLayout* layout = gb->layout();
        if (!layout) return nullptr;
        auto* header = new QLabel(title, gb);
        header->setObjectName(QStringLiteral("cosmicPrefSectionHeader"));
        if (auto* vbox = qobject_cast<QVBoxLayout*>(layout)) {
            vbox->insertWidget(0, header);
        }
        return header;
    };
    mHeaderBrushTools = insertHeader(ui->brushBox, tr("Brush Tools"));
    mHeaderMoveTool   = insertHeader(ui->moveBox,  tr("Move Tool"));
    mHeaderHandTool   = insertHeader(ui->handBox,  tr("Hand Tool"));
    
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

    if (ui->useQuickSizingBox) {
        ui->useQuickSizingBox->hide();
        ui->useQuickSizingBox->setChecked(false);
    }

    if (ui->brushBox) {
        ui->brushBox->hide();
    }

    if (ui->invertZoomDirectionBox) {
        QCheckBox* old = ui->invertZoomDirectionBox;
        QLayout* layout = old->parentWidget() ? old->parentWidget()->layout() : nullptr;
        if (layout) {
            auto* repl = new CosmicCheckboxToggle(old->parentWidget());
            repl->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            repl->setText(old->text());
            repl->setChecked(old->isChecked());
            repl->setFixedHeight(CosmicUiMetrics::controlThickness());

            if (auto* vbox = qobject_cast<QVBoxLayout*>(layout)) {
                int idx = vbox->indexOf(old);
                vbox->removeWidget(old);
                old->hide();
                vbox->insertWidget(qMax(0, idx), repl);
            } else {
                old->hide();
                layout->addWidget(repl);
            }

            connect(old, &QCheckBox::toggled, repl, [repl](bool on) {
                QSignalBlocker b(repl);
                repl->setChecked(on);
            });
            connect(repl, &CosmicCheckboxToggle::toggled, old, [old](bool on) {
                if (old->isChecked() == on) return;
                old->setChecked(on);
            });

            mInvertZoomToggle = repl;
            mCheckboxToggleMap[old] = repl;
        }
    }

    if (ui->rotationIncrementSlider) {
        QSlider* old = ui->rotationIncrementSlider;
        auto* repl = new CosmicInputField(old->parentWidget());
        repl->setRange(1.0, 360.0);
        repl->setDecimals(0);
        repl->setSingleStep(1.0);
        repl->setValue(15.0);

        if (auto* vbox = qobject_cast<QVBoxLayout*>(old->parentWidget()->layout())) {
            int idx = vbox->indexOf(old);
            vbox->removeWidget(old);
            old->hide();
            vbox->insertWidget(qMax(0, idx), repl);
        } else {
            old->hide();
            if (QLayout* lay = old->parentWidget()->layout()) lay->addWidget(repl);
        }

        mRotationIncrementField = repl;
    }

    connect(ui->invertZoomDirectionBox, &QCheckBox::stateChanged, this, &ToolsPage::invertZoomDirectionChange);
    if (mRotationIncrementField) {
        connect(mRotationIncrementField, &CosmicInputField::valueChanged, this, [this](double v) {
            rotationIncrementChange(static_cast<int>(v));
        });
    }
}

ToolsPage::~ToolsPage()
{
    delete ui;
}

void ToolsPage::updateValues()
{
    ui->useQuickSizingBox->setChecked(false);
    ui->invertZoomDirectionBox->setChecked(mManager->isOn(SETTING::INVERT_DRAG_ZOOM_DIRECTION));
    setRotationIncrement(mManager->getInt(SETTING::ROTATION_INCREMENT));
    // No-op: removed temporary hold threshold control
}

void ToolsPage::quickSizingChange(int b)
{
    Q_UNUSED(b)
}

void ToolsPage::invertZoomDirectionChange(int b)
{
    mManager->set(SETTING::INVERT_DRAG_ZOOM_DIRECTION, b != Qt::Unchecked);
}

void ToolsPage::setRotationIncrement(int angle)
{
    if (mRotationIncrementField) {
        QSignalBlocker b(mRotationIncrementField);
        mRotationIncrementField->setValue(angle);
    }
}

void ToolsPage::rotationIncrementChange(int value)
{
    int angle = qMax(1, value);
    while (360 % angle != 0) angle++;
    ui->rotationIncrementDisplay->setText(tr("%1 degrees").arg(angle)); // don't use tr()'s plural settings, it breaks Transifex.
    mManager->set(SETTING::ROTATION_INCREMENT, angle);

    if (mRotationIncrementField) {
        QSignalBlocker b(mRotationIncrementField);
        mRotationIncrementField->setValue(angle);
    }
}

// Removed: tempToolHoldThresholdChange; preference no longer exposed in UI

void ToolsPage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslatePageStrings();
    }
}

void ToolsPage::retranslatePageStrings()
{
    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    ui->retranslateUi(this);

    // Guard .ui labels that could go empty
    auto guardLabel = [](QLabel* lbl, const char* src) {
        if (lbl && lbl->text().isEmpty())
            lbl->setText(QString::fromUtf8(src));
    };
    guardLabel(ui->rotationIncrementLabel, "Rotation snap increment");

    // Section headers
    if (mHeaderBrushTools) mHeaderBrushTools->setText(safetr("Brush Tools"));
    if (mHeaderMoveTool)   mHeaderMoveTool->setText(safetr("Move Tool"));
    if (mHeaderHandTool)   mHeaderHandTool->setText(safetr("Hand Tool"));

    // Sync toggle text and tooltip from hidden checkboxes
    for (auto it = mCheckboxToggleMap.constBegin(); it != mCheckboxToggleMap.constEnd(); ++it) {
        QCheckBox* cb = it.key();
        CosmicCheckboxToggle* toggle = it.value();
        if (cb && toggle) {
            const QString t = cb->text();
            if (!t.isEmpty()) toggle->setText(t);
            const QString tip = cb->toolTip();
            if (!tip.isEmpty()) toggle->setToolTip(tip);
        }
    }

    // Re-render the rotation increment display with the new language's "degrees" word
    if (mManager) {
        rotationIncrementChange(mManager->getInt(SETTING::ROTATION_INCREMENT));
    }
}
