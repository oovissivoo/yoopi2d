#include "mainwindow.h"

#include "canvasview.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>

namespace
{
constexpr int kDefaultBrushSize = 5;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_canvasView(new CanvasView(this))
    , m_brushSlider(new QSlider(Qt::Horizontal, this))
    , m_brushSizeLabel(new QLabel(this))
{
    createActions();
    createMenus();
    createToolbars();

    setCentralWidget(m_canvasView);
    setWindowTitle(tr("ProCreate Stabiliser Example"));

    m_brushSlider->setRange(1, 50);
    m_brushSlider->setSingleStep(1);
    m_brushSlider->setPageStep(5);
    m_brushSlider->setValue(kDefaultBrushSize);
    m_brushSlider->setFixedWidth(160);

    connect(m_brushSlider, &QSlider::valueChanged, this, &MainWindow::onBrushSizeChanged);
    connect(m_smoothSlider, &QSlider::valueChanged, this, &MainWindow::onSmoothChanged);
    connect(m_iterationsSlider, &QSlider::valueChanged, this, &MainWindow::onIterationsChanged);
    onBrushSizeChanged(kDefaultBrushSize);
    onSmoothChanged(0);
    onIterationsChanged(0);

    connect(m_canvasView, &CanvasView::undoAvailabilityChanged, this, &MainWindow::updateUndoAvailability);
    connect(m_canvasView, &CanvasView::redoAvailabilityChanged, this, &MainWindow::updateRedoAvailability);

    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet("QStatusBar { background-color: #2a2a2a; border-top: 1px solid #1e1e1e; }");
    statusBar()->showMessage(tr("Pen tool active"));

    updateUndoAvailability(m_canvasView->canUndo());
    updateRedoAvailability(m_canvasView->canRedo());
}

void MainWindow::createActions()
{
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newCanvas);

    m_undoAction = new QAction(tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, m_canvasView, &CanvasView::undo);

    m_redoAction = new QAction(tr("&Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, m_canvasView, &CanvasView::redo);

    QIcon penIcon = QIcon::fromTheme("draw-freehand");
    if (penIcon.isNull()) {
        penIcon = style()->standardIcon(QStyle::SP_FileDialogDetailedView);
        if (penIcon.isNull()) {
            penIcon = style()->standardIcon(QStyle::SP_FileIcon);
        }
    }

    m_penAction = new QAction(penIcon, tr("Pen"), this);
    m_penAction->setCheckable(true);
    m_penAction->setChecked(true);
    m_penAction->setEnabled(false);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
}

void MainWindow::createToolbars()
{
    QToolBar *headerBar = addToolBar(tr("Brush"));
    headerBar->setMovable(false);
    headerBar->setIconSize(QSize(18, 18));
    headerBar->addWidget(new QLabel(tr("Brush Size:"), headerBar));
    headerBar->addWidget(m_brushSlider);

    // Smooth slider
    headerBar->addSeparator();
    QLabel *smoothLabel = new QLabel(tr("Smooth:"), headerBar);
    headerBar->addWidget(smoothLabel);
    m_smoothSlider = new QSlider(Qt::Horizontal, headerBar);
    m_smoothSlider->setRange(0, 100); // 0.0 to 1.0 (scaled by 100)
    m_smoothSlider->setSingleStep(5);
    m_smoothSlider->setPageStep(10);
    m_smoothSlider->setValue(0);
    m_smoothSlider->setFixedWidth(120);
    headerBar->addWidget(m_smoothSlider);
    
    // Iterations slider
    headerBar->addSeparator();
    QLabel *iterationsLabel = new QLabel(tr("Iterations:"), headerBar);
    headerBar->addWidget(iterationsLabel);
    m_iterationsSlider = new QSlider(Qt::Horizontal, headerBar);
    m_iterationsSlider->setRange(0, 10);
    m_iterationsSlider->setSingleStep(1);
    m_iterationsSlider->setPageStep(1);
    m_iterationsSlider->setValue(0);
    m_iterationsSlider->setFixedWidth(100);
    headerBar->addWidget(m_iterationsSlider);

    headerBar->addSeparator();
    m_brushSizeLabel->setMinimumWidth(40);
    headerBar->addWidget(m_brushSizeLabel);

    QToolBar *toolBar = addToolBar(tr("Tools"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->addAction(m_penAction);
    toolBar->addAction(m_undoAction);
    toolBar->addAction(m_redoAction);
    toolBar->setStyleSheet("QToolBar { background-color: #2a2a2a; border-bottom: 1px solid #1e1e1e; }");

    menuBar()->setStyleSheet("QMenuBar { background-color: #2f2f2f; color: white; }"
                             "QMenuBar::item { background: transparent; }"
                             "QMenuBar::item:selected { background: #3a3a3a; }");

    headerBar->setStyleSheet("QToolBar { background-color: #2a2a2a; border-bottom: 1px solid #1e1e1e; }");
    m_brushSlider->setStyleSheet("QSlider::groove:horizontal { height: 6px; background: #444; }"
                                "QSlider::handle:horizontal { background: #5c90ff; width: 14px; margin: -5px 0; border-radius: 7px; }");
    m_brushSizeLabel->setStyleSheet("QLabel { color: white; }");
}

void MainWindow::onBrushSizeChanged(int value)
{
    m_canvasView->setBrushSize(value);
    updateBrushSizeLabel(value);
}

void MainWindow::onSmoothChanged(int value)
{
    m_canvasView->setSmoothFactor(value / 100.0f); // Convert to 0.0-1.0
}

void MainWindow::onIterationsChanged(int value)
{
    m_canvasView->setSmoothIterations(value);
}

void MainWindow::updateBrushSizeLabel(int value)
{
    m_brushSizeLabel->setText(QString::number(value) + tr(" px"));
}

void MainWindow::updateUndoAvailability(bool available)
{
    m_undoAction->setEnabled(available);
}

void MainWindow::updateRedoAvailability(bool available)
{
    m_redoAction->setEnabled(available);
}

void MainWindow::newCanvas()
{
    m_canvasView->clearCanvas();
    statusBar()->showMessage(tr("Canvas cleared"), 1500);
}
