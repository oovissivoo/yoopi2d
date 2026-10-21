/*
 * IgnoreFillSplitToggle - Specialized Blender-style toggle with a left color area
 * The left ~30% shows the current ignore color and triggers a color-pick action when clicked.
 * The right ~70% behaves like a normal toggle (enable/disable ignore-in-fill).
 */
#ifndef IGNOREFILLSPLITTOGGLE_H
#define IGNOREFILLSPLITTOGGLE_H

#include <QWidget>
#include <QColor>
#include <QPixmap>

class IgnoreFillSplitToggle : public QWidget
{
    Q_OBJECT
public:
    explicit IgnoreFillSplitToggle(QWidget* parent = nullptr);

    // Toggle state
    bool isChecked() const { return mChecked; }
    void setChecked(bool checked);

    // Text
    QString text() const { return mText; }
    void setText(const QString& text);

    // Visual color shown on the left sample area
    QColor color() const { return mColor; }
    void setColor(const QColor& c);

    // Styling
    void setAccentColor(const QColor& c);
    void setBackgroundColor(const QColor& c);
    void setTextColor(const QColor& c);
    void setSelectedTextColor(const QColor& c);
    void setCornerRadius(int r);

    // Ratio of the color sample area (0..1). Default 0.3
    void setColorAreaRatio(qreal r);
    qreal colorAreaRatio() const { return mColorAreaRatio; }

    // Enable/disable (keeps internal visual state consistent)
    void setEnabled(bool enabled);

signals:
    void toggled(bool checked);
    void clicked();
    void colorAreaClicked();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    // State
    bool mChecked = true;
    bool mHovered = false;
    bool mPressed = false;
    bool mEnabled = true;

    // Content
    QString mText;
    QColor mColor = Qt::black; // visible color (non-premultiplied)

    // Styling (defaults follow Blender-style widgets)
    QColor mAccentColor;
    QColor mBackgroundColor;
    QColor mTextColor;
    QColor mSelectedTextColor;
    int mCornerRadius = 4;

    // Layout
    qreal mColorAreaRatio = 0.30; // 30% left area

    // Interaction tracking
    QPoint mPressPos;
    bool mPressOnLeftArea = false;

    // Helpers
    QRect colorAreaRect() const;
    void initializeDefaultColors();
};

#endif // IGNOREFILLSPLITTOGGLE_H
