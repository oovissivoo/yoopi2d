#include "mainwindow.h"
#include "preferencesdialog.h"
#include "brushsettingsdialog.h"
#include "newdocumentdialog.h"
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QFrame>
#include <QIcon>
#include <QDockWidget>
#include <QSettings>
#include <QStatusBar>
#include <QPushButton>
#include <QKeyEvent>
#include <QSizePolicy>
#include <QApplication>
#include <QShowEvent>
#include <QDebug>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , canvas(nullptr)
    , scene(nullptr)
    , canvasView(nullptr)
    , colorWheelDock(nullptr)
    , colorPaletteDock(nullptr)
    , xsheetDock(nullptr)
    , xsheetWidget(nullptr)
    , colorWheel(nullptr)
    , currentColorLabel(nullptr)
    , hueSlider(nullptr)
    , opacitySlider(nullptr)
    , brushSizeSlider(nullptr)
    , brushSettingsButton(nullptr)
    , paletteColorBoxes()
    , paletteColorCheckBoxes()
    , paletteSamplerButtons()
    , paletteColors()
    , lineColorIndex(-1)
    , lineColorIndices()
    , colorSamplingMode(false)
    , samplingColorIndex(-1)
    , originalSamplingColor()
    , viewMenu(nullptr)
    , editMenu(nullptr)
    , toolsToolBar(nullptr)
    , toolSettingsToolBar(nullptr)
    , newAct(nullptr)
    , openAct(nullptr)
    , saveAct(nullptr)
    , fillAct(nullptr)
    , contiguousSelectionAct(nullptr)
    , brushAct(nullptr)
    , undoAct(nullptr)
    , redoAct(nullptr)
    , preferencesAct(nullptr)
    , brushSettingsAct(nullptr)
    , colorReplaceAct(nullptr)
    , customCursorAct(nullptr)
    , currentColor(Qt::black)
    , currentBrushSize(5)
    , currentOpacity(255)
    , fillMode(false)
    , contiguousSelectionMode(false)
    , brushToolMode(true)
    , autoLineFillMode(false)
    , colorReplaceMode(false)
    , currentFrameImage()
    , isAnimating(false)
    , rectGapFillMode(false)
    , lassoGapFillMode(false)
    , lassoUseActiveColor(false)
{
    setWindowTitle("Paintly - Animation Coloring Tool");
    resize(1200, 800);
    
    // Initialize default palette colors
    // First 3 colors are red, green, and blue
    paletteColors.append(Qt::red);       // Red
    paletteColors.append(Qt::green);     // Green
    paletteColors.append(Qt::blue);      // Blue
    
    // Fill the rest of the palette with evenly distributed hues
    for (int i = 3; i < PALETTE_SIZE; i++) {
        paletteColors.append(QColor::fromHsv(i * 360 / PALETTE_SIZE, 255, 255));
    }
    
    // Initialize all UI components
    setupColorWheel();
    setupColorPalette();
    setupCanvas();
    
    // Create actions and menus after dock widgets are set up
    createActions();
    createMenus();
    createToolBars();
    
    // Set default canvas size
    canvas->setSize(QSize(800, 600));
    
    // Initialize color from the color wheel
    currentColor = colorWheel->color();
    updateCurrentColorDisplay(currentColor);
    canvas->setColor(currentColor);
    
    // Restore window state if any
    readSettings();
    
    // Status bar for messages
    statusBar()->showMessage("Ready", 2000);
}

MainWindow::~MainWindow()
{
    writeSettings();
}

void MainWindow::writeSettings()
{
    QSettings settings("Paintly", "Paintly");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::readSettings()
{
    QSettings settings("Paintly", "Paintly");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Force autoLineFillMode to always be true, overriding any saved settings
    autoLineFillMode = true;
}

void MainWindow::createActions()
{
    // File actions
    newAct = new QAction(QIcon(":/icons/24_light_tab-new.svg"), "&New", this);
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
    
    openAct = new QAction(QIcon(":/icons/24_light_draw-freehand.svg"), "&Open", this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);
    
    saveAct = new QAction(QIcon(":/icons/24_light_draw-arrow-forward.svg"), tr("&Save"), this);
    saveAct->setShortcut(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));
    
    // Edit actions
    undoAct = new QAction(QIcon(":/icons/light_edit-undo-tristate.svg"), "&Undo", this);
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setEnabled(false);
    connect(undoAct, &QAction::triggered, this, &MainWindow::undo);
    
    redoAct = new QAction(QIcon(":/icons/24_light_edit-redo.svg"), "&Redo", this);
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setEnabled(false);
    connect(redoAct, &QAction::triggered, this, &MainWindow::redo);
    
    // Tool actions
    brushAct = new QAction(QIcon(":/icons/brush.svg"), "&Brush Tool", this);
    brushAct->setCheckable(true);
    connect(brushAct, &QAction::triggered, this, &MainWindow::toggleBrushTool);
    
    fillAct = new QAction(QIcon(":/icons/Fill.svg"), "&Fill Tool", this);
    fillAct->setCheckable(true);
    connect(fillAct, &QAction::triggered, this, &MainWindow::toggleFillMode);
    
    contiguousSelectionAct = new QAction(QIcon(":/icons/tool_contiguous_selection_cursor.svg"), "&Contiguous Selection", this);
    contiguousSelectionAct->setCheckable(true);
    connect(contiguousSelectionAct, &QAction::triggered, this, &MainWindow::toggleContiguousSelection);

    // New: Gap fill tools
    rectGapFillAct = new QAction(QIcon(":/icons/dark_tool_rect_selection.svg"), "&Rectangle Gap Fill (Sampled)", this);
    rectGapFillAct->setCheckable(true);
    rectGapFillAct->setToolTip("Drag a rectangle to fill transparent pixels inside with the sampled start color");
    connect(rectGapFillAct, &QAction::toggled, this, &MainWindow::toggleRectGapFill);

    lassoGapFillAct = new QAction(QIcon(":/icons/Fill.svg"), "&Lasso Gap Fill", this);
    lassoGapFillAct->setCheckable(true);
    lassoGapFillAct->setToolTip("Draw a lasso to fill transparent pixels inside. Mode: sampled or active color");
    connect(lassoGapFillAct, &QAction::toggled, this, &MainWindow::toggleLassoGapFill);

    // Fallback icons if QRC icons are unavailable
    auto makeRectIcon = []() -> QIcon {
        QPixmap pm(18, 18);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen1(Qt::black); pen1.setWidth(2); p.setPen(pen1); p.drawRect(3, 3, 12, 12);
        QPen pen2(Qt::white); pen2.setWidth(1); p.setPen(pen2); p.drawRect(3, 3, 12, 12);
        return QIcon(pm);
    };
    auto makeLassoIcon = []() -> QIcon {
        QPixmap pm(18, 18);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(Qt::white); pen.setStyle(Qt::DashLine); pen.setWidth(2);
        p.setPen(QPen(QColor(0,0,0,200), 3, Qt::DashLine));
        QPainterPath path; path.moveTo(3,9); path.cubicTo(6,2, 12,2, 15,9); path.cubicTo(12,16, 6,16, 3,9);
        p.drawPath(path);
        p.setPen(pen);
        p.drawPath(path);
        return QIcon(pm);
    };
    if (rectGapFillAct->icon().isNull()) rectGapFillAct->setIcon(makeRectIcon());
    if (lassoGapFillAct->icon().isNull()) lassoGapFillAct->setIcon(makeLassoIcon());

    lassoUseActiveColorAct = new QAction("Lasso uses Active Color", this);
    lassoUseActiveColorAct->setCheckable(true);
    lassoUseActiveColorAct->setToolTip("When enabled, lasso fills with current color wheel color; otherwise uses sampled start color");
    connect(lassoUseActiveColorAct, &QAction::toggled, this, &MainWindow::toggleLassoUseActiveColor);
    
    // Color replace action - appears as a checkbox in the toolbar
    colorReplaceAct = new QAction("Color Replace", this);
    colorReplaceAct->setCheckable(true);
    colorReplaceAct->setToolTip("Replace all pixels of the clicked color with current color");
    connect(colorReplaceAct, &QAction::toggled, this, &MainWindow::toggleColorReplace);
    
    // Custom cursor action
    customCursorAct = new QAction("Custom Cursor", this);
    customCursorAct->setCheckable(true);
    customCursorAct->setChecked(true); // Enabled by default
    customCursorAct->setToolTip("Use custom circular cursor for brush tool");
    connect(customCursorAct, &QAction::toggled, this, &MainWindow::toggleCustomCursor);
    
    // Preferences action
    preferencesAct = new QAction(tr("&Preferences..."), this);
    preferencesAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Comma)); // Common shortcut for preferences
    connect(preferencesAct, SIGNAL(triggered()), this, SLOT(showPreferencesDialog()));
    
    // Brush Settings action
    brushSettingsAct = new QAction(QIcon(":/icons/settings.svg"), tr("Brush &Settings..."), this);
    connect(brushSettingsAct, &QAction::triggered, this, &MainWindow::showBrushSettings);
    
    // Load custom shortcuts
    loadShortcuts();
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    
    editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(preferencesAct);
    editMenu->addAction(brushSettingsAct);
    
    viewMenu = menuBar()->addMenu("&Window");
    viewMenu->addAction(colorWheelDock->toggleViewAction());
    viewMenu->addAction(colorPaletteDock->toggleViewAction());
    
    // Add animation timeline dock toggle action if it exists
    if (xsheetDock) {
        viewMenu->addAction(xsheetDock->toggleViewAction());
    }
    
    viewMenu->addSeparator();
    viewMenu->addAction(customCursorAct);
    
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction(brushAct);
    toolsMenu->addAction(fillAct);
    toolsMenu->addAction(contiguousSelectionAct);
    toolsMenu->addSeparator();
    toolsMenu->addAction(rectGapFillAct);
    toolsMenu->addAction(lassoGapFillAct);
    toolsMenu->addAction(lassoUseActiveColorAct);
}

void MainWindow::createToolBars()
{
    // Create Tools toolbar with all the actions related to drawing tools
    toolsToolBar = addToolBar("Tools");
    toolsToolBar->setObjectName("toolsToolBar");
    toolsToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // Add only the actual drawing tools
    toolsToolBar->addAction(brushAct);
    toolsToolBar->addAction(fillAct);
    toolsToolBar->addAction(contiguousSelectionAct);
    toolsToolBar->addSeparator();
    toolsToolBar->addAction(rectGapFillAct);
    toolsToolBar->addAction(lassoGapFillAct);
    toolsToolBar->addAction(lassoUseActiveColorAct);
    
    // Create Tool Settings toolbar for brush settings and other options
    toolSettingsToolBar = addToolBar("Tool Settings");
    toolSettingsToolBar->setObjectName("toolSettingsToolBar");
    
    // Add color replace action to tool settings toolbar (initially hidden)
    toolSettingsToolBar->addAction(colorReplaceAct);
    colorReplaceAct->setVisible(false); // Only show when fill tool is active
    
    // Add custom cursor action to tool settings toolbar
    toolSettingsToolBar->addAction(customCursorAct);
    
    // Add a label and slider for brush size
    QLabel *brushSizeLabel = new QLabel(tr("Brush Size:"));
    toolSettingsToolBar->addWidget(brushSizeLabel);
    
    // Create the brush size slider directly in the toolbar
    brushSizeSlider = new BlenderStyleSlider(Qt::Horizontal);
    brushSizeSlider->setRange(1, 18);
    brushSizeSlider->setValue(currentBrushSize);
    brushSizeSlider->setDefaultValue(5);
    brushSizeSlider->setValueDisplayFormat("Size: %d");
    brushSizeSlider->setFixedWidth(100);
    connect(brushSizeSlider, &BlenderStyleSlider::valueChanged,
            this, &MainWindow::setBrushSize);
    toolSettingsToolBar->addWidget(brushSizeSlider);
    
    toolSettingsToolBar->addSeparator();
    
    // Add a label and slider for opacity
    QLabel *opacityLabel = new QLabel(tr("Opacity:"));
    toolSettingsToolBar->addWidget(opacityLabel);
    
    // Create the opacity slider directly in the toolbar
    opacitySlider = new BlenderStyleSlider(Qt::Horizontal);
    opacitySlider->setRange(1, 100);
    // Convert the current opacity from 0-255 to 1-100 range
    int opacityPercentage = qRound(currentOpacity * 100.0 / 255.0);
    opacitySlider->setValue(opacityPercentage);
    opacitySlider->setDefaultValue(100);
    opacitySlider->setValueDisplayFormat("Opacity: %d%%");
    opacitySlider->setFixedWidth(120);
    connect(opacitySlider, &BlenderStyleSlider::valueChanged,
            this, &MainWindow::setOpacity);
    toolSettingsToolBar->addWidget(opacitySlider);
    
    toolSettingsToolBar->addSeparator();
    
    // Add brush settings button to the tool settings toolbar
    brushSettingsButton = new QPushButton(QIcon(":/icons/settings.svg"), "");
    brushSettingsButton->setToolTip(tr("Configure brush settings"));
    brushSettingsButton->setFixedSize(24, 24);
    brushSettingsButton->setIconSize(QSize(16, 16));
    connect(brushSettingsButton, &QPushButton::clicked, this, &MainWindow::showBrushSettings);
    toolSettingsToolBar->addWidget(brushSettingsButton);
    
    // Initially disable brush settings when app starts (will be enabled when brush tool is selected)
    brushSizeSlider->setEnabled(false);
    opacitySlider->setEnabled(false);
    brushSettingsButton->setEnabled(false);
    brushSettingsButton->setVisible(false);
}

void MainWindow::setupColorWheel()
{
    // Create color wheel dock widget
    colorWheelDock = new QDockWidget("Color Wheel", this);
    colorWheelDock->setObjectName("colorWheelDock"); // For saving state
    colorWheelDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    
    QWidget *colorWheelWidget = new QWidget(colorWheelDock);
    QVBoxLayout *colorWheelLayout = new QVBoxLayout(colorWheelWidget);
    
    // Color wheel
    colorWheel = new ColorWheel(this);
    connect(colorWheel, &ColorWheel::colorChanged, this, &MainWindow::setColor);
    colorWheelLayout->addWidget(colorWheel);
    
    // Current color display
    currentColorLabel = new QLabel(this);
    currentColorLabel->setFixedSize(30, 30);
    currentColorLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    currentColorLabel->setAutoFillBackground(true);
    
    // We'll initialize the color in the constructor after all components are set up
    
    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(currentColorLabel);
    bottomLayout->addStretch();
    colorWheelLayout->addLayout(bottomLayout);
    
    colorWheelWidget->setLayout(colorWheelLayout);
    colorWheelDock->setWidget(colorWheelWidget);
    addDockWidget(Qt::RightDockWidgetArea, colorWheelDock);
}

void MainWindow::updateCurrentColorDisplay(const QColor &color)
{
    QPalette pal = currentColorLabel->palette();
    pal.setColor(QPalette::Window, color);
    currentColorLabel->setPalette(pal);
}

void MainWindow::setColor(const QColor &color)
{
    currentColor = color;
    canvas->setColor(color);
    updateCurrentColorDisplay(color);
}

void MainWindow::setupColorPalette()
{
    // Create color palette dock widget
    colorPaletteDock = new QDockWidget("Color Palette", this);
    colorPaletteDock->setObjectName("colorPaletteDock"); // For saving state
    colorPaletteDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    colorPaletteDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    QWidget *colorPaletteWidget = new QWidget(colorPaletteDock);
    QGridLayout *colorPaletteLayout = new QGridLayout(colorPaletteWidget);
    colorPaletteLayout->setSpacing(5);
    colorPaletteLayout->setContentsMargins(5, 5, 5, 5);
    
    // Create color boxes and checkboxes
    for (int i = 0; i < PALETTE_SIZE; i++) {
        // Create color box
        QLabel *colorBox = new QLabel(colorPaletteWidget);
        colorBox->setFixedSize(40, 40);
        colorBox->setFrameStyle(QFrame::Panel | QFrame::Sunken);
        colorBox->setAutoFillBackground(true);
        
        // Set initial color
        QPalette pal = colorBox->palette();
        pal.setColor(QPalette::Window, paletteColors[i]);
        colorBox->setPalette(pal);
        
        // Make the color box clickable
        colorBox->setCursor(Qt::PointingHandCursor);
        colorBox->setProperty("index", i);
        colorBox->installEventFilter(this);
        
        // Create checkbox for AutoLine fill line color selection
        QCheckBox *checkBox = new QCheckBox(colorPaletteWidget);
        checkBox->setProperty("index", i);
        checkBox->setToolTip("Select as line color for boundary detection (lines of this color will act as fill boundaries)");
        checkBox->setAutoExclusive(false);
        
        // Set custom icons for checkbox
        checkBox->setIcon(QIcon(":/icons/uncheckedBox.svg"));
        checkBox->setIconSize(QSize(16, 16));
        checkBox->setStyleSheet("QCheckBox::indicator { width: 0; height: 0; } QCheckBox { padding: 0; margin: 0; }");
        
        // Connect to toggled signal to update icon
        connect(checkBox, &QCheckBox::toggled, [checkBox](bool checked) {
            if (checked) {
                checkBox->setIcon(QIcon(":/icons/checkedBox.svg"));
            } else {
                checkBox->setIcon(QIcon(":/icons/uncheckedBox.svg"));
            }
        });
        
        connect(checkBox, &QCheckBox::toggled, [this, i](bool checked) {
            toggleLineColorSelection(i, checked);
        });
        
        // Create color sampler button
        QPushButton *samplerButton = new QPushButton(colorPaletteWidget);
        samplerButton->setIcon(QIcon(":/icons/sampleColor.svg"));
        samplerButton->setIconSize(QSize(16, 16));
        samplerButton->setText("");
        samplerButton->setToolTip("Sample color from canvas");
        samplerButton->setFixedSize(20, 20);
        samplerButton->setProperty("index", i);
        connect(samplerButton, &QPushButton::clicked, [this, i]() {
            startColorSampling(i);
        });
        
        // Add to layout
        colorPaletteLayout->addWidget(colorBox, 0, i);
        colorPaletteLayout->addWidget(checkBox, 1, i, Qt::AlignHCenter);
        colorPaletteLayout->addWidget(samplerButton, 0, i, Qt::AlignRight | Qt::AlignTop);
        
        // Store in members
        paletteColorBoxes.append(colorBox);
        paletteColorCheckBoxes.append(checkBox);
        paletteSamplerButtons.append(samplerButton);
    }
    
    colorPaletteWidget->setLayout(colorPaletteLayout);
    
    // Set the size policy for the widget - allow it to be stretched horizontally but fixed vertically
    colorPaletteWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    colorPaletteWidget->adjustSize();
    
    colorPaletteDock->setWidget(colorPaletteWidget);
    
    // Set only a minimum height, not a minimum width
    QSize contentSize = colorPaletteWidget->sizeHint();
    colorPaletteDock->setMinimumHeight(contentSize.height());
    
    addDockWidget(Qt::BottomDockWidgetArea, colorPaletteDock);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Check if the event is for a color box in our palette
    if (event->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < paletteColorBoxes.size(); i++) {
            if (obj == paletteColorBoxes[i]) {
                openColorPicker(i);
                return true;
            }
        }
    }
    
    // Call base class implementation for other events
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::openColorPicker(int index)
{
    if (index >= 0 && index < paletteColors.size()) {
        QColor currentPaletteColor = paletteColors[index];
        
        // Create and use our custom popup color picker
        PopupColorPicker colorPicker(this);
        QColor newColor = colorPicker.getColor(currentPaletteColor);
        
        if (newColor.isValid() && newColor != currentPaletteColor) {
            // Update the stored color
            paletteColors[index] = newColor;
            
            // Update the color box display
            QPalette pal = paletteColorBoxes[index]->palette();
            pal.setColor(QPalette::Window, newColor);
            paletteColorBoxes[index]->setPalette(pal);
            
            // If this is the selected line color and we're using AutoLine fill,
            // update the line color
            if (index == lineColorIndex && autoLineFillMode) {
                updateAutoLineFillState();
            }
        }
    }
}

void MainWindow::toggleLineColorSelection(int index, bool checked)
{
    if (checked) {
        // Add to selected line colors list
        if (!lineColorIndices.contains(index)) {
            lineColorIndices.append(index);
        }
        
        // For backward compatibility
        lineColorIndex = index;
    } else {
        // Remove from selected line colors list
        lineColorIndices.removeAll(index);
        
        // For backward compatibility
        if (index == lineColorIndex) {
            lineColorIndex = lineColorIndices.isEmpty() ? -1 : lineColorIndices.last();
        }
    }
    
    // Update AutoLine fill state with the new selection
    updateAutoLineFillState();
}

void MainWindow::updateAutoLineFillState()
{
    // Enable AutoLine fill only if at least one color is selected in the palette
    bool hasSelectedColors = !lineColorIndices.isEmpty();
    
    canvas->setAutoLineFill(hasSelectedColors);
    
    if (hasSelectedColors) {
        // Clear previous line colors and add all selected colors
        canvas->clearLineColors();
        for (int index : lineColorIndices) {
            canvas->addLineColor(paletteColors[index]);
        }
        
        // For backwards compatibility
        if (lineColorIndex >= 0) {
            canvas->setLineColor(paletteColors[lineColorIndex]);
        }
        
        QString message = "Line colors selected for boundary detection: ";
        for (int i = 0; i < lineColorIndices.size(); ++i) {
            message += QString::number(lineColorIndices[i] + 1);
            if (i < lineColorIndices.size() - 1) {
                message += ", ";
            }
        }
        statusBar()->showMessage(message, 3000);
    } else {
        statusBar()->showMessage("No line colors selected for boundary detection", 3000);
    }
}

void MainWindow::toggleBrushTool()
{
    setActiveTool(brushAct);
}

void MainWindow::toggleFillMode()
{
    setActiveTool(fillAct);
}

void MainWindow::toggleContiguousSelection()
{
    contiguousSelectionMode = !contiguousSelectionMode;
    canvas->setContiguousSelection(contiguousSelectionMode);
    if (contiguousSelectionMode) {
        brushAct->setChecked(false);
        fillAct->setChecked(false);
        // Turn off gap fill modes when contiguous selection is active
        rectGapFillAct->setChecked(false);
        lassoGapFillAct->setChecked(false);
        canvas->setRectGapFillMode(false);
        canvas->setLassoGapFillMode(false);
    }
}

void MainWindow::toggleRectGapFill(bool enabled)
{
    rectGapFillMode = enabled;
    if (enabled) {
        // disable other tool modes to avoid conflicts
        brushAct->setChecked(false);
        fillAct->setChecked(false);
        contiguousSelectionAct->setChecked(false);
        lassoGapFillAct->setChecked(false);
        canvas->setFillMode(false);
        canvas->setContiguousSelection(false);
        canvas->setLassoGapFillMode(false);
    }
    canvas->setRectGapFillMode(enabled);
}

void MainWindow::toggleLassoGapFill(bool enabled)
{
    lassoGapFillMode = enabled;
    if (enabled) {
        // disable other tool modes to avoid conflicts
        brushAct->setChecked(false);
        fillAct->setChecked(false);
        contiguousSelectionAct->setChecked(false);
        rectGapFillAct->setChecked(false);
        canvas->setFillMode(false);
        canvas->setContiguousSelection(false);
        canvas->setRectGapFillMode(false);
    }
    canvas->setLassoGapFillMode(enabled);
}

void MainWindow::toggleLassoUseActiveColor(bool enabled)
{
    lassoUseActiveColor = enabled;
    canvas->setLassoUseActiveColor(enabled);
}

void MainWindow::toggleColorReplace(bool enabled)
{
    colorReplaceMode = enabled;
    // Pass the color replace mode to canvas
    canvas->setColorReplaceMode(enabled);
    
    if (enabled) {
        statusBar()->showMessage("Color replace mode enabled - will replace all pixels of clicked color", 3000);
    } else {
        statusBar()->showMessage("Color replace mode disabled - normal fill behavior", 3000);
    }
}

void MainWindow::setActiveTool(QAction *action)
{
    // Uncheck all tool actions
    fillAct->setChecked(false);
    contiguousSelectionAct->setChecked(false);
    brushAct->setChecked(false);
    
    // Also uncheck gap fill tool actions when switching main tool
    if (rectGapFillAct) rectGapFillAct->setChecked(false);
    if (lassoGapFillAct) lassoGapFillAct->setChecked(false);
    canvas->setRectGapFillMode(false);
    canvas->setLassoGapFillMode(false);
    
    // Check the active tool
    action->setChecked(true);
    
    // Reset all tool mode flags
    fillMode = false;
    contiguousSelectionMode = false;
    brushToolMode = false;
    
    // Clear all actions from the tool settings toolbar
    toolSettingsToolBar->clear();
    
    // Make sure sliders and button are initially hidden
    brushSizeSlider->setVisible(false);
    opacitySlider->setVisible(false);
    brushSettingsButton->setVisible(false);
    
    // Make fill tool options initially hidden
    colorReplaceAct->setVisible(false);
    
    // Set the appropriate mode flag and add relevant settings
    if (action == fillAct) {
        fillMode = true;
        statusBar()->showMessage("Fill tool selected", 2000);
        
        // Make fill tool options visible
        colorReplaceAct->setVisible(true);
        
        // Add fill tool specific settings
        toolSettingsToolBar->addAction(colorReplaceAct);
        
        // Make sure brush settings are not visible
        brushSizeSlider->setEnabled(false);
        opacitySlider->setEnabled(false);
        brushSettingsButton->setEnabled(false);
    } 
    else if (action == contiguousSelectionAct) {
        contiguousSelectionMode = true;
        statusBar()->showMessage("Contiguous selection tool selected", 2000);
        // No specific settings for contiguous selection tool
        
        // Make sure brush settings are not visible
        brushSizeSlider->setEnabled(false);
        opacitySlider->setEnabled(false);
        brushSettingsButton->setEnabled(false);
    }
    else if (action == brushAct) {
        brushToolMode = true;
        statusBar()->showMessage("Brush tool selected", 2000);
        
        // Make brush settings visible and enabled
        brushSizeSlider->setVisible(true);
        brushSizeSlider->setEnabled(true);
        opacitySlider->setVisible(true);
        opacitySlider->setEnabled(true);
        brushSettingsButton->setVisible(true);
        brushSettingsButton->setEnabled(true);
        
        // Create and add brush size controls
        QLabel *brushSizeLabel = new QLabel(tr("Brush Size:"));
        toolSettingsToolBar->addWidget(brushSizeLabel);
        toolSettingsToolBar->addWidget(brushSizeSlider);
        
        toolSettingsToolBar->addSeparator();
        
        // Create and add opacity controls
        QLabel *opacityLabel = new QLabel(tr("Opacity:"));
        toolSettingsToolBar->addWidget(opacityLabel);
        toolSettingsToolBar->addWidget(opacitySlider);
        
        toolSettingsToolBar->addSeparator();
        
        // Add brush settings button
        toolSettingsToolBar->addWidget(brushSettingsButton);
    }
    
    // Update canvas state
    canvas->setFillMode(fillMode);
    canvas->setContiguousSelection(contiguousSelectionMode);
    
    // Force update of the toolbar to ensure visibility changes are applied
    toolSettingsToolBar->update();
}

void MainWindow::setupCanvas()
{
    scene = new QGraphicsScene(this);
    canvasView = new QGraphicsView(scene);
    
    // Set OpenGL widget as viewport for hardware acceleration
    canvasView->setViewport(new QOpenGLWidget());
    
    // Enable antialiasing
    canvasView->setRenderHint(QPainter::Antialiasing);
    
    // Add optimization flags to improve performance
    canvasView->setOptimizationFlags(QGraphicsView::DontSavePainterState | 
                                    QGraphicsView::DontAdjustForAntialiasing);
    
    // Set viewport update mode to minimal for better performance
    canvasView->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    
    canvasView->setDragMode(QGraphicsView::NoDrag);
    canvasView->setBackgroundBrush(QBrush(QColor(64, 64, 64)));
    
    // Create canvas
    canvas = new Canvas();
    scene->addItem(canvas);
    
    // Create Xsheet widget
    xsheetWidget = new XsheetWidget(this);
    xsheetDock = new QDockWidget(tr("Animation Timeline"), this);
    xsheetDock->setObjectName("animationTimelineDock");
    xsheetDock->setWidget(xsheetWidget);
    xsheetDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, xsheetDock);
    
    // Make sure the animation timeline is visible by default
    xsheetDock->setVisible(true);
    
    // Connect Xsheet signals
    connect(xsheetWidget, &XsheetWidget::currentFrameChanged,
            this, &MainWindow::onCurrentFrameChanged);
    connect(xsheetWidget, &XsheetWidget::currentLayerChanged,
            this, &MainWindow::onCurrentLayerChanged);
    connect(xsheetWidget, &XsheetWidget::keyframeAdded,
            this, &MainWindow::onKeyframeAdded);
    connect(xsheetWidget, &XsheetWidget::keyframeRemoved,
            this, &MainWindow::onKeyframeRemoved);
    connect(xsheetWidget, &XsheetWidget::layersReordered,
            this, &MainWindow::updateCanvasFromCurrentFrame);
    
    // Connect Canvas signals
    connect(canvas, &Canvas::frameUpdated,
            this, &MainWindow::onFrameUpdated);
    
    // Initialize animation state
    isAnimating = false;
    currentFrameImage = QImage();
    
    // Connect canvas signals for handling color sampling
    connect(canvas, &Canvas::canvasClicked, this, [this](const QPointF &pos) {
        if (colorSamplingMode) {
            sampleColorFromCanvas(pos);
        }
    });
    
    // Connect to right-click signal to handle color sampling cancellation
    connect(canvas, &Canvas::canvasRightClicked, this, [this](const QPointF &pos) {
        Q_UNUSED(pos);
        if (colorSamplingMode) {
            // Restore original color
            if (samplingColorIndex >= 0 && samplingColorIndex < PALETTE_SIZE) {
                paletteColors[samplingColorIndex] = originalSamplingColor;
                
                // Update the color box display
                QPalette pal = paletteColorBoxes[samplingColorIndex]->palette();
                pal.setColor(QPalette::Window, originalSamplingColor);
                paletteColorBoxes[samplingColorIndex]->setPalette(pal);
                
                // Reset sampling mode
                colorSamplingMode = false;
                samplingColorIndex = -1;
                originalSamplingColor = QColor();
                
                // Restore cursor
                QApplication::restoreOverrideCursor();
                statusBar()->showMessage("Color sampling canceled", 2000);
            }
        }
    });
    
    connect(canvas, &Canvas::canvasMouseMoved, this, [this](const QPointF &pos) {
        if (colorSamplingMode) {
            previewSampleColorFromCanvas(pos);
        }
    });
    
    // Connect the undo/redo signals from canvas
    connect(canvas, &Canvas::undoAvailable, this, [this](bool available) {
        undoAct->setEnabled(available);
    });
    
    connect(canvas, &Canvas::redoAvailable, this, [this](bool available) {
        redoAct->setEnabled(available);
    });
    
    // Set view properties
    // SmartViewportUpdate: only redraws regions that actually changed.
    // FullViewportUpdate was overriding the MinimalViewportUpdate set above
    // and causing the entire screen to redraw on every event — very expensive
    // at high zoom where the canvas fills the whole viewport.
    canvasView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    canvasView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    canvasView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    canvasView->setFrameStyle(QFrame::NoFrame);
    canvasView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    canvasView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    
    // Set up central widget
    setCentralWidget(canvasView);
}

void MainWindow::onFrameUpdated(const QImage &frame)
{
    // Get current layer and frame
    QString currentLayer = xsheetWidget->getCurrentLayer();
    int currentFrame = xsheetWidget->getCurrentFrame();
    
    // We need to isolate just what the user drew on the current layer
    
    // 1. Create a blank image for the current layer if it doesn't exist yet
    QImage layerImage = xsheetWidget->getFrameImage(currentLayer, currentFrame);
    if (layerImage.isNull() || layerImage.size() != frame.size()) {
        layerImage = QImage(frame.size(), QImage::Format_ARGB32);
        layerImage.fill(Qt::transparent);
    }
    
    // 2. Create what the canvas SHOULD look like with all other layers 
    // but WITHOUT the current layer's content
    QImage otherLayersOnly(frame.size(), QImage::Format_ARGB32);
    otherLayersOnly.fill(Qt::transparent);
    
    QPainter otherPainter(&otherLayersOnly);
    
    // Iterate through layers in their display order (bottom to top)
    for (int i = 0; i < xsheetWidget->getLayerCount(); i++) {
        QString layerName = xsheetWidget->getLayerName(i);
        
        // Skip the current layer and hidden layers
        if (layerName == currentLayer || !xsheetWidget->isLayerVisible(layerName)) {
            continue;
        }
        
        // Find visible frame for this layer
        int visibleFrame = xsheetWidget->findVisibleFrameAt(layerName, currentFrame);
        if (visibleFrame > 0) {
            QImage img = xsheetWidget->getFrameImage(layerName, visibleFrame);
            if (!img.isNull()) {
                otherPainter.drawImage(0, 0, img);
            }
        }
    }
    otherPainter.end();
    
    // 3. Create a separate image that will ONLY contain new pixels drawn by the user
    QImage newUserDrawing(frame.size(), QImage::Format_ARGB32);
    newUserDrawing.fill(Qt::transparent);
    
    // Compare the full composite (frame) with what it would look like without this layer
    for (int y = 0; y < frame.height(); y++) {
        for (int x = 0; x < frame.width(); x++) {
            QColor framePixel = frame.pixelColor(x, y);
            QColor otherLayersPixel = otherLayersOnly.pixelColor(x, y);
            
            // If the pixel differs, it must be from the user's drawing on this layer
            if (framePixel != otherLayersPixel) {
                // Keep only the user's drawing - isolate what they added
                newUserDrawing.setPixelColor(x, y, framePixel);
            }
        }
    }
    
    // 4. Now we can simply replace the layer image with just the new drawing
    // This completely solves the compounding issue because we're isolating exactly
    // what the user drew
    xsheetWidget->setFrameImage(currentLayer, currentFrame, newUserDrawing);
    
    // Update status bar
    QString frameName = xsheetWidget->getFrameName(currentLayer, currentFrame);
    statusBar()->showMessage(QString("Updated frame: %1 (Layer: %2, Frame: %3)")
                          .arg(frameName)
                          .arg(currentLayer)
                          .arg(currentFrame), 3000);
}

void MainWindow::newFile()
{
    // Create and show the new document dialog
    NewDocumentDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Get the settings from the dialog
        QSize canvasSize = dialog.getCanvasSize();
        QColor backgroundColor = dialog.getBackgroundColor();
        bool isDrawMode = dialog.isDrawMode();
        
        // Set the canvas size
        canvas->setSize(canvasSize);
        
        // Fill with the selected background color
        QImage newImage(canvasSize, QImage::Format_ARGB32);
        newImage.fill(backgroundColor);
        canvas->setImage(newImage);
        
        // Reset animation state
        isAnimating = false;
        currentFrameImage = QImage();
        
        // If this is animate mode, setup the animation timeline
        if (!isDrawMode) {
            // Turn on animation mode
            isAnimating = true;
            
            // Clear existing data in xsheetWidget
            QString currentLayer = xsheetWidget->getCurrentLayer();
            if (!currentLayer.isEmpty()) {
                xsheetWidget->removeLayer(currentLayer);
            }
            
            // Set up initial layer and keyframe
            xsheetWidget->addLayer("A");
            xsheetWidget->setCurrentLayer("A");
            xsheetWidget->addKeyframe(1);
            xsheetWidget->setCurrentFrame(1);
            
            // Make sure the animation timeline is visible
            if (xsheetDock) {
                xsheetDock->setVisible(true);
                xsheetDock->raise();
            }
            
            // Update canvas with the initial frame
            updateCanvasFromCurrentFrame();
            
            // Show a message to the user
            statusBar()->showMessage("Animation mode enabled. Use the timeline at the bottom to add keyframes and layers.", 5000);
        }
        else {
            // In draw mode, hide the animation timeline
            if (xsheetDock) {
                xsheetDock->setVisible(false);
            }
        }
        
        statusBar()->showMessage(QString("New document created: %1 x %2")
                               .arg(canvasSize.width()).arg(canvasSize.height()), 2000);
    }
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open Image", "", "Image Files (*.png *.jpg *.bmp)");
    
    if (!fileName.isEmpty()) {
        QImage image(fileName);
        if (!image.isNull()) {
            canvas->setSize(image.size());
            canvas->setImage(image);
        } else {
            QMessageBox::warning(this, "Error", "Could not load image!");
        }
    }
}

void MainWindow::saveFile()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Image", "", "PNG Files (*.png);;JPEG Files (*.jpg);;BMP Files (*.bmp)");
    
    if (!fileName.isEmpty()) {
        if (!canvas->getImage().save(fileName)) {
            QMessageBox::warning(this, "Error", "Could not save image!");
        }
    }
}

void MainWindow::setBrushSize(int size)
{
    currentBrushSize = size;
    canvas->setBrushSize(size);
}

void MainWindow::setOpacity(int opacityPercentage)
{
    // Convert from percentage (1-100) to 0-255 range for internal use
    int opacity = qRound(opacityPercentage * 255.0 / 100.0);
    currentOpacity = opacity;
    canvas->setOpacity(opacity);
}

void MainWindow::startColorSampling(int index)
{
    if (index >= 0 && index < PALETTE_SIZE) {
        // Store the original color for possible cancellation
        originalSamplingColor = paletteColors[index];
        
        // Enter color sampling mode
        colorSamplingMode = true;
        samplingColorIndex = index;
        
        // Update canvas to disable other tools
        canvas->setColorSamplingMode(true);
        
        // Change cursor to indicate sampling mode
        setCursor(Qt::CrossCursor);
        
        // Connect to mouse move events for continuous sampling
        connect(canvas, &Canvas::canvasMouseMoved, this, &MainWindow::previewSampleColorFromCanvas);
        
        // Show a message to the user
        statusBar()->showMessage("Move over colors to preview. Click to select, right-click or ESC to cancel.", 5000);
    }
}

// This method handles mouse movement for continuous color preview
void MainWindow::previewSampleColorFromCanvas(const QPointF &pos)
{
    if (!colorSamplingMode || samplingColorIndex < 0) return;
    
    // Get the color at the current position
    QPointF canvasPos = canvas->mapFromScene(pos);
    
    // Check if the position is within the canvas image bounds
    if (canvasPos.x() >= 0 && canvasPos.x() < canvas->getImage().width() &&
        canvasPos.y() >= 0 && canvasPos.y() < canvas->getImage().height()) {
        
        // Sample the color from the canvas image
        QColor sampledColor = canvas->getImage().pixelColor(canvasPos.toPoint());
        
        // If the sampled pixel has color (not fully transparent)
        if (sampledColor.alpha() > 0) {
            // Update the color box display with the previewed color - don't update the stored color yet
            QPalette pal = paletteColorBoxes[samplingColorIndex]->palette();
            pal.setColor(QPalette::Window, sampledColor);
            paletteColorBoxes[samplingColorIndex]->setPalette(pal);
        }
    }
}

void MainWindow::sampleColorFromCanvas(const QPointF &pos)
{
    if (samplingColorIndex >= 0 && samplingColorIndex < PALETTE_SIZE) {
        // Get the color at the clicked position
        QPointF canvasPos = canvas->mapFromScene(pos);
        
        // Check if the position is within the canvas image bounds
        if (canvasPos.x() >= 0 && canvasPos.x() < canvas->getImage().width() &&
            canvasPos.y() >= 0 && canvasPos.y() < canvas->getImage().height()) {
            
            // Sample the color from the canvas image
            QColor sampledColor = canvas->getImage().pixelColor(canvasPos.toPoint());
            
            // If the sampled pixel has color (not fully transparent)
            if (sampledColor.alpha() > 0) {
                // Update the stored color
                paletteColors[samplingColorIndex] = sampledColor;
                
                // Update the color box display
                QPalette pal = paletteColorBoxes[samplingColorIndex]->palette();
                pal.setColor(QPalette::Window, sampledColor);
                paletteColorBoxes[samplingColorIndex]->setPalette(pal);
                
                // If this is the selected line color and we're using AutoLine fill,
                // update the line color
                if (lineColorIndices.contains(samplingColorIndex) && autoLineFillMode) {
                    updateAutoLineFillState();
                }
                
                statusBar()->showMessage("Color selected successfully", 2000);
            } else {
                // If transparent pixel, revert to original color
                QPalette pal = paletteColorBoxes[samplingColorIndex]->palette();
                pal.setColor(QPalette::Window, originalSamplingColor);
                paletteColorBoxes[samplingColorIndex]->setPalette(pal);
                statusBar()->showMessage("Couldn't sample color (transparent pixel)", 2000);
            }
        } else {
            // If outside the canvas bounds, revert to original color
            QPalette pal = paletteColorBoxes[samplingColorIndex]->palette();
            pal.setColor(QPalette::Window, originalSamplingColor);
            paletteColorBoxes[samplingColorIndex]->setPalette(pal);
            statusBar()->showMessage("Click was outside the canvas bounds", 2000);
        }
    }
    
    // Exit sampling mode
    colorSamplingMode = false;
    disconnect(canvas, &Canvas::canvasMouseMoved, this, &MainWindow::previewSampleColorFromCanvas);
    samplingColorIndex = -1;
    
    // Reset canvas to allow other tools
    canvas->setColorSamplingMode(false);
    
    setCursor(Qt::ArrowCursor);
}

// Add the keyPressEvent handler to handle Escape key
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Handle Escape key to cancel color sampling mode
    if (event->key() == Qt::Key_Escape && colorSamplingMode) {
        // Restore original color
        if (samplingColorIndex >= 0 && samplingColorIndex < PALETTE_SIZE) {
            QPalette pal = paletteColorBoxes[samplingColorIndex]->palette();
            pal.setColor(QPalette::Window, originalSamplingColor);
            paletteColorBoxes[samplingColorIndex]->setPalette(pal);
        }
        
        // Cancel color sampling mode
        colorSamplingMode = false;
        disconnect(canvas, &Canvas::canvasMouseMoved, this, &MainWindow::previewSampleColorFromCanvas);
        samplingColorIndex = -1;
        
        // Reset canvas to allow other tools
        canvas->setColorSamplingMode(false);
        
        setCursor(Qt::ArrowCursor);
        statusBar()->showMessage("Color sampling canceled", 2000);
        return;
    }
    
    // Check for Ctrl+Z (Undo) and Ctrl+Y/Ctrl+Shift+Z (Redo)
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z) {
            if (event->modifiers() & Qt::ShiftModifier) {
                // Ctrl+Shift+Z for Redo
                if (canvas->canRedo()) {
                    redo();
                    event->accept();
                    return;
                }
            } else {
                // Ctrl+Z for Undo
                if (canvas->canUndo()) {
                    undo();
                    event->accept();
                    return;
                }
            }
        } else if (event->key() == Qt::Key_Y) {
            // Ctrl+Y for Redo
            if (canvas->canRedo()) {
                redo();
                event->accept();
                return;
            }
        }
    }
    
    // Handle space key press for panning (for canvas)
    if (event->key() == Qt::Key_Space) {
        // This will be handled by the canvas if it has focus
        if (!canvasView->hasFocus()) {
            // Give focus to canvasView instead of trying to forward the event
            canvasView->setFocus();
        }
    }
    
    // Call base class implementation for other keys
    QMainWindow::keyPressEvent(event);
}

// Add new undo/redo methods
void MainWindow::undo()
{
    if (canvas->canUndo()) {
        canvas->undo();
        statusBar()->showMessage("Undo", 2000);
    }
}

void MainWindow::redo()
{
    if (canvas->canRedo()) {
        canvas->redo();
        statusBar()->showMessage("Redo", 2000);
    }
}

void MainWindow::updateUndoRedoActions(bool available)
{
    Q_UNUSED(available);
    // This slot is connected to both undoAvailable and redoAvailable signals
    // and will be reimplemented if needed for more complex logic
    // Currently, the direct connections to setEnabled handle our needs
}

void MainWindow::showPreferencesDialog()
{
    PreferencesDialog *dialog = new PreferencesDialog(this);
    dialog->setActions(getAllActions());
    
    // Connect canvas to preferences dialog for performance settings
    dialog->setCanvas(canvas);
    
    if (dialog->exec() == QDialog::Accepted) {
        // The preferences will be applied directly as settings are changed
    }
    
    delete dialog;
}

void MainWindow::showBrushSettings()
{
    BrushSettingsDialog dialog(canvas, this);
    dialog.exec();
}

void MainWindow::loadShortcuts()
{
    QSettings settings("Paintly", "Paintly");
    settings.beginGroup("Shortcuts");
    
    // Apply shortcuts to actions if they exist in settings
    QStringList shortcutKeys = settings.childKeys();
    for (const QString &key : shortcutKeys) {
        // Find the action with this name
        for (QAction *action : findChildren<QAction*>()) {
            if (action->text().remove('&') == key) {
                action->setShortcut(QKeySequence(settings.value(key).toString()));
                break;
            }
        }
    }
    
    settings.endGroup();
}

// After we've set up everything, initialize the default tool
void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // Ensure the brush tool is selected by default when the application is shown
    // This is a safer place to do it than in setupCanvas, since all UI elements are created
    if (brushAct) {
        setActiveTool(brushAct);
    }
}

QList<QAction*> MainWindow::getAllActions()
{
    QList<QAction*> allActions;
    
    // Add file menu actions
    allActions << newAct << openAct << saveAct;
    
    // Add edit menu actions
    allActions << undoAct << redoAct;
    
    // Add tool actions
    allActions << brushAct << fillAct << contiguousSelectionAct;
    
    // Add view actions if any
    // allActions << ...
    
    // Add preferences action itself
    allActions << preferencesAct;
    
    // Add any other actions that should be customizable
    allActions << brushSettingsAct;
    
    return allActions;
}

// Implement the custom cursor toggle function
void MainWindow::toggleCustomCursor(bool enabled)
{
    if (canvas) {
        canvas->setCustomCursorEnabled(enabled);
    }
}

void MainWindow::onCurrentFrameChanged(int frame)
{
    if (!isAnimating) {
        updateCanvasFromCurrentFrame();
    }
    
    // Update status bar with current frame info
    QString currentLayer = xsheetWidget->getCurrentLayer();
    QString frameName = xsheetWidget->getFrameName(currentLayer, frame);
    if (!frameName.isEmpty()) {
        statusBar()->showMessage(QString("Current frame: %1 (Layer: %2, Frame: %3)")
                              .arg(frameName)
                              .arg(currentLayer)
                              .arg(frame), 2000);
    }
}

void MainWindow::onCurrentLayerChanged(const QString &layer)
{
    updateCanvasFromCurrentFrame();
    
    // Update status bar with current layer info
    int currentFrame = xsheetWidget->getCurrentFrame();
    QString frameName = xsheetWidget->getFrameName(layer, currentFrame);
    if (!frameName.isEmpty()) {
        statusBar()->showMessage(QString("Current layer: %1, Frame: %2 (%3)")
                              .arg(layer)
                              .arg(currentFrame)
                              .arg(frameName), 2000);
    }
}

void MainWindow::onKeyframeAdded(const QString &layer, int frame)
{
    // When adding a keyframe, ALWAYS create a completely blank canvas for that layer only
    QSize canvasSize = canvas->getImage().size();
    
    // If the canvas size is valid, create a new blank image
    if (canvasSize.isValid() && canvasSize.width() > 0 && canvasSize.height() > 0) {
        // Create a new, completely blank/transparent image
        QImage newImage(canvasSize, QImage::Format_ARGB32);
        newImage.fill(Qt::transparent);
        
        // IMPORTANT: Set the new blank image as the frame image for this layer only
        // This ensures this layer's keyframe doesn't contain content from other layers
        xsheetWidget->setFrameImage(layer, frame, newImage);
        
        // Only update the canvas view if this is the current layer and frame
        if (layer == xsheetWidget->getCurrentLayer() && frame == xsheetWidget->getCurrentFrame()) {
            // Save current visibility settings for all layers
            QMap<QString, bool> layerVisibility;
            for (int i = 0; i < xsheetWidget->getLayerCount(); i++) {
                QString layerName = xsheetWidget->getLayerName(i);
                layerVisibility[layerName] = xsheetWidget->isLayerVisible(layerName);
            }
            
            // Temporarily hide all layers except this one
            for (int i = 0; i < xsheetWidget->getLayerCount(); i++) {
                QString layerName = xsheetWidget->getLayerName(i);
                if (layerName != layer) {
                    xsheetWidget->setLayerVisible(layerName, false);
                } else {
                    xsheetWidget->setLayerVisible(layerName, true);
                }
            }
            
            // Update canvas with only this layer visible, so it shows the blank keyframe
            updateCanvasFromCurrentFrame();
            
            // Now restore original visibility settings
            for (auto it = layerVisibility.begin(); it != layerVisibility.end(); ++it) {
                xsheetWidget->setLayerVisible(it.key(), it.value());
            }
            
            // Update the canvas again with all appropriate layers visible
            updateCanvasFromCurrentFrame();
            
            // Show status message
            statusBar()->showMessage(QString("Created new blank keyframe on Layer: %1, Frame: %2")
                                  .arg(layer)
                                  .arg(frame), 3000);
        }
    }
    
    // Set the frame name to follow the convention: layerName_frameNumber
    QString frameName = QString("%1_%2").arg(layer).arg(frame);
    xsheetWidget->setFrameName(layer, frame, frameName);
}

void MainWindow::onKeyframeRemoved(const QString &layer, int frame)
{
    if (layer == xsheetWidget->getCurrentLayer() && frame == xsheetWidget->getCurrentFrame()) {
        // When a keyframe is removed, clear the canvas
        canvas->clear();
        
        statusBar()->showMessage(QString("Keyframe removed from Layer: %1, Frame: %2")
                              .arg(layer)
                              .arg(frame), 3000);
    }
}

void MainWindow::updateCanvasFromCurrentFrame()
{
    QString currentLayer = xsheetWidget->getCurrentLayer();
    int currentFrame = xsheetWidget->getCurrentFrame();
    
    // Create a blank image to composite all layers onto
    QSize canvasSize = canvas->getImage().size();
    if (!canvasSize.isValid() || canvasSize.width() <= 0 || canvasSize.height() <= 0) {
        // Default size if we don't have a valid canvas size
        canvasSize = QSize(800, 600);
    }
    
    // Create a blank transparent image to start with
    QImage compositeImage(canvasSize, QImage::Format_ARGB32);
    compositeImage.fill(Qt::transparent);
    
    // Set up painter for compositing layers
    QPainter painter(&compositeImage);
    
    // Flag to check if we found any content
    bool foundContent = false;
    
    // Go through all layers from bottom to top (render in order)
    // Since layers are stored in the order they appear in the UI,
    // we need to iterate through them in forward order (first to last)
    for (int layerIndex = 0; layerIndex < xsheetWidget->getLayerCount(); layerIndex++) {
        QString layerName = xsheetWidget->getLayerName(layerIndex);
        
        // Skip hidden layers
        if (!xsheetWidget->isLayerVisible(layerName)) {
            continue;
        }
        
        // Find the appropriate frame to display
        int visibleFrame = xsheetWidget->findVisibleFrameAt(layerName, currentFrame);
        if (visibleFrame <= 0) {
            continue; // No visible frame for this layer
        }
        
        // Get the image for this frame
        QImage frameImage = xsheetWidget->getFrameImage(layerName, visibleFrame);
        
        if (!frameImage.isNull()) {
            // Draw this layer's frame onto the composite
            painter.drawImage(0, 0, frameImage);
            foundContent = true;
        }
    }
    
    painter.end();
    
    // Set the composite image to the canvas
    if (foundContent) {
        canvas->setImage(compositeImage);
        
        // Store the current frame image for animation
        currentFrameImage = compositeImage;
        
        // Update status bar
        statusBar()->showMessage(QString("Showing frame %1 (Layer: %2)")
                              .arg(currentFrame)
                              .arg(currentLayer), 2000);
    } else {
        // No content found, clear the canvas
        canvas->clear();
        currentFrameImage = QImage();
        
        statusBar()->showMessage(QString("No content at frame %1 (Layer: %2)")
                              .arg(currentFrame)
                              .arg(currentLayer), 2000);
    }
} 