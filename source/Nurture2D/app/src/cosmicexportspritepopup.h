#ifndef COSMICEXPORTSPRITEPOPUP_H
#define COSMICEXPORTSPRITEPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>
#include <QSize>
#include <QString>

class CosmicWindowHeader;
class CosmicButton;
class CosmicDropDown;
class CosmicInputField;
class CosmicCheckboxToggle;
class QLabel;
class QLineEdit;
class QButtonGroup;
class QAbstractButton;

class CosmicExportSpritePopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    enum class Layout { Horizontal = 0, Vertical = 1, Grid = 2 };

    explicit CosmicExportSpritePopup(QWidget* parent);

    void setResolution(const QSize& size);

    // Results
    QString destDir()      const;
    QString fileName()     const;
    QString exportFormat() const;
    QSize   frameSize()    const;
    Layout  spriteLayout() const;
    bool    transparency() const;

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onBrowseDest();

private:
    void buildUI();

    QPointer<CosmicWindowHeader> mHeader;

    QPointer<QLabel>             mDestLabel;
    QPointer<QLineEdit>          mFileNameEdit;

    QPointer<CosmicDropDown>     mFormatDrop;
    QPointer<CosmicInputField>   mWidthField;
    QPointer<CosmicInputField>   mHeightField;
    QPointer<CosmicCheckboxToggle> mTransparencyToggle;

    // Vertical layout selector buttons
    QPointer<QAbstractButton>    mBtnHorizontal;
    QPointer<QAbstractButton>    mBtnVertical;
    QPointer<QAbstractButton>    mBtnGrid;
    QButtonGroup*                mLayoutGroup = nullptr;

    QPointer<CosmicButton>       mExportBtn;
    QPointer<CosmicButton>       mCancelBtn;
};

#endif // COSMICEXPORTSPRITEPOPUP_H
