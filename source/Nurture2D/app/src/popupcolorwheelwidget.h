/*
Popup Color Wheel Widget
A simplified popup version of the ColorBox that only includes the color wheel,
preview, hex input, and wheel toggle - without the slider functionality.
Automatically closes when the mouse leaves the widget.
*/

#ifndef POPUPCOLORWHEELWIDGET_H
#define POPUPCOLORWHEELWIDGET_H

#include "cosmictoolspane.h"

#include <QColor>

#include <QPoint>

class QTimer;

class CosmicColorWheelWidget;
class PreferenceManager;

class PopupColorWheelWidget : public CosmicToolsPane
{
    Q_OBJECT

public:
    explicit PopupColorWheelWidget(QWidget* parent = nullptr);
    virtual ~PopupColorWheelWidget();

    QColor color() const;
    void setColor(const QColor& color);
    void showAt(const QPoint& position);
    void setPreferenceManager(PreferenceManager* manager);
    void startLoadingFeedback();

signals:
    void colorChanged(const QColor& color);
    void colorSelected(const QColor& color);

protected:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onAutoCloseTimer();

private:
    void setupUI();
    void setupConnections();

    void setAppMouseTrackingEnabled(bool enabled);

    void ensureResizeHitWidgets();
    void updateResizeHitWidgetsGeometry();

    enum class ResizeRegion
    {
        None,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    ResizeRegion hitTestResizeRegion(const QPoint& localPos) const;
    void updateResizeCursor(const QPoint& localPos);
    QRect applyResize(const QRect& startGeom, ResizeRegion region, const QPoint& delta) const;

    // UI Components
    CosmicColorWheelWidget* mWheel = nullptr;

    QWidget* mResizeHitLeft = nullptr;
    QWidget* mResizeHitRight = nullptr;
    QWidget* mResizeHitTop = nullptr;
    QWidget* mResizeHitBottom = nullptr;
    QWidget* mResizeHitTL = nullptr;
    QWidget* mResizeHitTR = nullptr;
    QWidget* mResizeHitBL = nullptr;
    QWidget* mResizeHitBR = nullptr;

    // Window interaction
    int mResizeBandPx = 14;
    int mCornerHitPx = 28;
    int mAutoClosePaddingPx = 12;
    bool mDraggingWindow = false;
    QPoint mDragGrabOffset;

    bool mResizingWindow = false;
    ResizeRegion mResizeRegion = ResizeRegion::None;
    QPoint mResizeStartGlobal;
    QRect mResizeStartGeom;

    bool mResizeCursorOverrideActive = false;
    bool mAppMouseTrackingEnabled = false;

    // Auto-close functionality
    QTimer* mAutoCloseTimer = nullptr;
    bool mClosePending = false;
    
    // State
    PreferenceManager* mPreferenceManager = nullptr;
};

#endif // POPUPCOLORWHEELWIDGET_H