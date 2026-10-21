#ifndef BLENDERSTYLEBUTTON_H
#define BLENDERSTYLEBUTTON_H

#include <QWidget>
#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QTimer>

/**
 * BlenderStyleButton - A flexible button widget matching Blender's UI aesthetic
 * 
 * This widget displays a button with:
 * Visual States:
 * - Borderless: Shows only icon, rounded highlight on hover
 * - Bordered: Normal button display with rounded edges, proper highlighting
 * - On click: Temporarily flashes application accent color
 * 
 * Layout States:
 * - Single: Renders as a standalone button with all corners rounded
 * - Connected: Allows visual merging with adjacent buttons (like BlenderStyleComboBox)
 *   - Merged areas are seamless (no border between connected buttons)
 *   - Outer edges of merged groups have rounded corners
 * 
 * Usage:
 *   auto* button = new BlenderStyleButton(this);
 *   button->setIcon(QPixmap(":/icon.svg"));
 *   button->setBorderless(true);  // Borderless mode
 *   button->setCornerMode(BlenderStyleButton::LeftCorners); // For merging
 *   connect(button, &BlenderStyleButton::clicked, ...);
 */
class BlenderStyleButton : public QWidget
{
    Q_OBJECT

public:
    explicit BlenderStyleButton(QWidget *parent = nullptr);
    ~BlenderStyleButton();
    
    // Visual state: borderless or bordered
    bool isBorderless() const { return mBorderless; }
    void setBorderless(bool borderless);
    
    // Icon
    void setIcon(const QPixmap &icon);
    void setIcon(const QIcon &icon, const QSize &size = QSize(16, 16));
    QPixmap icon() const { return mIcon; }
    
    // Text (optional, displayed with icon if both set)
    QString text() const { return mText; }
    void setText(const QString &text);
    
    // Checkable state (for toggle buttons)
    bool isCheckable() const { return mCheckable; }
    void setCheckable(bool checkable);
    
    bool isChecked() const { return mChecked; }
    void setChecked(bool checked);
    
    // Enable/disable
    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }
    
    // Styling
    void setAccentColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setCornerRadius(int radius);
    
    // Corner rounding control for merging adjacent buttons
    enum CornerMode {
        AllCorners,      // Round all corners (default - single mode)
        LeftCorners,     // Only round left corners
        RightCorners,    // Only round right corners
        NoCorners        // No rounded corners (middle of merged group)
    };
    void setCornerMode(CornerMode mode);
    CornerMode cornerMode() const { return mCornerMode; }
    
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
    BorderSides borderSides() const { return mBorderSides; }
    
    // Get the global accent color from preferences
    static QColor getAccentColor();

signals:
    void clicked();
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private slots:
    void onFlashTimeout();

private:
    // Visual state
    bool mBorderless;
    bool mHovered;
    bool mPressed;
    bool mEnabled;
    bool mFlashing;
    
    // Checkable state
    bool mCheckable;
    bool mChecked;
    
    // Content
    QPixmap mIcon;
    QString mText;
    
    // Styling
    QColor mAccentColor;
    QColor mBackgroundColor;
    QColor mTextColor;
    int mCornerRadius;
    CornerMode mCornerMode;
    BorderSides mBorderSides;
    
    // Flash timer for click feedback
    QTimer* mFlashTimer;
    int mFlashDuration;
    
    void initializeColors();
    void startFlash();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BlenderStyleButton::BorderSides)

#endif // BLENDERSTYLEBUTTON_H
