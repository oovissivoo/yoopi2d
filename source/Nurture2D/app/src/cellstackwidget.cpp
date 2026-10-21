#include "cellstackwidget.h"

#include "cosmicwarningpopup.h"
#include "cosmicconfirmpopup.h"
#include "../../CUI/cosmicuimetrics.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QIcon>
#include <QListWidget>
#include <QLineEdit>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QDebug>
#include <QFrame>
#include <QTimer>


#include "../../CUI/blenderstylebutton.h"

#include "editor.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "cellbank.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "scribblearea.h"
#include "undoredomanager.h"

CellStackWidget::CellStackWidget(QWidget* parent)
    : BaseDockWidget(parent)
{
    setWindowTitle(tr("Cell Stack"));
    setObjectName("CellStack");
}

CellStackWidget::~CellStackWidget() = default;

void CellStackWidget::initUI()
{
    if (mRoot)
    {
        return;
    }

    // Connect signals using base class editor()
    Editor* ed = editor();
    if (ed)
    {
        connect(ed->layers(), &LayerManager::currentLayerChanged, this, &CellStackWidget::refreshStack);
        connect(ed, &Editor::scrubbed, this, &CellStackWidget::refreshStack);
        connect(ed, &Editor::framesModified, this, &CellStackWidget::refreshStack);
        connect(ed, &Editor::updateTimeLine, this, &CellStackWidget::refreshStack);
    }

    mRoot = new QWidget(this);
    setDockContentWidget(mRoot);

    mLayout = new QVBoxLayout(mRoot);
    mLayout->setContentsMargins(6, 6, 6, 6);
    mLayout->setSpacing(4);



    // Second header row: basic actions for internal bitmap cell stack (no logic wired yet)
    mActionsHeader = new QWidget(mRoot);
    mActionsLayout = new QHBoxLayout(mActionsHeader);
    mActionsLayout->setContentsMargins(0, 0, 0, 0);
    mActionsLayout->setSpacing(4);

    mAddButton = new BlenderStyleButton(mActionsHeader);
    mAddButton->setBorderless(true);
    mAddButton->setCornerRadius(6);
    mAddButton->setIcon(QIcon(":/icons/themes/playful/timeline/frame-add.svg"), QSize(16, 16));
    connect(mAddButton, &BlenderStyleButton::clicked, this, &CellStackWidget::onAddCell);

    mRemoveButton = new BlenderStyleButton(mActionsHeader);
    mRemoveButton->setBorderless(true);
    mRemoveButton->setCornerRadius(6);
    mRemoveButton->setIcon(QIcon(":/icons/themes/playful/timeline/frame-remove.svg"), QSize(16, 16));
    connect(mRemoveButton, &BlenderStyleButton::clicked, this, &CellStackWidget::onRemoveCell);

    const QSize iconBtnSize(22, 22);
    mAddButton->setMinimumSize(iconBtnSize);
    mRemoveButton->setMinimumSize(iconBtnSize);

    // Let buttons stretch horizontally so they fill the header row
    mAddButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mRemoveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    mActionsLayout->addWidget(mAddButton, 0);
    mActionsLayout->addWidget(mRemoveButton, 0);

    // Visual separator between header controls and stack area
    mActionsHeader->setStyleSheet("border-bottom: 1px solid #000000;");

    mLayout->addWidget(mActionsHeader, 0);

    // List Widget
    mListWidget = new QListWidget(mRoot);
    mListWidget->setViewMode(QListView::ListMode);
    mListWidget->setMovement(QListView::Static);
    mListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mListWidget->setResizeMode(QListView::Adjust);
    mListWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    mListWidget->setDragEnabled(false);
    mListWidget->setAcceptDrops(false);
    mListWidget->setStyleSheet(
        "QListWidget::item:selected{border-radius:6px;padding:2px;background-color:palette(highlight);}"
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
    // mListWidget->setIconSize(QSize(42, 42)); // User requested no thumbnails
    connect(mListWidget, &QListWidget::currentRowChanged, this, &CellStackWidget::onCellSelected);
    connect(mListWidget, &QListWidget::itemDoubleClicked, this, &CellStackWidget::onCellDoubleClicked);
    mListWidget->installEventFilter(this);
    mListViewport = mListWidget->viewport();
    if (mListViewport) {
        mListViewport->installEventFilter(this);
    }

    mLayout->addWidget(mListWidget, 1);
}

void CellStackWidget::updateUI()
{
    refreshStack();
}

void CellStackWidget::refreshStack()
{
    Editor* ed = editor();
    if (!ed || !mListWidget) return;

    stopJiggle();

    // Block signals to avoid feedback loop
    bool signalsBlocked = mListWidget->blockSignals(true);
    
    // Store previous selection to restore it if possible
    int previouslySelectedId = -1;
    if (mListWidget->currentItem()) {
        previouslySelectedId = mListWidget->currentItem()->data(Qt::UserRole).toInt();
    }

    mListWidget->clear();

    Layer* layer = ed->layers()->currentLayer();
    if (layer && layer->type() == Layer::BITMAP)
    {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        CellBank* bank = bitmapLayer->cellBank();
        
        QList<int> cellIds = bank->getAllCellIds();

        // Add items
        for (int id : cellIds)
        {
            int index = bank->indexForCellId(id);
            QString name = QString::number(index > 0 ? index : id);
            
            QListWidgetItem* item = new QListWidgetItem(name, mListWidget);
            item->setData(Qt::UserRole, id);
            item->setTextAlignment(Qt::AlignCenter); 
            
            // Selection logic:
            // 1. If user previously selected a cell in the list, keep it selected (Persistence).
            // 2. If no user selection (first run), maybe select the one active on canvas?
            
            // We only AUTO-SELECT if we want the list to follow the canvas. 
            // The user complained about selection vanishing.
            // Let's prioritize restoring the manual selection.
        }
        
        // Restore selection
        bool selectionRestored = false;
        if (previouslySelectedId != -1) {
            for(int i=0; i<mListWidget->count(); ++i) {
                QListWidgetItem* item = mListWidget->item(i);
                if (item->data(Qt::UserRole).toInt() == previouslySelectedId) {
                    mListWidget->setCurrentItem(item);
                    selectionRestored = true;
                    break;
                }
            }
        }
        
        // If the previously selected item is gone (deleted), fallback to the one before it, or the last one.
        if (!selectionRestored && mListWidget->count() > 0) {
            // Select the last item? or the one that took its place?
            // Simple fallback: select last item
            mListWidget->setCurrentRow(mListWidget->count() - 1);
        }
    }

    mListWidget->blockSignals(signalsBlocked);
}

// Single click just highlights the row (handled by list widget default behavior)
void CellStackWidget::onCellSelected(int row)
{
    // Minimal logic or preview if needed
    Q_UNUSED(row);
}

// Helper logic for Exposing a cell (shared by double click and Expose button)
void CellStackWidget::exposeCell(int cellId)
{
    Editor* ed = editor();
    if (!ed) return;

    Layer* layer = ed->layers()->currentLayer();
    if (layer && layer->type() == Layer::BITMAP)
    {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        int currentFrame = ed->currentFrame();

        KeyFrame* kf = bitmapLayer->getKeyFrameAt(currentFrame);
        BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);

        // Scenario: Active frame already has this cell exposed?
        if (bkf && bkf->cellId() == cellId) {
            // REDUNDANT. Do nothing.
            return;
        }

        // Scenario: Active frame has DIFFERENT cell exposed? -> REPLACE
        // Scenario: Active frame is empty? -> ADD NEW

        // Create new KeyFrame exposing this cell
        BitmapKeyFrame* newKf = new BitmapKeyFrame(cellId);
        newKf->setPos(currentFrame);
        newKf->setCellBank(bitmapLayer->cellBank());

        bool exposureChanged = false;
        if (kf) {
             // REPLACE logic
             bitmapLayer->replaceKeyFrame(newKf);
             exposureChanged = true;
        } else {
             // ADD logic
             exposureChanged = bitmapLayer->addKeyFrame(currentFrame, newKf);
        }
        
        if (exposureChanged) {
            ed->updateUndoContext();
        }
        
        ed->setModified(ed->layers()->currentLayerIndex(), currentFrame);
        ed->scrubTo(currentFrame);
        ed->getScribbleArea()->update();
    }
}

void CellStackWidget::onCellDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    startInlineRename(item);
}

void CellStackWidget::onAddCell()
{
    Editor* ed = editor();
    if (!ed) return;
    ed->addNewKey();
}

void CellStackWidget::onRemoveCell()
{
    Editor* ed = editor();
    if (!ed || !mListWidget) return;
    stopJiggle();
    
    QListWidgetItem* item = mListWidget->currentItem();
    if (!item) return;
    int cellId = item->data(Qt::UserRole).toInt();
    
    Layer* layer = ed->layers()->currentLayer();
    if (layer && layer->type() == Layer::BITMAP)
    {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        CellBank* bank = bitmapLayer->cellBank();
        if (!bank) return;

        QList<int> allIds = bank->getAllCellIds();
        if (allIds.size() <= 1)
        {
            qDebug() << "[DeleteCell] last cell - will clear instead of delete. cellId=" << cellId;
            // Never remove the last cell: clear its bitmap and undo history, rename to "1".
            BitmapImage* img = bank->getCell(cellId);
            if (img)
            {
                img->clear();
            }
            if (QUndoStack* stack = bank->getOrCreateUndoStack(cellId)) {
                stack->clear();
            }
            bank->setCellName(cellId, QString::number(1));

            // Force immediate canvas refresh for the active frame
            emit ed->frameModified(ed->currentFrame());
            emit ed->framesModified();
            emit ed->updateTimeLine();
            refreshStack();
            ed->getScribbleArea()->update();
            return;
        }

        // More than one cell: allow permanent deletion

        qDebug() << "[DeleteCell] showing warning popup for cellId=" << cellId << "count=" << allIds.size();
        CosmicConfirmPopup popup(this);
        popup.setTitleText(tr("Delete Cell"));
        popup.setBodyText(tr("This will remove the cell from the bank and ALL references to it on the timeline. This action cannot be undone."));
        popup.setConfirmText(tr("Yes"));
        popup.setCancelText(tr("No"));
        popup.exec();
        qDebug() << "[DeleteCell] popup result=" << popup.result();
        if (popup.result() != QDialog::Accepted) {
            return;
        }

        // Before performing destructive removal, temporarily force undo/redo
        // to use the global stack so no per-cell stack remains active while
        // we delete keyframes and the associated cell data.
        if (UndoRedoManager* undoMgr = ed->undoRedo()) {
            undoMgr->setActiveCellStack(nullptr);
        }

        // 1. Remove references from Timeline (Clean up)
        QList<int> framesToRemove;
        bitmapLayer->foreachKeyFrame([&](KeyFrame* kf){
            BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
            if (bkf && bkf->cellId() == cellId) {
                framesToRemove.append(kf->pos());
            }
        });

        for (int framePos : framesToRemove) {
            bitmapLayer->removeKeyFrame(framePos);
        }

        // 2. Permanently delete from CellBank
        bank->removeCell(cellId);

        // Ensure undo context no longer points to a removed cell/stack
        ed->updateUndoContext();

        ed->layers()->notifyAnimationLengthChanged();
        emit ed->framesModified();
        emit ed->updateTimeLine();

        refreshStack();
        ed->getScribbleArea()->update();
    }
}

void CellStackWidget::startInlineRename(QListWidgetItem* item)
{
    if (!item || mIsInlineEditing || !mListWidget) return;

    Editor* ed = editor();
    if (!ed) return;

    int cellId = item->data(Qt::UserRole).toInt();
    Layer* layer = ed->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    int index = bank->indexForCellId(cellId);
    if (index <= 0) return;

    if (!mInlineEditor)
    {
        mInlineEditor = new QLineEdit(mListWidget);
        mInlineEditor->setFrame(false);
        mInlineEditor->setAlignment(Qt::AlignCenter);
        mInlineEditor->setTextMargins(0, 0, 0, 0);
        mInlineEditor->setStyleSheet(CosmicInlineEdit::stylesheet());
        mInlineEditor->installEventFilter(this);
    }

    mEditingCellId = cellId;
    mIsInlineEditing = true;

    mInlineEditor->setText(QString::number(index));
    QRect r = mListWidget->visualItemRect(item);
    // Height matches the text line, vertically centred in the item row
    const QFontMetrics fm(mInlineEditor->font());
    const int editH = fm.height() + 4;
    const int editY = r.top() + (r.height() - editH) / 2;
    mInlineEditor->setGeometry(QRect(r.left() + 4, editY, r.width() - 8, editH));
    mInlineEditor->show();
    mInlineEditor->setFocus(Qt::OtherFocusReason);
    mInlineEditor->selectAll();
}

void CellStackWidget::finishInlineRename(bool accept)
{
    if (!mIsInlineEditing) return;

    Editor* ed = editor();
    if (!ed || !mListWidget) {
        if (mInlineEditor) mInlineEditor->hide();
        mIsInlineEditing = false;
        mEditingCellId = -1;
        return;
    }

    int targetIndex = -1;
    if (accept && mInlineEditor)
    {
        bool ok = false;
        int typed = mInlineEditor->text().trimmed().toInt(&ok);
        if (ok) targetIndex = typed;
    }

    if (mInlineEditor) mInlineEditor->hide();

    Layer* layer = ed->layers()->currentLayer();
    if (accept && targetIndex > 0 && layer && layer->type() == Layer::BITMAP)
    {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        CellBank* bank = bitmapLayer->cellBank();
        if (bank)
        {
            QList<int> order = bank->getAllCellIds();
            int count = order.size();
            if (count > 0)
            {
                if (targetIndex > count) targetIndex = count;
                int currentIndex = bank->indexForCellId(mEditingCellId);
                if (currentIndex > 0 && currentIndex != targetIndex)
                {
                    order.removeOne(mEditingCellId);
                    order.insert(targetIndex - 1, mEditingCellId);
                    bank->setCellOrder(order);

                    emit ed->framesModified();
                    emit ed->updateTimeLine();
                    refreshStack();
                }
            }
        }
    }

    mIsInlineEditing = false;
    mEditingCellId = -1;
}

static QColor cellStackAccentColor(QWidget* w)
{
    return w ? w->palette().highlight().color() : QColor(Qt::white);
}

void CellStackWidget::stopJiggle()
{
    if (mDragIndicatorTimer && mDragIndicatorTimer->isActive())
    {
        mDragIndicatorTimer->stop();
    }
    if (mJiggleStartTimer && mJiggleStartTimer->isActive())
    {
        mJiggleStartTimer->stop();
    }
    if (mDragIndicator)
    {
        mDragIndicator->hide();
    }
    mJiggleRow = -1;
    mJiggleStep = 0;
}

void CellStackWidget::updateJiggleOverlayGeometry()
{
    if (!mListWidget || !mListViewport) return;
    if (mJiggleRow < 0 || mJiggleRow >= mListWidget->count()) return;

    QListWidgetItem* item = mListWidget->item(mJiggleRow);
    if (!item) return;

    QRect r = mListWidget->visualItemRect(item);

    static const int offsets[] = {0, -2, 2, -2, 2, -1, 1, 0, 0};
    const int count = int(sizeof(offsets) / sizeof(offsets[0]));
    int clampedStep = mJiggleStep;
    if (clampedStep < 0) clampedStep = 0;
    if (clampedStep >= count) clampedStep = count - 1;
    int offsetY = offsets[clampedStep];

    if (!mDragIndicator)
    {
        mDragIndicator = new QFrame(mListViewport);
        mDragIndicator->setFrameShape(QFrame::NoFrame);
        mDragIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    QColor accent = cellStackAccentColor(this);
    QString style = QString("background-color: transparent; border:1px solid %1; border-radius:6px;").arg(accent.name());
    mDragIndicator->setStyleSheet(style);

    QRect g = r.adjusted(2, 0, -2, 0);
    g.translate(0, offsetY);
    mDragIndicator->setGeometry(g);
    mDragIndicator->show();
}

void CellStackWidget::advanceJiggle()
{
    static const int offsetsCount = 9; // must match offsets[] length in updateJiggleOverlayGeometry
    ++mJiggleStep;
    if (mJiggleStep >= offsetsCount)
    {
        stopJiggle();
        return;
    }
    updateJiggleOverlayGeometry();
}

void CellStackWidget::startJiggle(int row)
{
    if (!mListWidget || !mListViewport) return;
    if (row < 0 || row >= mListWidget->count()) return;

    stopJiggle();
    mJiggleRow = row;
    mJiggleStep = 0;

    if (!mDragIndicatorTimer)
    {
        mDragIndicatorTimer = new QTimer(this);
        mDragIndicatorTimer->setInterval(30);
        connect(mDragIndicatorTimer, &QTimer::timeout, this, &CellStackWidget::advanceJiggle);
    }

    updateJiggleOverlayGeometry();
    mDragIndicatorTimer->start();
}

void CellStackWidget::commitOrderFromList(bool finalCommit)
{
    Editor* ed = editor();
    if (!ed || !mListWidget) return;

    Layer* layer = ed->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    QList<int> order;
    order.reserve(mListWidget->count());
    for (int i = 0; i < mListWidget->count(); ++i)
    {
        QListWidgetItem* it = mListWidget->item(i);
        if (!it) continue;
        int id = it->data(Qt::UserRole).toInt();
        if (id > 0) order.append(id);
    }

    if (!order.isEmpty())
    {
        bank->setCellOrder(order);
        for (int i = 0; i < mListWidget->count(); ++i)
        {
            QListWidgetItem* it = mListWidget->item(i);
            if (!it) continue;
            it->setText(QString::number(i + 1));
        }
        if (finalCommit)
        {
            emit ed->framesModified();
            emit ed->updateTimeLine();
            refreshStack();
        }
        else
        {
            emit ed->framesModified();
            emit ed->updateTimeLine();
        }
    }
}

bool CellStackWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mInlineEditor && mIsInlineEditing)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            {
                finishInlineRename(true);
                return true;
            }
            if (ke->key() == Qt::Key_Escape)
            {
                finishInlineRename(false);
                return true;
            }
        }
        else if (event->type() == QEvent::FocusOut)
        {
            finishInlineRename(true);
        }
    }

    if (watched == mListViewport)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton)
            {
                QListWidgetItem* item = mListWidget->itemAt(me->pos());
                if (item)
                {
                    mIsDragging = false;
                    mDragCurrentRow = mListWidget->row(item);
                    mDragStartPos = me->pos();
                    mDragTargetRow = mDragCurrentRow;
                    stopJiggle();
                    if (mJiggleStartTimer && mJiggleStartTimer->isActive())
                    {
                        mJiggleStartTimer->stop();
                    }
                }
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton && mDragCurrentRow >= 0)
            {
                if (mJiggleRow != -1)
                {
                    stopJiggle();
                }
                if (!mIsDragging)
                {
                    if ((me->pos() - mDragStartPos).manhattanLength() < QApplication::startDragDistance())
                    {
                        return BaseDockWidget::eventFilter(watched, event);
                    }
                    mIsDragging = true;
                }

                int targetRow = -1;
                if (QListWidgetItem* overItem = mListWidget->itemAt(me->pos()))
                {
                    targetRow = mListWidget->row(overItem);
                }
                else if (me->pos().y() < 0 && mListWidget->count() > 0)
                {
                    targetRow = 0;
                }
                else if (me->pos().y() > mListViewport->height() && mListWidget->count() > 0)
                {
                    targetRow = mListWidget->count() - 1;
                }

                if (targetRow >= 0 && targetRow != mDragCurrentRow)
                {
                    mDragTargetRow = targetRow;

                    QListWidgetItem* movingItem = mListWidget->takeItem(mDragCurrentRow);
                    mListWidget->insertItem(mDragTargetRow, movingItem);
                    mListWidget->setCurrentRow(mDragTargetRow);
                    mDragCurrentRow = mDragTargetRow;
                    commitOrderFromList(false);
                }

                if (!mJiggleStartTimer)
                {
                    mJiggleStartTimer = new QTimer(this);
                    mJiggleStartTimer->setInterval(160);
                    mJiggleStartTimer->setSingleShot(true);
                    connect(mJiggleStartTimer, &QTimer::timeout, this, [this]() {
                        if (mIsDragging && mDragCurrentRow >= 0 && mJiggleRow == -1)
                        {
                            startJiggle(mDragCurrentRow);
                        }
                    });
                }
                mJiggleStartTimer->start();
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            if (mIsDragging)
            {
                const bool hadJiggle = (mJiggleRow != -1);
                int endRow = mListWidget ? mListWidget->currentRow() : -1;
                commitOrderFromList(true);
                if (endRow >= 0 && !hadJiggle)
                {
                    startJiggle(endRow);
                }
            }
            mIsDragging = false;
            mDragCurrentRow = -1;
            mDragTargetRow = -1;
            if (mJiggleStartTimer && mJiggleStartTimer->isActive())
            {
                mJiggleStartTimer->stop();
            }
        }
    }

    return BaseDockWidget::eventFilter(watched, event);
}
