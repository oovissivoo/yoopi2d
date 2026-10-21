#ifndef COSMICEXPORTGIFPOPUP_H
#define COSMICEXPORTGIFPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>
#include <QSize>
#include <QString>

class CosmicWindowHeader;
class CosmicButton;
class CosmicInputField;
class CosmicCheckboxToggle;
class QLabel;
class QLineEdit;

/**
 * Dedicated GIF export popup.
 * Cards: Destination | Resolution | Frame Range | Options
 * Subclass of CosmicPopupWindow, window-modal.
 */
class CosmicExportGifPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicExportGifPopup(QWidget* parent);

    // Populate from editor state before exec()
    void setResolution(const QSize& size);
    void setDefaultRange(int startFrame, int endFrame, int fps);

    // Result accessors (valid after Accepted)
    QString filePath()     const;
    QSize   exportSize()   const;
    int     startFrame()   const;
    int     endFrame()     const;  ///< already clamped to 10-second cap
    bool    loop()         const;
    bool    transparency() const;

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onBrowse();
    void onRangeChanged();

private:
    void buildUI();
    void updateCapWarning();

    int mFps = 12;

    QPointer<CosmicWindowHeader>   mHeader;
    QPointer<QLineEdit>            mPathEdit;
    QPointer<CosmicInputField>     mWidthField;
    QPointer<CosmicInputField>     mHeightField;
    QPointer<CosmicInputField>     mStartField;
    QPointer<CosmicInputField>     mEndField;
    QPointer<QLabel>               mCapWarning;
    QPointer<CosmicCheckboxToggle> mTransparencyToggle;
    QPointer<CosmicCheckboxToggle> mLoopToggle;
    QPointer<CosmicButton>         mExportBtn;
    QPointer<CosmicButton>         mCancelBtn;
};

#endif // COSMICEXPORTGIFPOPUP_H
