#include "cosmicassetgallery.h"
#include "cosmicwindowheader.h"
#include "cosmichslider.h"
#include "cosmicbutton.h"
#include "cosmicuimetrics.h"
#include "../app/src/cosmicconfirmpopup.h"

#include "../app/src/managers/brushpresetstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QApplication>
#include <QSettings>
#include <QShowEvent>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ── Scrollbar stylesheet (matches reference window) ───────────────────────────
static const char* kScrollBarStyle =
    "QScrollArea { background: transparent; border: none; }"
    "QScrollBar:vertical {"
    "  background: #1f1f1f; border: none; border-radius: 6px;"
    "  width: 10px; margin: 0px; }"
    "QScrollBar::handle:vertical {"
    "  background: #505050; border-radius: 5px; min-height: 24px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical"
    "  { background: transparent; height: 0px; }"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical"
    "  { background: transparent; }"
    "QScrollBar:horizontal {"
    "  background: #1f1f1f; border: none; border-radius: 6px;"
    "  height: 10px; margin: 0px; }"
    "QScrollBar::handle:horizontal {"
    "  background: #505050; border-radius: 5px; min-width: 24px; }"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal"
    "  { background: transparent; width: 0px; }"
    "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal"
    "  { background: transparent; }";

// ── Settings keys ─────────────────────────────────────────────────────────────
static QString zoomKey(CosmicAssetGallery::AssetType t)
{
    return t == CosmicAssetGallery::AssetType::Texture
           ? QStringLiteral("CosmicAssetGallery/TextureZoom")
           : QStringLiteral("CosmicAssetGallery/TipZoom");
}

static const char* kSettingsOrg = "Pencil";
static const char* kSettingsApp = "Pencil";

// ── Background thumbnail loader ───────────────────────────────────────────────

class ThumbLoader : public QRunnable
{
public:
    ThumbLoader(const QString& id, const QByteArray& bytes,
                int targetPx, CosmicAssetModel* model)
        : mId(id), mBytes(bytes), mPx(targetPx), mModel(model)
    { setAutoDelete(true); }

    void run() override
    {
        QImage img;
        if (!img.loadFromData(mBytes)) return;

        // Scale to fill the square, then centre-crop.
        img = img.scaled(mPx, mPx, Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);
        if (img.width() > mPx || img.height() > mPx) {
            img = img.copy((img.width()  - mPx) / 2,
                           (img.height() - mPx) / 2, mPx, mPx);
        }

        CosmicAssetModel::thumbCache().insert(mId, new QPixmap(QPixmap::fromImage(img)), 1);
        QMetaObject::invokeMethod(mModel, "thumbnailReady",
                                  Qt::QueuedConnection, Q_ARG(QString, mId));
    }

private:
    QString           mId;
    QByteArray        mBytes;
    int               mPx;
    CosmicAssetModel* mModel;
};

// ── CosmicAssetModel ──────────────────────────────────────────────────────────

CosmicAssetModel::CosmicAssetModel(QObject* parent)
    : QAbstractListModel(parent) {}

QCache<QString, QPixmap>& CosmicAssetModel::thumbCache()
{
    // 128 entries × ~37 KB each (96×96 ARGB32) ≈ 4.7 MB max.
    static QCache<QString, QPixmap> sCache(128);
    return sCache;
}

void CosmicAssetModel::setEntries(const QVector<BrushAssetStore::AssetEntry>& entries)
{
    beginResetModel();
    mAll = entries;
    rebuildFiltered();
    endResetModel();
}

void CosmicAssetModel::setFilter(const QString& text)
{
    if (mFilter == text) return;
    mFilter = text;
    beginResetModel();
    rebuildFiltered();
    endResetModel();
}

void CosmicAssetModel::rebuildFiltered()
{
    if (mFilter.trimmed().isEmpty()) { mFiltered = mAll; return; }
    mFiltered.clear();
    for (const auto& e : mAll)
        if (e.displayName.contains(mFilter, Qt::CaseInsensitive))
            mFiltered.push_back(e);
}

int CosmicAssetModel::rowCount(const QModelIndex& parent) const
{ return parent.isValid() ? 0 : mFiltered.size(); }

QVariant CosmicAssetModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= mFiltered.size()) return {};
    const auto& e = mFiltered.at(index.row());
    switch (role) {
    case Qt::DisplayRole:  return e.displayName;
    case Qt::UserRole:     return e.id;
    case Qt::UserRole + 1: return e.builtIn;
    default:               return {};
    }
}

QString CosmicAssetModel::idAt(int row) const
{
    if (row < 0 || row >= mFiltered.size()) return {};
    return mFiltered.at(row).id;
}

bool CosmicAssetModel::isBuiltInAt(int row) const
{
    if (row < 0 || row >= mFiltered.size()) return false;
    return mFiltered.at(row).builtIn;
}

// ── CosmicAssetDelegate ───────────────────────────────────────────────────────

CosmicAssetDelegate::CosmicAssetDelegate(CosmicAssetModel* model, QObject* parent)
    : QStyledItemDelegate(parent), mModel(model)
{
    connect(model, &CosmicAssetModel::thumbnailReady, this,
            [this](const QString& id) {
                mInFlight.remove(id);
                emit sizeHintChanged(QModelIndex());
            });
}

void CosmicAssetDelegate::setThumbSize(int px)
{
    mThumbPx = qBound(48, px, 192);
    // Invalidate cache entries that were decoded at a different size.
    // Simplest: just clear them — they'll be re-decoded at the new size.
    CosmicAssetModel::thumbCache().clear();
    mInFlight.clear();
    emit sizeHintChanged(QModelIndex());
}

void CosmicAssetDelegate::requestThumbnail(const QString& id, bool builtIn) const
{
    if (mInFlight.contains(id)) return;
    QByteArray bytes;
    if (builtIn)
        bytes = BrushAssetStore::instance()->loadTexture(id);
    else {
        bytes = BrushAssetStore::instance()->loadTexture(id);
        if (bytes.isEmpty())
            bytes = BrushAssetStore::instance()->loadTip(id);
    }
    if (bytes.isEmpty()) return;
    mInFlight.insert(id);
    QThreadPool::globalInstance()->start(
        new ThumbLoader(id, bytes, mThumbPx, mModel));
}

void CosmicAssetDelegate::paint(QPainter* p,
                                const QStyleOptionViewItem& opt,
                                const QModelIndex& index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect cell = opt.rect;
    const int   r    = CosmicUiMetrics::cornerRadius();
    const bool  sel  = opt.state & QStyle::State_Selected;
    const bool  hov  = opt.state & QStyle::State_MouseOver;

    // Cell background.
    QColor bg(0x2a, 0x2a, 0x2a);
    if (sel)      bg = QColor(0x4a, 0x4a, 0x4a);
    else if (hov) bg = QColor(0x38, 0x38, 0x38);
    QPainterPath bgPath;
    bgPath.addRoundedRect(cell.adjusted(2, 2, -2, -2), r, r);
    p->fillPath(bgPath, bg);

    // Thumb rect — centred horizontally, padded top.
    const int px = mThumbPx;
    const QRect thumb(cell.left() + (cell.width() - px) / 2,
                      cell.top() + kPad,
                      px, px);

    const QString id      = index.data(Qt::UserRole).toString();
    const bool    builtIn = index.data(Qt::UserRole + 1).toBool();

    if (QPixmap* pm = CosmicAssetModel::thumbCache().object(id)) {
        QPainterPath clip;
        clip.addRoundedRect(thumb, r / 2, r / 2);
        p->setClipPath(clip);
        p->drawPixmap(thumb, *pm);
        p->setClipping(false);
    } else {
        p->fillRect(thumb, QColor(0x1a, 0x1a, 0x1a));
        p->setPen(QColor(0x55, 0x55, 0x55));
        p->drawText(thumb, Qt::AlignCenter, QStringLiteral("…"));
        requestThumbnail(id, builtIn);
    }

    // Built-in badge.
    if (builtIn) {
        const QRect badge(thumb.right() - 14, thumb.top() + 2, 12, 12);
        p->fillRect(badge, QColor(0x55, 0x55, 0x55, 180));
        p->setPen(QColor(0xcc, 0xcc, 0xcc));
        QFont bf = p->font(); bf.setPixelSize(8); p->setFont(bf);
        p->drawText(badge, Qt::AlignCenter, QStringLiteral("B"));
    }

    // Label.
    const QRect lbl(cell.left() + 2, thumb.bottom() + 3,
                    cell.width() - 4, kLabelH);
    p->setPen(sel ? Qt::white : QColor(0xb0, 0xb0, 0xb0));
    QFont lf = p->font(); lf.setPixelSize(11); p->setFont(lf);
    p->drawText(lbl, Qt::AlignHCenter | Qt::AlignVCenter,
                opt.fontMetrics.elidedText(
                    index.data(Qt::DisplayRole).toString(),
                    Qt::ElideRight, lbl.width()));

    p->restore();
}

QSize CosmicAssetDelegate::sizeHint(const QStyleOptionViewItem&,
                                    const QModelIndex&) const
{
    return QSize(mThumbPx + kPad * 2, mThumbPx + kLabelH + kPad * 2 + 4);
}

// ── CosmicAssetGallery ────────────────────────────────────────────────────────

CosmicAssetGallery::CosmicAssetGallery(AssetType type, QWidget* parent)
    : CosmicPopupWindow(parent), mType(type)
{
    setResizable(true);
    setDraggable(true);
    setCenterOnShow(true);
    setPersistentSizeKey(type == AssetType::Texture
                         ? QStringLiteral("CosmicAssetGallery/TextureSize")
                         : QStringLiteral("CosmicAssetGallery/TipSize"));
    buildUi();
}

void CosmicAssetGallery::buildUi()
{
    auto* root = new QWidget(this);
    auto* lay  = new QVBoxLayout(root);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(6);

    // ── Window header (close button) ─────────────────────────────────────────
    auto* header = new CosmicWindowHeader(root);
    connect(header, &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    lay->addWidget(header);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel(mType == AssetType::Texture
                             ? tr("Texture Gallery") : tr("Tip Gallery"), root);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "QLabel{color:#fff;font-weight:600;font-size:13px;background:transparent;}"));
    lay->addWidget(title);

    // ── Search bar ────────────────────────────────────────────────────────────
    mSearch = new QLineEdit(root);
    mSearch->setPlaceholderText(tr("Search…"));
    mSearch->setClearButtonEnabled(true);
    mSearch->setStyleSheet(QStringLiteral(
        "QLineEdit{background:#1e1e1e;color:#c0c0c0;border:1px solid #3a3a3a;"
        "border-radius:6px;padding:4px 8px;font-size:12px;}"));
    lay->addWidget(mSearch);

    // ── Toolbar row (zoom slider + delete) ───────────────────────────────────
    auto* toolbar = new QWidget(root);
    toolbar->setStyleSheet(QStringLiteral("background:transparent;"));
    auto* tbLay = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(0, 0, 0, 0);
    tbLay->setSpacing(6);

    // Zoom slider — persistent.
    const int savedZoom = loadZoom();
    mZoomSlider = new CosmicHSlider(toolbar);
    mZoomSlider->setRange(48, 192);
    mZoomSlider->setValue(savedZoom);
    mZoomSlider->setLabelMode(CosmicHSlider::Textless);
    mZoomSlider->setToolTip(tr("Thumbnail size"));
    tbLay->addWidget(mZoomSlider, 1);

    // Delete button — only enabled for user assets (not built-ins).
    auto* deleteBtn = new CosmicButton(tr("Delete"), toolbar);
    deleteBtn->setBordered(true);
    deleteBtn->setToolTip(tr("Remove selected asset"));
    deleteBtn->setFixedHeight(24);
    tbLay->addWidget(deleteBtn);

    lay->addWidget(toolbar);

    // ── Grid inside a scroll area ─────────────────────────────────────────────
    mModel    = new CosmicAssetModel(this);
    mDelegate = new CosmicAssetDelegate(mModel, this);
    mDelegate->setThumbSize(savedZoom);

    mView = new QListView(root);
    mView->setModel(mModel);
    mView->setItemDelegate(mDelegate);
    mView->setViewMode(QListView::IconMode);
    mView->setResizeMode(QListView::Adjust);
    mView->setMovement(QListView::Static);
    mView->setSelectionMode(QAbstractItemView::SingleSelection);
    mView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mView->setUniformItemSizes(true);
    mView->setSpacing(4);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mView->setStyleSheet(QStringLiteral(
        "QListView{background:#232323;border:none;border-radius:8px;}"
        "QListView::item{border:none;}"
    ) + QString::fromLatin1(kScrollBarStyle));
    mView->setFocusPolicy(Qt::StrongFocus);
    mView->setMinimumHeight(200);
    lay->addWidget(mView, 1);

    // Empty-state label.
    mEmpty = new QLabel(mType == AssetType::Texture
        ? tr("No textures imported yet.\nUse the + button to import one.")
        : tr("No custom tips imported yet.\nUse the + button to import one."), root);
    mEmpty->setAlignment(Qt::AlignCenter);
    mEmpty->setStyleSheet(QStringLiteral(
        "QLabel{color:#555;font-size:12px;background:transparent;}"));
    mEmpty->hide();
    lay->addWidget(mEmpty);

    // ── Wire ─────────────────────────────────────────────────────────────────
    connect(mSearch, &QLineEdit::textChanged, mModel, &CosmicAssetModel::setFilter);
    connect(mSearch, &QLineEdit::textChanged, this, [this]() {
        const bool empty = mModel->rowCount() == 0;
        mView->setVisible(!empty);
        mEmpty->setVisible(empty);
    });

    connect(mView, &QListView::activated, this, [this](const QModelIndex&) {
        commitSelection();
    });

    connect(mModel, &CosmicAssetModel::thumbnailReady, mView,
            static_cast<void(QWidget::*)()>(&QWidget::update));

    connect(mZoomSlider, &CosmicHSlider::valueChanged, this, [this](int v) {
        applyThumbSize(v);
        saveZoom(v);
    });

    connect(deleteBtn, &CosmicButton::clicked, this, [this]() {
        deleteSelected();
    });

    // Enable delete only when a non-built-in item is selected.
    auto updateDeleteEnabled = [this, deleteBtn]() {
        const QModelIndex idx = mView->currentIndex();
        const bool canDelete  = idx.isValid()
                             && !mModel->isBuiltInAt(idx.row());
        deleteBtn->setEnabled(canDelete);
    };
    connect(mView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, updateDeleteEnabled);
    deleteBtn->setEnabled(false);

    auto* popupLay = new QVBoxLayout(this);
    popupLay->setContentsMargins(0, 0, 0, 0);
    popupLay->addWidget(root);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

int CosmicAssetGallery::loadZoom() const
{
    QSettings s(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    return qBound(48, s.value(zoomKey(mType), 96).toInt(), 192);
}

void CosmicAssetGallery::saveZoom(int px)
{
    QSettings s(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    s.setValue(zoomKey(mType), px);
}

void CosmicAssetGallery::applyThumbSize(int px)
{
    mDelegate->setThumbSize(px);
    mView->setIconSize(QSize(px, px));
    mView->reset();
}

void CosmicAssetGallery::refresh()
{
    auto* store = BrushAssetStore::instance();
    const auto entries = (mType == AssetType::Texture)
                         ? store->listTextures() : store->listTips();
    mModel->setEntries(entries);

    const bool empty = entries.isEmpty();
    mView->setVisible(!empty);
    mEmpty->setVisible(empty);
}

void CosmicAssetGallery::commitSelection()
{
    const QModelIndex idx = mView->currentIndex();
    if (!idx.isValid()) return;
    const QString id = mModel->idAt(idx.row());
    if (id.isEmpty()) return;
    emit assetSelected(id);
    QDialog::accept();
}

void CosmicAssetGallery::deleteSelected()
{
    const QModelIndex idx = mView->currentIndex();
    if (!idx.isValid()) return;
    if (mModel->isBuiltInAt(idx.row())) return;

    const QString id = mModel->idAt(idx.row());
    if (id.isEmpty()) return;

    // Double-check: never delete built-in assets regardless of model state.
    if (BrushAssetStore::isBuiltinId(id)) return;

    const QString name = idx.data(Qt::DisplayRole).toString();

    CosmicConfirmPopup confirm(this);
    confirm.setTitleText(tr("Delete Asset"));
    confirm.setBodyText(tr("Delete \"%1\"?\n\nAny preset using it will revert to the default.").arg(name));
    confirm.setConfirmText(tr("Delete"));
    confirm.setCancelText(tr("Cancel"));
    if (confirm.exec() != QDialog::Accepted) return;

    // Safe-delete: clear the reference from every preset that uses this asset.
    auto* store = BrushPresetStore::instance();
    const auto presets = store->presets();
    for (const auto& p : presets) {
        if (mType == AssetType::Texture && p.textureAssetId == id) {
            ToolOptionWidget::BrushPreset updated = p;
            updated.textureAssetId.clear();
            store->updatePreset(p.id, updated);
        } else if (mType == AssetType::Tip && p.tipAssetId == id) {
            ToolOptionWidget::BrushPreset updated = p;
            updated.tipAssetId.clear();
            updated.tipMode = ToolOptionWidget::BrushPreset::TipMode::Hard;
            store->updatePreset(p.id, updated);
        }
    }

    if (mType == AssetType::Texture)
        BrushAssetStore::instance()->removeTexture(id);
    else
        BrushAssetStore::instance()->removeTip(id);

    CosmicAssetModel::thumbCache().remove(id);

    // Remember the row so we can restore selection after refresh.
    const int deletedRow = idx.row();

    refresh();

    // Select the next available item: same row if it still exists, else the one before it.
    const int newCount = mModel->rowCount();
    if (newCount > 0) {
        const int selectRow = qMin(deletedRow, newCount - 1);
        const QModelIndex newIdx = mModel->index(selectRow, 0);
        mView->setCurrentIndex(newIdx);
        mView->scrollTo(newIdx);
    }

    // Return focus to the content area so keyboard navigation works immediately.
    mView->setFocus(Qt::OtherFocusReason);
}

// ── Event overrides ───────────────────────────────────────────────────────────

void CosmicAssetGallery::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);
    refresh();
    QTimer::singleShot(0, this, [this]() {
        mView->setFocus(Qt::OtherFocusReason);
    });
}

void CosmicAssetGallery::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        e->accept();
        reject();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        const QWidget* fw = QApplication::focusWidget();
        // If focus is in the search bar let it handle the key normally.
        if (qobject_cast<const QLineEdit*>(fw)) {
            CosmicPopupWindow::keyPressEvent(e);
            return;
        }
        e->accept();
        commitSelection();
        return;
    }
    CosmicPopupWindow::keyPressEvent(e);
}
