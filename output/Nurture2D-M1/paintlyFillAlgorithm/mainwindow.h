#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QColorDialog>
#include <QSpinBox>
#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QDockWidget>
#include <QMenu>
#include <QCheckBox>
#include <QPushButton>
#include "colorwheel.h"
#include "canvas.h"
#include "popupcolorpicker.h"
#include "blenderstyleslider.h"
#include <QColor>
#include <QVector>
#include <QSettings>
#include "brushsettingsdialog.h"
#include "xsheetwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void setColor(const QColor &color);
    void setBrushSize(int size);
    void setOpacity(int opacity);
    void toggleFillMode();
    void toggleContiguousSelection();
    void toggleBrushTool();
    void openColorPicker(int index);
    void toggleLineColorSelection(int index, bool checked);
    void updateAutoLineFillState();
    void startColorSampling(int index);
    void sampleColorFromCanvas(const QPointF &pos);
    void previewSampleColorFromCanvas(const QPointF &pos);
    
    // Undo/Redo slots
    void undo();
    void redo();
    void updateUndoRedoActions(bool available);
    
    // Preferences
    void showPreferencesDialog();
    
    // Brush Settings
    void showBrushSettings();
    void toggleColorReplace(bool enabled);
    
    // Custom cursor
    void toggleCustomCursor(bool enabled);

    // New: gap fill tool toggles
    void toggleRectGapFill(bool enabled);
    void toggleLassoGapFill(bool enabled);
    void toggleLassoUseActiveColor(bool enabled);

    void onCurrentFrameChanged(int frame);
    void onCurrentLayerChanged(const QString &layer);
    void onKeyframeAdded(const QString &layer, int frame);
    void onKeyframeRemoved(const QString &layer, int frame);
    void onFrameUpdated(const QImage &frame);
    void updateCanvasFromCurrentFrame();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void createBrushDockWindow();
    void setupColorWheel();
    void setupCanvas();
    void setupColorPalette();
    void writeSettings();
    void readSettings();
    void updateCurrentColorDisplay(const QColor &color);
    void loadShortcuts();
    bool maybeSave();
    void loadFile(const QString &fileName);
    bool saveFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    QString strippedName(const QString &fullFileName);
    QList<QAction*> getAllActions();

    Canvas *canvas;
    QGraphicsScene *scene;
    QGraphicsView *canvasView;
    
    // Dock Widgets
    QDockWidget *colorWheelDock;
    QDockWidget *colorPaletteDock;
    QDockWidget *xsheetDock;
    XsheetWidget *xsheetWidget;
    
    // Color Tools
    ColorWheel *colorWheel;
    QLabel *currentColorLabel;
    BlenderStyleSlider *hueSlider;
    BlenderStyleSlider *opacitySlider;
    BlenderStyleSlider *brushSizeSlider;
    QPushButton *brushSettingsButton;
    
    // Color Palette
    static const int PALETTE_SIZE = 5;
    QList<QLabel*> paletteColorBoxes;
    QList<QCheckBox*> paletteColorCheckBoxes;
    QList<QPushButton*> paletteSamplerButtons;
    QList<QColor> paletteColors;
    int lineColorIndex;  // Index of color selected for AutoLine fill
    QList<int> lineColorIndices;  // List of all selected line color indices
    
    // Color Sampling
    bool colorSamplingMode;
    int samplingColorIndex;
    QColor originalSamplingColor;  // Store original color for cancellation
    
    // Menus
    QMenu *viewMenu;
    QMenu *editMenu;
    
    // Toolbars
    QToolBar *toolsToolBar;
    QToolBar *toolSettingsToolBar;
    
    // Actions
    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *fillAct;
    QAction *contiguousSelectionAct;
    QAction *brushAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *preferencesAct;
    QAction *brushSettingsAct;
    QAction *colorReplaceAct;
    QAction *customCursorAct; // New action for toggling custom cursor
    QAction *rectGapFillAct;  // New
    QAction *lassoGapFillAct; // New
    QAction *lassoUseActiveColorAct; // New mode toggle
    
    // State
    QColor currentColor;
    int currentBrushSize;
    int currentOpacity;
    bool fillMode;
    bool contiguousSelectionMode;
    bool brushToolMode;
    bool autoLineFillMode;
    bool colorReplaceMode;
    bool rectGapFillMode;   // New
    bool lassoGapFillMode;  // New
    bool lassoUseActiveColor; // New
    
    // Function to update tool states
    void setActiveTool(QAction *action);

    // Animation state
    QImage currentFrameImage;
    bool isAnimating;
};

#endif // MAINWINDOW_H 