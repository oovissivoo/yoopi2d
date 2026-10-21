#ifndef COSMICAUTOSAVEPOPUP_H
#define COSMICAUTOSAVEPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicButton;
class CosmicWindowHeader;
class QLabel;
class QKeyEvent;

// Autosave reminder popup shown when autosave fires on an unsaved project.
//
// Result codes (returned by exec()):
//   QDialog::Accepted  → user chose "Save Now"
//   CosmicAutoSavePopup::Snooze5  → snooze 5 minutes
//   CosmicAutoSavePopup::Snooze15 → snooze 15 minutes
//   QDialog::Rejected  → header close (treated as Snooze15)
class CosmicAutoSavePopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    static constexpr int Snooze5  = 2;
    static constexpr int Snooze15 = 3;

    explicit CosmicAutoSavePopup(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLabel>             mIcon;
    QPointer<QLabel>             mBody;
    QPointer<CosmicButton>       mSaveNow;
    QPointer<CosmicButton>       mSnooze5;
    QPointer<CosmicButton>       mSnooze15;
};

#endif // COSMICAUTOSAVEPOPUP_H
