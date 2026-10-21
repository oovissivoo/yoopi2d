#ifndef CELLSTACKWIDGET_H
#define CELLSTACKWIDGET_H

#include "basedockwidget.h"

class QWidget;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QPushButton;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QFrame;
class QTimer;

class BlenderStyleButton;
class Editor;

class CellStackWidget : public BaseDockWidget
{
    Q_OBJECT

public:
    explicit CellStackWidget(QWidget* parent = nullptr);
    ~CellStackWidget() override;

    void initUI() override;
    void updateUI() override;

protected:
    bool event(QEvent* event) override { return BaseDockWidget::event(event); }
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onCellSelected(int row);
    void onCellDoubleClicked(QListWidgetItem* item);
    void onAddCell();
    void onRemoveCell();
    void refreshStack();

private:
    void startInlineRename(QListWidgetItem* item);
    void finishInlineRename(bool accept);
    void commitOrderFromList(bool finalCommit);
    void startJiggle(int row);
    void stopJiggle();
    void updateJiggleOverlayGeometry();
    void advanceJiggle();

    void exposeCell(int cellId);
    QWidget* mRoot = nullptr;
    QVBoxLayout* mLayout = nullptr;

    // Second header row: stack actions (add / remove / import)
    QWidget* mActionsHeader = nullptr;
    QHBoxLayout* mActionsLayout = nullptr;
    BlenderStyleButton* mAddButton = nullptr;
    BlenderStyleButton* mRemoveButton = nullptr;

    // Body
    QListWidget* mListWidget = nullptr;
    QWidget* mListViewport = nullptr;
    QLineEdit* mInlineEditor = nullptr;
    int mEditingCellId = -1;
    bool mIsInlineEditing = false;

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

#endif // CELLSTACKWIDGET_H
