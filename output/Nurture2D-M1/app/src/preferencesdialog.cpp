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
#include "preferencesdialog.h"

#include "ui_preferencesdialog.h"
#include "preferencesdef.h"
#include "preferencemanager.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "../../CUI/cosmictext.h"
#include "../../CUI/cosmicuimetrics.h"
#include "cosmicwindowheader.h"
#include "windowsscrollbarspage.h"

namespace {
class PreferencesListDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

static QPainterPath leftRoundedSelectionPath(const QRectF& r, qreal radius, bool roundTopLeft, bool roundBottomLeft)
{
    const qreal rad = qMax<qreal>(0.0, qMin<qreal>(radius, qMin(r.width(), r.height()) * 0.5));

    QPainterPath p;
    p.moveTo(r.left() + (roundTopLeft ? rad : 0.0), r.top());
    p.lineTo(r.right(), r.top());
    p.lineTo(r.right(), r.bottom());
    p.lineTo(r.left() + (roundBottomLeft ? rad : 0.0), r.bottom());

    if (roundBottomLeft) {
        p.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - rad);
    } else {
        p.lineTo(r.left(), r.bottom());
    }

    p.lineTo(r.left(), r.top() + (roundTopLeft ? rad : 0.0));

    if (roundTopLeft) {
        p.quadTo(r.left(), r.top(), r.left() + rad, r.top());
    } else {
        p.lineTo(r.left(), r.top());
    }

    p.closeSubpath();
    return p;
}

class PreferencesCategoryDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        opt.state &= ~QStyle::State_HasFocus;

        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        if (selected) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);

            const int row = index.row();
            const bool roundTopLeft = (row == 0);
            const bool roundBottomLeft = false; // Only top category should be rounded

            QRectF r = QRectF(opt.rect);
            // Small inset so highlight doesn't touch the panel edge.
            r.adjust(1.0, 1.0, -1.0, -1.0);

            const QPainterPath path = leftRoundedSelectionPath(r, 13.0, roundTopLeft, roundBottomLeft);
            painter->fillPath(path, CosmicColor::accent());
            painter->restore();

            opt.state &= ~QStyle::State_Selected;
        }

        opt.backgroundBrush = Qt::NoBrush;

        if (selected) {
            // Draw text manually so we fully control the color
            painter->save();
            painter->setPen(QColor(0x1f, 0x1f, 0x1f, 0xff));
            painter->setFont(opt.font);
            painter->drawText(opt.rect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                              "  " + index.data(Qt::DisplayRole).toString());
            painter->restore();
        } else {
            opt.palette.setColor(QPalette::Text, QColor(255, 255, 255));
            QStyledItemDelegate::paint(painter, opt, index);
        }
    }
};
} // namespace

PreferencesDialog::PreferencesDialog(QWidget* parent) :
    CosmicPopupWindow(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    if (ui->contentsWidget && ui->pagesWidget) {
        for (int i = ui->contentsWidget->count() - 1; i >= 0; --i) {
            QListWidgetItem* it = ui->contentsWidget->item(i);
            if (!it) continue;
            const QString label = it->text();
            if (label == QStringLiteral("Timeline") || label == QStringLiteral("Toggle Taps")) {
                delete ui->contentsWidget->takeItem(i);
            }
        }
    }

    setResizable(true);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);
    setShadowEnabled(false);
    setShadowPadding(0);
    setPersistentSizeKey(QStringLiteral("CosmicPreferences/Size"));

    if (ui->mainLayout) {
        ui->mainLayout->setContentsMargins(16, 10, 16, 16);
        ui->mainLayout->setSpacing(10);

        mCosmicHeaderHost = new CosmicWindowHeader(this);
        connect(static_cast<CosmicWindowHeader*>(mCosmicHeaderHost.data()),
                &CosmicWindowHeader::closeRequested,
                this,
                &QDialog::close);
        ui->mainLayout->insertWidget(0, mCosmicHeaderHost);
    }

    if (ui->titleBarWidget) {
        ui->titleBarWidget->setVisible(false);
        ui->titleBarWidget->setMaximumHeight(0);
    }

    if (ui->buttonBox) {
        ui->buttonBox->setVisible(false);
        ui->buttonBox->setMaximumHeight(0);
    }

    if (ui->closeButton) {
        ui->closeButton->setVisible(false);
        ui->closeButton->setMaximumWidth(0);
    }

    if (ui->categoriesPanel) {
        ui->categoriesPanel->setStyleSheet(QStringLiteral(
            "QWidget {"
            "  background-color: #1a1a1a;"
            "  border-top-left-radius: 14px;"
            "  border-bottom-left-radius: 14px;"
            "}"
        ));
    }

    ui->contentsWidget->setItemDelegate(new PreferencesCategoryDelegate(ui->contentsWidget));

    for (int i = 0; i < ui->contentsWidget->count(); ++i) {
        if (QListWidgetItem* it = ui->contentsWidget->item(i)) {
            it->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        }
    }

    ui->contentsWidget->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "  border-top-left-radius: 13px;"
        "  border-bottom-left-radius: 13px;"
        "  selection-background-color: transparent;"
        "  selection-color: %1;"
        "  color: %1;"
        "}"
        "QListWidget::item {"
        "  color: %1;"
        "  padding: 12px 16px;"
        "  border-bottom: 1px solid #2a2a2a;"
        "  font-size: %2pt;"
        "  background-color: transparent;"
        "}"
        "QListWidget::item:focus { outline: none; }"
        "QListWidget::item:selected:focus { outline: none; }"
        "QListWidget::item:selected { background-color: transparent; }"
        "QListWidget::item:selected:hover { background-color: transparent; }"
        "QListWidget::item:last { border-bottom: none; }"
    ).arg(CosmicText::colorHex()).arg(CosmicText::fontSize()));

    // Apply CosmicText to the main navigation list
    if (ui->contentsWidget) {
        ui->contentsWidget->setFont(CosmicText::font());
        QPalette listPalette = ui->contentsWidget->palette();
        listPalette.setColor(QPalette::Text, CosmicText::color());
        listPalette.setColor(QPalette::HighlightedText, CosmicText::color());
        ui->contentsWidget->setPalette(listPalette);
    }
    
    // Apply CosmicText to the title label (but keep it white and bold for title)
    if (ui->titleLabel) {
        QFont titleFont = CosmicText::font();
        titleFont.setBold(true);
        titleFont.setPointSize(14); // Keep larger size for title
        ui->titleLabel->setFont(titleFont);
        // Keep white color for title
    }
}

void PreferencesDialog::onColorPreferenceChanged(SETTING setting)
{
    if (setting == SETTING::ACCENT_COLOR) {
        // Update the preferences dialog navigation styles when accent color changes
        const QColor base = BlenderStyleSlider::getAccentColor();
        const QString baseHex = base.name(QColor::HexRgb);
        const QString borderHex = base.darker(150).name(QColor::HexRgb);

        ui->contentsWidget->setStyleSheet(QStringLiteral(
            "QListWidget { font-size: %1pt; selection-background-color: transparent; selection-color: %2; color: %2; }"
            "QListWidget::item { padding: 8px 12px; margin: 0px 2px; color: %2; border-bottom: 1px solid #1f1f1f; }"
            "QListWidget::item:focus { outline: none; }"
            "QListWidget::item:selected:focus { outline: none; }"
            "QListWidget::item:selected {"
            "  background: %3;"
            "  border: 1px solid %4;"
            "  border-radius: 4px;"
            "  color: #ffffff;"
            "}"
        ).arg(CosmicText::fontSize()).arg(CosmicText::colorHex()).arg(baseHex, borderHex));

        // Propagate to child pages that use Blender-style dropdowns
        if (ui->general) {
            ui->general->updateAccentColors();
        }
    }
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::init(PreferenceManager* m)
{
    Q_ASSERT(m != nullptr);
    mPrefManager = m;

    // Initialize only the first page (General) to reduce startup overhead
    ui->general->setManager(mPrefManager);
    ui->general->updateValues();

    // Set manager for other pages but defer updateValues() until page is shown
    ui->filesPage->setManager(mPrefManager);
    ui->tools->setManager(mPrefManager);
    ui->shortcuts->setManager(mPrefManager);
    ui->windowsScrollbars->setManager(mPrefManager);
    
    // Connect to color preference changes for real-time updates
    connect(mPrefManager, &PreferenceManager::optionChanged, this, &PreferencesDialog::onColorPreferenceChanged);

    connect(ui->general, &GeneralPage::windowOpacityChange, this, &PreferencesDialog::windowOpacityChange);
    connect(ui->filesPage, &FilesPage::clearRecentList, this, &PreferencesDialog::clearRecentList);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::close);

    auto onCurrentItemChanged = static_cast<void (QListWidget::*)(QListWidgetItem*, QListWidgetItem*)>(&QListWidget::currentItemChanged);
    connect(ui->contentsWidget, onCurrentItemChanged, this, &PreferencesDialog::changePage);
}

void PreferencesDialog::closeEvent(QCloseEvent*)
{
    CosmicPopupWindow::closeEvent(nullptr);
}

void PreferencesDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current)
        current = previous;

    if (!current || !ui->contentsWidget || !ui->pagesWidget) {
        return;
    }

    const int pageIndex = ui->contentsWidget->row(current);
    if (pageIndex < 0 || pageIndex >= ui->pagesWidget->count()) {
        return;
    }

    ui->pagesWidget->setCurrentIndex(pageIndex);

    // Lazy initialization: update page values only when first shown
    QWidget* currentPage = ui->pagesWidget->widget(pageIndex);
    if (currentPage && !currentPage->property("valuesUpdated").toBool()) {
        if (currentPage == ui->filesPage) {
            ui->filesPage->updateValues();
        } else if (currentPage == ui->tools) {
            ui->tools->updateValues();
        } else if (currentPage == ui->windowsScrollbars) {
            ui->windowsScrollbars->updateValues();
        }
        currentPage->setProperty("valuesUpdated", true);
    }

    if (currentPage && !currentPage->property("cosmicTextApplied").toBool()) {
        applyCosmicTextRecursively(currentPage);
        currentPage->setProperty("cosmicTextApplied", true);
    }
}

void PreferencesDialog::updateRecentListBtn(bool isEmpty)
{
    if (isEmpty)
    {
        emit updateRecentFileListBtn();
    }
}

void PreferencesDialog::applyCosmicTextToAllWidgets()
{
    // Apply CosmicText to the main navigation list
    if (ui->contentsWidget) {
        ui->contentsWidget->setFont(CosmicText::font());
        QPalette listPalette = ui->contentsWidget->palette();
        listPalette.setColor(QPalette::Text, CosmicText::color());
        listPalette.setColor(QPalette::HighlightedText, CosmicText::color());
        ui->contentsWidget->setPalette(listPalette);
    }
    
    // Apply CosmicText to the title label (but keep it white and bold for title)
    if (ui->titleLabel) {
        QFont titleFont = CosmicText::font();
        titleFont.setBold(true);
        titleFont.setPointSize(14); // Keep larger size for title
        ui->titleLabel->setFont(titleFont);
        // Keep white color for title
    }
}

void PreferencesDialog::applyCosmicTextRecursively(QWidget* widget)
{
    if (!widget) return;
    
    // Apply CosmicText to current widget
    CosmicText::applyToWidget(widget);
    
    // Apply to all child widgets
    const QList<QWidget*> children = widget->findChildren<QWidget*>();
    for (QWidget* child : children) {
        // Skip certain widgets that should maintain their own styling
        if (child->metaObject()->className() == QString("QLineEdit") ||
            child->metaObject()->className() == QString("QTextEdit") ||
            child->metaObject()->className() == QString("QSpinBox") ||
            child->metaObject()->className() == QString("QDoubleSpinBox") ||
            child->metaObject()->className() == QString("QComboBox")) {
            continue;
        }
        
        CosmicText::applyToWidget(child);
    }
}
