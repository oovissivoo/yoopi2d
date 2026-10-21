#ifndef COSMICTOOLSPANE_H
#define COSMICTOOLSPANE_H

#include <QWidget>
#include <QString>
#include <QPoint>
 #include <QPainterPath>
 #include <QRect>
 #include <QRectF>

class QToolButton;
class QLabel;
class QWidget;

class CosmicToolsPane : public QWidget
{
    Q_OBJECT
public:
    enum class Side
    {
        Left,
        Right,
    };

    explicit CosmicToolsPane(const QString& title, Side side, QWidget* parent = nullptr);

    void setFloatingPopupMode(bool on);
    bool isFloatingPopupMode() const { return mFloatingPopupMode; }

    QWidget* headerWidget() const { return mHeader; }

    Side side() const { return mSide; }
    void setSide(Side side);

    void setExpanded(bool expanded);
    bool isExpanded() const { return mExpanded; }

    void setContentWidget(QWidget* w);
    QWidget* contentWidget() const;
    void refreshContentWidget();

    // Update the displayed title without changing the settings key.
    void setDisplayTitle(const QString& title);
    QString displayTitle() const;

    void setBodyForcedHidden(bool hidden);
    bool isBodyForcedHidden() const { return mBodyForcedHidden; }

    void setResizeGripEnabled(bool on);
    void setVerticalResizeGripEnabled(bool on);
    bool isVerticalResizeGripEnabled() const { return mVerticalResizeGripEnabled; }

    int lastExpandedHeight() const { return mLastExpandedHeight; }
    void setLastExpandedHeight(int height) { mLastExpandedHeight = height; }

signals:
    void expandedChanged(bool expanded);

    void dragStarted(const QPoint& globalPos);
    void dragMoved(const QPoint& globalPos);
    void dragEnded();

    void moveDragStarted(const QPoint& globalPos);
    void moveDragMoved(const QPoint& globalPos);
    void moveDragEnded();

    void resizeDragStarted(const QPoint& globalPos);
    void resizeDragMoved(const QPoint& globalPos);
    void resizeDragEnded();

    void resizeVDragStarted(const QPoint& globalPos);
    void resizeVDragMoved(const QPoint& globalPos);
    void resizeVDragEnded();

protected:
    void paintEvent(QPaintEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void updateExpandIcon();
    void syncBodyVisibility();
    void updateBodyVisibility();
    void updateHeaderUi();
    void updateResizeGripGeometry();
    void updateVerticalResizeGripGeometry();

    void ensurePaintCache();

    Side mSide = Side::Left;

    QWidget* mHeader = nullptr;
    QWidget* mHeaderCenterHit = nullptr;
    QWidget* mBody = nullptr;
    QWidget* mContent = nullptr;
    QWidget* mContentHost = nullptr;

    QToolButton* mDragButton = nullptr;
    QLabel* mTitleLabel = nullptr;
    QToolButton* mExpandButton = nullptr;

    QWidget* mResizeGrip = nullptr;
    bool mResizeGripEnabled = true;

    QWidget* mVerticalResizeGrip = nullptr;
    bool mVerticalResizeGripEnabled = false;

    bool mResizeActive = false;
    bool mVerticalResizeActive = false;

    bool mExpanded = true;
    bool mDragActive = false;
    bool mDragStarted = false;
    QPoint mDragPressGlobalPos;
    int mLastExpandedHeight = 0;
    bool mBodyForcedHidden = false;

    bool mFloatingPopupMode = false;

    bool mPaintCacheDirty = true;
    QRect mPaintCacheRect;
    QRect mPaintCacheHeaderGeom;
    bool mPaintCacheIsWindow = false;
    QPainterPath mPaintOuterPath;
    QPainterPath mPaintHeaderPath;
    QPainterPath mPaintHeaderClip;
};

#endif // COSMICTOOLSPANE_H
