#ifndef EXPORTANIMATIONDIALOG_H
#define EXPORTANIMATIONDIALOG_H

#include "importexportdialog.h"
#include <QSize>
#include <vector>
#include <utility>

namespace Ui { class ExportAnimationOptions; }

class CosmicDropDown;
class BlenderStyleToggle;
class CosmicInputField;

class ExportAnimationDialog : public ImportExportDialog
{
    Q_OBJECT
public:
    explicit ExportAnimationDialog(QWidget* parent);
    ~ExportAnimationDialog() override;

    // NEW: set resolution spin boxes based on current camera resolution
    void setResolution(const QSize& size);

    QSize getExportSize() const;
    QString getExportFormat() const;
    bool getTransparency() const;

    // Backward-compatible: now returns the parent folder name field
    QString getBaseFolderName() const;
    QString getDestinationDir() const;

    // New getters for enhanced export behavior
    bool isActiveLayerMode() const;
    bool shouldCreateParentFolder() const;
    QString getParentFolderName() const;

    // Export Specific controls
    enum class ExportSpecificMode {
        Keyframes = 0,
        CellsOnly = 1
    };

    bool isExportSpecificEnabled() const;
    ExportSpecificMode getExportSpecificMode() const;

private slots:
    void onBrowseDest();
    void onActiveLayerToggled(bool checked);

private:
    Ui::ExportAnimationOptions* ui;
    CosmicDropDown* mFormatDropDown = nullptr;
    CosmicDropDown* mExportSpecificModeDropDown = nullptr;
    BlenderStyleToggle* mActiveLayerToggle = nullptr;
    BlenderStyleToggle* mCreateParentFolderToggle = nullptr;

    CosmicInputField* mImgWidthField = nullptr;
    CosmicInputField* mImgHeightField = nullptr;
};

#endif // EXPORTANIMATIONDIALOG_H 