#ifndef COSMICCONFIRMPOPUP_H
#define COSMICCONFIRMPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class CosmicButton;
class CosmicWindowHeader;
class QLabel;
class QKeyEvent;

// Two-button confirm dialog matching the CosmicWarningPopup visual language.
// Layout: header (with close btn) | body text | [Cancel] [Confirm]
// - Confirm = CosmicGreen, default action (Enter)
// - Cancel  = neutral, also triggered by Escape or header close btn
// - Left/Right arrow keys navigate between the two buttons (wraps around)
//
// Usage:
//   CosmicConfirmPopup popup(parent);
//   popup.setBodyText("Are you sure?");
//   popup.setConfirmText("Delete");
//   if (popup.exec() == QDialog::Accepted) { ... }
class CosmicConfirmPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicConfirmPopup(QWidget* parent = nullptr);

    void setBodyText(const QString& body);
    void setConfirmText(const QString& text);
    void setCancelText(const QString& text);
    // Kept for API compatibility with existing callers; title is not shown in the new layout.
    void setTitleText(const QString&) {}

protected:
    void showEvent(QShowEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void focusConfirm();
    void focusCancel();

    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLabel>             mIcon;
    QPointer<QLabel>             mBody;
    QPointer<CosmicButton>       mConfirm;
    QPointer<CosmicButton>       mCancel;

    bool mConfirmFocused = true; // confirm is default-focused
};

#endif // COSMICCONFIRMPOPUP_H
