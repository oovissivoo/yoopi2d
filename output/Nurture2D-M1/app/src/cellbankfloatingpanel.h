#ifndef CELLBANKFLOATINGPANEL_H
#define CELLBANKFLOATINGPANEL_H

#include <QWidget>
#include <QColor>
#include <QLabel>

class QHBoxLayout;
class QVBoxLayout;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QFrame;
class QTimer;
class CosmicButton;
class Editor;

// CellBankFloatingPanel
//
// A standalone floating panel that shows the cell bank for the current bitmap layer.
// Rendered with CosmicToolsPane-style dark background and rounded corners.
// Layout: 70% left (title + action buttons) / 30% right (cell list).
// Shown/hidden by a toggle button placed bottom-right of the canvas.
class CellBankFloatingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CellBankFloatingPanel(QWidget* parent = nullptr);
    ~CellBankFloatingPanel() override;

    void setEditor(Editor* editor);
    Editor* editor() const { return mEditor; }

    void initUI();
    void updateUI();

    // Reposition relative to the toggle button anchor (bottom-right of canvas).
    // Call this whenever the canvas or bottom bar geometry changes.
    void repositionAbove(const QPoint& toggleBottomRight, int bottomBarTop);

protected:
    void paintEvent(QPaintEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void refreshStack();
    void onCellSelected(int row);
    void onCellDoubleClicked(QListWidgetItem* item);
    void onAddCell();
    void onRemoveCell();

private:
    void startInlineRename(QListWidgetItem* item);
    void finishInlineRename(bool accept);
    void exposeCell(int cellId);
    void commitOrderFromList(bool finalCommit);
    void startJiggle(int row);
    void stopJiggle();
    void updateJiggleOverlayGeometry();
    void advanceJiggle();

    Editor* mEditor = nullptr;
    bool mInitialized = false;

    // Left panel (70%) — title + buttons
    QWidget* mLeftPanel = nullptr;
    QLabel* mTitleLabel = nullptr;
    CosmicButton* mAddButton = nullptr;
    CosmicButton* mRemoveButton = nullptr;
    CosmicButton* mCleanupButton = nullptr;
    CosmicButton* mRenameTimelineOrderButton = nullptr;

    // Right panel (30%) — list
    QWidget* mRightPanel = nullptr;
    QListWidget* mListWidget = nullptr;
    QWidget* mListViewport = nullptr;
    QLineEdit* mInlineEditor = nullptr;
    int mEditingCellId = -1;
    bool mIsInlineEditing = false;

    // Drag-to-reorder state
    bool mIsDragging = false;
    int mDragCurrentRow = -1;
    QPoint mDragStartPos;
    int mDragTargetRow = -1;
    QFrame* mDragIndicator = nullptr;
    QTimer* mDragIndicatorTimer = nullptr;
    QTimer* mJiggleStartTimer = nullptr;
    int mJiggleRow = -1;
    int mJiggleStep = 0;
};

#endif // CELLBANKFLOATINGPANEL_H
