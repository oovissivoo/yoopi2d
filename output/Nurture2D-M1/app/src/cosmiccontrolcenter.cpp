#include "cosmiccontrolcenter.h"

#include "pencildef.h"

#include <QBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QShowEvent>
#include <QApplication>
#include <QLayoutItem>
#include <QToolBar>
#include <QToolButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QFrame>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QSettings>
#include <QSignalBlocker>
#include <QAbstractButton>

#include "pencolorpalette.h"
#include "autolinecolortoggle.h"
#include "editor.h"

#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/blenderstylemodalpopup.h"
#include "../CUI/cosmicuimetrics.h"

namespace {
QColor pillBg()
{
    return QColor(0x1f, 0x1f, 0x1f, 0xff);
}

} // namespace

QStringList CosmicControlCenter::loadActionOrder() const
{
    QSettings s(PENCIL2D, PENCIL2D);
    return s.value(QStringLiteral("CosmicControlCenter/ActionOrder")).toStringList();
}

void CosmicControlCenter::saveActionOrder(const QStringList& order) const
{
    QSettings s(PENCIL2D, PENCIL2D);
    s.setValue(QStringLiteral("CosmicControlCenter/ActionOrder"), order);
    s.sync();
}

QStringList CosmicControlCenter::actionOrderFromLayout() const
{
    if (!mPinsLayout) {
        return {};
    }

    QStringList out;
    for (int i = 0; i < mPinsLayout->count(); ++i) {
        QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
        if (!w || w == mDragPlaceholder) {
            continue;
        }
        if (!w->property("CosmicIsTile").toBool()) {
            continue;
        }
        if (w->property("CosmicTileGroupId").toInt() >= 0) {
            continue;
        }
        const QString key = w->property("CosmicActionKey").toString();
        if (!key.isEmpty()) {
            out.push_back(key);
        }
    }
    return out;
}

namespace {

class CosmicTilesPopupDialog : public QDialog
{
public:
    explicit CosmicTilesPopupDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(20);
        shadow->setColor(QColor(0, 0, 0, 80));
        shadow->setOffset(0, 0);
        setGraphicsEffect(shadow);
    }

    void setTitleBarHeight(int h) { mTitleBarHeight = h; }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        Q_UNUSED(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QColor accent = BlenderStyleModalPopup::getAccentColor();
        QPainterPath path;
        const QRectF r = QRectF(contentsRect()).adjusted(2 * 0.5, 2 * 0.5, -2 * 0.5, -2 * 0.5);
        path.addRoundedRect(r, 14, 14);
        p.fillPath(path, QColor(45, 45, 45));
        QPen pen(accent, 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    void mousePressEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton) {
            QRect titleRect(0, 0, width(), mTitleBarHeight);
            if (titleRect.contains(ev->pos())) {
                mDragging = true;
                mDragOffset = ev->globalPos() - frameGeometry().topLeft();
            }
        }
        QDialog::mousePressEvent(ev);
    }

    void mouseMoveEvent(QMouseEvent* ev) override
    {
        if (mDragging && (ev->buttons() & Qt::LeftButton)) {
            move(ev->globalPos() - mDragOffset);
        }
        QDialog::mouseMoveEvent(ev);
    }

    void mouseReleaseEvent(QMouseEvent* ev) override
    {
        mDragging = false;
        QDialog::mouseReleaseEvent(ev);
    }

private:
    bool mDragging = false;
    QPoint mDragOffset;
    int mTitleBarHeight = 40;
};

} // namespace

int CosmicControlCenter::layoutIndexOf(QWidget* w) const
{
    if (!mPinsLayout || !w) {
        return -1;
    }
    for (int i = 0; i < mPinsLayout->count(); ++i) {
        QLayoutItem* it = mPinsLayout->itemAt(i);
        if (it && it->widget() == w) {
            return i;
        }
    }
    return -1;
}

void CosmicControlCenter::updateGroupStartsFromLayout()
{
    if (!mPinsLayout) {
        return;
    }

    int penStart = -1;
    int autoStart = -1;

    int tileIdx = 0;
    for (int i = 0; i < mPinsLayout->count(); ++i) {
        QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
        if (!w) {
            continue;
        }

        // Placeholder is not a tile.
        if (w == mDragPlaceholder) {
            continue;
        }

        // We only count real tile widgets.
        if (!w->property("CosmicIsTile").toBool()) {
            continue;
        }

        const int gid = w->property("CosmicTileGroupId").toInt();
        const int gidx = w->property("CosmicTileGroupIndex").toInt();
        if (gid == 0 && gidx == 0 && penStart < 0) {
            penStart = tileIdx;
        }
        if (gid == 1 && gidx == 0 && autoStart < 0) {
            autoStart = tileIdx;
        }
        ++tileIdx;
    }

    if (penStart >= 0) {
        mPenGroupStart = penStart;
    }
    if (autoStart >= 0) {
        mAutoFillGroupStart = autoStart;
    }

    normalizeGroupPositions();
}

void CosmicControlCenter::beginGroupDrag(int groupId, const QPoint& viewportPos)
{
    if (!mEditMode || !mPinsLayout || !mScrollArea || !mScrollContent) {
        return;
    }
    if (mDragging) {
        return;
    }

    // Collect the current tile widgets that belong to the group.
    QVector<QWidget*> groupTiles;
    groupTiles.reserve(6);
    for (int i = 0; i < mPinsLayout->count(); ++i) {
        QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
        if (!w) {
            continue;
        }
        if (!w->property("CosmicIsTile").toBool()) {
            continue;
        }
        if (w->property("CosmicTileGroupId").toInt() != groupId) {
            continue;
        }
        groupTiles.push_back(w);
    }
    if (groupTiles.isEmpty()) {
        return;
    }

    std::sort(groupTiles.begin(), groupTiles.end(), [](QWidget* a, QWidget* b) {
        return a->property("CosmicTileGroupIndex").toInt() < b->property("CosmicTileGroupIndex").toInt();
    });

    // Cache original geometries for stable threshold calculations.
    mDragOriginalGeometries.clear();
    for (int i = 0; i < mPinsLayout->count(); ++i) {
        QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
        if (!w || w == mDragPlaceholder) {
            continue;
        }
        if (w->isVisible()) {
            mDragOriginalGeometries.insert(w, w->geometry());
        }
    }

    QWidget* first = groupTiles.first();
    const int firstLayoutIdx = layoutIndexOf(first);
    if (firstLayoutIdx < 0) {
        return;
    }

    // Create placeholder in the layout at the original group position.
    const int tileW = 40;
    const int tileH = 26;
    const int spacing = mPinsLayout->spacing();
    const int span = groupTiles.size();
    const int placeholderW = span * tileW + qMax(0, span - 1) * spacing;

    mDragPlaceholder = new QWidget(mScrollContent);
    mDragPlaceholder->setFixedSize(placeholderW, tileH);
    mDragPlaceholder->setVisible(true);

    mPinsLayout->insertWidget(firstLayoutIdx, mDragPlaceholder);

    // Remove the dragged tiles from the layout.
    for (QWidget* w : groupTiles) {
        mPinsLayout->removeWidget(w);
    }
    mPinsLayout->invalidate();
    mPinsLayout->activate();

    // Create floating host on top of the viewport.
    mDragFloatingHost = new QWidget(mScrollArea->viewport());
    mDragFloatingHost->setAttribute(Qt::WA_NoSystemBackground, true);
    mDragFloatingHost->setAutoFillBackground(false);
    mDragFloatingHost->setMouseTracking(true);
    mDragFloatingHost->setFixedSize(placeholderW, tileH);
    mDragFloatingHost->show();
    mDragFloatingHost->raise();

    auto* hl = new QHBoxLayout(mDragFloatingHost);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(spacing);
    mDragFloatingLayout = hl;
    mDragFloatingHost->setLayout(hl);

    for (QWidget* w : groupTiles) {
        w->setParent(mDragFloatingHost);
        hl->addWidget(w, 0, Qt::AlignVCenter);
    }

    // Compute grab offset relative to the floating host.
    const QPoint firstTopLeftViewport = first->mapTo(mScrollArea->viewport(), QPoint(0, 0));
    mDragGrabOffset = viewportPos - firstTopLeftViewport;

    mDraggingWidgets = groupTiles;
    mDragging = true;
    mDraggingGroupId = groupId;
    mDragGroupSpan = span;
    mLastDragTargetIndex = -1;

    mScrollArea->viewport()->grabMouse();
    updateGroupDrag(viewportPos);
}

void CosmicControlCenter::beginSingleTileDrag(QWidget* tile, const QPoint& viewportPos)
{
    if (!mEditMode || !mPinsLayout || !mScrollArea || !mScrollContent) {
        return;
    }
    if (!tile || !tile->property("CosmicIsTile").toBool()) {
        return;
    }
    if (mDragging) {
        return;
    }
    if (tile->property("CosmicTileGroupId").toInt() >= 0) {
        return;
    }

    const int tileLayoutIdx = layoutIndexOf(tile);
    if (tileLayoutIdx < 0) {
        return;
    }

    // Cache original geometries for stable threshold calculations.
    mDragOriginalGeometries.clear();
    for (int i = 0; i < mPinsLayout->count(); ++i) {
        QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
        if (!w || w == mDragPlaceholder) {
            continue;
        }
        if (w->isVisible()) {
            mDragOriginalGeometries.insert(w, w->geometry());
        }
    }

    const int tileW = tile->width();
    const int tileH = tile->height();

    mDragPlaceholder = new QWidget(mScrollContent);
    mDragPlaceholder->setFixedSize(tileW, tileH);
    mDragPlaceholder->setVisible(true);

    mPinsLayout->insertWidget(tileLayoutIdx, mDragPlaceholder);
    mPinsLayout->removeWidget(tile);
    mPinsLayout->invalidate();
    mPinsLayout->activate();

    // Floating host.
    mDragFloatingHost = new QWidget(mScrollArea->viewport());
    mDragFloatingHost->setAttribute(Qt::WA_NoSystemBackground, true);
    mDragFloatingHost->setAutoFillBackground(false);
    mDragFloatingHost->setMouseTracking(true);
    mDragFloatingHost->setFixedSize(tileW, tileH);
    mDragFloatingHost->show();
    mDragFloatingHost->raise();

    auto* hl = new QHBoxLayout(mDragFloatingHost);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    mDragFloatingLayout = hl;
    mDragFloatingHost->setLayout(hl);

    tile->setParent(mDragFloatingHost);
    hl->addWidget(tile, 0, Qt::AlignVCenter);

    const QPoint tileTopLeftViewport = tile->mapTo(mScrollArea->viewport(), QPoint(0, 0));
    mDragGrabOffset = viewportPos - tileTopLeftViewport;

    mDraggingWidgets.clear();
    mDraggingWidgets.push_back(tile);
    mDragging = true;
    mDraggingGroupId = -1;
    mDragGroupSpan = 1;
    mLastDragTargetIndex = -1;

    mScrollArea->viewport()->grabMouse();
    updateGroupDrag(viewportPos);
}

void CosmicControlCenter::updateGroupDrag(const QPoint& viewportPos)
{
    if (!mDragging || !mScrollArea || !mScrollArea->viewport() || !mPinsLayout || !mDragPlaceholder || !mDragFloatingHost) {
        return;
    }

    updateDragAutoScroll(viewportPos);

    // Move floating host.
    QPoint topLeft = viewportPos - mDragGrabOffset;
    const int minX = 0;
    const int maxX = qMax(0, mScrollArea->viewport()->width() - mDragFloatingHost->width());
    topLeft.setX(qBound(minX, topLeft.x(), maxX));
    topLeft.setY(qBound(0, topLeft.y(), qMax(0, mScrollArea->viewport()->height() - mDragFloatingHost->height())));
    mDragFloatingHost->move(topLeft);

    const int cursorXInContent = mScrollContent->mapFrom(mScrollArea->viewport(), viewportPos).x();
    const int placeholderIdx = layoutIndexOf(mDragPlaceholder);
    if (placeholderIdx < 0) {
        return;
    }

    int targetIdx = placeholderIdx;

    // Group-aware targeting: treat each group (gid >= 0) as an atomic block.
    // The placeholder can only be placed BEFORE the block start or AFTER the
    // block end; never between tiles within a block.
    const int n = mPinsLayout->count();
    for (int i = 0; i < n; ) {
        QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
        if (!w || w == mDragPlaceholder || !w->isVisible()) {
            ++i;
            continue;
        }

        const int gid = w->property("CosmicTileGroupId").toInt();
        if (gid >= 0) {
            // Find contiguous block range.
            int j = i;
            QRect blockRect;
            bool any = false;
            while (j < n) {
                QWidget* wj = mPinsLayout->itemAt(j) ? mPinsLayout->itemAt(j)->widget() : nullptr;
                if (!wj || wj == mDragPlaceholder || !wj->isVisible()) {
                    ++j;
                    continue;
                }
                if (wj->property("CosmicTileGroupId").toInt() != gid) {
                    break;
                }
                const QRect r = mDragOriginalGeometries.value(wj, wj->geometry());
                blockRect = any ? blockRect.united(r) : r;
                any = true;
                ++j;
            }

            const int centerX = any ? blockRect.center().x() : mDragOriginalGeometries.value(w, w->geometry()).center().x();
            if (cursorXInContent < centerX && placeholderIdx > i) {
                targetIdx = i;
                break;
            }
            if (cursorXInContent > centerX && placeholderIdx < i) {
                targetIdx = j;
            }
            i = j;
            continue;
        }

        // Empty/unassigned tile (gid < 0): treat as a single slot.
        const QRect originalRect = mDragOriginalGeometries.value(w, w->geometry());
        const int otherCenterX = originalRect.center().x();
        if (cursorXInContent < otherCenterX && placeholderIdx > i) {
            targetIdx = i;
            break;
        }
        if (cursorXInContent > otherCenterX && placeholderIdx < i) {
            targetIdx = i + 1;
        }
        ++i;
    }

    if (targetIdx != placeholderIdx) {
        mPinsLayout->removeWidget(mDragPlaceholder);
        mPinsLayout->insertWidget(targetIdx, mDragPlaceholder);
        mPinsLayout->invalidate();
        mPinsLayout->activate();
    }
}

void CosmicControlCenter::endGroupDrag()
{
    if (!mDragging) {
        return;
    }
    if (mScrollArea && mScrollArea->viewport()) {
        mScrollArea->viewport()->releaseMouse();
    }

    if (!mPinsLayout || !mDragPlaceholder) {
        mDragging = false;
        mDraggingGroupId = -1;
        mDraggingWidgets.clear();
        return;
    }

    const int insertIdx = layoutIndexOf(mDragPlaceholder);
    if (insertIdx >= 0) {
        // Reinsert dragged tiles at placeholder location.
        int idx = insertIdx;
        for (QWidget* w : mDraggingWidgets) {
            if (!w) {
                continue;
            }
            mDragFloatingLayout->removeWidget(w);
            w->setParent(mScrollContent);
            mPinsLayout->insertWidget(idx, w, 0, Qt::AlignVCenter);
            ++idx;
        }
    }

    mPinsLayout->removeWidget(mDragPlaceholder);
    delete mDragPlaceholder;
    mDragPlaceholder = nullptr;

    if (mDragFloatingHost) {
        delete mDragFloatingHost;
        mDragFloatingHost = nullptr;
        mDragFloatingLayout = nullptr;
    }

    mPinsLayout->invalidate();
    mPinsLayout->activate();

    mDraggingWidgets.clear();
    mDragOriginalGeometries.clear();

    updateGroupStartsFromLayout();
    saveTileLayoutSettings();
    saveActionOrder(actionOrderFromLayout());

    mDragging = false;
    mDraggingGroupId = -1;

    // Ensure edit-mode interaction suppression remains active.
    applyEditModeInteractionOverride();
}

QVector<QAction*> CosmicControlCenter::actionsForCategory(ToolbarCategory cat) const
{
    QToolBar* tb = nullptr;
    if (cat == ToolbarCategory::Main) tb = mMainToolbar;
    if (cat == ToolbarCategory::View) tb = mViewToolbar;
    if (cat == ToolbarCategory::Overlay) tb = mOverlayToolbar;
    if (!tb) {
        return {};
    }

    QVector<QAction*> out;
    const auto acts = tb->actions();
    out.reserve(acts.size());
    for (QAction* a : acts) {
        if (!a) continue;
        // Skip separators and empty actions.
        if (a->isSeparator()) continue;
        if (a->text().trimmed().isEmpty() && a->icon().isNull()) continue;
        out.push_back(a);
    }
    return out;
}

QString CosmicControlCenter::categoryKey(ToolbarCategory cat) const
{
    if (cat == ToolbarCategory::Main) return QStringLiteral("Main");
    if (cat == ToolbarCategory::View) return QStringLiteral("View");
    return QStringLiteral("Overlay");
}

bool CosmicControlCenter::isCategoryEnabled(ToolbarCategory cat) const
{
    QSettings s(PENCIL2D, PENCIL2D);
    return s.value(QStringLiteral("CosmicControlCenter/Categories/%1").arg(categoryKey(cat)), true).toBool();
}

void CosmicControlCenter::setCategoryEnabled(ToolbarCategory cat, bool on)
{
    QSettings s(PENCIL2D, PENCIL2D);
    s.setValue(QStringLiteral("CosmicControlCenter/Categories/%1").arg(categoryKey(cat)), on);
    s.sync();
}

QString CosmicControlCenter::actionKey(QAction* a) const
{
    if (!a) {
        return QString();
    }
    const QString obj = a->objectName().trimmed();
    if (!obj.isEmpty()) {
        return obj;
    }
    return a->text().trimmed();
}

bool CosmicControlCenter::isActionEnabled(QAction* a) const
{
    if (!a) {
        return false;
    }
    const QString k = actionKey(a);
    if (k.isEmpty()) {
        return true;
    }
    QSettings s(PENCIL2D, PENCIL2D);
    return s.value(QStringLiteral("CosmicControlCenter/Actions/%1").arg(k), true).toBool();
}

void CosmicControlCenter::setActionEnabled(QAction* a, bool on)
{
    if (!a) {
        return;
    }
    const QString k = actionKey(a);
    if (k.isEmpty()) {
        return;
    }
    QSettings s(PENCIL2D, PENCIL2D);
    s.setValue(QStringLiteral("CosmicControlCenter/Actions/%1").arg(k), on);
    s.sync();
}

void CosmicControlCenter::showTilesPopup(const QPoint& globalPos)
{
    if (mTilesPopup) {
        mTilesPopup->close();
        delete mTilesPopup;
        mTilesPopup = nullptr;
    }

    auto* popup = new CosmicTilesPopupDialog(nullptr);
    popup->setObjectName(QStringLiteral("CosmicControlCenterTilesPopup"));
    popup->setMinimumSize(480, 300);
    popup->resize(540, 340);
    popup->setTitleBarHeight(40);

    auto* mainLayout = new QVBoxLayout(popup);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(0);

    auto* titleBar = new QHBoxLayout();
    titleBar->setContentsMargins(15, 8, 8, 8);

    auto* titleLabel = new QLabel(QStringLiteral("Cosmic Center"), popup);
    titleLabel->setStyleSheet(QStringLiteral("color: white; font-weight: bold; font-size: 14px;"));

    auto* editToggle = new CosmicCheckboxToggle(popup);
    editToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    editToggle->setText(QStringLiteral("Enter Edit Mode"));
    editToggle->setChecked(mEditMode);
    editToggle->setBordered(false);
    editToggle->setBackgroundEnabled(false);
    editToggle->setHoverHighlightEnabled(true);
    editToggle->setFixedHeight(22);

    auto* closeBtn = new QPushButton(popup);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setIcon(QIcon(QStringLiteral(":/Interaction/CloseWindow.svg")));
    closeBtn->setIconSize(QSize(16, 16));
    closeBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton { background-color: transparent; border: none; border-radius: 12px; }"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 51); }"
            "QPushButton:pressed { background-color: rgba(255, 255, 255, 77); }"));

    QObject::connect(closeBtn, &QPushButton::clicked, popup, [popup]() { popup->close(); });

    titleBar->addWidget(titleLabel);
    titleBar->addStretch();
    titleBar->addWidget(editToggle);
    titleBar->addSpacing(10);
    titleBar->addWidget(closeBtn);

    mainLayout->addLayout(titleBar);

    auto* splitter = new QSplitter(Qt::Horizontal, popup);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background-color: #1a1a1a; }"));

    auto* categories = new QListWidget(splitter);
    categories->setSelectionMode(QAbstractItemView::SingleSelection);
    categories->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categories->setStyleSheet(QStringLiteral(
        "QListWidget { background-color: #1a1a1a; border: none; outline: none; }"
        "QListWidget::item { border-bottom: 1px solid #2a2a2a; }"
        "QListWidget::item:selected { background-color: %1; }"
        "QListWidget::item:selected:hover { background-color: %1; }"
        "QListWidget::item:last { border-bottom: none; }").arg(CosmicColor::accentHex())));

    auto* items = new QTreeWidget(splitter);
    items->setRootIsDecorated(false);
    items->setUniformRowHeights(true);
    items->setColumnCount(3);
    items->setHeaderHidden(true);
    items->setSelectionMode(QAbstractItemView::NoSelection);
    items->setFocusPolicy(Qt::NoFocus);
    items->header()->setStretchLastSection(true);
    items->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    items->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    items->setStyleSheet(QStringLiteral(
        "QTreeWidget { background-color: #2a2a2a; color: white; border: none; outline: none; }"
        "QTreeWidget::item { padding: 6px 10px; }"
        ));

    splitter->addWidget(categories);
    splitter->addWidget(items);
    splitter->setStretchFactor(0, 35);
    splitter->setStretchFactor(1, 65);
    splitter->setSizes({190, 350});
    categories->setMinimumWidth(200);
    mainLayout->addWidget(splitter, 1);

    auto addCategoryItem = [&](ToolbarCategory cat, const QString& label) {
        auto* it = new QListWidgetItem(categories);
        it->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        it->setSizeHint(QSize(200, 40));
        it->setData(Qt::UserRole, static_cast<int>(cat));
        categories->addItem(it);

        auto* row = new QWidget(categories);
        auto* rowL = new QHBoxLayout(row);
        rowL->setContentsMargins(12, 6, 12, 6);
        rowL->setSpacing(10);

        auto* toggle = new CosmicCheckboxToggle(row);
        toggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        toggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
        toggle->setChecked(isCategoryEnabled(cat));
        toggle->setBordered(false);
        toggle->setBackgroundEnabled(false);
        toggle->setHoverHighlightEnabled(true);
        toggle->setMinimumSize(18, 18);

        auto* text = new QLabel(label, row);
        text->setStyleSheet(QStringLiteral("color: white; font-size: 13px;"));

        rowL->addWidget(toggle);
        rowL->addWidget(text);
        rowL->addStretch();

        categories->setItemWidget(it, row);

        QObject::connect(toggle, &QAbstractButton::toggled, popup, [this, cat, toggle, categories, it]() {
            Q_UNUSED(toggle);
            setCategoryEnabled(cat, isCategoryEnabled(cat) ? true : true);
            setCategoryEnabled(cat, toggle->isChecked());
            rebuildTiles();
            if (categories && categories->currentItem() == it) {
                // trigger refresh via selection change handler
                QMetaObject::invokeMethod(categories, [categories]() { categories->update(); }, Qt::QueuedConnection);
            }
        });
    };

    addCategoryItem(ToolbarCategory::Main, QStringLiteral("Main"));
    addCategoryItem(ToolbarCategory::View, QStringLiteral("View"));
    addCategoryItem(ToolbarCategory::Overlay, QStringLiteral("Overlay Toolbars"));
    categories->setCurrentRow(0);

    auto repopulateItems = [this, categories, items]() {
        if (!items || !categories) {
            return;
        }
        QSignalBlocker b1(items);
        items->clear();
        QListWidgetItem* sel = categories->currentItem();
        if (!sel) {
            return;
        }
        const ToolbarCategory cat = static_cast<ToolbarCategory>(sel->data(Qt::UserRole).toInt());
        const bool catEnabled = isCategoryEnabled(cat);

        for (QAction* a : actionsForCategory(cat)) {
            if (!a) continue;
            auto* row = new QTreeWidgetItem(items);
            row->setFlags(Qt::ItemIsEnabled);
            row->setSizeHint(0, QSize(0, 34));
            const bool enabled = catEnabled && isActionEnabled(a);

            // Add padding around the toggle so its paint area isn't clipped by the view.
            auto* toggleHost = new QWidget(items);
            auto* toggleHostL = new QHBoxLayout(toggleHost);
            toggleHostL->setContentsMargins(6, 4, 6, 4);
            toggleHostL->setSpacing(0);

            auto* toggle = new CosmicCheckboxToggle(toggleHost);
            toggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            toggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
            toggle->setChecked(enabled);
            toggle->setEnabled(catEnabled);
            toggle->setBordered(false);
            toggle->setBackgroundEnabled(false);
            toggle->setHoverHighlightEnabled(true);
            toggle->setMinimumSize(18, 18);

            toggleHostL->addWidget(toggle);
            toggleHostL->addStretch();

            items->setItemWidget(row, 0, toggleHost);
            row->setIcon(1, a->icon());
            row->setText(2, a->text());
            row->setData(0, Qt::UserRole, QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(a)));

            QObject::connect(toggle, &QAbstractButton::toggled, items, [this, a](bool on) {
                setActionEnabled(a, on);
                rebuildTiles();
            });
        }
        items->setEnabled(catEnabled);
    };

    // Now that repopulateItems exists, wire category toggles to update both
    // persistence and the right-hand list.
    for (int i = 0; i < categories->count(); ++i) {
        QListWidgetItem* it = categories->item(i);
        QWidget* row = it ? categories->itemWidget(it) : nullptr;
        if (!it || !row) {
            continue;
        }
        auto* toggle = row->findChild<CosmicCheckboxToggle*>();
        if (!toggle) {
            continue;
        }
        const ToolbarCategory cat = static_cast<ToolbarCategory>(it->data(Qt::UserRole).toInt());
        QObject::connect(toggle, &QAbstractButton::toggled, popup, [this, cat, repopulateItems](bool on) {
            setCategoryEnabled(cat, on);
            rebuildTiles();
            repopulateItems();
        });
    }

    QObject::connect(editToggle, &QAbstractButton::toggled, popup, [this](bool on) {
        setEditMode(on);
        rebuildTiles();
    });

    QObject::connect(categories, &QListWidget::currentItemChanged, popup,
                     [repopulateItems](QListWidgetItem*, QListWidgetItem*) {
                         repopulateItems();
                     });

    // Category and item toggles are handled by CosmicCheckboxToggle widgets.

    repopulateItems();

    popup->move(globalPos);
    popup->show();
    popup->raise();
    popup->activateWindow();
    mTilesPopup = popup;
}

namespace {
QColor pinPillBg()
{
    return QColor(0x2a, 0x2a, 0x2a, 0xff);
}

class TileWidget : public QWidget
{
public:
    explicit TileWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMouseTracking(true);
        setProperty("CosmicIsTile", true);
        // Default: empty/unassigned tile slot.
        setProperty("CosmicTileGroupId", -1);
        setProperty("CosmicTileGroupIndex", -1);

        auto* l = new QHBoxLayout(this);
        l->setContentsMargins(4, 2, 4, 2);
        l->setSpacing(0);
        mLayout = l;
        setLayout(l);

        setFixedHeight(26);
        setFixedWidth(40);
    }

    void setContent(QWidget* w)
    {
        if (!w || !mLayout) {
            return;
        }
        w->setParent(this);
        w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        const QSize iconSize(20, 20);
        if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
            btn->setIconSize(iconSize);
        }
        const auto buttons = w->findChildren<QAbstractButton*>();
        for (QAbstractButton* b : buttons) {
            if (b) {
                b->setIconSize(iconSize);
            }
        }

        mLayout->addWidget(w, 0, Qt::AlignVCenter);
        updateGeometry();
    }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        Q_UNUSED(e);
    }

private:
    QHBoxLayout* mLayout = nullptr;
};
} // namespace

CosmicControlCenter::CosmicControlCenter(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("CosmicControlCenter"));
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(mPreferredHeight);
    setMouseTracking(true);

    auto* root = new QHBoxLayout(this);
    // Keep the first control fully inside the window on macOS, where the
    // frameless app window can sit flush against the display edge.
    root->setContentsMargins(20, 0, 8, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    mScrollArea = scroll;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setAttribute(Qt::WA_NoSystemBackground, true);
    scroll->setAutoFillBackground(false);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(false);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    scroll->setFixedHeight(mPreferredHeight);

    // Ensure the scroll area itself never paints an opaque background.
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea{background:transparent;}"
        "QScrollArea QWidget{background:transparent;}"));

    auto* content = new QWidget(scroll);
    mScrollContent = content;
    content->setObjectName(QStringLiteral("CosmicControlCenterContent"));
    content->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    content->setAttribute(Qt::WA_NoSystemBackground, true);
    content->setAutoFillBackground(false);

    auto* row = new QHBoxLayout(content);
    row->setContentsMargins(20, 0, 0, 0);
    row->setSpacing(6);
    mPinsLayout = row;
    content->setLayout(row);
    scroll->setWidget(content);

    root->addWidget(scroll);
    setLayout(root);

    loadTileLayoutSettings();
    if (scroll->viewport()) {
        scroll->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        scroll->viewport()->setAutoFillBackground(false);
        scroll->viewport()->setStyleSheet(QStringLiteral("background:transparent;"));
        scroll->viewport()->installEventFilter(this);
        scroll->viewport()->setMouseTracking(true);
    }
}

void CosmicControlCenter::setEditor(Editor* editor)
{
    if (mEditor == editor) {
        return;
    }
    mEditor = editor;
    rebuildTiles();
}

void CosmicControlCenter::setEditMode(bool on)
{
    if (mEditMode == on) {
        return;
    }

    mEditMode = on;

    applyEditModeInteractionOverride();

    if (!mEditMode) {
        mDragArmed = false;
        mDragging = false;
        mDraggingGroupId = -1;
        saveTileLayoutSettings();
    }
    update();
}

void CosmicControlCenter::applyEditModeInteractionOverride()
{
    // In edit mode the header contents must behave like draggable objects,
    // not interactive controls. Force all child widgets (buttons) to become
    // transparent to mouse so the viewport drag logic always receives input.
    if (!mScrollContent) {
        return;
    }

    const auto widgets = mScrollContent->findChildren<QWidget*>();
    for (QWidget* w : widgets) {
        if (!w) {
            continue;
        }
        if (w->property("CosmicIsTile").toBool()) {
            continue;
        }
        if (w == mScrollContent) {
            continue;
        }
        if (qobject_cast<QScrollBar*>(w)) {
            continue;
        }
        w->setAttribute(Qt::WA_TransparentForMouseEvents, mEditMode);
    }
}

void CosmicControlCenter::loadTileLayoutSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    mPenGroupStart = settings.value(QStringLiteral("CosmicControlCenter/PenGroupStart"), mPenGroupStart).toInt();
    mAutoFillGroupStart = settings.value(QStringLiteral("CosmicControlCenter/AutoFillGroupStart"), mAutoFillGroupStart).toInt();
    normalizeGroupPositions();
}

void CosmicControlCenter::saveTileLayoutSettings() const
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(QStringLiteral("CosmicControlCenter/PenGroupStart"), mPenGroupStart);
    settings.setValue(QStringLiteral("CosmicControlCenter/AutoFillGroupStart"), mAutoFillGroupStart);
    settings.sync();
}

void CosmicControlCenter::normalizeGroupPositions()
{
    const int penSpan = 6;
    const int autoSpan = 6;
    const int maxIndex = qMax(0, mLogicalTileCount - 1);

    mPenGroupStart = qBound(0, mPenGroupStart, qMax(0, mLogicalTileCount - penSpan));
    mAutoFillGroupStart = qBound(0, mAutoFillGroupStart, qMax(0, mLogicalTileCount - autoSpan));

    const int penEnd = mPenGroupStart + penSpan - 1;
    const int autoEnd = mAutoFillGroupStart + autoSpan - 1;
    Q_UNUSED(maxIndex);

    if (!(penEnd < mAutoFillGroupStart || autoEnd < mPenGroupStart)) {
        if (mPenGroupStart <= mAutoFillGroupStart) {
            mAutoFillGroupStart = qMin(mLogicalTileCount - autoSpan, penEnd + 1);
        } else {
            mPenGroupStart = qMax(0, mAutoFillGroupStart - penSpan);
        }
    }
}

int CosmicControlCenter::tileIndexAtViewportPos(const QPoint& viewportPos) const
{
    if (!mScrollArea) {
        return -1;
    }
    const int x = viewportPos.x() + mScrollArea->horizontalScrollBar()->value();
    const int tileW = 40;
    const int spacing = 6;
    const int step = tileW + spacing;
    if (step <= 0) {
        return -1;
    }
    const int idx = x / step;
    if (idx < 0 || idx >= mLogicalTileCount) {
        return -1;
    }
    return idx;
}

void CosmicControlCenter::updateDragAutoScroll(const QPoint& viewportPos)
{
    if (!mScrollArea || !mScrollArea->horizontalScrollBar()) {
        return;
    }
    QScrollBar* sb = mScrollArea->horizontalScrollBar();
    const int margin = 24;
    const int maxStep = 18;
    int delta = 0;
    if (viewportPos.x() < margin) {
        delta = -maxStep;
    } else if (viewportPos.x() > (mScrollArea->viewport()->width() - margin)) {
        delta = maxStep;
    }
    if (delta != 0) {
        sb->setValue(sb->value() + delta);
    }
}

bool CosmicControlCenter::isPresetColor(const QColor& color) const
{
    return mPenPalette ? mPenPalette->isPresetColor(color) : false;
}

void CosmicControlCenter::setColorFromExternal(const QColor& color)
{
    if (!mPenPalette) {
        return;
    }
    QSignalBlocker block(mPenPalette);
    mPenPalette->activateFirstButtonAndSetColor(color);
}

void CosmicControlCenter::setToolbars(QToolBar* mainToolbar, QToolBar* viewToolbar, QToolBar* overlayToolbar)
{
    if (mMainToolbar == mainToolbar && mViewToolbar == viewToolbar && mOverlayToolbar == overlayToolbar) {
        return;
    }
    mMainToolbar = mainToolbar;
    mViewToolbar = viewToolbar;
    mOverlayToolbar = overlayToolbar;
    rebuildTiles();
}

void CosmicControlCenter::rebuildTiles()
{
    if (!mPinsLayout) {
        return;
    }

    while (QLayoutItem* item = mPinsLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    mPenPalette = nullptr;
    mAutoFillToggle = nullptr;

    // Logical tile space.
    const int kLogicalTileCount = mLogicalTileCount;
    int lastUsedTileIndex = -1;

    const int penSpan = 6;
    const int autoSpan = 6;
    normalizeGroupPositions();
    lastUsedTileIndex = qMax(lastUsedTileIndex, mPenGroupStart + penSpan - 1);
    lastUsedTileIndex = qMax(lastUsedTileIndex, mAutoFillGroupStart + autoSpan - 1);

    // Append toolbar-driven tiles after the grouped palettes.
    QVector<QAction*> tileActions;
    if (isCategoryEnabled(ToolbarCategory::Main)) {
        for (QAction* a : actionsForCategory(ToolbarCategory::Main)) if (a && isActionEnabled(a)) tileActions.push_back(a);
    }
    if (isCategoryEnabled(ToolbarCategory::View)) {
        for (QAction* a : actionsForCategory(ToolbarCategory::View)) if (a && isActionEnabled(a)) tileActions.push_back(a);
    }
    if (isCategoryEnabled(ToolbarCategory::Overlay)) {
        for (QAction* a : actionsForCategory(ToolbarCategory::Overlay)) if (a && isActionEnabled(a)) tileActions.push_back(a);
    }

    // Apply stable, persisted action order (if we have one).
    {
        const QStringList order = loadActionOrder();
        if (!order.isEmpty() && !tileActions.isEmpty()) {
            QHash<QString, int> rank;
            for (int i = 0; i < order.size(); ++i) {
                rank.insert(order[i], i);
            }
            std::stable_sort(tileActions.begin(), tileActions.end(), [this, &rank](QAction* a, QAction* b) {
                const int ra = rank.value(actionKey(a), 1000000);
                const int rb = rank.value(actionKey(b), 1000000);
                return ra < rb;
            });
        }
    }

    int needed = lastUsedTileIndex + tileActions.size();
    lastUsedTileIndex = qMin(kLogicalTileCount - 1, qMax(lastUsedTileIndex, needed));
    const int physicalTileCount = qBound(1, lastUsedTileIndex + 1, kLogicalTileCount);

    QVector<TileWidget*> tiles;
    tiles.reserve(physicalTileCount);
    for (int i = 0; i < physicalTileCount; ++i) {
        auto* tile = new TileWidget(mScrollContent ? mScrollContent : this);
        tiles.push_back(tile);
        mPinsLayout->addWidget(tile, 0, Qt::AlignVCenter);
    }

    // Ensure the scroll content width reflects only the used span (so the
    // scrollbar doesn't pretend the entire 56 tiles exist when they're blank).
    if (mScrollContent) {
        const int tileW = tiles.isEmpty() ? 40 : tiles.first()->width();
        const int spacing = mPinsLayout ? mPinsLayout->spacing() : 0;
        const int leftInset = mPinsLayout ? mPinsLayout->contentsMargins().left() : 0;
        const int w = qMax(0, leftInset + physicalTileCount * tileW + qMax(0, physicalTileCount - 1) * spacing);
        mScrollContent->setFixedSize(w, mPreferredHeight);
    }

    // Palette is the data/logic owner; its 6 buttons get hosted into the pen group.
    {
        auto* palette = new PenColorPalette(this);
        mPenPalette = palette;
        palette->detachForExternalLayout();
        palette->setVisible(false);

        const QList<CosmicCheckboxToggle*> buttons = palette->colorButtons();
        const int count = qMin(buttons.size(), 6);
        for (int i = 0; i < count; ++i) {
            const int tileIndex = mPenGroupStart + i;
            if (tileIndex < 0 || tileIndex >= tiles.size() || !tiles[tileIndex] || !buttons[i]) {
                continue;
            }
            tiles[tileIndex]->setProperty("CosmicTileGroupId", 0);
            tiles[tileIndex]->setProperty("CosmicTileGroupIndex", i);
            tiles[tileIndex]->setContent(buttons[i]);
        }

        if (QWidget* sep = palette->firstSeparatorWidget()) {
            sep->setVisible(false);
        }

        connect(palette, &PenColorPalette::colorChanged, this, &CosmicControlCenter::colorChanged);
    }

    // Auto-fill toggle is split into 6 grouped tiles: [master][color0..4].
    if (mEditor) {
        auto* toggleOwner = new AutoLineColorToggle(mEditor, this);
        mAutoFillToggle = toggleOwner;
        toggleOwner->detachForExternalLayout();
        toggleOwner->setVisible(false);

        CosmicCheckboxToggle* master = toggleOwner->masterToggle();
        const QList<CosmicCheckboxToggle*> colorButtons = toggleOwner->colorButtons();
        const int baseIndex = mAutoFillGroupStart;

        if (master && baseIndex < tiles.size() && tiles[baseIndex]) {
            tiles[baseIndex]->setProperty("CosmicTileGroupId", 1);
            tiles[baseIndex]->setProperty("CosmicTileGroupIndex", 0);
            tiles[baseIndex]->setContent(master);
        }

        for (int i = 0; i < colorButtons.size() && i < 5; ++i) {
            const int tileIndex = baseIndex + 1 + i;
            if (tileIndex < 0 || tileIndex >= tiles.size() || !tiles[tileIndex] || !colorButtons[i]) {
                continue;
            }
            tiles[tileIndex]->setProperty("CosmicTileGroupId", 1);
            tiles[tileIndex]->setProperty("CosmicTileGroupIndex", 1 + i);
            tiles[tileIndex]->setContent(colorButtons[i]);
        }

        if (QWidget* sep = toggleOwner->masterSeparatorWidget()) {
            sep->setVisible(false);
        }
    }

    // Host legacy toolbar actions into the remaining tile slots.
    auto isGroupSlot = [&](int idx) {
        const int penSpan2 = 6;
        const int autoSpan2 = 6;
        if (idx >= mPenGroupStart && idx < (mPenGroupStart + penSpan2)) return true;
        if (idx >= mAutoFillGroupStart && idx < (mAutoFillGroupStart + autoSpan2)) return true;
        return false;
    };

    int nextIdx = 0;
    for (QAction* a : tileActions) {
        if (!a) {
            continue;
        }

        while (nextIdx < tiles.size() && isGroupSlot(nextIdx)) {
            ++nextIdx;
        }
        if (nextIdx < 0 || nextIdx >= tiles.size()) {
            break;
        }

        // Only place into empty (non-group) tiles.
        if (tiles[nextIdx]->property("CosmicTileGroupId").toInt() >= 0) {
            ++nextIdx;
            continue;
        }

        // Mark tile as an action slot so it can be drag-reordered and persisted.
        const QString aKey = actionKey(a);
        tiles[nextIdx]->setProperty("CosmicActionKey", aKey);

        auto* btn = new QToolButton(this);
        btn->setDefaultAction(a);
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setAutoRaise(true);
        btn->setIconSize(QSize(20, 20));
        btn->setStyleSheet(QStringLiteral("QToolButton{background:transparent;border:none;padding:0px;margin:0px;}"));
        tiles[nextIdx]->setContent(btn);
        ++nextIdx;
    }

    updateGeometry();
    update();

    // IMPORTANT: rebuildTiles recreates the tile widgets and re-parents all
    // controls, so the edit-mode interaction override must be re-applied.
    if (mEditMode) {
        applyEditModeInteractionOverride();
    }
}

void CosmicControlCenter::contextMenuEvent(QContextMenuEvent* event)
{
    if (!event) {
        return;
    }
    showTilesPopup(event->globalPos());
    event->accept();
}

void CosmicControlCenter::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
}

void CosmicControlCenter::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
}

void CosmicControlCenter::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
}

void CosmicControlCenter::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!mInitialScrollPositionPending || !mScrollArea) {
        return;
    }

    // The final scroll range is only known after the window has been shown
    // and its geometry has settled. Reset then, so the first tile is visible
    // on frameless macOS windows as well as after a display-size change.
    QTimer::singleShot(0, this, [this]() {
        if (!mScrollArea || !mScrollArea->horizontalScrollBar()) {
            return;
        }
        auto* bar = mScrollArea->horizontalScrollBar();
        bar->setValue(bar->minimum());
        mInitialScrollPositionPending = false;
    });
}

bool CosmicControlCenter::eventFilter(QObject* watched, QEvent* event)
{
    if (!mScrollArea || watched != mScrollArea->viewport()) {
        return QWidget::eventFilter(watched, event);
    }

    if (!mEditMode) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const QPoint vp = me->pos();
            mDragPressViewportPos = vp;
            const int idx = tileIndexAtViewportPos(vp);
            if (idx < 0) {
                return QWidget::eventFilter(watched, event);
            }

            auto tileAtLogicalIndex = [&](int logicalIdx) -> QWidget* {
                if (!mPinsLayout) return nullptr;
                int tileIdx = 0;
                for (int i = 0; i < mPinsLayout->count(); ++i) {
                    QWidget* w = mPinsLayout->itemAt(i) ? mPinsLayout->itemAt(i)->widget() : nullptr;
                    if (!w || w == mDragPlaceholder) continue;
                    if (!w->property("CosmicIsTile").toBool()) continue;
                    if (tileIdx == logicalIdx) return w;
                    ++tileIdx;
                }
                return nullptr;
            };

            QWidget* pressedTile = tileAtLogicalIndex(idx);

            const int penSpan = 6;
            const int autoSpan = 6;
            const int penEnd = mPenGroupStart + penSpan - 1;
            const int autoEnd = mAutoFillGroupStart + autoSpan - 1;

            if (idx >= mPenGroupStart && idx <= penEnd) {
                mDragArmed = true;
                mDragging = false;
                mDraggingGroupId = 0;
                mDragGroupSpan = penSpan;
                mDragStartIndexOriginal = mPenGroupStart;
                mLastDragTargetIndex = mPenGroupStart;
                return true;
            }
            if (idx >= mAutoFillGroupStart && idx <= autoEnd) {
                mDragArmed = true;
                mDragging = false;
                mDraggingGroupId = 1;
                mArmedTileWidget = nullptr;
                mDragGroupSpan = autoSpan;
                mDragStartIndexOriginal = mAutoFillGroupStart;
                mLastDragTargetIndex = mAutoFillGroupStart;
                return true;
            }

            // Single action tile drag.
            if (pressedTile && pressedTile->property("CosmicTileGroupId").toInt() < 0) {
                const QString aKey = pressedTile->property("CosmicActionKey").toString();
                if (!aKey.isEmpty()) {
                    mDragArmed = true;
                    mDragging = false;
                    mDraggingGroupId = -1;
                    mArmedTileWidget = pressedTile;
                    mDragGroupSpan = 1;
                    mDragStartIndexOriginal = idx;
                    mLastDragTargetIndex = idx;
                    return true;
                }
            }
        }
    }

    if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (mDragging) {
            updateGroupDrag(me->pos());
            return true;
        }
        if (mDragArmed) {
            const int dragThreshold = qApp ? qApp->startDragDistance() : 6;
            if ((me->pos() - mDragPressViewportPos).manhattanLength() < dragThreshold) {
                return true;
            }

            const int gid = mDraggingGroupId;
            mDragArmed = false;
            if (gid >= 0) {
                beginGroupDrag(gid, me->pos());
            } else if (mArmedTileWidget) {
                QWidget* t = mArmedTileWidget;
                mArmedTileWidget = nullptr;
                beginSingleTileDrag(t, me->pos());
            }
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && mDragging) {
            endGroupDrag();
            return true;
        }
        if (me->button() == Qt::LeftButton && mDragArmed) {
            mDragArmed = false;
            mDraggingGroupId = -1;
            mArmedTileWidget = nullptr;
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CosmicControlCenter::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    QColor bg = pillBg();
    if (mEditMode) {
        bg = QColor(0x32, 0x1f, 0x4f, 0xff);
    }
    p.setBrush(bg);
    p.drawRect(rect());
}
