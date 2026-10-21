/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef TOOLOPTIONDOCKWIDGET_H
#define TOOLOPTIONDOCKWIDGET_H

#include "basedockwidget.h"
#include "pencildef.h"
#include "dplatecolorpalette.h"
#include "preferencesdef.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include <QByteArray>
#include <QVector>
#include <QPointF>

namespace Ui
{
    class ToolOptions;
}
class QToolButton;
class SpinSlider;
class QCheckBox;
class QAbstractButton;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QGroupBox;
class QWidget;
class QLabel;
class Editor;
class BaseTool;
class BucketOptionsWidget;
class CameraOptionsWidget;
class PenColorPalette;
class DplateColorPalette;
class QPushButton;
class QHBoxLayout;
class QButtonGroup;
class PreferenceManager;
class BlenderStyleComboBox;
class BlenderStyleToggle;
class BlenderStyleModalPopup;
class BlenderStyleValueField;
class BlenderStyleSlider;
class CosmicTapperSliders;
class CosmicIntensityBox;
class CosmicCheckboxToggle;
class CosmicInputField;
class CosmicDropDown;
class CosmicIgnoreColor;

class ToolOptionWidget : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit ToolOptionWidget(QWidget* parent);
    virtual ~ToolOptionWidget() override;

    void initUI() override;
    void updateUI() override;

    // Targeted update for per-preset opacity without full UI refresh
    void updatePencilOpacityField();

    // Step the active smooth field by +1 or -1 (reuses the field's built-in increment/decrement)
    void stepSmoothValue(int delta);

    void makeConnectionToEditor(Editor* editor);

    // Accessors for preset management buttons
    QPushButton* getExportPresetButton() const { return mExportPresetBtn; }
    QPushButton* getRemovePresetButton() const { return mRemovePresetBtn; }
    QPushButton* getAddPresetButton() const { return mAddPresetBtn; }
    QPushButton* getEditNameButton() const;

    // Direct action helpers
    void exportCurrentPreset();
    void removeCurrentPreset();
    void renameCurrentPreset();

    QWidget* dockContentWidget() const { return mDockContentWidget; }
    QWidget* innerOptionsWidget() const { return mInnerOptionsWidget; }

    bool hasVisibleOptions() const;

    void refreshOptionsPaneState();

public slots:
    void onPresetDuplicateRequested(int index);
    void onColorPreferenceChanged(SETTING setting);
    void onToolPropertyChanged(ToolType, ToolPropertyType);
    void onToolChanged(ToolType toolType);
signals:
    void visibleOptionsChanged(bool hasOptions);

public:
    // Public type so external helpers (e.g. custom delegates) can render
    // brush presets visually in the Brush Factory.
    struct BrushPreset {
        QString id;
        QString name;
        qreal spacingMult;
        qreal scatter;
        qreal rotationDeg;
        qreal sizeJitter;
        qreal colorJitter;
        bool builtIn = false;

        // Tip
        enum class TipMode { Hard, Soft, Custom };
        TipMode tipMode = TipMode::Hard;
        QString tipAssetId;          // asset ID in BrushAssetStore (empty = built-in hard)
        int tipContrastPercent = 50;

        qreal textureTileScale = 12.0;
        int textureContrastPercent = 50;
        QString textureAssetId;      // asset ID in BrushAssetStore (empty = plain white)
        QVector<QPointF> thicknessCurvePoints;
        QVector<QPointF> opacityCurvePoints;
        bool thicknessCurveEnabled = true;
        bool opacityCurveEnabled = true;
    };

private:
    void setPenWidth(qreal);
    void setPenFeather(qreal);
    void setUseFeather(bool);
    void updateAccentColors();
    void setPenInvisibility(int);
    void setPressure(int);
    void setPreserveAlpha(int);
    void setVectorMergeEnabled(int);
    void setAA(int);
    void setStabilizerLevel(int);
    void setFillContour(int);
    void setBezier(bool);
    void setClosedPath(bool);
    void setDrawBehind(bool);
    void setShowSelectionInfo(bool);

    void disableAllOptions();
    void setVisibility(BaseTool*);
    void createUI();

    void applyUnifiedSmoothToTool(ToolType toolType);

    // Unified Pencil UI: keep shared stroke properties mirrored between Pencil and Pen.
    void mirrorUnifiedStrokeSettings(ToolType dstTool, ToolType srcTool);

private:
    Ui::ToolOptions* ui = nullptr;

    QWidget* mDockContentWidget = nullptr;
    QWidget* mInnerOptionsWidget = nullptr;
    bool mLastHasVisibleOptions = true;
    bool mHasVisibleOptionsInitialized = false;

    BucketOptionsWidget* mBucketOptionsWidget = nullptr;
    CameraOptionsWidget* mCameraOptionsWidget = nullptr;
    // PenColorPalette is relocated to the canvas overlay; keep pointer but unused/hidden
    PenColorPalette* mPenColorPalette = nullptr;
    DplateColorPalette* mDplateColorPalette = nullptr;
    QPushButton* mPresetResetButton = nullptr;
    QPushButton* mAddPresetBtn = nullptr;
    QPushButton* mRemovePresetBtn = nullptr;
    QPushButton* mTextureBtn = nullptr;
    QPushButton* mExportPresetBtn = nullptr;
    QPushButton* mImportPresetBtn = nullptr;
    QHBoxLayout* mPresetIOLayout = nullptr;
    // Pen Width cosmic input field (Pen tool only; replaces size slider visually)
    CosmicInputField* mPenWidthField = nullptr;

    // Pen smoothing mode control
    BlenderStyleComboBox* mPenSmoothingComboBox = nullptr;
    // Pen post-stroke correction control (0=Off,1=Low,2=Med,3=High)
    CosmicIntensityBox* mPostCorrBox = nullptr;
    // Hybrid bitmap centerline snapping strength (Curve/BSpline/Polyline)
    BlenderStyleComboBox* mHybridSnapComboBox = nullptr;
    // Draw behind toggle control (cosmic standard checkbox)
    CosmicCheckboxToggle* mDrawBehindToggle = nullptr;
    // Adjust pressure toggle and popup (Pen tool)
    BlenderStyleToggle* mAdjustPressureToggle = nullptr;
    BlenderStyleModalPopup* mAdjustPressurePopup = nullptr;
    BlenderStyleModalPopup* mCosmicPenSettingsPopup = nullptr;
    
    
    // Legacy Pencil tool opacity / stabiliser sliders (replaced by Cosmic split layout for Pencil)
    BlenderStyleSlider* mPencilOpacitySlider = nullptr;
    BlenderStyleSlider* mPencilStabilizerSlider = nullptr;
    
    // Anti-aliasing toggle control (replaces ui->useAABox)
    BlenderStyleToggle* mAntiAliasingToggle = nullptr;

    // New: Toggles replacing legacy QCheckBox widgets in tool options
    BlenderStyleToggle* mUseFeatherToggle = nullptr;
    BlenderStyleToggle* mShowInfoToggle = nullptr;
    BlenderStyleToggle* mFillContourToggle = nullptr;
    BlenderStyleToggle* mClosedPathToggle = nullptr;
    BlenderStyleToggle* mBezierToggle = nullptr;
    BlenderStyleToggle* mInvisibleToggle = nullptr;
    BlenderStyleToggle* mPreserveAlphaToggle = nullptr;
    BlenderStyleToggle* mVectorMergeToggle = nullptr;
    // Cosmic-style lasso toggle for Eraser (lives in CosmicSplitToolLayout for eraser)
    CosmicCheckboxToggle* mEraserLassoToggle = nullptr;

    CosmicTapperSliders* mCurveTapperSliders = nullptr;

    // CosmicSplitLayout-specific controls for Pen tool (right + bottom areas; layout-only for now)
    CosmicInputField*      mCosmicPenSizeField = nullptr;
    CosmicDropDown*        mCosmicPenModeDropDown = nullptr;
    CosmicInputField*      mCosmicPenSmoothField = nullptr;
    CosmicCheckboxToggle*  mCosmicPenDrawBehindToggle = nullptr;
    CosmicIntensityBox*    mCosmicPenPostIntensityBox = nullptr;
    CosmicCheckboxToggle*  mCosmicPenSettingsToggle = nullptr;

    // CosmicSplitLayout-specific controls for Pencil tool (bitmap pencil on bitmap layers)
    CosmicInputField*      mCosmicPencilSizeField = nullptr;
    CosmicInputField*      mCosmicPencilOpacityField = nullptr;
    CosmicDropDown*        mCosmicPencilModeDropDown = nullptr;

    // Unified Pencil UI: Post-stroke correction control (shared with Pen when using Pencil UI)
    CosmicIntensityBox*    mCosmicPencilPostIntensityBox = nullptr;

    // Row container widgets created by CosmicSplitToolLayout::createEntry (label + control).
    QWidget* mCosmicPencilOpacityRow = nullptr;
    QWidget* mCosmicPencilPostCorrRow = nullptr;

    int mUnifiedSmoothUiValue = 5;
    bool mUnifiedSmoothUiInitialized = false;
    bool mApplyingUnifiedSmoothToTool = false;
    CosmicInputField*      mCosmicPencilSmoothField = nullptr;
    CosmicCheckboxToggle*  mCosmicPencilDrawBehindToggle = nullptr;
    CosmicCheckboxToggle*  mCosmicPencilSettingsToggle = nullptr;

    // CosmicSplitLayout-specific controls for Eraser tool (left/right split rows)
    CosmicInputField*      mCosmicEraserSizeField = nullptr;
    CosmicCheckboxToggle*  mCosmicEraserPressureToggle = nullptr;

    // CosmicSplitLayout-specific controls for Eyedropper tool
    CosmicDropDown*        mCosmicEyedropperReferenceDropDown = nullptr;

    // CosmicSplitLayout-specific controls for Curve tool (bitmap stroke options)
    CosmicInputField*      mCosmicCurveWidthField = nullptr;
    CosmicTapperSliders*   mCosmicCurveTapperSliders = nullptr;
    CosmicIntensityBox*    mCosmicHybridSnapBox = nullptr;
    CosmicCheckboxToggle*  mCosmicCurveDrawBehindToggle = nullptr;
    CosmicCheckboxToggle*  mCosmicDplateDrawBehindToggle = nullptr;

    // CosmicSplitLayout-specific controls for Dplate box counts
    CosmicIntensityBox*    mCosmicDplateBigBoxField = nullptr;
    CosmicIntensityBox*    mCosmicDplateSmallBoxBox = nullptr;

    // CosmicSplitLayout-specific controls for Gap Fill tools
    CosmicCheckboxToggle*  mCosmicGapUseActiveToggle = nullptr;
    CosmicIgnoreColor*     mCosmicGapIgnoreColor    = nullptr;

    // Row label pointers for CosmicSplitToolLayout entries — needed for retranslation.
    // Each label is created inside createEntry() and stored here so changeEvent can update them.
    QLabel* mLabelPenSize          = nullptr;
    QLabel* mLabelPenMode          = nullptr;
    QLabel* mLabelPenSmooth        = nullptr;
    QLabel* mLabelPenDrawBehind    = nullptr;
    QLabel* mLabelPenPostCorr      = nullptr;
    QLabel* mLabelPencilSize       = nullptr;
    QLabel* mLabelPencilPostCorr   = nullptr;
    QLabel* mLabelPencilOpacity    = nullptr;
    QLabel* mLabelPencilMode       = nullptr;
    QLabel* mLabelPencilSmooth     = nullptr;
    QLabel* mLabelPencilDrawBehind = nullptr;
    QLabel* mLabelEraserSize       = nullptr;
    QLabel* mLabelEraserPressure   = nullptr;
    QLabel* mLabelEraserLasso      = nullptr;
    QLabel* mLabelCurveSize        = nullptr;
    QLabel* mLabelCurveSnap        = nullptr;
    QLabel* mLabelCurveTaper       = nullptr;
    QLabel* mLabelCurveDrawBehind  = nullptr;
    QLabel* mLabelDplateDrawBehind = nullptr;
    QLabel* mLabelDplateBigBoxes   = nullptr;
    QLabel* mLabelDplateSmallBoxes = nullptr;
    QLabel* mLabelEyedropperRef    = nullptr;
    QLabel* mLabelGapUseActive     = nullptr;
    QLabel* mLabelGapIgnoreColor   = nullptr;

    void hidePenColorPalette();
    void duplicatePreset(int index);

protected:
    void changeEvent(QEvent* e) override;

private:
    void retranslateUI();
};

#endif // TOOLOPTIONDOCKWIDGET_H
