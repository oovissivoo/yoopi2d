/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2008-2009 Mj Mendoza IV
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef MAINWINDOW2_H
#define MAINWINDOW2_H

#include <QMainWindow>
#include <QMap>
#include <functional>

template<typename T> class QList;
class QActionGroup;
class QToolBar;
class QMenu;
class Object;
class Editor;
class ScribbleArea;
class BaseDockWidget;
class ColorPaletteWidget;
class ShiftTraceWidget;
class CellStackWidget;
class ToolOptionWidget;
class TimeLine;
class ToolBoxDockWidget;
class PreferencesDialog;
class PreviewWidget;
class ColorBox;
class ColorInspector;
class RecentFileMenu;
class ActionCommands;
class UndoRedoCommand;
class LayerOpacityDialog;
class CosmicLayerOpacityPopup;
class PegBarAlignmentDialog;
class AddTransparencyToPaperDialog;
class RepositionFramesDialog;
class ReferenceWindow;
class ShortcutFilter;
class CanvasGalleryOverlay;
class QShortcut;
class CosmicLeftToolPanel;
class CosmicRightToolPanel;
class CosmicBottomToolsBar;
class CosmicPopupWindow;
class CosmicMenuBar;
class CosmicMenuWindow;
class PenColorPalette;
class AutoLineColorToggle;
class CellBankFloatingPanel;
class CosmicCheckboxToggle;
enum class SETTING;


namespace Ui
{
    class MainWindow2;
}

class MainWindow2 : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow2(QWidget* parent = nullptr);
    ~MainWindow2() override;

    Editor* mEditor = nullptr;

public slots:
    void updateBackupActionState();
    void updateSaveState();
    void openPegAlignDialog();
    void openRepositionDialog();
    void closeRepositionDialog();
    void openLayerOpacityDialog();
    void openAddTranspToPaperDialog();
    void currentLayerChanged();
    void selectionChanged();
    void viewFlipped();
    void openCanvasSizeDialog();
    void openCanvasColorDialog();
    void openExtraSpaceDialog();

public:
    void newDocument();
    void openDocument();
    bool saveDocument();
    bool saveAsNewDocument();
    bool maybeSave(bool isQuitting = false);
    bool autoSave();
    void emptyDocumentWhenErrorOccurred();
    void lockToolbars(bool locked);

    // import
    void importLayers();
    void importSingleImage();
    void importImageSequenceNew();
    void importMovieVideo();
    void importAnimatedImage();

    void lockWidgets(bool shouldLock);

    void setOpacity(int opacity);
    void preferences();

    void openStartupFile(const QString& filename);
    void openFile(const QString& filename);

    void displayMessageBox(const QString& title, const QString& body);
    void displayMessageBoxNoTitle(const QString& body);

signals:
    /** Emitted when window regains focus */
    void windowActivated();

protected:
    void tabletEvent(QTabletEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void changeEvent(QEvent* e) override;
    bool event(QEvent*) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    QMenu* createPopupMenu() override;

private slots:
    void updateCopyCutPasteEnabled();
    void restoreDockState();
    void onColorPreferenceChanged(SETTING setting);
    void updateAllBlenderStyleSliders();
    void applyAccentColorToToolbars();
    void updateUIThemeColors();
    void updateLayerMenu();
    void updateMenuStyles();
    void toggleDocksVisibility();
    void onCanvasPaletteColorChanged(const QColor& color);
    void onDockVisibilityChanged(bool visible);
    void showPopupColorWheelAtCursor();
    void onPlayTriggered();
private:
    void newObject();
    bool newObjectFromPresets(int presetIndex);
    bool openObject(const QString& strFilename);
    bool saveObject(QString strFileName);
    void closeDialogs();

    void createDockWidgets();
    void createMenus();
    void replaceUndoRedoActions();
    void setupKeyboardShortcuts();
    void clearKeyboardShortcuts();
    bool loadMostRecent();
    bool tryLoadPreset();

    void openPalette();
    void importPalette();
    void exportPalette();

    void readSettings();
    void writeSettings();
    void resetAndDockAllSubWidgets();
    void ensureMenubarUnobstructed();

    void changePlayState(bool isPlaying);

    void makeConnections(Editor*);
    void makeConnections(Editor*, ColorBox* colorBox);
    void makeConnections(Editor*, ColorInspector*);
    void makeConnections(Editor*, ScribbleArea*);
    void makeConnections(Editor*, ColorPaletteWidget*);
    void makeConnections(Editor*, TimeLine*);
    void makeConnections(Editor*, ToolOptionWidget*);

    bool tryRecoverUnsavedProject();
    void startProjectRecovery(int result);
    void applyRoundedMenuMask(QMenu* menu);

    // UI: Dock widgets
    ColorBox*             mColorBox = nullptr;
    ColorPaletteWidget*   mColorPalette = nullptr;
    ToolOptionWidget*     mToolOptions = nullptr;
    ToolBoxDockWidget*        mToolBox = nullptr;
    ShiftTraceWidget*         mShiftTraceWidget = nullptr;
    RecentFileMenu*       mRecentFileMenu = nullptr;
    PreferencesDialog*    mPrefDialog = nullptr;
    //PreviewWidget*      mPreview = nullptr;
    TimeLine*             mTimeLine = nullptr;
    ColorInspector*       mColorInspector = nullptr;
    class BrushFactoryDock* mBrushFactoryDock = nullptr;
    QToolBar*             mMainToolbar = nullptr;
    QToolBar*             mViewToolbar = nullptr;
    QToolBar*             mOverlayToolbar = nullptr;
    QToolBar*             mPenPaletteToolbar = nullptr;
    QToolBar*             mAutoFillToolbar = nullptr;
    QToolBar*             mToolsToolbar = nullptr;
    QActionGroup*         mToolsActionGroup = nullptr;
    QAction*              mActionShowCanvasPalette = nullptr;
    QAction*              mActionCanvasSize = nullptr;
    QAction*              mActionCanvasColor = nullptr;
    QAction*              mActionCanvasExtraSpace = nullptr;
    QAction*              mActionPencil = nullptr;
    QAction*              mActionCurve = nullptr;
    QAction*              mActionBSpline = nullptr;
    QAction*              mActionRectGapFill = nullptr;
    QAction*              mActionLassoGapFill = nullptr;
    QAction*              mActionLassoSelect = nullptr;
    QAction*              mActionDplate = nullptr;
    ReferenceWindow*      mReferenceWindow = nullptr;

    PegBarAlignmentDialog* mPegAlign = nullptr;
    RepositionFramesDialog* mReposDialog = nullptr;
    CosmicLayerOpacityPopup* mLayerOpacityDialog = nullptr;
    AddTransparencyToPaperDialog* mAddTranspToPaper = nullptr;

    void createToolbars();
    void createCosmicMenuBar();
    void retranslateCosmicMenuBar();
    void populateCosmicControlCenter();
    CosmicMenuWindow* buildFileMenuWindow();
    CosmicMenuWindow* buildImportSubmenu();
    CosmicMenuWindow* buildExportSubmenu();
    CosmicMenuWindow* buildRecentSubmenu();
    CosmicMenuWindow* buildEditMenuWindow();
    CosmicMenuWindow* buildSelectionSubmenu();
    CosmicMenuWindow* buildPrepareScannedSubmenu();
    CosmicMenuWindow* buildViewMenuWindow();
    CosmicMenuWindow* buildZoomSubmenu();
    CosmicMenuWindow* buildOnionSkinSubmenu();
    CosmicMenuWindow* buildLayerVisibilitySubmenu();
    CosmicMenuWindow* buildOverlaysSubmenu();
    CosmicMenuWindow* buildCanvasMenuWindow();
    CosmicMenuWindow* buildWindowsMenuWindow();
private:
    ActionCommands* mCommands = nullptr;
    QList<QToolBar*> mToolbars;

    QIcon mStartIcon;
    QIcon mStopIcon;

    // a hack for MacOS because closeEvent fires twice
    bool m2ndCloseEvent = false;

    // Whether to suppress the auto save dialog due to internal work
    bool mSuppressAutoSaveDialog = false;

    Ui::MainWindow2* ui = nullptr;

    // Shortcut handling for tool tap/hold behavior
    ShortcutFilter* mShortcutFilter = nullptr;

    // Canvas palette overlay
    class CanvasPaletteOverlay* mCanvasPaletteOverlay = nullptr;
    void updateCanvasPalettePosition();
    void updateCosmicToolsBarsPosition();
    QWidget* mCosmicLeftToolsBar = nullptr;
    QWidget* mCosmicRightToolsBar = nullptr;
    CosmicBottomToolsBar* mCosmicBottomToolsBar = nullptr;
    CosmicMenuBar* mCosmicMenuBar = nullptr;
    class CosmicControlCenter* mCosmicControlCenter = nullptr;
    CosmicMenuWindow* mCosmicEditMenu = nullptr;  // kept to refresh undo/redo on stack switch

    // Canvas timecode overlay (top-left pill on canvas)
    class CanvasTimecodeOverlay* mCanvasTimecodeOverlay = nullptr;

    // Canvas keyframe scrubber (top-center pill on canvas)
    class CanvasKeyframeScrubber* mCanvasKeyframeScrubber = nullptr;

    // Label factories for CC toggle actions — refreshed on language change
    QMap<QAction*, std::function<QString()>> mCCActionLabelFactories;

    // Cell Bank floating panel + toggle button (bottom-right of canvas)
    CellBankFloatingPanel* mCellBankPanel = nullptr;
    CosmicCheckboxToggle* mCellBankPanelToggle = nullptr;
    void updateCellBankPanelPosition();

    // Toolbar-hosted widgets (kept as members so shortcut bindings can trigger them)
    PenColorPalette* mPenPaletteToolbarWidget = nullptr;
    AutoLineColorToggle* mAutoFillToolbarWidget = nullptr;
    // Gallery overlay in canvas area
    CanvasGalleryOverlay* mCanvasGalleryOverlay = nullptr;

    // Shift & Trace floating window (CosmicPopupWindow)
    CosmicPopupWindow* mShiftTracePopupWindow = nullptr;
    QWidget* mShiftTracePopupContainer = nullptr;
    QWidget* mShiftTraceDockContent = nullptr;
    QWidget* mShiftTraceDockContentOriginalParent = nullptr;
    void ensureShiftTracePopupInitialized();
    void updateShiftTracePopupFromPreference();
    QPoint canvasCenterGlobal() const;
    // Applies or removes the lasso selection restriction mode that disables
    // timeline interactions, timeline shortcuts, frame/layer switching/manipulation,
    // and all import/export actions while a lasso selection exists.
    void applyLassoRestriction(bool restricted);
    // Returns true if there is an active floating selection (temporary buffer)
    // that either exists or still needs to be committed.
    bool hasFloatingSelection() const;
    // Tracks if restriction is currently active to avoid spamming popup
    bool mLassoRestrictionActive = false;
    // Re-entrancy guard while applying restriction to avoid recursive updates
    bool mApplyingLassoRestriction = false;
    // Show hint only once per restriction activation
    bool mRestrictionHintShown = false;
    void maybeShowRestrictionHint();

    // Brush Factory lock: when active, restrict interaction to the
    // BrushFactoryDock and its toggle so brush editing behaves
    // like a focused brush engine panel.
    bool mBrushFactoryLockActive = false;
    // Dedicated popup color wheel shortcut (independent of QAction system)
    QShortcut* mPopupColorWheelShortcut = nullptr;

    QAction* mToggleGapCheckAction = nullptr;
    QAction* mExportVideoAction = nullptr;
    QAction* mIncreaseSizeAction = nullptr;
    QAction* mDecreaseSizeAction = nullptr;
    QAction* mIncreaseOpacityAction = nullptr;
    QAction* mDecreaseOpacityAction = nullptr;
    QAction* mIncreaseSmoothAction = nullptr;
    QAction* mDecreaseSmoothAction = nullptr;
    // Toolbar visibility toggles (Toolbars menu / CosmicControlCenter groups)
    QAction* mCCToggleFileOpsAction    = nullptr;
    QAction* mCCToggleViewAction       = nullptr;
    QAction* mCCToggleOverlaysAction   = nullptr;
    QAction* mCCToggleUndoRedoAction   = nullptr;
    QAction* mCCToggleClearFrameAction = nullptr;

    bool mToolbarsLocked = false;
};

#endif // MAINWINDOW2_H
