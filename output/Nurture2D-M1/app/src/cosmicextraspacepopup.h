#ifndef COSMICEXTRASPACEPOPUP_H
#define COSMICEXTRASPACEPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicWindowHeader;
class CosmicInputField;
class CosmicDropDown;
class PreferenceManager;

// Popup for configuring canvas extra space (style + scale/manual dimensions).
class CosmicExtraSpacePopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicExtraSpacePopup(PreferenceManager* prefs, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* e) override;

private:
    void syncFromPrefs();

    PreferenceManager* mPrefs = nullptr;

    QPointer<CosmicWindowHeader> mHeader;
    QPointer<CosmicDropDown>     mStyleDropdown;
    QPointer<CosmicInputField>   mScaleField;   // shown in Auto mode
    QPointer<CosmicInputField>   mWField;       // shown in Manual mode
    QPointer<CosmicInputField>   mHField;       // shown in Manual mode
    QPointer<QWidget>            mAutoRow;
    QPointer<QWidget>            mManualRow;
};

#endif // COSMICEXTRASPACEPOPUP_H
