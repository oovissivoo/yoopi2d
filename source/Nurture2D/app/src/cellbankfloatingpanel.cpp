#include "cellbankfloatingpanel.h"

#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicuimetrics.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QFrame>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QDebug>
#include <QRegularExpression>
#include <QMap>
#include <QSet>

#include "editor.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "cellbank.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "scribblearea.h"
#include "undoredomanager.h"
#include "cosmicconfirmpopup.h"

static const int kPanelMinW = 260;
static const int kPanelMinH = 180;
static const int kRadius    = 10;

CellBankFloatingPanel::CellBankFloatingPanel(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMinimumSize(kPanelMinW, kPanelMinH);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setWindowTitle(tr("Cell Bank"));
}

CellBankFloatingPanel::~CellBankFloatingPanel() = default;

void CellBankFloatingPanel::setEditor(Editor* editor)
{
    mEditor = editor;
}

void CellBankFloatingPanel::initUI()
{
    if (mInitialized) return;
    mInitialized = true;

    if (mEditor) {
        connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, &CellBankFloatingPanel::refreshStack);
        connect(mEditor, &Editor::scrubbed,        this, &CellBankFloatingPanel::refreshStack);
        connect(mEditor, &Editor::framesModified,  this, &CellBankFloatingPanel::refreshStack);
        connect(mEditor, &Editor::updateTimeLine,  this, &CellBankFloatingPanel::refreshStack);
    }

    // Outer layout with padding for the painted border
    const int pad = 8;
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(pad, pad, pad, pad);
    outerLayout->setSpacing(0);

    // Title bar
    auto* titleBar = new QWidget(this);
    titleBar->setFixedHeight(26);
    titleBar->setAttribute(Qt::WA_NoSystemBackground, true);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(4, 0, 4, 0);
    titleLayout->setSpacing(0);
    auto* titleLabel = new QLabel(tr("Cell Bank"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #b8b8b8; background: transparent; font-size: 9pt; }"));
    titleLayout->addWidget(titleLabel, 1, Qt::AlignVCenter);
    mTitleLabel = titleLabel;
    outerLayout->addWidget(titleBar);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("QFrame { background: #3a3a3a; border: none; max-height: 1px; }"));
    outerLayout->addWidget(sep);
    outerLayout->addSpacing(4);

    // Body: splitter — actions (70%) left, list (30%) right
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #3a3a3a; }"));

    // --- Left panel (70%): header with Add/Remove/Cleanup, then empty body ---
    mLeftPanel = new QWidget(splitter);
    mLeftPanel->setAttribute(Qt::WA_NoSystemBackground, true);
    auto* leftLayout = new QVBoxLayout(mLeftPanel);
    leftLayout->setContentsMargins(4, 4, 2, 4);
    leftLayout->setSpacing(4);

    // Header strip: Add + Remove side by side
    auto* actionsHeader = new QWidget(mLeftPanel);
    actionsHeader->setAttribute(Qt::WA_NoSystemBackground, true);
    auto* actionsLayout = new QHBoxLayout(actionsHeader);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(4);

    auto makeBtn = [this](const QString& iconPath, const QString& tip) -> CosmicButton* {
        auto* b = new CosmicButton(mLeftPanel);
        b->setBordered(false);
        b->setFixedHeight(28);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setIcon(QIcon(iconPath));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        return b;
    };

    mAddButton     = makeBtn(QStringLiteral(":/Interaction/KeyframeAdd.svg"),    tr("Add new cell"));
    mRemoveButton  = makeBtn(QStringLiteral(":/Interaction/KeyframeRemove.svg"), tr("Remove selected cell"));

    // Cleanup: text-only, no icon, borderless
    mCleanupButton = new CosmicButton(tr("Remove Unused"), mLeftPanel);
    mCleanupButton->setBordered(false);
    mCleanupButton->setFixedHeight(28);
    mCleanupButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mCleanupButton->setToolTip(tr("Remove unused cells from bank"));

    actionsLayout->addWidget(mAddButton);
    actionsLayout->addWidget(mRemoveButton);
    actionsHeader->setLayout(actionsLayout);

    connect(mAddButton,    &QPushButton::clicked, this, &CellBankFloatingPanel::onAddCell);
    connect(mRemoveButton, &QPushButton::clicked, this, &CellBankFloatingPanel::onRemoveCell);
    connect(mCleanupButton, &QPushButton::clicked, this, [this]() {
        if (!mEditor) return;
        Layer* layer = mEditor->layers()->currentLayer();
        if (!layer || layer->type() != Layer::BITMAP) return;
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        CellBank* bank = bitmapLayer->cellBank();
        if (!bank) return;

        QSet<int> usedIds;
        bitmapLayer->foreachKeyFrame([&](KeyFrame* kf) {
            if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf))
                usedIds.insert(bkf->cellId());
        });

        QList<int> unusedIds;
        for (int id : bank->getAllCellIds())
            if (!usedIds.contains(id)) unusedIds.append(id);

        if (unusedIds.isEmpty()) return;

        CosmicConfirmPopup popup(this);
        popup.setTitleText(tr("Clean Up Unused Cells"));
        popup.setBodyText(tr("Remove %1 unused cell(s) from the bank? This cannot be undone.").arg(unusedIds.size()));
        popup.setConfirmText(tr("Remove"));
        popup.setCancelText(tr("Cancel"));
        popup.exec();
        if (popup.result() != QDialog::Accepted) return;

        for (int id : unusedIds) bank->removeCell(id);
        emit mEditor->framesModified();
        emit mEditor->updateTimeLine();
        refreshStack();
    });

    leftLayout->addWidget(actionsHeader);

    auto* headerSep = new QFrame(mLeftPanel);
    headerSep->setFrameShape(QFrame::HLine);
    headerSep->setStyleSheet(QStringLiteral("QFrame { background: #3a3a3a; border: none; max-height: 1px; }"));
    leftLayout->addWidget(headerSep);

    leftLayout->addWidget(mCleanupButton);

    // --- Rename buttons ---
    mRenameTimelineOrderButton = new CosmicButton(tr("Rename: Timeline"), mLeftPanel);
    mRenameTimelineOrderButton->setBordered(false);
    mRenameTimelineOrderButton->setFixedHeight(28);
    mRenameTimelineOrderButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mRenameTimelineOrderButton->setToolTip(tr("Rename cells 1,2,3... by first appearance on the timeline"));

    connect(mRenameTimelineOrderButton, &QPushButton::clicked, this, [this]() {
        if (!mEditor) return;
        Layer* layer = mEditor->layers()->currentLayer();
        if (!layer || layer->type() != Layer::BITMAP) return;
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        CellBank* bank = bitmapLayer->cellBank();
        if (!bank) return;

        const QString base = layer->name();

        // Walk timeline left-to-right, assign numbers on first encounter
        // foreachKeyFrame is descending (std::greater), so sort ascending first
        QList<KeyFrame*> frames;
        layer->foreachKeyFrame([&](KeyFrame* kf) { frames.append(kf); });
        std::sort(frames.begin(), frames.end(),
                  [](KeyFrame* a, KeyFrame* b) { return a->pos() < b->pos(); });

        QMap<int, int> cellToNum;
        int n = 1;
        for (KeyFrame* kf : frames) {
            if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf))
                if (!cellToNum.contains(bkf->cellId()))
                    cellToNum.insert(bkf->cellId(), n++);
        }
        // Cells not on the timeline get numbers after
        for (int id : bank->getAllCellIds())
            if (!cellToNum.contains(id))
                cellToNum.insert(id, n++);

        // Two-pass rename to avoid collisions
        for (int id : bank->getAllCellIds())
            bank->setCellName(id, QStringLiteral("__tmp_%1").arg(cellToNum[id]));
        for (int id : bank->getAllCellIds())
            bank->setCellName(id, QString("%1_%2").arg(base).arg(cellToNum[id], 3, 10, QChar('0')));

        // Stack reorders to match timeline sequence (same order as first appearances)
        QList<int> timelineOrder = bank->getAllCellIds();
        std::sort(timelineOrder.begin(), timelineOrder.end(), [&](int a, int b) {
            return cellToNum[a] < cellToNum[b];
        });
        bank->setCellOrder(timelineOrder);

        emit mEditor->framesModified();
        emit mEditor->updateTimeLine();
        refreshStack();
    });

    leftLayout->addWidget(mRenameTimelineOrderButton);
    leftLayout->addStretch(1);

    // --- Right panel (30%): cell list ---
    mRightPanel = new QWidget(splitter);
    mRightPanel->setAttribute(Qt::WA_NoSystemBackground, true);
    auto* rightLayout = new QVBoxLayout(mRightPanel);
    rightLayout->setContentsMargins(2, 4, 4, 4);
    rightLayout->setSpacing(0);

    mListWidget = new QListWidget(mRightPanel);
    mListWidget->setViewMode(QListView::ListMode);
    mListWidget->setMovement(QListView::Static);
    mListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mListWidget->setResizeMode(QListView::Adjust);
    mListWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    mListWidget->setDragEnabled(false);
    mListWidget->setAcceptDrops(false);
    mListWidget->setStyleSheet(QStringLiteral(
        "QListWidget { background: #1a1a1a; border: none; border-radius: 6px; }"
        "QListWidget::item:selected { border-radius:4px; background: palette(highlight); }"
        "QScrollBar:vertical { background:#1f1f1f; border:none; border-radius:4px; width:8px; margin:0; }"
        "QScrollBar::handle:vertical { background:#505050; border-radius:4px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background:transparent; height:0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }"));
    connect(mListWidget, &QListWidget::currentRowChanged, this, &CellBankFloatingPanel::onCellSelected);
    connect(mListWidget, &QListWidget::itemDoubleClicked, this, &CellBankFloatingPanel::onCellDoubleClicked);
    mListWidget->installEventFilter(this);
    mListViewport = mListWidget->viewport();
    if (mListViewport) mListViewport->installEventFilter(this);

    rightLayout->addWidget(mListWidget, 1);

    splitter->addWidget(mLeftPanel);
    splitter->addWidget(mRightPanel);
    // setSizes enforces the initial split; stretch factors alone don't set initial sizes
    splitter->setSizes({ 196, 84 }); // ~70/30 of the 280px default panel width

    outerLayout->addWidget(splitter, 1);
    setLayout(outerLayout);

    refreshStack();
}

void CellBankFloatingPanel::updateUI()
{
    refreshStack();
}

void CellBankFloatingPanel::repositionAbove(const QPoint& toggleBottomRight, int bottomBarTop)
{
    const int margin = 6;
    const int panelH = qMax(kPanelMinH, height());
    const int panelW = qMax(kPanelMinW, width());

    // Anchor bottom-right of panel to just above the toggle
    int x = toggleBottomRight.x() - panelW;
    int y = bottomBarTop - panelH - margin;

    // Clamp to parent bounds
    if (parentWidget()) {
        x = qMax(0, qMin(x, parentWidget()->width() - panelW));
        y = qMax(0, y);
    }

    setGeometry(x, y, panelW, panelH);
}

void CellBankFloatingPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF rc = rect().adjusted(1, 1, -1, -1);

    // Shadow-like outer stroke
    p.setPen(QPen(QColor(0, 0, 0, 120), 1.5));
    p.setBrush(QColor(0x2a, 0x2a, 0x2a, 0xf5));
    p.drawRoundedRect(rc, kRadius, kRadius);

    // Subtle inner highlight on top edge
    p.setPen(QPen(QColor(0xff, 0xff, 0xff, 18), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rc.adjusted(1, 1, -1, -1), kRadius - 1, kRadius - 1);
}

void CellBankFloatingPanel::refreshStack()
{
    if (!mEditor || !mListWidget) return;

    stopJiggle();
    bool blocked = mListWidget->blockSignals(true);

    int prevId = -1;
    if (mListWidget->currentItem())
        prevId = mListWidget->currentItem()->data(Qt::UserRole).toInt();

    mListWidget->clear();

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer && layer->type() == Layer::BITMAP) {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        CellBank* bank = bitmapLayer->cellBank();
        for (int id : bank->getAllCellIds()) {
            // Extract the numeric suffix from the cell name (e.g. "layer_004" -> "4")
            // Falls back to the raw id if the name has no recognisable number.
            QString rawName = bank->getCellName(id);
            static const QRegularExpression trailNum(QStringLiteral("(\\d+)$"));
            QRegularExpressionMatch m = trailNum.match(rawName);
            QString label;
            if (m.hasMatch()) {
                label = QString::number(m.captured(1).toInt()); // strips leading zeros
            } else {
                label = rawName.isEmpty() ? QString::number(id) : rawName;
            }
            auto* item = new QListWidgetItem(label, mListWidget);
            item->setData(Qt::UserRole, id);
            item->setTextAlignment(Qt::AlignCenter);
        }
    }

    bool restored = false;
    if (prevId != -1) {
        for (int i = 0; i < mListWidget->count(); ++i) {
            if (mListWidget->item(i)->data(Qt::UserRole).toInt() == prevId) {
                mListWidget->setCurrentRow(i);
                restored = true;
                break;
            }
        }
    }
    if (!restored && mListWidget->count() > 0)
        mListWidget->setCurrentRow(mListWidget->count() - 1);

    mListWidget->blockSignals(blocked);
}

void CellBankFloatingPanel::onCellSelected(int row)
{
    Q_UNUSED(row);
}

void CellBankFloatingPanel::onCellDoubleClicked(QListWidgetItem* item)
{
    if (item) startInlineRename(item);
}

void CellBankFloatingPanel::onAddCell()
{
    if (mEditor) mEditor->addNewKey();
}

void CellBankFloatingPanel::onRemoveCell()
{
    if (!mEditor || !mListWidget) return;
    stopJiggle();

    QListWidgetItem* item = mListWidget->currentItem();
    if (!item) return;
    int cellId = item->data(Qt::UserRole).toInt();

    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    QList<int> allIds = bank->getAllCellIds();
    if (allIds.size() <= 1) {
        // Clear last cell instead of deleting — keep its existing name
        if (BitmapImage* img = bank->getCell(cellId)) img->clear();
        if (QUndoStack* stack = bank->getOrCreateUndoStack(cellId)) stack->clear();
        // NOTE: intentionally not renaming here — cell keeps its original name
        // bank->setCellName(cellId, QStringLiteral("1"));
        emit mEditor->frameModified(mEditor->currentFrame());
        emit mEditor->framesModified();
        emit mEditor->updateTimeLine();
        refreshStack();
        if (mEditor->getScribbleArea()) mEditor->getScribbleArea()->update();
        return;
    }

    CosmicConfirmPopup popup(this);
    popup.setTitleText(tr("Delete Cell"));
    popup.setBodyText(tr("This will remove the cell from the bank and ALL references to it on the timeline. This action cannot be undone."));
    popup.setConfirmText(tr("Yes"));
    popup.setCancelText(tr("No"));
    popup.exec();
    if (popup.result() != QDialog::Accepted) return;

    if (UndoRedoManager* undoMgr = mEditor->undoRedo())
        undoMgr->setActiveCellStack(nullptr);

    QList<int> framesToRemove;
    bitmapLayer->foreachKeyFrame([&](KeyFrame* kf) {
        if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf))
            if (bkf->cellId() == cellId) framesToRemove.append(kf->pos());
    });
    for (int pos : framesToRemove) bitmapLayer->removeKeyFrame(pos);

    bank->removeCell(cellId);
    mEditor->updateUndoContext();
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
    emit mEditor->updateTimeLine();
    refreshStack();
    if (mEditor->getScribbleArea()) mEditor->getScribbleArea()->update();
}

void CellBankFloatingPanel::exposeCell(int cellId)
{
    if (!mEditor) return;
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    int currentFrame = mEditor->currentFrame();
    KeyFrame* kf = bitmapLayer->getKeyFrameAt(currentFrame);
    auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
    if (bkf && bkf->cellId() == cellId) return;

    auto* newKf = new BitmapKeyFrame(cellId);
    newKf->setPos(currentFrame);
    newKf->setCellBank(bitmapLayer->cellBank());

    bool changed = false;
    if (kf) {
        // Remove the existing keyframe and add the new one with the desired cell
        bitmapLayer->removeKeyFrame(currentFrame);
        changed = bitmapLayer->addKeyFrame(currentFrame, newKf);
    } else {
        changed = bitmapLayer->addKeyFrame(currentFrame, newKf);
    }
    if (changed) mEditor->updateUndoContext();

    mEditor->setModified(mEditor->layers()->currentLayerIndex(), currentFrame);
    mEditor->scrubTo(currentFrame);
    if (mEditor->getScribbleArea()) mEditor->getScribbleArea()->update();
}

void CellBankFloatingPanel::startInlineRename(QListWidgetItem* item)
{
    if (!item || mIsInlineEditing || !mListWidget || !mEditor) return;
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    int cellId = item->data(Qt::UserRole).toInt();
    int index = bank->indexForCellId(cellId);
    if (index <= 0) return;

    if (!mInlineEditor) {
        mInlineEditor = new QLineEdit(mListWidget);
        mInlineEditor->setFrame(false);
        mInlineEditor->setAlignment(Qt::AlignCenter);
        mInlineEditor->setStyleSheet(QStringLiteral(
            "QLineEdit { background:#3a3a3a; color:#e0e0e0; border:1px solid #6a4cff; border-radius:4px; padding:1px; }"));
        mInlineEditor->installEventFilter(this);
    }

    mEditingCellId = cellId;
    mIsInlineEditing = true;
    mInlineEditor->setText(QString::number(index));
    QRect r = mListWidget->visualItemRect(item);
    const QFontMetrics fm(mInlineEditor->font());
    const int editH = fm.height() + 4;
    mInlineEditor->setGeometry(r.left() + 4, r.top() + (r.height() - editH) / 2, r.width() - 8, editH);
    mInlineEditor->show();
    mInlineEditor->setFocus(Qt::OtherFocusReason);
    mInlineEditor->selectAll();
}

void CellBankFloatingPanel::finishInlineRename(bool accept)
{
    if (!mIsInlineEditing) return;
    if (mInlineEditor) mInlineEditor->hide();

    if (accept && mInlineEditor && mEditor) {
        bool ok = false;
        int targetIndex = mInlineEditor->text().trimmed().toInt(&ok);
        if (ok && targetIndex > 0) {
            Layer* layer = mEditor->layers()->currentLayer();
            if (layer && layer->type() == Layer::BITMAP) {
                LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
                CellBank* bank = bitmapLayer->cellBank();
                if (bank) {
                    QList<int> order = bank->getAllCellIds();
                    int count = order.size();
                    if (targetIndex > count) targetIndex = count;
                    int currentIndex = bank->indexForCellId(mEditingCellId);
                    if (currentIndex > 0 && currentIndex != targetIndex) {
                        order.removeOne(mEditingCellId);
                        order.insert(targetIndex - 1, mEditingCellId);
                        bank->setCellOrder(order);
                        emit mEditor->framesModified();
                        emit mEditor->updateTimeLine();
                        refreshStack();
                    }
                }
            }
        }
    }

    mIsInlineEditing = false;
    mEditingCellId = -1;
}

void CellBankFloatingPanel::commitOrderFromList(bool finalCommit)
{
    if (!mEditor || !mListWidget) return;
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    QList<int> order;
    order.reserve(mListWidget->count());
    for (int i = 0; i < mListWidget->count(); ++i) {
        if (auto* it = mListWidget->item(i)) {
            int id = it->data(Qt::UserRole).toInt();
            if (id > 0) order.append(id);
        }
    }
    if (!order.isEmpty()) {
        bank->setCellOrder(order);
        // NOTE: intentionally NOT renaming items here — cells keep their original names
        // regardless of their position in the list. Renaming logic kept below for future use:
        // for (int i = 0; i < mListWidget->count(); ++i)
        //     if (auto* it = mListWidget->item(i)) it->setText(QString::number(i + 1));
        emit mEditor->framesModified();
        emit mEditor->updateTimeLine();
        if (finalCommit) refreshStack();
    }
}

void CellBankFloatingPanel::stopJiggle()
{
    if (mDragIndicatorTimer && mDragIndicatorTimer->isActive()) mDragIndicatorTimer->stop();
    if (mJiggleStartTimer && mJiggleStartTimer->isActive()) mJiggleStartTimer->stop();
    if (mDragIndicator) mDragIndicator->hide();
    mJiggleRow = -1;
    mJiggleStep = 0;
}

void CellBankFloatingPanel::updateJiggleOverlayGeometry()
{
    if (!mListWidget || !mListViewport) return;
    if (mJiggleRow < 0 || mJiggleRow >= mListWidget->count()) return;
    QListWidgetItem* item = mListWidget->item(mJiggleRow);
    if (!item) return;

    QRect r = mListWidget->visualItemRect(item);
    static const int offsets[] = {0, -2, 2, -2, 2, -1, 1, 0, 0};
    const int count = int(sizeof(offsets) / sizeof(offsets[0]));
    int step = qBound(0, mJiggleStep, count - 1);

    if (!mDragIndicator) {
        mDragIndicator = new QFrame(mListViewport);
        mDragIndicator->setFrameShape(QFrame::NoFrame);
        mDragIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    const QColor accent = CosmicColor::accent();
    mDragIndicator->setStyleSheet(QString("background:transparent; border:1px solid %1; border-radius:4px;").arg(accent.name()));
    QRect g = r.adjusted(2, 0, -2, 0);
    g.translate(0, offsets[step]);
    mDragIndicator->setGeometry(g);
    mDragIndicator->show();
}

void CellBankFloatingPanel::advanceJiggle()
{
    static const int kCount = 9;
    if (++mJiggleStep >= kCount) { stopJiggle(); return; }
    updateJiggleOverlayGeometry();
}

void CellBankFloatingPanel::startJiggle(int row)
{
    if (!mListWidget || !mListViewport) return;
    if (row < 0 || row >= mListWidget->count()) return;
    stopJiggle();
    mJiggleRow = row;
    mJiggleStep = 0;
    if (!mDragIndicatorTimer) {
        mDragIndicatorTimer = new QTimer(this);
        mDragIndicatorTimer->setInterval(30);
        connect(mDragIndicatorTimer, &QTimer::timeout, this, &CellBankFloatingPanel::advanceJiggle);
    }
    updateJiggleOverlayGeometry();
    mDragIndicatorTimer->start();
}

bool CellBankFloatingPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mInlineEditor && mIsInlineEditing) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) { finishInlineRename(true); return true; }
            if (ke->key() == Qt::Key_Escape) { finishInlineRename(false); return true; }
        } else if (event->type() == QEvent::FocusOut) {
            finishInlineRename(true);
        }
    }

    if (watched == mListViewport) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                if (auto* item = mListWidget->itemAt(me->pos())) {
                    mIsDragging = false;
                    mDragCurrentRow = mListWidget->row(item);
                    mDragStartPos = me->pos();
                    mDragTargetRow = mDragCurrentRow;
                    stopJiggle();
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && mDragCurrentRow >= 0) {
                if (mJiggleRow != -1) stopJiggle();
                if (!mIsDragging) {
                    if ((me->pos() - mDragStartPos).manhattanLength() < QApplication::startDragDistance())
                        return QWidget::eventFilter(watched, event);
                    mIsDragging = true;
                }
                int targetRow = -1;
                if (auto* over = mListWidget->itemAt(me->pos()))
                    targetRow = mListWidget->row(over);
                else if (me->pos().y() < 0 && mListWidget->count() > 0)
                    targetRow = 0;
                else if (me->pos().y() > mListViewport->height() && mListWidget->count() > 0)
                    targetRow = mListWidget->count() - 1;

                if (targetRow >= 0 && targetRow != mDragCurrentRow) {
                    mDragTargetRow = targetRow;
                    auto* movingItem = mListWidget->takeItem(mDragCurrentRow);
                    mListWidget->insertItem(mDragTargetRow, movingItem);
                    mListWidget->setCurrentRow(mDragTargetRow);
                    mDragCurrentRow = mDragTargetRow;
                    commitOrderFromList(false);
                }
                if (!mJiggleStartTimer) {
                    mJiggleStartTimer = new QTimer(this);
                    mJiggleStartTimer->setInterval(160);
                    mJiggleStartTimer->setSingleShot(true);
                    connect(mJiggleStartTimer, &QTimer::timeout, this, [this]() {
                        if (mIsDragging && mDragCurrentRow >= 0 && mJiggleRow == -1)
                            startJiggle(mDragCurrentRow);
                    });
                }
                mJiggleStartTimer->start();
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (mIsDragging) {
                const bool hadJiggle = (mJiggleRow != -1);
                int endRow = mListWidget ? mListWidget->currentRow() : -1;
                commitOrderFromList(true);
                if (endRow >= 0 && !hadJiggle) startJiggle(endRow);
            }
            mIsDragging = false;
            mDragCurrentRow = -1;
            mDragTargetRow = -1;
            if (mJiggleStartTimer && mJiggleStartTimer->isActive()) mJiggleStartTimer->stop();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CellBankFloatingPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange) return;

    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    // Window title and panel title label
    setWindowTitle(safetr("Cell Bank"));
    if (mTitleLabel) mTitleLabel->setText(safetr("Cell Bank"));

    // Button texts and tooltips
    if (mAddButton)    mAddButton->setToolTip(safetr("Add new cell"));
    if (mRemoveButton) mRemoveButton->setToolTip(safetr("Remove selected cell"));
    if (mCleanupButton) {
        mCleanupButton->setText(safetr("Remove Unused"));
        mCleanupButton->setToolTip(safetr("Remove unused cells from bank"));
    }
    if (mRenameTimelineOrderButton) {
        mRenameTimelineOrderButton->setText(safetr("Rename: Timeline"));
        mRenameTimelineOrderButton->setToolTip(safetr("Rename cells 1,2,3... by first appearance on the timeline"));
    }
}
