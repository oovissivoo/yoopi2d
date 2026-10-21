#include "mainwindow.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Vector Drawing Application");
    setMinimumSize(800, 600);

    createActions();
    createToolbar();
    setupCentralWidget();
    createOptionsDock();
    createColorDock();

    // Set initial tool
    brushAction->setChecked(true);
    drawingCanvas->setCurrentTool(DrawingCanvas::BrushTool);

    // Initialize checkbox states
    drawBehindCheckBox->setEnabled(true);
    segmentEraserCheckBox->setEnabled(false); // Keep disabled until logic is implemented

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow()
{
    // Cleanup is handled by Qt parent-child system
}

void MainWindow::createActions()
{
    // Brush tool action
    brushAction = new QAction(QIcon(":/icons/brush.svg"), "Brush", this);
    brushAction->setCheckable(true);
    brushAction->setToolTip("Select brush tool");
    connect(brushAction, &QAction::triggered, this, &MainWindow::onBrushToolSelected);

    // Eraser tool action
    eraserAction = new QAction(QIcon(":/icons/eraser.svg"), "Eraser", this);
    eraserAction->setCheckable(true);
    eraserAction->setToolTip("Select eraser tool");
    connect(eraserAction, &QAction::triggered, this, &MainWindow::onEraserToolSelected);

    // Create action group for tool selection (only one tool can be selected at a time)
    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->addAction(brushAction);
    toolGroup->addAction(eraserAction);
}

void MainWindow::createToolbar()
{
    toolBar = addToolBar("Tools");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setIconSize(QSize(24, 24));

    toolBar->addAction(brushAction);
    toolBar->addAction(eraserAction);
}

void MainWindow::createOptionsDock()
{
    optionsDock = new QDockWidget("Options", this);
    optionsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    optionsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    QWidget *optionsWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(optionsWidget);

    // Brush Size Section
    QGroupBox *brushGroup = new QGroupBox("Brush Settings");
    QVBoxLayout *brushLayout = new QVBoxLayout(brushGroup);

    // Brush size slider
    QLabel *sizeLabel = new QLabel("Brush Size:");
    brushSizeSlider = new QSlider(Qt::Horizontal);
    brushSizeSlider->setRange(1, 50);
    brushSizeSlider->setValue(5);
    brushSizeSlider->setTickPosition(QSlider::TicksBelow);
    brushSizeSlider->setTickInterval(5);

    brushSizeLabel = new QLabel("5");
    brushSizeLabel->setAlignment(Qt::AlignCenter);
    brushSizeLabel->setMinimumWidth(30);

    QHBoxLayout *sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(brushSizeSlider);
    sizeLayout->addWidget(brushSizeLabel);

    brushLayout->addLayout(sizeLayout);

    // Draw behind checkbox
    drawBehindCheckBox = new QCheckBox("Draw Behind");
    drawBehindCheckBox->setToolTip("Draw new strokes behind existing strokes");
    brushLayout->addWidget(drawBehindCheckBox);

    brushGroup->setLayout(brushLayout);

    // Eraser Settings Section
    QGroupBox *eraserGroup = new QGroupBox("Eraser Settings");
    QVBoxLayout *eraserLayout = new QVBoxLayout(eraserGroup);

    // Segment eraser checkbox
    segmentEraserCheckBox = new QCheckBox("Segment Eraser");
    segmentEraserCheckBox->setToolTip("Enable segment-based erasing (not yet implemented)");
    segmentEraserCheckBox->setEnabled(false); // Disabled until logic is implemented
    eraserLayout->addWidget(segmentEraserCheckBox);

    eraserGroup->setLayout(eraserLayout);

    // Performance Monitoring Section
    QGroupBox *performanceGroup = new QGroupBox("Performance");
    QVBoxLayout *performanceLayout = new QVBoxLayout(performanceGroup);

    performanceLabel = new QLabel("Frame Time: -- ms\nStrokes: 0");
    performanceLabel->setStyleSheet("font-family: monospace;");

    performanceLayout->addWidget(performanceLabel);
    performanceGroup->setLayout(performanceLayout);

    layout->addWidget(brushGroup);
    layout->addWidget(eraserGroup);
    layout->addWidget(performanceGroup);
    layout->addStretch();

    // Set up performance monitoring timer
    performanceTimer = new QTimer(this);
    connect(performanceTimer, &QTimer::timeout, this, [this]() {
        int strokeCount = drawingCanvas->getStrokeCount();
        qreal avgFrameTime = drawingCanvas->getAverageFrameTime();
        performanceLabel->setText(QString("Frame Time: %1 ms\nStrokes: %2")
                                .arg(avgFrameTime, 0, 'f', 2)
                                .arg(strokeCount));
    });
    performanceTimer->start(1000); // Update every second

    optionsWidget->setLayout(layout);
    optionsDock->setWidget(optionsWidget);

    addDockWidget(Qt::RightDockWidgetArea, optionsDock);

    // Connect signals
    connect(brushSizeSlider, &QSlider::valueChanged, this, &MainWindow::onBrushSizeChanged);
    connect(brushSizeSlider, &QSlider::valueChanged, this, &MainWindow::updateBrushSizeLabel);
    connect(drawBehindCheckBox, &QCheckBox::toggled, this, &MainWindow::onDrawBehindToggled);
    connect(segmentEraserCheckBox, &QCheckBox::toggled, this, &MainWindow::onSegmentEraserToggled);
}

void MainWindow::createColorDock()
{
    colorDock = new QDockWidget("Color", this);
    colorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    colorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    colorWheel = new ColorWheel();
    colorDock->setWidget(colorWheel);

    addDockWidget(Qt::LeftDockWidgetArea, colorDock);

    // Connect color wheel signal
    connect(colorWheel, &ColorWheel::colorChanged, this, &MainWindow::onColorChanged);
}

void MainWindow::setupCentralWidget()
{
    drawingCanvas = new DrawingCanvas(this);

    // Create a scroll area to handle the large canvas
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(drawingCanvas);
    scrollArea->setWidgetResizable(false); // Keep canvas at fixed size
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Set canvas as central widget via scroll area
    setCentralWidget(scrollArea);
}

void MainWindow::onBrushToolSelected()
{
    drawingCanvas->setCurrentTool(DrawingCanvas::BrushTool);
    drawBehindCheckBox->setEnabled(true);
    // Hide/disable segment checkbox in brush mode
    segmentEraserCheckBox->setEnabled(false);
    statusBar()->showMessage("Brush tool selected");
}

void MainWindow::onEraserToolSelected()
{
    drawingCanvas->setCurrentTool(DrawingCanvas::EraserTool);
    drawBehindCheckBox->setEnabled(false);
    drawBehindCheckBox->setChecked(false); // Uncheck when switching to eraser
    drawingCanvas->setDrawBehindMode(false); // Disable draw behind mode
    // Segment eraser checkbox is available in eraser mode
    segmentEraserCheckBox->setEnabled(true);
    statusBar()->showMessage("Eraser tool selected");
}

void MainWindow::onBrushSizeChanged(int size)
{
    drawingCanvas->setBrushSize(size);
    statusBar()->showMessage(QString("Brush size changed to %1").arg(size));
}

void MainWindow::onColorChanged(const QColor &color)
{
    drawingCanvas->setBrushColor(color);
    statusBar()->showMessage(QString("Color changed to RGB(%1, %2, %3)")
                            .arg(color.red()).arg(color.green()).arg(color.blue()));
}

void MainWindow::updateBrushSizeLabel(int size)
{
    brushSizeLabel->setText(QString::number(size));
}

void MainWindow::onDrawBehindToggled(bool checked)
{
    drawingCanvas->setDrawBehindMode(checked);
    statusBar()->showMessage(checked ? "Draw behind mode enabled" : "Draw behind mode disabled");
}

void MainWindow::onSegmentEraserToggled(bool checked)
{
    drawingCanvas->setSegmentEraserMode(checked);
    statusBar()->showMessage(checked ? "Segment eraser enabled" : "Segment eraser disabled");
}
