#ifndef COSMICDROPDOWN_H
#define COSMICDROPDOWN_H

#include <QWidget>
#include <QColor>
#include <QStringList>
#include <QFrame>

class QListWidget;
class QMenu;
class QCloseEvent;
class QKeyEvent;
class QWheelEvent;

// Popup frame used by CosmicDropDown to notify when it closes
class CosmicPopupFrame : public QFrame
{
    Q_OBJECT

public:
    explicit CosmicPopupFrame(QWidget* parent = nullptr);

    void setCornerRadius(int r);
    void setBackgroundColor(const QColor& c);
    void setTopClickCloseHeight(int h);

signals:
    void popupClosed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    int mCornerRadius = 0;
    QColor mBackgroundColor;
    int mTopClickCloseHeight = 0;
};

/**
 * CosmicDropDown - Nurture2D cosmic UI dropdown selector.
 *
 * Visual design:
 * - Rectangular background with color #1f1f1f (1f1f1fff)
 * - Label text on the left, using color #b2b2b2 (b2b2b2ff)
 * - SVG dropdown icon on the right from :/Interaction/DropDownIcon.svg
 * - Popup list where the active/hovered item highlight uses #40555c (40555cff)
 */
class CosmicDropDown : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicDropDown(QWidget* parent = nullptr);
    ~CosmicDropDown();

    // Data
    void setOptions(const QStringList& options);
    QStringList options() const { return mOptions; }

    int currentIndex() const { return mCurrentIndex; }
    void setCurrentIndex(int index);
    QString currentText() const;
    bool isPopupVisible() const { return mPopupVisible; }

    // Styling (kept for API compatibility, but cosmic theme uses fixed colors by default)
    void setAccentColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setSelectedTextColor(const QColor& color);
    void setCornerRadius(int radius);

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
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void ensurePopup();
    void showPopup();
    void hidePopup();
    QMenu* findAncestorMenu() const;
    void showMenuPopup(QMenu* parentMenu);
    QRect arrowRect() const;
    QRect textRect() const;

    // Data
    QStringList mOptions;
    int mCurrentIndex;

    // Styling
    QColor mAccentColor;        // currently not used for popup highlight (fixed cosmic color)
    QColor mBackgroundColor;
    QColor mTextColor;
    QColor mSelectedTextColor;
    int mCornerRadius;
    QColor mBorderColor;
    int mBorderWidth;

    // UI state
    bool mHover;
    bool mPopupVisible;

    // Popup
    CosmicPopupFrame* mPopupFrame; // Qt::Popup container
    QListWidget* mListWidget;      // List of options inside popup
    QMenu* mMenuPopup;             // Submenu popup when embedded inside QMenu
};

#endif // COSMICDROPDOWN_H
