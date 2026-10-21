#ifndef COSMICEXPORTIMAGEPOPUP_H
#define COSMICEXPORTIMAGEPOPUP_H

#include "cosmicpopupwindow.h"
#include "filetype.h"
#include <QPointer>
#include <QStringList>

class CosmicWindowHeader;
class CosmicButton;
class CosmicDropDown;
class CosmicCheckboxToggle;
class QLabel;
class QLineEdit;

// Redesigned Export Image / Export Image Sequence popup.
// Mac-style layout: header → file card → options card → action row.
// All controls use Cosmic UI widgets.
class CosmicExportImagePopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    enum class ExportSpecificMode { Keyframes = 0, CellsOnly = 1 };

    explicit CosmicExportImagePopup(QWidget* parent, FileType fileType);

    // Results
    QString filePath() const;
    QString exportFormat() const;
    bool    transparency() const;
    bool    isExportSpecificEnabled() const;
    ExportSpecificMode exportSpecificMode() const;

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onBrowse();
    void onFormatChanged(int index);

private:
    void buildUI();
    void updateTransparencyState();

    FileType mFileType;

    QPointer<CosmicWindowHeader>   mHeader;
    QPointer<QLineEdit>            mPathEdit;
    QPointer<CosmicDropDown>       mFormatDrop;
    QPointer<CosmicCheckboxToggle> mTransparencyToggle;
    QPointer<CosmicCheckboxToggle> mExportSpecificToggle;
    QPointer<CosmicDropDown>       mSpecificModeDrop;
    QPointer<QWidget>              mSpecificModeRow;
    QPointer<CosmicButton>         mExportBtn;
    QPointer<CosmicButton>         mCancelBtn;
};

#endif // COSMICEXPORTIMAGEPOPUP_H
