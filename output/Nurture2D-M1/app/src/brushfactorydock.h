#ifndef BRUSHFACTORYDOCK_H
#define BRUSHFACTORYDOCK_H

#include "basedockwidget.h"
#include "tooloptionwidget.h"

class QListWidget;
class QListWidgetItem;
class QButtonGroup;
class QStackedWidget;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class CosmicHSlider;
class CosmicCheckboxToggle;
class CosmicButton;
class BezierCurveEditor;

class BrushFactoryDock : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit BrushFactoryDock(QWidget* parent = nullptr);
    ~BrushFactoryDock() override;

    void initUI() override;
    void updateUI() override;

signals:
    void requestClose();
    void activePresetAboutToChange(int oldIndex, int newIndex);

public slots:
    void onPresetCurvesCommitted(int index,
                                 const QVector<QPointF>& thickness,
                                 const QVector<QPointF>& opacity,
                                 bool thicknessEnabled,
                                 bool opacityEnabled);

    // Method to set active preset from external components
    void setActivePreset(int index);

    // Apply a full preset list from external UI (Brushes pane), persist it, and refresh UI.
    void applyExternalPresets(const QVector<ToolOptionWidget::BrushPreset>& presets, int activeIndex);

    // Apply store-driven preset changes (live slider editing). Must be fast and must not
    // persist back into QSettings (the store is already doing persistence).
    void applyStorePresets(const QVector<ToolOptionWidget::BrushPreset>& presets, int activeIndex);

private:
    QWidget* mContentWidget = nullptr;
    QListWidget* mPresetListWidget = nullptr;

    QWidget* mCategoryContainer = nullptr;
    QButtonGroup* mCategoryGroup = nullptr;
    QStackedWidget* mCategoryStack = nullptr;

    // Stroke category
    CosmicHSlider* mSpacingField = nullptr;
    CosmicHSlider* mScatterField = nullptr;
    CosmicHSlider* mRotationField = nullptr;
    CosmicHSlider* mColorJitterField = nullptr;

    // Pressure category
    CosmicCheckboxToggle* mSizePressureToggle = nullptr;
    CosmicCheckboxToggle* mFlowPressureToggle = nullptr;
    BezierCurveEditor* mSizeCurveEditor = nullptr;
    BezierCurveEditor* mFlowCurveEditor = nullptr;

    // Texture category
    QLabel* mTexturePreview = nullptr;
    CosmicHSlider* mTextureScaleField = nullptr;
    CosmicHSlider* mTextureContrastField = nullptr;
    CosmicButton* mAddPresetButton = nullptr;
    CosmicButton* mRemovePresetButton = nullptr;
    CosmicButton* mResetDefaultButton = nullptr;
    CosmicButton* mAddTextureButton = nullptr;
    CosmicButton* mRemoveTextureButton = nullptr;

    QVector<ToolOptionWidget::BrushPreset> mPresets;
    int mActivePresetIndex = 0;

    void loadPresets();
    void savePresets();
};

#endif // BRUSHFACTORYDOCK_H
