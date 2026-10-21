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
#include "shortcutspage.h"
#include "ui_shortcutspage.h"

#include <QApplication>
#include <QDebug>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QKeyEvent>
#include <QKeySequence>
#include "pencilsettings.h"
#include <QFile>
#include <QFileDialog>
 #include <QIcon>
 #include <QLabel>
 #include <QLayout>
 #include <QPainter>
#include <QToolButton>
#include <QEvent>
#include <QTimer>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "../../CUI/cosmicuimetrics.h"
#include "../../CUI/cosmictext.h"
#include "cosmicconfirmpopup.h"
#include "preferencemanager.h"
#include "preferencesdef.h"


static const int ACT_NAME_COLUMN = 0;
static const int KEY_SEQ_COLUMN  = 1;

static QString getHumanReadableShortcutName(const QString&);

namespace {
class CenteredTextWidget final : public QLabel
{
public:
    explicit CenteredTextWidget(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setTextAlignment(Qt::Alignment a) { m_align = a; update(); }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        return QSize(fm.horizontalAdvance(text()), fm.height());
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setPen(palette().color(QPalette::WindowText));

        const QFontMetrics fm(font());
        const QString elided = fm.elidedText(text(), Qt::ElideRight, width());
        const QRect br = fm.boundingRect(elided);

        int x = 0;
        if (m_align.testFlag(Qt::AlignHCenter)) {
            x = (width() - br.width()) / 2;
        } else if (m_align.testFlag(Qt::AlignRight)) {
            x = width() - br.width();
        }
        const int y = (height() - br.height()) / 2 - br.top();
        p.drawText(QPoint(x, y), elided);
    }

private:
    Qt::Alignment m_align = Qt::AlignLeft;
};
}

static void applyPaneUi(QPushButton* header, QFrame* contents)
{
    if (!header || !contents) {
        return;
    }

    const bool expanded = header->isChecked();
    contents->setVisible(expanded);

    const QIcon icon(expanded ? QStringLiteral(":/Interaction/ArrowDown.svg")
                              : QStringLiteral(":/Interaction/ArrowRight.svg"));
    header->setIcon(icon);
    header->setIconSize(QSize(18, 18));

    const int radius = CosmicUiMetrics::cornerRadius();

    QWidget* pane = header->parentWidget();
    if (pane) {
        pane->setAttribute(Qt::WA_StyledBackground, true);
        const QString paneName = pane->objectName();
        if (!paneName.isEmpty()) {
            pane->setStyleSheet(QStringLiteral(
                "QWidget#%1 {"
                "  background: #262626;"
                "  border-radius: %2px;"
                "}"
            ).arg(paneName).arg(radius));
        } else {
            pane->setStyleSheet(QStringLiteral(
                "QWidget {"
                "  background: #262626;"
                "  border-radius: %1px;"
                "}"
            ).arg(radius));
        }
    }

    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setAutoFillBackground(false);
    header->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: 0px;"
        "  border-radius: 0px;"
        "  padding-left: 16px;"
        "  padding-right: 16px;"
        "  color: #cfcfcf;"
        "  font-weight: 500;"
        "  text-align: left;"
        "}"
        "QPushButton:hover { color: #e0e0e0; }"
        "QPushButton:pressed { color: #f0f0f0; }"
    ));

    contents->setAttribute(Qt::WA_StyledBackground, true);
    contents->setAutoFillBackground(false);
    contents->setStyleSheet(QStringLiteral(
        "QFrame {"
        "  background: transparent;"
        "  border: 0px;"
        "  border-radius: 0px;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}"
    ));
}

ShortcutsPage::ShortcutsPage( QWidget* parent )
    : QWidget(parent),
    ui( new Ui::ShortcutsPage ),
    mAccentColor(BlenderStyleSlider::defaultSliderColor())
{
    ui->setupUi(this);

    {
        const QSet<QString> tabLayoutNames = {
            QStringLiteral("tabMenusLayout"),
            QStringLiteral("tabTimelineLayout"),
            QStringLiteral("tabToolsLayout"),
            QStringLiteral("tabSidePanelsLayout"),
            QStringLiteral("tabControlCenterLayout"),
        };

        const auto layouts = findChildren<QLayout*>();
        for (QLayout* l : layouts) {
            if (!l) {
                continue;
            }
            if (!tabLayoutNames.contains(l->objectName())) {
                continue;
            }
            l->setContentsMargins(0, 0, 0, 0);
            l->setSpacing(0);
        }
    }
    {
        const auto gridLayouts = findChildren<QGridLayout*>();
        for (QGridLayout* g : gridLayouts) {
            if (!g) {
                continue;
            }
            const QString n = g->objectName();
            if (!n.contains(QStringLiteral("PaneLayout"))) {
                continue;
            }
            g->setContentsMargins(0, 0, 0, 0);
            g->setHorizontalSpacing(0);
            g->setVerticalSpacing(0);
        }

        const auto paneWidgets = findChildren<QWidget*>();
        for (QWidget* w : paneWidgets) {
            if (!w) {
                continue;
            }
            const QString n = w->objectName();
            if (!n.endsWith(QStringLiteral("Pane"))) {
                continue;
            }
            // Only target our accordion card containers; avoid touching unrelated widgets.
            if (!n.contains(QStringLiteral("Pane")) || n.contains(QStringLiteral("Contents")) || n.contains(QStringLiteral("Header"))) {
                continue;
            }
            w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            w->setMinimumWidth(0);
            if (auto* layout = w->layout()) {
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(0);
            }
        }
    }

    {
        const auto vboxLayouts = findChildren<QVBoxLayout*>();
        for (QVBoxLayout* v : vboxLayouts) {
            if (!v) {
                continue;
            }
            const QString n = v->objectName();
            if (!n.endsWith(QStringLiteral("ScrollAreaContentsLayout"))) {
                continue;
            }
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(10);
        }
    }
    
    // Apply cosmic scrollbar styling to match timeline scrollbars
    const QString scrollbarStyle = 
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
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }";

    {
        const auto scrollAreas = findChildren<QScrollArea*>();
        for (QScrollArea* sa : scrollAreas) {
            if (!sa) {
                continue;
            }
            sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            if (QScrollBar* vsb = sa->verticalScrollBar()) {
                vsb->setFixedWidth(12);
                vsb->setStyleSheet(scrollbarStyle);
            }
            if (QWidget* vp = sa->viewport()) {
                vp->setAutoFillBackground(false);
                vp->setAttribute(Qt::WA_StyledBackground, true);
                vp->setStyleSheet(QStringLiteral("background: transparent;"));
            }
        }
    }

    if (ui->treeView) {
        ui->treeView->setVisible(false);
        ui->treeView->setMaximumHeight(0);
    }
    if (ui->groupBox) {
        ui->groupBox->setVisible(false);
        ui->groupBox->setMaximumHeight(0);
    }

    if (ui->searchEdit) {
        const int textH = QFontMetrics(ui->searchEdit->font()).height();
        const int h = qMax(32, textH + 14);
        ui->searchEdit->setFixedHeight(h);

        const int r = qMax(10, h / 2);
        ui->searchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  border-radius: %1px;"
            "  padding-left: 14px;"
            "  padding-right: 14px;"
            "  padding-top: 0px;"
            "  padding-bottom: 0px;"
            "  border: 1px solid #3a3a3a;"
            "  background: #1f1f1f;"
            "  color: %2;"
            "}"
            "QLineEdit:focus { border: 1px solid %3; }"
        ).arg(r).arg(CosmicText::colorHex()).arg(CosmicColor::accentHex()));

        for (QPushButton* b : {ui->btnLoadShortcuts, ui->btnSaveShortcuts, ui->restoreShortcutsButton}) {
            if (!b) {
                continue;
            }
            b->setFixedHeight(h);
            b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            b->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  border-radius: %1px;"
                "  padding-left: 14px;"
                "  padding-right: 14px;"
                "  border: 1px solid #3a3a3a;"
                "  background: #1f1f1f;"
                "  color: %2;"
                "}"
                "QPushButton:hover { background: #242424; }"
                "QPushButton:pressed { background: #2a2a2a; }"
            ).arg(r).arg(CosmicText::colorHex()));
        }
    }

    // Make the legacy shortcut recorder field hug content vertically.
    if (ui->keySequenceEdit) {
        const int textH = QFontMetrics(ui->keySequenceEdit->font()).height();
        const int h = qMax(22, textH + 6);
        ui->keySequenceEdit->setFixedHeight(h);
        ui->keySequenceEdit->setContentsMargins(0, 0, 0, 0);
        ui->keySequenceEdit->setStyleSheet(QStringLiteral(
            "QKeySequenceEdit { padding-top: 0px; padding-bottom: 0px; margin: 0px; }"
        ));

        if (QLineEdit* le = ui->keySequenceEdit->findChild<QLineEdit*>()) {
            le->setFixedHeight(h);
            le->setContentsMargins(0, 0, 0, 0);
            le->setStyleSheet(QStringLiteral(
                "QLineEdit { padding-top: 0px; padding-bottom: 0px; margin: 0px; }"
            ));
        }
    }

    if (ui->keySequenceEdit) {
        ui->keySequenceEdit->installEventFilter(this);
    }

    auto bindPane = [this](QPushButton* header, QFrame* contents) {
        if (!header || !contents) {
            return;
        }
        connect(header, &QPushButton::toggled, this, [this, header, contents](bool on) {
            applyPaneUi(header, contents);
            if (on) {
                reloadAllCategoryPanes();
            }
        });
        applyPaneUi(header, contents);
    };

    bindPane(ui->filePaneHeaderButton, ui->filePaneContents);
    bindPane(ui->editPaneHeaderButton, ui->editPaneContents);
    bindPane(ui->viewPaneHeaderButton, ui->viewPaneContents);
    bindPane(ui->layerPaneHeaderButton, ui->layerPaneContents);
    bindPane(ui->toolsPaneHeaderButton, ui->toolsPaneContents);

    bindPane(ui->controlCenterPalettePaneHeaderButton, ui->controlCenterPalettePaneContents);
    bindPane(ui->controlCenterAutoFillPaneHeaderButton, ui->controlCenterAutoFillPaneContents);
    bindPane(ui->controlCenterVisibilityPaneHeaderButton, ui->controlCenterVisibilityPaneContents);

    bindPane(ui->timelinePaneHeaderButton, ui->timelinePaneContents);
    bindPane(ui->sidePanelsPaneHeaderButton, ui->sidePanelsPaneContents);

    m_treeModel = new QStandardItemModel(this);
    m_treeModel->setColumnCount(2);
    m_treeModel->setHorizontalHeaderLabels({ tr("Action", "Shortcut table header"), tr("Shortcut", "Shortcut table header") });
    treeModelLoadShortcutsSetting();

    ui->treeView->setModel(m_treeModel);
    ui->treeView->resizeColumnToContents(0);
    updateHighlightColor();

    m_fileTreeModel = new QStandardItemModel(this);
    m_editTreeModel = new QStandardItemModel(this);
    m_viewTreeModel = new QStandardItemModel(this);
    m_layerTreeModel = new QStandardItemModel(this);
    m_toolsTreeModel = new QStandardItemModel(this);
    m_timelineTabTreeModel = new QStandardItemModel(this);
    m_sidePanelsTabTreeModel = new QStandardItemModel(this);
    m_papersPaneTreeModel = new QStandardItemModel(this);
    m_controlCenterTabTreeModel = new QStandardItemModel(this);
    m_controlCenterPaletteTreeModel = new QStandardItemModel(this);
    m_controlCenterAutoFillTreeModel = new QStandardItemModel(this);
    m_controlCenterVisibilityTreeModel = new QStandardItemModel(this);
    m_searchResultsTreeModel = new QStandardItemModel(this);

    for (QStandardItemModel* m : {m_fileTreeModel, m_editTreeModel, m_viewTreeModel, m_layerTreeModel, m_toolsTreeModel, m_timelineTabTreeModel, m_sidePanelsTabTreeModel, m_papersPaneTreeModel, m_controlCenterTabTreeModel, m_controlCenterPaletteTreeModel, m_controlCenterAutoFillTreeModel, m_controlCenterVisibilityTreeModel, m_searchResultsTreeModel}) {
        m->setColumnCount(2);
        m->setHorizontalHeaderLabels({ tr("Action", "Shortcut table header"), tr("Shortcut", "Shortcut table header") });
    }

    reloadAllCategoryPanes();

    // Search wiring
    if (auto se = findChild<QLineEdit*>("searchEdit")) {
        connect(se, &QLineEdit::textChanged, this, &ShortcutsPage::onSearchTextChanged);
    }

    connect( ui->treeView, &QTreeView::clicked, this, &ShortcutsPage::tableItemClicked );
    connect( ui->keySequenceEdit, &QKeySequenceEdit::editingFinished, this, &ShortcutsPage::keyCapLineEditTextChanged );
    connect( ui->keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutsPage::onKeySequenceLiveChanged );
    connect( ui->restoreShortcutsButton, &QPushButton::clicked, this, &ShortcutsPage::restoreShortcutsButtonClicked );
    connect(ui->btnSaveShortcuts, &QPushButton::clicked, this, &ShortcutsPage::saveShortcutsButtonClicked);
    connect(ui->btnLoadShortcuts, &QPushButton::clicked, this, &ShortcutsPage::loadShortcutsButtonClicked);
    connect( ui->clearButton, &QPushButton::clicked, this, &ShortcutsPage::clearButtonClicked );

    m_activeModel = m_treeModel;
    ui->treeView->selectionModel()->select(QItemSelection(m_treeModel->index(0, 0), m_treeModel->index(0, m_treeModel->columnCount() - 1)), QItemSelectionModel::Select);
    tableItemClicked(m_treeModel->index(0, 0));
}

ShortcutsPage::~ShortcutsPage() {
    // Detach the model from the view before it gets destroyed.
    // QListView/QTreeView holds a raw pointer to the model; if a queued mouse
    // event fires during destruction the view will call into a deleted model
    // through a zeroed vtable, causing a SIGSEGV.
    if (ui && ui->treeView) {
        ui->treeView->setModel(nullptr);
    }
    delete ui;
}

void ShortcutsPage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslatePageStrings();
    }
}

void ShortcutsPage::retranslatePageStrings()
{
    // Retranslate all .ui-sourced strings (buttons, tab titles, placeholders, etc.)
    ui->retranslateUi(this);

    // Update tree model column headers — these are set programmatically, not via .ui.
    const QStringList headers = {
        tr("Action",   "Shortcut table header"),
        tr("Shortcut", "Shortcut table header")
    };
    for (QStandardItemModel* m : {
            m_treeModel,
            m_fileTreeModel, m_editTreeModel, m_viewTreeModel,
            m_layerTreeModel, m_toolsTreeModel,
            m_timelineTabTreeModel, m_sidePanelsTabTreeModel, m_papersPaneTreeModel,
            m_controlCenterTabTreeModel, m_controlCenterPaletteTreeModel,
            m_controlCenterAutoFillTreeModel, m_controlCenterVisibilityTreeModel,
            m_searchResultsTreeModel
        }) {
        if (m) m->setHorizontalHeaderLabels(headers);
    }

    // Rebuild all shortcut rows — this re-calls getHumanReadableShortcutName()
    // which uses tr(), so all action labels pick up the new language.
    // Also rebuilds the clear-button tooltips.
    cancelRemap();
    treeModelLoadShortcutsSetting();
    reloadAllCategoryPanes();
}

void ShortcutsPage::tableItemClicked( const QModelIndex& modelIndex )
{
    setActiveShortcutModel(m_treeModel, modelIndex);
}

void ShortcutsPage::setActiveShortcutModel(QStandardItemModel* model, const QModelIndex& index)
{
    if (!model || !index.isValid()) {
        return;
    }

    m_activeModel = model;
    m_currentItemIndex = index;

    const int row = index.row();
    QStandardItem* actionItem = m_activeModel->item(row, ACT_NAME_COLUMN);
    QStandardItem* keySeqItem = m_activeModel->item(row, KEY_SEQ_COLUMN);
    if (!actionItem || !keySeqItem) {
        return;
    }

    ui->actionNameLabel->setText(actionItem->text());
    m_settingKeySequenceProgrammatically = true;
    ui->keySequenceEdit->setKeySequence(keySeqItem->text());
    m_settingKeySequenceProgrammatically = false;

    qDebug() << "Command Selected:" << actionItem->text();
    ui->keySequenceEdit->setFocus();
}

void ShortcutsPage::onKeySequenceLiveChanged(const QKeySequence& seq)
{
    if (m_settingKeySequenceProgrammatically) {
        return;
    }
    if (!m_remapIndex.isValid()) {
        return;
    }
    if (m_activeModel != m_remapModel) {
        return;
    }
    if (!m_currentItemIndex.isValid() || m_currentItemIndex.row() != m_remapIndex.row()) {
        return;
    }
    if (!m_remapShortcutLabel) {
        return;
    }

    // Restrict to a single chord: optional Ctrl/Shift/Alt + one non-modifier key.
    const int key0 = seq.count() > 0 ? seq[0] : 0;
    Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(key0 & Qt::KeyboardModifierMask);
    mods &= (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
    const int baseKey = key0 & ~Qt::KeyboardModifierMask;

    const bool baseIsModifier = (baseKey == Qt::Key_Control)
        || (baseKey == Qt::Key_Shift)
        || (baseKey == Qt::Key_Alt)
        || (baseKey == Qt::Key_Meta)
        || (baseKey == Qt::Key_AltGr);

    // If the user has only pressed modifiers, keep the UI in "..." and keep the editor empty.
    if (key0 == 0 || baseKey == 0 || baseIsModifier) {
        m_remapShortcutLabel->setText(QStringLiteral("..."));
        if (!ui->keySequenceEdit->keySequence().isEmpty()) {
            m_settingKeySequenceProgrammatically = true;
            ui->keySequenceEdit->setKeySequence(QKeySequence());
            m_settingKeySequenceProgrammatically = false;
        }
        return;
    }

    const int sanitized = int(mods) | baseKey;
    const QKeySequence sanitizedSeq(sanitized);
    if (ui->keySequenceEdit->keySequence() != sanitizedSeq) {
        m_settingKeySequenceProgrammatically = true;
        ui->keySequenceEdit->setKeySequence(sanitizedSeq);
        m_settingKeySequenceProgrammatically = false;
    }

    QString t = sanitizedSeq.toString(QKeySequence::NativeText);
    if (t.trimmed().isEmpty()) {
        t = QStringLiteral("...");
    }
    t.replace('+', QStringLiteral(" + "));
    m_remapShortcutLabel->setText(t);

    // Auto-confirm as soon as a valid chord is complete, but defer to next tick
    // so the UI can repaint the live text (otherwise it may appear to stay as "...").
    QTimer::singleShot(0, this, [this]() {
        if (!m_remapIndex.isValid()) {
            return;
        }
        if (m_activeModel != m_remapModel) {
            return;
        }
        if (!m_currentItemIndex.isValid() || m_currentItemIndex.row() != m_remapIndex.row()) {
            return;
        }
        if (ui && ui->keySequenceEdit && !ui->keySequenceEdit->keySequence().isEmpty()) {
            keyCapLineEditTextChanged();
        }
    });
}

void ShortcutsPage::keyCapLineEditTextChanged()
{
    QKeySequence keySequence = ui->keySequenceEdit->keySequence();
    if (!m_currentItemIndex.isValid())
    {
        return;
    }

    // Esc is reserved for cancelling remap and must never be stored as a shortcut.
    if (keySequence.count() > 0 && ((keySequence[0] & ~Qt::KeyboardModifierMask) == Qt::Key_Escape)) {
        m_settingKeySequenceProgrammatically = true;
        ui->keySequenceEdit->setKeySequence(QKeySequence());
        m_settingKeySequenceProgrammatically = false;
        return;
    }

    // Leaving remap mode (commit or cancel) happens here.
    // Save these before clearing so we can restore on cancel.
    QPointer<QLabel> savedRemapLabel = m_remapShortcutLabel;
    QString savedOriginalText = m_remapOriginalText;

    if (m_remapShortcutContainer) {
        m_remapShortcutContainer->setProperty("remapping", false);
        m_remapShortcutContainer->style()->unpolish(m_remapShortcutContainer);
        m_remapShortcutContainer->style()->polish(m_remapShortcutContainer);
        m_remapShortcutContainer->update();
    }
    m_remapModel = nullptr;
    m_remapIndex = QModelIndex();
    m_remapRow.clear();
    m_remapShortcutContainer.clear();
    m_remapShortcutLabel.clear();
    m_remapOriginalText.clear();

    if (!m_activeModel) {
        return;
    }

    int row = m_currentItemIndex.row();
    QStandardItem* actionItem = m_activeModel->item(row, ACT_NAME_COLUMN);
    QStandardItem* keySeqItem = m_activeModel->item(row, KEY_SEQ_COLUMN);

    QString strCmdName = actionItem->data().toString();
    QString strKeySeq  = keySequence.toString( QKeySequence::PortableText );

    QSettings setting( PENCIL2D, PENCIL2D );
    setting.beginGroup("shortcuts");

    if (isKeySequenceExist(setting, strCmdName, keySequence))
    {
        CosmicConfirmPopup msgBox(this);
        msgBox.setBodyText(tr("%1 is already assigned. Replace it?").arg(keySequence.toString(QKeySequence::NativeText)));
        msgBox.setConfirmText(tr("Replace"));
        msgBox.setCancelText(tr("Cancel"));

        if (msgBox.exec() != QDialog::Accepted)
        {
            m_settingKeySequenceProgrammatically = true;
            ui->keySequenceEdit->setKeySequence( QKeySequence(keySeqItem->text()) );
            m_settingKeySequenceProgrammatically = false;
            if (savedRemapLabel) {
                savedRemapLabel->setText(savedOriginalText);
            }
            return;
        }
        removeDuplicateKeySequence(&setting, keySequence);
    }

    setting.setValue(strCmdName, strKeySeq);
    setting.endGroup();
    setting.sync();

    const QString nativeText = keySequence.toString(QKeySequence::NativeText);
    if (keySeqItem) {
        keySeqItem->setText(nativeText);
    }
    // Update the inline shortcut label (savedRemapLabel because m_remapShortcutLabel was cleared above).
    if (savedRemapLabel) {
        QString t = nativeText;
        if (t.trimmed().isEmpty()) {
            t = QStringLiteral("...");
        }
        t.replace('+', QStringLiteral(" + "));
        savedRemapLabel->setText(t);
    }

    // Reload all panes so any displaced shortcut (cleared by removeDuplicateKeySequence) is
    // reflected immediately in the UI without needing to close/reopen the preferences window.
    treeModelLoadShortcutsSetting();
    refreshAfterSave();

    qDebug() << "Shortcut " << strCmdName << " = " << strKeySeq;
}

void ShortcutsPage::saveShortcutsButtonClicked()
{
    QSettings settings( PENCIL2D, PENCIL2D );
    settings.beginGroup( "LastSavePath" );

    QString fDir = settings.value("Shortcuts").toString();
    if (fDir.isEmpty())
        fDir = QDir::homePath();

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save Pencil2D Shortcut file"),
                                                    fDir + "/" + tr("untitled.pcls"),
                                                    tr("Pencil2D Shortcut File(*.pcls)"));
    if (fileName.isEmpty()) {
        settings.endGroup();
        return;
    }

    settings.setValue("Shortcuts", QFileInfo(fileName).absolutePath());
    settings.endGroup();

    QSettings out(fileName, QSettings::IniFormat);
    settings.beginGroup( "shortcuts" );
    out.beginGroup("shortcuts");

    foreach (QString key, settings.allKeys())
    {
        out.setValue(key, settings.value(key));
    }
}

void ShortcutsPage::loadShortcutsButtonClicked()
{
    QSettings settings( PENCIL2D, PENCIL2D );
    settings.beginGroup("LastSavePath");
    QString fDir = settings.value("Shortcuts").toString();
    settings.endGroup();
    if (fDir.isEmpty())
        fDir = QDir::homePath();

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Pencil2D Shortcut file"),
                                                    fDir,
                                                    tr("Pencil2D Shortcut File(*.pcls)"));

    if (fileName.isEmpty())
    {
        return;
    }

    if (!QFileInfo(fileName).isReadable())
    {
        qDebug() << "Shortcut file not readable";
        return;
    }

    QSettings input(fileName, QSettings::IniFormat);
    if (input.status() != QSettings::NoError || !input.childGroups().contains("shortcuts"))
    {
        qDebug() << "Error accessing or parsing shortcut file" << input.status();
        return;
    }

    input.beginGroup("shortcuts");
    settings.beginGroup("shortcuts");
    foreach (QString pShortcutsKey, input.allKeys())
    {
        settings.setValue(pShortcutsKey, input.value(pShortcutsKey));
    }

    treeModelLoadShortcutsSetting();
    refreshAfterSave();
}

void ShortcutsPage::loadCategoryModel(QStandardItemModel* model, const QStringList& cmdNames)
{
    if (!model) {
        return;
    }

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup("shortcuts");

    model->setRowCount(cmdNames.size());
    if (model->columnCount() < 2) {
        model->setColumnCount(2);
    }

    int row = 0;
    for (const QString& cmdName : cmdNames) {
        const QString& human = getHumanReadableShortcutName(cmdName);
        if (human.isEmpty()) {
            continue;
        }

        QString strKeySequence = settings.value(cmdName).toString();
        strKeySequence = QKeySequence(strKeySequence).toString(QKeySequence::NativeText);

        if (model->item(row, ACT_NAME_COLUMN) == nullptr) {
            model->setItem(row, ACT_NAME_COLUMN, new QStandardItem());
        }
        if (model->item(row, KEY_SEQ_COLUMN) == nullptr) {
            model->setItem(row, KEY_SEQ_COLUMN, new QStandardItem());
        }

        model->item(row, ACT_NAME_COLUMN)->setData(cmdName);
        model->item(row, ACT_NAME_COLUMN)->setText(human);
        model->item(row, ACT_NAME_COLUMN)->setEditable(false);

        model->item(row, KEY_SEQ_COLUMN)->setText(strKeySequence);
        model->item(row, KEY_SEQ_COLUMN)->setEditable(false);

        ++row;
    }

    model->setRowCount(row);
    settings.endGroup();
}

void ShortcutsPage::rebuildCategoryShortcutsList(QStandardItemModel* model, QVBoxLayout* listLayout)
{
    if (!ui || !listLayout || !model) {
        return;
    }

    while (QLayoutItem* item = listLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex idx = model->index(row, 0);
        const QString actionText = model->item(row, ACT_NAME_COLUMN)->text();
        QString shortcutText = model->item(row, KEY_SEQ_COLUMN)->text();
        shortcutText.replace('+', QStringLiteral(" + "));
        const QString cmdName = model->item(row, ACT_NAME_COLUMN)->data().toString();

        // Resolve a color dot for commands that map to a specific color slot.
        // Order matches the FILL_COLORS list in PenColorPalette / AutoLineColorToggle:
        //   slot index 0 = Red, 1 = Green, 2 = Blue, 3 = Yellow, 4 = Magenta
        static const QHash<QString, QColor> kCmdColors = {
            { QStringLiteral(CMD_CONTROL_CENTER_PEN_PALETTE_2), Qt::red     },
            { QStringLiteral(CMD_CONTROL_CENTER_PEN_PALETTE_3), Qt::green   },
            { QStringLiteral(CMD_CONTROL_CENTER_PEN_PALETTE_4), Qt::blue    },
            { QStringLiteral(CMD_CONTROL_CENTER_PEN_PALETTE_5), Qt::yellow  },
            { QStringLiteral(CMD_CONTROL_CENTER_PEN_PALETTE_6), QColor(0xff, 0x00, 0xff) },
            { QStringLiteral(CMD_CONTROL_CENTER_AUTOFILL_COLOR_1), Qt::red     },
            { QStringLiteral(CMD_CONTROL_CENTER_AUTOFILL_COLOR_2), Qt::green   },
            { QStringLiteral(CMD_CONTROL_CENTER_AUTOFILL_COLOR_3), Qt::blue    },
            { QStringLiteral(CMD_CONTROL_CENTER_AUTOFILL_COLOR_4), Qt::yellow  },
            { QStringLiteral(CMD_CONTROL_CENTER_AUTOFILL_COLOR_5), QColor(0xff, 0x00, 0xff) },
        };
        const QColor rowDotColor = kCmdColors.value(cmdName, QColor());

        QWidget* rowWidget = new QWidget();
        rowWidget->setObjectName(QStringLiteral("shortcutRow"));
        rowWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        const int textH = QFontMetrics(rowWidget->font()).height();
        // Make the pill hug content (thin) while still safe for different fonts/DPI.
        constexpr int kPillExtra = 6; // total extra height above text
        constexpr int kRowExtra = 8;
        const int pillH = qMax(22, textH + kPillExtra);
        const int rowH = qMax(26, textH + kRowExtra);
        rowWidget->setFixedHeight(rowH);

        auto* h = new QHBoxLayout(rowWidget);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);
        h->setAlignment(Qt::AlignVCenter);

        QWidget* actionContainer = new QWidget(rowWidget);
        actionContainer->setObjectName(QStringLiteral("shortcutActionContainer"));
        actionContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        actionContainer->setFixedHeight(pillH);
        auto* actionLayout = new QHBoxLayout(actionContainer);
        actionLayout->setContentsMargins(10, 0, 10, 0);
        actionLayout->setSpacing(0);
        actionLayout->setAlignment(Qt::AlignVCenter);

        CenteredTextWidget* actionLabel = new CenteredTextWidget(actionContainer);
        actionLabel->setObjectName(QStringLiteral("shortcutActionLabel"));
        actionLabel->setText(actionText);
        actionLabel->setTextAlignment(Qt::AlignLeft);
        actionLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        actionLabel->setFixedHeight(pillH);
        
        // Apply CosmicText styling
        CosmicText::applyToWidget(actionLabel);

        QWidget* actionSlot = new QWidget(actionContainer);
        actionSlot->setObjectName(QStringLiteral("shortcutActionSlot"));
        actionSlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        actionSlot->setFixedHeight(pillH);
        auto* actionSlotLayout = new QVBoxLayout(actionSlot);
        actionSlotLayout->setContentsMargins(0, 0, 0, 0);
        actionSlotLayout->setSpacing(0);

        actionSlotLayout->addWidget(actionLabel, 1);
        // Prepend a small color dot for color-coded shortcut rows.
        if (rowDotColor.isValid()) {
            const int dotSz = qMax(8, textH - 4);
            QLabel* dot = new QLabel(actionContainer);
            dot->setFixedSize(dotSz, dotSz);
            dot->setAttribute(Qt::WA_TransparentForMouseEvents);
            // Circular dot via border-radius
            dot->setStyleSheet(QStringLiteral(
                "QLabel { background: %1; border-radius: %2px; border: none; }"
            ).arg(rowDotColor.name()).arg(dotSz / 2));
            actionLayout->insertWidget(0, dot, 0, Qt::AlignVCenter);
            actionLayout->setContentsMargins(8, 0, 10, 0);
            actionLayout->setSpacing(6);
        }
        actionLayout->addWidget(actionSlot, 1);

        QWidget* shortcutContainer = new QWidget(rowWidget);
        shortcutContainer->setObjectName(QStringLiteral("shortcutContainer"));
        shortcutContainer->setMinimumWidth(200);
        shortcutContainer->setMaximumWidth(200);
        shortcutContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        shortcutContainer->setFixedHeight(pillH);
        shortcutContainer->setProperty("remapping", false);
        auto* shortcutLayout = new QHBoxLayout(shortcutContainer);
        shortcutLayout->setContentsMargins(12, 0, 12, 0);
        shortcutLayout->setSpacing(0);
        shortcutLayout->setAlignment(Qt::AlignVCenter);

        CenteredTextWidget* shortcutLabel = new CenteredTextWidget(shortcutContainer);
        shortcutLabel->setObjectName(QStringLiteral("shortcutLabel"));
        shortcutLabel->setText(shortcutText);
        shortcutLabel->setTextAlignment(Qt::AlignHCenter);
        shortcutLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        shortcutLabel->setFixedHeight(pillH);
        
        // Apply CosmicText styling
        CosmicText::applyToWidget(shortcutLabel);

        QWidget* shortcutSlot = new QWidget(shortcutContainer);
        shortcutSlot->setObjectName(QStringLiteral("shortcutSlot"));
        shortcutSlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        shortcutSlot->setFixedHeight(pillH);
        auto* shortcutSlotLayout = new QVBoxLayout(shortcutSlot);
        shortcutSlotLayout->setContentsMargins(0, 0, 0, 0);
        shortcutSlotLayout->setSpacing(0);

        shortcutSlotLayout->addWidget(shortcutLabel, 1);
        shortcutLayout->addWidget(shortcutSlot, 1);

        QToolButton* clearBtn = new QToolButton(rowWidget);
        clearBtn->setObjectName(QStringLiteral("shortcutClearButton"));
        clearBtn->setIcon(QIcon(QStringLiteral(":/Interaction/CloseWindow.svg")));
        const int clearSz = pillH;
        clearBtn->setIconSize(QSize(qMax(12, clearSz - 10), qMax(12, clearSz - 10)));
        clearBtn->setFixedSize(QSize(clearSz, clearSz));
        clearBtn->setAutoRaise(true);
        clearBtn->setContentsMargins(0, 0, 0, 0);
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setToolTip(tr("Clear"));

        connect(clearBtn, &QToolButton::clicked, this, [this, cmdName]() {
            if (cmdName.isEmpty()) {
                return;
            }
            QSettings setting(PENCIL2D, PENCIL2D);
            setting.beginGroup("shortcuts");
            setting.setValue(cmdName, QString());
            setting.endGroup();
            setting.sync();

            treeModelLoadShortcutsSetting();
            refreshAfterSave();
        });

        // Click-to-remap
        for (QObject* o : {static_cast<QObject*>(rowWidget), static_cast<QObject*>(actionContainer), static_cast<QObject*>(actionLabel), static_cast<QObject*>(shortcutContainer), static_cast<QObject*>(shortcutLabel)}) {
            o->installEventFilter(this);
            m_shortcutRowIndex.insert(o, idx);
            m_shortcutRowModel.insert(o, model);
        }

        h->addWidget(actionContainer);
        h->addWidget(shortcutContainer);
        h->addWidget(clearBtn);
        h->setStretch(0, 1);
        h->setStretch(1, 0);
        h->setStretch(2, 0);

        const QString remapColor = CosmicColor::accentHex();
        const int radius = CosmicUiMetrics::cornerRadius();
        rowWidget->setStyleSheet(QStringLiteral(
            "QWidget#shortcutRow { background: transparent; }"
            "QWidget#shortcutRow QLabel#shortcutActionLabel {"
            "  color: %1;"
            "  font-weight: 400;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0px;"
            "  margin: 0px;"
            "}"
            "QWidget#shortcutRow QWidget#shortcutContainer {"
            "  background: #2f2f2f;"
            "  border: none;"
            "  border-radius: %3px;"
            "}"
            "QWidget#shortcutRow QWidget#shortcutContainer[remapping=\"true\"] {"
            "  background: %2;"
            "  border: none;"
            "  border-radius: %3px;"
            "}"
            "QWidget#shortcutRow QLabel#shortcutLabel {"
            "  color: %1;"
            "  font-weight: 400;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0px;"
            "  margin: 0px;"
            "}"
            "QWidget#shortcutRow QToolButton#shortcutClearButton {"
            "  background: transparent;"
            "  border: none;"
            "}"
        ).arg(CosmicText::colorHex()).arg(remapColor).arg(radius));

        listLayout->addWidget(rowWidget);
    }
}

void ShortcutsPage::reloadAllCategoryPanes()
{
    // Clear stale widget→model mappings before rebuilding rows.
    // rebuildCategoryShortcutsList destroys old row widgets via deleteLater,
    // so any pointers still in these hashes would be dangling after the rebuild.
    m_shortcutRowIndex.clear();
    m_shortcutRowModel.clear();

    // File
    static const QStringList fileCmds = {
        CMD_NEW_FILE,
        CMD_OPEN_FILE,
        CMD_SAVE_FILE,
        CMD_SAVE_AS,
        CMD_IMPORT_IMAGE,
        CMD_IMPORT_IMAGE_SEQ,
        CMD_IMPORT_LAYERS,
        CMD_EXPORT_IMAGE,
        CMD_EXPORT_ANIMATION,
        CMD_EXPORT_GIF,
        CMD_EXPORT_SPRITE,
        CMD_EXIT,
    };

    // Edit
    static const QStringList editCmds = {
        CMD_UNDO,
        CMD_REDO,
        CMD_CUT,
        CMD_COPY,
        CMD_PASTE_FROM_PREVIOUS,
        CMD_PASTE,
        CMD_CLEAR_FRAME,
        CMD_SELECTION_FLIP_HORIZONTAL,
        CMD_SELECTION_FLIP_VERTICAL,
        CMD_SELECT_ALL,
        CMD_DESELECT_ALL,
        CMD_PEGBAR_ALIGNMENT,
        CMD_PREFERENCE,
    };

    // View
    static const QStringList viewCmds = {
        CMD_RESET_ZOOM_ROTATE,
        CMD_ZOOM_IN,
        CMD_ZOOM_OUT,
        CMD_ZOOM_100,
        CMD_ZOOM_50,
        CMD_ZOOM_33,
        CMD_ZOOM_25,
        CMD_ROTATE_CLOCK,
        CMD_ROTATE_ANTI_CLOCK,
        CMD_RESET_ROTATION,
        CMD_FLIP_HORIZONTAL,
        CMD_FLIP_VERTICAL,
        CMD_OVERLAY_CENTER,
        CMD_OVERLAY_THIRDS,
        CMD_OVERLAY_GOLDEN_RATIO,
        CMD_OVERLAY_SAFE_AREAS,
    };

    // Layer
    static const QStringList layerCmds = {
        CMD_NEW_BITMAP_LAYER,
        CMD_DELETE_CUR_LAYER,
        CMD_CHANGE_LINE_COLOR_KEYFRAME,
        CMD_CHANGE_LINE_COLOR_LAYER,
        CMD_CHANGE_LAYER_OPACITY,
        CMD_CURRENT_LAYER_VISIBILITY,
        CMD_RELATIVE_LAYER_VISIBILITY,
        CMD_ALL_LAYER_VISIBILITY,
    };

    // Timeline — all playback, frame navigation, frame editing, onion skin,
    // and timeline panel controls. The Menus > Animation pane is removed;
    // this is the single authoritative home for everything animation/timeline.
    static const QStringList timelineCmds = {
        CMD_PLAY,
        CMD_LOOP,
        CMD_GOTO_PREV_FRAME,
        CMD_GOTO_NEXT_FRAME,
        CMD_GOTO_PREV_KEY_FRAME,
        CMD_GOTO_NEXT_KEY_FRAME,
        CMD_ADD_FRAME,
        CMD_DUPLICATE_FRAME,
        CMD_REMOVE_FRAME,
        CMD_MOVE_FRAME_BACKWARD,
        CMD_MOVE_FRAME_FORWARD,
        CMD_FLIP_INBETWEEN,
        CMD_FLIP_ROLLING,
        CMD_SELECTION_ADD_FRAME_EXPOSURE,
        CMD_SELECTION_SUBTRACT_FRAME_EXPOSURE,
        CMD_REVERSE_SELECTED_FRAMES,
        CMD_REMOVE_SELECTED_FRAMES,
        CMD_SELECTION_REPOSITION_FRAMES,
        // Retime — set spacing of selected keyframes (no ripple)
        CMD_RETIME_1S,
        CMD_RETIME_2S,
        CMD_RETIME_3S,
        // Retime ripple — set spacing and shift all following keyframes
        CMD_RETIME_1S_RIPPLE,
        CMD_RETIME_2S_RIPPLE,
        CMD_RETIME_3S_RIPPLE,
        // Make current drawing unique (break shared exposure)
        CMD_MAKE_UNIQUE_DRAWING,
        // Layer navigation (shared by timeline and papers pane)
        CMD_SELECT_LAYER_ABOVE,
        CMD_SELECT_LAYER_BELOW,
        CMD_ONIONSKIN_PREV,
        CMD_ONIONSKIN_NEXT,
        CMD_COSMIC_BOTTOM_TIMELINE_LOCK_TOGGLE,
    };

    // Papers pane — inner-layer management within a cell
    static const QStringList papersPaneCmds = {
        CMD_PAPERS_NEW,
        CMD_PAPERS_REMOVE,
        CMD_PAPERS_IMPORT,
        CMD_PAPERS_SELECT_ABOVE,
        CMD_PAPERS_SELECT_BELOW,
    };

    // Tools
    static const QStringList toolsCmds = {
        CMD_RESET_ALL_TOOLS,
        CMD_TOOL_PEN,
        CMD_TOOL_HAND,
        CMD_TOOL_BUCKET,
        CMD_TOOL_EYEDROPPER,
        CMD_TOOL_ERASER,
        CMD_TOOL_POLYLINE,
        CMD_TOOL_PENCIL,
        CMD_TOOL_CURVE,
        CMD_TOOL_BSPLINE,
        CMD_TOOL_RECT_GAP_FILL,
        CMD_TOOL_LASSO_GAP_FILL,
        CMD_TOOL_LASSO_SELECT,
        CMD_TOOL_DPLATE,

        // Hold / temporary tools (user-assignable)
        CMD_TEMP_TOOL_PEN,
        CMD_TEMP_TOOL_PENCIL,
        CMD_TEMP_TOOL_ERASER,
        CMD_TEMP_TOOL_SELECT,
        CMD_TEMP_TOOL_MOVE,
        CMD_TEMP_TOOL_POLYLINE,
        CMD_TEMP_TOOL_CURVE,
        CMD_TEMP_TOOL_BSPLINE,
        CMD_TEMP_TOOL_BUCKET,
        CMD_TEMP_TOOL_EYEDROPPER,
        CMD_TEMP_TOOL_DPLATE,
        CMD_TEMP_TOOL_HAND,
        CMD_TEMP_TOOL_LASSO_SELECT,
        CMD_TEMP_RECT_GAP_FILL,
        CMD_TEMP_LASSO_GAP_FILL,

        // Tool property adjustments
        CMD_INCREASE_SIZE,
        CMD_DECREASE_SIZE,
        CMD_INCREASE_OPACITY,
        CMD_DECREASE_OPACITY,
        CMD_INCREASE_SMOOTH,
        CMD_DECREASE_SMOOTH,
    };

    // SidePanels — panel lock toggles and per-pane visibility.
    static const QStringList sidePanelsCmds = {
        CMD_COSMIC_LEFT_LOCK_TOGGLE,
        CMD_COSMIC_RIGHT_LOCK_TOGGLE,
        CMD_COSMIC_PANE_TOGGLE_TOOLS,
        CMD_COSMIC_PANE_TOGGLE_TOOLS_OPTIONS,
        CMD_COSMIC_PANE_TOGGLE_BRUSHES,
        CMD_COSMIC_PANE_TOGGLE_COLOR_WHEEL,
        CMD_COSMIC_PANE_TOGGLE_PALETTES,
        CMD_COSMIC_PANE_TOGGLE_REFERENCE,
    };

    // ControlCenter (Cosmic top bar groups)
    static const QStringList controlCenterPaletteCmds = {
        CMD_CONTROL_CENTER_PEN_PALETTE_1,
        CMD_CONTROL_CENTER_PEN_PALETTE_2,
        CMD_CONTROL_CENTER_PEN_PALETTE_3,
        CMD_CONTROL_CENTER_PEN_PALETTE_4,
        CMD_CONTROL_CENTER_PEN_PALETTE_5,
        CMD_CONTROL_CENTER_PEN_PALETTE_6,
    };

    static const QStringList controlCenterAutoFillCmds = {
        CMD_CONTROL_CENTER_AUTOFILL_MASTER,
        CMD_CONTROL_CENTER_AUTOFILL_COLOR_1,
        CMD_CONTROL_CENTER_AUTOFILL_COLOR_2,
        CMD_CONTROL_CENTER_AUTOFILL_COLOR_3,
        CMD_CONTROL_CENTER_AUTOFILL_COLOR_4,
        CMD_CONTROL_CENTER_AUTOFILL_COLOR_5,
    };

    // Toolbar Visibility — mirrors the 5 entries in the Toolbars menu exactly.
    static const QStringList controlCenterVisibilityCmds = {
        CMD_CC_TOGGLE_FILE_OPS,
        CMD_CC_TOGGLE_VIEW,
        CMD_CC_TOGGLE_OVERLAYS,
        CMD_CC_TOGGLE_UNDO_REDO,
        CMD_CC_TOGGLE_CLEAR_FRAME,
    };

    m_shortcutRowIndex.clear();
    m_shortcutRowModel.clear();

    auto rebuildIfExpanded = [this](QPushButton* header,
                                   QStandardItemModel* model,
                                   const QStringList& cmds,
                                   QVBoxLayout* listLayout) {
        if (!header || !listLayout || !model) {
            return;
        }
        // Always reload the model data so it stays in sync with QSettings,
        // even when the pane is collapsed. This prevents stale shortcut text
        // from appearing when the user later expands a pane.
        loadCategoryModel(model, cmds);
        if (!header->isChecked()) {
            return;
        }
        rebuildCategoryShortcutsList(model, listLayout);
    };

    rebuildIfExpanded(ui->filePaneHeaderButton, m_fileTreeModel, fileCmds, ui->fileShortcutsListLayout);
    rebuildIfExpanded(ui->editPaneHeaderButton, m_editTreeModel, editCmds, ui->editShortcutsListLayout);
    rebuildIfExpanded(ui->viewPaneHeaderButton, m_viewTreeModel, viewCmds, ui->viewShortcutsListLayout);
    rebuildIfExpanded(ui->layerPaneHeaderButton, m_layerTreeModel, layerCmds, ui->layerShortcutsListLayout);
    rebuildIfExpanded(ui->toolsPaneHeaderButton, m_toolsTreeModel, toolsCmds, ui->toolsTabShortcutsListLayout);

    rebuildIfExpanded(ui->timelinePaneHeaderButton, m_timelineTabTreeModel, timelineCmds, ui->timelineShortcutsListLayout);
    rebuildIfExpanded(ui->papersPanePaneHeaderButton, m_papersPaneTreeModel, papersPaneCmds, ui->papersPaneShortcutsListLayout);
    rebuildIfExpanded(ui->sidePanelsPaneHeaderButton, m_sidePanelsTabTreeModel, sidePanelsCmds, ui->sidePanelsShortcutsListLayout);

    rebuildIfExpanded(ui->controlCenterPalettePaneHeaderButton, m_controlCenterPaletteTreeModel, controlCenterPaletteCmds, ui->controlCenterPaletteShortcutsListLayout);
    rebuildIfExpanded(ui->controlCenterAutoFillPaneHeaderButton, m_controlCenterAutoFillTreeModel, controlCenterAutoFillCmds, ui->controlCenterAutoFillShortcutsListLayout);
    rebuildIfExpanded(ui->controlCenterVisibilityPaneHeaderButton, m_controlCenterVisibilityTreeModel, controlCenterVisibilityCmds, ui->controlCenterVisibilityShortcutsListLayout);
}

void ShortcutsPage::restoreShortcutsButtonClicked()
{
    restoreShortcutsToDefault();
    treeModelLoadShortcutsSetting();
    refreshAfterSave();
}

void ShortcutsPage::refreshAfterSave()
{
    // Always reload the category panes.
    reloadAllCategoryPanes();

    // If the search page is currently visible, re-run the search so the results
    // rows are rebuilt with fresh event-filter registrations. Without this, the
    // rows remain visible but are no longer interactive after a save.
    if (ui && ui->shortcutsMainStack &&
        ui->shortcutsMainStack->currentWidget() == ui->shortcutsSearchPage)
    {
        if (QLineEdit* se = findChild<QLineEdit*>(QStringLiteral("searchEdit"))) {
            onSearchTextChanged(se->text());
        }
    }
}

bool ShortcutsPage::isKeySequenceExist(const QSettings& settings, QString strTargetCmdName, QKeySequence targetkeySeq)
{
    foreach (QString strCmdName, settings.allKeys())
    {
        if (strTargetCmdName == strCmdName)
        {
            continue;
        }

        QString strCmdKeySeq = settings.value(strCmdName).toString();
        if (QKeySequence(strCmdKeySeq) == targetkeySeq)
        {
            return true;
        }
    }
    return false;
}

void ShortcutsPage::removeDuplicateKeySequence(QSettings* settings, QKeySequence keySeq)
{
    foreach(QString strCmdName, settings->allKeys())
    {
        QString strCmdKeySeq = settings->value(strCmdName).toString();

        if ( strCmdKeySeq == keySeq.toString(QKeySequence::PortableText))
        {
            settings->setValue(strCmdName, "");
        }
    }
}

void ShortcutsPage::treeModelLoadShortcutsSetting()
{
    // Load shortcuts from settings
    QSettings settings( PENCIL2D, PENCIL2D );
    settings.beginGroup("shortcuts");

    m_treeModel->setRowCount(settings.allKeys().size());
    if (m_treeModel->columnCount() < 2) {
        m_treeModel->setColumnCount(2);
    }

    int row = 0;
    foreach (QString strCmdName, settings.allKeys())
    {
        const QString &strShortcutName = getHumanReadableShortcutName(strCmdName);
        if (strShortcutName.isEmpty()) {
            // Shortcut not supported by this version of Pencil2D
            continue;
        }

        QString strKeySequence = settings.value(strCmdName).toString();

        //convert to native format
        strKeySequence = QKeySequence(strKeySequence).toString(QKeySequence::NativeText);

        if (m_treeModel->item(row , ACT_NAME_COLUMN) == nullptr)
            m_treeModel->setItem(row, ACT_NAME_COLUMN, new QStandardItem());
        if (m_treeModel->item(row, KEY_SEQ_COLUMN) == nullptr)
            m_treeModel->setItem(row, KEY_SEQ_COLUMN, new QStandardItem());

        m_treeModel->item(row, ACT_NAME_COLUMN)->setData(strCmdName);
        m_treeModel->item(row, ACT_NAME_COLUMN)->setText(strShortcutName);
        m_treeModel->item(row, ACT_NAME_COLUMN)->setEditable(false);
        m_treeModel->item(row, KEY_SEQ_COLUMN)->setText(strKeySequence);
        m_treeModel->item(row, KEY_SEQ_COLUMN)->setEditable(false);

        row++;
    }
    m_treeModel->setRowCount(row);
    settings.endGroup();

    ui->treeView->resizeColumnToContents( 0 );
}

void ShortcutsPage::clearButtonClicked()
{
    if ( !m_currentItemIndex.isValid() )
    {
        return;
    }

    if (!m_activeModel) {
        return;
    }

    int row = m_currentItemIndex.row();
    QStandardItem* actionItem = m_activeModel->item(row, ACT_NAME_COLUMN);

    QString strCmdName = QString("shortcuts/%1").arg( actionItem->data().toString() );

    QSettings setting( PENCIL2D, PENCIL2D );
    setting.setValue( strCmdName, "" );
    setting.sync();

    ui->keySequenceEdit->clear();

    treeModelLoadShortcutsSetting();
    refreshAfterSave();
}

void ShortcutsPage::cancelRemap()
{
    if (!m_remapIndex.isValid()) {
        return;
    }

    if (m_remapShortcutLabel) {
        m_remapShortcutLabel->setText(m_remapOriginalText);
    }
    if (m_remapShortcutContainer) {
        m_remapShortcutContainer->setProperty("remapping", false);
        m_remapShortcutContainer->style()->unpolish(m_remapShortcutContainer);
        m_remapShortcutContainer->style()->polish(m_remapShortcutContainer);
        m_remapShortcutContainer->update();
    }

    // Restore the original key in the editor field.
    QString parseable = m_remapOriginalText;
    parseable.replace(QStringLiteral(" + "), QStringLiteral("+"));
    m_settingKeySequenceProgrammatically = true;
    ui->keySequenceEdit->setKeySequence(QKeySequence(parseable));
    m_settingKeySequenceProgrammatically = false;

    m_remapModel = nullptr;
    m_remapIndex = QModelIndex();
    m_remapRow.clear();
    m_remapShortcutContainer.clear();
    m_remapShortcutLabel.clear();
    m_remapOriginalText.clear();
}

bool ShortcutsPage::eventFilter(QObject* watched, QEvent* event)
{
    // While remapping shortcuts, capture keystrokes directly from the editor so the UI updates live.
    if (ui && watched == ui->keySequenceEdit && event && event->type() == QEvent::KeyPress) {
        if (m_remapIndex.isValid() && m_activeModel == m_remapModel && m_remapShortcutLabel) {
            auto* ke = static_cast<QKeyEvent*>(event);
            const int key = ke->key();

            // Esc cancels remapping. It should never be set as a shortcut.
            if (key == Qt::Key_Escape) {
                cancelRemap();
                return true;
            }

            const bool keyIsModifier = (key == Qt::Key_Control)
                || (key == Qt::Key_Shift)
                || (key == Qt::Key_Alt)
                || (key == Qt::Key_Meta)
                || (key == Qt::Key_AltGr);

            if (keyIsModifier || key == 0) {
                m_remapShortcutLabel->setText(QStringLiteral("..."));
                return true;
            }

            Qt::KeyboardModifiers mods = ke->modifiers();
            mods &= (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
            const int sanitized = int(mods) | key;
            const QKeySequence sanitizedSeq(sanitized);

            QString t = sanitizedSeq.toString(QKeySequence::NativeText);
            if (t.trimmed().isEmpty()) {
                t = QStringLiteral("...");
            }
            t.replace('+', QStringLiteral(" + "));
            m_remapShortcutLabel->setText(t);

            m_settingKeySequenceProgrammatically = true;
            ui->keySequenceEdit->setKeySequence(sanitizedSeq);
            m_settingKeySequenceProgrammatically = false;

            QTimer::singleShot(0, this, [this]() {
                if (!m_remapIndex.isValid()) {
                    return;
                }
                if (m_activeModel != m_remapModel) {
                    return;
                }
                if (!m_currentItemIndex.isValid() || m_currentItemIndex.row() != m_remapIndex.row()) {
                    return;
                }
                if (ui && ui->keySequenceEdit && !ui->keySequenceEdit->keySequence().isEmpty()) {
                    keyCapLineEditTextChanged();
                }
            });

            return true;
        }
    }

    // Cancel remap when the key sequence editor loses focus to something outside a shortcut row.
    if (ui && watched == ui->keySequenceEdit && event && event->type() == QEvent::FocusOut) {
        if (m_remapIndex.isValid()) {
            auto* fe = static_cast<QFocusEvent*>(event);
            // Only cancel on a genuine user interaction (mouse click, tab, etc.),
            // not on programmatic focus changes we trigger ourselves.
            if (fe->reason() != Qt::ActiveWindowFocusReason && fe->reason() != Qt::OtherFocusReason) {
                // Check if focus is moving to a widget that is part of a shortcut row.
                // If so, the MouseButtonPress handler will start the new remap — don't cancel here.
                QWidget* focusDest = QApplication::focusWidget();
                bool goingToShortcutRow = focusDest && m_shortcutRowIndex.contains(focusDest);
                if (!goingToShortcutRow) {
                    cancelRemap();
                }
            }
        }
    }

    if (event && event->type() == QEvent::MouseButtonPress) {
        auto it = m_shortcutRowIndex.constFind(watched);
        if (it != m_shortcutRowIndex.constEnd()) {
            const QModelIndex idx = it.value();
            QStandardItemModel* model = m_shortcutRowModel.value(watched, nullptr);
            if (!model) {
                return true;
            }

            // If already remapping a different row, cancel it first then start the new one.
            if (m_remapIndex.isValid() && (model != m_remapModel || idx.row() != m_remapIndex.row())) {
                cancelRemap();
            }

            setActiveShortcutModel(model, idx);

            QWidget* w = qobject_cast<QWidget*>(watched);
            while (w && w->objectName() != QStringLiteral("shortcutRow") && w->objectName() != QStringLiteral("fileShortcutRow")) {
                w = qobject_cast<QWidget*>(w->parentWidget());
            }
            if (!w) {
                return true;
            }

            m_remapModel = model;
            m_remapIndex = idx;
            m_remapRow = w;

            // Find generic widget IDs first; fall back to legacy file-only IDs.
            m_remapShortcutLabel = w->findChild<QLabel*>(QStringLiteral("shortcutLabel"));
            if (!m_remapShortcutLabel) {
                m_remapShortcutLabel = w->findChild<QLabel*>(QStringLiteral("fileShortcutLabel"));
            }
            m_remapShortcutContainer = w->findChild<QWidget*>(QStringLiteral("shortcutContainer"));
            if (!m_remapShortcutContainer) {
                m_remapShortcutContainer = w->findChild<QWidget*>(QStringLiteral("fileShortcutContainer"));
            }

            if (m_remapShortcutLabel) {
                m_remapOriginalText = m_remapShortcutLabel->text();
                m_remapShortcutLabel->setText(QStringLiteral("..."));
            } else {
                m_remapOriginalText.clear();
            }

            // Start remapping from an empty editor state so "..." transitions immediately as the user types.
            m_settingKeySequenceProgrammatically = true;
            ui->keySequenceEdit->setKeySequence(QKeySequence());
            m_settingKeySequenceProgrammatically = false;
            ui->keySequenceEdit->setFocus();

            if (m_remapShortcutContainer) {
                m_remapShortcutContainer->setProperty("remapping", true);
                m_remapShortcutContainer->style()->unpolish(m_remapShortcutContainer);
                m_remapShortcutContainer->style()->polish(m_remapShortcutContainer);
                m_remapShortcutContainer->update();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ShortcutsPage::onSearchTextChanged(const QString& text)
{
    const QString q = text.trimmed();

    if (!ui->shortcutsMainStack || !ui->shortcutsNormalPage || !ui->shortcutsSearchPage) {
        return;
    }

    // Always cancel any in-progress remap when the search changes — the row
    // widgets are about to be destroyed and rebuilt, so any live remap state
    // would point at dangling pointers.
    cancelRemap();

    if (q.isEmpty()) {
        // Switching back to the normal page: clear search row hashes so the
        // now-deleted search result widgets don't linger as stale entries.
        m_shortcutRowIndex.clear();
        m_shortcutRowModel.clear();
        ui->shortcutsMainStack->setCurrentWidget(ui->shortcutsNormalPage);
        if (ui->searchResultsListLayout) {
            QLayoutItem* it = nullptr;
            while ((it = ui->searchResultsListLayout->takeAt(0))) {
                if (QWidget* w = it->widget()) {
                    w->deleteLater();
                }
                delete it;
            }
        }
        return;
    }

    ui->shortcutsMainStack->setCurrentWidget(ui->shortcutsSearchPage);

    QStringList matchedCmds;
    {
        QSettings s(PENCIL2D, PENCIL2D);
        s.beginGroup(QStringLiteral("shortcuts"));
        const QStringList keys = s.allKeys();
        for (const QString& cmd : keys) {
            const QString action = getHumanReadableShortcutName(cmd);
            const QString seq = QKeySequence(s.value(cmd).toString()).toString(QKeySequence::NativeText);
            if (action.contains(q, Qt::CaseInsensitive) || seq.contains(q, Qt::CaseInsensitive)) {
                matchedCmds.append(cmd);
            }
        }
        s.endGroup();
    }

    // Clear stale search-result row mappings before rebuilding.
    m_shortcutRowIndex.clear();
    m_shortcutRowModel.clear();

    if (ui->searchResultsListLayout) {
        QLayoutItem* it = nullptr;
        while ((it = ui->searchResultsListLayout->takeAt(0))) {
            if (QWidget* w = it->widget()) {
                w->deleteLater();
            }
            delete it;
        }
    }

    if (ui->searchResultsListLayout) {
        loadCategoryModel(m_searchResultsTreeModel, matchedCmds);
        rebuildCategoryShortcutsList(m_searchResultsTreeModel, ui->searchResultsListLayout);
    }
}

/**
 * Translates internal to human-readable shortcut names
 *
 * @param[in] cmdName The name of the setting corresponding to the shortcut
 * @return The translated, human-readable name of the shortcut
 */
static QString getHumanReadableShortcutName(const QString& cmdName)
{
    if (cmdName.startsWith(QStringLiteral("CmdControlCenterToolbarAction_"))) {
        QSettings s(PENCIL2D, PENCIL2D);
        const QString metaKey = QStringLiteral("shortcutNames/") + cmdName;
        const QString t = s.value(metaKey).toString();
        if (!t.trimmed().isEmpty()) {
            return t;
        }
        return cmdName;
    }

    static QHash<QString, QString> humanReadableShortcutNames = QHash<QString, QString>{
        {CMD_ADD_FRAME, ShortcutsPage::tr("Add Frame", "Shortcut")},
        {CMD_CLEAR_FRAME, ShortcutsPage::tr("Clear Frame", "Shortcut")},
        {CMD_COPY, ShortcutsPage::tr("Copy", "Shortcut")},
        {CMD_PASTE_FROM_PREVIOUS, ShortcutsPage::tr("Paste from Previous Keyframe", "Shortcut")},
        {CMD_CUT, ShortcutsPage::tr("Cut", "Shortcut")},
        {CMD_DELETE_CUR_LAYER, ShortcutsPage::tr("Delete Current Layer", "Shortcut")},
        {CMD_DESELECT_ALL, ShortcutsPage::tr("Deselect All", "Shortcut")},
        {CMD_DUPLICATE_FRAME, ShortcutsPage::tr("Duplicate Frame", "Shortcut")},
        {CMD_EXIT, ShortcutsPage::tr("Exit", "Shortcut")},
        {CMD_EXPORT_IMAGE, ShortcutsPage::tr("Export Image", "Shortcut")},
        {CMD_EXPORT_IMAGE_SEQ, ShortcutsPage::tr("Export Image Sequence", "Shortcut")},
        {CMD_EXPORT_MOVIE, ShortcutsPage::tr("Export Movie", "Shortcut")},
        {CMD_EXPORT_GIF, ShortcutsPage::tr("Export Animated GIF", "Shortcut")},
        {CMD_EXPORT_SPRITE, ShortcutsPage::tr("Export Sprite Sheet", "Shortcut")},
        {CMD_EXPORT_PALETTE, ShortcutsPage::tr("Export Palette", "Shortcut")},
        {CMD_EXPORT_ANIMATION, ShortcutsPage::tr("Export Animation", "Shortcut")},
        {CMD_FLIP_HORIZONTAL, ShortcutsPage::tr("View: Horizontal Flip", "Shortcut")},
        {CMD_FLIP_VERTICAL, ShortcutsPage::tr("View: Vertical Flip", "Shortcut")},
        {CMD_FLIP_INBETWEEN, ShortcutsPage::tr("Flip In-Between", "Shortcut")},
        {CMD_FLIP_ROLLING, ShortcutsPage::tr("Flip Rolling", "Shortcut")},
        {CMD_GOTO_NEXT_FRAME, ShortcutsPage::tr("Next Frame", "Shortcut")},
        {CMD_GOTO_NEXT_KEY_FRAME, ShortcutsPage::tr("Next Keyframe", "Shortcut")},
        {CMD_GOTO_PREV_FRAME, ShortcutsPage::tr("Previous Frame", "Shortcut")},
        {CMD_SELECTION_FLIP_HORIZONTAL, ShortcutsPage::tr("Selection: Horizontal Flip", "Shortcut")},
        {CMD_SELECTION_FLIP_VERTICAL, ShortcutsPage::tr("Selection: Vertical Flip", "Shortcut")},
        {CMD_GOTO_PREV_KEY_FRAME, ShortcutsPage::tr("Previous Keyframe", "Shortcut")},
        {CMD_SELECTION_REPOSITION_FRAMES, ShortcutsPage::tr("Selection: Reposition Frames", "Shortcut")},
        {CMD_SELECTION_ADD_FRAME_EXPOSURE, ShortcutsPage::tr("Selection: Add Frame Exposure", "Shortcut")},
        {CMD_SELECTION_SUBTRACT_FRAME_EXPOSURE, ShortcutsPage::tr("Selection: Subtract Frame Exposure", "Shortcut")},
        {CMD_REVERSE_SELECTED_FRAMES, ShortcutsPage::tr("Selection: Reverse Keyframes", "Shortcut")},
        {CMD_REMOVE_SELECTED_FRAMES, ShortcutsPage::tr("Selection: Remove Keyframes", "Shortcut")},
        {CMD_OVERLAY_CENTER, ShortcutsPage::tr("Toggle Center Overlay", "Shortcut")},
        {CMD_OVERLAY_THIRDS, ShortcutsPage::tr("Toggle Thirds Overlay", "Shortcut")},
        {CMD_OVERLAY_GOLDEN_RATIO, ShortcutsPage::tr("Toggle Golden Ratio Overlay", "Shortcut")},
        {CMD_OVERLAY_SAFE_AREAS, ShortcutsPage::tr("Toggle Safe Areas Overlay", "Shortcut")},
        {CMD_IMPORT_IMAGE, ShortcutsPage::tr("Import Image", "Shortcut")},
        {CMD_IMPORT_IMAGE_SEQ, ShortcutsPage::tr("Import Image Sequence", "Shortcut")},
        {CMD_IMPORT_IMAGE_PREDEFINED_SET, ShortcutsPage::tr("Import Image Predefined Set", "Shortcut")},
        {CMD_IMPORT_MOVIE_VIDEO, ShortcutsPage::tr("Import Movie Video", "Shortcut")},
        {CMD_IMPORT_MOVIE_AUDIO, ShortcutsPage::tr("Import Movie Audio", "Shortcut")},
        {CMD_IMPORT_ANIMATED_IMAGE, ShortcutsPage::tr("Import Animated Image", "Shortcut")},
        {CMD_IMPORT_LAYERS, ShortcutsPage::tr("Import Layers from project file", "Shortcut")},
        {CMD_IMPORT_PALETTE, ShortcutsPage::tr("Import Palette (Append)", "Shortcut")},
        {CMD_IMPORT_PALETTE_REPLACE, ShortcutsPage::tr("Import Palette (Replace)", "Shortcut")},
        {CMD_IMPORT_SOUND, ShortcutsPage::tr("Import Sound", "Shortcut")},
        {CMD_ALL_LAYER_VISIBILITY, ShortcutsPage::tr("Show All Layers", "Shortcut")},
        {CMD_CURRENT_LAYER_VISIBILITY, ShortcutsPage::tr("Show Current Layer Only", "Shortcut")},
        {CMD_RELATIVE_LAYER_VISIBILITY, ShortcutsPage::tr("Show Layers Relative to Current Layer", "Shortcut")},
        {CMD_LOOP, ShortcutsPage::tr("Toggle Loop", "Shortcut")},
        {CMD_LOOP_CONTROL, ShortcutsPage::tr("Toggle Range Playback", "Shortcut")},
        {CMD_MOVE_FRAME_BACKWARD, ShortcutsPage::tr("Move Frame Backward", "Shortcut")},
        {CMD_MOVE_FRAME_FORWARD, ShortcutsPage::tr("Move Frame Forward", "Shortcut")},
        // Retime
        {CMD_RETIME_1S,         ShortcutsPage::tr("Retime: 1s (Selected Only)", "Shortcut")},
        {CMD_RETIME_2S,         ShortcutsPage::tr("Retime: 2s (Selected Only)", "Shortcut")},
        {CMD_RETIME_3S,         ShortcutsPage::tr("Retime: 3s (Selected Only)", "Shortcut")},
        {CMD_RETIME_1S_RIPPLE,  ShortcutsPage::tr("Retime: 1s + Ripple", "Shortcut")},
        {CMD_RETIME_2S_RIPPLE,  ShortcutsPage::tr("Retime: 2s + Ripple", "Shortcut")},
        {CMD_RETIME_3S_RIPPLE,  ShortcutsPage::tr("Retime: 3s + Ripple", "Shortcut")},
        {CMD_MAKE_UNIQUE_DRAWING, ShortcutsPage::tr("Make Drawing Unique", "Shortcut")},
        // Layer navigation (timeline + papers pane)
        {CMD_SELECT_LAYER_ABOVE, ShortcutsPage::tr("Select Layer Above", "Shortcut")},
        {CMD_SELECT_LAYER_BELOW, ShortcutsPage::tr("Select Layer Below", "Shortcut")},
        // Papers pane
        {CMD_PAPERS_NEW,          ShortcutsPage::tr("Papers: New Paper", "Shortcut")},
        {CMD_PAPERS_REMOVE,       ShortcutsPage::tr("Papers: Remove Paper", "Shortcut")},
        {CMD_PAPERS_IMPORT,       ShortcutsPage::tr("Papers: Import Image", "Shortcut")},
        {CMD_PAPERS_SELECT_ABOVE, ShortcutsPage::tr("Papers: Select Paper Above", "Shortcut")},
        {CMD_PAPERS_SELECT_BELOW, ShortcutsPage::tr("Papers: Select Paper Below", "Shortcut")},
        {CMD_NEW_BITMAP_LAYER, ShortcutsPage::tr("New Bitmap Layer", "Shortcut")},
        {CMD_NEW_FILE, ShortcutsPage::tr("New File", "Shortcut")},
        {CMD_NEW_SOUND_LAYER, ShortcutsPage::tr("New Sound Layer", "Shortcut")},
        {CMD_ONIONSKIN_NEXT, ShortcutsPage::tr("Toggle Next Onion Skin", "Shortcut")},
        {CMD_ONIONSKIN_PREV, ShortcutsPage::tr("Toggle Previous Onion Skin", "Shortcut")},
        {CMD_OPEN_FILE, ShortcutsPage::tr("Open File", "Shortcut")},
        {CMD_PASTE, ShortcutsPage::tr("Paste", "Shortcut")},
        {CMD_PLAY, ShortcutsPage::tr("Play/Stop", "Shortcut")},
        {CMD_PEGBAR_ALIGNMENT, ShortcutsPage::tr("Peg bar Alignment", "Shortcut")},
        {CMD_PREFERENCE, ShortcutsPage::tr("Preferences", "Shortcut")},
        {CMD_REDO, ShortcutsPage::tr("Redo", "Shortcut")},
        {CMD_REMOVE_FRAME, ShortcutsPage::tr("Remove Frame", "Shortcut")},
        {CMD_SELECTION_FREE_TRANSFORM, ShortcutsPage::tr("Selection: Free Transform", "Shortcut")},
        {CMD_RESET_ZOOM_ROTATE, ShortcutsPage::tr("Reset View", "Shortcut")},
        {CMD_CENTER_VIEW, ShortcutsPage::tr("Center View", "Shortcut")},
        {CMD_PAN_VIEW, ShortcutsPage::tr("Pan View", "Shortcut")},
        {CMD_TEMP_PAN_HOLD, ShortcutsPage::tr("Hold: Pan (Space)", "Shortcut")},
        {CMD_TEMP_ZOOM_HOLD, ShortcutsPage::tr("Hold: Zoom (Ctrl+Space)", "Shortcut")},
        {CMD_TEMP_ROTATE_HOLD, ShortcutsPage::tr("Hold: Rotate (Shift+Space)", "Shortcut")},
        {CMD_TEMP_RECT_GAP_FILL, ShortcutsPage::tr("Hold: Rect Gap Fill", "Shortcut")},
        {CMD_TEMP_LASSO_GAP_FILL, ShortcutsPage::tr("Hold: Lasso Gap Fill", "Shortcut")},
        {CMD_TEMP_TOOL_PEN, ShortcutsPage::tr("Hold: Pen Tool", "Shortcut")},
        {CMD_TEMP_TOOL_ERASER, ShortcutsPage::tr("Hold: Eraser Tool", "Shortcut")},
        {CMD_TEMP_TOOL_POLYLINE, ShortcutsPage::tr("Hold: Polyline Tool", "Shortcut")},
        {CMD_TEMP_TOOL_CURVE, ShortcutsPage::tr("Hold: Curve Tool", "Shortcut")},
        {CMD_TEMP_TOOL_BSPLINE, ShortcutsPage::tr("Hold: B-Spline Tool", "Shortcut")},
        {CMD_TEMP_TOOL_BUCKET, ShortcutsPage::tr("Hold: Bucket Tool", "Shortcut")},
        {CMD_TEMP_TOOL_EYEDROPPER, ShortcutsPage::tr("Hold: Eyedropper Tool", "Shortcut")},
        {CMD_TEMP_TOOL_DPLATE, ShortcutsPage::tr("Hold: Dplate Tool", "Shortcut")},
        {CMD_TEMP_TOOL_HAND, ShortcutsPage::tr("Hold: Hand Tool", "Shortcut")},
        {CMD_TEMP_TOOL_CAMERA, ShortcutsPage::tr("Hold: Camera Tool", "Shortcut")},
        {CMD_TEMP_TOOL_LASSO_SELECT, ShortcutsPage::tr("Hold: Lasso Select Tool", "Shortcut")},
        {CMD_TEMP_RECT_GAP_FILL, ShortcutsPage::tr("Hold: Rect Gap Fill", "Shortcut")},
        {CMD_TEMP_LASSO_GAP_FILL, ShortcutsPage::tr("Hold: Lasso Gap Fill", "Shortcut")},
        {CMD_ROTATE_ANTI_CLOCK, ShortcutsPage::tr("Rotate Anticlockwise", "Shortcut")},
        {CMD_ROTATE_CLOCK, ShortcutsPage::tr("Rotate Clockwise", "Shortcut")},
        {CMD_RESET_ROTATION, ShortcutsPage::tr("Reset Rotation", "Shortcut")},
        {CMD_SAVE_AS, ShortcutsPage::tr("Save File As", "Shortcut")},
        {CMD_SAVE_FILE, ShortcutsPage::tr("Save File", "Shortcut")},
        {CMD_SELECT_ALL, ShortcutsPage::tr("Select All", "Shortcut")},
        {CMD_TOGGLE_COLOR_LIBRARY, ShortcutsPage::tr("Toggle Color Palette Window Visibility", "Shortcut")},
        {CMD_POPUP_COLOR_WHEEL, ShortcutsPage::tr("Popup Color Wheel", "Shortcut")},
        {CMD_TOGGLE_GAP_CHECK, ShortcutsPage::tr("Toggle Gap Checking", "Shortcut")},
        {CMD_HELP, ShortcutsPage::tr("Help", "Shortcut")},
        {CMD_INCREASE_SIZE, ShortcutsPage::tr("Increase Size", "Shortcut")},
        {CMD_DECREASE_SIZE, ShortcutsPage::tr("Decrease Size", "Shortcut")},
        {CMD_INCREASE_OPACITY, ShortcutsPage::tr("Increase Opacity", "Shortcut")},
        {CMD_DECREASE_OPACITY, ShortcutsPage::tr("Decrease Opacity", "Shortcut")},
        {CMD_INCREASE_SMOOTH, ShortcutsPage::tr("Increase Smooth", "Shortcut")},
        {CMD_DECREASE_SMOOTH, ShortcutsPage::tr("Decrease Smooth", "Shortcut")},
        {CMD_NEW_VECTOR_LAYER, ShortcutsPage::tr("New Vector Layer", "Shortcut")},
        {CMD_NEW_CAMERA_LAYER, ShortcutsPage::tr("New Camera Layer", "Shortcut")},

        {CMD_COSMIC_BOTTOM_TIMELINE_LOCK_TOGGLE, ShortcutsPage::tr("Cosmic Timeline Panel: Toggle Lock", "Shortcut")},

        {CMD_COSMIC_LEFT_LOCK_TOGGLE, ShortcutsPage::tr("Cosmic Left Toolpanel: Toggle Lock", "Shortcut")},
        {CMD_COSMIC_RIGHT_LOCK_TOGGLE, ShortcutsPage::tr("Cosmic Right Toolpanel: Toggle Lock", "Shortcut")},
        {CMD_COSMIC_PANE_TOGGLE_TOOLS, ShortcutsPage::tr("Cosmic Pane: Toggle Tools", "Shortcut")},
        {CMD_COSMIC_PANE_TOGGLE_TOOLS_OPTIONS, ShortcutsPage::tr("Cosmic Pane: Toggle Tools Options", "Shortcut")},
        {CMD_COSMIC_PANE_TOGGLE_BRUSHES, ShortcutsPage::tr("Cosmic Pane: Toggle Brushes", "Shortcut")},
        {CMD_COSMIC_PANE_TOGGLE_COLOR_WHEEL, ShortcutsPage::tr("Cosmic Pane: Toggle Color Wheel", "Shortcut")},
        {CMD_COSMIC_PANE_TOGGLE_PALETTES, ShortcutsPage::tr("Cosmic Pane: Toggle Palettes", "Shortcut")},
        {CMD_COSMIC_PANE_TOGGLE_REFERENCE, ShortcutsPage::tr("Cosmic Pane: Toggle Reference", "Shortcut")},

        {CMD_CONTROL_CENTER_PEN_PALETTE_1, ShortcutsPage::tr("Pen Palette: Current", "Shortcut")},
        {CMD_CONTROL_CENTER_PEN_PALETTE_2, ShortcutsPage::tr("Pen Palette: Red", "Shortcut")},
        {CMD_CONTROL_CENTER_PEN_PALETTE_3, ShortcutsPage::tr("Pen Palette: Green", "Shortcut")},
        {CMD_CONTROL_CENTER_PEN_PALETTE_4, ShortcutsPage::tr("Pen Palette: Blue", "Shortcut")},
        {CMD_CONTROL_CENTER_PEN_PALETTE_5, ShortcutsPage::tr("Pen Palette: Yellow", "Shortcut")},
        {CMD_CONTROL_CENTER_PEN_PALETTE_6, ShortcutsPage::tr("Pen Palette: Magenta", "Shortcut")},
        {CMD_CONTROL_CENTER_AUTOFILL_MASTER, ShortcutsPage::tr("AutoFill: Global Toggle", "Shortcut")},
        {CMD_CONTROL_CENTER_AUTOFILL_COLOR_1, ShortcutsPage::tr("AutoFill: Red", "Shortcut")},
        {CMD_CONTROL_CENTER_AUTOFILL_COLOR_2, ShortcutsPage::tr("AutoFill: Green", "Shortcut")},
        {CMD_CONTROL_CENTER_AUTOFILL_COLOR_3, ShortcutsPage::tr("AutoFill: Blue", "Shortcut")},
        {CMD_CONTROL_CENTER_AUTOFILL_COLOR_4, ShortcutsPage::tr("AutoFill: Yellow", "Shortcut")},
        {CMD_CONTROL_CENTER_AUTOFILL_COLOR_5, ShortcutsPage::tr("AutoFill: Magenta", "Shortcut")},
        {CMD_CC_TOGGLE_FILE_OPS,    ShortcutsPage::tr("Toolbar: File Operations", "Shortcut")},
        {CMD_CC_TOGGLE_VIEW,        ShortcutsPage::tr("Toolbar: View Controls", "Shortcut")},
        {CMD_CC_TOGGLE_OVERLAYS,    ShortcutsPage::tr("Toolbar: Overlays", "Shortcut")},
        {CMD_CC_TOGGLE_UNDO_REDO,   ShortcutsPage::tr("Toolbar: Undo / Redo", "Shortcut")},
        {CMD_CC_TOGGLE_CLEAR_FRAME, ShortcutsPage::tr("Toolbar: Clear Frame", "Shortcut")},
        {CMD_TOOL_BRUSH, ShortcutsPage::tr("Brush Tool", "Shortcut")},
        {CMD_TOOL_BUCKET, ShortcutsPage::tr("Bucket Tool", "Shortcut")},
        {CMD_TOOL_RECT_GAP_FILL, ShortcutsPage::tr("Rect Gap Fill Tool", "Shortcut")},
        {CMD_TOOL_LASSO_GAP_FILL, ShortcutsPage::tr("Lasso Gap Fill Tool", "Shortcut")},
        {CMD_TOOL_DPLATE, ShortcutsPage::tr("Dplate Tool", "Shortcut")},
        {CMD_TOOL_ERASER, ShortcutsPage::tr("Eraser Tool", "Shortcut")},
        {CMD_TOOL_EYEDROPPER, ShortcutsPage::tr("Eyedropper Tool", "Shortcut")},
        {CMD_TOOL_HAND, ShortcutsPage::tr("Hand Tool", "Shortcut")},
        {CMD_TOOL_PEN, ShortcutsPage::tr("Pen Tool", "Shortcut")},
        {CMD_TOOL_PENCIL, ShortcutsPage::tr("Pencil Tool", "Shortcut")},
        {CMD_TOOL_POLYLINE, ShortcutsPage::tr("Polyline Tool", "Shortcut")},
        {CMD_TOOL_CURVE, ShortcutsPage::tr("Curve Tool", "Shortcut")},
        {CMD_TOOL_BSPLINE, ShortcutsPage::tr("B-Spline Tool", "Shortcut")},
        {CMD_TOOL_LASSO_SELECT, ShortcutsPage::tr("Lasso Select Tool", "Shortcut")},
        {CMD_TOOL_SMUDGE, ShortcutsPage::tr("Smudge Tool", "Shortcut")},
        {CMD_RESET_ALL_TOOLS, ShortcutsPage::tr("Reset all tools to default", "Shortcut")},
        {CMD_CHANGE_LINE_COLOR_KEYFRAME, ShortcutsPage::tr("Change Line Color (Current keyframe)", "Shortcut")},
        {CMD_CHANGE_LINE_COLOR_LAYER, ShortcutsPage::tr("Change Line Color (All keyframes on layer)", "Shortcut")},
        {CMD_CHANGE_LAYER_OPACITY, ShortcutsPage::tr("Change Layer / Keyframe Opacity", "Shortcut")},
        {CMD_UNDO, ShortcutsPage::tr("Undo", "Shortcut")},
        {CMD_ZOOM_100, ShortcutsPage::tr("Set Zoom to 100%", "Shortcut")},
        {CMD_ZOOM_200, ShortcutsPage::tr("Set Zoom to 200%", "Shortcut")},
        {CMD_ZOOM_25, ShortcutsPage::tr("Set Zoom to 25%", "Shortcut")},
        {CMD_ZOOM_300, ShortcutsPage::tr("Set Zoom to 300%", "Shortcut")},
        {CMD_ZOOM_33, ShortcutsPage::tr("Set Zoom to 33%", "Shortcut")},
        {CMD_ZOOM_400, ShortcutsPage::tr("Set Zoom to 400%", "Shortcut")},
        {CMD_ZOOM_50, ShortcutsPage::tr("Set Zoom to 50%", "Shortcut")},
        {CMD_ZOOM_IN, ShortcutsPage::tr("Zoom In", "Shortcut")},
        {CMD_ZOOM_OUT, ShortcutsPage::tr("Zoom Out", "Shortcut")},
        {CMD_CANVAS_SIZE,         ShortcutsPage::tr("Canvas: Set Canvas Size", "Shortcut")},
        {CMD_CANVAS_COLOR,        ShortcutsPage::tr("Canvas: Set Canvas Color", "Shortcut")},
        {CMD_CANVAS_EXTRA_SPACE,  ShortcutsPage::tr("Canvas: Set Extra Space", "Shortcut")},
        {CMD_CANVAS_SHOW_PALETTE, ShortcutsPage::tr("Canvas: Toggle Color Palette", "Shortcut")},
    };

    return humanReadableShortcutNames.value(cmdName, QString());
}

void ShortcutsPage::updateHighlightColor()
{
    if (mManager) {
        int accentRgba = mManager->getInt(SETTING::ACCENT_COLOR);
        mAccentColor = QColor::fromRgba(accentRgba);
        
        // Connect to preference changes if not already connected
        connect(mManager, &PreferenceManager::optionChanged, this, &ShortcutsPage::onAccentColorChanged, Qt::UniqueConnection);
    }
    
    const QString style = QStringLiteral(
        "QTreeView { color: %1; selection-background-color: transparent; selection-color: %1; }"
        "QTreeView::item { color: %1; }"
        "QTreeView::item:selected { background: %2; color: #ffffff; }"
        "QTreeView::item:selected:active { background: %2; color: #ffffff; }"
        "QTreeView::item:selected:!active { background: %2; color: #ffffff; }"
    ).arg(CosmicText::colorHex()).arg(mAccentColor.name(QColor::HexRgb));

    // Apply the highlight color
    ui->treeView->setStyleSheet(style);
}

void ShortcutsPage::onAccentColorChanged()
{
    if (mManager) {
        int accentRgba = mManager->getInt(SETTING::ACCENT_COLOR);
        QColor newAccentColor = QColor::fromRgba(accentRgba);
        
        if (newAccentColor != mAccentColor) {
            mAccentColor = newAccentColor;
            updateHighlightColor();
        }
    }
}
