#ifndef TOOLSPAGE_H
#define TOOLSPAGE_H

#include <QWidget>
#include <QMap>

class PreferenceManager;
class QCheckBox;
class QLabel;
class CosmicCheckboxToggle;
class CosmicInputField;

namespace Ui {
class ToolsPage;
}

class ToolsPage : public QWidget
{
Q_OBJECT
public:
    ToolsPage();
    ~ToolsPage() override;
    void setManager(PreferenceManager* p) { mManager = p; }

public slots:
    void updateValues();
    void quickSizingChange(int);
    void setRotationIncrement(int);
    void rotationIncrementChange(int);
    void invertZoomDirectionChange(int);

protected:
    void changeEvent(QEvent* e) override;

private:
    void retranslatePageStrings();

    Ui::ToolsPage* ui = nullptr;
    PreferenceManager* mManager = nullptr;
    CosmicCheckboxToggle* mQuickSizingToggle = nullptr;
    CosmicCheckboxToggle* mInvertZoomToggle  = nullptr;
    CosmicInputField*     mRotationIncrementField = nullptr;

    // Section headers
    QLabel* mHeaderBrushTools = nullptr;
    QLabel* mHeaderMoveTool   = nullptr;
    QLabel* mHeaderHandTool   = nullptr;

    // Checkbox → toggle map for text/tooltip sync
    QMap<QCheckBox*, CosmicCheckboxToggle*> mCheckboxToggleMap;
};

#endif // TOOLSPAGE_H
