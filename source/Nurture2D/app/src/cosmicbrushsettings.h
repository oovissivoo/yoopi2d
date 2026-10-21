#ifndef COSMICBRUSHSETTINGS_H
#define COSMICBRUSHSETTINGS_H

#include "cosmicpopupwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QPointer>

#include "tooloptionwidget.h"

class CosmicHSlider;
class CosmicCheckboxToggle;
class CosmicButton;
class CosmicEnumSelector;
class CosmicSliderCard;
class BezierCurveEditor;
class CosmicWindowHeader;

class CosmicBrushSettings : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicBrushSettings(int presetIndex, QWidget* parent = nullptr);
    ~CosmicBrushSettings();

    void setPresetId(const QString& id);
    QString presetId() const;

    void setPresetName(const QString& name);
    QString presetName() const;

    // When true, only the Stroke category is shown with size-pressure toggle + curve only.
    void setFinalLineMode(bool finalLine);

    // Seed the live pen tool state so the popup shows current values when opened in Final Line mode.
    void setFinalLineLiveState(bool enabled, const QVector<QPointF>& curvePoints);

signals:
    // Emitted when Final Line pressure settings change so the caller can push them to PenTool.
    void finalLinePressureChanged(bool enabled, QVector<QPointF> curvePoints);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onTabSelected(int index);
    void onPresetsUpdated(const QVector<ToolOptionWidget::BrushPreset>& presets, const QString& activePresetId);

private:
    void setupUI();
    void updateLayout();
    void setupTabBar();
    void updateCategoryStyles();
    void updateContentForCategory(const QString& category);
    void applyBrushesSliderStyle(QWidget* widget);
    void rebuildStrokeUI(QVBoxLayout* layout);
    void rebuildColorUI(QVBoxLayout* layout);
    void rebuildTipUI(QVBoxLayout* layout);
    void rebuildTextureUI(QVBoxLayout* layout);
    void rebuildPressureUI(QVBoxLayout* layout);
    void syncControlsFromStore();
    bool loadPresetFromStore(const QString& presetId, ToolOptionWidget::BrushPreset& outPreset) const;
    
    int mPresetIndex;
    QString mPresetId;
    QString mPresetName;
    
    // UI components
    QVBoxLayout* mMainLayout;

    QPointer<CosmicWindowHeader> mWindowHeader;
    QPointer<QLabel> mPresetHeaderLabel;
    
    // Tab selector (CosmicEnumSelector replaces the old QPushButton tab bar)
    QPointer<CosmicEnumSelector> mTabSelector;
    QStackedWidget* mPageStack = nullptr;
    QStringList mTabCategories;
    QWidget* mSettingsContentWidget = nullptr;

    // Stroke controls (live bound) — CosmicSliderCards
    QPointer<CosmicSliderCard> mSpacingCard;
    QPointer<CosmicSliderCard> mScatterCard;
    QPointer<CosmicSliderCard> mRotationCard;
    QPointer<CosmicSliderCard> mSizeJitterCard;

    // Keep raw slider pointers for isSliderDown() checks during sync
    QPointer<CosmicHSlider> mSpacingSlider;
    QPointer<CosmicHSlider> mScatterSlider;
    QPointer<CosmicHSlider> mRotationSlider;
    QPointer<CosmicHSlider> mSizeJitterSlider;
    QPointer<CosmicHSlider> mColorJitterSlider; // lives on Color tab

    // Color tab
    QPointer<CosmicSliderCard> mColorJitterCard;

    // Tip controls (live bound)
    QPointer<CosmicEnumSelector> mTipModeSelector;
    QPointer<QLabel>             mTipPreviewLabel;
    QPointer<QLabel>             mTipNameLabel;
    QPointer<CosmicSliderCard>   mTipContrastCard;
    QPointer<CosmicHSlider>      mTipContrastSlider;
    QPointer<CosmicButton>       mGalleryTipButton;

    // Texture controls (live bound)
    QPointer<CosmicSliderCard> mTextureScaleCard;
    QPointer<CosmicSliderCard> mTextureContrastCard;
    QPointer<CosmicHSlider>    mTextureContrastSlider; // raw pointer for isSliderDown()
    QPointer<QLabel>           mTexturePreviewLabel;
    QPointer<QLabel>           mTextureNameLabel;
    QPointer<CosmicButton>     mAddTextureButton;
    QPointer<CosmicButton>     mRemoveTextureButton;
    QPointer<CosmicButton>     mGalleryTextureButton;

    // Pressure controls (live bound)
    QPointer<CosmicCheckboxToggle> mSizePressureToggle;
    QPointer<CosmicCheckboxToggle> mFlowPressureToggle;
    QPointer<BezierCurveEditor>    mSizeCurveEditor;
    QPointer<BezierCurveEditor>    mFlowCurveEditor;

    // Sync safety
    bool mSyncQueued = false;
    bool mSyncInProgress = false;
    bool mFinalLineMode = false;

    // Cached live state for Final Line (seeded from PenTool before popup opens)
    bool mFinalLineLiveEnabled = true;
    QVector<QPointF> mFinalLineLiveCurve;
    
    // Styling
    static constexpr int BORDER_RADIUS = 14;
    static constexpr int BORDER_WIDTH = 2;
};

#endif // COSMICBRUSHSETTINGS_H
