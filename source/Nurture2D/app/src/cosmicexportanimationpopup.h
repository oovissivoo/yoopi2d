#ifndef COSMICEXPORTANIMATIONPOPUP_H
#define COSMICEXPORTANIMATIONPOPUP_H

#include "cosmicpopupwindow.h"
#include <QPointer>
#include <QSize>
#include <QString>

class CosmicWindowHeader;
class CosmicButton;
class CosmicDropDown;
class CosmicInputField;
class CosmicCheckboxToggle;
class CosmicEnumSelector;
class QLabel;
class QLineEdit;

class CosmicExportAnimationPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    // Sequence = all frames, Keyframes = keyframe positions only, Drawings = unique cells only
    enum class FrameMode { Sequence = 0, KeyframesOnly = 1, DrawingsOnly = 2 };

    explicit CosmicExportAnimationPopup(QWidget* parent);

    void setResolution(const QSize& size);

    // Results
    QString destDir()          const;
    QString mainFolderName()   const;
    // Alias for callers that use the old name
    QString parentFolderName() const { return mainFolderName(); }
    QString exportFormat()     const;
    QSize   exportSize()       const;
    bool    transparency()     const;
    bool    isActiveLayerMode() const;
    // Always true — subfolder creation is always enabled
    bool    shouldCreateParentFolder() const { return true; }
    // Maps FrameMode back to the legacy API
    bool    isExportSpecificEnabled() const;
    // Legacy enum compat
    enum class ExportSpecificMode { Keyframes = 0, CellsOnly = 1 };
    ExportSpecificMode exportSpecificMode() const;

    FrameMode frameMode() const;

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onBrowseDest();
    void onActiveLayerToggled(bool on);

private:
    void buildUI();

    QPointer<CosmicWindowHeader>   mHeader;

    // Destination
    QPointer<QLabel>               mDestLabel;   // path display label
    QPointer<QLineEdit>            mFolderNameEdit;

    // Format & Resolution
    QPointer<CosmicDropDown>       mFormatDrop;
    QPointer<CosmicInputField>     mWidthField;
    QPointer<CosmicInputField>     mHeightField;
    QPointer<QWidget>              mResCard;
    QPointer<CosmicCheckboxToggle> mTransparencyToggle;

    // Layer Mode
    QPointer<CosmicCheckboxToggle> mActiveLayerToggle;

    // Frame Mode selector
    QPointer<CosmicEnumSelector>   mFrameModeSelector;

    // Action buttons
    QPointer<CosmicButton>         mExportBtn;
    QPointer<CosmicButton>         mCancelBtn;
};

#endif // COSMICEXPORTANIMATIONPOPUP_H
