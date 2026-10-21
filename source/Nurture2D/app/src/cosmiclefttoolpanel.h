#ifndef COSMICLEFTTOOLPANEL_H
#define COSMICLEFTTOOLPANEL_H

#include <QWidget>
#include <QHash>
#include <QTimer>

class CosmicLeftToolsBar;
class CosmicToolsPane;
class QVBoxLayout;
class QSpacerItem;
class QScrollArea;
class QScrollBar;
class BrushPresetListWidget;
class Editor;

class CosmicLeftToolPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicLeftToolPanel(Editor* editor, QWidget* parent = nullptr);
    ~CosmicLeftToolPanel() override;

    int reservedWidth() const;

    CosmicToolsPane* toolsPane() const { return mToolsPane; }
    CosmicToolsPane* toolsOptionsPane() const { return mToolsOptionsPane; }
    CosmicToolsPane* brushesPane() const { return mBrushesPane; }
    CosmicToolsPane* layersPane() const { return mLayersPane; }

    bool lockEnabled() const;
    void setLockEnabled(bool enabled);
    void toggleLock();

    bool paneEnabled(const QString& paneName) const;
    void setPaneEnabled(const QString& paneName, bool enabled);
    void togglePaneEnabled(const QString& paneName);

    void refreshPaneLayout();

    CosmicToolsPane* detachPaneForTransfer(const QString& paneName, bool* enabledOut);
    void attachTransferredPane(CosmicToolsPane* pane, const QString& paneName, bool enabled, int insertIdx);

    bool acceptsTransferAt(const QPoint& globalPos) const;
    void externalTransferHover(CosmicToolsPane* pane, const QString& paneName, const QPoint& globalPos);
    void externalTransferLeave();
    void externalTransferDrop(CosmicToolsPane* pane, const QString& paneName, bool enabled);

// Methods to sync pill toggle order with pane order
    QStringList getCurrentPaneOrder() const;
    void updatePillToggleOrder();
    void ensureStretchAtBottom();

    // Index system - pane positions as single source of truth
    QStringList getPaneIndexOrder() const;
    void updatePaneIndexOrder();

signals:
    void reservedWidthChanged();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void loadSurvivalSettings();
    void saveSurvivalSettings() const;
    void applyPaneIndexOrder(const QStringList& order);
    static QStringList normalizedPaneIndexOrder(const QStringList& order);

    void syncPaneVisibility();
    void updateSliderVisibility();
    
    // Auto-hide/show functionality
    void updateAutoHideVisibility();
    void showToolPanels();
    void hideToolPanels();
    bool isCursorOverToolArea(const QPoint& globalPos) const;

    void beginStackResize(const QPoint& globalPos);
    void updateStackResize(const QPoint& globalPos);
    void endStackResize();
    void updateStackResizeGripGeometry();

    void beginPaneVResize(CosmicToolsPane* pane, const QPoint& globalPos);
    void updatePaneVResize(const QPoint& globalPos);
    void endPaneVResize();
    void updatePaneResizeSplitterGeometry();
    void updatePaneSplitter(CosmicToolsPane* pane, QWidget* splitter, int splitterW, int splitterH);

    void onPaneDragStarted(CosmicToolsPane* pane, const QPoint& globalPos);
    void onPaneDragMoved(const QPoint& globalPos);
    void onPaneDragEnded();

    void onPaneMoveDragStarted(CosmicToolsPane* pane, const QPoint& globalPos);
    void onPaneMoveDragMoved(const QPoint& globalPos);
    void onPaneMoveDragEnded();

    // Connection management helpers
    void bindPaneToThisPanel(CosmicToolsPane* pane, const QString& paneName);
    void unbindPaneFromThisPanel(CosmicToolsPane* pane);

    class CosmicRightToolPanel* findOtherPanel() const;
    void clearMoveState(bool restoreIfNeeded);
    void applyTransferCursor(const QString& iconName);
    void clearTransferCursor();
    void clearExternalPlaceholder();
    void setPaneFloating(CosmicToolsPane* pane, bool floating, const QPoint& globalPos = QPoint());
    void showDropZoneIndicator(const QRect& area);
    void hideDropZoneIndicator();
    void updateDropZoneIndicator(const QPoint& globalPos);
    int layoutIndexOf(QWidget* w) const;
    int stretchIndex() const;
    int clampBeforeStretch(int idx) const;
    void movePlaceholderTo(int layoutIdx);
    void moveExternalPlaceholderTo(int layoutIdx);
    void animatePanesToLayout();
    void ensureReservedInsetUpdated();

    CosmicLeftToolsBar* mPill = nullptr;
    CosmicToolsPane* mToolsPane = nullptr;
    CosmicToolsPane* mToolsOptionsPane = nullptr;
    CosmicToolsPane* mBrushesPane = nullptr;
    CosmicToolsPane* mLayersPane = nullptr;

    QWidget* mPanesContainer = nullptr;
    QVBoxLayout* mPanesLayout = nullptr;
    QScrollArea* mPanesScrollArea = nullptr;
    QScrollBar* mPaneSlider = nullptr;

    // Drag-reorder state
    CosmicToolsPane* mDraggingPane = nullptr;
    QWidget* mDragPlaceholder = nullptr;
    QPoint mDragGrabOffset;

    // Cross-panel move state (floating)
    QHash<QString, CosmicToolsPane*> mPanes;
    QHash<QString, QString> mPaneIcons;
    CosmicToolsPane* mMovingPane = nullptr;
    QString mMovingPaneName;
    bool mMovingPaneWasEnabled = true;
    int mMoveOriginIndex = -1;
    QWidget* mMovePlaceholder = nullptr;
    QHash<QWidget*, QRect> mMoveOriginalGeometries;
    QPoint mMoveGrabOffset;
    Qt::WindowFlags mMovePrevFlags;
    bool mMoveOverOther = false;
    bool mTransferCursorApplied = false;

    QWidget* mExternalPlaceholder = nullptr;
    QString mExternalIncomingName;
    QHash<QWidget*, QRect> mExternalOriginalGeometries; // Store original positions for stable transfer thresholds
    QWidget* mDropZoneIndicator = nullptr; // Blue tinted area showing where pane will be docked

    // Stack-wide resize handling
    QWidget* mStackResizeGrip = nullptr;

    // Resize state
    bool mIsResizing = false;
    int mResizeStartX = 0;
    int mResizeStartWidth = 0;

    // Per-pane vertical resize state
    bool mIsVResizing = false;
    CosmicToolsPane* mVResizePane = nullptr;
    int mVResizeStartY = 0;
    int mVResizeStartHeight = 0;

    // Per-pane resize splitters (one per resizable pane, always positioned below the pane)
    QWidget* mToolsResizeSplitter = nullptr;
    QWidget* mToolsOptionsResizeSplitter = nullptr;
    QWidget* mBrushesResizeSplitter = nullptr;
    QWidget* mLayersResizeSplitter = nullptr;

    QHash<CosmicToolsPane*, int> mLastExpandedHeights;

    QHash<QWidget*, QRect> mLastGeometries;
    QHash<QWidget*, QRect> mOriginalGeometries; // Store original positions before drag starts
    
    // Index system - single source of truth for pane positions
    QStringList mPaneIndexOrder;

    int mPaneWidth = 280;
    int mGap = 8;
    
    // Auto-hide functionality
    bool mAutoHideEnabled = true;
    QTimer* mAutoHideTimer = nullptr;
};

#endif // COSMICLEFTTOOLPANEL_H
