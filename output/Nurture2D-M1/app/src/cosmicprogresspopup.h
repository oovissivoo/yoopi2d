#ifndef COSMICPROGRESSPOPUP_H
#define COSMICPROGRESSPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>

class QLabel;
class CosmicButton;
class CosmicProgressBar;

// General-purpose progress popup matching the app's visual language.
// - No cancel button by default (non-interruptible operations like save/open).
// - Call setCancelEnabled(true) to show a Cancel button for abortable operations.
// - wasCanceled() returns true if the user clicked Cancel.
// - API mirrors QProgressDialog so callers can swap with minimal changes.
class CosmicProgressPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicProgressPopup(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setStatus(const QString& status);
    void setRange(int minimum, int maximum);
    void setValue(int value);
    void setCancelEnabled(bool on);

    bool wasCanceled() const { return mCanceled; }
    int  maximum() const;
    int  minimum() const;

    // Show centered over the parent window immediately (no animation delay).
    void showCentered();

    // close() alias for callers that use progress.close()
    void close() { QDialog::reject(); }

signals:
    void canceled();

protected:
    void showEvent(QShowEvent* e) override;

private:
    QPointer<QLabel>           mTitleLabel;
    QPointer<QLabel>           mStatusLabel;
    QPointer<CosmicProgressBar> mBar;
    QPointer<CosmicButton>     mCancelBtn;

    bool mCanceled = false;
};

#endif // COSMICPROGRESSPOPUP_H
