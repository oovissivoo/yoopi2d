#include "newdocumentdialog.h"
#include "popupcolorpicker.h"
#include "../CUI/cosmicdropdown.h"
#include "blenderstyleslider.h"
#include <QDebug>

NewDocumentDialog::NewDocumentDialog(QWidget *parent)
    : QDialog(parent)
    , backgroundColor(Qt::white)
    , drawModeSelected(true)
{
    setWindowTitle(tr("New Document"));
    setupUI();
}

NewDocumentDialog::~NewDocumentDialog()
{
}

QSize NewDocumentDialog::getCanvasSize() const
{
    return QSize(widthSpinBox->value(), heightSpinBox->value());
}

QColor NewDocumentDialog::getBackgroundColor() const
{
    return backgroundColor;
}

bool NewDocumentDialog::isDrawMode() const
{
    return drawModeSelected;
}

void NewDocumentDialog::setupUI()
{
    // Create main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    tabWidget = new QTabWidget(this);
    
    // Create tabs
    setupDrawTab();
    setupAnimateTab();
    
    // Add tabs to tab widget
    tabWidget->addTab(drawTab, tr("Draw"));
    tabWidget->addTab(animateTab, tr("Animate"));
    
    // Create button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    // Create OK and Cancel buttons
    okButton = new QPushButton(tr("OK"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);
    
    // Connect buttons
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // Add buttons to layout
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    // Add tab widget and button layout to main layout
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);
    
    // Connect tab changed signal
    connect(tabWidget, &QTabWidget::currentChanged, [this](int index) {
        drawModeSelected = (index == 0);
    });
    
    // Set layout
    setLayout(mainLayout);
    
    // Set minimum size
    setMinimumSize(400, 300);
}

void NewDocumentDialog::setupDrawTab()
{
    // Create draw tab
    drawTab = new QWidget(this);
    QVBoxLayout *drawLayout = new QVBoxLayout(drawTab);
    
    // Create canvas size group
    QGroupBox *canvasSizeGroup = new QGroupBox(tr("Canvas Size"), drawTab);
    QGridLayout *canvasSizeLayout = new QGridLayout(canvasSizeGroup);
    
    // Create preset sizes dropdown (migrated)
    QLabel *presetLabel = new QLabel(tr("Preset:"), canvasSizeGroup);
    mPresetSizeDropDown = new CosmicDropDown(canvasSizeGroup);
    // Build shared preset data vector and options (index 0 reserved for Custom)
    mPresetSizeData.clear();
    mPresetSizeData << QSize(800, 600) << QSize(800, 600) << QSize(1024, 768) << QSize(1280, 720) << QSize(1920, 1080);
    {
        QStringList opts;
        opts << tr("Custom") << tr("800 x 600") << tr("1024 x 768") << tr("1280 x 720 (HD)") << tr("1920 x 1080 (Full HD)");
        mPresetSizeDropDown->setOptions(opts);
        mPresetSizeDropDown->setCurrentIndex(0);
    }
    connect(mPresetSizeDropDown, &CosmicDropDown::currentIndexChanged, this, &NewDocumentDialog::setPresetSize);
    
    // Create width and height spin boxes
    QLabel *widthLabel = new QLabel(tr("Width:"), canvasSizeGroup);
    QLabel *heightLabel = new QLabel(tr("Height:"), canvasSizeGroup);
    
    widthSpinBox = new QSpinBox(canvasSizeGroup);
    heightSpinBox = new QSpinBox(canvasSizeGroup);
    
    widthSpinBox->setRange(1, 4096);
    heightSpinBox->setRange(1, 4096);
    
    widthSpinBox->setValue(800);
    heightSpinBox->setValue(600);
    
    widthSpinBox->setSuffix(" px");
    heightSpinBox->setSuffix(" px");
    
    // Add to canvas size layout
    canvasSizeLayout->addWidget(presetLabel, 0, 0);
    canvasSizeLayout->addWidget(mPresetSizeDropDown, 0, 1);
    canvasSizeLayout->addWidget(widthLabel, 1, 0);
    canvasSizeLayout->addWidget(widthSpinBox, 1, 1);
    canvasSizeLayout->addWidget(heightLabel, 2, 0);
    canvasSizeLayout->addWidget(heightSpinBox, 2, 1);
    
    // Create background color group
    QGroupBox *bgColorGroup = new QGroupBox(tr("Background Color"), drawTab);
    QHBoxLayout *bgColorLayout = new QHBoxLayout(bgColorGroup);
    
    // Create color display label
    backgroundColorLabel = new QLabel(bgColorGroup);
    backgroundColorLabel->setFixedSize(50, 30);
    backgroundColorLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    backgroundColorLabel->setAutoFillBackground(true);
    
    // Create color picker button
    colorPickerButton = new QPushButton(tr("Choose Color..."), bgColorGroup);
    connect(colorPickerButton, &QPushButton::clicked, this, &NewDocumentDialog::openColorPicker);
    
    // Add to background color layout
    bgColorLayout->addWidget(backgroundColorLabel);
    bgColorLayout->addWidget(colorPickerButton);
    bgColorLayout->addStretch();
    
    // Add groups to draw layout
    drawLayout->addWidget(canvasSizeGroup);
    drawLayout->addWidget(bgColorGroup);
    drawLayout->addStretch();
    
    // Update background color display
    updateBgColorLabel();
}

void NewDocumentDialog::setupAnimateTab()
{
    // Create animate tab
    animateTab = new QWidget(this);
    QVBoxLayout *animateLayout = new QVBoxLayout(animateTab);
    
    // Canvas size group
    QGroupBox *canvasSizeGroup = new QGroupBox(tr("Canvas Size"), animateTab);
    QGridLayout *canvasSizeLayout = new QGridLayout(canvasSizeGroup);
    
    // Create preset sizes dropdown for animation tab (shared data)
    QLabel *presetLabel = new QLabel(tr("Preset:"), canvasSizeGroup);
    mAnimPresetSizeDropDown = new CosmicDropDown(canvasSizeGroup);
    if (mPresetSizeDropDown) {
        mAnimPresetSizeDropDown->setOptions(mPresetSizeDropDown->options());
        mAnimPresetSizeDropDown->setCurrentIndex(0);
    }
    connect(mAnimPresetSizeDropDown, &CosmicDropDown::currentIndexChanged, this, &NewDocumentDialog::setPresetSize);
    
    // Use the same width and height spin boxes as the draw tab
    QLabel *widthLabel = new QLabel(tr("Width:"), canvasSizeGroup);
    QLabel *heightLabel = new QLabel(tr("Height:"), canvasSizeGroup);
    
    // Add to canvas size layout
    canvasSizeLayout->addWidget(presetLabel, 0, 0);
    canvasSizeLayout->addWidget(mAnimPresetSizeDropDown, 0, 1);
    canvasSizeLayout->addWidget(widthLabel, 1, 0);
    canvasSizeLayout->addWidget(widthSpinBox, 1, 1);
    canvasSizeLayout->addWidget(heightLabel, 2, 0);
    canvasSizeLayout->addWidget(heightSpinBox, 2, 1);
    
    // Background color group
    QGroupBox *bgColorGroup = new QGroupBox(tr("Background Color"), animateTab);
    QHBoxLayout *bgColorLayout = new QHBoxLayout(bgColorGroup);
    
    // Use the same background color settings as the draw tab
    bgColorLayout->addWidget(backgroundColorLabel);
    bgColorLayout->addWidget(colorPickerButton);
    bgColorLayout->addStretch();
    
    // Animation settings group
    QGroupBox *animSettingsGroup = new QGroupBox(tr("Animation Settings"), animateTab);
    QVBoxLayout *animSettingsLayout = new QVBoxLayout(animSettingsGroup);
    
    // Add a note about the animation system
    QLabel *noteLabel = new QLabel(tr("The animation system uses an X-sheet style timeline "
                                     "with layers organized horizontally and frames vertically."), 
                                   animSettingsGroup);
    noteLabel->setWordWrap(true);
    
    // Add the note to the animation settings layout
    animSettingsLayout->addWidget(noteLabel);
    
    // Add all groups to animate layout
    animateLayout->addWidget(canvasSizeGroup);
    animateLayout->addWidget(bgColorGroup);
    animateLayout->addWidget(animSettingsGroup);
    animateLayout->addStretch();
}

void NewDocumentDialog::updateBgColorLabel()
{
    QPalette pal = backgroundColorLabel->palette();
    pal.setColor(QPalette::Window, backgroundColor);
    backgroundColorLabel->setPalette(pal);
}

void NewDocumentDialog::openColorPicker()
{
    // Use the existing popup color picker
    PopupColorPicker colorPicker(this);
    QColor newColor = colorPicker.getColor(backgroundColor);
    
    if (newColor.isValid()) {
        backgroundColor = newColor;
        updateBgColorLabel();
    }
}

void NewDocumentDialog::setPresetSize(int index)
{
    if (index == 0) {
        // Custom size, do nothing
        return;
    }
    // Use shared preset data vector; current index maps directly
    if (index >= 0 && index < mPresetSizeData.size()) {
        const QSize presetSize = mPresetSizeData[index];
        widthSpinBox->setValue(presetSize.width());
        heightSpinBox->setValue(presetSize.height());
    }
    // Reset both dropdowns to Custom, keeping signals blocked to avoid recursion
    if (mPresetSizeDropDown) {
        QSignalBlocker b(mPresetSizeDropDown);
        mPresetSizeDropDown->setCurrentIndex(0);
    }
    if (mAnimPresetSizeDropDown) {
        QSignalBlocker b(mAnimPresetSizeDropDown);
        mAnimPresetSizeDropDown->setCurrentIndex(0);
    }
}