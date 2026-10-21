#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QDockWidget>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QTimer>
#include <QCheckBox>
#include "drawingcanvas.h"
#include "colorwheel.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBrushToolSelected();
    void onEraserToolSelected();
    void onBrushSizeChanged(int size);
    void onColorChanged(const QColor &color);
    void onDrawBehindToggled(bool checked);
    void onSegmentEraserToggled(bool checked);

private:
    void createActions();
    void createToolbar();
    void createOptionsDock();
    void createColorDock();
    void setupCentralWidget();

    // UI Components
    QToolBar *toolBar;
    QDockWidget *optionsDock;
    QDockWidget *colorDock;
    DrawingCanvas *drawingCanvas;
    ColorWheel *colorWheel;

    // Actions
    QAction *brushAction;
    QAction *eraserAction;

    // Options widgets
    QSlider *brushSizeSlider;
    QLabel *brushSizeLabel;
    QCheckBox *drawBehindCheckBox;
    QCheckBox *segmentEraserCheckBox;

    // Performance monitoring
    QLabel *performanceLabel;
    QTimer *performanceTimer;

    void updateBrushSizeLabel(int size);
};

#endif // MAINWINDOW_H
