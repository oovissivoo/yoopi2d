#ifndef COSMICCAMERAPROPERTIESPOPUP_H
#define COSMICCAMERAPROPERTIESPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicWindowHeader;
class CosmicButton;
class CosmicInputField;
class QLineEdit;

// Redesigned Camera Properties popup.
// Cards: Identity | Canvas Size
class CosmicCameraPropertiesPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicCameraPropertiesPopup(const QString& name, int width, int height,
                                         QWidget* parent = nullptr);

    QString cameraName() const;
    int     cameraWidth() const;
    int     cameraHeight() const;

protected:
    void showEvent(QShowEvent* e) override;

private:
    void buildUI(const QString& name, int width, int height);

    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLineEdit>          mNameEdit;
    QPointer<CosmicInputField>   mWidthField;
    QPointer<CosmicInputField>   mHeightField;
};

#endif // COSMICCAMERAPROPERTIESPOPUP_H
