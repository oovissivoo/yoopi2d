#ifndef COSMICSAVEPROGRESSPOPUP_H
#define COSMICSAVEPROGRESSPOPUP_H

#include "cosmicpopupwindow.h"

class QLabel;
class CosmicProgressBar;

class CosmicSaveProgressPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicSaveProgressPopup(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setStatus(const QString& status);
    void setRange(int minimum, int maximum);
    void setValue(int value);
    void showCentered();

protected:
    void showEvent(QShowEvent* e) override;

private:
    QLabel*           mTitleLabel  = nullptr;
    QLabel*           mStatusLabel = nullptr;
    CosmicProgressBar* mBar        = nullptr;
};

#endif // COSMICSAVEPROGRESSPOPUP_H
