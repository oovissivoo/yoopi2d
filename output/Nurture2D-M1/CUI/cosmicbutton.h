#ifndef COSMICBUTTON_H
#define COSMICBUTTON_H

#include <QPushButton>
#include <QColor>

// CosmicButton - borderless icon/text button with hover / press background.
//
// Visual rules:
// - Normal: no visible background (fully transparent), no border.
// - Hover: background-color = #4d4d4dff.
// - Pressed (while held): background-color = #40555cff.
// - Text/icon color uses the global dock text color (#b3b3b3ff) via palette.
//
// Behavior:
// - Standard QPushButton semantics for click/press/release.
// - Background color updates instantly on hover enter/leave and press/release.
class CosmicButton : public QPushButton
{
    Q_OBJECT

public:
    explicit CosmicButton(QWidget* parent = nullptr);
    explicit CosmicButton(const QString& text, QWidget* parent = nullptr);

    void setTextColor(const QColor& color);

    void setBackgroundColors(const QColor& normal,
                             const QColor& hover,
                             const QColor& pressed);

    void setBordered(bool bordered);
    void setAccentColor(const QColor& color);
    void setTextAlignment(Qt::Alignment alignment);
    // Override the corner radius (default: uses CosmicUiMetrics::cornerRadius())
    void setCornerRadius(int px);

    // Selective corner rounding. By default all four corners use the standard radius.
    // Pass false for any corner to make it square (0 radius).
    void setCornerRadii(bool topLeft, bool topRight, bool bottomLeft, bool bottomRight);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void refreshAccentFromPaletteIfNeeded();
    QColor mNormalBg;
    QColor mHoverBg;
    QColor mPressedBg;
    QColor mTextColor;
    bool   mTextColorOverridden = false;

    bool mBordered = false;
    QColor mAccentColor;
    bool mAccentColorOverridden = false;

    bool mHover = false;
    bool mPressed = false;

    // Corner rounding flags (all true by default)
    bool mRadiusTL = true;
    bool mRadiusTR = true;
    bool mRadiusBL = true;
    bool mRadiusBR = true;

    Qt::Alignment mTextAlignment = Qt::AlignHCenter;
    int mCornerRadiusOverride = -1; // -1 = use CosmicUiMetrics::cornerRadius()
};

#endif // COSMICBUTTON_H
