#ifndef COSMICPOPUPWINDOW_H
#define COSMICPOPUPWINDOW_H

#include <QDialog>
#include <QColor>
#include <QMargins>
#include <QPoint>
#include <QPointer>

#include <QString>
#include <QPainterPath>

class QTimer;

class CosmicPopupWindow : public QDialog
{
    Q_OBJECT

public:
    enum class ModalMode
    {
        NonModal,
        WindowModal,
        ApplicationModal,
    };

    enum class ClosePolicy
    {
        Manual,
        CloseOnMouseLeave,
    };

    enum class Anchor
    {
        Center,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    explicit CosmicPopupWindow(QWidget* parent = nullptr);
    ~CosmicPopupWindow() override;

    void setModalMode(ModalMode mode);
    ModalMode modalMode() const { return mModalMode; }

    void setClosePolicy(ClosePolicy policy);
    ClosePolicy closePolicy() const { return mClosePolicy; }

    void setAutoCloseDelayMs(int ms);
    int autoCloseDelayMs() const { return mAutoCloseDelayMs; }

    void setResizable(bool on);
    bool isResizable() const { return mResizable; }

    void setFixedWindowSize(const QSize& sz);
    void clearFixedWindowSize();
    bool hasFixedWindowSize() const { return mHasFixedWindowSize; }

    void setPanelRadius(int px);
    int panelRadius() const { return mPanelRadius; }

    void setBorderWidth(int px);
    int borderWidth() const { return mBorderWidth; }

    void setPanelColor(const QColor& c);
    QColor panelColor() const { return mPanelColor; }

    void setBorderColor(const QColor& c);
    QColor borderColor() const { return mBorderColor; }

    void setShadowEnabled(bool on);
    bool shadowEnabled() const { return mShadowEnabled; }

    void setShadowPadding(int px);
    int shadowPadding() const { return mShadowPad; }

    void setShadowStrength(int alpha0to255);
    int shadowStrength() const { return mShadowStrength; }

    void setDraggable(bool on);
    bool isDraggable() const { return mDraggable; }

    void setDragRegionHeight(int px);
    int dragRegionHeight() const { return mDragRegionHeight; }

    void showAt(const QPoint& globalPos, Anchor anchor = Anchor::Center, bool clampToScreen = true);

    void setCenterOnShow(bool on);
    bool centerOnShow() const { return mCenterOnShow; }

    void setPersistentSizeKey(const QString& key);
    QString persistentSizeKey() const { return mPersistentSizeKey; }

protected:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    void paintEvent(QPaintEvent* e) override;

    void showEvent(QShowEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

    void keyPressEvent(QKeyEvent* e) override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
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

    void ensureResizeHitWidgets();
    void updateResizeHitWidgetsGeometry();

    ResizeRegion hitTestResizeRegion(const QPoint& localPos) const;
    QRect applyResize(const QRect& startGeom, ResizeRegion region, const QPoint& delta) const;

    void startAutoCloseTimer();
    void stopAutoCloseTimer();

    void schedulePersistSize();
    void persistSizeNow();

    void ensurePanelPath();

    ModalMode mModalMode = ModalMode::NonModal;

    ClosePolicy mClosePolicy = ClosePolicy::Manual;
    int mAutoCloseDelayMs = 300;

    bool mResizable = false;
    bool mHasFixedWindowSize = false;
    QSize mFixedWindowSize;

    bool mShadowEnabled = false;
    int mShadowPad = 0;
    int mShadowStrength = 0;

    bool mCompositorAvailable = true; // false on X11 without a compositor

    int mPanelRadius = 0;
    int mBorderWidth = 2;
    QColor mPanelColor = QColor(45, 45, 45);
    QColor mBorderColor = QColor(0, 0, 0, 0);

    bool mDraggable = true;
    int mDragRegionHeight = 48;

    QPointer<QTimer> mAutoCloseTimer;

    bool mCenterOnShow = true;
    bool mSkipCenterOnce = false;

    QString mPersistentSizeKey;
    QPointer<QTimer> mPersistSizeDebounce;

    bool mDraggingWindow = false;
    QPoint mDragGrabOffset;

    bool mResizingWindow = false;
    ResizeRegion mResizeRegion = ResizeRegion::None;
    QPoint mResizeStartGlobal;
    QRect mResizeStartGeom;

    bool mPanelPathDirty = true;
    QRect mPanelPathRect;
    int mPanelPathPad = 0;
    int mPanelPathRadius = 0;
    int mPanelPathBorderWidth = 0;
    QPainterPath mPanelPath;
    QRectF mPanelFillRect;

    int mResizeBandPx = 10;
    int mCornerHitPx = 24;

    QWidget* mResizeHitLeft = nullptr;
    QWidget* mResizeHitRight = nullptr;
    QWidget* mResizeHitTop = nullptr;
    QWidget* mResizeHitBottom = nullptr;
    QWidget* mResizeHitTL = nullptr;
    QWidget* mResizeHitTR = nullptr;
    QWidget* mResizeHitBL = nullptr;
    QWidget* mResizeHitBR = nullptr;

    void syncTemplateMetrics();
};

#endif // COSMICPOPUPWINDOW_H
