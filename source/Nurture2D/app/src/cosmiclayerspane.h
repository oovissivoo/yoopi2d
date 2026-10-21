#ifndef COSMICLAYERSPANE_H
#define COSMICLAYERSPANE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QVector>

class Editor;
class CosmicButton;
class CosmicHSlider;
class QLineEdit;
class QScrollArea;
class LayerItem;
class LayerDropIndicator;
class PopupColorWheelWidget;

class CosmicLayersPane : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicLayersPane(Editor* editor, QWidget* parent = nullptr);
    void refresh();
    void setLassoRestricted(bool restricted);

signals:
    void addLayerRequested();
    void removeLayerRequested();
    void importImageToLayerRequested();

public:
    // Shortcut-triggered inner-layer navigation.
    // "Above" = lower index in the stack (index 0 = bottom, higher = top in UI).
    void selectPaperAbove();
    void selectPaperBelow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void changeEvent(QEvent* event) override;

private:
    void connectSignals();
    int  currentCellId() const;
    void applyTint(const QColor& color);
    void updateTintButtonIcon();

    void onItemSelected(int logicalIdx);
    void onItemVisToggled(int logicalIdx);
    void onItemLockToggled(int logicalIdx);
    void onItemDblClicked(int logicalIdx);

    // Inline rename — owned directly, parented to scroll viewport (stable)
    void startInlineRename(int logicalIdx);
    void finishInlineRename(bool accept);
    QRect inlineRenameRect(int logicalIdx) const;

    // Drag reorder
    void startDrag(int visualIndex);
    void updateDrag(const QPoint& globalPos);
    void endDrag();
    int  getDropIndex(const QPoint& globalPos) const;

    // Image drag-and-drop into papers pane
    void handleImageDragEnter(QDragEnterEvent* e);
    void handleImageDragMove(QDragMoveEvent* e);
    void handleImageDrop(QDropEvent* e);
    void updateImageDropIndicator(const QPoint& viewportPos);
    void hideImageDropIndicator();
    int  getImageDropIndex(const QPoint& viewportPos) const;
    void importImagesAtIndex(const QList<QUrl>& urls, int insertAboveVisualIndex);

    Editor*               mEditor          = nullptr;
    CosmicButton*         mTintButton      = nullptr;
    CosmicButton*         mAddButton       = nullptr;
    CosmicButton*         mImportButton    = nullptr;
    CosmicButton*         mDeleteButton    = nullptr;
    CosmicHSlider*        mOpacitySlider   = nullptr;
    QLabel*               mEmptyStateLabel = nullptr;
    QLabel*               mLockHdrLabel    = nullptr;  // lock column header tooltip
    QScrollArea*          mScrollArea      = nullptr;
    QWidget*              mContainerWidget = nullptr;
    QVBoxLayout*          mContainerLayout = nullptr;
    QWidget*              mDropIndicator   = nullptr;
    PopupColorWheelWidget* mColorWheelPopup = nullptr;

    // Inline rename state
    QLineEdit*            mInlineEditor    = nullptr;
    bool                  mIsInlineEditing = false;
    int                   mRenamingIndex   = -1;
    bool                  mHasAppEventFilter = false;

    QVector<LayerItem*>   mItems;

    // Drag reorder state
    bool                  mIsDragging      = false;
    int                   mDragFromIndex   = -1;
    int                   mDropIndex       = -1;

    // Image drop indicator (separate from reorder indicator)
    QWidget*              mImageDropIndicator = nullptr;
    bool                  mIsImageDragging    = false;
};

#endif // COSMICLAYERSPANE_H
