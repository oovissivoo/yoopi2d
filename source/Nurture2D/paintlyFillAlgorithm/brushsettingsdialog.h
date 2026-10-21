#ifndef BRUSHSETTINGSDIALOG_H
#define BRUSHSETTINGSDIALOG_H

#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QGridLayout>
#include "blenderstyleslider.h"
#include "../CUI/cosmicdropdown.h"
#include "brushengine.h"
#include "canvas.h"

class BrushSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrushSettingsDialog(Canvas *canvas, QWidget *parent = nullptr);
    ~BrushSettingsDialog();

private slots:
    void updateHardness(int value);
    void updateFlow(int value);
    void updateRoundness(int value);
    void updateAngle(int value);
    void updateSpacing(int value);
    void updateJitter(int value);
    void updateScatterX(int value);
    void updateScatterY(int value);
    void updateBlendMode(int index);
    void updateBrushTip(int index);
    void togglePressure(bool enabled);
    void togglePressureSize(bool enabled);
    void togglePressureOpacity(bool enabled);
    void toggleAntialiasing(bool enabled);
    void toggleTiltSensitivity(bool enabled);
    void updateTiltSensitivity(int value);
    void toggleRotationSensitivity(bool enabled);
    void updateRotationSensitivity(int value);
    void updatePressureCurve(int value);
    void resetSettings();

private:
    Canvas *m_canvas;
    
    BlenderStyleSlider *m_hardnessSlider;
    BlenderStyleSlider *m_flowSlider;
    BlenderStyleSlider *m_roundnessSlider;
    BlenderStyleSlider *m_angleSlider;
    BlenderStyleSlider *m_spacingSlider;
    BlenderStyleSlider *m_jitterSlider;
    BlenderStyleSlider *m_scatterXSlider;
    BlenderStyleSlider *m_scatterYSlider;
    
    BlenderStyleSlider *m_tiltSensitivitySlider;
    BlenderStyleSlider *m_rotationSensitivitySlider;
    BlenderStyleSlider *m_pressureCurveSlider;

    // Migrated from QComboBox to CosmicDropDown
    CosmicDropDown* m_blendModeDropDown = nullptr;
    CosmicDropDown* m_brushTipDropDown = nullptr;
    // Parallel vectors mapping dropdown indices to enum values
    QVector<BlendMode> m_blendModeData;
    QVector<BrushTip> m_brushTipData;
    
    QCheckBox *m_pressureCheckBox;
    QCheckBox *m_pressureSizeCheckBox;
    QCheckBox *m_pressureOpacityCheckBox;
    QCheckBox *m_antialiasingCheckBox;
    
    QCheckBox *m_tiltCheckBox;
    QCheckBox *m_rotationCheckBox;
    
    QPushButton *m_resetButton;
    
    void setupUI();
    void connectSignals();
    void initializeValues();
};

#endif // BRUSHSETTINGSDIALOG_H
