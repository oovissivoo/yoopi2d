#ifndef COSMICEXPORTMOVIEPOPUP_H
#define COSMICEXPORTMOVIEPOPUP_H

#include "cosmicpopupwindow.h"
#include "filetype.h"
#include <QPointer>
#include <QSize>
#include <QString>
#include <QStringList>
#include <utility>
#include <vector>

class CosmicWindowHeader;
class CosmicButton;
class CosmicDropDown;
class CosmicInputField;
class CosmicCheckboxToggle;
class QLabel;
class QLineEdit;

// Redesigned Export Movie / Export GIF popup.
// Cards: Destination | Camera | Resolution | Frame Range | Options
class CosmicExportMoviePopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicExportMoviePopup(QWidget* parent, FileType fileType);

    void setCamerasInfo(const std::vector<std::pair<QString, QSize>>& cameras);
    void setDefaultRange(int startFrame, int endFrame, int endFrameWithSounds);

    // Results
    QString filePath() const;
    QString selectedCameraName() const;
    QSize   exportSize() const;
    int     startFrame() const;
    int     endFrame() const;
    bool    loop() const;
    bool    transparency() const;

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onBrowse();
    void onCameraChanged(int index);
    void onFilePathChanged();
    void validateResolution();

private:
    void buildUI();
    void updateLoopState();
    void updateTransparencyState();

    FileType mFileType;
    int mEndFrame = 0;
    int mEndFrameWithSounds = 0;
    std::vector<std::pair<QString, QSize>> mCameraData;

    QPointer<CosmicWindowHeader>   mHeader;
    QPointer<QLineEdit>            mPathEdit;
    QPointer<CosmicDropDown>       mCameraDrop;
    QPointer<CosmicInputField>     mWidthField;
    QPointer<CosmicInputField>     mHeightField;
    QPointer<QLabel>               mResWarning;
    QPointer<CosmicInputField>     mStartField;
    QPointer<CosmicInputField>     mEndField;
    QPointer<CosmicCheckboxToggle> mIncludeSoundsToggle;
    QPointer<CosmicCheckboxToggle> mLoopToggle;
    QPointer<CosmicCheckboxToggle> mTransparencyToggle;
    QPointer<CosmicButton>         mExportBtn;
    QPointer<CosmicButton>         mCancelBtn;
};

#endif // COSMICEXPORTMOVIEPOPUP_H
