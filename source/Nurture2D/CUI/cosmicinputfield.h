#ifndef COSMICINPUTFIELD_H
#define COSMICINPUTFIELD_H

#include <QWidget>
#include <QColor>
#include <QLineEdit>
#include <QTimer>

// CosmicInputField - cosmic numeric input control with drag and inline edit.
//
// Visual variants:
// - Legacy: dark side panels (0x1f1f1fff) for decrement / increment buttons.
// - Minimal (default): side panels share the accent background, giving a
//   seamless modern bar as in the reference mockups.
//
// Interaction:
// - Click & drag horizontally on the center field to change value.
//   * Drag right  -> increase value (snaps to whole numbers)
//   * Drag left   -> decrease value (snaps to whole numbers)
//   * Hold Ctrl   -> snap to 0.5 increments (1.0, 1.5, 2.0 ...)
//   * Hold Shift  -> snap to nearest 5 (1, 5, 10, 15, 20, 25, 30)
// - Single-click on center (no drag) opens an inline editor (QLineEdit).
//   * The numeric text is fully selected by default so the user can
//     immediately type a new value.
// - Left / right regions act as decrement / increment buttons (snap to whole numbers).
// - Mouse wheel adjusts with the same Ctrl / Shift snap modifiers.
//
// Text rendering always uses the global cosmic text color (#b3b3b3ff) by
// default, and arrow icons come from the Interaction/ArrowLeft/Right assets.
class CosmicInputField : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(int buttonWidth READ buttonWidth WRITE setButtonWidth)

public:
    enum Variant {
        Legacy,
        Minimal
    };
    Q_ENUM(Variant)

    explicit CosmicInputField(QWidget* parent = nullptr);

    // Value properties
    double value() const { return mValue; }
    void setValue(double value);

    double minimum() const { return mMinimum; }
    void setMinimum(double min);

    double maximum() const { return mMaximum; }
    void setMaximum(double max);

    void setRange(double min, double max);

    double singleStep() const { return mSingleStep; }
    void setSingleStep(double step);

    int decimals() const { return mDecimals; }
    void setDecimals(int decimals);

    QString suffix() const { return mSuffix; }
    void setSuffix(const QString& suffix);

    QColor accentColor() const { return mAccentColor; }
    void setAccentColor(const QColor& color);

    QColor textColor() const { return mTextColor; }
    void setTextColor(const QColor& color);

    int buttonWidth() const { return mButtonWidth; }
    void setButtonWidth(int width);

    Variant variant() const { return mVariant; }
    void setVariant(Variant variant);

signals:
    void valueChanged(double value);
    void valueCommitted(double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private slots:
    void onInputFinished();
    void checkForSingleClick();

private:
    enum HitRegion {
        None,
        LeftButton,
        CenterField,
        RightButton
    };

    HitRegion hitTest(const QPoint& pos) const;
    QRect getLeftButtonRect() const;
    QRect getCenterFieldRect() const;
    QRect getRightButtonRect() const;
public:
    void increment();
    void decrement();
private:
    void showInputField();
    void hideInputField();
    QString formatDisplayValue(double val) const;
    QString formatNumericValue(double val) const;
    double clampValue(double val) const;

    // Data
    double mValue;
    double mMinimum;
    double mMaximum;
    double mSingleStep;
    int mDecimals;
    QString mSuffix;

    // Styling
    QColor mAccentColor;
    QColor mTextColor;
    Variant mVariant;
    int mButtonWidth;
    int mCornerRadius;
    QColor mBorderColor;
    int mBorderWidth;

    // Interaction state
    HitRegion mHoverRegion;
    HitRegion mPressedRegion;
    bool mDragging;
    QPoint mDragStartPos;
    double mDragStartValue;
    int mClickThreshold; // pixels to distinguish click from drag

    // Input field
    QLineEdit* mInputField;
    QTimer* mClickTimer;
    bool mPotentialClick;
    bool mInlineEditing;
};

#endif // COSMICINPUTFIELD_H
