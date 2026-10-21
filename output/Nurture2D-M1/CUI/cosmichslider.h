#ifndef COSMICHSLIDER_H
#define COSMICHSLIDER_H

#include <QSlider>
#include <QColor>
#include <QString>

class QLineEdit;

/**
 * CosmicHSlider - Horizontal cosmic-style slider with optional in-groove label.
 *
 * Visual design:
 * - Orientation is always horizontal.
 * - Background: solid rectangle using color #1f1f1f (1f1f1fff).
 * - Handle: vertical bar spanning almost full height, color #808080.
 * - Text color: global dock text color #b3b3b3 (b3b3b3ff).
 *
 * Behavior:
 * - Inherits QSlider value/range API and signals.
 * - Click + drag anywhere on the control to adjust the value.
 * - Optional label rendered inside the groove; position depends on value:
 *      value in (50%, 100%]  -> label aligned to the left half of the groove
 *      value in [0%, 50%]    -> label aligned to the right half of the groove
 */
class CosmicHSlider : public QSlider
{
    Q_OBJECT

public:
    enum LabelMode {
        Textless,
        WithLabel
    };

    explicit CosmicHSlider(QWidget* parent = nullptr);
    explicit CosmicHSlider(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~CosmicHSlider() override;

    void setBackgroundColor(const QColor& color);
    void setHandleColor(const QColor& color);
    void setTextColor(const QColor& color);

    void setLabel(const QString& text);
    QString label() const { return mLabel; }

    void setLabelMode(LabelMode mode);
    LabelMode labelMode() const { return mLabelMode; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    int valueFromPosition(const QPoint& pos) const;
    QRect leftLabelRect(const QRect& grooveRect) const;
    QRect rightLabelRect(const QRect& grooveRect) const;
    void beginInlineEdit();

    QColor mBackgroundColor;
    QColor mHandleColor;
    QColor mHandleHoverColor;
    QColor mTextColor;

    QString mLabel;
    LabelMode mLabelMode = Textless;

    bool mDragging = false;
    bool mHover = false;

    bool mPressOnTrack = false;
    QLineEdit* mInlineEditor = nullptr;
    int mInlineEditOriginalValue = 0;
    bool mInlineEditCancelled = false;
};

#endif // COSMICHSLIDER_H
