#ifndef COSMICFILEBROWSERPOPUP_H
#define COSMICFILEBROWSERPOPUP_H

#include "cosmicpopupwindow.h"

#include <QDateTime>
#include <QFileInfo>
#include <QModelIndex>
#include <QPointer>
#include <QPoint>
#include <QStringList>

class CosmicButton;
class CosmicCheckboxToggle;
class QAbstractItemView;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QListWidgetItem;
class QFileIconProvider;
class QWidget;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeView;
class QSortFilterProxyModel;
class QTimer;
class CosmicRenameOperator;
class CosmicWindowHeader;

class CosmicFileBrowserPopup : public CosmicPopupWindow
{
    Q_OBJECT

public:
    enum class Mode
    {
        OpenFile,
        OpenFiles,
        SaveFile,
    };

    struct Result
    {
        bool accepted = false;
        QStringList selectedPaths;
    };

    struct BookmarkEntry
    {
        QString path;
        QString name; // display name only; does NOT rename the folder
    };

    explicit CosmicFileBrowserPopup(Mode mode,
                                   const QString& caption,
                                   const QString& initialDir,
                                   const QStringList& nameFilters,
                                   QWidget* parent = nullptr);

    Result execAndGetResult();

    void setDefaultSuffix(const QString& suffix);
    // When enabled, the browser accepts the current directory as the selection.
    // The accept button shows "Select" and is always enabled once a dir is loaded.
    void setFolderMode(bool on);

protected:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    void done(int r) override;
    void changeEvent(QEvent* e) override;

private slots:
    void onNavigateUp();
    void onNavigateBack();
    void onNavigateForward();
    void onPathCommitted();
    void onSearchChanged(const QString& text);
    void onActivatedInView(const QModelIndex& index);
    void onSelectionChanged();
    void onAddBookmarkClicked();
    void onSidebarItemActivated(QListWidgetItem* item);
    void onSidebarContextMenuRequested(const QPoint& pos);
    void onSidebarItemChanged(QListWidgetItem* item);
    void onContentContextMenuRequested(const QPoint& pos);
    void onRefreshClicked();
    void onNewFolderClicked();
    void onToggleHiddenClicked(bool on);
    void onTypeSelectTimeout();
    void onAcceptClicked();
    void onCancelClicked();
    void onViewModeList();
    void onViewModeGrid();

private:
    enum class ViewMode
    {
        List,
        Grid,
    };

    enum class SidebarItemType
    {
        Location,
        Bookmark,
        Device,
    };

    void buildUi();
    void applyCosmicStyling();
    void showViewModeDropdown();
    void showSettingsDropdown();
    void updatePathLabel();
    void setSearchVisible(bool visible);

    void applyZoom();
    void updateZoomButtonsEnabled();

    int minZoomPxForViewMode(ViewMode mode) const;
    int maxZoomPxForViewMode(ViewMode mode) const;
    int zoomPxForViewMode(ViewMode mode) const;
    void setZoomPxForViewMode(ViewMode mode, int px);
    int activeZoomPx() const;
    void setActiveZoomPx(int px);

    void scheduleVisibleThumbnailPrefetch();
    void prefetchVisibleThumbnailsNow();

    void setCurrentDir(const QString& dirPath, bool pushHistory);
    void rebuildBreadcrumb();

    QString currentDir() const;

    QString selectedPathForSave() const;
    QString selectedPathForOpenSingle() const;
    QStringList selectedPathsForOpenMulti() const;

    QFileInfo fileInfoForViewIndex(QObject* viewObj, const QModelIndex& index) const;

    QAbstractItemView* activeContentView() const;
    QStringList selectedPathsInActiveView() const;

    void opCopy(bool cut);
    void opPaste(const QString& targetDir);
    void opRename();
    void opDelete();

    void updateAcceptEnabled();
    void updateSidebar();
    void updateSidebarActiveHighlight();

    void applySidebarSectionVisibility();

    QString uiSettingsGroupKeyForMode() const;
    void applyFsModelFilter();
    void refreshCurrentDir();
    void startRenameForPath(const QString& absolutePath);

    void loadUiSettings();
    void saveUiSettings() const;

    void loadBookmarks();
    void saveBookmarks() const;

    int indexOfBookmarkPath(const QString& path) const;
    void removeBookmarkAt(int index);

    QString typeStringForFileInfo(const QFileInfo& fi) const;

    Mode mMode;
    ViewMode mViewMode = ViewMode::List;

    QString mCaption;
    QStringList mNameFilters;
    QString mDefaultSuffix;
    bool mFolderMode = false;

    QStringList mBackStack;
    QStringList mForwardStack;

    QPointer<QSplitter> mSplitter;
    QPointer<QListWidget> mSidebar;

    QPointer<CosmicButton> mAddBookmarkBtn;
    QVector<BookmarkEntry> mBookmarks;

    int mBookmarksRowStart = -1;
    int mBookmarksRowEndExclusive = -1;

    int mLocationsHeaderRow = -1;
    int mLocationsRowStart = -1;
    int mLocationsRowEndExclusive = -1;

    int mBookmarksHeaderRow = -1;

    int mDevicesHeaderRow = -1;
    int mDevicesRowStart = -1;
    int mDevicesRowEndExclusive = -1;

    bool mLocationsExpanded = true;
    bool mBookmarksExpanded = true;
    bool mDevicesExpanded = true;

    QPointer<CosmicWindowHeader> mWindowHeader;

    // Legacy header widget (kept for compat, hidden in new layout)
    QPointer<QWidget> mHeader;
    QPointer<CosmicButton> mBackBtn;
    QPointer<CosmicButton> mForwardBtn;
    QPointer<CosmicButton> mUpBtn;

    QPointer<QWidget> mBreadcrumb;
    QPointer<QLineEdit> mPathEdit;

    // New two-panel header widgets
    QPointer<QWidget> mLeftPanelHeader;   // holds nav buttons
    QPointer<QWidget> mRightPanelHeader;  // holds path label, search, view toggle, settings

    // Path squircle label (double-click to edit)
    QPointer<QWidget>   mPathLabelWidget; // container with squircle bg
    QPointer<QLabel>    mPathLabel;       // display label
    QPointer<QStackedWidget> mPathStack;  // 0=label, 1=edit

    // Search toggle
    QPointer<CosmicButton> mSearchToggleBtn;
    bool mSearchVisible = false;

    // View mode dropdown button
    QPointer<CosmicButton> mViewModeDropBtn;

    // Settings dropdown button
    QPointer<CosmicButton> mSettingsBtn;

    QPointer<QLineEdit> mSearchEdit;
    QPointer<CosmicButton> mRefreshBtn;
    QPointer<CosmicButton> mNewFolderBtn;
    QPointer<CosmicCheckboxToggle> mShowHiddenBtn;
    bool mShowHidden = false;
    QPointer<CosmicButton> mZoomInBtn;
    QPointer<CosmicButton> mZoomOutBtn;
    int mZoomMasterPx = 64;
    int mZoomGridPx = 64;
    int mZoomListPx = 32;
    QPointer<CosmicButton> mListBtn;
    QPointer<CosmicButton> mGridBtn;

    QPointer<QStackedWidget> mViews;
    QPointer<QTreeView> mTree;
    QPointer<QListView> mGrid;

    QFileIconProvider* mIconProvider = nullptr;

    QPointer<QWidget> mBottomBar;
    QPointer<QLineEdit> mFileNameEdit;
    QPointer<CosmicButton> mCancelBtn;
    QPointer<CosmicButton> mAcceptBtn;

    QPointer<QFileSystemModel> mFsModel;
    QPointer<QSortFilterProxyModel> mProxyModel;

    QPointer<QTimer> mUiSaveDebounce;
    QPointer<QTimer> mThumbPrefetchDebounce;
    QPointer<QTimer> mTypeSelectTimer;
    QString mTypeSelectBuffer;
    QPointer<CosmicRenameOperator> mRenameOp;

    QString mPendingRenamePath;
    QPointer<QAbstractItemView> mPendingRenameView;

    QString mActiveSidebarPath;

    bool mBookmarkDragActive = false; // true while a folder is being dragged from content area

    QPoint mSidebarPressPos;
    bool mSidebarIsDragging = false;
    QListWidgetItem* mSidebarDraggedItem = nullptr;
    int mSidebarDragStartRow = -1;
    int mSidebarDropRow = -1;
    QListWidgetItem* mSidebarPlaceholder = nullptr;

    Result mResult;
};

#endif // COSMICFILEBROWSERPOPUP_H
