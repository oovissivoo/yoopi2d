#ifndef BLENDERSTYLEMODALPOPUP_H
#define BLENDERSTYLEMODALPOPUP_H

#include <QDialog>
#include <QPaintEvent>
#include <QTimer>

/**
 * BlenderStyleModalPopup - A modal popup with glowing accent-colored border
 * 
 * Features:
 * - Bold glowing outline using accent color
 * - Frameless but with custom border
 * - Stays open while active
 * - Blocks interaction with rest of app (but not the toggle that controls it)
 */
class BlenderStyleModalPopup : public QDialog
{
    Q_OBJECT

public:
    explicit BlenderStyleModalPopup(QWidget *parent = nullptr);
    ~BlenderStyleModalPopup();
    
    // Set the accent color for the glow
    void setAccentColor(const QColor &color);
    
    // Set glow intensity (0.0 - 1.0)
    void setGlowIntensity(qreal intensity);
    
    // Set border width
    void setBorderWidth(int width);
    
    // Set the controlling toggle (allows interaction with this widget)
    void setControllingToggle(QWidget* toggle);
    
    // Get the global accent color
    static QColor getAccentColor();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void updateGlow();

private:
    QColor mAccentColor;
    qreal mGlowIntensity;
    int mBorderWidth;
    QTimer* mGlowTimer;
    qreal mGlowPhase; // For pulsing animation
    QWidget* mControllingToggle; // The toggle that controls this popup
};

#endif // BLENDERSTYLEMODALPOPUP_H

