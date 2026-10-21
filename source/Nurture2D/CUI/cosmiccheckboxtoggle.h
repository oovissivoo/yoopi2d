#ifndef COSMICCHECKBOXTOGGLE_H
#define COSMICCHECKBOXTOGGLE_H

#include <QAbstractButton>
#include <QColor>
#include <QIcon>

// CosmicCheckboxToggle
//
// A two-state toggle control with two high-level modes:
//  - Custom: icon/text combinations with optional background (bordered / borderless).
//  - DefaultCheckbox: square checkbox indicator with a checkmark icon when enabled.
//
// Color rules:
//  - Base background (bordered, custom mode): #4d4d4dff.
//  - Press color (while mouse is pressed): #999999ff.
//  - Enabled (checked) background: accent color (default #40555cff).
//  - DefaultCheckbox square color: #4d4d4dff (unchecked), press color #999999ff, checked uses check icon.
//
// Content rules in Custom mode:
//  - Text and/or icon can be shown (showText / showIcon flags).
//  - Borderless: no background when unchecked; only accent/press backgrounds are drawn.
//  - Bordered: base background is #4d4d4dff.
class CosmicCheckboxToggle : public QAbstractButton
{
    Q_OBJECT

public:
    enum Mode {
        Custom,
        DefaultCheckbox
    };

    enum ScaleMode {
        ScaleNormal,   // original sizing
        ScaleBigBox,   // DefaultCheckbox: larger box/background, normal icon
        ScaleBig       // Larger overall control (icon, bg, press) in all modes
    };

    explicit CosmicCheckboxToggle(QWidget* parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return mMode; }

    void setScaleMode(ScaleMode mode);
    ScaleMode scaleMode() const { return mScaleMode; }

    // Custom mode configuration
    void setBordered(bool bordered);
    bool isBordered() const { return mBordered; }

    void setShowText(bool show);
    bool showText() const { return mShowText; }

    void setShowIcon(bool show);
    bool showIcon() const { return mShowIcon; }

    void setAccentColor(const QColor& color);
    QColor accentColor() const { return mAccentColor; }

    void setBaseBackgroundColor(const QColor& color);
    void clearBaseBackgroundColor();

    void setTextColor(const QColor& color);
    void clearTextColor();

    void setCheckedTextColor(const QColor& color);
    void clearCheckedTextColor();

    void setBackgroundEnabled(bool enabled);
    bool backgroundEnabled() const { return mBackgroundEnabled; }

    void setHoverHighlightEnabled(bool enabled);
    bool hoverHighlightEnabled() const { return mHoverHighlightEnabled; }

    // When enabled, the icon is painted at reduced opacity while the toggle is unchecked.
    void setIconDimWhenUnchecked(bool dim);
    bool iconDimWhenUnchecked() const { return mIconDimWhenUnchecked; }

    // Default checkbox mode configuration
    void setCheckIcon(const QIcon& icon);
    QIcon checkIcon() const { return mCheckIcon; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;
    QSize sizeHint() const override;

private:
    void refreshAccentFromPaletteIfNeeded();
    void paintCustom(QPainter& p);
    void paintDefaultCheckbox(QPainter& p);

    Mode mMode = Custom;
    ScaleMode mScaleMode = ScaleNormal;

    // Custom mode flags
    bool mBordered = false;      // false => borderless (no base bg when unchecked)
    bool mShowText = true;
    bool mShowIcon = true;

    QColor mAccentColor;
    QIcon mCheckIcon;

     bool mAccentColorOverridden = false;

    bool mHover = false;
    bool mPressed = false;
    bool mBackgroundEnabled = true;
    bool mHoverHighlightEnabled = true;
    bool mIconDimWhenUnchecked = false;

    bool mBaseBgOverridden = false;
    QColor mBaseBg;

    bool mTextColorOverridden = false;
    QColor mTextColor;

    bool mCheckedTextColorOverridden = false;
    QColor mCheckedTextColor;
};

#endif // COSMICCHECKBOXTOGGLE_H
