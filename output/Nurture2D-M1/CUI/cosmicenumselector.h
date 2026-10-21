#ifndef COSMICENUMSELECTOR_H
#define COSMICENUMSELECTOR_H

#include <QWidget>
#include <QStringList>
#include <QColor>
#include <QVariantAnimation>

// CosmicEnumSelector - a horizontal pill-style tab/enum selector.
//
// Visual design:
//  - Outer container: background #1a1a1a, thin 1px border in CosmicAccentColor,
//    corner radius = CosmicUiMetrics::cornerRadius().
//  - Height = CosmicUiMetrics::controlThickness() + 10.
//  - Selected item: animated sliding pill filled with CosmicAccentColor.
//  - Unselected: muted #aaaaaa text; hover shows subtle #252525 tint.
class CosmicEnumSelector : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicEnumSelector(QWidget* parent = nullptr);

    // Visual flags — default matches the original design (border + background on).
    void setShowBorder(bool show);
    void setShowBackground(bool show);

    void setOptions(const QStringList& options);
    QStringList options() const { return mOptions; }

    // Setting index programmatically skips the slide animation.
    void setCurrentIndex(int index, bool animate = false);
    int currentIndex() const { return mCurrentIndex; }

    void setCurrentOption(const QString& option);
    QString currentOption() const;

    // Per-item enable/disable. Disabled items are greyed out and unclickable.
    void setItemEnabled(int index, bool enabled);
    bool isItemEnabled(int index) const;

signals:
    void currentIndexChanged(int index);
    void currentOptionChanged(const QString& option);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QRectF itemRect(int index) const;
    int indexAtPos(const QPoint& pos) const;
    void animateTo(int newIndex);
    void snapHighlightToIndex(int index);

    QStringList mOptions;
    QVector<bool> mItemEnabled; // parallel to mOptions, all true by default
    int mCurrentIndex = 0;
    int mHoverIndex   = -1;
    int mPressIndex   = -1;

    bool mShowBorder     = true;
    bool mShowBackground = true;

    // Animated highlight rect (x position slides, width/height are fixed per item)
    QRectF mHighlightRect;
    QVariantAnimation* mSlideAnim = nullptr;
};

#endif // COSMICENUMSELECTOR_H
