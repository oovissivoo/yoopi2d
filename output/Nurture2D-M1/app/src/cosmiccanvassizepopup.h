#ifndef COSMICCANVASSIZEPOPUP_H
#define COSMICCANVASSIZEPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicWindowHeader;
class CosmicInputField;

// Modal popup for setting canvas size (width x height).
// Summons with the same scale+fade animation as CosmicWarningPopup.
class CosmicCanvasSizePopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicCanvasSizePopup(QWidget* parent = nullptr);

    void setInitialSize(int width, int height);

    int canvasWidth() const;
    int canvasHeight() const;

protected:
    void showEvent(QShowEvent* e) override;

private:
    QPointer<CosmicWindowHeader> mHeader;
    QPointer<CosmicInputField>   mWidthField;
    QPointer<CosmicInputField>   mHeightField;
};

#endif // COSMICCANVASSIZEPOPUP_H
