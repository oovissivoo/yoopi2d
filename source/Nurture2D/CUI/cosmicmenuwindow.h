#ifndef COSMICMENUWINDOW_H
#define COSMICMENUWINDOW_H

#include <QWidget>
#include <QVector>
#include <QRect>
#include <QString>
#include <QTimer>
#include <functional>

class QAction;
class QPainter;
class CosmicMenuBar;

/**
 * CosmicMenuWindow — custom-rendered popup menu widget.
 *
 * Uses Qt::Tool | Qt::FramelessWindowHint (NOT Qt::Popup) so the bar
 * retains mouse events while the menu is open, enabling seamless hover
 * switching between entries. Closing is managed manually via an
 * application-level event filter installed by CosmicMenuBar.
 *
 * All rendering via QPainter. Colors from CosmicColor, metrics from CosmicUiMetrics.
 */
class CosmicMenuWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicMenuWindow(QWidget* parent = nullptr);
    ~CosmicMenuWindow() override;

    // Item population
    void addAction(QAction* action);
    // label is the display string; labelFactory (optional) refreshes it on retranslation.
    void addMenu(const QString& label, CosmicMenuWindow* submenu,
                 std::function<QString()> labelFactory = {});
    void addSeparator();
    void clearItems();

    // Popup control
    void popup(const QPoint& globalTopLeft);
    void closeAll();

    // Width policy
    void setMaximumMenuWidth(int px);

    // Returns true if globalPos is inside this window or any open child
    bool containsGlobal(const QPoint& globalPos) const;
    // Routes a global mouse-move hover update into this window or its open child
    void updateHoverFromGlobal(const QPoint& globalPos);

    // Replaces the first two action items (undo/redo) with new action pointers.
    // Called by MainWindow2::replaceUndoRedoActions() when the undo stack switches.
    void replaceUndoRedo(QAction* newUndo, QAction* newRedo);

    // Re-evaluate all submenu label factories and repaint (call after language change)
    void retranslate();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    enum class ItemKind { Action, Submenu, Separator };

    struct MenuItem {
        ItemKind                 kind    = ItemKind::Action;
        QAction*                 action  = nullptr;
        QString                  label;
        std::function<QString()> labelFactory; // re-evaluated on retranslate()
        CosmicMenuWindow*        submenu = nullptr;   // not owned
        QRect                    rowRect;
    };

    void recalcLayout();
    QSize computePreferredSize() const;
    void openSubmenuAt(int index);
    void closeSubmenu();
    int  itemAtPos(const QPoint& localPos) const;
    int  nextEnabledItem(int from, int direction) const;
    bool isItemEnabled(int index) const;
    void triggerItem(int index);
    void paintItem(QPainter& p, int index) const;

    QVector<MenuItem> mItems;
    int mHoverIndex    = -1;
    int mActiveSubmenu = -1;

    CosmicMenuBar* mOwnerBar    = nullptr;
    int            mBarIndex    = -1;

    int mMaxWidth = 400;

    QTimer* mSubmenuTimer   = nullptr;
    int     mPendingSubmenu = -1;

    static constexpr int kMinWidth         = 220;
    static constexpr int kToggleAreaWidth  = 28;
    static constexpr int kTextLeftPad      = 8;
    static constexpr int kTextRightPad     = 16;
    static constexpr int kShortcutGap      = 24;
    static constexpr int kRowMinHeight     = 30;
    static constexpr int kSeparatorVMargin = 4;
    static constexpr int kWindowVPad       = 6;

    friend class CosmicMenuBar;
};

#endif // COSMICMENUWINDOW_H
