#ifndef BLENDERSTYLEDROPDOWN_H
#define BLENDERSTYLEDROPDOWN_H

#include <QWidget>
#include <QColor>
#include <QStringList>
#include <QFrame>

class QListWidget;
class PopupFrame;
class QCloseEvent;
class QKeyEvent;
class QWheelEvent;

// Custom popup frame that notifies when it closes
class PopupFrame : public QFrame
{
    Q_OBJECT

public:
    explicit PopupFrame(QWidget* parent = nullptr);

signals:
    void popupClosed();

protected:
    void closeEvent(QCloseEvent* event) override;
};

/**
 * BlenderStyleDropDown - A Blender-like dropdown selector widget.
 *
 * Visuals:
 * - Rounded rectangular capsule
 * - Current option label on the left, down-arrow indicator on the right
 * - Popup list with accent-colored rounded hover/selection highlight
 */
class BlenderStyleDropDown : public QWidget
{
    Q_OBJECT

public:
    explicit BlenderStyleDropDown(QWidget* parent = nullptr);
    ~BlenderStyleDropDown();

    // Data
    void setOptions(const QStringList& options);
    QStringList options() const { return mOptions; }

    int currentIndex() const { return mCurrentIndex; }
    void setCurrentIndex(int index);
    QString currentText() const;

    // Styling
    void setAccentColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setSelectedTextColor(const QColor& color);
    void setCornerRadius(int radius);

    // Shared default accent color
    static QColor getAccentColor();

signals:
    void currentIndexChanged(int index);
    void activated(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void ensurePopup();
    void showPopup();
    void hidePopup();
    QRect arrowRect() const;
    QRect textRect() const;

    // Data
    QStringList mOptions;
    int mCurrentIndex;

    // Styling
    QColor mAccentColor;
    QColor mBackgroundColor;
    QColor mTextColor;
    QColor mSelectedTextColor;
    int mCornerRadius;

    // UI state
    bool mHover;
    bool mPopupVisible;

    // Popup
    PopupFrame* mPopupFrame;      // Qt::Popup container
    QListWidget* mListWidget; // List of options inside popup
};

#endif // BLENDERSTYLEDROPDOWN_H


