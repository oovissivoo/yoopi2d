#ifndef BLENDERSTYLETOGGLE_H
#define BLENDERSTYLETOGGLE_H

#include <QWidget>
#include <QString>
#include <QMouseEvent>
#include <QPaintEvent>

/**
 * BlenderStyleToggle - A toggleable button widget matching Blender's UI aesthetic
 * 
 * This widget displays a single button that can be toggled on/off with:
 * - Rounded corners on the left and right edges
 * - Highlighted state when toggled on (accent color)
 * - Hover feedback
 * - Blender-style appearance
 * 
 * Usage:
 *   auto* toggle = new BlenderStyleToggle(this);
 *   toggle->setText("Draw Behind");
 *   connect(toggle, &BlenderStyleToggle::toggled, ...);
 */
class BlenderStyleToggle : public QWidget
{
    Q_OBJECT

public:
    explicit BlenderStyleToggle(QWidget *parent = nullptr);
    ~BlenderStyleToggle();
    
    // Get/Set checked state
    bool isChecked() const { return mChecked; }
    void setChecked(bool checked);
    
    // Get/Set text
    QString text() const { return mText; }
    void setText(const QString &text);
    
    // Get/Set icon (optional)
    void setIcon(const QPixmap &icon);
    void clearIcon();
    
    // Enable/disable the toggle
    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }
    
    // Styling
    void setAccentColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setSelectedTextColor(const QColor &color);
    void setCornerRadius(int radius);
    
    // Borderless visual mode: when off, draw no background/border; when on, use accent background
    void setBorderlessMode(bool enabled);
    bool isBorderlessMode() const { return mBorderlessMode; }
    
    // Corner rounding control for merging adjacent toggles
    enum CornerMode {
        AllCorners,      // Round all corners (default)
        LeftCorners,     // Only round left corners
        RightCorners,    // Only round right corners
        NoCorners        // No rounded corners
    };
    void setCornerMode(CornerMode mode);
    
    // Border control for seamless merging
    enum BorderSide {
        BorderNone   = 0x0,
        BorderLeft   = 0x1,
        BorderTop    = 0x2,
        BorderRight  = 0x4,
        BorderBottom = 0x8,
        BorderAll    = BorderLeft | BorderTop | BorderRight | BorderBottom
    };
    Q_DECLARE_FLAGS(BorderSides, BorderSide)
    void setBorderSides(BorderSides sides);
    
    // Get the global accent color from preferences
    static QColor getAccentColor();

signals:
    void toggled(bool checked);
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    // State
    bool mChecked;
    bool mHovered;
    bool mPressed;
    bool mEnabled;
    
    // Content
    QString mText;
    QPixmap mIcon;
    
    // Styling
    QColor mAccentColor;
    QColor mBackgroundColor;
    QColor mTextColor;
    QColor mSelectedTextColor;
    int mCornerRadius;
    CornerMode mCornerMode;
    BorderSides mBorderSides;
    bool mBorderlessMode;
    
    void initializeColors();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BlenderStyleToggle::BorderSides)

#endif // BLENDERSTYLETOGGLE_H
