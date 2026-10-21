#ifndef COSMICASSETGALLERY_H
#define COSMICASSETGALLERY_H

#include "cosmicpopupwindow.h"
#include "../app/src/managers/brushassetstore.h"

#include <QAbstractListModel>
#include <QCache>
#include <QPixmap>
#include <QRunnable>
#include <QSet>
#include <QStyledItemDelegate>
#include <QVector>

class QListView;
class QLineEdit;
class QLabel;
class QScrollArea;
class CosmicHSlider;
class CosmicWindowHeader;

// ── Model ─────────────────────────────────────────────────────────────────────
// Holds only {id, displayName} — no image bytes in memory.

class CosmicAssetModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit CosmicAssetModel(QObject* parent = nullptr);

    void setEntries(const QVector<BrushAssetStore::AssetEntry>& entries);
    void setFilter(const QString& text);

    int     rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QString idAt(int row) const;
    bool    isBuiltInAt(int row) const;

    // Shared LRU thumbnail cache — keyed by asset ID, capped at 128 entries.
    static QCache<QString, QPixmap>& thumbCache();

signals:
    void thumbnailReady(const QString& id);

private:
    QVector<BrushAssetStore::AssetEntry> mAll;
    QVector<BrushAssetStore::AssetEntry> mFiltered;
    QString mFilter;
    void rebuildFiltered();
};

// ── Delegate ──────────────────────────────────────────────────────────────────

class CosmicAssetDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    static constexpr int kThumbSize = 96;
    static constexpr int kLabelH   = 18;
    static constexpr int kPad      = 8;
    static constexpr int kCellW    = kThumbSize + kPad * 2;
    static constexpr int kCellH    = kThumbSize + kLabelH + kPad * 2 + 4;

    explicit CosmicAssetDelegate(CosmicAssetModel* model, QObject* parent = nullptr);

    void  paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;

    void setThumbSize(int px);
    int  thumbSize() const { return mThumbPx; }

private:
    void requestThumbnail(const QString& id, bool builtIn) const;

    CosmicAssetModel*  mModel    = nullptr;
    int                mThumbPx  = kThumbSize;
    mutable QSet<QString> mInFlight;
};

// ── Gallery popup ─────────────────────────────────────────────────────────────

class CosmicAssetGallery : public CosmicPopupWindow
{
    Q_OBJECT
public:
    enum class AssetType { Texture, Tip };

    explicit CosmicAssetGallery(AssetType type, QWidget* parent = nullptr);

    void refresh();

signals:
    void assetSelected(const QString& assetId);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void buildUi();
    void commitSelection();
    void deleteSelected();
    void applyThumbSize(int px);
    void saveZoom(int px);
    int  loadZoom() const;

    AssetType            mType;
    CosmicAssetModel*    mModel     = nullptr;
    CosmicAssetDelegate* mDelegate  = nullptr;
    QListView*           mView      = nullptr;
    QScrollArea*         mScrollArea = nullptr;
    QLineEdit*           mSearch    = nullptr;
    QLabel*              mEmpty     = nullptr;
    CosmicHSlider*       mZoomSlider = nullptr;
};

#endif // COSMICASSETGALLERY_H
