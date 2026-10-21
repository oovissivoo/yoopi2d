#ifndef COSMICCONTROLCENTER_H
#define COSMICCONTROLCENTER_H

#include <QWidget>
#include <QHash>
#include <QRect>
#include <QVector>

class Editor;
class PenColorPalette;
class AutoLineColorToggle;
class QToolBar;
class QHBoxLayout;
class QScrollArea;
class QContextMenuEvent;
class QMouseEvent;
class QShowEvent;
class QLayoutItem;

class CosmicControlCenter : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicControlCenter(QWidget* parent = nullptr);

    void setEditor(Editor* editor);
    void setToolbars(QToolBar* mainToolbar, QToolBar* viewToolbar, QToolBar* overlayToolbar);

    PenColorPalette* penPalette() const { return mPenPalette; }
    AutoLineColorToggle* autoFillToggle() const { return mAutoFillToggle; }

    int preferredHeight() const { return mPreferredHeight; }

    bool isPresetColor(const QColor& color) const;

signals:
    void colorChanged(const QColor& color);

public slots:
    void setColorFromExternal(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuildTiles();

    void setEditMode(bool on);
    void applyEditModeInteractionOverride();
    void loadTileLayoutSettings();
    void saveTileLayoutSettings() const;
    void normalizeGroupPositions();
    int tileIndexAtViewportPos(const QPoint& viewportPos) const;
    void updateDragAutoScroll(const QPoint& viewportPos);

    int layoutIndexOf(QWidget* w) const;
    void updateGroupStartsFromLayout();
    void beginGroupDrag(int groupId, const QPoint& viewportPos);
    void beginSingleTileDrag(QWidget* tile, const QPoint& viewportPos);
    void updateGroupDrag(const QPoint& viewportPos);
    void endGroupDrag();

    enum class ToolbarCategory { Main = 0, View = 1, Overlay = 2 };
    QVector<QAction*> actionsForCategory(ToolbarCategory cat) const;
    QString categoryKey(ToolbarCategory cat) const;
    bool isCategoryEnabled(ToolbarCategory cat) const;
    void setCategoryEnabled(ToolbarCategory cat, bool on);
    QString actionKey(QAction* a) const;
    bool isActionEnabled(QAction* a) const;
    void setActionEnabled(QAction* a, bool on);
    QStringList loadActionOrder() const;
    void saveActionOrder(const QStringList& order) const;
    QStringList actionOrderFromLayout() const;
    void showTilesPopup(const QPoint& globalPos);

    int mPreferredHeight = 32;

    Editor* mEditor = nullptr;
    QScrollArea* mScrollArea = nullptr;
    QWidget* mScrollContent = nullptr;
    QHBoxLayout* mPinsLayout = nullptr;
    PenColorPalette* mPenPalette = nullptr;
    AutoLineColorToggle* mAutoFillToggle = nullptr;

    QToolBar* mMainToolbar = nullptr;
    QToolBar* mViewToolbar = nullptr;
    QToolBar* mOverlayToolbar = nullptr;

    QWidget* mTilesPopup = nullptr;

    bool mEditMode = false;
    bool mInitialScrollPositionPending = true;

    int mPenGroupStart = 0;
    int mAutoFillGroupStart = 6;

    int mLogicalTileCount = 56;
    int mLastUsedTileIndex = 11;

    bool mDragArmed = false;
    bool mDragging = false;
    int mDraggingGroupId = -1;
    int mDragGroupSpan = 0;
    int mDragStartIndexOriginal = 0;
    QPoint mDragPressViewportPos;
    int mLastDragTargetIndex = -1;

    QWidget* mDragPlaceholder = nullptr;
    QWidget* mDragFloatingHost = nullptr;
    QHBoxLayout* mDragFloatingLayout = nullptr;
    QPoint mDragGrabOffset;

    QVector<QWidget*> mDraggingWidgets;
    QHash<QWidget*, QRect> mDragOriginalGeometries;

    QWidget* mArmedTileWidget = nullptr;
};

#endif // COSMICCONTROLCENTER_H
