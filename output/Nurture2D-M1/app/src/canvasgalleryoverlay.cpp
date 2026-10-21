#include "canvasgalleryoverlay.h"

#include "cosmicconfirmpopup.h"
#include "cosmicwindowheader.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmichslider.h"
#include "../CUI/cosmicuimetrics.h"

#include "pencildef.h"

#include <QSettings>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSvgRenderer>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QScrollBar>
#include <QTimer>
#include <QMap>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QSignalBlocker>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int kLogoH        = 44;  // rendered logo height
static constexpr int kLogoGap      = 14;  // clearance each side of logo in the strip
static constexpr int kHeaderH           = 28;  // CosmicWindowHeader fixed height
static constexpr int kLogoPad           = 8;   // breathing room below logo before border line
static constexpr int kBorderSideInset   = 16;  // left/right/bottom inset from window edge
// Border top = header + full logo height + padding, so the logo sits entirely below the header
static constexpr int kBorderTopY        = kHeaderH + kLogoH + kLogoPad;

// ── Helpers kept for later phases ────────────────────────────────────────────
static QPixmap makeSquareThumb(const QImage& src, int size)
{
    if (src.isNull()) return QPixmap();
    QImage scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QRect crop((scaled.width() - size) / 2, (scaled.height() - size) / 2, size, size);
    return QPixmap::fromImage(scaled.copy(crop));
}

namespace {
class GallerySquircleDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!painter) { QStyledItemDelegate::paint(painter, option, index); return; }
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.state &= ~QStyle::State_HasFocus;
        const QRect r = opt.rect.adjusted(2, 2, -2, -2);
        const int rad = qMax(6, CosmicUiMetrics::cornerRadius());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        if (opt.state.testFlag(QStyle::State_Selected)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(opt.palette.color(QPalette::Highlight));
            painter->drawRoundedRect(r, rad, rad);
        } else if (opt.state.testFlag(QStyle::State_MouseOver)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0x4d, 0x4d, 0x4d));
            painter->drawRoundedRect(r, rad, rad);
        }
        painter->restore();
        opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
        opt.backgroundBrush = Qt::NoBrush;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

static QString pillScrollBarStyle()
{
    return QStringLiteral(
        "QScrollBar:vertical { background:#1f1f1f; border:none; border-radius:6px; width:12px; margin:0; }"
        "QScrollBar::handle:vertical { background:#505050; border-radius:6px; min-height:26px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background:transparent; height:0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }"
        "QScrollBar:horizontal { background:#1f1f1f; border:none; border-radius:6px; height:12px; margin:0; }"
        "QScrollBar::handle:horizontal { background:#505050; border-radius:6px; min-width:26px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background:transparent; width:0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background:transparent; }"
    );
}
} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────
CanvasGalleryOverlay::CanvasGalleryOverlay(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::NonModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(true);
    setMinimumSize(540, 420);
    resize(860, 640);
    setAcceptDrops(true);

    setShadowEnabled(false);
    setShadowPadding(0);
    setShadowStrength(0);
    setBorderWidth(2);
    setPanelColor(QColor(0x20, 0x20, 0x20));
    setBorderColor(CosmicColor::popupBorder());
    setPersistentSizeKey(QStringLiteral("CanvasGalleryOverlay/Size"));
    setDragRegionHeight(48);

    // Close button — top-left, absolute positioned inside the window
    mWindowHeader = new CosmicWindowHeader(this);
    mWindowHeader->setGeometry(12, 12, 80, 28);
    connect(mWindowHeader, &CosmicWindowHeader::closeRequested, this, [this]{ emit requestClose(); });

    // Load logo SVG (same asset as CosmicMenuBar), tinted to text color
    QSvgRenderer svg(QStringLiteral(":/Windows&AppIcons/MenubarIcon 1.svg"));
    if (svg.isValid()) {
        const QSize sz = svg.defaultSize();
        const int lw = sz.height() > 0 ? qRound(sz.width() * (double)kLogoH / sz.height()) : kLogoH;
        mLogoPixmap = QPixmap(lw, kLogoH);
        mLogoPixmap.fill(Qt::transparent);
        QPainter lp(&mLogoPixmap);
        svg.render(&lp);
        lp.end();
        // Tint to white (text color) using SourceIn compositing
        lp.begin(&mLogoPixmap);
        lp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        lp.fillRect(mLogoPixmap.rect(), palette().color(QPalette::Text));
        lp.end();
    }

    // ── Content layout (inside the inner border area) ─────────────────────────
    // Root widget positioned to match the inner border rect
    mContentWidget = new QWidget(this);
    mContentWidget->setStyleSheet("QWidget { background: transparent; }");
    // Force full repaints to prevent partial-update glitches during resize/interaction
    mContentWidget->setAttribute(Qt::WA_StaticContents, false);

    auto* rootVLayout = new QVBoxLayout(mContentWidget);
    // Extra top padding so the logo (centered on the border line) has full breathing room
    rootVLayout->setContentsMargins(10, kLogoH / 2 + 10, 10, 10);
    rootVLayout->setSpacing(8);

    const QString areaBg = QStringLiteral("#1a1a1a");
    const int     areaRad = CosmicUiMetrics::windowCornerRadius();
    const QString squircle = QStringLiteral(
        "border: 1px solid #3c3c3c;"
        "border-radius: %1px;"
        "background: #1a1a1a;").arg(areaRad);

    // ── Top row: search bar — full width ─────────────────────────────────────
    mSearchBar = new QWidget(mContentWidget);
    mSearchBar->setObjectName("GallerySearchBar");
    mSearchBar->setStyleSheet(
        QStringLiteral("QWidget#GallerySearchBar { %1 }").arg(squircle));
    mSearchBar->setFixedHeight(36);

    auto* searchBarLayout = new QHBoxLayout(mSearchBar);
    searchBarLayout->setContentsMargins(8, 0, 8, 0);
    searchBarLayout->setSpacing(6);

    auto* searchIcon = new QLabel(mSearchBar);
    searchIcon->setPixmap(QIcon(":/FileBrowser/Search.svg").pixmap(16, 16));
    searchIcon->setFixedSize(16, 16);
    searchBarLayout->addWidget(searchIcon);

    mSearch = new QLineEdit(mSearchBar);
    mSearch->setPlaceholderText(tr("Search..."));
    mSearch->setStyleSheet(
        "QLineEdit { background: transparent; border: none; color: #e6e6e6; }");
    searchBarLayout->addWidget(mSearch, 1);

    rootVLayout->addWidget(mSearchBar);

    // ── Main row: folder list (30%) + gap + gallery area (70%) ───────────────
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(8);

    mFolderListArea = new QWidget(mContentWidget);
    mFolderListArea->setObjectName("GalleryFolderList");
    mFolderListArea->setStyleSheet(
        QStringLiteral("QWidget#GalleryFolderList { %1 }").arg(squircle));

    auto* folderLayout = new QVBoxLayout(mFolderListArea);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(0);

    mCategoryList = new QListWidget(mFolderListArea);
    mCategoryList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; color: #e6e6e6; outline: none; }"
        "QListWidget::item { padding: 4px 6px; border-radius: 4px; }"
        "QListWidget::item:selected { background: #3a3a3a; }"
        "QListWidget::item:hover { background: #2e2e2e; }"));
    mCategoryList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mCategoryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (auto* vs = mCategoryList->verticalScrollBar()) {
        vs->setStyleSheet(pillScrollBarStyle());
        connect(vs, &QScrollBar::valueChanged, this, [this](int){ repositionCategoryButtons(); });
    }
    folderLayout->addWidget(mCategoryList, 1);

    mUniqueCategoriesCache = QStringList() << QStringLiteral("General");
    rebuildCategoryList();

    // Gallery area: grid on top, toolbar strip at bottom
    mGalleryArea = new QWidget(mContentWidget);
    mGalleryArea->setObjectName("GalleryMainArea");
    mGalleryArea->setStyleSheet(
        QStringLiteral("QWidget#GalleryMainArea { %1 }").arg(squircle));

    auto* galleryAreaLayout = new QVBoxLayout(mGalleryArea);
    galleryAreaLayout->setContentsMargins(4, 4, 4, 4);
    galleryAreaLayout->setSpacing(4);

    // ── Gallery toolbar: slider (left) + trash (right) ────────────────────────
    auto* galleryToolbar = new QWidget(mGalleryArea);
    galleryToolbar->setStyleSheet("QWidget { background: transparent; }");
    auto* toolbarLayout = new QHBoxLayout(galleryToolbar);
    toolbarLayout->setContentsMargins(2, 0, 2, 0);
    toolbarLayout->setSpacing(6);

    {
        QSettings s(PENCIL2D, PENCIL2D);
        mThumbSize = qBound(72, s.value(QStringLiteral("GalleryThumbSize"), mThumbSize).toInt(), 320);
    }
    mThumbSlider = new CosmicHSlider(galleryToolbar);
    mThumbSlider->setOrientation(Qt::Horizontal);
    mThumbSlider->setRange(72, 320);
    mThumbSlider->setValue(mThumbSize);
    mThumbSlider->setLabelMode(CosmicHSlider::Textless);
    mThumbSlider->setToolTip(tr("Thumbnail size"));

    mTrashButton = new CosmicButton(galleryToolbar);
    mTrashButton->setBordered(false);
    mTrashButton->setToolTip(tr("Delete selected"));
    mTrashButton->setIcon(QIcon(":/Interaction/trash.svg"));
    mTrashButton->setIconSize(QSize(16, 16));
    mTrashButton->setFixedSize(28, 28);

    toolbarLayout->addWidget(mThumbSlider, 1);
    toolbarLayout->addWidget(mTrashButton);
    galleryAreaLayout->addWidget(galleryToolbar);

    mGrid = new QListWidget(mGalleryArea);
    mGrid->setAcceptDrops(true);
    mGrid->setDragDropMode(QAbstractItemView::NoDragDrop);
    mGrid->viewport()->setAcceptDrops(true);
    mGrid->viewport()->installEventFilter(this);
    galleryAreaLayout->addWidget(mGrid, 1);

    bottomRow->addWidget(mFolderListArea, 30);
    bottomRow->addWidget(mGalleryArea, 70);
    rootVLayout->addLayout(bottomRow, 1);

    // ── Wire up all interactions ──────────────────────────────────────────────
    connect(mSearch, &QLineEdit::textChanged, this, &CanvasGalleryOverlay::filterItems);
    connect(mCategoryList, &QListWidget::currentRowChanged, this, [this](int){
        applyCategoryFilter();
        emit activeCategoryChanged(mActiveCategory);
    });

    connect(mThumbSlider, &QSlider::valueChanged, this, [this](int v){ setThumbSize(v); });

    connect(mTrashButton, &QPushButton::clicked, this, [this]{
        QList<int> indices;
        for (QListWidgetItem* it : mGrid->selectedItems())
            indices.push_back(it->data(Qt::UserRole).toInt());
        if (indices.isEmpty()) return;
        QPointer<CanvasGalleryOverlay> self(this);
        QTimer::singleShot(0, this, [self, indices]() {
            if (!self) return;
            CosmicConfirmPopup confirm(self);
            confirm.setBodyText(indices.size() == 1
                ? tr("Delete this image? This cannot be undone.")
                : tr("Delete %1 images? This cannot be undone.").arg(indices.size()));
            confirm.setConfirmText(tr("Delete"));
            confirm.setCancelText(tr("Cancel"));
            if (confirm.exec() != QDialog::Accepted) return;
            emit self->requestDeleteSelected(indices);
            // Fallback selection: select the item that lands at the same visual
            // position after the grid is rebuilt (handled in applyCategoryFilter
            // via mCurrentIndexCache update from galleryContentUpdated).
        });
    });

    connect(mGrid, &QListWidget::itemActivated, this, [this](QListWidgetItem* it){
        if (!it) return;
        bool ok = false;
        const int idx = it->data(Qt::UserRole).toInt(&ok);
        if (ok) emit imageActivated(idx);
    });
    connect(mGrid, &QListWidget::itemClicked, this, [this](QListWidgetItem* it){
        if (!it) return;
        bool ok = false;
        const int idx = it->data(Qt::UserRole).toInt(&ok);
        if (ok) emit imageActivated(idx);
    });

    // ── Grid setup ────────────────────────────────────────────────────────────
    mGrid->show();
    mGrid->setViewMode(QListView::IconMode);
    mGrid->setResizeMode(QListView::Adjust);
    mGrid->setMovement(QListView::Static);
    mGrid->setWrapping(true);
    mGrid->setIconSize(QSize(mThumbSize, mThumbSize));
    mGrid->setGridSize(QSize(mThumbSize + mSpacing, mThumbSize + 28 + mSpacing));
    mGrid->setSpacing(mSpacing);
    mGrid->setUniformItemSizes(true);
    mGrid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mGrid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mGrid->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mGrid->setStyleSheet("QListWidget { background: transparent; border: none; color: #e6e6e6; }");
    mGrid->setItemDelegate(new GallerySquircleDelegate(mGrid));
    if (auto* vs = mGrid->verticalScrollBar())   vs->setStyleSheet(pillScrollBarStyle());
    if (auto* hs = mGrid->horizontalScrollBar()) hs->setStyleSheet(pillScrollBarStyle());

    updateContentGeometry();
}

// ── Paint: border + logo ──────────────────────────────────────────────────────
void CanvasGalleryOverlay::paintEvent(QPaintEvent* e)
{
    CosmicPopupWindow::paintEvent(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor bgColor    = panelColor();           // window background — same color for the strip
    const QColor borderColor(0x3c, 0x3c, 0x3c);
    const qreal  bw  = 1.5;
    const int    rad = CosmicUiMetrics::windowCornerRadius();

    // Inner border rect — top starts below the header area
    const QRectF br = QRectF(rect()).adjusted(
        kBorderSideInset, kBorderTopY, -kBorderSideInset, -kBorderSideInset);

    // 1. Draw the full rounded border
    p.setPen(QPen(borderColor, bw));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(br, rad, rad);

    // 2. Paint a background-colored strip over the top edge center to "erase" it
    const qreal cx       = br.center().x();
    const qreal topY     = br.top();
    const qreal logoW    = mLogoPixmap.isNull() ? 0.0 : (double)mLogoPixmap.width();
    const qreal stripW   = logoW + kLogoGap * 2;
    const qreal stripH   = bw + 2.0;   // just enough to cover the border line cleanly
    const QRectF stripRect(cx - stripW / 2.0, topY - stripH / 2.0, stripW, stripH);

    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.setRenderHint(QPainter::Antialiasing, false);   // hard edges for the cover strip
    p.drawRect(stripRect);

    // 3. Draw the logo centered on the border line (half above, half below)
    if (!mLogoPixmap.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const int lx = qRound(cx - logoW / 2.0);
        const int ly = qRound(topY - kLogoH / 2.0);  // vertically centered on the border line
        p.drawPixmap(lx, ly, mLogoPixmap);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
void CanvasGalleryOverlay::setImages(const QVector<QImage>& images, const QStringList& labels,
                                     int currentIndex, const QStringList& categories)
{
    mImagesCache       = images;
    mLabelsCache       = labels;
    mCurrentIndexCache = currentIndex;

    // categories is parallel to images (per-image category label)
    // If not provided, everything goes to "General"
    mCategoriesCache.clear();
    mCategoriesCache.reserve(images.size());
    for (int i = 0; i < images.size(); ++i)
        mCategoriesCache.append(i < categories.size() ? categories[i] : QStringLiteral("General"));

    // Unique ordered list — "General" always first
    mUniqueCategoriesCache.clear();
    mUniqueCategoriesCache.append(QStringLiteral("General"));
    for (const QString& c : mCategoriesCache) {
        if (!mUniqueCategoriesCache.contains(c))
            mUniqueCategoriesCache.append(c);
    }

    rebuildCategoryList();
    applyCategoryFilter();
}

void CanvasGalleryOverlay::showOverlay()
{
    showOverlayAt(parentWidget()
                      ? parentWidget()->mapToGlobal(parentWidget()->rect().center())
                      : QPoint());
}

void CanvasGalleryOverlay::showOverlayAt(const QPoint& globalCenter)
{
    const QPoint center = globalCenter.isNull()
                              ? (parentWidget()
                                     ? parentWidget()->mapToGlobal(parentWidget()->rect().center())
                                     : QPoint(800, 450))
                              : globalCenter;
    showAt(center, Anchor::Center, true);
    if (mSearch) mSearch->setFocus();
}

void CanvasGalleryOverlay::hideOverlay()
{
    hide();
}

void CanvasGalleryOverlay::updateContentGeometry()
{
    if (!mContentWidget) return;
    // Match the inner border rect exactly
    const QRect cr(
        kBorderSideInset,
        kBorderTopY,
        width()  - kBorderSideInset * 2,
        height() - kBorderTopY - kBorderSideInset);
    mContentWidget->setGeometry(cr);
}

void CanvasGalleryOverlay::resizeEvent(QResizeEvent* e)
{
    CosmicPopupWindow::resizeEvent(e);
    updateContentGeometry();
    update();
    QTimer::singleShot(0, this, &CanvasGalleryOverlay::repositionCategoryButtons);
}

// ── Category list ─────────────────────────────────────────────────────────────
void CanvasGalleryOverlay::rebuildCategoryList()
{
    if (!mCategoryList) return;
    const QString prev = mActiveCategory;

    // Remove old trash buttons
    for (auto* btn : mCategoryTrashBtns) btn->deleteLater();
    mCategoryTrashBtns.clear();
    mCategoryList->clear();

    for (const QString& cat : mUniqueCategoriesCache) {
        auto* item = new QListWidgetItem(mCategoryList);
        item->setData(Qt::UserRole, cat);
        item->setSizeHint(QSize(0, 28));
        mCategoryList->addItem(item);

        // Label — transparent so the list item receives clicks for selection
        auto* rowWidget = new QWidget();
        rowWidget->setStyleSheet("QWidget { background: transparent; }");
        rowWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(8, 2, 4, 2);
        auto* label = new QLabel(cat, rowWidget);
        label->setStyleSheet("QLabel { color: #e6e6e6; background: transparent; }");
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        rowLayout->addWidget(label, 1);
        mCategoryList->setItemWidget(item, rowWidget);

        // Trash button parented to viewport — sits on top, receives its own clicks
        auto* btn = new CosmicButton(mCategoryList->viewport());
        btn->setBordered(false);
        btn->setIcon(QIcon(":/Interaction/trash.svg"));
        btn->setIconSize(QSize(12, 12));
        btn->setFixedSize(20, 20);
        btn->setToolTip(cat == QStringLiteral("General")
                            ? tr("Clear General images")
                            : tr("Clear and remove \"%1\"").arg(cat));
        btn->show();
        mCategoryTrashBtns[cat] = btn;

        const QString catCopy = cat;
        connect(btn, &QPushButton::clicked, this, [this, catCopy]{
            // Collect global indices for this specific category
            QList<int> indices;
            for (int i = 0; i < mImagesCache.size(); ++i) {
                const QString& imgCat = (i < mCategoriesCache.size())
                                            ? mCategoriesCache[i]
                                            : QStringLiteral("General");
                if (imgCat == catCopy) indices.append(i);
            }
            const bool isGeneral = (catCopy == QStringLiteral("General"));
            const QString msg = isGeneral
                ? tr("Clear all images in \"General\"? This cannot be undone.")
                : tr("Delete category \"%1\" and all its images? This cannot be undone.").arg(catCopy);
            QPointer<CanvasGalleryOverlay> self(this);
            QTimer::singleShot(0, this, [self, indices, msg, isGeneral, catCopy]() {
                if (!self) return;
                CosmicConfirmPopup confirm(self);
                confirm.setBodyText(msg);
                confirm.setConfirmText(isGeneral ? tr("Clear") : tr("Delete"));
                confirm.setCancelText(tr("Cancel"));
                if (confirm.exec() != QDialog::Accepted) return;
                // Switch category list to General before deleting so the overlay
                // never stays on a category that's about to disappear.
                if (!isGeneral) {
                    for (int i = 0; i < self->mCategoryList->count(); ++i) {
                        if (self->mCategoryList->item(i)->data(Qt::UserRole).toString()
                                == QStringLiteral("General")) {
                            self->mCategoryList->setCurrentRow(i);
                            break;
                        }
                    }
                }
                if (!indices.isEmpty()) emit self->requestDeleteSelected(indices);
                if (!isGeneral) emit self->requestDeleteCategory(catCopy);
            });
        });
    }

    // Position buttons after layout settles
    QTimer::singleShot(0, this, &CanvasGalleryOverlay::repositionCategoryButtons);

    // Restore selection
    mActiveCategory = prev;
    for (int i = 0; i < mCategoryList->count(); ++i) {
        if (mCategoryList->item(i)->data(Qt::UserRole).toString() == mActiveCategory) {
            mCategoryList->setCurrentRow(i);
            return;
        }
    }
    if (mCategoryList->count() > 0)
        mCategoryList->setCurrentRow(0);
}

void CanvasGalleryOverlay::repositionCategoryButtons()
{
    if (!mCategoryList) return;
    for (int i = 0; i < mCategoryList->count(); ++i) {
        QListWidgetItem* item = mCategoryList->item(i);
        const QString cat = item->data(Qt::UserRole).toString();
        auto it = mCategoryTrashBtns.find(cat);
        if (it == mCategoryTrashBtns.end()) continue;
        const QRect r = mCategoryList->visualItemRect(item);
        it.value()->move(r.right() - 24, r.top() + (r.height() - 20) / 2);
        it.value()->raise();
    }
}

void CanvasGalleryOverlay::applyCategoryFilter()
{
    if (!mCategoryList) return;
    QListWidgetItem* sel = mCategoryList->currentItem();
    mActiveCategory = (sel && !sel->data(Qt::UserRole).toString().isEmpty())
                          ? sel->data(Qt::UserRole).toString()
                          : QStringLiteral("General");

    // Build filtered lists, storing GLOBAL indices so actions target the right images
    QVector<QImage> filteredImgs;
    QStringList filteredLabels;
    QList<int> globalIndices;   // parallel: filteredImgs[i] came from mImagesCache[globalIndices[i]]
    int filteredCurrent = -1;

    for (int i = 0; i < mImagesCache.size(); ++i) {
        const QString& cat = (i < mCategoriesCache.size()) ? mCategoriesCache[i] : QStringLiteral("General");
        if (mActiveCategory.isEmpty() || cat == mActiveCategory) {
            if (i == mCurrentIndexCache) filteredCurrent = filteredImgs.size();
            filteredImgs.append(mImagesCache[i]);
            filteredLabels.append(i < mLabelsCache.size() ? mLabelsCache[i] : QString());
            globalIndices.append(i);
        }
    }

    // Rebuild grid with global indices stored per item
    if (!mGrid) return;
    mGrid->clear();
    mGrid->setUpdatesEnabled(false);
    mGrid->setIconSize(QSize(mThumbSize, mThumbSize));
    mGrid->setGridSize(QSize(mThumbSize + mSpacing, mThumbSize + 28 + mSpacing));
    for (int i = 0; i < filteredImgs.size(); ++i) {
        QPixmap px = makeSquareThumb(filteredImgs[i], mThumbSize);
        QString label = filteredLabels[i].isEmpty() ? QString::number(i + 1) : filteredLabels[i];
        auto* item = new QListWidgetItem(QIcon(px), label);
        item->setData(Qt::UserRole, globalIndices[i]);  // ← global index
        item->setSizeHint(mGrid->gridSize());
        mGrid->addItem(item);
        if (i == filteredCurrent) item->setSelected(true);
    }
    mGrid->setUpdatesEnabled(true);

    // Fallback: if nothing is selected (current image was deleted or not in this
    // category), select the first visible item so the grid always has a selection.
    if (mGrid->selectedItems().isEmpty() && mGrid->count() > 0) {
        mGrid->item(0)->setSelected(true);
    }

    filterItems(mSearch ? mSearch->text() : QString());
}

// ── Disconnected feature implementations (ready for phase 2+) ────────────────
void CanvasGalleryOverlay::setThumbSize(int px)
{
    const int newPx = qBound(72, px, 320);
    if (newPx == mThumbSize) return;
    mThumbSize = newPx;
    QSettings s(PENCIL2D, PENCIL2D);
    s.setValue(QStringLiteral("GalleryThumbSize"), mThumbSize);
    if (mThumbSlider && mThumbSlider->value() != newPx) {
        QSignalBlocker b(mThumbSlider);
        mThumbSlider->setValue(newPx);
    }
    // Re-apply category filter so the grid respects both category and new thumb size
    applyCategoryFilter();
}

void CanvasGalleryOverlay::rebuildGrid(const QVector<QImage>& images, const QStringList& labels, int currentIndex)
{
    if (!mGrid) return;
    mGrid->clear();
    mGrid->setUpdatesEnabled(false);
    mGrid->setIconSize(QSize(mThumbSize, mThumbSize));
    mGrid->setGridSize(QSize(mThumbSize + mSpacing, mThumbSize + 28 + mSpacing));
    for (int i = 0; i < images.size(); ++i) {
        QPixmap px = makeSquareThumb(images[i], mThumbSize);
        QString label = (i < labels.size()) ? labels[i] : QString::number(i + 1);
        auto* item = new QListWidgetItem(QIcon(px), label);
        item->setData(Qt::UserRole, i);
        item->setSizeHint(mGrid->gridSize());
        mGrid->addItem(item);
        if (i == currentIndex) item->setSelected(true);
    }
    mGrid->setUpdatesEnabled(true);
}

void CanvasGalleryOverlay::filterItems(const QString& text)
{
    if (!mGrid) return;
    const QString q = text.trimmed();
    for (int i = 0; i < mGrid->count(); ++i) {
        QListWidgetItem* it = mGrid->item(i);
        it->setHidden(!q.isEmpty() && !it->text().contains(q, Qt::CaseInsensitive));
    }
}

// ── Drag and drop ─────────────────────────────────────────────────────────────
static bool mimeHasImages(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls()) return false;
    for (const QUrl& url : mime->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        QFileInfo info(path);
        if (info.isDir()) {
            QDir dir(path);
            for (const QFileInfo& fi : dir.entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot)) {
                if (QImageReader(fi.absoluteFilePath()).canRead()) return true;
            }
        } else if (QImageReader(path).canRead()) {
            return true;
        }
    }
    return false;
}

bool CanvasGalleryOverlay::eventFilter(QObject* watched, QEvent* e)
{
    // Forward drag/drop events from the grid viewport up to our own handlers
    if (mGrid && watched == mGrid->viewport()) {
        if (e->type() == QEvent::DragEnter) {
            dragEnterEvent(static_cast<QDragEnterEvent*>(e));
            return true;
        }
        if (e->type() == QEvent::DragMove) {
            dragMoveEvent(static_cast<QDragMoveEvent*>(e));
            return true;
        }
        if (e->type() == QEvent::Drop) {
            dropEvent(static_cast<QDropEvent*>(e));
            return true;
        }
        if (e->type() == QEvent::DragLeave) {
            return true;
        }
    }
    return CosmicPopupWindow::eventFilter(watched, e);
}

void CanvasGalleryOverlay::dragEnterEvent(QDragEnterEvent* e)
{
    if (mimeHasImages(e->mimeData()))
        e->acceptProposedAction();
    else
        e->ignore();
}

void CanvasGalleryOverlay::dragMoveEvent(QDragMoveEvent* e)
{
    if (mimeHasImages(e->mimeData()))
        e->acceptProposedAction();
    else
        e->ignore();
}

void CanvasGalleryOverlay::dropEvent(QDropEvent* e)
{
    if (!mimeHasImages(e->mimeData())) { e->ignore(); return; }
    emit requestImportMimeData(e->mimeData());
    e->acceptProposedAction();
}
