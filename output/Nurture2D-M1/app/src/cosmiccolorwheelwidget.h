#ifndef COSMICCOLORWHEELWIDGET_H
#define COSMICCOLORWHEELWIDGET_H

#include <QWidget>
#include <QColor>
#include <QImage>
#include <QPixmap>

class QLabel;
class QLineEdit;
class CosmicCheckboxToggle;

class CosmicColorWheelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicColorWheelWidget(QWidget* parent = nullptr);

    QColor color() const;

signals:
    void colorChanged(const QColor& color);
    void colorSelected(const QColor& color);

public slots:
    void setColor(const QColor& color);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    enum class DragMode
    {
        None,
        Hue,
        Sv,
    };

    void updateLayoutRects();
    void updateFooterGeometry();
    void ensureSvImage();
    void ensureHueRingPixmap();

    bool isInHueRing(const QPointF& p) const;
    bool isInSvSquare(const QPointF& p) const;

    void applyHueFromPoint(const QPointF& p, bool emitSignal);
    void applySvFromPoint(const QPointF& p, bool emitSignal);

    static int clampInt(int v, int lo, int hi);
    static qreal clampF(qreal v, qreal lo, qreal hi);
    static QString colorToHex(const QColor& color);
    static QColor hexToColor(const QString& text, int fallbackAlpha);
    static QIcon makeWhiteIcon(const QString& path);

private:
    QColor mCurrent = QColor::fromHsv(0, 0, 0);
    bool mTriangleMode = false;

    QRectF mWheelRect;
    QRectF mSvRect;

    int mWheelThickness = 36;

    QImage mSvImage;
    int mSvHueCache = -1;
    QSize mSvSizeCache;
    QPixmap mSvPixmap;
    qreal mSvDprCache = 0.0;

    QPixmap mHueRingPixmap;
    QSize mHueRingSizeCache;
    int mHueRingThicknessCache = -1;
    qreal mHueRingDprCache = 0.0;

    DragMode mDragMode = DragMode::None;

    QWidget* mFooter = nullptr;
    QWidget* mColorPreview = nullptr;
    QLabel* mHexPrefix = nullptr;
    QLineEdit* mHexEdit = nullptr;
    CosmicCheckboxToggle* mModeToggle = nullptr;

    int mFooterHeight = 34;
    bool mUpdatingHex = false;
};

#endif // COSMICCOLORWHEELWIDGET_H
