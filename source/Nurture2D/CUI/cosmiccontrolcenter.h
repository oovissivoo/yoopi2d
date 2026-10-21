#ifndef COSMICCONTROLCENTER_H
#define COSMICCONTROLCENTER_H

#include "cosmiccontrolcenterlayout.h"
#include <QWidget>
#include <QHash>
#include <QSet>

class CosmicPopupContextMenu;

class CosmicControlCenter : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicControlCenter(QWidget* parent = nullptr);
    ~CosmicControlCenter() override = default;

    int barHeight() const;

    void addSingle(const QString& id, QAction* action);
    void addGroup(const QString& groupId, const QVector<QAction*>& actions);
    void addWidgetGroup(const QString& groupId, QWidget* widget, int span);
    void addSpacer();

    void saveLayout();
    void loadLayout();

    /** Show or hide all cells belonging to a group id. */
    void setGroupVisible(const QString& groupId, bool visible);
    bool isGroupVisible(const QString& groupId) const;

    bool isEditMode() const { return mEditMode; }

signals:
    void editModeChanged(bool active);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    void toggleEditMode();
    void showContextMenu(const QPoint& globalPos);
    void repositionHostedWidgets();

    int   cellWidth()  const;
    int   cellHeight() const;
    int   cellAt(int x) const;
    QRect cellRect(int cell) const;
    QRect groupRect(int anchorCell, int span) const;

    void drawCell(QPainter& p, int cell);
    void drawGhost(QPainter& p);

    CosmicControlCenterLayout mLayout;
    QHash<QString, QWidget*>  mHostedWidgets;
    QSet<QString>             mHiddenGroups;  // group/item ids that are hidden

    bool   mEditMode       = false;
    bool   mDragging       = false;
    int    mDragAnchorCell = -1;
    int    mDragSpan       = 1;
    int    mDragGrabOffset = 0;
    QPoint mDragPos;
    int    mHoverCell      = -1;
};

#endif // COSMICCONTROLCENTER_H
