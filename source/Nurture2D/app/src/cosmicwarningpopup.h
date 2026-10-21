#ifndef COSMICWARNINGPOPUP_H
#define COSMICWARNINGPOPUP_H

#include "cosmicpopupwindow.h"

#include <QPointer>

class CosmicButton;
class QLabel;

class CosmicWarningPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicWarningPopup(QWidget* parent = nullptr);

    void setTitleText(const QString& title);
    void setBodyText(const QString& body);

    void setConfirmText(const QString& text);

    void setWarningIconEnabled(bool on);
    bool warningIconEnabled() const { return mWarningIconEnabled; }
    void setConfirmGreen(bool green);
    // Adds a darker inset card below the body text, good for code/format examples
    void setExamplesText(const QString& text);

protected:
    void showEvent(QShowEvent* e) override;

private:
    void rebuildWarningIcon();

    bool mWarningIconEnabled = true;

    QPointer<QLabel> mIcon;
    QPointer<QLabel> mBody;
    QPointer<QWidget> mExamplesCard;
    QPointer<QLabel> mExamples;
    QPointer<CosmicButton> mConfirm;
};

#endif // COSMICWARNINGPOPUP_H
