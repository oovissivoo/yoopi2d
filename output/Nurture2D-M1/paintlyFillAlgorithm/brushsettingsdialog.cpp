#include "brushsettingsdialog.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QStackedWidget>

BrushSettingsDialog::BrushSettingsDialog(Canvas *canvas, QWidget *parent)
    : QDialog(parent)
    , m_canvas(canvas)
{
    setWindowTitle(tr("Brush Settings"));
    setupUI();
    connectSignals();
    initializeValues();
    resize(400, 500);
}

BrushSettingsDialog::~BrushSettingsDialog()
{
}

void BrushSettingsDialog::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    // Create the left panel for category buttons
    QVBoxLayout *categoriesLayout = new QVBoxLayout();
    QWidget *categoriesPanel = new QWidget();
    categoriesPanel->setFixedWidth(150); // Set fixed width for the left panel
    categoriesPanel->setLayout(categoriesLayout);
    
    // Style for the category buttons
    QString buttonStyle = 
        "QPushButton {"
        "   text-align: left;"
        "   padding: 8px;"
        "   border: none;"
        "   border-radius: 4px;"
        "   background-color: transparent;"
        "   font-weight: normal;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(200, 200, 200, 40);"
        "}"
        "QPushButton:checked {"
        "   background-color: rgba(100, 100, 255, 60);"
        "   font-weight: bold;"
        "}";
    
    // Create category buttons
    QPushButton *presetsButton = new QPushButton("Brush Presets");
    QPushButton *strokeButton = new QPushButton("Stroke");
    QPushButton *pressureButton = new QPushButton("Pressure");
    QPushButton *textureButton = new QPushButton("Texture");
    
    // Set button properties
    presetsButton->setCheckable(true);
    strokeButton->setCheckable(true);
    pressureButton->setCheckable(true);
    textureButton->setCheckable(true);
    
    presetsButton->setStyleSheet(buttonStyle);
    strokeButton->setStyleSheet(buttonStyle);
    pressureButton->setStyleSheet(buttonStyle);
    textureButton->setStyleSheet(buttonStyle);
    
    // Add buttons to the categories layout
    categoriesLayout->addWidget(presetsButton);
    categoriesLayout->addWidget(strokeButton);
    categoriesLayout->addWidget(pressureButton);
    categoriesLayout->addWidget(textureButton);
    categoriesLayout->addStretch();
    
    // Create the stacked widget for the right panel
    QStackedWidget *settingsStack = new QStackedWidget();
    
    // Create pages for each category
    QWidget *presetsPage = new QWidget();
    QWidget *strokePage = new QWidget();
    QWidget *pressurePage = new QWidget();
    QWidget *texturePage = new QWidget();
    
    // Create layouts for each page
    QVBoxLayout *presetsLayout = new QVBoxLayout(presetsPage);
    QGridLayout *strokeLayout = new QGridLayout(strokePage);
    QGridLayout *pressureLayout = new QGridLayout(pressurePage);
    QGridLayout *textureLayout = new QGridLayout(texturePage);
    
    // Add pages to the stack
    settingsStack->addWidget(presetsPage);
    settingsStack->addWidget(strokePage);
    settingsStack->addWidget(pressurePage);
    settingsStack->addWidget(texturePage);
    
    // Connect buttons to change the current page
    connect(presetsButton, &QPushButton::clicked, [=]() {
        settingsStack->setCurrentIndex(0);
        presetsButton->setChecked(true);
        strokeButton->setChecked(false);
        pressureButton->setChecked(false);
        textureButton->setChecked(false);
    });
    
    connect(strokeButton, &QPushButton::clicked, [=]() {
        settingsStack->setCurrentIndex(1);
        presetsButton->setChecked(false);
        strokeButton->setChecked(true);
        pressureButton->setChecked(false);
        textureButton->setChecked(false);
    });
    
    connect(pressureButton, &QPushButton::clicked, [=]() {
        settingsStack->setCurrentIndex(2);
        presetsButton->setChecked(false);
        strokeButton->setChecked(false);
        pressureButton->setChecked(true);
        textureButton->setChecked(false);
    });
    
    connect(textureButton, &QPushButton::clicked, [=]() {
        settingsStack->setCurrentIndex(3);
        presetsButton->setChecked(false);
        strokeButton->setChecked(false);
        pressureButton->setChecked(false);
        textureButton->setChecked(true);
    });
    
    // Set the first button as checked by default
    presetsButton->setChecked(true);
    
    //---------------------------------------------
    // Brush Presets page
    //---------------------------------------------
    m_resetButton = new QPushButton(tr("Reset to Defaults"));
    presetsLayout->addWidget(m_resetButton);
    presetsLayout->addStretch();
    
    //---------------------------------------------
    // Stroke page
    //---------------------------------------------

    QHBoxLayout *brushTipLayout = new QHBoxLayout();
    QLabel *brushTipLabel = new QLabel(tr("Brush Tip:"));
    m_brushTipDropDown = new CosmicDropDown();
    {
        QStringList tipOptions;
        tipOptions << tr("Circle") << tr("Square") << tr("Diamond") << tr("Texture");
        m_brushTipData = { BrushTip::Circle, BrushTip::Square, BrushTip::Diamond, BrushTip::Texture };
        m_brushTipDropDown->setOptions(tipOptions);
    }
    brushTipLayout->addWidget(brushTipLabel);
    brushTipLayout->addWidget(m_brushTipDropDown);
    QWidget *brushTipContainer = new QWidget();
    brushTipContainer->setLayout(brushTipLayout);
    strokeLayout->addWidget(brushTipContainer, 0, 0, 1, 2);
    
    QLabel *hardnessLabel = new QLabel(tr("Hardness:"));
    m_hardnessSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_hardnessSlider->setRange(0, 100);
    m_hardnessSlider->setValue(100);
    m_hardnessSlider->setDefaultValue(100);
    m_hardnessSlider->setValueDisplayFormat("Hardness: %d%%");
    strokeLayout->addWidget(hardnessLabel, 1, 0);
    strokeLayout->addWidget(m_hardnessSlider, 1, 1);
    
    QLabel *flowLabel = new QLabel(tr("Flow:"));
    m_flowSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_flowSlider->setRange(0, 100);
    m_flowSlider->setValue(100);
    m_flowSlider->setDefaultValue(100);
    m_flowSlider->setValueDisplayFormat("Flow: %d%%");
    strokeLayout->addWidget(flowLabel, 2, 0);
    strokeLayout->addWidget(m_flowSlider, 2, 1);
    
    QLabel *roundnessLabel = new QLabel(tr("Roundness:"));
    m_roundnessSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_roundnessSlider->setRange(0, 100);
    m_roundnessSlider->setValue(100);
    m_roundnessSlider->setDefaultValue(100);
    m_roundnessSlider->setValueDisplayFormat("Roundness: %d%%");
    strokeLayout->addWidget(roundnessLabel, 3, 0);
    strokeLayout->addWidget(m_roundnessSlider, 3, 1);
    
    QLabel *angleLabel = new QLabel(tr("Angle:"));
    m_angleSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_angleSlider->setRange(0, 360);
    m_angleSlider->setValue(0);
    m_angleSlider->setDefaultValue(0);
    m_angleSlider->setValueDisplayFormat("Angle: %d°");
    strokeLayout->addWidget(angleLabel, 4, 0);
    strokeLayout->addWidget(m_angleSlider, 4, 1);
    
    QLabel *spacingLabel = new QLabel(tr("Spacing:"));
    m_spacingSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_spacingSlider->setRange(1, 200);
    m_spacingSlider->setValue(10);
    m_spacingSlider->setDefaultValue(10);
    m_spacingSlider->setValueDisplayFormat("Spacing: %d%%");
    strokeLayout->addWidget(spacingLabel, 5, 0);
    strokeLayout->addWidget(m_spacingSlider, 5, 1);
    
    QLabel *jitterLabel = new QLabel(tr("Jitter:"));
    m_jitterSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_jitterSlider->setRange(0, 100);
    m_jitterSlider->setValue(0);
    m_jitterSlider->setDefaultValue(0);
    m_jitterSlider->setValueDisplayFormat("Jitter: %d%%");
    strokeLayout->addWidget(jitterLabel, 6, 0);
    strokeLayout->addWidget(m_jitterSlider, 6, 1);
    
    QLabel *blendModeLabel = new QLabel(tr("Blend Mode:"));
    m_blendModeDropDown = new CosmicDropDown();
    {
        QStringList blendOptions;
        m_blendModeData.clear();
        blendOptions << tr("Normal"); m_blendModeData << BlendMode::Normal;
        blendOptions << tr("Multiply"); m_blendModeData << BlendMode::Multiply;
        blendOptions << tr("Screen"); m_blendModeData << BlendMode::Screen;
        blendOptions << tr("Overlay"); m_blendModeData << BlendMode::Overlay;
        blendOptions << tr("Dodge"); m_blendModeData << BlendMode::Dodge;
        blendOptions << tr("Burn"); m_blendModeData << BlendMode::Burn;
        blendOptions << tr("Darken"); m_blendModeData << BlendMode::Darken;
        blendOptions << tr("Lighten"); m_blendModeData << BlendMode::Lighten;
        blendOptions << tr("Color Dodge"); m_blendModeData << BlendMode::ColorDodge;
        blendOptions << tr("Color Burn"); m_blendModeData << BlendMode::ColorBurn;
        blendOptions << tr("Hard Light"); m_blendModeData << BlendMode::HardLight;
        blendOptions << tr("Soft Light"); m_blendModeData << BlendMode::SoftLight;
        blendOptions << tr("Difference"); m_blendModeData << BlendMode::Difference;
        blendOptions << tr("Exclusion"); m_blendModeData << BlendMode::Exclusion;
        blendOptions << tr("Color"); m_blendModeData << BlendMode::Color;
        blendOptions << tr("Luminosity"); m_blendModeData << BlendMode::Luminosity;
        m_blendModeDropDown->setOptions(blendOptions);
    }
    strokeLayout->addWidget(blendModeLabel, 7, 0);
    strokeLayout->addWidget(m_blendModeDropDown, 7, 1);
    
    m_antialiasingCheckBox = new QCheckBox(tr("Enable Antialiasing"));
    strokeLayout->addWidget(m_antialiasingCheckBox, 8, 0, 1, 2);
    strokeLayout->setRowStretch(9, 1);
    
    //---------------------------------------------
    // Pressure page
    //---------------------------------------------
    
    m_pressureCheckBox = new QCheckBox(tr("Enable Pressure Sensitivity"));
    pressureLayout->addWidget(m_pressureCheckBox, 0, 0, 1, 2);
    
    m_pressureSizeCheckBox = new QCheckBox(tr("  → Apply to Size"));
    pressureLayout->addWidget(m_pressureSizeCheckBox, 1, 0, 1, 2);
    
    m_pressureOpacityCheckBox = new QCheckBox(tr("  → Apply to Opacity"));
    pressureLayout->addWidget(m_pressureOpacityCheckBox, 2, 0, 1, 2);
    
    QLabel *pressureCurveLabel = new QLabel(tr("Pressure Curve:"));
    m_pressureCurveSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_pressureCurveSlider->setRange(0, 100);
    m_pressureCurveSlider->setValue(50);
    m_pressureCurveSlider->setDefaultValue(50);
    m_pressureCurveSlider->setValueDisplayFormat("Pressure Curve: %d%%");
    pressureLayout->addWidget(pressureCurveLabel, 3, 0);
    pressureLayout->addWidget(m_pressureCurveSlider, 3, 1);
    
    m_tiltCheckBox = new QCheckBox(tr("Enable Tilt Sensitivity"));
    pressureLayout->addWidget(m_tiltCheckBox, 4, 0, 1, 2);
    
    QLabel *tiltSensitivityLabel = new QLabel(tr("  → Tilt Sensitivity:"));
    m_tiltSensitivitySlider = new BlenderStyleSlider(Qt::Horizontal);
    m_tiltSensitivitySlider->setRange(0, 100);
    m_tiltSensitivitySlider->setValue(50);
    m_tiltSensitivitySlider->setDefaultValue(50);
    m_tiltSensitivitySlider->setValueDisplayFormat("Tilt Sensitivity: %d%%");
    pressureLayout->addWidget(tiltSensitivityLabel, 5, 0);
    pressureLayout->addWidget(m_tiltSensitivitySlider, 5, 1);
    
    m_rotationCheckBox = new QCheckBox(tr("Enable Rotation Sensitivity"));
    pressureLayout->addWidget(m_rotationCheckBox, 6, 0, 1, 2);
    
    QLabel *rotationSensitivityLabel = new QLabel(tr("  → Rotation Sensitivity:"));
    m_rotationSensitivitySlider = new BlenderStyleSlider(Qt::Horizontal);
    m_rotationSensitivitySlider->setRange(0, 100);
    m_rotationSensitivitySlider->setValue(50);
    m_rotationSensitivitySlider->setDefaultValue(50);
    m_rotationSensitivitySlider->setValueDisplayFormat("Rotation Sensitivity: %d%%");
    pressureLayout->addWidget(rotationSensitivityLabel, 7, 0);
    pressureLayout->addWidget(m_rotationSensitivitySlider, 7, 1);
    pressureLayout->setRowStretch(8, 1);
    
    //---------------------------------------------
    // Texture page
    //---------------------------------------------
    
    QLabel *scatterXLabel = new QLabel(tr("Scatter X:"));
    m_scatterXSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_scatterXSlider->setRange(0, 100);
    m_scatterXSlider->setValue(0);
    m_scatterXSlider->setDefaultValue(0);
    m_scatterXSlider->setValueDisplayFormat("Scatter X: %d%%");
    textureLayout->addWidget(scatterXLabel, 0, 0);
    textureLayout->addWidget(m_scatterXSlider, 0, 1);
    
    QLabel *scatterYLabel = new QLabel(tr("Scatter Y:"));
    m_scatterYSlider = new BlenderStyleSlider(Qt::Horizontal);
    m_scatterYSlider->setRange(0, 100);
    m_scatterYSlider->setValue(0);
    m_scatterYSlider->setDefaultValue(0);
    m_scatterYSlider->setValueDisplayFormat("Scatter Y: %d%%");
    textureLayout->addWidget(scatterYLabel, 1, 0);
    textureLayout->addWidget(m_scatterYSlider, 1, 1);
    textureLayout->setRowStretch(2, 1);
    
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    QVBoxLayout *rightPanelLayout = new QVBoxLayout();
    rightPanelLayout->addWidget(settingsStack, 1);
    rightPanelLayout->addWidget(buttonBox);
    
    mainLayout->addWidget(categoriesPanel);
    mainLayout->addWidget(separator);
    mainLayout->addLayout(rightPanelLayout, 1);
    
    setMinimumSize(600, 500);
}

void BrushSettingsDialog::connectSignals()
{
    connect(m_hardnessSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateHardness);
    connect(m_flowSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateFlow);
    connect(m_roundnessSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateRoundness);
    connect(m_angleSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateAngle);
    connect(m_spacingSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateSpacing);
    connect(m_jitterSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateJitter);
    connect(m_scatterXSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateScatterX);
    connect(m_scatterYSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateScatterY);

    connect(m_blendModeDropDown, &CosmicDropDown::currentIndexChanged, this, &BrushSettingsDialog::updateBlendMode);
    connect(m_brushTipDropDown, &CosmicDropDown::currentIndexChanged, this, &BrushSettingsDialog::updateBrushTip);
    
    connect(m_pressureCheckBox, &QCheckBox::toggled, this, &BrushSettingsDialog::togglePressure);
    connect(m_pressureSizeCheckBox, &QCheckBox::toggled, this, &BrushSettingsDialog::togglePressureSize);
    connect(m_pressureOpacityCheckBox, &QCheckBox::toggled, this, &BrushSettingsDialog::togglePressureOpacity);
    connect(m_antialiasingCheckBox, &QCheckBox::toggled, this, &BrushSettingsDialog::toggleAntialiasing);
    
    connect(m_pressureCurveSlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updatePressureCurve);
    connect(m_tiltCheckBox, &QCheckBox::toggled, this, &BrushSettingsDialog::toggleTiltSensitivity);
    connect(m_tiltSensitivitySlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateTiltSensitivity);
    connect(m_rotationCheckBox, &QCheckBox::toggled, this, &BrushSettingsDialog::toggleRotationSensitivity);
    connect(m_rotationSensitivitySlider, &BlenderStyleSlider::valueChanged, this, &BrushSettingsDialog::updateRotationSensitivity);
    
    connect(m_resetButton, &QPushButton::clicked, this, &BrushSettingsDialog::resetSettings);
}

void BrushSettingsDialog::initializeValues()
{
    BrushEngine *brushEngine = m_canvas->brushEngine();
    BrushSettings settings = brushEngine->settings();
    
    m_hardnessSlider->setValue(static_cast<int>(settings.hardness * 100));
    m_flowSlider->setValue(static_cast<int>(settings.flow * 100));
    m_roundnessSlider->setValue(static_cast<int>(settings.roundness * 100));
    m_angleSlider->setValue(static_cast<int>(settings.angle));
    m_spacingSlider->setValue(static_cast<int>(settings.spacing * 100));
    m_jitterSlider->setValue(static_cast<int>(settings.jitter * 100));
    m_scatterXSlider->setValue(static_cast<int>(settings.scatterX * 100));
    m_scatterYSlider->setValue(static_cast<int>(settings.scatterY * 100));
    
    m_pressureCurveSlider->setValue(static_cast<int>(settings.pressureCurve * 100));
    m_tiltCheckBox->setChecked(settings.useTiltEnabled);
    m_tiltSensitivitySlider->setValue(static_cast<int>(settings.tiltSensitivity * 100));
    m_rotationCheckBox->setChecked(settings.useRotationEnabled);
    m_rotationSensitivitySlider->setValue(static_cast<int>(settings.rotationSensitivity * 100));
    
    m_tiltSensitivitySlider->setEnabled(settings.useTiltEnabled);
    m_rotationSensitivitySlider->setEnabled(settings.useRotationEnabled);
    
    int blendModeIndex = 0;
    for (int i = 0; i < m_blendModeData.size(); ++i) { if (m_blendModeData[i] == settings.blendMode) { blendModeIndex = i; break; } }
    if (m_blendModeDropDown) m_blendModeDropDown->setCurrentIndex(blendModeIndex);
    int brushTipIndex = 0;
    for (int i = 0; i < m_brushTipData.size(); ++i) { if (m_brushTipData[i] == settings.brushTip) { brushTipIndex = i; break; } }
    if (m_brushTipDropDown) m_brushTipDropDown->setCurrentIndex(brushTipIndex);
    
    m_pressureCheckBox->setChecked(settings.usePressure);
    m_pressureSizeCheckBox->setChecked(settings.useSizePressure);
    m_pressureOpacityCheckBox->setChecked(settings.useOpacityPressure);
    m_antialiasingCheckBox->setChecked(settings.antialias);
    
    m_pressureSizeCheckBox->setEnabled(settings.usePressure);
    m_pressureOpacityCheckBox->setEnabled(settings.usePressure);
}

void BrushSettingsDialog::updateHardness(int value)   { m_canvas->setBrushHardness(value / 100.0); }
void BrushSettingsDialog::updateFlow(int value)        { m_canvas->setBrushFlow(value / 100.0); }
void BrushSettingsDialog::updateRoundness(int value)   { m_canvas->setBrushRoundness(value / 100.0); }
void BrushSettingsDialog::updateAngle(int value)       { m_canvas->setBrushAngle(value); }
void BrushSettingsDialog::updateSpacing(int value)     { m_canvas->setBrushSpacing(value / 100.0); }
void BrushSettingsDialog::updateJitter(int value)      { m_canvas->setBrushJitter(value / 100.0); }

void BrushSettingsDialog::updateScatterX(int value)
{
    m_canvas->setBrushScatter(value / 100.0, m_scatterYSlider->value() / 100.0);
}

void BrushSettingsDialog::updateScatterY(int value)
{
    m_canvas->setBrushScatter(m_scatterXSlider->value() / 100.0, value / 100.0);
}

void BrushSettingsDialog::updateBlendMode(int index)
{
    if (index >= 0 && index < m_blendModeData.size())
        m_canvas->setBrushBlendMode(m_blendModeData[index]);
}

void BrushSettingsDialog::updateBrushTip(int index)
{
    if (index >= 0 && index < m_brushTipData.size())
        m_canvas->setBrushTip(m_brushTipData[index]);
}

void BrushSettingsDialog::togglePressure(bool enabled)
{
    m_pressureSizeCheckBox->setEnabled(enabled);
    m_pressureOpacityCheckBox->setEnabled(enabled);
    bool sizeEnabled = enabled && m_pressureSizeCheckBox->isChecked();
    bool opacityEnabled = enabled && m_pressureOpacityCheckBox->isChecked();
    m_canvas->setBrushPressureEnabled(enabled, sizeEnabled, opacityEnabled);
}

void BrushSettingsDialog::togglePressureSize(bool enabled)
{
    bool pressureEnabled = m_pressureCheckBox->isChecked();
    bool opacityEnabled = m_pressureOpacityCheckBox->isChecked();
    if (pressureEnabled)
        m_canvas->setBrushPressureEnabled(pressureEnabled, enabled, opacityEnabled);
}

void BrushSettingsDialog::togglePressureOpacity(bool enabled)
{
    bool pressureEnabled = m_pressureCheckBox->isChecked();
    bool sizeEnabled = m_pressureSizeCheckBox->isChecked();
    if (pressureEnabled)
        m_canvas->setBrushPressureEnabled(pressureEnabled, sizeEnabled, enabled);
}

void BrushSettingsDialog::toggleAntialiasing(bool enabled)
{
    m_canvas->setAntialiasingEnabled(enabled);
}

void BrushSettingsDialog::toggleTiltSensitivity(bool enabled)
{
    m_tiltSensitivitySlider->setEnabled(enabled);
    BrushEngine *brushEngine = m_canvas->brushEngine();
    BrushSettings settings = brushEngine->settings();
    settings.useTiltEnabled = enabled;
    brushEngine->setSettings(settings);
}

void BrushSettingsDialog::updateTiltSensitivity(int value)
{
    BrushEngine *brushEngine = m_canvas->brushEngine();
    BrushSettings settings = brushEngine->settings();
    settings.tiltSensitivity = value / 100.0;
    brushEngine->setSettings(settings);
}

void BrushSettingsDialog::toggleRotationSensitivity(bool enabled)
{
    m_rotationSensitivitySlider->setEnabled(enabled);
    BrushEngine *brushEngine = m_canvas->brushEngine();
    BrushSettings settings = brushEngine->settings();
    settings.useRotationEnabled = enabled;
    brushEngine->setSettings(settings);
}

void BrushSettingsDialog::updateRotationSensitivity(int value)
{
    BrushEngine *brushEngine = m_canvas->brushEngine();
    BrushSettings settings = brushEngine->settings();
    settings.rotationSensitivity = value / 100.0;
    brushEngine->setSettings(settings);
}

void BrushSettingsDialog::updatePressureCurve(int value)
{
    BrushEngine *brushEngine = m_canvas->brushEngine();
    BrushSettings settings = brushEngine->settings();
    settings.pressureCurve = value / 100.0;
    brushEngine->setSettings(settings);
}

void BrushSettingsDialog::resetSettings()
{
    m_hardnessSlider->setValue(80);
    m_flowSlider->setValue(100);
    m_roundnessSlider->setValue(100);
    m_angleSlider->setValue(0);
    m_spacingSlider->setValue(10);
    m_jitterSlider->setValue(0);
    m_scatterXSlider->setValue(0);
    m_scatterYSlider->setValue(0);
    m_pressureCurveSlider->setValue(50);
    m_tiltCheckBox->setChecked(false);
    m_tiltSensitivitySlider->setValue(50);
    m_rotationCheckBox->setChecked(false);
    m_rotationSensitivitySlider->setValue(50);
    
    if (m_blendModeDropDown) m_blendModeDropDown->setCurrentIndex(0);
    if (m_brushTipDropDown)  m_brushTipDropDown->setCurrentIndex(0);
    
    m_pressureCheckBox->setChecked(false);
    m_pressureSizeCheckBox->setChecked(true);
    m_pressureOpacityCheckBox->setChecked(true);
    m_antialiasingCheckBox->setChecked(true);
    
    updateHardness(m_hardnessSlider->value());
    updateFlow(m_flowSlider->value());
    updateRoundness(m_roundnessSlider->value());
    updateAngle(m_angleSlider->value());
    updateSpacing(m_spacingSlider->value());
    updateJitter(m_jitterSlider->value());
    updateScatterX(m_scatterXSlider->value());
    updateScatterY(m_scatterYSlider->value());
    updateBlendMode(m_blendModeDropDown ? m_blendModeDropDown->currentIndex() : 0);
    updateBrushTip(m_brushTipDropDown ? m_brushTipDropDown->currentIndex() : 0);
    toggleAntialiasing(m_antialiasingCheckBox->isChecked());
    togglePressure(m_pressureCheckBox->isChecked());
    updatePressureCurve(m_pressureCurveSlider->value());
    toggleTiltSensitivity(m_tiltCheckBox->isChecked());
    updateTiltSensitivity(m_tiltSensitivitySlider->value());
    toggleRotationSensitivity(m_rotationCheckBox->isChecked());
    updateRotationSensitivity(m_rotationSensitivitySlider->value());
}
