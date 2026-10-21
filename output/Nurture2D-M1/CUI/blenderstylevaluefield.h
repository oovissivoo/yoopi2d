#ifndef BLENDERSTYLEVALUEFIELD_H
#define BLENDERSTYLEVALUEFIELD_H

#include <QWidget>
#include <QColor>
#include <QLineEdit>
#include <QTimer>

class BlenderStyleValueField : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(QString label READ label WRITE setLabel)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius)
    Q_PROPERTY(bool compactMode READ compactMode WRITE setCompactMode)
    Q_PROPERTY(bool legacySidePanels READ legacySidePanels WRITE setLegacySidePanels)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth)
    // Display mode: embedded (label + value inside) vs textless (only value inside; label drawn externally)
    Q_PROPERTY(bool textlessMode READ textlessMode WRITE setTextlessMode)

public:
    explicit BlenderStyleValueField(QWidget *parent = nullptr);

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

    // Label
    QString label() const { return mLabel; }
    void setLabel(const QString &label);

    // Styling
    QColor accentColor() const { return mAccentColor; }
    void setAccentColor(const QColor &color);

    QColor backgroundColor() const { return mBackgroundColor; }
    void setBackgroundColor(const QColor &color);

    QColor textColor() const { return mTextColor; }
    void setTextColor(const QColor &color);

    int cornerRadius() const { return mCornerRadius; }
    void setCornerRadius(int radius);

    bool legacySidePanels() const { return mLegacySidePanels; }
    void setLegacySidePanels(bool enabled);

    QColor borderColor() const { return mBorderColor; }
    void setBorderColor(const QColor& color);

    int borderWidth() const { return mBorderWidth; }
    void setBorderWidth(int width);

    // Compact mode
    bool compactMode() const { return mCompactMode; }
    void setCompactMode(bool compact);

    // Textless mode: render only the numeric value inside the field; label is expected to be drawn externally
    bool textlessMode() const { return mTextlessMode; }
    void setTextlessMode(bool enabled);

    static QColor getAccentColor();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void valueChanged(double value);

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

    HitRegion hitTest(const QPoint &pos) const;
    QRect getLeftButtonRect() const;
    QRect getCenterFieldRect() const;
    QRect getRightButtonRect() const;
    void increment();
    void decrement();
    void showInputField();
    void hideInputField();
    QString formatValue(double val) const;
    double clampValue(double val) const;

    // Data
    double mValue;
    double mMinimum;
    double mMaximum;
    double mSingleStep;
    int mDecimals;
    QString mLabel;

    // Styling
    QColor mAccentColor;
    QColor mBackgroundColor;
    QColor mTextColor;
    int mCornerRadius;
    int mButtonWidth;
    bool mCompactMode;
    bool mTextlessMode;
    bool mLegacySidePanels;
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
};

#endif // BLENDERSTYLEVALUEFIELD_H

