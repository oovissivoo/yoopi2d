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
#include "colorpalettewidget.h"
#include "ui_colorpalette.h"

// Standard libraries
#include <cmath>

// Qt
#include <QDebug>
#include <QListWidgetItem>
#include "popupcolorpicker.h"
#include <QPushButton>
#include <QSettings>
#include <QtMath>
#include <QScrollBar>
#include <QAbstractItemModel>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QApplication>
#include <QStyle>
#include <QMouseEvent>

// Preset system and utilities
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSvgRenderer>
#include <QHBoxLayout>

// App
#include "filedialog.h"
#include "cosmicwarningpopup.h"
#include "cosmicconfirmpopup.h"
#include "cosmicpopupcontextmenu.h"
#include "cosmicinputpopup.h"

// Project
#include "colorref.h"
#include "object.h"
#include "editor.h"
#include "layerbitmap.h"
#include "colormanager.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicuimetrics.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "preferencemanager.h"
#include "preferencesdef.h"

namespace
{
    constexpr int kColorRole = Qt::UserRole + 1;

    class PaletteGridItemDelegate final : public QStyledItemDelegate
    {
    public:
        explicit PaletteGridItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, false);

            const QRect r = opt.rect;
            const QColor color = index.data(kColorRole).value<QColor>();
            const bool isSelected = opt.state & QStyle::State_Selected;
            const QColor emptyCellColor(QStringLiteral("#2b2b2b"));

            if (isSelected)
            {
                // Both borders are inset — the cell still fills its full rect.
                // Outer ring: CosmicColor::accent() (blue-violet), 2px
                // Inner ring: #ff1a40 (red), 2px
                // Color fill in the remaining center.
                constexpr int outerW = 2;
                constexpr int innerW = 2;

                painter->fillRect(r, CosmicColor::accent());
                const QRect afterOuter = r.adjusted(outerW, outerW, -outerW, -outerW);
                painter->fillRect(afterOuter, QColor(QStringLiteral("#ff1a40")));
                const QRect colorRect = afterOuter.adjusted(innerW, innerW, -innerW, -innerW);
                if (colorRect.isValid())
                    painter->fillRect(colorRect, color.isValid() ? color : emptyCellColor);
            }
            else
            {
                // Fill the entire cell rect — no border, no shrink, no gap.
                painter->fillRect(r, color.isValid() ? color : emptyCellColor);
            }

            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            Q_UNUSED(index)
            return option.decorationSize.isValid() ? option.decorationSize : QStyledItemDelegate::sizeHint(option, index);
        }
    };

    // Delegate for list mode: square color swatch on the left, editable label on the right.
    // Active row label area is filled with CosmicColor::accent(). No gaps between rows.
    class PaletteListItemDelegate final : public QStyledItemDelegate
    {
    public:
        explicit PaletteListItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, false);

            const QRect r = option.rect;
            const QColor swatchColor = index.data(kColorRole).value<QColor>();
            const bool isSelected = option.state & QStyle::State_Selected;

            // Square swatch: full row height, same width as height
            const int swatchSize = r.height();
            const QRect swatchRect(r.left(), r.top(), swatchSize, r.height());

            // Label area: remainder of the row
            const QRect labelRect(r.left() + swatchSize, r.top(), r.width() - swatchSize, r.height());

            // Draw swatch
            const QColor emptyCellColor(QStringLiteral("#2b2b2b"));
            painter->fillRect(swatchRect, swatchColor.isValid() ? swatchColor : emptyCellColor);

            // Draw label background
            if (isSelected)
            {
                painter->fillRect(labelRect, CosmicColor::accent());
            }
            else
            {
                painter->fillRect(labelRect, QColor(QStringLiteral("#2b2b2b")));
            }

            // Draw label text — skip when the delegate editor is active on this row
            const bool isEditing = option.state & QStyle::State_Editing;
            if (!isEditing)
            {
                const QString text = index.data(Qt::DisplayRole).toString();
                painter->setPen(Qt::white);
                QFont f = option.font;
                f.setBold(true);
                painter->setFont(f);
                const QRect textRect = labelRect.adjusted(8, 0, -4, 0);
                painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
            }

            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            Q_UNUSED(index)
            // Row height driven by icon/decoration size set on the list widget
            const int h = option.decorationSize.isValid() ? option.decorationSize.height() : 28;
            return QSize(option.rect.width(), h);
        }

        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            Q_UNUSED(option); Q_UNUSED(index);
            auto* edit = new QLineEdit(parent);
            edit->setFrame(false);
            edit->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            edit->setTextMargins(0, 0, 0, 0);
            edit->setStyleSheet(CosmicInlineEdit::stylesheet());
            return edit;
        }

        void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            Q_UNUSED(index);
            const int swatchSize = option.rect.height();
            const QFontMetrics fm(option.font);
            const int editH = fm.height() + 4;
            const int editY = option.rect.top() + (option.rect.height() - editH) / 2;
            editor->setGeometry(QRect(option.rect.left() + swatchSize + 4,
                                      editY,
                                      option.rect.width() - swatchSize - 8,
                                      editH));
        }
    };
}

namespace {
QIcon loadSvgIconForCosmicPalette(const QString& path, const QSize& size)
{
    QIcon icon;
    QSvgRenderer renderer(path);
    if (renderer.isValid()) {
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
        return icon;
    }
    return QIcon(path);
}

void removeSpacerItemFromLayout(QHBoxLayout* layout, QSpacerItem*& spacer)
{
    if (!layout || !spacer) {
        return;
    }
    const int idx = layout->indexOf(spacer);
    if (idx < 0) {
        return;
    }
    QLayoutItem* item = layout->takeAt(idx);
    delete item;
    spacer = nullptr;
}

void clearLayoutItems(QHBoxLayout* layout)
{
    if (!layout) {
        return;
    }

    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (!item) {
            continue;
        }
        delete item;
    }
}

void rebuildPaletteTopRow(Ui::ColorPalette* ui,
                          CosmicButton* addButton,
                          CosmicButton* replaceButton,
                          CosmicButton* removeButton,
                          CosmicDropDown* presetDropDown,
                          CosmicButton* settingsButton)
{
    if (!ui || !ui->horizontalLayout || !addButton || !replaceButton || !removeButton || !presetDropDown || !settingsButton) {
        return;
    }

    if (ui->horizontalLayout->property("CosmicPaletteTopRowRebuilt").toBool()) {
        return;
    }

    // Remove legacy expanding spacers that were used to center a fixed-width combo.
    removeSpacerItemFromLayout(ui->horizontalLayout, ui->horizontalSpacer_2);
    removeSpacerItemFromLayout(ui->horizontalLayout, ui->horizontalSpacer);

    // Hide legacy widgets but keep them alive.
    if (ui->addColorButton) ui->addColorButton->hide();
    if (ui->removeColorButton) ui->removeColorButton->hide();
    if (ui->presetComboBox) ui->presetComboBox->hide();
    if (ui->palettePref) ui->palettePref->hide();

    // Clear the row and re-add in the required order.
    clearLayoutItems(ui->horizontalLayout);
    ui->horizontalLayout->addWidget(addButton);
    ui->horizontalLayout->addWidget(removeButton);
    ui->horizontalLayout->addWidget(presetDropDown);
    ui->horizontalLayout->addWidget(replaceButton);
    ui->horizontalLayout->addWidget(settingsButton);

    const int presetIdx = ui->horizontalLayout->indexOf(presetDropDown);
    if (presetIdx >= 0) {
        ui->horizontalLayout->setStretch(presetIdx, 1);
    }

    ui->horizontalLayout->setProperty("CosmicPaletteTopRowRebuilt", true);
}
}

// Built-in preset definitions
const QString ColorPaletteWidget::BUILTIN_PRESET_GENGA = "Genga";
const QString ColorPaletteWidget::BUILTIN_PRESET_GENGA_FLUSHED = "Genga Flushed";
const QStringList ColorPaletteWidget::GENGA_COLORS = {"FE7FFE", "FE7F01", "FF8180", "FFFF00", "807FFF", "7FFEBF", "7FFFFF"};
const QStringList ColorPaletteWidget::GENGA_FLUSHED_COLORS = {"FECBFE", "FFAF64", "FFB3B3", "FEFE7F", "A5A5FE", "CBFEE5", "B2FEFE"};


ColorPaletteWidget::ColorPaletteWidget(QWidget* parent) :
    BaseDockWidget(parent),
    ui(new Ui::ColorPalette)
{
    ui->setupUi(this);
    // Initialize preset-related members
    mPresetComboBox = nullptr;
    mImportPresetAction = nullptr;
    mExportPresetAction = nullptr;
    mDeletePresetAction = nullptr;
    mPresetSeparator = nullptr;
    mNewPresetAction = nullptr;
    mRenamePresetAction = nullptr;
    mPresetsDirectory.clear();
    mPresetsMetadataFile.clear();
}

ColorPaletteWidget::~ColorPaletteWidget()
{
    delete ui;
}

void ColorPaletteWidget::initUI()
{
    if (!mEditor) {
        qCritical() << "ColorPaletteWidget::initUI: mEditor is null!";
        return;
    }
    QSettings settings(PENCIL2D, PENCIL2D);
    int colorGridSize = settings.value("PreferredColorGridSize", 34).toInt();
    mFitSwatches = settings.value("FitSwatchSize", false).toBool();
    if (mFitSwatches)
    {
        fitSwatchSize();
    }

    mIconSize = QSize(colorGridSize, colorGridSize);

    ui->colorListWidget->setContextMenuPolicy(Qt::NoContextMenu);

    if (!mDefaultItemDelegate)
    {
        mDefaultItemDelegate = ui->colorListWidget->itemDelegate();
    }
    if (!mGridItemDelegate)
    {
        mGridItemDelegate = new PaletteGridItemDelegate(ui->colorListWidget);
    }
    if (!mListItemDelegate)
    {
        mListItemDelegate = new PaletteListItemDelegate(ui->colorListWidget);
    }

    // Apply pill scrollbar styling to match tool panels
    ui->colorListWidget->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { padding: 0px; margin: 0px; border: none; }"
        "QListWidget::item:selected { background: transparent; }"
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
        "QScrollBar:horizontal {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  height: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-width: 26px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: transparent; width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
    ));

    QString sViewMode = settings.value("ColorPaletteViewMode", "ListMode").toString();
    if (sViewMode == "ListMode")
        setListMode();
    else
        setGridMode();

    buttonStylesheet = "::menu-indicator{ image: none; }"
                             "QPushButton { border: 0px; }"
                             "QPushButton:pressed { border: 1px solid #ADADAD; border-radius: 2px; background-color: #D5D5D5; }"
                             "QPushButton:checked { border: 1px solid #ADADAD; border-radius: 2px; background-color: #D5D5D5; }";

    if (ui->horizontalLayout && !mAddColorButtonCosmic) {
        auto configureCosmicToolbarButton = [](CosmicButton* b) {
            b->setBordered(false);
            b->setFixedSize(28, 28);
            b->setIconSize(QSize(18, 18));
        };

        mAddColorButtonCosmic = new CosmicButton(this);
        configureCosmicToolbarButton(mAddColorButtonCosmic);
        mAddColorButtonCosmic->setIcon(loadSvgIconForCosmicPalette(QStringLiteral(":/Interaction/LayerAdd.svg"), QSize(18, 18)));
        mAddColorButtonCosmic->setToolTip(tr("Add Color"));

        mReplaceColorButtonCosmic = new CosmicButton(this);
        configureCosmicToolbarButton(mReplaceColorButtonCosmic);
        mReplaceColorButtonCosmic->setIcon(loadSvgIconForCosmicPalette(QStringLiteral(":/Interaction/PalletReplaceColor.svg"), QSize(18, 18)));
        mReplaceColorButtonCosmic->setToolTip(tr("Replace Palette Color"));

        mRemoveColorButtonCosmic = new CosmicButton(this);
        configureCosmicToolbarButton(mRemoveColorButtonCosmic);
        mRemoveColorButtonCosmic->setIcon(loadSvgIconForCosmicPalette(QStringLiteral(":/Interaction/LayerRemove.svg"), QSize(18, 18)));
        mRemoveColorButtonCosmic->setToolTip(tr("Remove Color"));

        mPalettePrefButtonCosmic = new CosmicButton(this);
        configureCosmicToolbarButton(mPalettePrefButtonCosmic);
        mPalettePrefButtonCosmic->setIcon(loadSvgIconForCosmicPalette(QStringLiteral(":/Interaction/settings.svg"), QSize(18, 18)));
        mPalettePrefButtonCosmic->setToolTip(tr("Palette Preferences"));

        connect(mAddColorButtonCosmic, &QPushButton::clicked, this, &ColorPaletteWidget::clickAddColorButton);
        connect(mReplaceColorButtonCosmic, &QPushButton::clicked, this, &ColorPaletteWidget::replaceItem);
        connect(mRemoveColorButtonCosmic, &QPushButton::clicked, this, &ColorPaletteWidget::clickRemoveColorButton);
    }

    ui->addColorButton->setStyleSheet(buttonStylesheet);
    ui->removeColorButton->setStyleSheet(buttonStylesheet);
    // colorDialogButton has been replaced by preset combo box in UI

    // Set up highlight color
    updateHighlightColor();
    
    // Connect to preference changes for accent color updates
    if (mEditor && mEditor->preference()) {
        connect(mEditor->preference(), &PreferenceManager::optionChanged, 
                this, &ColorPaletteWidget::onAccentColorChanged, Qt::UniqueConnection);
    }

    palettePreferences();

    connect(ui->colorListWidget, &QListWidget::itemClicked, this, &ColorPaletteWidget::clickColorListItem);
    connect(ui->colorListWidget->model(), &QAbstractItemModel::rowsMoved, this, &ColorPaletteWidget::onRowsMoved);

    ui->colorListWidget->viewport()->installEventFilter(this);

    connect(ui->colorListWidget, &QListWidget::itemDoubleClicked, this, &ColorPaletteWidget::changeColorName);
    connect(ui->colorListWidget, &QListWidget::itemChanged, this, &ColorPaletteWidget::onItemChanged);

    connect(ui->addColorButton, &QPushButton::clicked, this, &ColorPaletteWidget::clickAddColorButton);
    // colorDialogButton removed in UI; no connection here
    connect(ui->removeColorButton, &QPushButton::clicked, this, &ColorPaletteWidget::clickRemoveColorButton);

    connect(editor(), &Editor::objectLoaded, this, &ColorPaletteWidget::updateUI);

    // Preset system
    // During initialization, avoid changing the tool's current color when
    // applying presets. We'll restore the tool color to black after setup.
    try {
        mSuppressToolColorChangeDuringInit = true;
        // Also set a global app property so ColorBox (color wheel) can skip
        // updating itself from ColorManager during initialization.
        qApp->setProperty("SuppressColorBoxUpdates", true);
        initializePresetSystem();
        loadPresetList();

        rebuildPaletteTopRow(ui, mAddColorButtonCosmic, mReplaceColorButtonCosmic, mRemoveColorButtonCosmic, mPresetComboBox, mPalettePrefButtonCosmic);

        mSuppressToolColorChangeDuringInit = false;
        qApp->setProperty("SuppressColorBoxUpdates", false);
    } catch (const std::exception& e) {
        qCritical() << "ColorPaletteWidget: Exception during preset initialization:" << e.what();
        mSuppressToolColorChangeDuringInit = false;
        qApp->setProperty("SuppressColorBoxUpdates", false);
    }

    // Final initialization step: ensure the tool's current drawing color is black
    // without modifying the palette. Use setFrontColor so we don't overwrite
    // any palette entries (setIndexedColor writes to the palette).
    // This maintains consistency with app startup behavior.
    if (mEditor && mEditor->color())
    {
        mEditor->color()->setFrontColor(QColor("#000000"));
    }
}

void ColorPaletteWidget::updateUI()
{
    if (!mEditor) {
        qWarning() << "ColorPaletteWidget::updateUI: mEditor is null";
        return;
    }
    mObject = mEditor->object();
    if (!mObject) {
        qWarning() << "ColorPaletteWidget::updateUI: mObject is null";
        return;
    }
    // If the object has no palette and this is a newly created object (no file path),
    // apply the default built-in preset so users start with Genga.
    if (mObject && mObject->getColorCount() == 0 && mObject->filePath().isEmpty())
    {
        ensureBuiltInPresetsExist();
        applyDefaultPreset();
    }

    refreshColorList();
    updateGridUI();

    // If this update was triggered for layout-only reasons (view mode or swatch size
    // changes), avoid changing the preset selection. Also avoid clearing while applying
    // a preset programmatically. Do NOT reset the preset selection here to prevent
    // the combo box from showing an empty label on startup or other external updates.
    // External callers (e.g., MainWindow2) can still call resetPresetSelection() when
    // they explicitly need to indicate the palette no longer matches a preset.
}

void ColorPaletteWidget::setCore(Editor* editor)
{
    mEditor = editor;
    mObject = mEditor->object();
}

void ColorPaletteWidget::addItem()
{
    QSignalBlocker b(ui->colorListWidget);
    if (!checkCanModifyCurrentPreset()) return;
    QColor newColor = mEditor->color()->frontColor(false);

    ColorRef ref(newColor);

    const int colorIndex = ui->colorListWidget->count();
    mObject->addColorAtIndex(colorIndex, ref);

    // Set active before refresh so the list lands on the new entry.
    mEditor->color()->setColorNumber(colorIndex);

    refreshColorList();

    if (mFitSwatches)
    {
        fitSwatchSize();
    }

    QListWidgetItem* item = ui->colorListWidget->item(colorIndex);
    if (item)
    {
        ui->colorListWidget->scrollToItem(item);
        ui->colorListWidget->editItem(item);
    }
    autoSaveCurrentPreset();
}

void ColorPaletteWidget::replaceItem()
{
    QSignalBlocker b(ui->colorListWidget);
    if (!checkCanModifyCurrentPreset()) return;
    int index = ui->colorListWidget->currentRow();

    QColor newColor = mEditor->color()->frontColor(false);

    if (index < 0 ) { return; }

    updateItemColor(index, newColor);
    emit colorNumberChanged(index);
    ui->colorListWidget->setCurrentRow(index);
    // Palette modified; auto-save to current preset if applicable
    autoSaveCurrentPreset();
}

void ColorPaletteWidget::removeItem()
{
    QSignalBlocker b(ui->colorListWidget);
    if (!checkCanModifyCurrentPreset()) return;
    clickRemoveColorButton();
}

void ColorPaletteWidget::selectColorNumber(int colorNumber) const
{
    // Guard against invalid selection when palette is empty
    if (!mObject) return;
    const int colorCount = mObject->getColorCount();
    if (colorCount == 0 || colorNumber < 0 || colorNumber >= colorCount)
    {
        ui->colorListWidget->clearSelection();
        ui->colorListWidget->setCurrentRow(-1);
        return;
    }
    ui->colorListWidget->setCurrentRow(colorNumber);
}

int ColorPaletteWidget::currentColorNumber()
{
    if (!mObject) return -1;
    if (mObject->getColorCount() == 0) return -1;
    int row = ui->colorListWidget->currentRow();
    if (row < 0) return -1;
    return row;
}

void ColorPaletteWidget::refreshColorList()
{
    if (!editor() || !editor()->object()) {
        qWarning() << "ColorPaletteWidget: Cannot refresh, editor or object is null";
        return;
    }
    QSignalBlocker b(ui->colorListWidget);
    if (ui->colorListWidget->count() > 0)
    {
        ui->colorListWidget->clear();
    }
    int colorCount = editor()->object()->getColorCount();

    for (int i = 0; i < colorCount; i++)
    {
        addSwatch(i);
    }

    // If palette empty, ensure no selection
    if (colorCount == 0)
    {
        ui->colorListWidget->clearSelection();
        ui->colorListWidget->setCurrentRow(-1);
    }
    else
    {
        selectColorNumber(editor()->color()->frontColorNumber());
    }
    updateGridUI();
    update();
}

void ColorPaletteWidget::addSwatch(int colorIndex) const
{
    const ColorRef colorRef = mObject->getColor(colorIndex);
    QListWidgetItem* colorItem = new QListWidgetItem(ui->colorListWidget);

    // Store the authoritative color in model data so the grid delegate can paint without per-item pixmaps.
    colorItem->setData(kColorRole, colorRef.color);

    if (ui->colorListWidget->viewMode() != QListView::IconMode)
    {
        colorItem->setText(colorRef.name);
    }
    else
    {
        colorItem->setToolTip(colorRef.name);
    }

    if (ui->colorListWidget->viewMode() != QListView::IconMode)
    {
        // List mode: keep the legacy icon behavior (small and acceptable memory-wise)
        QPixmap colorSwatch(mIconSize);
        QPainter swatchPainter(&colorSwatch);
        swatchPainter.drawTiledPixmap(0, 0, mIconSize.width(), mIconSize.height(), QPixmap(":/background/checkerboard.png"));
        swatchPainter.fillRect(0, 0, mIconSize.width(), mIconSize.height(), colorRef.color);
        swatchPainter.end();

        QIcon swatchIcon;
        swatchIcon.addPixmap(colorSwatch, QIcon::Normal);
        swatchIcon.addPixmap(colorSwatch, QIcon::Selected);
        colorItem->setIcon(swatchIcon);
    }

    colorItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);

    ui->colorListWidget->addItem(colorItem);
}

void ColorPaletteWidget::changeColorName(QListWidgetItem* item)
{
    Q_ASSERT(item != nullptr);

    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        int colorNumber = ui->colorListWidget->row(item);
        if (colorNumber > -1)
        {
            CosmicInputPopup popup(this);
            popup.setTitleText(tr("Rename Color"));
            popup.setPlaceholderText(tr("Color name"));
            popup.setInitialText(mObject->getColor(colorNumber).name);
            popup.setConfirmText(tr("Rename"));
            popup.exec();
            if (popup.result() == QDialog::Accepted && !popup.text().isEmpty())
            {
                if (!checkCanModifyCurrentPreset()) return;
                mObject->renameColor(colorNumber, popup.text());
                refreshColorList();
                autoSaveCurrentPreset();
            }
        }
    }
}

void ColorPaletteWidget::onItemChanged(QListWidgetItem* item)
{
    int index = ui->colorListWidget->row(item);
    QString newColorName = item->text();
    mObject->renameColor(index, newColorName);
}

void ColorPaletteWidget::onRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(destination)
    Q_UNUSED(end)

    if (!checkCanModifyCurrentPreset()) return;

    // QAbstractItemModel::rowsMoved gives us the source row (start) and the
    // destination row (row) after Qt has already reordered the list widget's
    // internal model. We just need to mirror that same move in the object data.
    // When moving forward Qt reports the destination one past the insertion point,
    // so adjust to get the actual landed index.
    int from = start;
    int to   = (start < row) ? row - 1 : row;

    if (from == to) return;

    // Mirror the move in the palette and vector layers.
    mObject->movePaletteColor(from, to);
    mObject->moveVectorColor(from, to);

    // Keep selection on the moved item (now at index `to`).
    mEditor->color()->setColorNumber(to);

    refreshColorList();
    autoSaveCurrentPreset();
}

void ColorPaletteWidget::clickColorListItem(QListWidgetItem* currentItem)
{
    int colorIndex = ui->colorListWidget->row(currentItem);
    emit colorNumberChanged(colorIndex);
}

void ColorPaletteWidget::palettePreferences()
{
    ui->colorListWidget->setMinimumWidth(ui->colorListWidget->sizeHintForColumn(0));

    // Wire up QAction signals (connections are UniqueConnection so safe to call multiple times)
    connect(ui->listModeAction,   &QAction::triggered, this, &ColorPaletteWidget::setListMode,        Qt::UniqueConnection);
    connect(ui->gridModeAction,   &QAction::triggered, this, &ColorPaletteWidget::setGridMode,        Qt::UniqueConnection);
    connect(ui->fitSwatchAction,  &QAction::triggered, this, &ColorPaletteWidget::fitSwatchSize,      Qt::UniqueConnection);
    connect(ui->smallSwatchAction,&QAction::triggered, this, &ColorPaletteWidget::setSwatchSizeSmall, Qt::UniqueConnection);
    connect(ui->mediumSwatchAction,&QAction::triggered,this, &ColorPaletteWidget::setSwatchSizeMedium,Qt::UniqueConnection);
    connect(ui->largeSwatchAction,&QAction::triggered, this, &ColorPaletteWidget::setSwatchSizeLarge, Qt::UniqueConnection);

    // Create preset actions once
    if (!mPresetSeparator)
    {
        mPresetSeparator = new QAction("", this);
        mPresetSeparator->setSeparator(true);

        mNewPresetAction = new QAction(tr("New Preset"), this);
        connect(mNewPresetAction, &QAction::triggered, this, &ColorPaletteWidget::newPreset);

        mRenamePresetAction = new QAction(tr("Rename Preset"), this);
        connect(mRenamePresetAction, &QAction::triggered, this, &ColorPaletteWidget::renameCurrentPreset);

        mImportPresetAction = new QAction(tr("Import Preset"), this);
        connect(mImportPresetAction, &QAction::triggered, this, &ColorPaletteWidget::importPresetFromFile);

        mExportPresetAction = new QAction(tr("Export Preset"), this);
        connect(mExportPresetAction, &QAction::triggered, this, &ColorPaletteWidget::exportPresetToFile);

        mDeletePresetAction = new QAction(tr("Delete Preset"), this);
        connect(mDeletePresetAction, &QAction::triggered, this, &ColorPaletteWidget::deleteCurrentPreset);
    }

    // Connect the prefs button to show a CosmicPopupContextMenu on click
    if (mPalettePrefButtonCosmic)
    {
        connect(mPalettePrefButtonCosmic, &QPushButton::clicked, this, &ColorPaletteWidget::showPrefsMenu, Qt::UniqueConnection);
    }
}

void ColorPaletteWidget::showPrefsMenu()
{
    const bool isListMode = (ui->colorListWidget->viewMode() == QListView::ListMode);
    const int iconW = mIconSize.width();

    CosmicPopupContextMenu menu(this);

    // View mode
    menu.addCheckableItem(tr("List Mode"), QStringLiteral("list"),   isListMode);
    menu.addCheckableItem(tr("Grid Mode"), QStringLiteral("grid"),  !isListMode);
    menu.addSeparator();

    // Swatch size
    menu.addCheckableItem(tr("Small"),  QStringLiteral("small"),  !mFitSwatches && iconW <= MIN_ICON_SIZE);
    menu.addCheckableItem(tr("Medium"), QStringLiteral("medium"), !mFitSwatches && iconW > MIN_ICON_SIZE && iconW <= MEDIUM_ICON_SIZE);
    menu.addCheckableItem(tr("Large"),  QStringLiteral("large"),  !mFitSwatches && iconW > MEDIUM_ICON_SIZE);
    menu.addCheckableItem(tr("Fit"),    QStringLiteral("fit"),     mFitSwatches);
    menu.addSeparator();

    // Preset operations
    menu.addItem(tr("New Preset"),    QStringLiteral("new_preset"));
    menu.addItem(tr("Rename Preset"), QStringLiteral("rename_preset"));
    menu.addItem(tr("Import Preset"), QStringLiteral("import_preset"));
    menu.addItem(tr("Export Preset"), QStringLiteral("export_preset"));
    menu.addItem(tr("Delete Preset"), QStringLiteral("delete_preset"));

    const QPoint globalPos = mPalettePrefButtonCosmic
        ? mPalettePrefButtonCosmic->mapToGlobal(mPalettePrefButtonCosmic->rect().bottomLeft())
        : QCursor::pos();

    const QString chosen = menu.execAt(globalPos);

    if      (chosen == QStringLiteral("list"))           setListMode();
    else if (chosen == QStringLiteral("grid"))           setGridMode();
    else if (chosen == QStringLiteral("small"))          setSwatchSizeSmall();
    else if (chosen == QStringLiteral("medium"))         setSwatchSizeMedium();
    else if (chosen == QStringLiteral("large"))          setSwatchSizeLarge();
    else if (chosen == QStringLiteral("fit"))            fitSwatchSize();
    else if (chosen == QStringLiteral("new_preset"))     newPreset();
    else if (chosen == QStringLiteral("rename_preset"))  renameCurrentPreset();
    else if (chosen == QStringLiteral("import_preset"))  importPresetFromFile();
    else if (chosen == QStringLiteral("export_preset"))  exportPresetToFile();
    else if (chosen == QStringLiteral("delete_preset"))  deleteCurrentPreset();
}

void ColorPaletteWidget::setListMode()
{
    if (mListItemDelegate)
    {
        ui->colorListWidget->setItemDelegate(mListItemDelegate);
    }
    ui->colorListWidget->setViewMode(QListView::ListMode);
    ui->colorListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->colorListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->colorListWidget->setGridSize(QSize(-1, -1));
    // Zero spacing so rows are flush with no gaps at any zoom level
    ui->colorListWidget->setSpacing(0);
    ui->colorListWidget->setContentsMargins(0, 0, 0, 0);
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    // This update is layout-only (view mode change) so avoid clearing preset selection
    mLayoutRefreshOnly = true;
    updateUI();
    mLayoutRefreshOnly = false;

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("ColorPaletteViewMode", "ListMode");
}

void ColorPaletteWidget::setGridMode()
{
    if (mGridItemDelegate)
    {
        ui->colorListWidget->setItemDelegate(mGridItemDelegate);
    }
    ui->colorListWidget->setViewMode(QListView::IconMode);
    ui->colorListWidget->setMovement(QListView::Static);
    ui->colorListWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    ui->colorListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->colorListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->colorListWidget->setSpacing(-1);
    ui->colorListWidget->setWrapping(true);
    ui->colorListWidget->setResizeMode(QListView::Adjust);
    ui->colorListWidget->setUniformItemSizes(true);
    ui->colorListWidget->setGridSize(QSize(mIconSize.width(), mIconSize.height()));
    ui->colorListWidget->setIconSize(QSize(mIconSize.width(), mIconSize.height()));
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    // This update is layout-only (view mode change) so avoid clearing preset selection
    mLayoutRefreshOnly = true;
    updateUI();
    mLayoutRefreshOnly = false;

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("ColorPaletteViewMode", "GridMode");
}

void ColorPaletteWidget::setSwatchSizeSmall()
{
    if (mIconSize.width() > MIN_ICON_SIZE)
    {
        mIconSize = QSize(MIN_ICON_SIZE, MIN_ICON_SIZE);
        // Swatch size change is layout-only; preserve preset selection
        mLayoutRefreshOnly = true;
        updateUI();
        mLayoutRefreshOnly = false;

        mFitSwatches = false;
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("PreferredColorGridSize", MIN_ICON_SIZE);
        settings.setValue("FitSwatchSize", false);
    }
}

void ColorPaletteWidget::setSwatchSizeMedium()
{
    if (mIconSize.width() != MEDIUM_ICON_SIZE)
    {
        mIconSize = QSize(MEDIUM_ICON_SIZE, MEDIUM_ICON_SIZE);
        // Swatch size change is layout-only; preserve preset selection
        mLayoutRefreshOnly = true;
        updateUI();
        mLayoutRefreshOnly = false;

        mFitSwatches = false;
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("PreferredColorGridSize", MEDIUM_ICON_SIZE);
        settings.setValue("FitSwatchSize", false);
    }
}

void ColorPaletteWidget::setSwatchSizeLarge()
{
    if (mIconSize.width() < MAX_ICON_SIZE)
    {
        mIconSize = QSize(MAX_ICON_SIZE, MAX_ICON_SIZE);
        // Swatch size change is layout-only; preserve preset selection
        mLayoutRefreshOnly = true;
        updateUI();
        mLayoutRefreshOnly = false;

        mFitSwatches = false;
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("PreferredColorGridSize", MAX_ICON_SIZE);
        settings.setValue("FitSwatchSize", false);
    }
}

void ColorPaletteWidget::adjustSwatches()
{
    if (mFitSwatches)
        fitSwatchSize();
}

void ColorPaletteWidget::fitSwatchSize()
{
    int height = ui->colorListWidget->height();
    int width = ui->colorListWidget->width();
    int hScrollBar = ui->colorListWidget->horizontalScrollBar()->geometry().height() + 6;
    int vScrollBar = ui->colorListWidget->verticalScrollBar()->geometry().width() * 2;
    int colorCount = editor()->object()->getColorCount();
    int size;

    // Guard against division by zero when palette is empty
    if (colorCount == 0) {
        size = MIN_ICON_SIZE;
        mIconSize = QSize(size, size);
        return;
    }

    if (ui->colorListWidget->viewMode() == QListView::ListMode)
    {
        size = qFloor((height - hScrollBar - (4 * colorCount)) / colorCount);
        if (size < MIN_ICON_SIZE) size = MIN_ICON_SIZE;
        if (size > MAX_ICON_SIZE) size = MAX_ICON_SIZE;
    }
    else
    {
        const int availableWidth = qMax(1, width - vScrollBar);
        const int columns = qMax(1, availableWidth / MIN_ICON_SIZE);
        size = availableWidth / columns;
        if (size < MIN_ICON_SIZE) size = MIN_ICON_SIZE;
        if (size > MAX_ICON_SIZE) size = MAX_ICON_SIZE;
    }
    mIconSize = QSize(size, size);

    // fitSwatchSize triggers a layout refresh; avoid clearing preset selection during it
    mLayoutRefreshOnly = true;
    updateUI();
    mLayoutRefreshOnly = false;

    mFitSwatches = true;
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("PreferredColorGridSize", size);
    settings.setValue("FitSwatchSize", true);
}

void ColorPaletteWidget::resizeEvent(QResizeEvent* event)
{
    // resize triggers a layout refresh; avoid clearing preset selection during it
    mLayoutRefreshOnly = true;
    updateUI();
    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    mLayoutRefreshOnly = false;
    QWidget::resizeEvent(event);
}

void ColorPaletteWidget::changeEvent(QEvent* event)
{
    BaseDockWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange) return;

    ui->retranslateUi(this);

    // Retranslate QAction texts — these are created once in palettePreferences()
    // and never updated otherwise.
    if (mNewPresetAction)    mNewPresetAction->setText(tr("New Preset"));
    if (mRenamePresetAction) mRenamePresetAction->setText(tr("Rename Preset"));
    if (mImportPresetAction) mImportPresetAction->setText(tr("Import Preset"));
    if (mExportPresetAction) mExportPresetAction->setText(tr("Export Preset"));
    if (mDeletePresetAction) mDeletePresetAction->setText(tr("Delete Preset"));
    // Note: showPrefsMenu() already calls tr() fresh on every open,
    // so the popup menu items are always translated correctly.
}

bool ColorPaletteWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->colorListWidget->viewport()
        && event->type() == QEvent::MouseButtonRelease
        && ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        auto* me = static_cast<QMouseEvent*>(event);
        QListWidgetItem* item = ui->colorListWidget->itemAt(me->pos());
        if (item)
        {
            ui->colorListWidget->setCurrentItem(item);
            emit colorNumberChanged(ui->colorListWidget->row(item));
        }
        return false; // let Qt finish its own handling too
    }
    return BaseDockWidget::eventFilter(obj, event);
}

void ColorPaletteWidget::updateGridUI()
{
    if (ui->colorListWidget->viewMode() == QListView::IconMode) {
        // spacing(-1) = Qt's "no spacing" sentinel for IconMode — setSpacing(0) still
        // leaves internal padding. gridSize must exactly match iconSize so tiles are flush.
        ui->colorListWidget->setSpacing(-1);
        ui->colorListWidget->setIconSize(QSize(mIconSize.width(), mIconSize.height()));
        ui->colorListWidget->setGridSize(QSize(mIconSize.width(), mIconSize.height()));
    }
    else
    {
        // List mode: no spacing so rows are flush regardless of zoom level
        ui->colorListWidget->setSpacing(0);
        ui->colorListWidget->setIconSize(mIconSize);
        ui->colorListWidget->setGridSize(QSize(-1, -1));
    }
}

void ColorPaletteWidget::clickColorDialogButton()
{
    mIsColorDialog = true;
    clickAddColorButton();
    mIsColorDialog = false;
}

void ColorPaletteWidget::clickAddColorButton()
{
    if (!checkCanModifyCurrentPreset()) return;
    QColor prevColor = Qt::white;

    QColor newColor;

    if (mIsColorDialog) {
        PopupColorPicker colorPicker(this);
        newColor = colorPicker.getColor(prevColor);
    } else {
        newColor = mEditor->color()->frontColor(false);
    }

    if (!newColor.isValid())
    {
        return; // User canceled operation
    }

    int colorIndex = mObject->getColorCount();
    ColorRef ref(newColor);

    mObject->addColor(ref);

    // Set the new index as active before refreshing so the list lands on it.
    editor()->color()->setColorNumber(colorIndex);
    editor()->color()->setIndexedColor(ref.color);

    refreshColorList();

    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    // Palette modified; auto-save to current preset if applicable
    autoSaveCurrentPreset();
}

void ColorPaletteWidget::clickRemoveColorButton()
{
    if (!checkCanModifyCurrentPreset()) return;
    for (auto item : ui->colorListWidget->selectedItems())
    {
        int index = ui->colorListWidget->row(item);

        // items are not deleted by qt, it has to be done manually
        // delete should happen before removing the color from from palette
        // as the palette will be one ahead and crash otherwise

        // Allow removing all colors (empty palettes are supported)
        bool colorRemoved = false;
        if (mObject->isColorInUse(index))
        {
            bool accepted = false;
            if (!mMultipleSelected)
                accepted = showPaletteWarning();

            if ((accepted || mMultipleSelected))
            {
                delete item;
                mObject->removeColor(index);
                colorRemoved = true;
            }
        }
        else
        {
            delete item;
            mObject->removeColor(index);
            colorRemoved = true;
        }

        if (colorRemoved) {
            // Handle color selection after removal
            if (mObject->getColorCount() == 0)
            {
                // Palette is now empty, clear selection instead of emitting -1
                mEditor->color()->clearSelection();
                ui->colorListWidget->clearSelection();
            }
            else
            {
                // Select previous entry, or first if we removed index 0.
                int newIndex = qBound(0, index - 1, mObject->getColorCount() - 1);
                // Update color manager first so refreshColorList lands on the right row.
                mEditor->color()->setColorNumber(newIndex);
                emit colorNumberChanged(newIndex);
            }
        }
    }
    mMultipleSelected = false;

    // Always rebuild the list to reflect the removal.
    refreshColorList();

    if (mFitSwatches)
    {
        fitSwatchSize();
    }
    mEditor->updateFrame();
    // Palette modified; auto-save to current preset if applicable
    autoSaveCurrentPreset();
}

bool ColorPaletteWidget::showPaletteWarning()
{
    CosmicConfirmPopup popup(this);
    popup.setTitleText(tr("Delete Color"));
    popup.setBodyText(tr("The color(s) you are about to delete are currently being used by one or multiple strokes."));
    popup.setConfirmText(tr("Delete"));
    popup.setCancelText(tr("Cancel"));
    popup.exec();
    if (popup.result() == QDialog::Accepted)
    {
        if (ui->colorListWidget->selectedItems().size() > 1)
        {
            mMultipleSelected = true;
        }
        return true;
    }
    return false;
}

void ColorPaletteWidget::showPaletteReminder()
{
    CosmicWarningPopup popup(this);
    popup.setTitleText(tr("Palette Restriction"));
    popup.setBodyText(tr("The palette requires at least one swatch to remain functional"));
    popup.exec();
}

void ColorPaletteWidget::updateItemColor(int itemIndex, QColor newColor)
{
    QListWidgetItem* item = ui->colorListWidget->item(itemIndex);
    if (!item) return;

    item->setData(kColorRole, newColor);

    if (ui->colorListWidget->viewMode() != QListView::IconMode)
    {
        QPixmap colorSwatch(mIconSize);
        QPainter swatchPainter(&colorSwatch);
        swatchPainter.drawTiledPixmap(0, 0, mIconSize.width(), mIconSize.height(), QPixmap(":/background/checkerboard.png"));
        swatchPainter.fillRect(0, 0, mIconSize.width(), mIconSize.height(), newColor);
        swatchPainter.end();

        QIcon swatchIcon;
        swatchIcon.addPixmap(colorSwatch, QIcon::Normal);
        swatchIcon.addPixmap(colorSwatch, QIcon::Selected);
        item->setIcon(swatchIcon);
    }
    editor()->object()->setColor(itemIndex, newColor);

    // Make sure to update grid in grid mode
    if (ui->colorListWidget->viewMode() == QListView::IconMode)
    {
        updateGridUI();
    }
}

// Preset system implementation
void ColorPaletteWidget::initializePresetSystem()
{
    try {
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
        const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
        const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#endif
        if (baseDir.isEmpty()) {
            qCritical() << "ColorPaletteWidget: Cannot determine app data location";
            return;
        }
        QDir dir(baseDir);
        if (!dir.mkpath("color_palettes")) {
            qCritical() << "ColorPaletteWidget: Failed to create color_palettes directory";
            return;
        }
        if (!dir.cd("color_palettes")) {
            qCritical() << "ColorPaletteWidget: Failed to access color_palettes directory";
            return;
        }
        mPresetsDirectory = dir.absolutePath();
        mPresetsMetadataFile = dir.absoluteFilePath("presets.ini");

        if (!QFile::exists(mPresetsMetadataFile))
        {
            QSettings s(mPresetsMetadataFile, QSettings::IniFormat);
            s.beginGroup("Presets");
            s.endGroup();
            s.sync();
        }

        if (!mPresetComboBox)
        {
            // Replace UI combo with CosmicDropDown and keep it in the top toolbar row
            if (auto uiCombo = findChild<QComboBox*>(QStringLiteral("presetComboBox"))) {
                mPresetComboBox = new CosmicDropDown(uiCombo->parentWidget());
                // Prefer inserting into the known top row layout from the UI to avoid
                // accidentally appending to the outer vertical layout (which would place
                // the dropdown at the bottom of the dock).
                if (ui && ui->horizontalLayout) {
                    int idx = ui->horizontalLayout->indexOf(uiCombo);
                    if (idx < 0) {
                        // Fallback to right before the preferences button if the combo can't be found
                        int prefIdx = ui->horizontalLayout->indexOf(ui->palettePref);
                        idx = (prefIdx >= 0) ? prefIdx : ui->horizontalLayout->count();
                    }
                    ui->horizontalLayout->insertWidget(idx, mPresetComboBox);
                    uiCombo->hide();
                } else if (auto layoutHost = uiCombo->parentWidget()->layout()) {
                    // Secondary fallback: insert next to the original combo if possible
                    if (auto row = qobject_cast<QHBoxLayout*>(layoutHost)) {
                        int idx = row->indexOf(uiCombo);
                        if (idx < 0) idx = row->count();
                        row->insertWidget(idx, mPresetComboBox);
                    } else {
                        // Last resort: add to the top by inserting at index 0 when possible
                        layoutHost->addWidget(mPresetComboBox);
                    }
                    uiCombo->hide();
                }
            }
        }

        // Built-in presets are in-memory — no disk setup needed for them
        // (ensureBuiltInPresetsExist is now a no-op kept for API compatibility)
    } catch (const std::exception& e) {
        qCritical() << "ColorPaletteWidget: Exception in initializePresetSystem:" << e.what();
    }
}

void ColorPaletteWidget::loadPresetList()
{
    if (!mPresetComboBox) {
        qWarning() << "ColorPaletteWidget: mPresetComboBox is null, cannot load presets";
        return;
    }
    if (mPresetsMetadataFile.isEmpty()) {
        qWarning() << "ColorPaletteWidget: Preset system not initialized";
        return;
    }
    // Block signals while we rebuild the combo box
    {
        QSignalBlocker blocker(mPresetComboBox);
        QStringList options;
        const auto meta = loadPresetMetadata();
        // Built-in presets are always present — no disk file needed
        options << BUILTIN_PRESET_GENGA;
        options << BUILTIN_PRESET_GENGA_FLUSHED;
        for (auto it = meta.constBegin(); it != meta.constEnd(); ++it) {
            if (isBuiltInPreset(it.key())) continue;
            options << it.key();
        }
        // Populate dropdown
        mPresetComboBox->setOptions(options);

        // Determine which preset should be selected after loading the list.
        QSettings settings(PENCIL2D, PENCIL2D);
        const QString lastActive = settings.value("LastActivePreset", QString()).toString();
        int selectedIdx = options.indexOf(lastActive);
        if (selectedIdx < 0 && !options.isEmpty()) selectedIdx = 0;
        if (selectedIdx >= 0) mPresetComboBox->setCurrentIndex(selectedIdx);
    }
    // Now that signals are unblocked (blocker scope ended), explicitly apply
    // the selected preset so the palette matches the combo box on startup.
    QSettings settings(PENCIL2D, PENCIL2D);
    const QString lastActive = settings.value("LastActivePreset", QString()).toString();
    int selectedIdx = mPresetComboBox->currentIndex();
    if (selectedIdx < 0 && !lastActive.isEmpty()) {
        // Attempt to find by text from options
        selectedIdx = lastActive.isEmpty() ? -1 : selectedIdx;
    }
    if (selectedIdx >= 0)
    {
        const QString selName = mPresetComboBox->currentText();
        const QString selPath = getPresetFilePath(selName);
        if (!selPath.isEmpty())
            applyPreset(selPath);
    }
    // Connect the preset combo box signal only once
    if (!mPresetComboConnected)
    {
        connect(mPresetComboBox, &CosmicDropDown::currentIndexChanged, this, &ColorPaletteWidget::onPresetSelected);
        mPresetComboConnected = true;
    }
}

QMap<QString, QString> ColorPaletteWidget::loadPresetMetadata()
{
    QMap<QString, QString> map;
    QSettings s(mPresetsMetadataFile, QSettings::IniFormat);
    s.beginGroup("Presets");
    const QStringList keys = s.childKeys();
    for (const QString& k : keys)
    {
        map.insert(k, s.value(k).toString());
    }
    s.endGroup();
    return map;
}

bool ColorPaletteWidget::isPresetNameDuplicate(const QString& presetName, const QString& excludeName)
{
    const auto metadata = loadPresetMetadata();
    for (const QString& existingName : metadata.keys())
    {
        if (!excludeName.isEmpty() && existingName == excludeName) continue;
        if (existingName.compare(presetName, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}

bool ColorPaletteWidget::isBuiltInPreset(const QString& presetName) const
{
    if (presetName.isEmpty()) return false;
    if (presetName.compare(BUILTIN_PRESET_GENGA, Qt::CaseInsensitive) == 0) return true;
    if (presetName.compare(BUILTIN_PRESET_GENGA_FLUSHED, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// Check whether the currently selected preset can be modified by the user.
// Returns true if modification is allowed, false otherwise. If a built-in preset
// is selected, shows a warning dialog and returns false.
bool ColorPaletteWidget::checkCanModifyCurrentPreset()
{
    if (!mPresetComboBox) return true; // No preset UI present, allow modifications
    const int idx = mPresetComboBox->currentIndex();
    if (idx < 0) return true; // No preset selected, allow modifications

    QString presetName;
    const QStringList opts = mPresetComboBox->options();
    if (idx >= 0 && idx < opts.size()) presetName = opts[idx];
    if (isBuiltInPreset(presetName))
    {
        CosmicWarningPopup popup(this);
        popup.setTitleText(tr("Preset Modification"));
        popup.setBodyText(tr("Cannot modify built-in preset '%1'. Please create a new preset or select a custom preset first.").arg(presetName));
        popup.exec();
        return false;
    }
    return true;
}

// Automatically save the current palette to the currently selected preset
// unless the preset is built-in. This will keep the preset selection intact.
void ColorPaletteWidget::autoSaveCurrentPreset()
{
    if (!mPresetComboBox || !mEditor || !mObject) return;
    const int idx = mPresetComboBox->currentIndex();
    if (idx < 0) return; // No preset selected

    QString presetName;
    {
        const QStringList opts = mPresetComboBox->options();
        if (idx >= 0 && idx < opts.size()) presetName = opts[idx].trimmed();
    }
    if (presetName.isEmpty()) return;
    if (isBuiltInPreset(presetName)) return; // Do not auto-save built-ins

    // Save without refreshing the UI — the combo box is already showing the
    // correct preset and calling loadPresetList() would trigger applyPreset()
    // which resets the color selection to index 0.
    QSignalBlocker blocker(mPresetComboBox);
    savePresetWithName(presetName, false);
}

// Helper to save the current palette using an explicit name. Returns true on success.
bool ColorPaletteWidget::savePresetWithName(const QString& presetName, bool refreshUi)
{
    if (!mEditor || !mObject) return false;
    const QString safeName = presetName.trimmed();
    if (safeName.isEmpty()) return false;

    // If a preset exists with this name, remove its metadata first to avoid stale entries
    if (isPresetNameDuplicate(safeName))
    {
        // capture old file name to allow recovery if needed
        const auto metaBefore = loadPresetMetadata();
        const QString oldFileName = metaBefore.value(safeName, QString());

        if (!removePresetMetadata(safeName))
        {
            return false;
        }

        const QString oldFilePath = QDir(mPresetsDirectory).absoluteFilePath(oldFileName);
        if (!oldFileName.isEmpty() && QFile::exists(oldFilePath))
        {
            QFile::remove(oldFilePath);
        }
    }

    const QString fileName = QString("preset_%1.xml").arg(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    const QString fullPath = QDir(mPresetsDirectory).absoluteFilePath(fileName);
    mObject->exportPalette(fullPath);

    if (!savePresetMetadata(safeName, fileName))
    {
        // If metadata save failed, try to remove the newly created file
        QFile::remove(fullPath);
        return false;
    }

    if (refreshUi)
    {
        loadPresetList();
        if (mPresetComboBox)
        {
            int idx = mPresetComboBox->options().indexOf(safeName);
            if (idx >= 0) mPresetComboBox->setCurrentIndex(idx);
        }
    }

    // Persist last active preset so it can be restored on app restart
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("LastActivePreset", safeName);
    return true;
}

void ColorPaletteWidget::createBuiltInPresetFile(const QString& /*presetName*/, const QStringList& /*hexColors*/)
{
    // Built-in presets are now purely in-memory — no files are written to disk.
    // This function is intentionally a no-op.
}

void ColorPaletteWidget::ensureBuiltInPresetsExist()
{
    // Built-in presets are always available in memory; nothing to create on disk.
}

void ColorPaletteWidget::applyDefaultPreset()
{
    // Apply Genga directly from in-memory color data — no file needed
    mIsApplyingPreset = true;
    mObject->clearPalette();
    for (const QString& hex : GENGA_COLORS)
    {
        QColor c;
        c.setNamedColor("#" + hex);
        if (c.isValid()) mObject->addColor(ColorRef(c, QString()));
    }
    refreshColorList();
    if (mObject->getColorCount() > 0)
    {
        mEditor->color()->setColorNumber(0);
        mEditor->color()->setFrontColor(QColor("#000000"));
    }
    mIsApplyingPreset = false;
    if (mPresetComboBox)
    {
        int idx = mPresetComboBox->options().indexOf(BUILTIN_PRESET_GENGA);
        if (idx >= 0) mPresetComboBox->setCurrentIndex(idx);
    }
}

void ColorPaletteWidget::onPresetSelected(int index)
{
    if (!mPresetComboBox) return;
    if (index < 0) return; // Only skip if no selection (-1)
    QString presetName;
    {
        const QStringList opts = mPresetComboBox->options();
        if (index >= 0 && index < opts.size()) presetName = opts[index];
    }
    const QString filePath = getPresetFilePath(presetName);
    // Persist the active preset selection so it can be restored on app startup
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("LastActivePreset", presetName);
    if (!filePath.isEmpty())
    {
        applyPreset(filePath);
    }
}

void ColorPaletteWidget::applyPreset(const QString& presetFilePath)
{
    if (!mEditor || !mObject) return;

    // Built-in presets are loaded from in-memory color data, not from disk
    if (presetFilePath == QStringLiteral("builtin:"))
    {
        // Determine which built-in is selected
        QString presetName;
        if (mPresetComboBox)
        {
            const int idx = mPresetComboBox->currentIndex();
            const QStringList opts = mPresetComboBox->options();
            if (idx >= 0 && idx < opts.size()) presetName = opts[idx];
        }
        const QStringList* colors = nullptr;
        if (presetName.compare(BUILTIN_PRESET_GENGA, Qt::CaseInsensitive) == 0)
            colors = &GENGA_COLORS;
        else if (presetName.compare(BUILTIN_PRESET_GENGA_FLUSHED, Qt::CaseInsensitive) == 0)
            colors = &GENGA_FLUSHED_COLORS;
        if (!colors) return;

        mIsApplyingPreset = true;
        mObject->clearPalette();
        for (const QString& hex : *colors)
        {
            QColor c;
            c.setNamedColor("#" + hex);
            if (c.isValid()) mObject->addColor(ColorRef(c, QString()));
        }
        refreshColorList();
        if (mObject->getColorCount() > 0 && !mSuppressToolColorChangeDuringInit)
            mEditor->color()->setColorNumber(0);
        mIsApplyingPreset = false;
        return;
    }

    // Mark that we're applying a preset so refreshes don't reset the combo box
    mIsApplyingPreset = true;

    for (int i = 0; i < mObject->getColorCount(); ++i)
    {
        if (!mObject->isColorInUse(i))
        {
            continue;
        }
        CosmicConfirmPopup applyConfirm(this);
        applyConfirm.setTitleText(tr("Open Palette"));
        applyConfirm.setBodyText(tr("Opening a palette will replace the old palette. Color(s) in strokes will be altered by this action!"));
        applyConfirm.setConfirmText(tr("Open Palette"));
        applyConfirm.exec();
        if (applyConfirm.result() != QDialog::Accepted)
        {
            // Ensure we reset the applying state when the user cancels
            mIsApplyingPreset = false;
            return;
        }
        break;
    }

    mObject->openPalette(presetFilePath);
    refreshColorList();
    // Guard against empty palettes. During initialization we may suppress
    // changing the tool's color; respect that flag here.
    if (mObject->getColorCount() > 0)
    {
        if (!mSuppressToolColorChangeDuringInit) {
            mEditor->color()->setColorNumber(0);
            // Set the front color to black for consistency with app startup behavior
            // This ensures drawing uses black color while the palette is available for selection
            mEditor->color()->setFrontColor(QColor("#000000"));
        }
    }
    else
    {
        if (!mSuppressToolColorChangeDuringInit)
            mEditor->color()->clearSelection();
    }

    // Finished applying preset
    mIsApplyingPreset = false;

    // If we are suppressing tool color changes during initialization, do not
    // modify the tool's current color here. Otherwise, set the tool color to
    // the first palette color so the tool follows the palette selection.
    if (mEditor && mEditor->color())
    {
        if (!mSuppressToolColorChangeDuringInit)
        {
            if (mObject->getColorCount() > 0)
            {
                QColor first = mObject->getColor(0).color;
                mEditor->color()->setIndexedColor(first);
            }
        }
    }
}

void ColorPaletteWidget::saveCurrentPaletteAsPreset()
{
    if (!mEditor || !mObject) return;
    CosmicInputPopup popup(this);
    popup.setTitleText(tr("Save as Preset"));
    popup.setPlaceholderText(tr("Preset name"));
    popup.setConfirmText(tr("Save"));
    popup.exec();
    if (popup.result() != QDialog::Accepted || popup.text().isEmpty()) return;
    savePresetWithName(popup.text());
}

void ColorPaletteWidget::importPresetFromFile()
{
    const QString src = FileDialog::getOpenFileName(this, FileType::PALETTE);
    if (src.isEmpty()) return;
    CosmicInputPopup importNamePopup(this);
    importNamePopup.setTitleText(tr("Import Preset"));
    importNamePopup.setPlaceholderText(tr("Preset name"));
    importNamePopup.setInitialText(QFileInfo(src).baseName());
    importNamePopup.setConfirmText(tr("Import"));
    importNamePopup.exec();
    if (importNamePopup.result() != QDialog::Accepted || importNamePopup.text().isEmpty()) return;
    QString safeName = importNamePopup.text();

    // Prevent overwriting built-in presets
    if (isBuiltInPreset(safeName))
    {
        CosmicWarningPopup popup(this);
        popup.setTitleText(tr("Import Preset"));
        popup.setBodyText(tr("Cannot overwrite built-in preset '%1'. Please choose a different name.").arg(safeName));
        popup.exec();
        return;
    }

    // If duplicate exists, ask to overwrite. If yes, remove metadata first, then delete file.
    QString oldFileName;
    if (isPresetNameDuplicate(safeName))
    {
        CosmicConfirmPopup dupPopup(this);
        dupPopup.setTitleText(tr("Duplicate Preset Name"));
        dupPopup.setBodyText(tr("A preset with the name '%1' already exists. Do you want to overwrite it?").arg(safeName));
        dupPopup.setConfirmText(tr("Overwrite"));
        dupPopup.exec();
        if (dupPopup.result() != QDialog::Accepted)
        {
            return;
        }

        // capture old filename to allow recovery on failure
        const auto metaBefore = loadPresetMetadata();
        oldFileName = metaBefore.value(safeName, QString());

        // Remove metadata first to avoid stale entries on failure
        if (!removePresetMetadata(safeName))
        {
            CosmicWarningPopup errPopup(this);
            errPopup.setTitleText(tr("Import Preset"));
            errPopup.setBodyText(tr("Failed to remove existing preset metadata."));
            errPopup.exec();
            return;
        }

        const QString oldFilePath = QDir(mPresetsDirectory).absoluteFilePath(oldFileName);
        if (!oldFileName.isEmpty() && QFile::exists(oldFilePath))
        {
            QFile::remove(oldFilePath);
        }
    }

    const QString suffix = QFileInfo(src).suffix();
    const QString fileName = QString("preset_%1.%2").arg(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()).arg(suffix.isEmpty() ? QString("xml") : suffix);
    const QString dst = QDir(mPresetsDirectory).absoluteFilePath(fileName);
    QFile::remove(dst);
    if (!QFile::copy(src, dst))
    {
        // Attempt to restore metadata if we removed it earlier
        if (!oldFileName.isEmpty())
        {
            savePresetMetadata(safeName, oldFileName);
        }
        { CosmicWarningPopup p(this); p.setTitleText(tr("Import Preset")); p.setBodyText(tr("Failed to copy preset file.")); p.exec(); }
        return;
    }

    if (!savePresetMetadata(safeName, fileName))
    {
        // Rollback copied file on metadata failure and attempt to restore old metadata
        QFile::remove(dst);
        if (!oldFileName.isEmpty())
        {
            savePresetMetadata(safeName, oldFileName);
        }
        { CosmicWarningPopup p(this); p.setTitleText(tr("Import Preset")); p.setBodyText(tr("Failed to save preset metadata.")); p.exec(); }
        return;
    }

    loadPresetList();
    // After import, select the newly imported preset without triggering applyPreset
    if (mPresetComboBox)
    {
        QSignalBlocker blocker(mPresetComboBox);
        const QStringList opts = mPresetComboBox->options();
        const int idx = opts.indexOf(safeName);
        if (idx >= 0) mPresetComboBox->setCurrentIndex(idx);
    }
    { CosmicWarningPopup p(this); p.setTitleText(tr("Import Preset")); p.setBodyText(tr("Preset imported successfully.")); p.setWarningIconEnabled(false); p.exec(); }
}

void ColorPaletteWidget::exportPresetToFile()
{
    QString targetPath = FileDialog::getSaveFileName(this, FileType::PALETTE);
    if (targetPath.isEmpty()) return;

    // Ensure the file has an extension — the save dialog should handle this
    // but guard against edge cases.
    if (QFileInfo(targetPath).suffix().isEmpty())
        targetPath += QStringLiteral(".xml");

    if (!mPresetComboBox || mPresetComboBox->currentIndex() < 0)
    {
        // No preset selected — export the live palette directly
        if (mEditor && mObject)
        {
            if (mObject->exportPalette(targetPath))
            {
                CosmicWarningPopup p(this);
                p.setTitleText(tr("Export Preset"));
                p.setBodyText(tr("Palette exported to:\n%1").arg(targetPath));
                p.setWarningIconEnabled(false);
                p.exec();
            }
            else
            {
                CosmicWarningPopup p(this);
                p.setTitleText(tr("Export Preset"));
                p.setBodyText(tr("Failed to export palette."));
                p.exec();
            }
        }
        return;
    }

    const QString presetName = mPresetComboBox->currentText();
    const QString src = getPresetFilePath(presetName);
    if (src.isEmpty())
    {
        CosmicWarningPopup p(this);
        p.setTitleText(tr("Export Preset"));
        p.setBodyText(tr("Could not find the preset file for '%1'.").arg(presetName));
        p.exec();
        return;
    }

    // Copy the preset file to the chosen location — original stays untouched.
    QFile::remove(targetPath);
    if (!QFile::copy(src, targetPath))
    {
        CosmicWarningPopup p(this);
        p.setTitleText(tr("Export Preset"));
        p.setBodyText(tr("Failed to write file:\n%1").arg(targetPath));
        p.exec();
        return;
    }

    CosmicWarningPopup p(this);
    p.setTitleText(tr("Export Preset"));
    p.setBodyText(tr("Preset exported to:\n%1").arg(targetPath));
    p.setWarningIconEnabled(false);
    p.exec();
}

void ColorPaletteWidget::deleteCurrentPreset()
{
    if (!mPresetComboBox || mPresetComboBox->currentIndex() < 0)
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("Delete Preset")); p.setBodyText(tr("Please select a preset to delete.")); p.exec();
        return;
    }
    const QString name = mPresetComboBox->currentText();
    if (isBuiltInPreset(name))
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("Delete Preset")); p.setBodyText(tr("Cannot delete built-in preset '%1'.").arg(name)); p.exec();
        return;
    }
    const QString path = getPresetFilePath(name);
    if (name.isEmpty() || path.isEmpty()) return;
    {
        CosmicConfirmPopup p(this);
        p.setTitleText(tr("Delete Preset"));
        p.setBodyText(tr("Are you sure you want to delete preset '%1'?").arg(name));
        p.setConfirmText(tr("Delete"));
        p.exec();
        if (p.result() != QDialog::Accepted) return;
    }
    QFile::remove(path);
    removePresetMetadata(name);
    loadPresetList();
    // After deletion, fallback to Genga built-in preset if available, otherwise first entry
    if (mPresetComboBox)
    {
        int idx = mPresetComboBox->options().indexOf(BUILTIN_PRESET_GENGA);
        if (idx >= 0)
        {
            mPresetComboBox->setCurrentIndex(idx);
            const QString filePath = getPresetFilePath(BUILTIN_PRESET_GENGA);
            if (!filePath.isEmpty()) applyPreset(filePath);
        }
        else if (!mPresetComboBox->options().isEmpty())
        {
            // Select first available preset
            mPresetComboBox->setCurrentIndex(0);
            const QString presetName = mPresetComboBox->currentText();
            const QString filePath = getPresetFilePath(presetName);
            if (!filePath.isEmpty()) applyPreset(filePath);
        }
    }
}

void ColorPaletteWidget::renameCurrentPreset()
{
    if (!mPresetComboBox || mPresetComboBox->currentIndex() < 0)
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Please select a preset to rename.")); p.exec();
        return;
    }

    const QString oldName = mPresetComboBox->currentText();
    if (isBuiltInPreset(oldName))
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Cannot rename built-in preset '%1'.").arg(oldName)); p.exec();
        return;
    }
    const QString filePath = getPresetFilePath(oldName);

    if (oldName.isEmpty() || filePath.isEmpty())
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Could not find the selected preset.")); p.exec();
        return;
    }

    // Prompt for new name
    CosmicInputPopup renamePopup(this);
    renamePopup.setTitleText(tr("Rename Preset"));
    renamePopup.setPlaceholderText(tr("New preset name"));
    renamePopup.setInitialText(oldName);
    renamePopup.setConfirmText(tr("Rename"));
    renamePopup.exec();
    if (renamePopup.result() != QDialog::Accepted || renamePopup.text().isEmpty()) return;

    QString safeNewName = renamePopup.text();

    // Check if the new name is the same as the old name (case-insensitive)
    if (safeNewName.compare(oldName, Qt::CaseInsensitive) == 0)
    {
        return; // No change needed
    }

    // Check for duplicate names (excluding the current preset being renamed)
    if (isPresetNameDuplicate(safeNewName, oldName))
    {
        CosmicConfirmPopup dupPopup(this);
        dupPopup.setTitleText(tr("Duplicate Preset Name"));
        dupPopup.setBodyText(tr("A preset with the name '%1' already exists. Do you want to overwrite it?").arg(safeNewName));
        dupPopup.setConfirmText(tr("Overwrite"));
        dupPopup.exec();
        if (dupPopup.result() != QDialog::Accepted)
        {
            return; // User chose not to overwrite
        }

        // If overwriting, remove metadata first and then delete the file. Check results to avoid inconsistencies.
        if (!removePresetMetadata(safeNewName))
        {
            CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Failed to remove existing preset metadata.")); p.exec();
            return;
        }

        const QString oldFilePath = getPresetFilePath(safeNewName);
        if (!oldFilePath.isEmpty())
        {
            if (!QFile::remove(oldFilePath))
            {
                // Attempt to restore metadata if we failed to remove the file
                savePresetMetadata(safeNewName, filePath);
                CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Failed to remove existing preset file.")); p.exec();
                return;
            }
        }
    }

    // Get the file name associated with the old preset
    const auto metadata = loadPresetMetadata();
    const QString fileName = metadata.value(oldName);

    if (fileName.isEmpty())
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Could not find preset file information.")); p.exec();
        return;
    }

    // Remove old metadata entry and add new one with the same file name
    removePresetMetadata(oldName);
    savePresetMetadata(safeNewName, fileName);

    // Refresh the preset list and select the renamed preset
    loadPresetList();

    if (mPresetComboBox)
    {
        int idx = mPresetComboBox->options().indexOf(safeNewName);
        if (idx >= 0) mPresetComboBox->setCurrentIndex(idx);
    }

    { CosmicWarningPopup p(this); p.setTitleText(tr("Rename Preset")); p.setBodyText(tr("Preset renamed successfully.")); p.setWarningIconEnabled(false); p.exec(); }
}

void ColorPaletteWidget::newPreset()
{
    if (!mEditor || !mObject) return;

    CosmicInputPopup newPresetPopup(this);
    newPresetPopup.setTitleText(tr("New Preset"));
    newPresetPopup.setPlaceholderText(tr("Preset name"));
    newPresetPopup.setConfirmText(tr("Create"));
    newPresetPopup.exec();
    if (newPresetPopup.result() != QDialog::Accepted || newPresetPopup.text().isEmpty()) return;

    QString safeName = newPresetPopup.text();

    // Check if name is a built-in preset
    if (isBuiltInPreset(safeName))
    {
        CosmicWarningPopup p(this); p.setTitleText(tr("New Preset")); p.setBodyText(tr("Cannot use built-in preset name '%1'. Please choose a different name.").arg(safeName)); p.exec();
        return;
    }

    // Check for duplicate names
    if (isPresetNameDuplicate(safeName))
    {
        CosmicConfirmPopup dupPopup(this);
        dupPopup.setTitleText(tr("Duplicate Preset Name"));
        dupPopup.setBodyText(tr("A preset with the name '%1' already exists. Do you want to overwrite it?").arg(safeName));
        dupPopup.setConfirmText(tr("Overwrite"));
        dupPopup.exec();
        if (dupPopup.result() != QDialog::Accepted)
        {
            return;
        }
    }

    // Clear the palette
    mObject->clearPalette();
    refreshColorList();

    // Clear color selection for empty palette
    if (mEditor && mEditor->color())
    {
        mEditor->color()->clearSelection();
    }

    // Save as new preset (this will also select it in the combo box). Use
    // refreshUi=false so we don't reload the combo box immediately; instead
    // we add the entry and select it to allow immediate interaction.
    if (savePresetWithName(safeName, false))
    {
        // Ensure the new preset is in the combo box and select it without
        // triggering onPresetSelected.
        if (mPresetComboBox)
        {
            QSignalBlocker blocker(mPresetComboBox);
            // Append the new preset if not present
            QStringList opts = mPresetComboBox->options();
            if (!opts.contains(safeName)) {
                opts << safeName;
                mPresetComboBox->setOptions(opts);
            }
            int idx = mPresetComboBox->options().indexOf(safeName);
            if (idx >= 0) mPresetComboBox->setCurrentIndex(idx);
        }

        { CosmicWarningPopup p(this); p.setTitleText(tr("New Preset")); p.setBodyText(tr("New empty preset '%1' created. Add colors using the + button.").arg(safeName)); p.setWarningIconEnabled(false); p.exec(); }
    }
}

QString ColorPaletteWidget::getPresetFilePath(const QString& presetName)
{
    // Built-in presets live in memory — return a sentinel so callers
    // can route to applyBuiltInPresetByName() instead of a file path.
    if (isBuiltInPreset(presetName)) return QStringLiteral("builtin:");
    const auto meta = loadPresetMetadata();
    const QString fileName = meta.value(presetName);
    if (fileName.isEmpty()) return QString();
    return QDir(mPresetsDirectory).absoluteFilePath(fileName);
}

bool ColorPaletteWidget::savePresetMetadata(const QString& presetName, const QString& fileName)
{
    if (presetName.trimmed().isEmpty() || fileName.trimmed().isEmpty()) return false;
    QSettings s(mPresetsMetadataFile, QSettings::IniFormat);
    s.beginGroup("Presets");
    // Remove any existing key matching case-insensitively first
    const QStringList keys = s.childKeys();
    for (const QString& k : keys)
    {
        if (k.compare(presetName, Qt::CaseInsensitive) == 0)
        {
            s.remove(k);
            break;
        }
    }
    // Save using the provided casing but ensure lookups are case-insensitive elsewhere
    s.setValue(presetName, fileName);
    s.endGroup();
    s.sync();
    return true;
}

bool ColorPaletteWidget::removePresetMetadata(const QString& presetName)
{
    if (presetName.isEmpty()) return false;
    QSettings s(mPresetsMetadataFile, QSettings::IniFormat);
    s.beginGroup("Presets");
    const QStringList keys = s.childKeys();
    // Remove with case-insensitive matching by normalizing keys
    for (const QString& k : keys)
    {
        if (k.compare(presetName, Qt::CaseInsensitive) == 0)
        {
            s.remove(k);
            s.endGroup();
            s.sync();
            return true;
        }
    }
    s.endGroup();
    s.sync();
    return false;
}

void ColorPaletteWidget::resetPresetSelection()
{
    if (!mPresetComboBox) return;
    QSignalBlocker b(mPresetComboBox);
    // Clear selection to indicate palette has been modified and doesn't match any preset
    mPresetComboBox->setCurrentIndex(-1);
}

void ColorPaletteWidget::updateHighlightColor()
{
    QColor accentColor = BlenderStyleSlider::defaultSliderColor();
    
    if (mEditor && mEditor->preference()) {
        int accentRgba = mEditor->preference()->getInt(SETTING::ACCENT_COLOR);
        accentColor = QColor::fromRgba(accentRgba);
    }
    
    // Apply the highlight color to the list widget
    ui->colorListWidget->setStyleSheet(QStringLiteral(
        "QListWidget::item:selected { background: %1; }"
        "QListWidget::item:selected:active { background: %1; }"
        "QListWidget::item:selected:!active { background: %1; }"
    ).arg(accentColor.name(QColor::HexRgb)));
}

void ColorPaletteWidget::onAccentColorChanged(SETTING setting)
{
    if (setting == SETTING::ACCENT_COLOR) {
        updateHighlightColor();
    }
}
