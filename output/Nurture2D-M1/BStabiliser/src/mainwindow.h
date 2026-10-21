#pragma once

#include <QMainWindow>

class QAction;
class QLabel;
class QSlider;
class CanvasView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onBrushSizeChanged(int value);
    void onSmoothChanged(int value);
    void onIterationsChanged(int value);
    void updateUndoAvailability(bool available);
    void updateRedoAvailability(bool available);
    void newCanvas();

private:
    void createActions();
    void createMenus();
    void createToolbars();
    void updateBrushSizeLabel(int value);

    CanvasView *m_canvasView;
    QAction *m_newAction;
    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_penAction;
    QSlider *m_brushSlider;
    QLabel *m_brushSizeLabel;
    QSlider *m_smoothSlider;
    QLabel *m_smoothLabel;
    QSlider *m_iterationsSlider;
    QLabel *m_iterationsLabel;
};
