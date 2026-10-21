#ifndef COSMICINPUTPOPUP_H
#define COSMICINPUTPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicButton;
class CosmicWindowHeader;
class QLabel;
class QLineEdit;

// Modal popup with a single-line text input.
// Usage:
//   CosmicInputPopup popup(parent);
//   popup.setTitleText("Rename Color");
//   popup.setPlaceholderText("Color name");
//   popup.setInitialText("Red");
//   if (popup.exec() == QDialog::Accepted)
//       QString name = popup.text();
class CosmicInputPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicInputPopup(QWidget* parent = nullptr);

    void setTitleText(const QString& title);
    void setBodyText(const QString& body);       // optional description line
    void setInitialText(const QString& text);
    void setPlaceholderText(const QString& text);
    void setConfirmText(const QString& text);
    void setCancelText(const QString& text);

    QString text() const;

protected:
    void showEvent(QShowEvent* e) override;

private:
    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLabel>             mTitle;
    QPointer<QLabel>             mBody;
    QPointer<QLineEdit>          mEdit;
    QPointer<CosmicButton>       mConfirm;
    QPointer<CosmicButton>       mCancel;
};

#endif // COSMICINPUTPOPUP_H
