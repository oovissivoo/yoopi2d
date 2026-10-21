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

#include "generalpage.h"

#include <memory>
#include <QScrollBar>
#include <QSettings>
#include <QTranslator>
#include <QColorDialog>
#include <QPointer>
#include <QTimer>
#include <QEvent>
#include <QCheckBox>

#include "pencildef.h"
#include "preferencemanager.h"
#include "popupcolorwheelwidget.h"
#include "colorwheel.h"
#include "cosmicwarningpopup.h"
#include "pencil2d.h"

#include "ui_generalpage.h"
// Match Tool Options slider appearance and behavior
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "../../CUI/blenderstylevaluefield.h"
#include "../../CUI/cosmicdropdown.h"
#include "../../CUI/cosmicuimetrics.h"
#include "../../CUI/cosmicinputfield.h"
#include "../../CUI/cosmiccheckboxtoggle.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

GeneralPage::GeneralPage() : ui(new Ui::GeneralPage)
{
    ui->setupUi(this);

    // Option B: remove group headers entirely (prevents Qt from reserving a title band).
    {
        const QList<QGroupBox*> groups = findChildren<QGroupBox*>();
        for (QGroupBox* gb : groups) {
            if (gb) {
                gb->setTitle(QString());
                gb->setFlat(true);
            }
        }
    }

    // Add lightweight in-card section headers (macOS-like) so cards remain understandable
    // even when QGroupBox titles are hidden.
    {
        auto insertHeader = [](QGroupBox* gb, const QString& title) -> QLabel* {
            if (!gb) return nullptr;
            QLayout* layout = gb->layout();
            if (!layout) return nullptr;

            auto* header = new QLabel(title, gb);
            header->setObjectName(QStringLiteral("cosmicPrefSectionHeader"));

            // If the group already has a vertical layout, just insert at the top.
            if (auto* vbox = qobject_cast<QVBoxLayout*>(layout)) {
                vbox->insertWidget(0, header);
                return header;
            }

            // For grid layouts, shift existing rows down by 1 and place header at row 0.
            if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
                const int cols = qMax(1, grid->columnCount());

                struct Item {
                    QLayoutItem* item;
                    int row;
                    int col;
                    int rowSpan;
                    int colSpan;
                };

                QList<Item> items;
                items.reserve(grid->count());

                // Snapshot positions before removal.
                for (int i = 0; i < grid->count(); ++i) {
                    int row = -1, col = -1, rowSpan = 1, colSpan = 1;
                    grid->getItemPosition(i, &row, &col, &rowSpan, &colSpan);
                    QLayoutItem* it = grid->itemAt(i);
                    if (!it) continue;
                    items.push_back({ it, row, col, rowSpan, colSpan });
                }

                // Remove everything from the grid safely.
                for (int i = grid->count() - 1; i >= 0; --i) {
                    grid->takeAt(i);
                }

                grid->addWidget(header, 0, 0, 1, cols);

                for (const Item& rec : items) {
                    const int newRow = (rec.row >= 0) ? (rec.row + 1) : 1;
                    if (QWidget* w = rec.item->widget()) {
                        grid->addWidget(w, newRow, qMax(0, rec.col), rec.rowSpan, rec.colSpan);
                    } else if (QLayout* l = rec.item->layout()) {
                        grid->addLayout(l, newRow, qMax(0, rec.col), rec.rowSpan, rec.colSpan);
                    } else {
                        grid->addItem(rec.item, newRow, qMax(0, rec.col), rec.rowSpan, rec.colSpan);
                    }
                }
                return header;
            }

            // Fallback: if layout type is unknown, just append a header above by adding it as a child.
            header->move(0, 0);
            header->show();
            return header;
        };

        mHeaderLanguage   = insertHeader(ui->groupBox,    tr("Language"));
        mHeaderAppearance = insertHeader(ui->appearanceBox, tr("Appearance"));
        mHeaderCursors    = insertHeader(ui->cursorsBox,   tr("Cursors"));
        mHeaderCanvas     = insertHeader(ui->displayBox,   tr("Canvas"));
        mHeaderGrid       = insertHeader(ui->gridBox,      tr("Grid"));
        mHeaderOverlays   = insertHeader(ui->groupBox_2,   tr("Overlays"));
        mHeaderAdvanced   = insertHeader(ui->advancedBox,  tr("Advanced"));
        mHeaderUndoRedo   = insertHeader(ui->groupBox_3,   tr("Undo / Redo"));

        // Style headers via stylesheet (avoid touching global QSS).
        const QString headerStyle = QStringLiteral(
            "QLabel#cosmicPrefSectionHeader{"
            " background:transparent;"
            " color:#b3b3b3;"
            " font-weight:600;"
            " padding:0px;"
            " margin:0px;"
            "}" 
            "QWidget#cosmicPrefSectionContent{background:transparent;}"
        );
        setStyleSheet(styleSheet() + headerStyle);
    }

    // Reorder cards in a more logical flow.
    if (ui->lay) {
        QList<QWidget*> order = {
            ui->groupBox,
            ui->appearanceBox,
            ui->cursorsBox,
            ui->displayBox,
            ui->gridBox,
            ui->groupBox_2,
            ui->advancedBox,
            ui->groupBox_3
        };

        for (QWidget* w : order) {
            if (!w) continue;
            ui->lay->removeWidget(w);
        }
        for (QWidget* w : order) {
            if (!w) continue;
            ui->lay->addWidget(w);
        }
        // Keep the spacer at the end if present.
    }

    if (ui->scrollArea) {
        ui->scrollArea->setFrameShape(QFrame::NoFrame);
        ui->scrollArea->setWidgetResizable(true);
        ui->scrollArea->viewport()->setAutoFillBackground(false);
        ui->scrollArea->setAutoFillBackground(false);
    }

    if (ui->windowOpacityBox) {
        ui->windowOpacityBox->hide();
    }
    if (ui->backgroundBox) {
        ui->backgroundBox->hide();
    }
    if (ui->editingBox) {
        ui->editingBox->hide();
    }

    // Cosmic/macOS-like cards: remove heavy outlines and use rounded sections.
    {
        const int cr = qMax(0, CosmicUiMetrics::cornerRadius());
        const QString cardStyle = QString(
            "QScrollArea{background:transparent;border:none;}"
            "QWidget#contents{background:transparent;}"
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
        ).arg(QString::number(cr));

        setStyleSheet(styleSheet() + cardStyle);

        if (ui->lay) {
            ui->lay->setContentsMargins(12, 12, 12, 12);
            ui->lay->setSpacing(10);
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

    // Replace stock sliders with CosmicInputField for consistent look/feel
    {
        // Window opacity slider
        QSlider* old = ui->windowOpacityLevel;
        auto* repl = new CosmicInputField(old->parentWidget());
        repl->setRange(old->minimum(), old->maximum());
        repl->setDecimals(0);
        repl->setSingleStep(1.0);
        repl->setValue(old->value());
        // Insert at same grid position (row 0, col 1)
        if (auto grid = qobject_cast<QGridLayout*>(old->parentWidget()->layout())) {
            grid->removeWidget(old);
            old->hide();
            grid->addWidget(repl, 0, 1, 1, 1);
        }
        mWindowOpacityField = repl;
    }
    {
        // Curve smoothing slider
        QSlider* old = ui->curveSmoothingLevel;
        auto* repl = new CosmicInputField(old->parentWidget());
        repl->setRange(old->minimum(), old->maximum());
        repl->setDecimals(0);
        repl->setSingleStep(1.0);
        repl->setValue(old->value());
        if (auto grid = qobject_cast<QGridLayout*>(old->parentWidget()->layout())) {
            grid->removeWidget(old);
            old->hide();
            // In editingBox grid, the slider is at row 1, col 0
            grid->addWidget(repl, 1, 0, 1, 1);
        }
        mCurveSmoothingField = repl;
    }

    // Replace QSpinBox controls with CosmicInputField
    {
        // Grid Width
        QSpinBox* oldGridW = ui->gridSizeInputW;
        auto* gridWidth = new CosmicInputField(oldGridW->parentWidget());
        gridWidth->setRange(oldGridW->minimum(), oldGridW->maximum());
        gridWidth->setDecimals(0);
        gridWidth->setSingleStep(1.0);
        gridWidth->setValue(oldGridW->value());
        
        if (auto grid = qobject_cast<QGridLayout*>(oldGridW->parentWidget()->layout())) {
            // Get the position of the old widget
            int row = -1, col = -1, rowSpan = 1, colSpan = 1;
            int idx = grid->indexOf(oldGridW);
            if (idx >= 0) {
                grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
            }
            grid->removeWidget(oldGridW);
            oldGridW->hide();
            if (row >= 0 && col >= 0) {
                // Preserve the original label in column 0; field lives in the original value column.
                grid->addWidget(gridWidth, row, col, rowSpan, colSpan);
                grid->setColumnStretch(0, 1);
                grid->setColumnStretch(1, 0);
            }
        }
        mGridWidthField = gridWidth;
    }
    {
        // Grid Height
        QSpinBox* oldGridH = ui->gridSizeInputH;
        auto* gridHeight = new CosmicInputField(oldGridH->parentWidget());
        gridHeight->setRange(oldGridH->minimum(), oldGridH->maximum());
        gridHeight->setDecimals(0);
        gridHeight->setSingleStep(1.0);
        gridHeight->setValue(oldGridH->value());
        
        if (auto grid = qobject_cast<QGridLayout*>(oldGridH->parentWidget()->layout())) {
            int row = -1, col = -1, rowSpan = 1, colSpan = 1;
            int idx = grid->indexOf(oldGridH);
            if (idx >= 0) {
                grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
            }
            grid->removeWidget(oldGridH);
            oldGridH->hide();
            if (row >= 0 && col >= 0) {
                grid->addWidget(gridHeight, row, col, rowSpan, colSpan);
                grid->setColumnStretch(0, 1);
                grid->setColumnStretch(1, 0);
            }
        }
        mGridHeightField = gridHeight;
    }
    {
        // Action Safe Input
        QSpinBox* oldActionSafe = ui->actionSafeInput;
        auto* actionSafe = new CosmicInputField(oldActionSafe->parentWidget());
        actionSafe->setRange(oldActionSafe->minimum(), oldActionSafe->maximum());
        actionSafe->setDecimals(0);
        actionSafe->setSingleStep(1.0);
        actionSafe->setValue(oldActionSafe->value());
        
        if (auto grid = qobject_cast<QGridLayout*>(oldActionSafe->parentWidget()->layout())) {
            int row = -1, col = -1, rowSpan = 1, colSpan = 1;
            int idx = grid->indexOf(oldActionSafe);
            if (idx >= 0) {
                grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
            }
            grid->removeWidget(oldActionSafe);
            oldActionSafe->hide();
            if (row >= 0 && col >= 0) {
                // Remove the spacer at column 2 if it exists
                QLayoutItem* spacer = grid->itemAtPosition(row, 2);
                if (spacer) {
                    grid->removeItem(spacer);
                    delete spacer;
                }
                // Place the input field at the original spinbox column (col 3).
                // col 0 = checkbox, col 1 = label — do not overwrite them.
                grid->addWidget(actionSafe, row, col, rowSpan, colSpan);
                grid->setColumnStretch(1, 1); // label column stretches
                grid->setColumnStretch(col, 0); // input field stays compact
            }
        }
        mActionSafeField = actionSafe;
    }
    {
        // Title Safe Input
        QSpinBox* oldTitleSafe = ui->titleSafeInput;
        auto* titleSafe = new CosmicInputField(oldTitleSafe->parentWidget());
        titleSafe->setRange(oldTitleSafe->minimum(), oldTitleSafe->maximum());
        titleSafe->setDecimals(0);
        titleSafe->setSingleStep(1.0);
        titleSafe->setValue(oldTitleSafe->value());
        
        if (auto grid = qobject_cast<QGridLayout*>(oldTitleSafe->parentWidget()->layout())) {
            int row = -1, col = -1, rowSpan = 1, colSpan = 1;
            int idx = grid->indexOf(oldTitleSafe);
            if (idx >= 0) {
                grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
            }
            grid->removeWidget(oldTitleSafe);
            oldTitleSafe->hide();
            if (row >= 0 && col >= 0) {
                // Remove the spacer at column 2 if it exists
                QLayoutItem* spacer = grid->itemAtPosition(row, 2);
                if (spacer) {
                    grid->removeItem(spacer);
                    delete spacer;
                }
                // Place the input field at the original spinbox column (col 3).
                // col 0 = checkbox, col 1 = label — do not overwrite them.
                grid->addWidget(titleSafe, row, col, rowSpan, colSpan);
                grid->setColumnStretch(1, 1); // label column stretches
                grid->setColumnStretch(col, 0); // input field stays compact
            }
        }
        mTitleSafeField = titleSafe;
    }
    {
        // Frame Pool Size Spin (in HBox layout)
        QSpinBox* oldFramePool = ui->framePoolSizeSpin;
        auto* framePool = new CosmicInputField(oldFramePool->parentWidget());
        framePool->setRange(oldFramePool->minimum(), oldFramePool->maximum());
        framePool->setDecimals(0);
        framePool->setSingleStep(1.0);
        framePool->setValue(oldFramePool->value());
        
        // Find the actual HBoxLayout that contains the spinbox
        // The spinbox is in: GroupBox -> VBoxLayout -> HBoxLayout
        QLayout* parentLayout = nullptr;
        QWidget* parentWidget = oldFramePool->parentWidget();
        
        // Search through the parent widget's layout to find which layout contains this spinbox
        if (parentWidget && parentWidget->layout()) {
            QLayout* mainLayout = parentWidget->layout();
            for (int i = 0; i < mainLayout->count(); ++i) {
                QLayoutItem* item = mainLayout->itemAt(i);
                if (item && item->layout()) {
                    QLayout* subLayout = item->layout();
                    // Check if this sublayout contains our spinbox
                    for (int j = 0; j < subLayout->count(); ++j) {
                        QLayoutItem* subItem = subLayout->itemAt(j);
                        if (subItem && subItem->widget() == oldFramePool) {
                            parentLayout = subLayout;
                            break;
                        }
                    }
                }
                if (parentLayout) break;
            }
        }
        
        // Now replace in the correct layout
        if (auto hbox = qobject_cast<QHBoxLayout*>(parentLayout)) {
            // Remove the old spinbox
            hbox->removeWidget(oldFramePool);
            oldFramePool->hide();

            // Keep the label visible; insert the new field where the spinbox was.
            hbox->addWidget(framePool, 0);
            hbox->setStretchFactor(framePool, 0);
        }
        mFramePoolSizeField = framePool;
    }
    {
        // Undo Steps Box (in HBox layout)
        QSpinBox* oldUndoSteps = ui->undoStepsBox;
        auto* undoSteps = new CosmicInputField(oldUndoSteps->parentWidget());
        undoSteps->setRange(oldUndoSteps->minimum(), oldUndoSteps->maximum());
        undoSteps->setDecimals(0);
        undoSteps->setSingleStep(1.0);
        undoSteps->setValue(oldUndoSteps->value());
        
        // Find the actual HBoxLayout that contains the spinbox
        // The spinbox is in: GroupBox -> VBoxLayout -> HBoxLayout
        QLayout* parentLayout = nullptr;
        QWidget* parentWidget = oldUndoSteps->parentWidget();
        
        // Search through the parent widget's layout to find which layout contains this spinbox
        if (parentWidget && parentWidget->layout()) {
            QLayout* mainLayout = parentWidget->layout();
            for (int i = 0; i < mainLayout->count(); ++i) {
                QLayoutItem* item = mainLayout->itemAt(i);
                if (item && item->layout()) {
                    QLayout* subLayout = item->layout();
                    // Check if this sublayout contains our spinbox
                    for (int j = 0; j < subLayout->count(); ++j) {
                        QLayoutItem* subItem = subLayout->itemAt(j);
                        if (subItem && subItem->widget() == oldUndoSteps) {
                            parentLayout = subLayout;
                            break;
                        }
                    }
                }
                if (parentLayout) break;
            }
        }
        
        // Now replace in the correct layout
        if (auto hbox = qobject_cast<QHBoxLayout*>(parentLayout)) {
            // Remove the old spinbox
            hbox->removeWidget(oldUndoSteps);
            oldUndoSteps->hide();

            // Keep the label visible; insert the new field where the spinbox was.
            hbox->addWidget(undoSteps, 0);
            hbox->setStretchFactor(undoSteps, 0);
        }
        mUndoStepsField = undoSteps;
    }

    // Replace default checkboxes with CosmicCheckboxToggle for consistency.
    // Keep original checkbox hidden so existing signal/slot wiring stays intact.
    {
        auto replaceCheckBox = [this](QCheckBox* old) {
            if (!old) return;
            QLayout* layout = old->parentWidget() ? old->parentWidget()->layout() : nullptr;
            if (!layout) return;

            auto* repl = new CosmicCheckboxToggle(old->parentWidget());
            repl->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            repl->setText(old->text());
            repl->setToolTip(old->toolTip());
            repl->setChecked(old->isChecked());
            repl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            repl->setFixedHeight(CosmicUiMetrics::controlThickness());

            if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
                int row = -1, col = -1, rowSpan = 1, colSpan = 1;
                int idx = grid->indexOf(old);
                if (idx >= 0) {
                    grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
                }
                grid->removeWidget(old);
                old->hide();
                if (row >= 0 && col >= 0) {
                    grid->addWidget(repl, row, col, rowSpan, colSpan);
                } else {
                    grid->addWidget(repl);
                }
            } else if (auto* box = qobject_cast<QBoxLayout*>(layout)) {
                int idx = box->indexOf(old);
                box->removeWidget(old);
                old->hide();
                if (idx >= 0) {
                    box->insertWidget(idx, repl);
                } else {
                    box->addWidget(repl);
                }
            } else {
                // Fallback: append
                old->hide();
                layout->addWidget(repl);
            }

            // Two-way sync: UI refreshes update the hidden checkbox; the toggle follows.
            connect(old, &QCheckBox::toggled, repl, [repl](bool on) {
                QSignalBlocker b(repl);
                repl->setChecked(on);
            });
            connect(repl, &CosmicCheckboxToggle::toggled, old, [old](bool on) {
                if (old->isChecked() == on) return;
                old->setChecked(on);
            });

            // Store mapping so we can retranslate the toggle label on language change.
            mCheckboxToggleMap[old] = repl;
        };

        replaceCheckBox(ui->toolCursorsBox);
        replaceCheckBox(ui->gridCheckBox);
        replaceCheckBox(ui->actionSafeCheckBox);
        replaceCheckBox(ui->titleSafeCheckBox);
        replaceCheckBox(ui->safeHelperTextCheckbox);
        replaceCheckBox(ui->invertScrollDirectionBox);
        replaceCheckBox(ui->zoomHorizontalDragBox);
        replaceCheckBox(ui->lowResNavigationBox);
        replaceCheckBox(ui->highResBox);
        replaceCheckBox(ui->antialiasingBox);
        replaceCheckBox(ui->canvasCursorBox);
    }

    // Keep labels visible for clarity (macOS-like label-left/control-right rows).
    
    // Hide Apply/Cancel buttons (settings now save immediately)
    if (ui->undoRedoGroupApplyButton) ui->undoRedoGroupApplyButton->hide();
    if (ui->undoRedoGroupCancelButton) ui->undoRedoGroupCancelButton->hide();

    QSettings settings(PENCIL2D, PENCIL2D);

    QString languages [][3]
        {
            // translatable string, endonym, locale code
            { tr("Arabic"), QStringLiteral("العربية"), "ar" },
            { tr("Bulgarian"), QStringLiteral("Български"), "bg" },
            { tr("Catalan"), QStringLiteral("Català"), "ca" },
            { tr("Czech"), QStringLiteral("Čeština"), "cs" },
            { tr("Danish"), QStringLiteral("Dansk"), "da" },
            { tr("German"), QStringLiteral("Deutsch"), "de" },
            { tr("Greek"), QStringLiteral("Ελληνικά"), "el" },
            { tr("English"), QStringLiteral("English"), "en" },
            { tr("Spanish"), QStringLiteral("Español"), "es" },
            { tr("Estonian"), QStringLiteral("Eesti"), "et" },
            { tr("Persian"), QStringLiteral("فارسی"), "fa" },
            { tr("French"), QStringLiteral("Français"), "fr" },
            { tr("Hebrew"), QStringLiteral("עברית"), "he" },
            { tr("Hungarian"), QStringLiteral("Magyar"), "hu_HU" },
            { tr("Indonesian"), QStringLiteral("Bahasa Indonesia"), "id" },
            { tr("Italian"), QStringLiteral("Italiano"), "it" },
            { tr("Japanese"), QStringLiteral("日本語"), "ja" },
            { tr("Kabyle"), QStringLiteral("Taqbaylit"), "kab" },
            { tr("Korean"), QStringLiteral("한국어"), "ko" },
            { tr("Norwegian Bokmål"), QStringLiteral("Norsk bokmål"), "nb" },
            { tr("Dutch \u2013 Netherlands"), QStringLiteral("Nederlands \u2013 Nederland"), "nl_NL" },
            { tr("Polish"), QStringLiteral("Polski"), "pl" },
            { tr("Portuguese \u2013 Portugal"), QStringLiteral("Português \u2013 Portugal"), "pt_PT" },
            { tr("Portuguese \u2013 Brazil"), QStringLiteral("Português \u2013 Brasil"), "pt_BR" },
            { tr("Russian"), QStringLiteral("Русский"), "ru" },
            { tr("Slovene"), QStringLiteral("Slovenščina"), "sl" },
            { tr("Swedish"), QStringLiteral("Svenska"), "sv" },
            { tr("Turkish"), QStringLiteral("Türkçe"), "tr" },
            { tr("Vietnamese"), QStringLiteral("Tiếng Việt"), "vi" },
            { tr("Cantonese"), QStringLiteral("粵语"), "yue" },
            { tr("Chinese \u2013 China"), QStringLiteral("简体中文"), "zh_CN" },
            { tr("Chinese \u2013 Taiwan"), QStringLiteral("繁體中文"), "zh_TW" },
        };

    for (auto& lang : languages)
    {
        const QString itemText = QStringLiteral("%1 (%2)").arg(lang[0]).arg(lang[1]);
        const QString localeCode = lang[2];
        ui->languageCombo->addItem(itemText, localeCode);
    }

    // Replace language QComboBox with CosmicDropDown
    {
        QComboBox* old = ui->languageCombo;
        if (old) {
            // Build options and parallel locale code list from existing combo items
            QStringList displayOptions;
            mLanguageLocaleCodes.clear();
            for (int i = 0; i < old->count(); ++i) {
                displayOptions << old->itemText(i);
                mLanguageLocaleCodes << old->itemData(i).toString();
            }

            auto* repl = new CosmicDropDown(old->parentWidget());
            repl->setOptions(displayOptions);

            // Insert at the same position in parent layout
            if (QLayout* parentLayout = old->parentWidget() ? old->parentWidget()->layout() : nullptr) {
                if (auto* grid = qobject_cast<QGridLayout*>(parentLayout)) {
                    int row = -1, col = -1, rowSpan = 1, colSpan = 1;
                    int idx = grid->indexOf(old);
                    if (idx >= 0) {
                        grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
                    }
                    grid->removeWidget(old);
                    old->hide();
                    if (row >= 0 && col >= 0) {
                        grid->addWidget(repl, row, col, rowSpan, colSpan);
                    } else {
                        grid->addWidget(repl, grid->rowCount(), 0, 1, grid->columnCount() > 0 ? grid->columnCount() : 1);
                    }
                } else if (auto* hbox = qobject_cast<QHBoxLayout*>(parentLayout)) {
                    int idx = hbox->indexOf(old);
                    hbox->removeWidget(old);
                    old->hide();
                    if (idx >= 0) {
                        hbox->insertWidget(idx, repl);
                    } else {
                        hbox->addWidget(repl);
                    }
                } else if (auto* vbox = qobject_cast<QVBoxLayout*>(parentLayout)) {
                    int idx = vbox->indexOf(old);
                    vbox->removeWidget(old);
                    old->hide();
                    if (idx >= 0) {
                        vbox->insertWidget(idx, repl);
                    } else {
                        vbox->addWidget(repl);
                    }
                }
            }

            mLanguageDropDown = repl;
            old->setVisible(false);
        }
    }

    // Replace cursor style QComboBox with CosmicDropDown
    {
        QComboBox* old = ui->cursorStyleCombo;
        if (old) {
            auto* repl = new CosmicDropDown(old->parentWidget());
            repl->setOptions({
                tr("Circle"),
                tr("Cross"),
                tr("Triangle Up"),
                tr("Triangle Down"),
                tr("Triangle Left"),
                tr("Triangle Right"),
                tr("Triangle Left-Up"),
                tr("Triangle Left-Down"),
                tr("Triangle Right-Up"),
                tr("Triangle Right-Down")
            });

            // Insert at the same position in parent layout
            if (QLayout* parentLayout = old->parentWidget() ? old->parentWidget()->layout() : nullptr) {
                if (auto* grid = qobject_cast<QGridLayout*>(parentLayout)) {
                    int row = -1, col = -1, rowSpan = 1, colSpan = 1;
                    int idx = grid->indexOf(old);
                    if (idx >= 0) {
                        grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
                    }
                    grid->removeWidget(old);
                    old->hide();
                    if (row >= 0 && col >= 0) {
                        grid->addWidget(repl, row, col, rowSpan, colSpan);
                    } else {
                        grid->addWidget(repl, grid->rowCount(), 0, 1, grid->columnCount() > 0 ? grid->columnCount() : 1);
                    }
                } else if (auto* hbox = qobject_cast<QHBoxLayout*>(parentLayout)) {
                    int idx = hbox->indexOf(old);
                    hbox->removeWidget(old);
                    old->hide();
                    if (idx >= 0) {
                        hbox->insertWidget(idx, repl);
                    } else {
                        hbox->addWidget(repl);
                    }
                } else if (auto* vbox = qobject_cast<QVBoxLayout*>(parentLayout)) {
                    int idx = vbox->indexOf(old);
                    vbox->removeWidget(old);
                    old->hide();
                    if (idx >= 0) {
                        vbox->insertWidget(idx, repl);
                    } else {
                        vbox->addWidget(repl);
                    }
                }
            }

            mCursorStyleDropDown = repl;
            old->setVisible(false);
        }
    }

    int value = settings.value("windowOpacity").toInt();
    if (mWindowOpacityField) {
        mWindowOpacityField->setValue(100 - value);
    }

    QPixmap previewCheckerboard(":background/checkerboard.png");
    QPixmap previewWhite(32, 32);
    QPixmap previewGrey(32, 32);
    QPixmap previewDots(":background/dots.png");
    QPixmap previewWeave(":background/weave.jpg");

    previewWhite.fill(Qt::white);
    previewGrey.fill(Qt::lightGray);

    ui->checkerBackgroundButton->setIcon(previewCheckerboard.scaled(32, 32));
    ui->whiteBackgroundButton->setIcon(previewWhite);
    ui->greyBackgroundButton->setIcon(previewGrey);
    ui->dotsBackgroundButton->setIcon(previewDots.scaled(32, 32));
    ui->weaveBackgroundButton->setIcon(previewWeave.scaled(32, 32));
    ui->backgroundButtons->setId(ui->checkerBackgroundButton, 1);
    ui->backgroundButtons->setId(ui->whiteBackgroundButton, 2);
    ui->backgroundButtons->setId(ui->greyBackgroundButton, 3);
    ui->backgroundButtons->setId(ui->dotsBackgroundButton, 4);
    ui->backgroundButtons->setId(ui->weaveBackgroundButton, 5);

    

    auto buttonClicked = static_cast<void (QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked);
    if (mLanguageDropDown) {
        connect(mLanguageDropDown, &CosmicDropDown::currentIndexChanged, this, &GeneralPage::languageChanged);
    }
    if (mWindowOpacityField) {
        connect(mWindowOpacityField, &CosmicInputField::valueChanged,
                this, [this](double value) { windowOpacityChange(static_cast<int>(value)); });
    }
    connect(ui->backgroundButtons, buttonClicked, this, &GeneralPage::backgroundChanged);
    connect(ui->shadowsBox, &QCheckBox::stateChanged, this, &GeneralPage::shadowsCheckboxStateChanged);
    connect(ui->toolCursorsBox, &QCheckBox::stateChanged, this, &GeneralPage::toolCursorsCheckboxStateChanged);
    connect(ui->antialiasingBox, &QCheckBox::stateChanged, this, &GeneralPage::antiAliasCheckboxStateChanged);
    if (mCurveSmoothingField) {
        connect(mCurveSmoothingField, &CosmicInputField::valueChanged,
                this, [this](double value) { curveSmoothingChanged(static_cast<int>(value)); });
    }
    connect(ui->highResBox, &QCheckBox::stateChanged, this, &GeneralPage::highResCheckboxStateChanged);
    // canvasCursorBox is hidden; keep preference functional but remove UI hookups that toggle enable state
    // connect(ui->canvasCursorBox, &QCheckBox::stateChanged, this, &GeneralPage::canvasCursorCheckboxStateChanged);
    if (mCursorStyleDropDown) {
        connect(mCursorStyleDropDown, &CosmicDropDown::currentIndexChanged, this, &GeneralPage::cursorStyleChanged);
    }
    connect(ui->alwaysShowCircleBox, &QCheckBox::stateChanged, this, &GeneralPage::alwaysShowCircleChanged);
    connect(ui->cursorPressureSizeBox, &QCheckBox::stateChanged, this, &GeneralPage::cursorPressureSizeChanged);
    connect(ui->rememberCursorSizeBox, &QCheckBox::stateChanged, this, &GeneralPage::rememberCursorSizeChanged);

    if (ui->shadowsBox) {
        ui->shadowsBox->setChecked(false);
        ui->shadowsBox->hide();
    }
    if (ui->alwaysShowCircleBox) {
        ui->alwaysShowCircleBox->setChecked(false);
        ui->alwaysShowCircleBox->hide();
    }
    if (ui->cursorPressureSizeBox) {
        ui->cursorPressureSizeBox->setChecked(false);
        ui->cursorPressureSizeBox->hide();
    }
    if (ui->rememberCursorSizeBox) {
        ui->rememberCursorSizeBox->setChecked(false);
        ui->rememberCursorSizeBox->hide();
    }
    
    if (mGridWidthField) {
        connect(mGridWidthField, &CosmicInputField::valueChanged,
                this, [this](double value) { gridWidthChanged(static_cast<int>(value)); });
    }
    if (mGridHeightField) {
        connect(mGridHeightField, &CosmicInputField::valueChanged,
                this, [this](double value) { gridHeightChanged(static_cast<int>(value)); });
    }
    connect(ui->actionSafeCheckBox, &QCheckBox::stateChanged, this, &GeneralPage::actionSafeCheckBoxStateChanged);
    if (mActionSafeField) {
        connect(mActionSafeField, &CosmicInputField::valueChanged,
                this, [this](double value) { actionSafeAreaChanged(static_cast<int>(value)); });
    }
    connect(ui->titleSafeCheckBox, &QCheckBox::stateChanged, this, &GeneralPage::titleSafeCheckBoxStateChanged);
    if (mTitleSafeField) {
        connect(mTitleSafeField, &CosmicInputField::valueChanged,
                this, [this](double value) { titleSafeAreaChanged(static_cast<int>(value)); });
    }
    connect(ui->safeHelperTextCheckbox, &QCheckBox::stateChanged, this, &GeneralPage::SafeAreaHelperTextCheckBoxStateChanged);
    connect(ui->gridCheckBox, &QCheckBox::stateChanged, this, &GeneralPage::gridCheckBoxStateChanged);
    if (mFramePoolSizeField) {
        connect(mFramePoolSizeField, &CosmicInputField::valueChanged,
                this, [this](double value) { frameCacheNumberChanged(static_cast<int>(value)); });
    }
    connect(ui->invertScrollDirectionBox, &QCheckBox::stateChanged, this, &GeneralPage::invertScrollDirectionBoxStateChanged);
    connect(ui->zoomHorizontalDragBox, &QCheckBox::stateChanged, this, [this](int b){ mManager->set(SETTING::ZOOM_HORIZONTAL_DRAG, b != Qt::Unchecked); });
    connect(ui->lowResNavigationBox, &QCheckBox::stateChanged, this, [this](int b){ mManager->set(SETTING::LOW_RES_NAVIGATION, b != Qt::Unchecked); });
    if (mUndoStepsField) {
        connect(mUndoStepsField, &CosmicInputField::valueChanged,
            this, [this](double value) { 
                mManager->set(SETTING::UNDO_REDO_MAX_STEPS, static_cast<int>(value));
                undoRedoMaxStepsChanged(); 
            });
    }

    // Inject a small control to configure Gap Checking background color in the Appearance box
    if (ui->appearanceBox) {
        auto* layout = ui->appearanceBox->layout();
        if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
            // Background color
            auto* labelBg = new QLabel(tr("Gap Check BG"), ui->appearanceBox);
            mGapCheckBgLabel = labelBg;
            mGapCheckBgButton = new QPushButton(ui->appearanceBox);
            mGapCheckBgButton->setText("");
            mGapCheckBgButton->setFixedSize(QSize(32, 18));
            mGapCheckBgButton->setCursor(Qt::PointingHandCursor);
            grid->addWidget(labelBg, grid->rowCount(), 0, 1, 1);
            grid->addWidget(mGapCheckBgButton, grid->rowCount()-1, 1, 1, 1);
            connect(mGapCheckBgButton, &QPushButton::clicked, this, [this]() {
                const int rgbaPref = mManager->getInt(SETTING::GAP_CHECK_BG_COLOR);
                const QColor initial = QColor::fromRgba(rgbaPref);
                auto* popup = new PopupColorWheelWidget(nullptr);
                if (initial.isValid()) popup->setColor(initial);
                // Only apply when the popup is closed (accepted via colorSelected or on close event)
                QPointer<GeneralPage> self(this);
                connect(popup, &PopupColorWheelWidget::colorSelected, this, [self](const QColor& c){
                    if (!self) return;
                    if (c.isValid()) {
                        self->mManager->set(SETTING::GAP_CHECK_BG_COLOR, static_cast<int>(c.rgba()));
                        self->refreshGapCheckColorButtons();
                    }
                });
                // Also update the swatch preview live without committing preference
                connect(popup, &PopupColorWheelWidget::colorChanged, this, [self](const QColor& c){
                    if (!self) return;
                    self->setButtonColorSwatch(self->mGapCheckBgButton, c);
                });
                // Note: Gap check colors are preference settings, not synced with main color wheel
                popup->showAt(QCursor::pos());
            });

            // Drawing color
            auto* labelDraw = new QLabel(tr("Gap Check Draw"), ui->appearanceBox);
            mGapCheckDrawLabel = labelDraw;
            mGapCheckDrawButton = new QPushButton(ui->appearanceBox);
            mGapCheckDrawButton->setText("");
            mGapCheckDrawButton->setFixedSize(QSize(32, 18));
            mGapCheckDrawButton->setCursor(Qt::PointingHandCursor);
            grid->addWidget(labelDraw, grid->rowCount(), 0, 1, 1);
            grid->addWidget(mGapCheckDrawButton, grid->rowCount()-1, 1, 1, 1);
            connect(mGapCheckDrawButton, &QPushButton::clicked, this, [this]() {
                const int rgbaPref = mManager->getInt(SETTING::GAP_CHECK_DRAW_COLOR);
                const QColor initial = QColor::fromRgba(rgbaPref);
                auto* popup = new PopupColorWheelWidget(nullptr);
                if (initial.isValid()) popup->setColor(initial);
                QPointer<GeneralPage> self(this);
                connect(popup, &PopupColorWheelWidget::colorSelected, this, [self](const QColor& c){
                    if (!self) return;
                    if (c.isValid()) {
                        self->mManager->set(SETTING::GAP_CHECK_DRAW_COLOR, static_cast<int>(c.rgba()));
                        self->refreshGapCheckColorButtons();
                    }
                });
                connect(popup, &PopupColorWheelWidget::colorChanged, this, [self](const QColor& c){
                    if (!self) return;
                    self->setButtonColorSwatch(self->mGapCheckDrawButton, c);
                });
                // Note: Gap check colors are preference settings, not synced with main color wheel
                popup->showAt(QCursor::pos());
            });
        } else if (auto* vbox = qobject_cast<QVBoxLayout*>(layout)) {
            // Background color row
            auto* rowBg = new QHBoxLayout();
            auto* labelBg = new QLabel(tr("Gap Check BG"), ui->appearanceBox);
            mGapCheckBgLabel = labelBg;
            mGapCheckBgButton = new QPushButton(ui->appearanceBox);
            mGapCheckBgButton->setText("");
            mGapCheckBgButton->setFixedSize(QSize(32, 18));
            mGapCheckBgButton->setCursor(Qt::PointingHandCursor);
            rowBg->addWidget(labelBg);
            rowBg->addStretch();
            rowBg->addWidget(mGapCheckBgButton);
            vbox->addLayout(rowBg);
            connect(mGapCheckBgButton, &QPushButton::clicked, this, [this]() {
                const int rgbaPref = mManager->getInt(SETTING::GAP_CHECK_BG_COLOR);
                const QColor initial = QColor::fromRgba(rgbaPref);
                auto* popup = new PopupColorWheelWidget(nullptr);
                if (initial.isValid()) popup->setColor(initial);
                QPointer<GeneralPage> self(this);
                connect(popup, &PopupColorWheelWidget::colorSelected, this, [self](const QColor& c){
                    if (!self) return;
                    if (c.isValid()) {
                        self->mManager->set(SETTING::GAP_CHECK_BG_COLOR, static_cast<int>(c.rgba()));
                        self->refreshGapCheckColorButtons();
                    }
                });
                connect(popup, &PopupColorWheelWidget::colorChanged, this, [self](const QColor& c){
                    if (!self) return;
                    self->setButtonColorSwatch(self->mGapCheckBgButton, c);
                });
                popup->showAt(QCursor::pos());
            });

            // Drawing color row
            auto* rowDraw = new QHBoxLayout();
            auto* labelDraw = new QLabel(tr("Gap Check Draw"), ui->appearanceBox);
            mGapCheckDrawLabel = labelDraw;
            mGapCheckDrawButton = new QPushButton(ui->appearanceBox);
            mGapCheckDrawButton->setText("");
            mGapCheckDrawButton->setFixedSize(QSize(32, 18));
            mGapCheckDrawButton->setCursor(Qt::PointingHandCursor);
            rowDraw->addWidget(labelDraw);
            rowDraw->addStretch();
            rowDraw->addWidget(mGapCheckDrawButton);
            vbox->addLayout(rowDraw);
            connect(mGapCheckDrawButton, &QPushButton::clicked, this, [this]() {
                const int rgbaPref = mManager->getInt(SETTING::GAP_CHECK_DRAW_COLOR);
                const QColor initial = QColor::fromRgba(rgbaPref);
                auto* popup = new PopupColorWheelWidget(nullptr);
                if (initial.isValid()) popup->setColor(initial);
                QPointer<GeneralPage> self(this);
                connect(popup, &PopupColorWheelWidget::colorSelected, this, [self](const QColor& c){
                    if (!self) return;
                    if (c.isValid()) {
                        self->mManager->set(SETTING::GAP_CHECK_DRAW_COLOR, static_cast<int>(c.rgba()));
                        self->refreshGapCheckColorButtons();
                    }
                });
                connect(popup, &PopupColorWheelWidget::colorChanged, this, [self](const QColor& c){
                    if (!self) return;
                    self->setButtonColorSwatch(self->mGapCheckDrawButton, c);
                });
                // Note: Gap check colors are preference settings, not synced with main color wheel
                popup->showAt(QCursor::pos());
            });
        }
        // Initialize swatches from current preferences
        refreshGapCheckColorButtons();
    }
}

GeneralPage::~GeneralPage()
{
    delete ui;
}

void GeneralPage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslatePageStrings();
    }
}

void GeneralPage::retranslatePageStrings()
{
    // Helper: use translated string, fall back to source if translation is empty.
    // This prevents headers/labels from vanishing when a language file has an
    // empty (unfinished) entry for a string.
    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    // Retranslate all strings coming from the .ui file.
    ui->retranslateUi(this);

    // Re-apply .ui labels explicitly so an empty translation never blanks them.
    // retranslateUi() sets them from the .qm file; if the entry is empty we
    // restore the English source string as a safe fallback.
    auto guardLabel = [](QLabel* lbl, const char* src) {
        if (lbl && lbl->text().isEmpty())
            lbl->setText(QString::fromUtf8(src));
    };
    guardLabel(ui->cursorStyleLabel,  "Cursor Shape:");
    guardLabel(ui->labGridW,          "Grid Width");
    guardLabel(ui->labGridH,          "Grid Height");
    guardLabel(ui->labActionSafe,     "Enable Action Safe area (%)");
    guardLabel(ui->labTitleSafe,      "Enable Title Safe area (%)");
    guardLabel(ui->labSafeHelperText, "Show Safe area labels");
    guardLabel(ui->cacheLabel,        "Memory Cache Budget");
    guardLabel(ui->undoStepsLabel,    "Maximum Number of Undo/Redo Steps");

    // Update dynamically-created section header labels.
    if (mHeaderLanguage)   mHeaderLanguage->setText(safetr("Language"));
    if (mHeaderAppearance) mHeaderAppearance->setText(safetr("Appearance"));
    if (mHeaderCursors)    mHeaderCursors->setText(safetr("Cursors"));
    if (mHeaderCanvas)     mHeaderCanvas->setText(safetr("Canvas"));
    if (mHeaderGrid)       mHeaderGrid->setText(safetr("Grid"));
    if (mHeaderOverlays)   mHeaderOverlays->setText(safetr("Overlays"));
    if (mHeaderAdvanced)   mHeaderAdvanced->setText(safetr("Advanced"));
    if (mHeaderUndoRedo)   mHeaderUndoRedo->setText(safetr("Undo / Redo"));

    // Sync translated text from each hidden QCheckBox to its visible CosmicCheckboxToggle.
    // If the translated text is empty (unfinished entry), keep the previous text.
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

    // Update dynamically-created Gap Check color picker labels.
    if (mGapCheckBgLabel)   mGapCheckBgLabel->setText(safetr("Gap Check BG"));
    if (mGapCheckDrawLabel) mGapCheckDrawLabel->setText(safetr("Gap Check Draw"));

    // Update cursor style dropdown options with translated strings.
    if (mCursorStyleDropDown) {
        const int savedIndex = mCursorStyleDropDown->currentIndex();
        mCursorStyleDropDown->setOptions({
            tr("Circle"),
            tr("Cross"),
            tr("Triangle Up"),
            tr("Triangle Down"),
            tr("Triangle Left"),
            tr("Triangle Right"),
            tr("Triangle Left-Up"),
            tr("Triangle Left-Down"),
            tr("Triangle Right-Up"),
            tr("Triangle Right-Down")
        });
        QSignalBlocker b(mCursorStyleDropDown);
        mCursorStyleDropDown->setCurrentIndex(savedIndex);
    }

    // Rebuild language dropdown display names with translated language names.
    if (mLanguageDropDown && mManager) {
        // Use the saved locale code to find the correct index — do NOT rely on
        // currentIndex() which may be stale if retranslation fires before updateValues().
        const QString savedLocale = mManager->getString(SETTING::LANGUAGE);
        const int savedIndex = savedLocale.isEmpty()
            ? mLanguageDropDown->currentIndex()   // system language: keep whatever is shown
            : mLanguageLocaleCodes.indexOf(savedLocale);

        // Re-build the same language table used in the constructor.
        QString languages[][3] = {
            { tr("Arabic"),                  QStringLiteral("العربية"),           "ar"    },
            { tr("Bulgarian"),               QStringLiteral("Български"),         "bg"    },
            { tr("Catalan"),                 QStringLiteral("Català"),            "ca"    },
            { tr("Czech"),                   QStringLiteral("Čeština"),           "cs"    },
            { tr("Danish"),                  QStringLiteral("Dansk"),             "da"    },
            { tr("German"),                  QStringLiteral("Deutsch"),           "de"    },
            { tr("Greek"),                   QStringLiteral("Ελληνικά"),          "el"    },
            { tr("English"),                 QStringLiteral("English"),           "en"    },
            { tr("Spanish"),                 QStringLiteral("Español"),           "es"    },
            { tr("Estonian"),                QStringLiteral("Eesti"),             "et"    },
            { tr("Persian"),                 QStringLiteral("فارسی"),             "fa"    },
            { tr("French"),                  QStringLiteral("Français"),          "fr"    },
            { tr("Hebrew"),                  QStringLiteral("עברית"),             "he"    },
            { tr("Hungarian"),               QStringLiteral("Magyar"),            "hu_HU" },
            { tr("Indonesian"),              QStringLiteral("Bahasa Indonesia"),  "id"    },
            { tr("Italian"),                 QStringLiteral("Italiano"),          "it"    },
            { tr("Japanese"),                QStringLiteral("日本語"),             "ja"    },
            { tr("Kabyle"),                  QStringLiteral("Taqbaylit"),         "kab"   },
            { tr("Korean"),                  QStringLiteral("한국어"),             "ko"    },
            { tr("Norwegian Bokmål"),        QStringLiteral("Norsk bokmål"),      "nb"    },
            { tr("Dutch \u2013 Netherlands"),QStringLiteral("Nederlands \u2013 Nederland"), "nl_NL" },
            { tr("Polish"),                  QStringLiteral("Polski"),            "pl"    },
            { tr("Portuguese \u2013 Portugal"), QStringLiteral("Português \u2013 Portugal"), "pt_PT" },
            { tr("Portuguese \u2013 Brazil"),QStringLiteral("Português \u2013 Brasil"),     "pt_BR" },
            { tr("Russian"),                 QStringLiteral("Русский"),           "ru"    },
            { tr("Slovene"),                 QStringLiteral("Slovenščina"),       "sl"    },
            { tr("Swedish"),                 QStringLiteral("Svenska"),           "sv"    },
            { tr("Turkish"),                 QStringLiteral("Türkçe"),            "tr"    },
            { tr("Vietnamese"),              QStringLiteral("Tiếng Việt"),        "vi"    },
            { tr("Cantonese"),               QStringLiteral("粵语"),              "yue"   },
            { tr("Chinese \u2013 China"),    QStringLiteral("简体中文"),           "zh_CN" },
            { tr("Chinese \u2013 Taiwan"),   QStringLiteral("繁體中文"),           "zh_TW" },
        };

        QStringList displayOptions;
        for (auto& lang : languages) {
            displayOptions << QStringLiteral("%1 (%2)").arg(lang[0]).arg(lang[1]);
        }

        QSignalBlocker b(mLanguageDropDown);
        mLanguageDropDown->setOptions(displayOptions);
        if (savedIndex >= 0 && savedIndex < displayOptions.size()) {
            mLanguageDropDown->setCurrentIndex(savedIndex);
        }
    }
}

void GeneralPage::updateAccentColors()
{
    const QColor accent = BlenderStyleSlider::getAccentColor();
    if (mLanguageDropDown) mLanguageDropDown->setAccentColor(accent);
    if (mCursorStyleDropDown) mCursorStyleDropDown->setAccentColor(accent);
}

void GeneralPage::updateValues()
{
    if (mLanguageDropDown) {
        const QString targetLocale = mManager->getString(SETTING::LANGUAGE);
        int index = mLanguageLocaleCodes.indexOf(targetLocale);
        if (index >= 0) {
            QSignalBlocker b(mLanguageDropDown);
            mLanguageDropDown->setCurrentIndex(index);
        }
        // If targetLocale is empty (system language) or not found, leave the
        // dropdown at whatever index it currently shows — don't force it to 0.
    }

    if (mCurveSmoothingField) {
        QSignalBlocker b1(mCurveSmoothingField);
        mCurveSmoothingField->setValue(mManager->getInt(SETTING::CURVE_SMOOTHING));
    }
    if (mWindowOpacityField) {
        QSignalBlocker b2(mWindowOpacityField);
        mWindowOpacityField->setValue(100 - mManager->getInt(SETTING::WINDOW_OPACITY));
    }
    QSignalBlocker b3(ui->shadowsBox);
    ui->shadowsBox->setChecked(mManager->isOn(SETTING::SHADOW));
    QSignalBlocker b4(ui->toolCursorsBox);
    ui->toolCursorsBox->setChecked(mManager->isOn(SETTING::TOOL_CURSOR));
    QSignalBlocker b5(ui->antialiasingBox);
    ui->antialiasingBox->setChecked(mManager->isOn(SETTING::ANTIALIAS));
    QSignalBlocker b6(ui->canvasCursorBox);
    ui->canvasCursorBox->setChecked(mManager->isOn(SETTING::CANVAS_CURSOR));
    if (mGridWidthField) {
        QSignalBlocker b7(mGridWidthField);
        mGridWidthField->setValue(mManager->getInt(SETTING::GRID_SIZE_W));
    }
    if (mGridHeightField) {
        QSignalBlocker b11(mGridHeightField);
        mGridHeightField->setValue(mManager->getInt(SETTING::GRID_SIZE_H));
    }
    QSignalBlocker b8(ui->gridCheckBox);
    ui->gridCheckBox->setChecked(mManager->isOn(SETTING::GRID));
    QSignalBlocker b16(ui->actionSafeCheckBox);

    bool actionSafeOn = mManager->isOn(SETTING::ACTION_SAFE_ON);
    ui->actionSafeCheckBox->setChecked(actionSafeOn);
    if (mActionSafeField) {
        QSignalBlocker b14(mActionSafeField);
        mActionSafeField->setValue(mManager->getInt(SETTING::ACTION_SAFE));
    }
    QSignalBlocker b17(ui->titleSafeCheckBox);
    bool titleSafeOn = mManager->isOn(SETTING::TITLE_SAFE_ON);
    ui->titleSafeCheckBox->setChecked(titleSafeOn);
    if (mTitleSafeField) {
        QSignalBlocker b15(mTitleSafeField);
        mTitleSafeField->setValue(mManager->getInt(SETTING::TITLE_SAFE));
    }

    QSignalBlocker b18(ui->safeHelperTextCheckbox);
    ui->safeHelperTextCheckbox->setChecked(mManager->isOn(SETTING::OVERLAY_SAFE_HELPER_TEXT_ON));

    QSignalBlocker b9(ui->highResBox);
    ui->highResBox->setChecked(mManager->isOn(SETTING::HIGH_RESOLUTION));

    QSignalBlocker b10(ui->backgroundButtons);
    QString bgName = mManager->getString(SETTING::BACKGROUND_STYLE);

    if (mFramePoolSizeField) {
        QSignalBlocker b12(mFramePoolSizeField);
        mFramePoolSizeField->setValue(mManager->getInt(SETTING::FRAME_POOL_SIZE));
    }

    if (mUndoStepsField) {
        QSignalBlocker bUndoRedoLimitSpinBox(mUndoStepsField);
        mUndoStepsField->setValue(mManager->getInt(SETTING::UNDO_REDO_MAX_STEPS));
    }

    int buttonIdx = 1;
    if (bgName == "checkerboard") buttonIdx = 1;
    else if (bgName == "white")   buttonIdx = 2;
    else if (bgName == "grey")    buttonIdx = 3;
    else if (bgName == "dots")    buttonIdx = 4;
    else if (bgName == "weave")   buttonIdx = 5;
    else {
        // Unknown/corrupt value in settings; fall back to a safe default.
        buttonIdx = 0;
    }

    ui->backgroundButtons->button(buttonIdx)->setChecked(true);

    ui->invertScrollDirectionBox->setChecked(mManager->isOn(SETTING::INVERT_SCROLL_ZOOM_DIRECTION));
    ui->zoomHorizontalDragBox->setChecked(mManager->isOn(SETTING::ZOOM_HORIZONTAL_DRAG));
    ui->lowResNavigationBox->setChecked(mManager->isOn(SETTING::LOW_RES_NAVIGATION));
    {
        if (mCursorStyleDropDown) {
            QSignalBlocker b8(mCursorStyleDropDown);
            mCursorStyleDropDown->setCurrentIndex(mManager->getInt(SETTING::CURSOR_STYLE));
        }
    }
    {
        QSignalBlocker b9(ui->alwaysShowCircleBox);
        ui->alwaysShowCircleBox->setChecked(mManager->isOn(SETTING::ALWAYS_SHOW_CIRCLE));
    }
    {
        QSignalBlocker b10(ui->cursorPressureSizeBox);
        ui->cursorPressureSizeBox->setChecked(mManager->isOn(SETTING::CURSOR_PRESSURE_SIZE));
    }
    {
        QSignalBlocker b11(ui->rememberCursorSizeBox);
        ui->rememberCursorSizeBox->setChecked(mManager->isOn(SETTING::REMEMBER_CURSOR_SIZE));
    }
    // Ensure defaults exist
    (void)mManager->isOn(SETTING::GAP_CHECK_MODE);
    (void)mManager->getInt(SETTING::GAP_CHECK_BG_COLOR);
    (void)mManager->getInt(SETTING::GAP_CHECK_DRAW_COLOR);
    refreshGapCheckColorButtons();
}

void GeneralPage::languageChanged(int i)
{
    if (!mManager) {
        return;
    }

    if (mIsApplyingLanguageChange) {
        return;
    }

    if (i < 0 || i >= mLanguageLocaleCodes.size()) {
        return;
    }

    mIsApplyingLanguageChange = true;

    const QString strLocale = mLanguageLocaleCodes.at(i);
    mManager->set(SETTING::LANGUAGE, strLocale);

    // Delegate translator management to Pencil2D so there is one authoritative
    // owner of the app translator — no more double-translator conflicts.
    const bool loaded = Pencil2D::switchAppTranslator(strLocale);

    mIsApplyingLanguageChange = false;

    // Show the restart notice after returning to the event loop so the dropdown
    // popup has already closed — avoids any nested-event-loop interaction.
    QPointer<GeneralPage> self(this);
    QTimer::singleShot(0, this, [self, loaded]() {
        if (!self) return;

        auto* popup = new CosmicWarningPopup(self);
        popup->setAttribute(Qt::WA_DeleteOnClose, true);
        popup->setWarningIconEnabled(true);

        if (loaded) {
            popup->setTitleText(QCoreApplication::translate("GeneralPage", "Language Changed"));
            popup->setBodyText(QCoreApplication::translate("GeneralPage",
                "The language has been applied. Some UI elements may require a restart to fully update."));
        } else {
            popup->setTitleText(QCoreApplication::translate("GeneralPage", "Language Unavailable"));
            popup->setBodyText(QCoreApplication::translate("GeneralPage",
                "No translation was found for the selected language. The interface will remain in the current language."));
        }
        popup->setConfirmText(QCoreApplication::translate("GeneralPage", "OK"));
        popup->exec();
    });
}

void GeneralPage::backgroundChanged(QAbstractButton* button)
{
    QString brushName = "white";
    if (button == ui->checkerBackgroundButton)    brushName = "checkerboard";
    else if (button == ui->whiteBackgroundButton) brushName = "white";
    else if (button == ui->greyBackgroundButton)  brushName = "grey";
    else if (button == ui->dotsBackgroundButton)  brushName = "dots";
    else if (button == ui->weaveBackgroundButton) brushName = "weave";
    else return; // unknown button — ignore rather than crash
    mManager->set(SETTING::BACKGROUND_STYLE, brushName);
}

void GeneralPage::curveSmoothingChanged(int value)
{
    mManager->set(SETTING::CURVE_SMOOTHING, value);
}

void GeneralPage::highResCheckboxStateChanged(int b)
{
    mManager->set(SETTING::HIGH_RESOLUTION, b != Qt::Unchecked);
}

void GeneralPage::shadowsCheckboxStateChanged(int b)
{
    mManager->set(SETTING::SHADOW, b != Qt::Unchecked);
}

void GeneralPage::antiAliasCheckboxStateChanged(int b)
{
    mManager->set(SETTING::ANTIALIAS, b != Qt::Unchecked);
}

void GeneralPage::toolCursorsCheckboxStateChanged(int b)
{
    mManager->set(SETTING::TOOL_CURSOR, b != Qt::Unchecked);
}

void GeneralPage::canvasCursorCheckboxStateChanged(int b)
{
    mManager->set(SETTING::CANVAS_CURSOR, b != Qt::Unchecked);
}

void GeneralPage::cursorStyleChanged(int index)
{
    mManager->set(SETTING::CURSOR_STYLE, index);
}

void GeneralPage::alwaysShowCircleChanged(int b)
{
    mManager->set(SETTING::ALWAYS_SHOW_CIRCLE, b != Qt::Unchecked);
}

void GeneralPage::gridWidthChanged(int value)
{
    mManager->set(SETTING::GRID_SIZE_W, value);
}

void GeneralPage::gridHeightChanged(int value)
{
    mManager->set(SETTING::GRID_SIZE_H, value);
}

void GeneralPage::actionSafeCheckBoxStateChanged(int b)
{
    mManager->set(SETTING::ACTION_SAFE_ON, b != Qt::Unchecked);
    updateSafeHelperTextEnabledState();
}

void GeneralPage::actionSafeAreaChanged(int value)
{
    mManager->set(SETTING::ACTION_SAFE, value);
}

void GeneralPage::titleSafeCheckBoxStateChanged(int b)
{
    mManager->set(SETTING::TITLE_SAFE_ON, b != Qt::Unchecked);
    updateSafeHelperTextEnabledState();
}

void GeneralPage::updateSafeHelperTextEnabledState()
{
    if (!ui->actionSafeCheckBox->isChecked() && !ui->titleSafeCheckBox->isChecked()) {
        ui->safeHelperTextCheckbox->setEnabled(false);
        ui->labSafeHelperText->setEnabled(false);
    } else {
        ui->safeHelperTextCheckbox->setEnabled(true);
        ui->labSafeHelperText->setEnabled(true);
    }
}

void GeneralPage::SafeAreaHelperTextCheckBoxStateChanged(int b)
{
    mManager->set(SETTING::OVERLAY_SAFE_HELPER_TEXT_ON, b != Qt::Unchecked);
}

void GeneralPage::titleSafeAreaChanged(int value)
{
    mManager->set(SETTING::TITLE_SAFE, value);
}

void GeneralPage::gridCheckBoxStateChanged(int b)
{
    mManager->set(SETTING::GRID, b != Qt::Unchecked);
}

void GeneralPage::frameCacheNumberChanged(int value)
{
    mManager->set(SETTING::FRAME_POOL_SIZE, value);
}

void GeneralPage::invertScrollDirectionBoxStateChanged(int b)
{
    mManager->set(SETTING::INVERT_SCROLL_ZOOM_DIRECTION, b != Qt::Unchecked);
}

void GeneralPage::undoRedoMaxStepsChanged()
{
    ui->undoRedoGroupApplyButton->setEnabled(true);
    ui->undoRedoGroupCancelButton->setEnabled(true);
}

void GeneralPage::refreshGapCheckColorButtons()
{
    if (!mManager) return;
    if (mGapCheckBgButton) {
        const QColor c = QColor::fromRgba(mManager->getInt(SETTING::GAP_CHECK_BG_COLOR));
        setButtonColorSwatch(mGapCheckBgButton, c.isValid() ? c : QColor(0,255,255));
    }
    if (mGapCheckDrawButton) {
        const QColor c = QColor::fromRgba(mManager->getInt(SETTING::GAP_CHECK_DRAW_COLOR));
        setButtonColorSwatch(mGapCheckDrawButton, c.isValid() ? c : QColor(211,211,211));
    }
}

void GeneralPage::cursorPressureSizeChanged(int b)
{
    mManager->set(SETTING::CURSOR_PRESSURE_SIZE, b != Qt::Unchecked);
}

void GeneralPage::rememberCursorSizeChanged(int b)
{
    mManager->set(SETTING::REMEMBER_CURSOR_SIZE, b != Qt::Unchecked);
}

void GeneralPage::setButtonColorSwatch(QPushButton* button, const QColor& color)
{
    if (!button) return;
    QString style = QString("QPushButton { border: 1px solid #666; background-color: %1; }")
                        .arg(color.name(QColor::HexArgb));
    button->setStyleSheet(style);
}
