#ifndef COSMICABOUTPOPUP_H
#define COSMICABOUTPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicWindowHeader;
class CosmicButton;
class QLabel;

// Redesigned About popup with Mac-style card layout.
class CosmicAboutPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicAboutPopup(QWidget* parent = nullptr);
    void init();

protected:
    void showEvent(QShowEvent* e) override;

private:
    void buildUI();

    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLabel>             mVersionLabel;
    QPointer<QLabel>             mSysInfoLabel;
    QPointer<CosmicButton>       mCopyBtn;
    QPointer<CosmicButton>       mCloseBtn;

    QStringList mDevText;
};

#endif // COSMICABOUTPOPUP_H
