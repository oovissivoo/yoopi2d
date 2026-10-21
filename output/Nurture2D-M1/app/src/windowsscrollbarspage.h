#ifndef WINDOWSSCROLLBARSPAGE_H
#define WINDOWSSCROLLBARSPAGE_H

#include <QWidget>
#include <QPointer>
#include "preferencesdef.h"

class PreferenceManager;
class QPropertyAnimation;
class QLabel;

// Icon-only option tile: shows SVG, active = slightly larger + accent underline glow.
class WindowOptionTile : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight)

public:
    explicit WindowOptionTile(const QString& iconPath, int value, QWidget* parent = nullptr);

    int   value()    const { return mValue; }
    bool  isActive() const { return mActive; }
    void  setActive(bool on, bool animate = true);
    void  setTileSize(int w, int h);  // override default 130×44

    qreal highlight() const { return mHighlight; }
    void  setHighlight(qreal v);

signals:
    void clicked(int value);

protected:
    void mousePressEvent(QMouseEvent*) override;
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override;

private:
    QString  mIconPath;
    int      mValue     = 0;
    bool     mActive    = false;
    qreal    mHighlight = 0.0;
    int      mTileW     = 130;
    int      mTileH     = 44;
    QPointer<QPropertyAnimation> mAnim;
};

class WindowsScrollbarsPage : public QWidget
{
    Q_OBJECT

public:
    explicit WindowsScrollbarsPage(QWidget* parent = nullptr);
    ~WindowsScrollbarsPage() override = default;

    void setManager(PreferenceManager* m) { mManager = m; }

public slots:
    void updateValues();

protected:
    void changeEvent(QEvent* e) override;

private slots:
    void onCloseStyleChanged(int value);
    void onClosePositionChanged(int value);

private:
    void retranslatePageStrings();
    QWidget* makeCard(const QString& title, QWidget* content);

    PreferenceManager* mManager = nullptr;

    WindowOptionTile* mStyleDefault = nullptr;
    WindowOptionTile* mStyleMac     = nullptr;
    WindowOptionTile* mPosLeft      = nullptr;
    WindowOptionTile* mPosRight     = nullptr;

    // Labels that need retranslation on language change
    QLabel* mCardTitleLabel      = nullptr;
    QLabel* mButtonTypeLabel     = nullptr;
    QLabel* mButtonPositionLabel = nullptr;
};

#endif // WINDOWSSCROLLBARSPAGE_H
