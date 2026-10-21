#ifndef COSMICMENUBAR_H
#define COSMICMENUBAR_H

#include <QWidget>
#include <QVector>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QPixmap>
#include <functional>

class CosmicMenuWindow;

class CosmicMenuBar : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicMenuBar(QWidget* parent = nullptr);
    ~CosmicMenuBar() override;

    // label is the display string; labelFactory (optional) is called on
    // retranslation to refresh the label. Pass a lambda returning tr("…").
    void addEntry(const QString& label, CosmicMenuWindow* menu,
                  std::function<QString()> labelFactory = {});
    void removeEntry(const QString& label);
    int  entryCount() const;
    void setEntryEnabled(const QString& label, bool enabled);
    int  menuBarHeight() const;

    // Decorative logo icon shown as the first item (non-interactive, like Blender)
    void setLogoIcon(const QPixmap& pixmap);

    // Version label drawn at the far-right edge of the bar (e.g. "2026")
    void setVersionLabel(const QString& label);

    // Force-close every menu and submenu immediately
    void forceCloseAll();

    // Re-evaluate all label factories and repaint (call after language change)
    void retranslate();

    // Called by CosmicMenuWindow keyboard nav
    void openEntryAt(int index);
    // Alias kept for CosmicMenuWindow back-compat
    void closeActiveMenu();

signals:
    void entryActivated(const QString& label);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void enterEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent* e) override;
    QSize sizeHint() const override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    struct MenuEntry {
        QString                  label;
        std::function<QString()> labelFactory; // re-evaluated on retranslate()
        CosmicMenuWindow*        menu    = nullptr;
        bool                     enabled = true;
        QRect                    hitRect;
    };

    void computeHitRects();
    int  entryAtPos(const QPoint& pos) const;
    bool isInsideMenuSystem(const QPoint& globalPos) const;
    void startTracking();
    void stopTracking();

    QVector<MenuEntry> mEntries;
    int  mHoverIndex        = -1;
    int  mActiveIndex       = -1;
    bool mTrackingInstalled = false;
    bool mClosing           = false;
    QTimer* mPollTimer      = nullptr;

    QPixmap mLogoPixmap;
    int     mLogoAreaWidth  = 0;  // reserved width before first menu entry
    QString mVersionLabel;        // drawn at the far-right edge in CosmicGreen

    static constexpr int kVerticalPadding = 4;
    static constexpr int kEntryHPad       = 12;
    static constexpr int kLogoHPad        = 8;  // horizontal padding around logo

    friend class CosmicMenuWindow;
};

#endif // COSMICMENUBAR_H
