#ifndef COSMICWINDOWHEADER_H
#define COSMICWINDOWHEADER_H

#include <QWidget>
#include <QPointer>

class CosmicButton;

class CosmicWindowHeader : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicWindowHeader(QWidget* parent = nullptr);

    void setCloseOnLeft(bool on);
    void setCloseStyle(int style); // 0 = default X icon, 1 = mac red circle

    // Apply position + style to every live CosmicWindowHeader in the app.
    static void applyStyleToAll(int style, bool closeOnLeft);

signals:
    void closeRequested();

private:
    void rebuildLayout();
    void updateIcon();

    bool mCloseOnLeft = true;
    int  mCloseStyle  = 0;
    QPointer<CosmicButton> mCloseBtn;
};

#endif // COSMICWINDOWHEADER_H
