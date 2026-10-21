#include "cosmicfilebrowserpopup.h"

#include "pencildef.h"

#include "cosmicrenameoperator.h"
#include "cosmicwindowheader.h"

#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include "cosmicpopupcontextmenu.h"

#include <QAbstractProxyModel>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QFileIconProvider>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMessageBox>
#include <QModelIndex>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QImageReader>
#include <QCache>
#include <QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QStandardPaths>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <memory>
#include <QSet>

namespace {
static QColor cosmicPanel() { return QColor(0x2f, 0x2f, 0x2f); }
static QColor cosmicBorder() { return CosmicColor::popupBorder(); }
static QColor cosmicText() { return QColor(0xb3, 0xb3, 0xb3); }
static QColor cosmicHover() { return QColor(0x4d, 0x4d, 0x4d); }
static QColor cosmicLockedAccent() { return CosmicColor::accent(); }
static QString lastDirKey() { return QStringLiteral("CosmicFileBrowser/LastDir"); }
static QString uiSettingsGroupKey() { return QStringLiteral("CosmicFileBrowser/UI"); }
static QString bookmarksSettingsGroupKey() { return QStringLiteral("CosmicFileBrowser/Bookmarks"); }

static QString showHiddenKey() { return QStringLiteral("ShowHidden"); }

static QString locationsExpandedKey() { return QStringLiteral("LocationsExpanded"); }
static QString bookmarksExpandedKey() { return QStringLiteral("BookmarksExpanded"); }
static QString devicesExpandedKey() { return QStringLiteral("DevicesExpanded"); }

static QString headerStateKey() { return QStringLiteral("HeaderState"); }

static QString kCutMimeType() { return QStringLiteral("application/x-nurture2d-cut"); }

static bool isTypeSelectChar(const QString& t)
{
    if (t.isEmpty()) {
        return false;
    }
    if (t.size() != 1) {
        return false;
    }
    const QChar c = t.at(0);
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char(' ');
}

class CosmicRoundedItemDelegate final : public QStyledItemDelegate
{
public:
    explicit CosmicRoundedItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!painter) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.state &= ~QStyle::State_HasFocus;

        // "Add Bookmark" button row — draw as a bordered pill button centered in the row
        const bool isAddBookmark = index.data(Qt::UserRole + 20).toBool();
        if (isAddBookmark) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            const int rad = qMax(3, CosmicUiMetrics::cornerRadius());
            const QRect r = opt.rect.adjusted(6, 4, -6, -4);
            const bool dragActive = index.data(Qt::UserRole + 21).toBool();
            const bool hovered = !dragActive && (opt.state.testFlag(QStyle::State_MouseOver)
                                              || opt.state.testFlag(QStyle::State_Selected));

            if (dragActive) {
                // Dashed green border, no fill
                QPen dashPen(CosmicColor::green(), 1.5, Qt::DashLine);
                painter->setPen(dashPen);
                painter->setBrush(QColor(0x62, 0xcc, 0x29, 30)); // faint green tint
                painter->drawRoundedRect(r, rad, rad);
                painter->setPen(CosmicColor::green());
                painter->drawText(r, Qt::AlignCenter, tr("Drag and Drop"));
            } else {
                painter->setPen(QPen(QColor(0x55, 0x55, 0x55), 1));
                painter->setBrush(hovered ? QColor(0x44, 0x44, 0x44) : QColor(0x33, 0x33, 0x33));
                painter->drawRoundedRect(r, rad, rad);
                painter->setPen(QColor(0xb3, 0xb3, 0xb3));
                painter->drawText(r, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
            }

            painter->restore();
            return;
        }

        // Section header items — draw bold white text left, arrow icon right, no selection bg
        const bool isSectionHeader = index.data(Qt::UserRole + 10).toBool();
        if (isSectionHeader) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Bold white text
            QFont f = opt.font;
            f.setBold(true);
            painter->setFont(f);
            painter->setPen(QColor(0xff, 0xff, 0xff));
            const QRect textR = opt.rect.adjusted(8, 0, -24, 0);
            painter->drawText(textR, Qt::AlignVCenter | Qt::AlignLeft,
                              index.data(Qt::DisplayRole).toString());

            // Arrow icon right-aligned
            const bool expanded = index.data(Qt::UserRole + 11).toBool();
            const QIcon arrowIcon = expanded
                ? QIcon(QStringLiteral(":/Interaction/ArrowUp.svg"))
                : QIcon(QStringLiteral(":/Interaction/ArrowDown.svg"));
            const QPixmap arrow = arrowIcon.pixmap(11, 11);
            const int ax = opt.rect.right() - 8 - arrow.width();
            const int ay = opt.rect.top() + (opt.rect.height() - arrow.height()) / 2;
            painter->drawPixmap(ax, ay, arrow);

            painter->restore();
            return;
        }

        // Section header items (NoItemFlags) — draw nothing (fallback for old-style headers)
        if (!(index.flags() & Qt::ItemIsEnabled)) {
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_Selected;
            opt.state &= ~QStyle::State_MouseOver;
            QStyledItemDelegate::paint(painter, opt, index);
            return;
        }

        const int rad = qMax(3, CosmicUiMetrics::cornerRadius());

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        bool hasExplicitBg = false;
        QBrush explicitBg;
        const QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.canConvert<QBrush>()) {
            explicitBg = bgVar.value<QBrush>();
            hasExplicitBg = (explicitBg.style() != Qt::NoBrush);
        }

        if (hasExplicitBg) {
            opt.state &= ~QStyle::State_Selected;
            opt.state &= ~QStyle::State_MouseOver;
            opt.state &= ~QStyle::State_HasFocus;
        }

        // Paint selection/hover as a single full-row strip, drawn only on column 0
        // to avoid per-cell highlights. We expand the rect to cover the full row width.
        if (!hasExplicitBg && index.column() == 0) {
            const bool selected = opt.state.testFlag(QStyle::State_Selected);
            const bool hovered  = opt.state.testFlag(QStyle::State_MouseOver);

            // Detect grid mode: QListView with IconMode
            const auto* listView = qobject_cast<const QListView*>(opt.widget);
            const bool isGrid = listView && listView->viewMode() == QListView::IconMode;

            if (isGrid) {
                // Grid: squircle highlight per-item, no full-row strip
                if (selected || hovered) {
                    const QRect itemRect = opt.rect.adjusted(2, 2, -2, -2);
                    const QColor fill = selected
                        ? opt.palette.color(QPalette::Highlight)
                        : cosmicHover();
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(fill);
                    painter->drawRoundedRect(itemRect, rad, rad);
                }
            } else {
                // List: single unified full-row strip
                if (selected || hovered) {
                    int rowWidth = opt.widget ? opt.widget->width() : opt.rect.width();
                    const QRect rowRect(opt.rect.left(), opt.rect.top() + 1, rowWidth - 4, opt.rect.height() - 2);
                    const QColor fill = selected ? opt.palette.color(QPalette::Highlight) : cosmicHover();
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(fill);
                    painter->drawRoundedRect(rowRect, rad, rad);
                }
            }
        } else if (hasExplicitBg) {
            const QRect r = opt.rect.adjusted(2, 1, -2, -1);
            painter->setPen(Qt::NoPen);
            painter->setBrush(explicitBg);
            painter->drawRoundedRect(r, rad, rad);
        }

        // Suppress the default square selection/hover so it doesn't overdraw our strip
        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_MouseOver;

        painter->restore();

        opt.backgroundBrush = Qt::NoBrush;
        QStyledItemDelegate::paint(painter, opt, index);
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
        const QFontMetrics fm(option.font);
        const int editH = fm.height() + 4;
        const int editY = option.rect.top() + (option.rect.height() - editH) / 2;
        // Indent to match the item's text start (skip icon if present)
        const int iconW = option.decorationSize.isValid() ? option.decorationSize.width() + 4 : 0;
        editor->setGeometry(QRect(option.rect.left() + iconW + 4,
                                  editY,
                                  option.rect.width() - iconW - 8,
                                  editH));
    }
};

class CosmicFileSystemModel final : public QFileSystemModel
{
public:
    using QFileSystemModel::QFileSystemModel;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        // QFileSystemModel caches icons internally. For our async thumbnail pipeline we must
        // re-query the icon provider so the view can swap placeholders for real thumbnails.
        if (role == Qt::DecorationRole && index.isValid() && index.column() == 0) {
            if (auto* prov = iconProvider()) {
                return prov->icon(fileInfo(index));
            }
        }
        return QFileSystemModel::data(index, role);
    }
};

static bool isSubPathOf(const QString& baseDir, const QString& path)
{
    const QString base = QDir::cleanPath(QFileInfo(baseDir).absoluteFilePath());
    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (base.isEmpty() || p.isEmpty()) {
        return false;
    }
    if (base == p) {
        return true;
    }
    const QString prefix = base.endsWith(QDir::separator()) ? base : (base + QDir::separator());
    return p.startsWith(prefix);
}

static bool copyFileWithOverwrite(const QString& srcFile, const QString& dstFile, QString* err)
{
    if (QFileInfo(dstFile).exists()) {
        if (!QFile::remove(dstFile)) {
            if (err) *err = QStringLiteral("Failed to overwrite '%1'.").arg(dstFile);
            return false;
        }
    }

    if (!QFile::copy(srcFile, dstFile)) {
        if (err) *err = QStringLiteral("Failed to copy '%1' to '%2'.").arg(srcFile, dstFile);
        return false;
    }
    return true;
}

static bool copyDirRecursive(const QString& srcDir, const QString& dstDir, QString* err)
{
    const QDir src(srcDir);
    if (!src.exists()) {
        if (err) *err = QStringLiteral("Source directory does not exist: '%1'.").arg(srcDir);
        return false;
    }

    QDir dst(dstDir);
    if (!dst.exists()) {
        if (!QDir().mkpath(dstDir)) {
            if (err) *err = QStringLiteral("Failed to create directory: '%1'.").arg(dstDir);
            return false;
        }
    }

    QDirIterator it(srcDir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        const QString rel = src.relativeFilePath(fi.absoluteFilePath());
        const QString dstPath = QDir(dstDir).filePath(rel);

        if (fi.isDir()) {
            if (!QDir().mkpath(dstPath)) {
                if (err) *err = QStringLiteral("Failed to create directory: '%1'.").arg(dstPath);
                return false;
            }
        } else if (fi.isFile() || fi.isSymLink()) {
            if (!QDir().mkpath(QFileInfo(dstPath).absolutePath())) {
                if (err) *err = QStringLiteral("Failed to create directory: '%1'.").arg(QFileInfo(dstPath).absolutePath());
                return false;
            }
            if (!copyFileWithOverwrite(fi.absoluteFilePath(), dstPath, err)) {
                return false;
            }
        }
    }

    return true;
}

static QStringList globPatternsFromFilterString(const QString& filter)
{
    QString f = filter;
    const int lp = f.indexOf('(');
    const int rp = f.lastIndexOf(')');
    if (lp >= 0 && rp > lp) {
        f = f.mid(lp + 1, rp - lp - 1);
    }

    QStringList out;
    const QStringList parts = f.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        if (p.contains('*') || p.contains('?')) {
            out.push_back(p);
        }
    }
    return out;
}

static void setAddBookmarkDragHover(CosmicButton* b, bool on)
{
    if (!b) {
        return;
    }
    if (on) {
        b->setStyleSheet(QStringLiteral(
            "QPushButton { border: 2px solid #40555c; background: #2b2b2b; }"
        ));
    } else {
        b->setStyleSheet(QString());
    }
}

static QString defaultBookmarkNameForPath(const QString& path)
{
    QFileInfo fi(path);
    const QString name = fi.fileName();
    if (!name.trimmed().isEmpty()) {
        return name;
    }
    return QDir::cleanPath(path);
}

class CosmicFileIconProvider final : public QFileIconProvider
{
public:
    explicit CosmicFileIconProvider(QFileSystemModel* model, int thumbPx)
        : mFolderIcon(QStringLiteral(":/FileBrowser/Folder.svg"))
        , mImageIcon(QStringLiteral(":/FileBrowser/Image.svg"))
        , mAudioIcon(QStringLiteral(":/FileBrowser/Audio.svg"))
        , mProjectIcon(QStringLiteral(":/icons/logo.png"))
        , mModel(model)
        , mThumbPx(qBound(32, thumbPx, 256))
        , mAlive(std::make_shared<bool>(true))
    {
        mThumbCache.setMaxCost(256);
    }

    ~CosmicFileIconProvider()
    {
        // Signal any pending QTimer::singleShot callbacks that this object is gone.
        *mAlive = false;
    }

    void setThumbnailPx(int px)
    {
        mThumbPx = qBound(32, px, 256);
    }

    void setThumbnailsEnabled(bool on)
    {
        mThumbnailsEnabled = on;
    }

    QIcon icon(const QFileInfo& info) const override
    {
        if (info.isDir()) {
            return mFolderIcon;
        }

        const QString suffix = info.suffix().toLower();
        if (suffix == QStringLiteral("pclx")) {
            return mProjectIcon;
        }

        static const QSet<QString> imageExt = {
            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"),
            QStringLiteral("gif"), QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("webp"),
            QStringLiteral("svg")
        };

        static const QSet<QString> audioExt = {
            QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("flac"),
            QStringLiteral("aac"), QStringLiteral("m4a")
        };

        if (imageExt.contains(suffix)) {
            if (!mThumbnailsEnabled) {
                return mImageIcon;
            }

            const QString path = info.absoluteFilePath();
            if (!path.isEmpty()) {
                const qint64 stamp = info.lastModified().toMSecsSinceEpoch();
                const QString key = QStringLiteral("%1|%2|%3").arg(path).arg(stamp).arg(mThumbPx);
                if (auto* cached = mThumbCache.object(key)) {
                    return QIcon(*cached);
                }

                // Non-blocking: return placeholder icon immediately, and queue an async thumbnail decode.
                if (mModel) {
                    const QModelIndex srcIdx = mModel->index(path);
                    const QPersistentModelIndex idx(srcIdx);
                    // Queue the job even if the index isn't valid yet; it will be cached and
                    // picked up on the next paint/prefetch after the directory is populated.
                    queueThumbnailJob(path, stamp, mThumbPx, idx);
                }
            }
            return mImageIcon;
        }
        if (audioExt.contains(suffix)) {
            return mAudioIcon;
        }

        return QFileIconProvider::icon(info);
    }

private:
    void queueThumbnailJob(const QString& path, qint64 stamp, int px, const QPersistentModelIndex& targetIdx) const
    {
        const QString jobKey = QStringLiteral("%1|%2|%3").arg(path).arg(stamp).arg(px);
        {
            QMutexLocker lock(&mMutex);
            if (mJobsInFlight.contains(jobKey)) {
                return;
            }
            mJobsInFlight.insert(jobKey);
        }

        // Decode/scale off the UI thread.
        // Capture mAlive so the UI-thread callback can bail out if the icon provider
        // was destroyed while the background job was in flight.
        std::shared_ptr<bool> alive = mAlive;
        QtConcurrent::run([this, alive, path, stamp, px, jobKey, targetIdx]() {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            reader.setScaledSize(QSize(px, px));
            const QImage img = reader.read();
            if (img.isNull()) {
                if (*alive) {
                    QMutexLocker lock(&mMutex);
                    mJobsInFlight.remove(jobKey);
                }
                return;
            }

            // IMPORTANT: QPixmap is GUI-thread-only. Keep the worker strictly on QImage.
            // Cache update + repaint are scheduled onto the model's thread (UI).
            QPointer<QFileSystemModel> model = mModel;
            QTimer::singleShot(0, model, [this, alive, model, path, jobKey, img, targetIdx]() {
                // Guard: if the icon provider was destroyed before this callback fired, bail out.
                if (!*alive) {
                    return;
                }
                {
                    QMutexLocker lock(&mMutex);
                    if (!model) {
                        mJobsInFlight.remove(jobKey);
                        return;
                    }

                    // Cache write is protected because icon() reads from it.
                    mThumbCache.insert(jobKey, new QPixmap(QPixmap::fromImage(img)), 1);
                    mJobsInFlight.remove(jobKey);
                }

                if (targetIdx.isValid()) {
                    emit model->dataChanged(targetIdx, targetIdx, QVector<int>() << Qt::DecorationRole);
                    return;
                }

                // If the index wasn't valid at queue time (directory still loading), try again now.
                const QModelIndex idx = model->index(path);
                if (idx.isValid()) {
                    emit model->dataChanged(idx, idx, QVector<int>() << Qt::DecorationRole);
                }
            });
        });
    }

    QIcon mFolderIcon;
    QIcon mImageIcon;
    QIcon mAudioIcon;
    QIcon mProjectIcon;

    QPointer<QFileSystemModel> mModel;

    bool mThumbnailsEnabled = true;

    int mThumbPx = 64;
    mutable QCache<QString, QPixmap> mThumbCache;
    mutable QMutex mMutex;
    mutable QSet<QString> mJobsInFlight;

    // Shared alive-flag: set to false in destructor so pending QTimer callbacks
    // can detect that this object is gone and skip accessing its members.
    std::shared_ptr<bool> mAlive;
};

class FileNameFilterProxy : public QSortFilterProxyModel
{
public:
    explicit FileNameFilterProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setRecursiveFilteringEnabled(true);
    }

    void setNameSubstringFilter(const QString& s)
    {
        mSubstr = s;
        invalidateFilter();
    }

    void setAllowedNameGlobs(const QStringList& globs)
    {
        mAllowedGlobs = globs;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
    {
        const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
        if (!idx.isValid()) {
            return false;
        }

        const QString name = sourceModel()->data(idx, QFileSystemModel::FileNameRole).toString();
        const bool isDir = sourceModel()->hasChildren(idx);

        // When the user is actively searching, filter both files and folders
        // by the substring. This makes search general regardless of how the
        // browser was opened and regardless of whether the item is a folder.
        if (!mSubstr.trimmed().isEmpty()) {
            return name.contains(mSubstr, Qt::CaseInsensitive);
        }

        // No active search — directories always pass through.
        if (isDir) {
            return true;
        }

        // No active search — apply the file-type glob filter normally.
        if (!mAllowedGlobs.isEmpty()) {
            if (!QDir::match(mAllowedGlobs, name)) {
                return false;
            }
        }

        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }

private:
    QString mSubstr;
    QStringList mAllowedGlobs;
};

}

CosmicFileBrowserPopup::CosmicFileBrowserPopup(Mode mode,
                                   const QString& caption,
                                   const QString& initialDir,
                                   const QStringList& nameFilters,
                                   QWidget* parent)
    : CosmicPopupWindow(parent)
    , mMode(mode)
    , mCaption(caption)
    , mNameFilters(nameFilters)
{
    qDebug() << "[CosmicFileBrowserPopup] ctor mode=" << static_cast<int>(mMode)
             << "caption=" << mCaption
             << "initialDir=" << initialDir;

    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(true);
    setMinimumSize(QSize(900, 560));

    setShadowEnabled(false);
    setShadowPadding(0);

    setPanelColor(cosmicPanel());
    setBorderColor(cosmicBorder());
    setBorderWidth(2);

    buildUi();

    mUiSaveDebounce = new QTimer(this);
    mUiSaveDebounce->setSingleShot(true);
    mUiSaveDebounce->setInterval(350);
    connect(mUiSaveDebounce, &QTimer::timeout, this, [this]() { saveUiSettings(); });

    mThumbPrefetchDebounce = new QTimer(this);
    mThumbPrefetchDebounce->setSingleShot(true);
    mThumbPrefetchDebounce->setInterval(60);
    connect(mThumbPrefetchDebounce, &QTimer::timeout, this, &CosmicFileBrowserPopup::prefetchVisibleThumbnailsNow);

    if (mFsModel) {
        connect(mFsModel, &QFileSystemModel::directoryLoaded, this, [this](const QString& path) {
            // Directory listing finished; now indexes are reliable and the view can be prefetched.
            if (QDir::cleanPath(path) == QDir::cleanPath(currentDir())) {
                scheduleVisibleThumbnailPrefetch();
            }
        });
    }

    if (mProxyModel) {
        connect(mProxyModel, &QAbstractItemModel::modelReset, this, [this]() { scheduleVisibleThumbnailPrefetch(); });
        connect(mProxyModel, &QAbstractItemModel::layoutChanged, this, [this]() { scheduleVisibleThumbnailPrefetch(); });
        connect(mProxyModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int, int) { scheduleVisibleThumbnailPrefetch(); });
        connect(mProxyModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) { scheduleVisibleThumbnailPrefetch(); });
    }

    mTypeSelectTimer = new QTimer(this);
    mTypeSelectTimer->setSingleShot(true);
    mTypeSelectTimer->setInterval(900);
    connect(mTypeSelectTimer, &QTimer::timeout, this, &CosmicFileBrowserPopup::onTypeSelectTimeout);

    mRenameOp = new CosmicRenameOperator(this);
    connect(mRenameOp, &CosmicRenameOperator::committed, this, [this](const QString& text) {
        const QString newName = text.trimmed();
        const QString srcPath = mPendingRenamePath;
        mPendingRenamePath.clear();
        mPendingRenameView.clear();

        if (srcPath.isEmpty()) {
            return;
        }
        QFileInfo srcFi(srcPath);
        if (!srcFi.exists()) {
            return;
        }
        const QString currentName = srcFi.fileName();
        if (newName.isEmpty() || newName == currentName) {
            return;
        }
        if (newName.contains(QDir::separator())) {
            QMessageBox::critical(this, tr("File operation failed"), tr("Invalid name."));
            return;
        }

        const QString dstPath = QDir(srcFi.absolutePath()).filePath(newName);
        if (QFileInfo(dstPath).exists()) {
            QMessageBox::critical(this, tr("File operation failed"), tr("A file or folder with that name already exists."));
            return;
        }

        if (!QDir().rename(srcPath, dstPath)) {
            QMessageBox::critical(this, tr("File operation failed"), tr("Failed to rename '%1'.").arg(srcPath));
            return;
        }

        setCurrentDir(currentDir(), false);
    });
    connect(mRenameOp, &CosmicRenameOperator::canceled, this, [this]() {
        mPendingRenamePath.clear();
        mPendingRenameView.clear();
    });

    loadUiSettings();
    loadBookmarks();
    updateSidebar();

    QString startDir = initialDir;
    if (startDir.isEmpty()) {
        QSettings settings(PENCIL2D, PENCIL2D);
        startDir = settings.value(lastDirKey(), QDir::homePath()).toString();
    }
    setCurrentDir(startDir, false);

    if (mViewMode == ViewMode::Grid) {
        onViewModeGrid();
    } else {
        onViewModeList();
    }
}

QString CosmicFileBrowserPopup::uiSettingsGroupKeyForMode() const
{
    switch (mMode)
    {
    case Mode::SaveFile:
        return uiSettingsGroupKey() + QStringLiteral("/Save");
    case Mode::OpenFiles:
    case Mode::OpenFile:
    default:
        return uiSettingsGroupKey() + QStringLiteral("/Open");
    }
}

void CosmicFileBrowserPopup::applyFsModelFilter()
{
    if (!mFsModel) {
        return;
    }

    QDir::Filters f = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs;
    if (mShowHidden) {
        f |= QDir::Hidden | QDir::System;
    }
    mFsModel->setFilter(f);
}

void CosmicFileBrowserPopup::refreshCurrentDir()
{
    setCurrentDir(currentDir(), false);
}

bool CosmicFileBrowserPopup::event(QEvent* e)
{
    // Thumbnails depend on view geometry and model population. Make sure we prefetch after the
    // widget is actually shown/resized/activated (not just constructed), without relying on mouse moves.
    if (e) {
        if (e->type() == QEvent::Show || e->type() == QEvent::Resize || e->type() == QEvent::WindowActivate) {
            QTimer::singleShot(0, this, [this]() { scheduleVisibleThumbnailPrefetch(); });
        }
        // On show, give focus to the content view so arrow keys work immediately.
        if (e->type() == QEvent::Show) {
            QTimer::singleShot(0, this, [this]() {
                QAbstractItemView* v = activeContentView();
                if (v) {
                    v->setFocus(Qt::OtherFocusReason);
                }
            });
        }
    }
    return CosmicPopupWindow::event(e);
}

CosmicFileBrowserPopup::Result CosmicFileBrowserPopup::execAndGetResult()
{
    mResult = Result();
    exec();
    return mResult;
}

void CosmicFileBrowserPopup::setDefaultSuffix(const QString& suffix)
{
    mDefaultSuffix = suffix;
}

void CosmicFileBrowserPopup::setFolderMode(bool on)
{
    mFolderMode = on;
    if (mAcceptBtn) {
        mAcceptBtn->setText(on ? tr("Select") : (mMode == Mode::SaveFile ? tr("Save") : tr("Open")));
    }
    updateAcceptEnabled();
}

void CosmicFileBrowserPopup::closeEvent(QCloseEvent* e)
{
    saveUiSettings();
    saveBookmarks();
    CosmicPopupWindow::closeEvent(e);
}

void CosmicFileBrowserPopup::done(int r)
{
    saveUiSettings();
    saveBookmarks();
    QDialog::done(r);
}

void CosmicFileBrowserPopup::buildUi()
{
    QWidget* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicFileBrowserRoot"));
    root->setStyleSheet(QStringLiteral("QWidget#CosmicFileBrowserRoot { background: transparent; }") );

    auto* outer = new QVBoxLayout(root);
    outer->setContentsMargins(16, 10, 16, 16);
    outer->setSpacing(10);

    mWindowHeader = new CosmicWindowHeader(root);
    connect(mWindowHeader, &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    outer->addWidget(mWindowHeader);

    // ── Hidden legacy header (kept so existing code paths compile) ──────────
    mHeader = new QWidget(root);
    mHeader->hide();
    mPathEdit = new QLineEdit(mHeader);
    mPathEdit->setPlaceholderText(tr("Path"));
    // No built-in clear button — we add our own trailing action that exits edit mode instead

    // ── Shared controls (still wired up the same way) ────────────────────────
    mSearchEdit = new QLineEdit(root);
    mSearchEdit->setPlaceholderText(tr("Search"));
    mSearchEdit->setClearButtonEnabled(true);

    mRefreshBtn = new CosmicButton(root);
    mRefreshBtn->setIcon(QIcon(QStringLiteral(":/Interaction/ResetShift.svg")));
    mRefreshBtn->setIconSize(QSize(16, 16));
    mRefreshBtn->setFixedSize(28, 28);
    mRefreshBtn->setToolTip(tr("Refresh"));
    mRefreshBtn->setBordered(false);
    mRefreshBtn->hide();

    mNewFolderBtn = new CosmicButton(root);
    mNewFolderBtn->setIcon(QIcon(QStringLiteral(":/Interaction/LayerAdd.svg")));
    mNewFolderBtn->setIconSize(QSize(16, 16));
    mNewFolderBtn->setFixedSize(28, 28);
    mNewFolderBtn->setToolTip(tr("New Folder"));
    mNewFolderBtn->setBordered(false);
    mNewFolderBtn->hide();

    mShowHiddenBtn = new CosmicCheckboxToggle(root);
    mShowHiddenBtn->setMode(CosmicCheckboxToggle::Custom);
    mShowHiddenBtn->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mShowHiddenBtn->setBordered(false);
    mShowHiddenBtn->setBackgroundEnabled(false);
    mShowHiddenBtn->setShowIcon(true);
    mShowHiddenBtn->setShowText(false);
    mShowHiddenBtn->setFixedSize(28, 28);
    mShowHiddenBtn->setToolTip(tr("Show Hidden"));
    mShowHiddenBtn->setChecked(false);
    mShowHiddenBtn->setAccentColor(palette().color(QPalette::Highlight));
    mShowHiddenBtn->setCheckedTextColor(Qt::white);
    mShowHiddenBtn->setIcon(QIcon(QStringLiteral(":/Interaction/Hide.svg")));
    mShowHiddenBtn->hide();

    mZoomInBtn = new CosmicButton(root);
    mZoomInBtn->setIcon(QIcon(QStringLiteral(":/Interaction/ZoomIn.svg")));
    mZoomInBtn->setIconSize(QSize(16, 16));
    mZoomInBtn->setFixedSize(28, 28);
    mZoomInBtn->setToolTip(tr("Zoom In"));
    mZoomInBtn->setAutoRepeat(true);
    mZoomInBtn->setAutoRepeatDelay(450);
    mZoomInBtn->setAutoRepeatInterval(45);
    mZoomInBtn->hide();

    mZoomOutBtn = new CosmicButton(root);
    mZoomOutBtn->setIcon(QIcon(QStringLiteral(":/Interaction/ZoomOut.svg")));
    mZoomOutBtn->setIconSize(QSize(16, 16));
    mZoomOutBtn->setFixedSize(28, 28);
    mZoomOutBtn->setToolTip(tr("Zoom Out"));
    mZoomOutBtn->setAutoRepeat(true);
    mZoomOutBtn->setAutoRepeatDelay(450);
    mZoomOutBtn->setAutoRepeatInterval(45);
    mZoomOutBtn->hide();

    // Legacy list/grid buttons (kept for slot wiring, hidden)
    mListBtn = new CosmicButton(root);
    mListBtn->setCheckable(true);
    mListBtn->setChecked(true);
    mListBtn->setFixedSize(28, 28);
    mListBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/ListView.svg")));
    mListBtn->setIconSize(QSize(16, 16));
    mListBtn->hide();

    mGridBtn = new CosmicButton(root);
    mGridBtn->setCheckable(true);
    mGridBtn->setChecked(false);
    mGridBtn->setFixedSize(28, 28);
    mGridBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/GridView.svg")));
    mGridBtn->setIconSize(QSize(16, 16));
    mGridBtn->hide();

    // ── Two-panel body ────────────────────────────────────────────────────────
    // We build a horizontal container that holds:
    //   [left panel: header + sidebar] | [right panel: header + content]
    auto* bodyWidget = new QWidget(root);
    bodyWidget->setObjectName(QStringLiteral("CosmicFileBrowserBody"));
    bodyWidget->setStyleSheet(QStringLiteral("QWidget#CosmicFileBrowserBody { background: transparent; }"));
    auto* bodyLay = new QHBoxLayout(bodyWidget);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    // ── LEFT PANEL ────────────────────────────────────────────────────────────
    auto* leftPanel = new QWidget(bodyWidget);
    leftPanel->setObjectName(QStringLiteral("CosmicFBLeftPanel"));
    leftPanel->setStyleSheet(QStringLiteral(
        "QWidget#CosmicFBLeftPanel { background: #1a1a1a; border-top-left-radius: 14px; border-bottom-left-radius: 14px; }"));
    leftPanel->setFixedWidth(220);
    auto* leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(0);

    // Left header: nav buttons
    mLeftPanelHeader = new QWidget(leftPanel);
    mLeftPanelHeader->setObjectName(QStringLiteral("CosmicFBLeftHeader"));
    mLeftPanelHeader->setStyleSheet(QStringLiteral(
        "QWidget#CosmicFBLeftHeader { background: transparent; }"));
    mLeftPanelHeader->setFixedHeight(44);
    auto* leftHeaderLay = new QHBoxLayout(mLeftPanelHeader);
    leftHeaderLay->setContentsMargins(10, 6, 10, 6);
    leftHeaderLay->setSpacing(4);

    mBackBtn = new CosmicButton(mLeftPanelHeader);
    mBackBtn->setIcon(QIcon(QStringLiteral(":/Interaction/ArrowLeft.svg")));
    mBackBtn->setIconSize(QSize(14, 14));
    mBackBtn->setFixedSize(28, 28);
    mBackBtn->setToolTip(tr("Back"));
    mBackBtn->setBordered(false);

    mForwardBtn = new CosmicButton(mLeftPanelHeader);
    mForwardBtn->setIcon(QIcon(QStringLiteral(":/Interaction/ArrowRight.svg")));
    mForwardBtn->setIconSize(QSize(14, 14));
    mForwardBtn->setFixedSize(28, 28);
    mForwardBtn->setToolTip(tr("Forward"));
    mForwardBtn->setBordered(false);

    mUpBtn = new CosmicButton(mLeftPanelHeader);
    mUpBtn->setIcon(QIcon(QStringLiteral(":/Interaction/ArrowUp.svg")));
    mUpBtn->setIconSize(QSize(14, 14));
    mUpBtn->setFixedSize(28, 28);
    mUpBtn->setToolTip(tr("Up one level"));
    mUpBtn->setBordered(false);

    leftHeaderLay->addWidget(mBackBtn);
    leftHeaderLay->addWidget(mForwardBtn);
    leftHeaderLay->addSpacing(4);
    leftHeaderLay->addWidget(mUpBtn);
    leftHeaderLay->addStretch(1);

    // Thin separator line below left header
    auto* leftSep = new QFrame(leftPanel);
    leftSep->setFrameShape(QFrame::HLine);
    leftSep->setFixedHeight(1);
    leftSep->setStyleSheet(QStringLiteral("QFrame { background: #252525; border: none; }"));

    leftLay->addWidget(mLeftPanelHeader);
    leftLay->addWidget(leftSep);

    // Sidebar list
    mSidebar = new QListWidget(leftPanel);
    mSidebar->setFrameShape(QFrame::NoFrame);
    mSidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mSidebar->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mSidebar->setUniformItemSizes(true);
    mSidebar->setContextMenuPolicy(Qt::CustomContextMenu);
    mSidebar->setDragEnabled(true);
    mSidebar->setAcceptDrops(true);
    mSidebar->setDropIndicatorShown(true);
    mSidebar->setDefaultDropAction(Qt::MoveAction);
    mSidebar->setDragDropMode(QAbstractItemView::InternalMove);
    if (mSidebar->viewport()) {
        mSidebar->viewport()->installEventFilter(this);
    }
    mSidebar->setItemDelegate(new CosmicRoundedItemDelegate(mSidebar));
    mSidebar->setContentsMargins(0, 0, 0, 0);

    leftLay->addWidget(mSidebar, 1);
    bodyLay->addWidget(leftPanel);

    // Thin vertical divider between panels
    auto* panelDivider = new QFrame(bodyWidget);
    panelDivider->setFrameShape(QFrame::VLine);
    panelDivider->setFixedWidth(1);
    panelDivider->setStyleSheet(QStringLiteral("QFrame { background: #252525; border: none; }"));
    bodyLay->addWidget(panelDivider);

    // ── RIGHT PANEL ───────────────────────────────────────────────────────────
    auto* rightPanel = new QWidget(bodyWidget);
    rightPanel->setObjectName(QStringLiteral("CosmicFBRightPanel"));
    rightPanel->setStyleSheet(QStringLiteral(
        "QWidget#CosmicFBRightPanel { background: #242424; border-top-right-radius: 14px; border-bottom-right-radius: 14px; }"));
    auto* rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    // Right header
    mRightPanelHeader = new QWidget(rightPanel);
    mRightPanelHeader->setObjectName(QStringLiteral("CosmicFBRightHeader"));
    mRightPanelHeader->setStyleSheet(QStringLiteral(
        "QWidget#CosmicFBRightHeader { background: transparent; }"));
    mRightPanelHeader->setFixedHeight(44);
    auto* rightHeaderLay = new QHBoxLayout(mRightPanelHeader);
    rightHeaderLay->setContentsMargins(10, 6, 10, 6);
    rightHeaderLay->setSpacing(6);

    // Path squircle label + inline edit (stacked)
    mPathStack = new QStackedWidget(mRightPanelHeader);
    mPathStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mPathStack->setFixedHeight(28);
    mPathStack->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));

    // Page 0: squircle label
    mPathLabelWidget = new QWidget(mPathStack);
    mPathLabelWidget->setObjectName(QStringLiteral("CosmicFBPathLabel"));
    mPathLabelWidget->setCursor(Qt::PointingHandCursor);
    auto* pathLabelLay = new QHBoxLayout(mPathLabelWidget);
    pathLabelLay->setContentsMargins(10, 0, 10, 0);
    pathLabelLay->setSpacing(6);

    // Small home icon
    auto* pathIcon = new QLabel(mPathLabelWidget);
    pathIcon->setPixmap(QIcon(QStringLiteral(":/FileBrowser/Folder.svg")).pixmap(14, 14));
    pathIcon->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
    pathLabelLay->addWidget(pathIcon);

    mPathLabel = new QLabel(mPathLabelWidget);
    mPathLabel->setStyleSheet(QStringLiteral("QLabel { color: #b3b3b3; background: transparent; }"));
    mPathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mPathLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    pathLabelLay->addWidget(mPathLabel, 1);
    mPathStack->addWidget(mPathLabelWidget); // index 0

    // Page 1: inline edit
    mPathEdit->setParent(mPathStack);
    mPathStack->addWidget(mPathEdit); // index 1

    // Trailing action on the path edit: clicking it exits edit mode and restores
    // the path label without committing any typed text.
    {
        QAction* exitEditAction = new QAction(QIcon(QStringLiteral(":/Interaction/CloseWindow.svg")), tr("Cancel"), mPathEdit);
        mPathEdit->addAction(exitEditAction, QLineEdit::TrailingPosition);
        connect(exitEditAction, &QAction::triggered, this, [this]() {
            // Restore the original path text and switch back to label mode
            if (mPathEdit && mActiveSidebarPath.isEmpty() == false) {
                mPathEdit->setText(mActiveSidebarPath);
            }
            if (mPathStack) {
                mPathStack->setCurrentIndex(0);
            }
            updatePathLabel();
        });
    }

    // Install double-click on the label widget to switch to edit mode
    mPathLabelWidget->installEventFilter(this);

    rightHeaderLay->addWidget(mPathStack, 1);

    // Search toggle button (magnifier icon) — checkable so it highlights when active
    mSearchToggleBtn = new CosmicButton(mRightPanelHeader);
    mSearchToggleBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/Search.svg")));
    mSearchToggleBtn->setIconSize(QSize(15, 15));
    mSearchToggleBtn->setFixedSize(28, 28);
    mSearchToggleBtn->setToolTip(tr("Search"));
    mSearchToggleBtn->setBordered(false);
    mSearchToggleBtn->setCheckable(true);
    rightHeaderLay->addWidget(mSearchToggleBtn);

    // Search field lives inside mPathStack as page 2 so it replaces the path
    // display area when search is active — no separate widget in the header.
    mSearchEdit->setParent(mPathStack);
    mSearchEdit->setFixedHeight(28);
    mPathStack->addWidget(mSearchEdit); // index 2

    // Thin separator
    auto* hdrSep1 = new QFrame(mRightPanelHeader);
    hdrSep1->setFrameShape(QFrame::VLine);
    hdrSep1->setFixedWidth(1);
    hdrSep1->setFixedHeight(18);
    hdrSep1->setStyleSheet(QStringLiteral("QFrame { background: #3a3a3a; border: none; }"));
    rightHeaderLay->addWidget(hdrSep1);

    // View mode toggle: list icon ↔ grid icon, single click switches mode
    mViewModeDropBtn = new CosmicButton(mRightPanelHeader);
    mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/ListView.svg")));
    mViewModeDropBtn->setIconSize(QSize(15, 15));
    mViewModeDropBtn->setFixedSize(28, 28);
    mViewModeDropBtn->setToolTip(tr("Toggle view mode"));
    mViewModeDropBtn->setBordered(false);
    rightHeaderLay->addWidget(mViewModeDropBtn);

    // Settings dropdown button (gear icon)
    mSettingsBtn = new CosmicButton(mRightPanelHeader);
    mSettingsBtn->setIcon(QIcon(QStringLiteral(":/Interaction/settings.svg")));
    mSettingsBtn->setIconSize(QSize(15, 15));
    mSettingsBtn->setFixedSize(28, 28);
    mSettingsBtn->setToolTip(tr("Settings"));
    mSettingsBtn->setBordered(false);
    rightHeaderLay->addWidget(mSettingsBtn);

    // Thin separator below right header
    auto* rightSep = new QFrame(rightPanel);
    rightSep->setFrameShape(QFrame::HLine);
    rightSep->setFixedHeight(1);
    rightSep->setStyleSheet(QStringLiteral("QFrame { background: #252525; border: none; }"));

    rightLay->addWidget(mRightPanelHeader);
    rightLay->addWidget(rightSep);

    mFsModel = new CosmicFileSystemModel(this);
    mIconProvider = new CosmicFileIconProvider(mFsModel, activeZoomPx());
    mFsModel->setIconProvider(mIconProvider);
    applyFsModelFilter();

    auto* proxy = new FileNameFilterProxy(this);
    proxy->setSourceModel(mFsModel);
    mProxyModel = proxy;

    const QString activeFilter = (!mNameFilters.isEmpty() ? mNameFilters.first() : QString());
    proxy->setAllowedNameGlobs(globPatternsFromFilterString(activeFilter));

    mViews = new QStackedWidget(rightPanel);

    mTree = new QTreeView(mViews);
    mTree->setModel(proxy);
    mTree->setRootIsDecorated(false);
    mTree->setAlternatingRowColors(false);
    mTree->setSelectionMode(mMode == Mode::OpenFiles ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    mTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTree->setSortingEnabled(true);
    mTree->sortByColumn(0, Qt::AscendingOrder);
    mTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTree->setUniformRowHeights(true);
    mTree->setItemsExpandable(false);
    mTree->setAllColumnsShowFocus(true);
    mTree->header()->setStretchLastSection(true);
    mTree->setColumnHidden(1, false); // Size
    mTree->setColumnHidden(2, false); // Type
    mTree->setColumnHidden(3, false); // Date Modified
    mTree->setContextMenuPolicy(Qt::CustomContextMenu);
    mTree->setDragEnabled(true);
    mTree->setAcceptDrops(true);
    mTree->setDropIndicatorShown(true);
    mTree->setDragDropMode(QAbstractItemView::DragDrop);
    mTree->setDefaultDropAction(Qt::CopyAction);
    mTree->setItemDelegate(new CosmicRoundedItemDelegate(mTree));
    mTree->setMouseTracking(true);
    if (mTree->viewport()) {
        mTree->viewport()->setMouseTracking(true);
        mTree->viewport()->setAttribute(Qt::WA_Hover, true);
    }

    mGrid = new QListView(mViews);
    mGrid->setModel(proxy);
    mGrid->setModelColumn(0);
    mGrid->setViewMode(QListView::IconMode);
    mGrid->setResizeMode(QListView::Adjust);
    mGrid->setSelectionMode(mMode == Mode::OpenFiles ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    mGrid->setUniformItemSizes(true);
    mGrid->viewport()->installEventFilter(this);
    mTree->viewport()->installEventFilter(this);
    mGrid->setIconSize(QSize(56, 56));
    mGrid->setGridSize(QSize(110, 92));
    mGrid->setContextMenuPolicy(Qt::CustomContextMenu);
    mGrid->setDragEnabled(true);
    mGrid->setAcceptDrops(true);
    mGrid->setDropIndicatorShown(true);
    mGrid->setDragDropMode(QAbstractItemView::DragDrop);
    mGrid->setDefaultDropAction(Qt::CopyAction);
    mGrid->setItemDelegate(new CosmicRoundedItemDelegate(mGrid));
    mGrid->setMouseTracking(true);
    if (mGrid->viewport()) {
        mGrid->viewport()->setMouseTracking(true);
        mGrid->viewport()->setAttribute(Qt::WA_Hover, true);
    }

    mViews->addWidget(mTree);
    mViews->addWidget(mGrid);

    // Share a single selection model between both views so list ↔ grid stay in sync
    mGrid->setSelectionModel(mTree->selectionModel());

    rightLay->addWidget(mViews, 1);
    bodyLay->addWidget(rightPanel, 1);

    outer->addWidget(bodyWidget, 1);

    mBottomBar = new QWidget(root);
    mBottomBar->setObjectName(QStringLiteral("CosmicFileBrowserBottom"));
    mBottomBar->setStyleSheet(QStringLiteral("QWidget#CosmicFileBrowserBottom { background: transparent; }") );
    auto* bottomLay = new QHBoxLayout(mBottomBar);
    bottomLay->setContentsMargins(0, 0, 0, 0);
    bottomLay->setSpacing(8);

    mFileNameEdit = new QLineEdit(mBottomBar);
    mFileNameEdit->setPlaceholderText(tr("File name"));

    mCancelBtn = new CosmicButton(tr("Cancel"), mBottomBar);
    mCancelBtn->setBordered(true);

    mAcceptBtn = new CosmicButton(mBottomBar);
    mAcceptBtn->setBordered(true);

    const QString acceptText = (mMode == Mode::SaveFile) ? tr("Save") : tr("Open");
    mAcceptBtn->setText(acceptText);

    if (mMode == Mode::SaveFile) {
        bottomLay->addWidget(mFileNameEdit, 1);
    } else {
        mFileNameEdit->hide();
        bottomLay->addStretch(1);
    }

    bottomLay->addWidget(mAcceptBtn);

    mCancelBtn->hide();

    outer->addWidget(mBottomBar);

    auto* popupLay = new QVBoxLayout(this);
    popupLay->setContentsMargins(0, 0, 0, 0);
    popupLay->setSpacing(0);
    popupLay->addWidget(root);

    applyCosmicStyling();
    applyZoom();
    updateZoomButtonsEnabled();
    updateSidebar();
    scheduleVisibleThumbnailPrefetch();

    // Ensure initial enabled state is correct (especially for SaveFile mode where
    // enabling depends on the filename edit).
    updateAcceptEnabled();

    connect(mUpBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onNavigateUp);
    connect(mBackBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onNavigateBack);
    connect(mForwardBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onNavigateForward);
    connect(mPathEdit, &QLineEdit::editingFinished, this, &CosmicFileBrowserPopup::onPathCommitted);
    connect(mSearchEdit, &QLineEdit::textChanged, this, &CosmicFileBrowserPopup::onSearchChanged);

    connect(mRefreshBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onRefreshClicked);
    connect(mNewFolderBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onNewFolderClicked);
    connect(mShowHiddenBtn, &CosmicCheckboxToggle::toggled, this, &CosmicFileBrowserPopup::onToggleHiddenClicked);
    connect(mShowHiddenBtn, &CosmicCheckboxToggle::toggled, this, [this](bool on) {
        if (!mShowHiddenBtn) {
            return;
        }
        // Icon should reflect the current state.
        // on=true  => hidden files are visible => show "Show" icon
        // on=false => hidden files are hidden  => show "Hide" icon
        mShowHiddenBtn->setIcon(QIcon(on ? QStringLiteral(":/Interaction/Show.svg")
                                       : QStringLiteral(":/Interaction/Hide.svg")));
    });

    connect(mTree, &QTreeView::activated, this, &CosmicFileBrowserPopup::onActivatedInView);
    connect(mGrid, &QListView::activated, this, &CosmicFileBrowserPopup::onActivatedInView);

    if (mTree && mTree->verticalScrollBar()) {
        connect(mTree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { scheduleVisibleThumbnailPrefetch(); });
    }
    if (mTree && mTree->horizontalScrollBar()) {
        connect(mTree->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { scheduleVisibleThumbnailPrefetch(); });
    }
    if (mGrid && mGrid->verticalScrollBar()) {
        connect(mGrid->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { scheduleVisibleThumbnailPrefetch(); });
    }
    if (mGrid && mGrid->horizontalScrollBar()) {
        connect(mGrid->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { scheduleVisibleThumbnailPrefetch(); });
    }

    connect(mSidebar, &QListWidget::itemActivated, this, &CosmicFileBrowserPopup::onSidebarItemActivated);
    connect(mSidebar, &QListWidget::itemChanged, this, &CosmicFileBrowserPopup::onSidebarItemChanged);
    connect(mSidebar, &QWidget::customContextMenuRequested, this, &CosmicFileBrowserPopup::onSidebarContextMenuRequested);

    connect(mTree, &QWidget::customContextMenuRequested, this, &CosmicFileBrowserPopup::onContentContextMenuRequested);
    connect(mGrid, &QWidget::customContextMenuRequested, this, &CosmicFileBrowserPopup::onContentContextMenuRequested);

    // Single connection — both views share the same selection model
    if (mTree->selectionModel()) {
        connect(mTree->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &CosmicFileBrowserPopup::onSelectionChanged);
    }

    connect(mAcceptBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onAcceptClicked);
    // No explicit Cancel button; Escape key and the window header close button handle cancellation.

    if (mMode == Mode::SaveFile && mFileNameEdit) {
        connect(mFileNameEdit, &QLineEdit::textChanged, this, [this](const QString&) {
            updateAcceptEnabled();
        });
        connect(mFileNameEdit, &QLineEdit::returnPressed, this, &CosmicFileBrowserPopup::onAcceptClicked);
    }

    connect(mListBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onViewModeList);
    connect(mGridBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onViewModeGrid);

    // New header button connections
    connect(mSearchToggleBtn, &QPushButton::clicked, this, [this]() {
        setSearchVisible(!mSearchVisible);
    });
    connect(mViewModeDropBtn, &QPushButton::clicked, this, [this]() {
        if (mViewMode == ViewMode::List) {
            onViewModeGrid();
            if (mViewModeDropBtn) {
                mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/GridView.svg")));
            }
        } else {
            onViewModeList();
            if (mViewModeDropBtn) {
                mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/ListView.svg")));
            }
        }
    });
    connect(mSettingsBtn, &QPushButton::clicked, this, [this]() {
        showSettingsDropdown();
    });
    // Commit path edit on Enter / focus-out
    connect(mPathEdit, &QLineEdit::editingFinished, this, [this]() {
        onPathCommitted();
        if (mPathStack) {
            mPathStack->setCurrentIndex(0);
        }
        updatePathLabel();
    });

    auto applyZoomWithSaveDebounce = [this]() {
        applyZoom();
        updateZoomButtonsEnabled();
        if (mUiSaveDebounce) {
            mUiSaveDebounce->start();
        }
    };

    connect(mZoomInBtn, &QPushButton::clicked, this, [this, applyZoomWithSaveDebounce]() {
        const int cur = activeZoomPx();
        const int next = qBound(minZoomPxForViewMode(mViewMode), cur + 8, maxZoomPxForViewMode(mViewMode));
        if (next == cur) {
            updateZoomButtonsEnabled();
            return;
        }
        setActiveZoomPx(next);
        applyZoomWithSaveDebounce();
    });
    connect(mZoomOutBtn, &QPushButton::clicked, this, [this, applyZoomWithSaveDebounce]() {
        const int cur = activeZoomPx();
        const int next = qBound(minZoomPxForViewMode(mViewMode), cur - 8, maxZoomPxForViewMode(mViewMode));
        if (next == cur) {
            updateZoomButtonsEnabled();
            return;
        }
        setActiveZoomPx(next);
        applyZoomWithSaveDebounce();
    });

    if (mAddBookmarkBtn) {
        connect(mAddBookmarkBtn, &QPushButton::clicked, this, &CosmicFileBrowserPopup::onAddBookmarkClicked);
    }

    connect(mSidebar, &QListWidget::itemPressed, this, [this](QListWidgetItem* it) {
        if (mSidebarIsDragging) {
            return;
        }
        if (!it) {
            return;
        }

        // Add Bookmark button row
        if (it->data(Qt::UserRole + 20).toBool()) {
            mSidebar->clearSelection();
            mSidebar->setCurrentItem(nullptr);
            onAddBookmarkClicked();
            return;
        }

        // Section header — toggle expand/collapse on every press
        if (it->data(Qt::UserRole + 10).toBool()) {
            void* ptr = it->data(Qt::UserRole + 12).value<void*>();
            if (ptr) {
                bool* expanded = static_cast<bool*>(ptr);
                *expanded = !(*expanded);
                it->setData(Qt::UserRole + 11, *expanded);
                mSidebar->clearSelection();
                mSidebar->setCurrentItem(nullptr);
                applySidebarSectionVisibility();
                saveUiSettings();
            }
            return;
        }

        const QString p = it->data(Qt::UserRole).toString();
        if (!p.isEmpty()) {
            setCurrentDir(p, true);
        }
    });

    updateAcceptEnabled();

    mBackBtn->setEnabled(!mBackStack.isEmpty());
    mForwardBtn->setEnabled(!mForwardStack.isEmpty());
}

void CosmicFileBrowserPopup::applyCosmicStyling()
{
    // Important: this popup is not a child widget of MainWindow2, so it may not inherit
    // the app's accent/highlight color from QSS. Explicitly propagate the highlight.
    // Use the Cosmic blue-violet accent as the single source of truth for selection/checked highlights.
    // This avoids any mismatch when dialogs don't inherit MainWindow QSS palette state.
    const QColor accent = cosmicLockedAccent();

    QPalette pal = palette();
    pal.setColor(QPalette::Text, cosmicText());
    pal.setColor(QPalette::WindowText, cosmicText());
    pal.setColor(QPalette::ButtonText, cosmicText());
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(pal);

    const int r = CosmicUiMetrics::cornerRadius();

    // Keep toggle highlight in sync with the (now forced) popup highlight.
    if (mShowHiddenBtn) {
        mShowHiddenBtn->setAccentColor(accent);
        mShowHiddenBtn->setCheckedTextColor(Qt::white);
    }

    // Path squircle label: cosmic corner radius + thin border
    if (mPathLabelWidget) {
        mPathLabelWidget->setStyleSheet(QString(
            "QWidget#CosmicFBPathLabel { background: #2e2e2e; border-radius: %1px; border: 1px solid #3d3d3d; }"
        ).arg(r));
    }

    const QString lineEditStyle = QString(
        "QLineEdit { background: #1f1f1f; color: %1; border: 1px solid %2; border-radius: %3px; padding: 0px 8px; height: %4px;"
        " selection-background-color: %2; selection-color: #ffffff; }"
        "QLineEdit:focus { background: #1f1f1f; }"
    ).arg(cosmicText().name()).arg(CosmicColor::accentHex()).arg(r).arg(CosmicUiMetrics::controlThickness());

    mPathEdit->setStyleSheet(lineEditStyle);
    mSearchEdit->setStyleSheet(lineEditStyle);
    mFileNameEdit->setStyleSheet(lineEditStyle);

    if (mSidebar) {
        mSidebar->setFocusPolicy(Qt::NoFocus);
    }
    if (mTree) {
        mTree->setFocusPolicy(Qt::NoFocus);
    }
    if (mGrid) {
        mGrid->setFocusPolicy(Qt::NoFocus);
    }

    // Sidebar: transparent bg (left panel widget provides the background)
    mSidebar->setStyleSheet(QString(
        "QListWidget { background: transparent; color: %1; padding: 0px; border: none; }"
        "QListWidget::item { padding: 4px 6px 4px 0px; border-radius: %2px; border: none; outline: none; }"
        "QListWidget::item:hover { background: #2e2e2e; }"
        "QListWidget::item:selected { background: %3; color: white; }"
        "QListWidget::item:selected:active { outline: none; }"
    ).arg(cosmicText().name()).arg(r).arg(accent.name()));

    const QString viewStyle = QString(
        "QTreeView, QListView { background: #242424; color: %1; border: none; }"
        "QHeaderView::section { background: #242424; color: %1; border: none; padding: 4px 8px; }"
        "QTreeView::item { padding: 4px 8px; border: none; outline: none; }"
        "QTreeView::item:selected, QListView::item:selected { background: %2; color: white; }"
        "QTreeView::item:focus, QListView::item:focus { outline: none; }"
    ).arg(cosmicText().name()).arg(accent.name());

    mTree->setStyleSheet(viewStyle);
    mGrid->setStyleSheet(viewStyle);

    const QString scrollBarStyle = QStringLiteral(
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
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  background: transparent;"
        "  width: 0px;"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: transparent;"
        "}"
    );

    if (mSidebar && mSidebar->verticalScrollBar()) {
        mSidebar->verticalScrollBar()->setFixedWidth(12);
        mSidebar->verticalScrollBar()->setStyleSheet(scrollBarStyle);
    }
    if (mTree) {
        if (mTree->verticalScrollBar()) {
            mTree->verticalScrollBar()->setFixedWidth(12);
            mTree->verticalScrollBar()->setStyleSheet(scrollBarStyle);
        }
        if (mTree->horizontalScrollBar()) {
            mTree->horizontalScrollBar()->setFixedHeight(12);
            mTree->horizontalScrollBar()->setStyleSheet(scrollBarStyle);
        }
    }
    if (mGrid) {
        if (mGrid->verticalScrollBar()) {
            mGrid->verticalScrollBar()->setFixedWidth(12);
            mGrid->verticalScrollBar()->setStyleSheet(scrollBarStyle);
        }
        if (mGrid->horizontalScrollBar()) {
            mGrid->horizontalScrollBar()->setFixedHeight(12);
            mGrid->horizontalScrollBar()->setStyleSheet(scrollBarStyle);
        }
    }
}

void CosmicFileBrowserPopup::updatePathLabel()
{
    if (!mPathLabel || !mPathEdit) {
        return;
    }
    const QString p = mPathEdit->text().trimmed();
    mPathLabel->setText(p.isEmpty() ? tr("(no path)") : p);
}

void CosmicFileBrowserPopup::setSearchVisible(bool visible)
{
    mSearchVisible = visible;
    if (!mSearchEdit || !mPathStack) {
        return;
    }

    // Sync the toggle button's checked state so it highlights when active
    if (mSearchToggleBtn) {
        mSearchToggleBtn->setChecked(visible);
    }

    if (visible) {
        // Switch the path area to show the search input (page 2)
        mPathStack->setCurrentIndex(2);
        mSearchEdit->setFocus();
        mSearchEdit->selectAll();
    } else {
        // Clear search and restore the path label (page 0)
        mSearchEdit->clear();
        mPathStack->setCurrentIndex(0);
    }
}

void CosmicFileBrowserPopup::showViewModeDropdown()
{
    if (!mViewModeDropBtn) {
        return;
    }
    CosmicPopupContextMenu menu(this);
    menu.addItem(tr("List View"),  QStringLiteral("list"),  true, QIcon(QStringLiteral(":/FileBrowser/ListView.svg")));
    menu.addItem(tr("Grid View"),  QStringLiteral("grid"),  true, QIcon(QStringLiteral(":/FileBrowser/GridView.svg")));

    const QPoint globalPos = mViewModeDropBtn->mapToGlobal(QPoint(0, mViewModeDropBtn->height() + 4));
    const QString chosen = menu.execAt(globalPos);
    if (chosen == QStringLiteral("list")) {
        onViewModeList();
        if (mViewModeDropBtn) {
            mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/ListView.svg")));
        }
    } else if (chosen == QStringLiteral("grid")) {
        onViewModeGrid();
        if (mViewModeDropBtn) {
            mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/GridView.svg")));
        }
    }
}

void CosmicFileBrowserPopup::showSettingsDropdown()
{
    if (!mSettingsBtn) {
        return;
    }
    CosmicPopupContextMenu menu(this);
    menu.addItem(tr("Refresh"),          QStringLiteral("refresh"),    true);
    menu.addItem(tr("New Folder"),        QStringLiteral("newfolder"),  true);
    menu.addSeparator();
    menu.addCheckableItem(tr("Show Hidden Files"), QStringLiteral("hidden"), mShowHidden);
    menu.addSeparator();
    menu.addItem(tr("Zoom In"),           QStringLiteral("zoomin"),     mZoomInBtn ? mZoomInBtn->isEnabled() : true);
    menu.addItem(tr("Zoom Out"),          QStringLiteral("zoomout"),    mZoomOutBtn ? mZoomOutBtn->isEnabled() : true);

    const QPoint globalPos = mSettingsBtn->mapToGlobal(QPoint(0, mSettingsBtn->height() + 4));
    const QString chosen = menu.execAt(globalPos);

    if (chosen == QStringLiteral("refresh")) {
        onRefreshClicked();
    } else if (chosen == QStringLiteral("newfolder")) {
        onNewFolderClicked();
    } else if (chosen == QStringLiteral("hidden")) {
        const bool newState = !mShowHidden;
        if (mShowHiddenBtn) {
            mShowHiddenBtn->setChecked(newState);
        }
        onToggleHiddenClicked(newState);
    } else if (chosen == QStringLiteral("zoomin")) {
        if (mZoomInBtn) {
            emit mZoomInBtn->clicked();
        }
    } else if (chosen == QStringLiteral("zoomout")) {
        if (mZoomOutBtn) {
            emit mZoomOutBtn->clicked();
        }
    }
}

void CosmicFileBrowserPopup::applyZoom()
{
    if (!mTree || !mGrid) {
        return;
    }

    // Grid: thumbnails / large icons.
    const int gridPx = qBound(minZoomPxForViewMode(ViewMode::Grid), zoomPxForViewMode(ViewMode::Grid), maxZoomPxForViewMode(ViewMode::Grid));
    mGrid->setIconSize(QSize(gridPx, gridPx));
    mGrid->setGridSize(QSize(gridPx + 36, gridPx + 52));

    // List: smaller icons.
    // Keep list/grid zoom unified: list icons track the same zoom value, just clamped to a reasonable max.
    const int listPx = qBound(minZoomPxForViewMode(ViewMode::List), zoomPxForViewMode(ViewMode::List), maxZoomPxForViewMode(ViewMode::List));
    mTree->setIconSize(QSize(listPx, listPx));

    if (auto* p = dynamic_cast<CosmicFileIconProvider*>(mIconProvider)) {
        const int thumbPx = (mViewMode == ViewMode::Grid) ? gridPx : listPx;
        p->setThumbnailPx(thumbPx);
        p->setThumbnailsEnabled(true);
    }

    // Trigger visual refresh.
    mTree->viewport()->update();
    mGrid->viewport()->update();
}

void CosmicFileBrowserPopup::updateZoomButtonsEnabled()
{
    if (!mZoomInBtn || !mZoomOutBtn) {
        return;
    }

    const int cur = activeZoomPx();
    const int minPx = minZoomPxForViewMode(mViewMode);
    const int maxPx = maxZoomPxForViewMode(mViewMode);
    mZoomOutBtn->setEnabled(cur > minPx);
    mZoomInBtn->setEnabled(cur < maxPx);
}

void CosmicFileBrowserPopup::scheduleVisibleThumbnailPrefetch()
{
    if (mThumbPrefetchDebounce) {
        mThumbPrefetchDebounce->start();
    }
}

void CosmicFileBrowserPopup::prefetchVisibleThumbnailsNow()
{
    QAbstractItemView* view = activeContentView();
    if (!view || !view->model() || !view->viewport()) {
        return;
    }

    const QRect vr = view->viewport()->rect();
    if (vr.isEmpty()) {
        return;
    }

    // Querying DecorationRole triggers our icon provider and queues async thumbnail jobs.
    // This is intentionally lightweight: only visible items (debounced), no synchronous decoding.
    if (auto* tv = qobject_cast<QTreeView*>(view)) {
        // indexAt(0,0) can land in the header margin; probe a few y offsets.
        QModelIndex idx;
        for (int y = 0; y <= 24 && !idx.isValid(); y += 6) {
            idx = tv->indexAt(QPoint(2, y));
        }
        if (!idx.isValid()) {
            return;
        }
        idx = idx.sibling(idx.row(), 0);
        const int yLimit = vr.bottom() + 80;
        for (int i = 0; idx.isValid() && i < 2048; ++i) {
            const QRect ir = tv->visualRect(idx);
            if (!ir.isValid()) {
                break;
            }
            if (ir.top() > yLimit) {
                break;
            }
            (void)view->model()->data(idx, Qt::DecorationRole);
            idx = tv->indexBelow(idx);
            if (idx.isValid()) {
                idx = idx.sibling(idx.row(), 0);
            }
        }

        // Ensure we repaint after queuing/cache hits, so the user doesn't need a mouse move.
        view->viewport()->update(vr);
        return;
    }

    if (auto* lv = qobject_cast<QListView*>(view)) {
        // Sampling points can miss items depending on spacing/flow. Instead, walk rows and check visualRect.
        const int rc = view->model()->rowCount(view->rootIndex());
        if (rc <= 0) {
            return;
        }

        QModelIndex first;
        for (int y = 0; y <= 32 && !first.isValid(); y += 8) {
            first = lv->indexAt(QPoint(4, y));
        }
        if (!first.isValid()) {
            // No item at the top-left; fall back to probing the center.
            first = lv->indexAt(QPoint(vr.center().x(), 4));
        }
        if (!first.isValid()) {
            return;
        }

        int firstRow = qBound(0, first.row(), rc - 1);
        // To avoid scanning huge directories, cap work per pass.
        const int maxScan = qMin(rc, 800);
        int scanned = 0;

        for (int r = firstRow; r < rc && scanned < maxScan; ++r) {
            const QModelIndex idx = view->model()->index(r, 0, view->rootIndex());
            if (!idx.isValid()) {
                continue;
            }
            const QRect ir = lv->visualRect(idx);
            if (!ir.isValid()) {
                continue;
            }
            if (ir.top() > vr.bottom() + 120) {
                break;
            }
            if (ir.bottom() < vr.top() - 120) {
                continue;
            }
            (void)view->model()->data(idx, Qt::DecorationRole);
            ++scanned;
        }

        // Ensure we repaint after queuing/cache hits, so the user doesn't need a mouse move.
        view->viewport()->update(vr);

        return;
    }
}

int CosmicFileBrowserPopup::minZoomPxForViewMode(ViewMode mode) const
{
    // Keep list mode more compact; grid mode allows larger previews.
    return (mode == ViewMode::Grid) ? 32 : 16;
}

int CosmicFileBrowserPopup::maxZoomPxForViewMode(ViewMode mode) const
{
    return (mode == ViewMode::Grid) ? 192 : 96;
}

int CosmicFileBrowserPopup::zoomPxForViewMode(ViewMode mode) const
{
    return (mode == ViewMode::Grid) ? mZoomGridPx : mZoomListPx;
}

void CosmicFileBrowserPopup::setZoomPxForViewMode(ViewMode mode, int px)
{
    const int clamped = qBound(minZoomPxForViewMode(mode), px, maxZoomPxForViewMode(mode));
    if (mode == ViewMode::Grid) {
        mZoomGridPx = clamped;
    } else {
        mZoomListPx = clamped;
    }
}

int CosmicFileBrowserPopup::activeZoomPx() const
{
    return qBound(minZoomPxForViewMode(mViewMode), zoomPxForViewMode(mViewMode), maxZoomPxForViewMode(mViewMode));
}

void CosmicFileBrowserPopup::setActiveZoomPx(int px)
{
    setZoomPxForViewMode(mViewMode, px);
    mZoomMasterPx = activeZoomPx();
}

void CosmicFileBrowserPopup::onRefreshClicked()
{
    refreshCurrentDir();
}

void CosmicFileBrowserPopup::onNewFolderClicked()
{
    const QString dir = currentDir();
    if (dir.isEmpty()) {
        return;
    }

    // Cosmic-like modal input popup (Mac look) instead of QInputDialog.
    class NewFolderPopup final : public CosmicPopupWindow
    {
    public:
        explicit NewFolderPopup(QWidget* parent = nullptr)
            : CosmicPopupWindow(parent)
        {
            setModalMode(ModalMode::WindowModal);
            setClosePolicy(ClosePolicy::Manual);
            setResizable(false);
            setFixedWindowSize(QSize(420, 130));
            setShadowEnabled(false);
            setShadowPadding(0);
            setPanelColor(QColor(0x2f, 0x2f, 0x2f));
            setBorderColor(CosmicColor::popupBorder());
            setBorderWidth(2);

            auto* root = new QWidget(this);
            root->setObjectName(QStringLiteral("NewFolderPopupRoot"));
            root->setStyleSheet(QStringLiteral("QWidget#NewFolderPopupRoot{ background: transparent; border: none; }") );
            auto* lay = new QVBoxLayout(root);
            lay->setContentsMargins(16, 16, 16, 16);
            lay->setSpacing(10);

            mEdit = new QLineEdit(root);
            mEdit->setPlaceholderText(QCoreApplication::translate("CosmicFileBrowserPopup", "New folder name"));
            mEdit->setClearButtonEnabled(true);
            mEdit->setFrame(false);
            lay->addWidget(mEdit);

            auto* btnRow = new QHBoxLayout();
            btnRow->setContentsMargins(0, 0, 0, 0);
            btnRow->setSpacing(8);
            btnRow->addStretch(1);

            mCancel = new CosmicButton(QCoreApplication::translate("CosmicFileBrowserPopup", "Cancel"), root);
            mCancel->setBordered(true);
            mCreate = new CosmicButton(QCoreApplication::translate("CosmicFileBrowserPopup", "Create"), root);
            mCreate->setBordered(true);
            btnRow->addWidget(mCancel);
            btnRow->addWidget(mCreate);
            lay->addLayout(btnRow);

            auto* outer = new QVBoxLayout(this);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->setSpacing(0);
            outer->addWidget(root);

            connect(mCancel, &QPushButton::clicked, this, &QDialog::reject);
            connect(mCreate, &QPushButton::clicked, this, &QDialog::accept);
            connect(mEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
        }

        QString text() const { return mEdit ? mEdit->text() : QString(); }

        void applyLineEditStyle(const QString& style)
        {
            if (mEdit) {
                mEdit->setStyleSheet(style);
            }
        }

    private:
        QPointer<QLineEdit> mEdit;
        QPointer<CosmicButton> mCancel;
        QPointer<CosmicButton> mCreate;
    };

    NewFolderPopup popup(this);
    // Match Cosmic line edit styling used by file browser header.
    {
        popup.applyLineEditStyle(CosmicInlineEdit::stylesheet());
    }
    popup.exec();
    const QString name = popup.text().trimmed();
    if (popup.result() != QDialog::Accepted || name.isEmpty()) {
        return;
    }
    if (name.contains(QDir::separator())) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Invalid name."));
        return;
    }

    const QString full = QDir(dir).absoluteFilePath(name);
    if (QFileInfo(full).exists()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("A file or folder with that name already exists."));
        return;
    }

    if (!QDir().mkpath(full)) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to create '%1'.").arg(full));
        return;
    }

    refreshCurrentDir();
}

void CosmicFileBrowserPopup::onToggleHiddenClicked(bool on)
{
    mShowHidden = on;
    applyFsModelFilter();
    refreshCurrentDir();
    if (mUiSaveDebounce) {
        mUiSaveDebounce->start();
    }
}

void CosmicFileBrowserPopup::onTypeSelectTimeout()
{
    mTypeSelectBuffer.clear();
}

void CosmicFileBrowserPopup::startRenameForPath(const QString& absolutePath)
{
    if (absolutePath.trimmed().isEmpty() || !mFsModel) {
        return;
    }

    QAbstractItemView* view = activeContentView();
    if (!view || !view->model()) {
        return;
    }

    const QModelIndex srcIdx = mFsModel->index(absolutePath);
    if (!srcIdx.isValid()) {
        return;
    }

    QModelIndex idx = srcIdx;
    if (auto* proxy = qobject_cast<QAbstractProxyModel*>(view->model())) {
        idx = proxy->mapFromSource(srcIdx);
    }
    if (!idx.isValid()) {
        return;
    }

    view->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    view->scrollTo(idx);
    opRename();
}

QAbstractItemView* CosmicFileBrowserPopup::activeContentView() const
{
    if (mViewMode == ViewMode::Grid) {
        return mGrid;
    }
    return mTree;
}

QStringList CosmicFileBrowserPopup::selectedPathsInActiveView() const
{
    QStringList out;
    QAbstractItemView* view = activeContentView();
    if (!view || !view->selectionModel()) {
        return out;
    }

    // QListView (IconMode) doesn't reliably report selectedRows(); use selectedIndexes() and
    // normalize to column 0.
    const QModelIndexList sel = view->selectionModel()->selectedIndexes();
    for (QModelIndex idx : sel) {
        if (!idx.isValid()) {
            continue;
        }
        if (idx.column() != 0) {
            idx = idx.sibling(idx.row(), 0);
        }

        const QFileInfo fi = fileInfoForViewIndex(view, idx);
        if (fi.exists()) {
            out.push_back(fi.absoluteFilePath());
        }
    }

    out.removeDuplicates();
    return out;
}

void CosmicFileBrowserPopup::opCopy(bool cut)
{
    const QStringList selectedPaths = selectedPathsInActiveView();
    if (selectedPaths.isEmpty()) {
        return;
    }

    auto* mime = new QMimeData();
    QList<QUrl> urls;
    urls.reserve(selectedPaths.size());
    for (const QString& p : selectedPaths) {
        urls.push_back(QUrl::fromLocalFile(p));
    }
    mime->setUrls(urls);
    mime->setData(kCutMimeType(), cut ? QByteArray("1") : QByteArray("0"));
    QApplication::clipboard()->setMimeData(mime);
}

void CosmicFileBrowserPopup::opPaste(const QString& targetDir)
{
    const QClipboard* cb = QApplication::clipboard();
    const QMimeData* md = cb ? cb->mimeData() : nullptr;
    if (!md || !md->hasUrls() || md->urls().isEmpty()) {
        return;
    }

    const QString dstDir = QDir::cleanPath(targetDir);
    if (dstDir.isEmpty()) {
        return;
    }

    auto showError = [&](const QString& msg) {
        QMessageBox::critical(this, tr("File operation failed"), msg);
    };

    auto conflictPrompt = [&](const QString& dstPath) {
        QMessageBox mb(this);
        mb.setIcon(QMessageBox::Question);
        mb.setWindowTitle(tr("File exists"));
        mb.setText(tr("'%1' already exists.").arg(dstPath));
        QPushButton* overwriteBtn = mb.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
        QPushButton* skipBtn = mb.addButton(tr("Skip"), QMessageBox::RejectRole);
        QPushButton* cancelBtn = mb.addButton(tr("Cancel"), QMessageBox::DestructiveRole);
        mb.setDefaultButton(skipBtn);
        mb.exec();
        if (mb.clickedButton() == overwriteBtn) return QStringLiteral("overwrite");
        if (mb.clickedButton() == skipBtn) return QStringLiteral("skip");
        Q_UNUSED(cancelBtn);
        return QStringLiteral("cancel");
    };

    const bool isCut = (md->data(kCutMimeType()) == QByteArray("1"));
    const QList<QUrl> urls = md->urls();

    for (const QUrl& url : urls) {
        const QString srcPath = QDir::cleanPath(url.toLocalFile());
        if (srcPath.isEmpty()) {
            continue;
        }
        QFileInfo srcFi(srcPath);
        if (!srcFi.exists()) {
            continue;
        }

        const QString dstPath = QDir(dstDir).filePath(srcFi.fileName());
        if (QDir::cleanPath(dstPath) == QDir::cleanPath(srcPath)) {
            continue;
        }

        if (srcFi.isDir() && isSubPathOf(srcPath, dstPath)) {
            showError(tr("Cannot paste a folder into itself."));
            return;
        }

        if (QFileInfo(dstPath).exists()) {
            const QString decision = conflictPrompt(dstPath);
            if (decision == QStringLiteral("skip")) {
                continue;
            }
            if (decision == QStringLiteral("cancel")) {
                return;
            }
        }

        QString err;
        bool ok = false;

        if (isCut) {
            if (QDir().rename(srcPath, dstPath)) {
                ok = true;
            } else {
                if (srcFi.isDir()) {
                    ok = copyDirRecursive(srcPath, dstPath, &err);
                    if (ok) {
                        ok = QDir(srcPath).removeRecursively();
                        if (!ok && err.isEmpty()) {
                            err = tr("Failed to remove source folder '%1'.").arg(srcPath);
                        }
                    }
                } else {
                    ok = copyFileWithOverwrite(srcPath, dstPath, &err);
                    if (ok) {
                        ok = QFile::remove(srcPath);
                        if (!ok && err.isEmpty()) {
                            err = tr("Failed to remove source file '%1'.").arg(srcPath);
                        }
                    }
                }
            }
        } else {
            if (srcFi.isDir()) {
                ok = copyDirRecursive(srcPath, dstPath, &err);
            } else {
                ok = copyFileWithOverwrite(srcPath, dstPath, &err);
            }
        }

        if (!ok) {
            if (err.isEmpty()) {
                err = tr("Operation failed for '%1'.").arg(srcPath);
            }
            showError(err);
            return;
        }
    }

    if (isCut) {
        QApplication::clipboard()->clear();
    }

    setCurrentDir(currentDir(), false);
}

void CosmicFileBrowserPopup::opRename()
{
    const QStringList selectedPaths = selectedPathsInActiveView();
    if (selectedPaths.size() != 1) {
        return;
    }

    const QString srcPath = selectedPaths.first();
    QFileInfo srcFi(srcPath);
    if (!srcFi.exists()) {
        return;
    }

    // Mac-style rename popup (Cosmic window) instead of inline overlay.
    class RenamePopup final : public CosmicPopupWindow
    {
    public:
        explicit RenamePopup(const QString& initialName, QWidget* parent = nullptr)
            : CosmicPopupWindow(parent)
        {
            setModalMode(ModalMode::WindowModal);
            setClosePolicy(ClosePolicy::Manual);
            setResizable(false);
            setFixedWindowSize(QSize(420, 130));
            setShadowEnabled(false);
            setShadowPadding(0);
            setPanelColor(QColor(0x2f, 0x2f, 0x2f));
            setBorderColor(CosmicColor::popupBorder());
            setBorderWidth(2);

            auto* root = new QWidget(this);
            root->setObjectName(QStringLiteral("RenamePopupRoot"));
            root->setStyleSheet(QStringLiteral("QWidget#RenamePopupRoot{ background: transparent; border: none; }") );
            auto* lay = new QVBoxLayout(root);
            lay->setContentsMargins(16, 16, 16, 16);
            lay->setSpacing(10);

            mEdit = new QLineEdit(root);
            mEdit->setText(initialName);
            mEdit->setClearButtonEnabled(true);
            mEdit->setFrame(false);
            lay->addWidget(mEdit);

            // Select all text on open so the user can immediately type to replace
            QTimer::singleShot(0, mEdit, [this]() {
                if (mEdit) { mEdit->selectAll(); mEdit->setFocus(); }
            });

            auto* btnRow = new QHBoxLayout();
            btnRow->setContentsMargins(0, 0, 0, 0);
            btnRow->setSpacing(8);
            btnRow->addStretch(1);

            mCancel = new CosmicButton(QCoreApplication::translate("CosmicFileBrowserPopup", "Cancel"), root);
            mCancel->setBordered(true);
            mOk = new CosmicButton(QCoreApplication::translate("CosmicFileBrowserPopup", "Rename"), root);
            mOk->setBordered(true);
            btnRow->addWidget(mCancel);
            btnRow->addWidget(mOk);
            lay->addLayout(btnRow);

            auto* outer = new QVBoxLayout(this);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->setSpacing(0);
            outer->addWidget(root);

            connect(mCancel, &QPushButton::clicked, this, &QDialog::reject);
            connect(mOk, &QPushButton::clicked, this, &QDialog::accept);
            connect(mEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
        }

        void applyLineEditStyle(const QString& style)
        {
            if (mEdit) {
                mEdit->setStyleSheet(style);
            }
        }

        QString text() const { return mEdit ? mEdit->text() : QString(); }

    private:
        QPointer<QLineEdit> mEdit;
        QPointer<CosmicButton> mCancel;
        QPointer<CosmicButton> mOk;
    };

    RenamePopup popup(srcFi.fileName(), this);
    {
        popup.applyLineEditStyle(CosmicInlineEdit::stylesheet());
    }
    popup.exec();

    if (popup.result() != QDialog::Accepted) {
        return;
    }

    const QString newName = popup.text().trimmed();
    const QString currentName = srcFi.fileName();
    if (newName.isEmpty() || newName == currentName) {
        return;
    }
    if (newName.contains(QDir::separator())) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Invalid name."));
        return;
    }

    const QString dstPath = QDir(srcFi.absolutePath()).filePath(newName);
    if (QFileInfo(dstPath).exists()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("A file or folder with that name already exists."));
        return;
    }

    if (!QDir().rename(srcFi.absoluteFilePath(), dstPath)) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to rename '%1'.").arg(srcFi.absoluteFilePath()));
        return;
    }

    refreshCurrentDir();
}

void CosmicFileBrowserPopup::opDelete()
{
    const QStringList selectedPaths = selectedPathsInActiveView();
    if (selectedPaths.isEmpty()) {
        return;
    }

    class DeletePopup final : public CosmicPopupWindow
    {
    public:
        explicit DeletePopup(int count, QWidget* parent = nullptr)
            : CosmicPopupWindow(parent)
        {
            setModalMode(ModalMode::WindowModal);
            setClosePolicy(ClosePolicy::Manual);
            setResizable(false);
            setFixedWindowSize(QSize(460, 150));
            setShadowEnabled(false);
            setShadowPadding(0);
            setPanelColor(QColor(0x2f, 0x2f, 0x2f));
            setBorderColor(CosmicColor::popupBorder());
            setBorderWidth(2);

            auto* root = new QWidget(this);
            root->setObjectName(QStringLiteral("DeletePopupRoot"));
            root->setStyleSheet(QStringLiteral("QWidget#DeletePopupRoot{ background: transparent; border: none; }") );
            auto* lay = new QVBoxLayout(root);
            lay->setContentsMargins(16, 16, 16, 16);
            lay->setSpacing(10);

            auto* title = new QLabel(tr("Delete"), root);
            title->setStyleSheet(QStringLiteral("QLabel{ background: transparent; color: #ffffff; font-weight: 700; }") );
            lay->addWidget(title);

            const QString msg = (count == 1)
                                    ? tr("Delete the selected item? This cannot be undone.")
                                    : tr("Delete the selected items? This cannot be undone.");
            auto* body = new QLabel(msg, root);
            body->setWordWrap(true);
            body->setStyleSheet(QStringLiteral("QLabel{ background: transparent; color: #b3b3b3; }") );
            lay->addWidget(body);

            auto* btnRow = new QHBoxLayout();
            btnRow->setContentsMargins(0, 0, 0, 0);
            btnRow->setSpacing(8);
            btnRow->addStretch(1);

            mCancel = new CosmicButton(tr("Cancel"), root);
            mCancel->setBordered(true);
            mDelete = new CosmicButton(tr("Delete"), root);
            mDelete->setBordered(true);
            btnRow->addWidget(mCancel);
            btnRow->addWidget(mDelete);
            lay->addLayout(btnRow);

            auto* outer = new QVBoxLayout(this);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->setSpacing(0);
            outer->addWidget(root);

            connect(mCancel, &QPushButton::clicked, this, &QDialog::reject);
            connect(mDelete, &QPushButton::clicked, this, &QDialog::accept);
        }

    private:
        QPointer<CosmicButton> mCancel;
        QPointer<CosmicButton> mDelete;
    };

    DeletePopup popup(selectedPaths.size(), this);
    popup.exec();
    if (popup.result() != QDialog::Accepted) {
        return;
    }

    for (const QString& p : selectedPaths) {
        QFileInfo fi(p);
        if (!fi.exists()) {
            continue;
        }

        bool ok = false;
        if (fi.isDir()) {
            ok = QDir(p).removeRecursively();
        } else {
            ok = QFile::remove(p);
        }

        if (!ok) {
            QMessageBox::critical(this, tr("File operation failed"), tr("Failed to delete '%1'.").arg(p));
            return;
        }
    }

    setCurrentDir(currentDir(), false);
}

void CosmicFileBrowserPopup::keyPressEvent(QKeyEvent* e)
{
    if (!e) {
        CosmicPopupWindow::keyPressEvent(e);
        return;
    }

    // Escape: if search is active, close search first rather than closing the dialog.
    if (e->key() == Qt::Key_Escape) {
        if (mSearchVisible) {
            e->accept();
            setSearchVisible(false);
            return;
        }
        e->accept();
        onCancelClicked();
        return;
    }

    // Enter/Return: when focus is inside the content view, the view's own activated
    // signal handles it (emitted by QAbstractItemView on Enter) and calls onActivatedInView.
    // We only need to act here as a fallback when focus is elsewhere (e.g. on the dialog itself).
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        const QWidget* fw = QApplication::focusWidget();
        const bool focusInLineEdit = qobject_cast<const QLineEdit*>(fw) != nullptr;
        // Line-edits handle returnPressed themselves (mFileNameEdit → onAcceptClicked,
        // mPathEdit → onPathCommitted). Pass through normally.
        if (focusInLineEdit) {
            CosmicPopupWindow::keyPressEvent(e);
            return;
        }
        const QAbstractItemView* activeView = activeContentView();
        const bool focusInView = activeView && (fw == activeView || activeView->isAncestorOf(fw));
        if (focusInView) {
            // The view will emit activated → onActivatedInView. Don't interfere.
            CosmicPopupWindow::keyPressEvent(e);
            return;
        }
        // Focus is somewhere else (dialog, sidebar, buttons): commit if possible, else swallow.
        e->accept();
        if (mAcceptBtn && mAcceptBtn->isEnabled()) {
            onAcceptClicked();
        }
        return;
    }

    const bool ctrl = e->modifiers().testFlag(Qt::ControlModifier);
    if (ctrl && e->key() == Qt::Key_C) {
        e->accept();
        opCopy(false);
        return;
    }
    if (ctrl && e->key() == Qt::Key_X) {
        e->accept();
        opCopy(true);
        return;
    }
    if (ctrl && e->key() == Qt::Key_V) {
        e->accept();
        opPaste(currentDir());
        return;
    }
    if (e->key() == Qt::Key_F2) {
        e->accept();

        const QWidget* fw = QApplication::focusWidget();
        const bool focusInSidebar = fw && mSidebar && (fw == mSidebar || mSidebar->isAncestorOf(fw));
        if (focusInSidebar) {
            QListWidgetItem* it = mSidebar ? mSidebar->currentItem() : nullptr;
            if (it) {
                const SidebarItemType type = static_cast<SidebarItemType>(it->data(Qt::UserRole + 1).toInt());
                if (type == SidebarItemType::Bookmark) {
                    mSidebar->editItem(it);
                    return;
                }
            }
        }

        opRename();
        return;
    }
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        e->accept();
        opDelete();
        return;
    }

    if (!ctrl) {
        const QString t = e->text();
        if (isTypeSelectChar(t)) {
            const QWidget* fw = QApplication::focusWidget();
            if (!qobject_cast<const QLineEdit*>(fw)) {
                e->accept();
                mTypeSelectBuffer += t;
                if (mTypeSelectTimer) {
                    mTypeSelectTimer->start();
                }

                QAbstractItemView* view = activeContentView();
                if (!view || !view->model()) {
                    return;
                }

                const int rows = view->model()->rowCount(view->rootIndex());
                if (rows <= 0) {
                    return;
                }

                int startRow = 0;
                const QModelIndex cur = view->currentIndex();
                if (cur.isValid() && cur.parent() == view->rootIndex()) {
                    startRow = (cur.row() + 1) % rows;
                }

                const QString needle = mTypeSelectBuffer.trimmed();
                if (needle.isEmpty()) {
                    return;
                }

                auto matches = [&](int r) {
                    const QModelIndex idx = view->model()->index(r, 0, view->rootIndex());
                    const QString name = view->model()->data(idx, Qt::DisplayRole).toString();
                    return name.startsWith(needle, Qt::CaseInsensitive);
                };

                int found = -1;
                for (int i = 0; i < rows; ++i) {
                    const int r = (startRow + i) % rows;
                    if (matches(r)) {
                        found = r;
                        break;
                    }
                }

                if (found >= 0) {
                    const QModelIndex idx = view->model()->index(found, 0, view->rootIndex());
                    if (idx.isValid()) {
                        view->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
                        view->scrollTo(idx);
                    }
                }

                return;
            }
        }
    }

    CosmicPopupWindow::keyPressEvent(e);
}

void CosmicFileBrowserPopup::updateSidebar()
{
    if (!mSidebar) {
        return;
    }

    const QSignalBlocker blocker(mSidebar);
    mSidebar->clear();

    auto addSectionHeader = [&](const QString& text, bool* expanded, int* outHeaderRow) {
        auto* it = new QListWidgetItem(text);
        it->setFlags(Qt::ItemIsEnabled);  // selectable so it gets full width, but not Qt::ItemIsSelectable
        it->setData(Qt::UserRole + 10, true);           // IsSectionHeader
        it->setData(Qt::UserRole + 11, *expanded);      // SectionExpanded
        it->setData(Qt::UserRole + 12, QVariant::fromValue<void*>(expanded)); // ptr to bool
        it->setSizeHint(QSize(0, 30));
        mSidebar->addItem(it);

        if (outHeaderRow) {
            *outHeaderRow = mSidebar->row(it);
        }
    };

    const QIcon folderIcon(QStringLiteral(":/FileBrowser/Folder.svg"));

    auto addNavItem = [&](SidebarItemType type, const QString& label, const QString& path, const QIcon& icon, int bookmarkIndex = -1) {
        auto* it = new QListWidgetItem(icon, label);
        it->setData(Qt::UserRole, path);
        it->setData(Qt::UserRole + 1, static_cast<int>(type));
        it->setData(Qt::UserRole + 2, bookmarkIndex);

        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (type == SidebarItemType::Bookmark) {
            flags |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
        }
        it->setFlags(flags);
        mSidebar->addItem(it);
    };

    auto addButtonRow = [&]() {
        auto* it = new QListWidgetItem(tr("Add Bookmark"));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        it->setData(Qt::UserRole + 20, true);  // IsAddBookmarkBtn
        it->setSizeHint(QSize(0, 32));
        mSidebar->addItem(it);
        // mAddBookmarkBtn kept as nullptr; clicks handled via currentRowChanged
    };

    // Default locations (top area)
    addSectionHeader(tr("Locations"), &mLocationsExpanded, &mLocationsHeaderRow);
    mLocationsRowStart = mSidebar->count();
    addNavItem(SidebarItemType::Location, tr("Home"), QDir::homePath(), folderIcon);
    addNavItem(SidebarItemType::Location, tr("Desktop"), QDir::homePath() + QStringLiteral("/Desktop"), folderIcon);
    addNavItem(SidebarItemType::Location, tr("Documents"), QDir::homePath() + QStringLiteral("/Documents"), folderIcon);
    addNavItem(SidebarItemType::Location, tr("Downloads"), QDir::homePath() + QStringLiteral("/Downloads"), folderIcon);
    addNavItem(SidebarItemType::Location, tr("Pictures"), QDir::homePath() + QStringLiteral("/Pictures"), folderIcon);
    addNavItem(SidebarItemType::Location, tr("Music"), QDir::homePath() + QStringLiteral("/Music"), folderIcon);
    addNavItem(SidebarItemType::Location, tr("Videos"), QDir::homePath() + QStringLiteral("/Videos"), folderIcon);
    mLocationsRowEndExclusive = mSidebar->count();

    // Bookmarks area (middle)
    addSectionHeader(tr("Bookmarks"), &mBookmarksExpanded, &mBookmarksHeaderRow);
    mBookmarksRowStart = mSidebar->count();
    for (int i = 0; i < mBookmarks.size(); ++i) {
        const BookmarkEntry& bm = mBookmarks[i];
        const QString cleanedPath = QDir::cleanPath(bm.path);
        if (cleanedPath.trimmed().isEmpty()) {
            continue;
        }
        const QString label = bm.name.trimmed().isEmpty() ? defaultBookmarkNameForPath(cleanedPath) : bm.name;
        addNavItem(SidebarItemType::Bookmark, label, cleanedPath, folderIcon, i);
    }
    mBookmarksRowEndExclusive = mSidebar->count();

    addButtonRow();
    mBookmarksRowEndExclusive = mSidebar->count();

    // Devices area (bottom)
    addSectionHeader(tr("Devices"), &mDevicesExpanded, &mDevicesHeaderRow);
    mDevicesRowStart = mSidebar->count();
    addNavItem(SidebarItemType::Device, tr("Root"), QStringLiteral("/"), folderIcon);
    mDevicesRowEndExclusive = mSidebar->count();

    applySidebarSectionVisibility();

    updateSidebarActiveHighlight();
}

static void setBookmarkItemDragState(QListWidget* sidebar, bool active)
{
    if (!sidebar) return;
    for (int i = 0; i < sidebar->count(); ++i) {
        QListWidgetItem* it = sidebar->item(i);
        if (it && it->data(Qt::UserRole + 20).toBool()) {
            it->setData(Qt::UserRole + 21, active);
            const QRect r = sidebar->visualItemRect(it);
            if (sidebar->viewport() && !r.isEmpty()) {
                sidebar->viewport()->update(r);
            }
            break;
        }
    }
}

void CosmicFileBrowserPopup::updateSidebarActiveHighlight()
{
    if (!mSidebar) {
        return;
    }

    const QString activeDir = QDir::cleanPath(currentDir());
    QString bestPath;
    int bestLen = -1;
    for (int r = 0; r < mSidebar->count(); ++r) {
        QListWidgetItem* it = mSidebar->item(r);
        if (!it) {
            continue;
        }
        const QString p = QDir::cleanPath(it->data(Qt::UserRole).toString());
        if (p.trimmed().isEmpty()) {
            continue;
        }
        if (isSubPathOf(p, activeDir)) {
            const int l = p.length();
            if (l > bestLen) {
                bestLen = l;
                bestPath = p;
            }
        }
    }

    mActiveSidebarPath = bestPath;

    const QString active = QDir::cleanPath(mActiveSidebarPath);

    const QSignalBlocker blocker(mSidebar);
    for (int r = 0; r < mSidebar->count(); ++r) {
        QListWidgetItem* it = mSidebar->item(r);
        if (!it) {
            continue;
        }
        const QString p = QDir::cleanPath(it->data(Qt::UserRole).toString());
        const bool isActive = (!active.isEmpty() && !p.isEmpty() && p == active);
        if (isActive) {
            it->setBackground(QBrush(cosmicLockedAccent()));
            it->setForeground(QBrush(Qt::white));
        } else {
            it->setBackground(QBrush());
            it->setForeground(QBrush(cosmicText()));
        }
    }
    mSidebar->viewport()->update();
}

void CosmicFileBrowserPopup::applySidebarSectionVisibility()
{
    if (!mSidebar) {
        return;
    }

    auto applyRange = [&](int start, int endExclusive, bool expanded) {
        if (start < 0 || endExclusive <= start) {
            return;
        }
        const int end = qMin(endExclusive, mSidebar->count());
        for (int r = start; r < end; ++r) {
            if (QListWidgetItem* it = mSidebar->item(r)) {
                it->setHidden(!expanded);
            }
        }
    };

    applyRange(mLocationsRowStart, mLocationsRowEndExclusive, mLocationsExpanded);
    applyRange(mBookmarksRowStart, mBookmarksRowEndExclusive, mBookmarksExpanded);
    applyRange(mDevicesRowStart, mDevicesRowEndExclusive, mDevicesExpanded);
}

static QVector<CosmicFileBrowserPopup::BookmarkEntry> rebuildBookmarksFromSidebarOrder(
    const QListWidget* sidebar,
    int rowStart,
    int rowEndExclusive,
    const QVector<CosmicFileBrowserPopup::BookmarkEntry>& current)
{
    if (!sidebar || rowStart < 0 || rowEndExclusive <= rowStart) {
        return current;
    }

    QHash<QString, CosmicFileBrowserPopup::BookmarkEntry> byPath;
    byPath.reserve(current.size());
    for (const auto& e : current) {
        byPath.insert(QDir::cleanPath(e.path), e);
    }

    QVector<CosmicFileBrowserPopup::BookmarkEntry> out;
    out.reserve(rowEndExclusive - rowStart);
    for (int r = rowStart; r < rowEndExclusive; ++r) {
        const QListWidgetItem* it = sidebar->item(r);
        if (!it) {
            continue;
        }
        const QString p = QDir::cleanPath(it->data(Qt::UserRole).toString());
        if (p.trimmed().isEmpty()) {
            continue;
        }

        if (byPath.contains(p)) {
            out.push_back(byPath.value(p));
        } else {
            CosmicFileBrowserPopup::BookmarkEntry e;
            e.path = p;
            e.name = defaultBookmarkNameForPath(p);
            out.push_back(e);
        }
    }
    return out;
}

void CosmicFileBrowserPopup::onAddBookmarkClicked()
{
    const QString dir = currentDir();
    if (dir.isEmpty()) {
        return;
    }

    const QString cleaned = QDir::cleanPath(dir);
    if (indexOfBookmarkPath(cleaned) < 0) {
        BookmarkEntry e;
        e.path = cleaned;
        e.name = defaultBookmarkNameForPath(cleaned);
        mBookmarks.push_back(e);
        saveBookmarks();
    }

    updateSidebar();
}

void CosmicFileBrowserPopup::onSidebarItemActivated(QListWidgetItem* item)
{
    if (!item) {
        return;
    }
    // Section headers handle their own toggle — don't navigate
    if (item->data(Qt::UserRole + 10).toBool()) {
        return;
    }
    const QString p = item->data(Qt::UserRole).toString();
    if (p.trimmed().isEmpty()) {
        return;
    }
    setCurrentDir(p, true);
}

void CosmicFileBrowserPopup::onSidebarContextMenuRequested(const QPoint& pos)
{
    if (!mSidebar) {
        return;
    }

    QListWidgetItem* item = mSidebar->itemAt(pos);
    if (!item) {
        return;
    }

    const SidebarItemType type = static_cast<SidebarItemType>(item->data(Qt::UserRole + 1).toInt());
    if (type != SidebarItemType::Bookmark) {
        return;
    }

    const int bmIndex = item->data(Qt::UserRole + 2).toInt();
    if (bmIndex < 0 || bmIndex >= mBookmarks.size()) {
        return;
    }

    CosmicPopupContextMenu menu(this);
    menu.addItem(tr("Rename"), QStringLiteral("rename"));
    menu.addItem(tr("Remove Bookmark"), QStringLiteral("remove"), true, QIcon(QStringLiteral(":/Interaction/trash.svg")));

    const QString chosen = menu.execAt(mSidebar->viewport()->mapToGlobal(pos));
    if (chosen == QStringLiteral("rename")) {
        mSidebar->setCurrentItem(item);
        mSidebar->editItem(item);
        return;
    }
    if (chosen == QStringLiteral("remove")) {
        removeBookmarkAt(bmIndex);
        return;
    }
}

void CosmicFileBrowserPopup::onSidebarItemChanged(QListWidgetItem* item)
{
    if (!item) {
        return;
    }
    const SidebarItemType type = static_cast<SidebarItemType>(item->data(Qt::UserRole + 1).toInt());
    if (type != SidebarItemType::Bookmark) {
        return;
    }

    const int bmIndex = item->data(Qt::UserRole + 2).toInt();
    if (bmIndex < 0 || bmIndex >= mBookmarks.size()) {
        return;
    }

    const QString newName = item->text().trimmed();
    const QString safeName = newName.isEmpty() ? defaultBookmarkNameForPath(mBookmarks[bmIndex].path) : newName;
    if (mBookmarks[bmIndex].name == safeName) {
        return;
    }

    mBookmarks[bmIndex].name = safeName;
    saveBookmarks();
    updateSidebar();
}

int CosmicFileBrowserPopup::indexOfBookmarkPath(const QString& path) const
{
    const QString cleaned = QDir::cleanPath(path);
    for (int i = 0; i < mBookmarks.size(); ++i) {
        if (QDir::cleanPath(mBookmarks[i].path) == cleaned) {
            return i;
        }
    }
    return -1;
}

void CosmicFileBrowserPopup::removeBookmarkAt(int index)
{
    if (index < 0 || index >= mBookmarks.size()) {
        return;
    }
    mBookmarks.removeAt(index);
    saveBookmarks();
    updateSidebar();
}

void CosmicFileBrowserPopup::loadUiSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(uiSettingsGroupKeyForMode());

    const QByteArray g = settings.value(QStringLiteral("Geometry")).toByteArray();
    if (!g.isEmpty()) {
        restoreGeometry(g);

        // Defensive: if a stored geometry places the popup off-screen, move it back.
        // This is important because otherwise the user experiences "no dialog opened".
        const QRect geom = frameGeometry();
        QScreen* screen = QGuiApplication::screenAt(geom.center());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen) {
            const QRect avail = screen->availableGeometry();
            if (!avail.intersects(geom)) {
                move(avail.center() - QPoint(width() / 2, height() / 2));
            }
        }
    }

    const int zoomGrid = settings.value(QStringLiteral("ZoomGridPx"), mZoomGridPx).toInt();
    const int zoomList = settings.value(QStringLiteral("ZoomListPx"), mZoomListPx).toInt();
    mZoomGridPx = qBound(minZoomPxForViewMode(ViewMode::Grid), zoomGrid, maxZoomPxForViewMode(ViewMode::Grid));
    mZoomListPx = qBound(minZoomPxForViewMode(ViewMode::List), zoomList, maxZoomPxForViewMode(ViewMode::List));

    const int viewMode = settings.value(QStringLiteral("ViewMode"), static_cast<int>(mViewMode)).toInt();
    mViewMode = (viewMode == static_cast<int>(ViewMode::Grid)) ? ViewMode::Grid : ViewMode::List;
    mZoomMasterPx = activeZoomPx();

    mShowHidden = settings.value(showHiddenKey(), mShowHidden).toBool();
    if (mShowHiddenBtn) {
        mShowHiddenBtn->setChecked(mShowHidden);
    }
    applyFsModelFilter();

    if (mSplitter) {
        const QByteArray s = settings.value(QStringLiteral("Splitter")).toByteArray();
        if (!s.isEmpty()) {
            mSplitter->restoreState(s);
        }
    }

    if (mTree && mTree->header()) {
        const QByteArray hs = settings.value(headerStateKey()).toByteArray();
        if (!hs.isEmpty()) {
            mTree->header()->restoreState(hs);
        }
    }

    mLocationsExpanded = settings.value(locationsExpandedKey(), mLocationsExpanded).toBool();
    mBookmarksExpanded = settings.value(bookmarksExpandedKey(), mBookmarksExpanded).toBool();
    mDevicesExpanded = settings.value(devicesExpandedKey(), mDevicesExpanded).toBool();

    settings.endGroup();
}

void CosmicFileBrowserPopup::saveUiSettings() const
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(uiSettingsGroupKeyForMode());
    settings.setValue(QStringLiteral("Geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ZoomGridPx"), mZoomGridPx);
    settings.setValue(QStringLiteral("ZoomListPx"), mZoomListPx);
    settings.setValue(QStringLiteral("ViewMode"), static_cast<int>(mViewMode));
    settings.setValue(showHiddenKey(), mShowHidden);
    if (mSplitter) {
        settings.setValue(QStringLiteral("Splitter"), mSplitter->saveState());
    }

    if (mTree && mTree->header()) {
        settings.setValue(headerStateKey(), mTree->header()->saveState());
    }

    settings.setValue(locationsExpandedKey(), mLocationsExpanded);
    settings.setValue(bookmarksExpandedKey(), mBookmarksExpanded);
    settings.setValue(devicesExpandedKey(), mDevicesExpanded);
    settings.endGroup();
}

void CosmicFileBrowserPopup::loadBookmarks()
{
    mBookmarks.clear();

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(bookmarksSettingsGroupKey());

    const QVariantList list = settings.value(QStringLiteral("Items")).toList();
    if (!list.isEmpty()) {
        for (const QVariant& v : list) {
            const QVariantMap m = v.toMap();
            const QString path = QDir::cleanPath(m.value(QStringLiteral("path")).toString());
            if (path.trimmed().isEmpty()) {
                continue;
            }
            BookmarkEntry e;
            e.path = path;
            e.name = m.value(QStringLiteral("name")).toString();
            if (e.name.trimmed().isEmpty()) {
                e.name = defaultBookmarkNameForPath(path);
            }
            mBookmarks.push_back(e);
        }
        settings.endGroup();
        return;
    }

    // Back-compat: earlier versions stored a plain QStringList.
    const QStringList legacy = settings.value(QStringLiteral("Paths")).toStringList();
    for (const QString& p : legacy) {
        const QString path = QDir::cleanPath(p);
        if (path.trimmed().isEmpty()) {
            continue;
        }
        BookmarkEntry e;
        e.path = path;
        e.name = defaultBookmarkNameForPath(path);
        mBookmarks.push_back(e);
    }

    settings.endGroup();
}

void CosmicFileBrowserPopup::saveBookmarks() const
{
    QVariantList list;
    list.reserve(mBookmarks.size());
    for (const BookmarkEntry& e : mBookmarks) {
        if (e.path.trimmed().isEmpty()) {
            continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("path"), QDir::cleanPath(e.path));
        m.insert(QStringLiteral("name"), e.name);
        list.push_back(m);
    }

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(bookmarksSettingsGroupKey());
    settings.setValue(QStringLiteral("Items"), list);
    settings.endGroup();
}

QFileInfo CosmicFileBrowserPopup::fileInfoForViewIndex(QObject* viewObj, const QModelIndex& index) const
{
    if (!mFsModel || !index.isValid() || !viewObj) {
        return QFileInfo();
    }

    auto* view = qobject_cast<QAbstractItemView*>(viewObj);
    if (!view || !view->model()) {
        return QFileInfo();
    }

    QModelIndex srcIndex = index;
    if (auto* proxy = qobject_cast<QAbstractProxyModel*>(view->model())) {
        srcIndex = proxy->mapToSource(index);
    }

    return mFsModel->fileInfo(srcIndex);
}

void CosmicFileBrowserPopup::onContentContextMenuRequested(const QPoint& pos)
{
    QObject* viewObj = sender();
    auto* view = qobject_cast<QAbstractItemView*>(viewObj);
    if (!view) {
        return;
    }

    QModelIndex idx = view->indexAt(pos);
    const bool clickedOnItem = idx.isValid();
    if (idx.isValid() && idx.column() != 0) {
        idx = idx.sibling(idx.row(), 0);
    }

    // Blender-like behavior: right-click selects the item under the cursor.
    if (clickedOnItem && view->selectionModel() && !view->selectionModel()->isSelected(idx)) {
        view->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    }

    // Use the same selection path as F2 so rename/copy/cut/delete behave consistently.
    const QStringList selectedPaths = selectedPathsInActiveView();

    const bool hasSelection = !selectedPaths.isEmpty();

    CosmicPopupContextMenu menu(this);
    menu.addItem(tr("Rename"), QStringLiteral("rename"), selectedPaths.size() == 1);
    menu.addItem(tr("Delete"), QStringLiteral("delete"), hasSelection, QIcon(QStringLiteral(":/Interaction/trash.svg")));

    const QString chosen = menu.execAt(view->viewport()->mapToGlobal(pos));
    if (chosen.isEmpty()) {
        return;
    }

    if (chosen == QStringLiteral("rename")) {
        opRename();
        return;
    }
    if (chosen == QStringLiteral("delete")) {
        opDelete();
        return;
    }
}

bool CosmicFileBrowserPopup::eventFilter(QObject* watched, QEvent* event)
{
    // Detect drag start/end on content viewports → show bookmark drop zone
    const bool isContentViewport = (mTree && watched == mTree->viewport())
                                || (mGrid && watched == mGrid->viewport());
    if (isContentViewport) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me && (me->buttons() & Qt::LeftButton) && !mBookmarkDragActive) {
                // A drag is in progress from the content area
                mBookmarkDragActive = true;
                setBookmarkItemDragState(mSidebar, true);
            }
        } else if (event->type() == QEvent::MouseButtonRelease
                || event->type() == QEvent::Drop) {
            if (mBookmarkDragActive) {
                mBookmarkDragActive = false;
                setBookmarkItemDragState(mSidebar, false);
            }
        }
    }

    // Single-click on path label → switch to inline edit mode
    if (watched == mPathLabelWidget.data() && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me && me->button() == Qt::LeftButton) {
            if (mPathStack) {
                mPathStack->setCurrentIndex(1);
                if (mPathEdit) {
                    mPathEdit->setFocus();
                    mPathEdit->selectAll();
                }
            }
            return true;
        }
    }

    if (watched == (mSidebar ? mSidebar->viewport() : nullptr)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (!mSidebar || !me || me->button() != Qt::LeftButton) {
                return CosmicPopupWindow::eventFilter(watched, event);
            }

            const int row = mSidebar->indexAt(me->pos()).row();
            const bool within = (row >= mBookmarksRowStart && row < mBookmarksRowEndExclusive);
            if (!within) {
                mSidebarIsDragging = false;
                mSidebarDraggedItem = nullptr;
                mSidebarDragStartRow = -1;
                mSidebarDropRow = -1;
                mSidebarPlaceholder = nullptr;
                return CosmicPopupWindow::eventFilter(watched, event);
            }

            mSidebarPressPos = me->pos();
            mSidebarIsDragging = false;
            mSidebarDraggedItem = mSidebar->item(row);
            mSidebarDragStartRow = row;
            mSidebarDropRow = row;

            if (mSidebarDraggedItem) {
                mSidebar->setCurrentItem(mSidebarDraggedItem);
            }
            return CosmicPopupWindow::eventFilter(watched, event);
        }

        if (event->type() == QEvent::DragEnter) {
            // Accept external folder drags (from content area) for the bookmark drop zone
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de && de->mimeData() && de->mimeData()->hasUrls() && mSidebarDragStartRow < 0) {
                de->setDropAction(Qt::CopyAction);
                de->accept();
                mBookmarkDragActive = true;
                setBookmarkItemDragState(mSidebar, true);
                return true;
            }
            if (mSidebarDragStartRow < mBookmarksRowStart || mSidebarDragStartRow >= mBookmarksRowEndExclusive) {
                event->ignore();
                return true;
            }
            event->accept();
            return true;
        }

        if (event->type() == QEvent::DragMove) {
            // Accept external folder drags hovering over the sidebar
            auto* de = static_cast<QDragMoveEvent*>(event);
            if (de && de->mimeData() && de->mimeData()->hasUrls() && mSidebarDragStartRow < 0) {
                de->setDropAction(Qt::CopyAction);
                de->accept();
                return true;
            }

            if (mSidebarDragStartRow < mBookmarksRowStart || mSidebarDragStartRow >= mBookmarksRowEndExclusive) {
                event->ignore();
                return true;
            }

            if (!de) {
                return true;
            }

            int row = mSidebar->indexAt(de->pos()).row();
            if (row < 0) {
                row = (de->pos().y() < 0) ? mBookmarksRowStart : (mBookmarksRowEndExclusive - 1);
            }

            if (row < mBookmarksRowStart || row >= mBookmarksRowEndExclusive) {
                de->ignore();
                return true;
            }

            de->accept();
            return CosmicPopupWindow::eventFilter(watched, event);
        }

        if (event->type() == QEvent::Drop) {
            // External folder drop onto the bookmark button item
            auto* dd = static_cast<QDropEvent*>(event);
            if (dd && dd->mimeData() && dd->mimeData()->hasUrls() && mSidebarDragStartRow < 0) {
                mBookmarkDragActive = false;
                setBookmarkItemDragState(mSidebar, false);
                bool addedAny = false;
                for (const QUrl& url : dd->mimeData()->urls()) {
                    const QString p = url.toLocalFile();
                    if (p.isEmpty()) continue;
                    QFileInfo fi(p);
                    if (!fi.exists() || !fi.isDir()) continue;
                    const QString cleaned = QDir::cleanPath(fi.absoluteFilePath());
                    if (indexOfBookmarkPath(cleaned) < 0) {
                        BookmarkEntry e;
                        e.path = cleaned;
                        e.name = defaultBookmarkNameForPath(cleaned);
                        mBookmarks.push_back(e);
                        addedAny = true;
                    }
                }
                if (addedAny) {
                    saveBookmarks();
                    updateSidebar();
                }
                dd->setDropAction(Qt::CopyAction);
                dd->accept();
                return true;
            }

            if (mSidebarDragStartRow < mBookmarksRowStart || mSidebarDragStartRow >= mBookmarksRowEndExclusive) {
                event->ignore();
                return true;
            }

            auto* de = static_cast<QDropEvent*>(event);
            if (de) {
                int row = mSidebar->indexAt(de->pos()).row();
                if (row < 0) {
                    row = (de->pos().y() < 0) ? mBookmarksRowStart : (mBookmarksRowEndExclusive - 1);
                }
                if (row < mBookmarksRowStart || row >= mBookmarksRowEndExclusive) {
                    de->ignore();
                    return true;
                }
            }

            const bool handled = CosmicPopupWindow::eventFilter(watched, event);
            QTimer::singleShot(0, this, [this]() {
                if (!mSidebar) {
                    return;
                }
                mBookmarks = rebuildBookmarksFromSidebarOrder(mSidebar, mBookmarksRowStart, mBookmarksRowEndExclusive, mBookmarks);
                saveBookmarks();
                updateSidebar();
            });
            mSidebarDragStartRow = -1;
            mSidebarDropRow = -1;
            return handled;
        }

        if (event->type() == QEvent::DragLeave) {
            if (mBookmarkDragActive) {
                mBookmarkDragActive = false;
                setBookmarkItemDragState(mSidebar, false);
            }
            mSidebarDragStartRow = -1;
            mSidebarDropRow = -1;
            return CosmicPopupWindow::eventFilter(watched, event);
        }
    }

    if ((watched == (mTree ? mTree->viewport() : nullptr) || watched == (mGrid ? mGrid->viewport() : nullptr)) && event && event->type() == QEvent::Drop) {
        auto* dd = static_cast<QDropEvent*>(event);
        if (!dd || !dd->mimeData() || !dd->mimeData()->hasUrls() || dd->mimeData()->urls().isEmpty()) {
            return CosmicPopupWindow::eventFilter(watched, event);
        }

        QAbstractItemView* view = (watched == (mTree ? mTree->viewport() : nullptr)) ? static_cast<QAbstractItemView*>(mTree) : static_cast<QAbstractItemView*>(mGrid);
        if (!view) {
            return CosmicPopupWindow::eventFilter(watched, event);
        }

        // Determine paste target: drop on folder => paste into it.
        QString target = QDir::cleanPath(currentDir());
        const QPoint viewPos = view->viewport()->mapFromGlobal(dd->pos());
        const QModelIndex idx = view->indexAt(viewPos);
        if (idx.isValid()) {
            const QFileInfo fi = fileInfoForViewIndex(view, idx);
            if (fi.exists() && fi.isDir()) {
                target = QDir::cleanPath(fi.absoluteFilePath());
            }
        }

        // If the drag originates from our own view, treat MoveAction as cut.
        const bool fromSelf = (dd->source() == mTree || dd->source() == mGrid);
        const bool isMove = fromSelf && (dd->dropAction() == Qt::MoveAction);

        auto* mime = new QMimeData();
        mime->setUrls(dd->mimeData()->urls());
        mime->setData(kCutMimeType(), isMove ? QByteArray("1") : QByteArray("0"));
        QApplication::clipboard()->setMimeData(mime);

        opPaste(target);
        dd->acceptProposedAction();
        return true;
    }

    if ((watched == (mTree ? mTree->viewport() : nullptr) || watched == (mGrid ? mGrid->viewport() : nullptr)) && event && event->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(event);
        if (we->modifiers().testFlag(Qt::ControlModifier)) {
            const int delta = we->angleDelta().y();
            if (delta != 0) {
                const int step = (delta > 0) ? 8 : -8;
                const int cur = activeZoomPx();
                const int newPx = qBound(minZoomPxForViewMode(mViewMode), cur + step, maxZoomPxForViewMode(mViewMode));
                if (newPx != cur) {
                    setActiveZoomPx(newPx);
                    applyZoom();
                }
                updateZoomButtonsEnabled();
                we->accept();
                return true;
            }
        }
    }

    if (watched == mAddBookmarkBtn) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* de = static_cast<QDragMoveEvent*>(event);
            const QMimeData* md = de->mimeData();
            if (md && md->hasUrls()) {
                for (const QUrl& url : md->urls()) {
                    const QString p = url.toLocalFile();
                    if (p.isEmpty()) {
                        continue;
                    }
                    QFileInfo fi(p);
                    if (fi.exists() && fi.isDir()) {
                        setAddBookmarkDragHover(mAddBookmarkBtn, true);
                        de->acceptProposedAction();
                        return true;
                    }
                }
            }
            setAddBookmarkDragHover(mAddBookmarkBtn, false);
            return true;
        }

        if (event->type() == QEvent::Drop) {
            auto* dd = static_cast<QDropEvent*>(event);
            const QMimeData* md = dd->mimeData();
            bool addedAny = false;
            if (md && md->hasUrls()) {
                for (const QUrl& url : md->urls()) {
                    const QString p = url.toLocalFile();
                    if (p.isEmpty()) {
                        continue;
                    }
                    QFileInfo fi(p);
                    if (!fi.exists() || !fi.isDir()) {
                        continue;
                    }

                    const QString cleaned = QDir::cleanPath(fi.absoluteFilePath());
                    if (indexOfBookmarkPath(cleaned) < 0) {
                        BookmarkEntry e;
                        e.path = cleaned;
                        e.name = defaultBookmarkNameForPath(cleaned);
                        mBookmarks.push_back(e);
                        addedAny = true;
                    }
                }
            }

            setAddBookmarkDragHover(mAddBookmarkBtn, false);

            if (addedAny) {
                saveBookmarks();
                updateSidebar();
                dd->acceptProposedAction();
            }
            return true;
        }

        if (event->type() == QEvent::DragLeave) {
            setAddBookmarkDragHover(mAddBookmarkBtn, false);
            return true;
        }
    }

    return CosmicPopupWindow::eventFilter(watched, event);
}

QString CosmicFileBrowserPopup::currentDir() const
{
    return mFsModel ? mFsModel->rootPath() : QString();
}

void CosmicFileBrowserPopup::setCurrentDir(const QString& dirPath, bool pushHistory)
{
    const QString cleaned = QDir::cleanPath(dirPath);
    QFileInfo fi(cleaned);

    QString target = cleaned;
    if (!fi.exists()) {
        return;
    }
    if (fi.isFile()) {
        target = fi.absolutePath();
    }

    if (pushHistory) {
        const QString cur = currentDir();
        if (!cur.isEmpty() && cur != target) {
            mBackStack.push_back(cur);
            mForwardStack.clear();
        }
    }

    const bool isRefresh = (!pushHistory && target == currentDir());
    const QStringList keepSelection = isRefresh ? selectedPathsInActiveView() : QStringList();

    // Clear search on navigation so the user doesn't end up "stuck" filtering in a new folder.
    if (mSearchEdit && !mSearchEdit->text().isEmpty()) {
        mSearchEdit->clear();
        if (auto* proxy = dynamic_cast<FileNameFilterProxy*>(mProxyModel.data())) {
            proxy->setNameSubstringFilter(QString());
        }
    }

    if (isRefresh && mFsModel) {
        const QString refreshDir = target;
        QMetaObject::Connection* conn = new QMetaObject::Connection();
        *conn = connect(mFsModel, &QFileSystemModel::directoryLoaded, this, [this, refreshDir, keepSelection, conn](const QString& loaded) {
            if (QDir::cleanPath(loaded) != QDir::cleanPath(refreshDir)) {
                return;
            }
            disconnect(*conn);
            delete conn;

            if (keepSelection.isEmpty()) {
                return;
            }

            QAbstractItemView* view = activeContentView();
            if (!view || !view->model() || !view->selectionModel()) {
                return;
            }

            QItemSelectionModel* sm = view->selectionModel();
            sm->clearSelection();

            QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(view->model());
            for (const QString& p : keepSelection) {
                if (p.isEmpty()) {
                    continue;
                }
                const QModelIndex src = mFsModel->index(p);
                if (!src.isValid()) {
                    continue;
                }
                QModelIndex idx = src;
                if (proxy) {
                    idx = proxy->mapFromSource(src);
                }
                if (!idx.isValid() || idx.column() != 0) {
                    continue;
                }
                sm->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        });
    }

    mFsModel->setRootPath(target);

    mActiveSidebarPath = target;
    updateSidebarActiveHighlight();

    QModelIndex srcRoot = mFsModel->index(target);
    QModelIndex proxyRoot = srcRoot;
    if (mTree && mTree->model()) {
        if (auto* proxy = qobject_cast<QAbstractProxyModel*>(mTree->model())) {
            proxyRoot = proxy->mapFromSource(srcRoot);
        }
    }

    if (mTree) {
        mTree->setRootIndex(proxyRoot);
    }
    if (mGrid) {
        mGrid->setRootIndex(proxyRoot);
    }

    scheduleVisibleThumbnailPrefetch();

    mPathEdit->setText(target);
    rebuildBreadcrumb();
    updatePathLabel();

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(lastDirKey(), target);

    updateAcceptEnabled();

    if (mBackBtn) {
        mBackBtn->setEnabled(!mBackStack.isEmpty());
    }
    if (mForwardBtn) {
        mForwardBtn->setEnabled(!mForwardStack.isEmpty());
    }
}

void CosmicFileBrowserPopup::rebuildBreadcrumb()
{
    if (!mBreadcrumb) {
        return;
    }

    auto* lay = qobject_cast<QHBoxLayout*>(mBreadcrumb->layout());
    if (!lay) {
        return;
    }

    while (QLayoutItem* item = lay->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QString p = mPathEdit ? mPathEdit->text().trimmed() : QString();
    if (p.isEmpty()) {
        return;
    }

    QStringList parts;
    QString accum;
    if (p.startsWith('/')) {
        parts = p.split('/', Qt::SkipEmptyParts);
        accum = QStringLiteral("/");

        CosmicButton* rootBtn = new CosmicButton(QStringLiteral("/"), mBreadcrumb);
        rootBtn->setBordered(false);
        connect(rootBtn, &QPushButton::clicked, this, [this]() { setCurrentDir(QStringLiteral("/"), true); });
        lay->addWidget(rootBtn);
    } else {
        parts = p.split(QDir::separator(), Qt::SkipEmptyParts);
    }

    for (int i = 0; i < parts.size(); ++i) {
        const QString part = parts[i];
        if (accum.endsWith('/') || accum.isEmpty()) {
            accum += part;
        } else {
            accum += QStringLiteral("/") + part;
        }

        QLabel* sep = new QLabel(QStringLiteral(">"), mBreadcrumb);
        sep->setStyleSheet(QStringLiteral("QLabel{ color:#808080; padding:0px 2px; }"));
        lay->addWidget(sep);

        CosmicButton* btn = new CosmicButton(part, mBreadcrumb);
        btn->setBordered(false);
        connect(btn, &QPushButton::clicked, this, [this, accum]() { setCurrentDir(accum, true); });
        lay->addWidget(btn);
    }

    lay->addStretch(1);
}

void CosmicFileBrowserPopup::onNavigateUp()
{
    const QString cur = currentDir();
    if (cur.isEmpty()) {
        return;
    }
    QDir d(cur);
    if (d.cdUp()) {
        setCurrentDir(d.absolutePath(), true);
    }
}

void CosmicFileBrowserPopup::onNavigateBack()
{
    if (mBackStack.isEmpty()) {
        return;
    }
    const QString cur = currentDir();
    const QString prev = mBackStack.takeLast();
    if (!cur.isEmpty()) {
        mForwardStack.push_back(cur);
    }
    setCurrentDir(prev, false);

    if (mBackBtn) {
        mBackBtn->setEnabled(!mBackStack.isEmpty());
    }
    if (mForwardBtn) {
        mForwardBtn->setEnabled(!mForwardStack.isEmpty());
    }
}

void CosmicFileBrowserPopup::onNavigateForward()
{
    if (mForwardStack.isEmpty()) {
        return;
    }
    const QString cur = currentDir();
    const QString next = mForwardStack.takeLast();
    if (!cur.isEmpty()) {
        mBackStack.push_back(cur);
    }
    setCurrentDir(next, false);

    if (mBackBtn) {
        mBackBtn->setEnabled(!mBackStack.isEmpty());
    }
    if (mForwardBtn) {
        mForwardBtn->setEnabled(!mForwardStack.isEmpty());
    }
}

void CosmicFileBrowserPopup::onPathCommitted()
{
    const QString p = mPathEdit ? mPathEdit->text().trimmed() : QString();
    if (p.isEmpty()) {
        return;
    }
    setCurrentDir(p, true);
}

void CosmicFileBrowserPopup::onSearchChanged(const QString& text)
{
    if (!mTree || !mTree->model()) {
        return;
    }

    auto* proxy = dynamic_cast<FileNameFilterProxy*>(mTree->model());
    if (!proxy) {
        return;
    }

    proxy->setNameSubstringFilter(text);

    if (!mTree || !mGrid) {
        return;
    }

    // After the filter changes the proxy remaps its indices, so the root index
    // the views were pointing at may have shifted or become invalid.
    // Re-anchor both views to the current directory every time the filter changes.
    if (!mActiveSidebarPath.isEmpty() && mFsModel) {
        const QModelIndex srcRoot = mFsModel->index(mActiveSidebarPath);
        QModelIndex proxyRoot = srcRoot;
        if (auto* p = qobject_cast<QAbstractProxyModel*>(mTree->model())) {
            proxyRoot = p->mapFromSource(srcRoot);
        }
        mTree->setRootIndex(proxyRoot);
        if (mGrid) {
            QModelIndex gridProxyRoot = srcRoot;
            if (auto* p = qobject_cast<QAbstractProxyModel*>(mGrid->model())) {
                gridProxyRoot = p->mapFromSource(srcRoot);
            }
            mGrid->setRootIndex(gridProxyRoot);
        }
    }

    // Focus first visible match so incremental search feels "active".
    if (text.trimmed().isEmpty()) {
        return;
    }

    QAbstractItemView* view = activeContentView();
    if (!view || !view->model()) {
        return;
    }

    const int rows = view->model()->rowCount(view->rootIndex());
    for (int r = 0; r < rows; ++r) {
        const QModelIndex idx = view->model()->index(r, 0, view->rootIndex());
        if (idx.isValid()) {
            view->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
            view->scrollTo(idx);
            break;
        }
    }
}

void CosmicFileBrowserPopup::onActivatedInView(const QModelIndex& index)
{
    if (!index.isValid() || !mFsModel) {
        return;
    }

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(sender());
    if (!view || !view->model()) {
        return;
    }

    QModelIndex srcIndex = index;
    if (auto* proxy = qobject_cast<QAbstractProxyModel*>(view->model())) {
        srcIndex = proxy->mapToSource(index);
    }

    QFileInfo fi = mFsModel->fileInfo(srcIndex);
    if (fi.isDir()) {
        setCurrentDir(fi.absoluteFilePath(), true);
        return;
    }

    if (mMode == Mode::OpenFile || mMode == Mode::OpenFiles) {
        onAcceptClicked();
    } else if (mMode == Mode::SaveFile) {
        // Double-clicking a file in Save mode populates the filename field.
        if (mFileNameEdit) {
            mFileNameEdit->setText(fi.fileName());
        }
    }
}

void CosmicFileBrowserPopup::onSelectionChanged()
{
    updateAcceptEnabled();
}

QString CosmicFileBrowserPopup::selectedPathForOpenSingle() const
{
    QAbstractItemView* v = (mViewMode == ViewMode::List) ? static_cast<QAbstractItemView*>(mTree) : static_cast<QAbstractItemView*>(mGrid);
    if (!v || !v->selectionModel()) {
        return QString();
    }

    // QListView (IconMode) doesn't reliably report selectedRows(); use selectedIndexes()
    // normalized to column 0, consistent with selectedPathsInActiveView().
    const QModelIndexList all = v->selectionModel()->selectedIndexes();
    QModelIndex idx;
    for (const QModelIndex& i : all) {
        if (i.column() == 0) { idx = i; break; }
    }
    if (!idx.isValid()) {
        return QString();
    }

    QModelIndex srcIndex = idx;
    if (auto* proxy = qobject_cast<QAbstractProxyModel*>(v->model())) {
        srcIndex = proxy->mapToSource(idx);
    }

    const QFileInfo fi = mFsModel->fileInfo(srcIndex);
    if (!fi.exists() || fi.isDir()) {
        return QString();
    }
    return fi.absoluteFilePath();
}

QStringList CosmicFileBrowserPopup::selectedPathsForOpenMulti() const
{
    QStringList out;
    QAbstractItemView* v = (mViewMode == ViewMode::List) ? static_cast<QAbstractItemView*>(mTree) : static_cast<QAbstractItemView*>(mGrid);
    if (!v || !v->selectionModel()) {
        return out;
    }

    // QListView (IconMode) doesn't reliably report selectedRows(); use selectedIndexes()
    // normalized to column 0, consistent with selectedPathsInActiveView().
    const QModelIndexList all = v->selectionModel()->selectedIndexes();
    for (QModelIndex idx : all) {
        if (idx.column() != 0) {
            idx = idx.sibling(idx.row(), 0);
        }
        if (!idx.isValid()) {
            continue;
        }
        QModelIndex srcIndex = idx;
        if (auto* proxy = qobject_cast<QAbstractProxyModel*>(v->model())) {
            srcIndex = proxy->mapToSource(idx);
        }
        const QFileInfo fi = mFsModel->fileInfo(srcIndex);
        if (fi.exists() && fi.isFile()) {
            out.push_back(fi.absoluteFilePath());
        }
    }

    out.removeDuplicates();
    return out;
}

QString CosmicFileBrowserPopup::selectedPathForSave() const
{
    const QString dir = currentDir();
    if (dir.isEmpty()) {
        return QString();
    }

    QString name = mFileNameEdit ? mFileNameEdit->text().trimmed() : QString();
    if (name.isEmpty()) {
        const QString fromSel = selectedPathForOpenSingle();
        if (!fromSel.isEmpty()) {
            name = QFileInfo(fromSel).fileName();
        }
    }

    if (name.isEmpty()) {
        return QString();
    }

    QString full = QDir(dir).absoluteFilePath(name);

    if (!mDefaultSuffix.isEmpty()) {
        if (!full.endsWith(mDefaultSuffix, Qt::CaseInsensitive)) {
            full += mDefaultSuffix;
        }
    }

    return full;
}

void CosmicFileBrowserPopup::updateAcceptEnabled()
{
    if (!mAcceptBtn) {
        return;
    }

    bool enabled = false;
    if (mFolderMode) {
        // In folder mode, enable as long as we have a valid current directory
        enabled = !currentDir().isEmpty() && QDir(currentDir()).exists();
    } else if (mMode == Mode::SaveFile) {
        enabled = !selectedPathForSave().isEmpty();
    } else if (mMode == Mode::OpenFiles) {
        enabled = !selectedPathsForOpenMulti().isEmpty();
    } else {
        enabled = !selectedPathForOpenSingle().isEmpty();
    }

    mAcceptBtn->setEnabled(enabled);
}

void CosmicFileBrowserPopup::onAcceptClicked()
{
    // Folder mode: return the current directory
    if (mFolderMode) {
        const QString dir = currentDir();
        if (dir.isEmpty() || !QDir(dir).exists()) return;
        mResult.accepted = true;
        mResult.selectedPaths = QStringList() << dir;
        accept();
        return;
    }

    if (mMode == Mode::SaveFile) {
        const QString p = selectedPathForSave();
        if (p.isEmpty()) {
            return;
        }

        if (QFileInfo::exists(p)) {
            const auto btn = QMessageBox::question(this, tr("Overwrite"), tr("File already exists. Overwrite?"), QMessageBox::Yes | QMessageBox::No);
            if (btn != QMessageBox::Yes) {
                return;
            }
        }

        mResult.accepted = true;
        mResult.selectedPaths = (QStringList() << p);
        accept();
        return;
    }

    if (mMode == Mode::OpenFiles) {
        const QStringList p = selectedPathsForOpenMulti();
        if (p.isEmpty()) {
            return;
        }
        mResult.accepted = true;
        mResult.selectedPaths = p;
        accept();
        return;
    }

    const QString p = selectedPathForOpenSingle();
    if (p.isEmpty()) {
        return;
    }

    mResult.accepted = true;
    mResult.selectedPaths = (QStringList() << p);
    accept();
}

void CosmicFileBrowserPopup::onCancelClicked()
{
    mResult.accepted = false;
    mResult.selectedPaths.clear();
    reject();
}

void CosmicFileBrowserPopup::onViewModeList()
{
    mViewMode = ViewMode::List;
    mListBtn->setChecked(true);
    mGridBtn->setChecked(false);
    mViews->setCurrentWidget(mTree);
    mZoomMasterPx = activeZoomPx();
    applyZoom();
    updateZoomButtonsEnabled();
    scheduleVisibleThumbnailPrefetch();
    updateAcceptEnabled();
    mTree->setFocus(Qt::OtherFocusReason);
    if (mViewModeDropBtn) {
        mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/ListView.svg")));
        mViewModeDropBtn->setToolTip(tr("Switch to grid view"));
    }
}

void CosmicFileBrowserPopup::onViewModeGrid()
{
    mViewMode = ViewMode::Grid;
    mListBtn->setChecked(false);
    mGridBtn->setChecked(true);
    mViews->setCurrentWidget(mGrid);
    mZoomMasterPx = activeZoomPx();
    applyZoom();
    updateZoomButtonsEnabled();
    updateAcceptEnabled();
    mGrid->setFocus(Qt::OtherFocusReason);
    if (mViewModeDropBtn) {
        mViewModeDropBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/GridView.svg")));
        mViewModeDropBtn->setToolTip(tr("Switch to list view"));
    }
}

void CosmicFileBrowserPopup::changeEvent(QEvent* e)
{
    CosmicPopupWindow::changeEvent(e);
    if (e->type() != QEvent::LanguageChange) return;

    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    // Navigation button tooltips
    if (mBackBtn)    mBackBtn->setToolTip(safetr("Back"));
    if (mForwardBtn) mForwardBtn->setToolTip(safetr("Forward"));
    if (mUpBtn)      mUpBtn->setToolTip(safetr("Up one level"));

    // Toolbar button tooltips
    if (mRefreshBtn)      mRefreshBtn->setToolTip(safetr("Refresh"));
    if (mNewFolderBtn)    mNewFolderBtn->setToolTip(safetr("New Folder"));
    if (mShowHiddenBtn)   mShowHiddenBtn->setToolTip(safetr("Show Hidden"));
    if (mZoomInBtn)       mZoomInBtn->setToolTip(safetr("Zoom In"));
    if (mZoomOutBtn)      mZoomOutBtn->setToolTip(safetr("Zoom Out"));
    if (mSearchToggleBtn) mSearchToggleBtn->setToolTip(safetr("Search"));
    if (mViewModeDropBtn) mViewModeDropBtn->setToolTip(safetr("Toggle view mode"));
    if (mSettingsBtn)     mSettingsBtn->setToolTip(safetr("Settings"));

    // Path edit placeholder
    if (mPathEdit) mPathEdit->setPlaceholderText(safetr("Path"));

    // Search edit placeholder
    if (mSearchEdit) mSearchEdit->setPlaceholderText(safetr("Search"));

    // Bottom bar
    if (mFileNameEdit) mFileNameEdit->setPlaceholderText(safetr("File name"));
    if (mCancelBtn)    mCancelBtn->setText(safetr("Cancel"));
    if (mAcceptBtn) {
        if (mFolderMode) {
            mAcceptBtn->setText(safetr("Select"));
        } else {
            mAcceptBtn->setText(mMode == Mode::SaveFile ? safetr("Save") : safetr("Open"));
        }
    }

    // Update path label in case "(no path)" is shown
    updatePathLabel();

    // Note: all context menus (settings, view mode, file, sidebar) are created
    // fresh on every open via tr() — they are always correctly translated.
}
