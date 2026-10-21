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

#include "mainwindow2.h"
#include "ui_mainwindow2.h"
#include "canvasgalleryoverlay.h"
#include "referencewindow.h"
#include "brushfactorydock.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmiccontrolcenter.h"
#include "n2duimetrics.h"
#include "pencilsettings.h"

// Qt headers
#include <QtGui>
#include <QTimer>
#include <QScreen>
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTabletEvent>
#include "cosmicsaveprogresspopup.h"
#include <QTabWidget>
#include <QStandardPaths>
#include <QDateTime>
#include <QLabel>
#include <QClipboard>
#include <QToolBar>
#include <QToolButton>
#include <QDebug>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QSplitter>
#include <QWidgetAction>
#include <QIcon>
#include <QTimer>
#include <QShortcut>
#include <QShortcutEvent>
#include <QKeyEvent>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QBitmap>
#include <QRubberBand>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <algorithm>

#ifdef Q_OS_MACOS
namespace {
// macOS virtual key code for the physical ANSI Z key. Unlike QKeyEvent::key(),
// this remains stable when the active input source changes to Cyrillic or another layout.
constexpr quint32 kMacVirtualKeyZ = 6;
constexpr quint32 kMacVirtualKeyComma = 43;
constexpr quint32 kMacVirtualKeyPeriod = 47;

bool isMacClipboardShortcut(const QKeyEvent* event, bool* paste)
{
    const auto modifiers = event->modifiers();
    if (!modifiers.testFlag(Qt::ControlModifier) ||
        (modifiers & (Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier))) return false;
    if (event->nativeVirtualKey() != 8 && event->nativeVirtualKey() != 9) return false;
    *paste = event->nativeVirtualKey() == 9;
    const QString configured = pencilSettings().value(QStringLiteral("shortcuts/")
        + (*paste ? QStringLiteral("CmdPaste") : QStringLiteral("CmdCopy"))).toString()
        .remove('"').trimmed();
    return QKeySequence(configured) == QKeySequence(*paste ? "Ctrl+V" : "Ctrl+C");
}

bool isMacCommandZ(const QKeyEvent* event)
{
    if (!event || event->nativeVirtualKey() != kMacVirtualKeyZ) return false;
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    return (modifiers & Qt::ControlModifier) &&
           !(modifiers & (Qt::AltModifier | Qt::MetaModifier));
}

bool isMacKeyframeNavigationShortcut(const QKeyEvent* event, bool* goNext)
{
    if (!event || !goNext) return false;

    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (modifiers.testFlag(Qt::ControlModifier) ||
        modifiers.testFlag(Qt::MetaModifier)) {
        return false;
    }

    // Use macOS virtual key codes for the physical comma and period keys so
    // Qt's translated character does not change the shortcut with the layout.
    const quint32 virtualKey = event->nativeVirtualKey();
    const bool next = virtualKey == kMacVirtualKeyPeriod;
    const bool previous = virtualKey == kMacVirtualKeyComma;
    if (!next && !previous) return false;

    // The ordinary comma/period bindings used to step through every timeline
    // position. Both those bindings and the Option aliases now navigate drawings.
    // Shift is allowed for the printed < and > symbols on the same physical keys.
    const bool option = modifiers.testFlag(Qt::AltModifier);
    const QString command = option
        ? (next ? QStringLiteral("CmdGotoNextKeyFrame") : QStringLiteral("CmdGotoPreviousKeyFrame"))
        : (next ? QStringLiteral("CmdGotoNextFrame") : QStringLiteral("CmdGotoPreviousFrame"));
    const QString expected = (option ? QStringLiteral("Alt+") : QString())
        + (next ? QStringLiteral(".") : QStringLiteral(","));
    const QString configured = pencilSettings()
        .value(QStringLiteral("shortcuts/%1").arg(command)).toString()
        .remove(QLatin1Char('"')).trimmed();
    if (configured.compare(expected, Qt::CaseInsensitive) != 0) return false;

    *goNext = next;
    return true;
}
}
#endif

// core_lib headers
#include "pencildef.h"
#include "canvaspainter.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "structure/celllayerstack.h"

#include "cosmicwindowheader.h"
#include "titlebarwidget.h"

#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmictext.h"
#include "colormanager.h"
#include "importimageconfig.h"

#include "n2dprojectio.h"
#include "scenesequencemanager.h"
#include "toolmanager.h"
#include "playbackmanager.h"
#include "selectionmanager.h"
#include "soundmanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "layermanager.h"
#include "undoredomanager.h"
#include "managers/settingsmanager.h"

#include "actioncommands.h"
#include "../core_lib/src/util/fileformat.h"
#include "util.h"
#include "undoredocommand.h"
#include "popupcolorwheelwidget.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include <QWidgetAction>
#include "recentfilemenu.h"

// app headers
#include "colorbox.h"
// #include "colorslidersdock.h" // removed: replaced by new cosmic color wheel
#include "colorinspector.h"
#include "colorpalettewidget.h"
#include "tooloptionwidget.h"
#include "preferencesdialog.h"
#include "timeline.h"
#include "toolbox.h"
// #include "onionskinwidget.h" // removed: replaced by Shift & Trace
#include "scribblearea.h"
#include "pegbaralignmentdialog.h"
#include "addtransparencytopaperdialog.h"
#include "repositionframesdialog.h"

#include "errordialog.h"
#include "cosmicwarningpopup.h"
#include "cosmicprogresspopup.h"
#include "cosmicconfirmpopup.h"
#include "cosmicautosavepopup.h"
#include "filedialog.h"
#include "importimageseqdialog.h"
#include "importlayersdialog.h"
#include "importpositiondialog.h"
#include "layeropacitydialog.h"
// Cosmic redesigned dialogs
#include "cosmicimportlayerspopup.h"
#include "cosmiclayeropacitypopup.h"
#include "cosmiccamerapropertiespopup.h"
#include "app_util.h"
#include "presetdialog.h"
#include "pegbaralignmentdialog.h"
#include "camerapropertiesdialog.h"
#include "cosmiccanvassizepopup.h"
#include "cosmicextraspacepopup.h"
#include "layercamera.h"
#include "shifttracewidget.h"
#include "cosmicpopupwindow.h"
#include "shortcutfilter.h"
#include "referencewindow.h"
#include "inlinecolorinspector.h"
#include "canvaspaletteoverlay.h"
#include "canvastimecodeoverlay.h"
#include "canvaskeyframescrubber.h"
#include "canvasgalleryoverlay.h"
#include "ignorefillsplittoggle.h"
#include "brushfactorydock.h"
#include "pencolorpalette.h"
#include "autolinecolortoggle.h"
#include "cosmiclefttoolpanel.h"
#include "cosmicrighttoolpanel.h"
#include "cosmictoolspane.h"
#include "cosmiccolorwheelwidget.h"
#include "cosmiccolorsliders.h"
#include "cellbankfloatingpanel.h"
#include "cosmiclayerspane.h"
#include "cosmicconfirmpopup.h"
#include "brushpresetlistwidget.h"
#include "cosmicbrushsettings.h"
#include "pentool.h"
#include "penciltool.h"
#include "managers/brushpresetstore.h"
#include "cosmicrighttoolsbar.h"
#include "cosmicbottomtoolsbar.h"
#include "../../CUI/cosmicmenubar.h"
#include "../../CUI/cosmicmenuwindow.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QStyle>
#include "../CUI/blenderstyledropdown.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/blenderstylecombobox.h"
#include "../CUI/blenderstyletoggle.h"
#include "../CUI/blenderstylemodalpopup.h"
#include "cellstackwidget.h"

#include "managers/shifttracemanager.h"
#include "stroketool.h"

#ifdef GIT_TIMESTAMP
#define BUILD_DATE S__GIT_TIMESTAMP
#else
#define BUILD_DATE __DATE__
#endif

#define PENCIL_WINDOW_TITLE QString("[*]Nurture2D")

#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "cosmicpopupcontextmenu.h"

// Helper struct for image sequence parsing (file scope)
struct ImageSequenceFileInfo {
    QString filePath;
    QString baseName;   // name without separator+digits
    QChar separator;    // '.', '-', '_'
    int frameNumber = -1; // extracted frame number (already clamped to >= 1)
    int digitCount = 0;   // number of digits in the padding
    bool valid() const { return !baseName.isEmpty() && frameNumber >= 1 && digitCount >= 1; }
};

// Parse padding patterns: name_NNN.ext, name-NNN.ext, name.NNN.ext
// Any digit count >= 1 is accepted. Frame 0 maps to frame 1.
static ImageSequenceFileInfo parseImageSequencePadding(const QString& filePath)
{
    ImageSequenceFileInfo info;
    info.filePath = filePath;
    const QFileInfo fi(filePath);
    const QString base = fi.completeBaseName();
    if (base.isEmpty()) return info;

    // Find the last separator (_  -  .) followed by a run of digits that ends the basename
    static const QChar seps[] = { QChar('_'), QChar('-'), QChar('.') };
    int bestSepIdx = -1;
    int bestDigitCount = 0;
    for (const QChar sep : seps) {
        int idx = base.lastIndexOf(sep);
        if (idx <= 0 || idx >= base.size() - 1) continue;
        int count = 0;
        for (int i = idx + 1; i < base.size(); ++i) {
            if (base.at(i).isDigit()) ++count; else { count = 0; break; }
        }
        if (count > 0 && idx > bestSepIdx) {
            bestSepIdx = idx;
            bestDigitCount = count;
        }
    }
    if (bestSepIdx < 0) return info;

    const QString digits = base.mid(bestSepIdx + 1, bestDigitCount);
    bool ok = false;
    int frameNum = digits.toInt(&ok);
    if (!ok) return info;

    info.baseName  = base.left(bestSepIdx);
    info.separator = base.at(bestSepIdx);
    info.digitCount = bestDigitCount;
    // Frame 0 in the filename maps to frame 1 on the timeline
    info.frameNumber = qMax(1, frameNum);
    return info;
}

QPoint MainWindow2::canvasCenterGlobal() const
{
    if (ui && ui->scribbleArea) {
        return ui->scribbleArea->mapToGlobal(ui->scribbleArea->rect().center());
    }
    return mapToGlobal(rect().center());
}

void MainWindow2::ensureShiftTracePopupInitialized()
{
    if (mShiftTracePopupWindow) {
        return;
    }

    mShiftTracePopupWindow = new CosmicPopupWindow(this);
    mShiftTracePopupWindow->setModalMode(CosmicPopupWindow::ModalMode::NonModal);
    mShiftTracePopupWindow->setClosePolicy(CosmicPopupWindow::ClosePolicy::Manual);
    mShiftTracePopupWindow->setResizable(false);
    mShiftTracePopupWindow->setDraggable(true);
    mShiftTracePopupWindow->setFixedWindowSize(QSize(720, 320));
    mShiftTracePopupWindow->setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    mShiftTracePopupWindow->setBorderColor(CosmicColor::popupBorder());
    mShiftTracePopupWindow->setBorderWidth(2);

    mShiftTracePopupContainer = new QWidget(mShiftTracePopupWindow);
    mShiftTracePopupContainer->setObjectName(QStringLiteral("ShiftTracePopupContainer"));
    mShiftTracePopupContainer->setStyleSheet("QWidget#ShiftTracePopupContainer { background: transparent; }");
    mShiftTracePopupContainer->setAutoFillBackground(false);
    mShiftTracePopupContainer->setAttribute(Qt::WA_NoSystemBackground, true);
    auto* outer = new QVBoxLayout(mShiftTracePopupContainer);
    outer->setContentsMargins(16, 10, 16, 16);
    outer->setSpacing(10);

    auto* windowHeader = new CosmicWindowHeader(mShiftTracePopupContainer);
    outer->addWidget(windowHeader);

    QWidget* body = new QWidget(mShiftTracePopupContainer);
    body->setObjectName(QStringLiteral("ShiftTracePopupBody"));
    body->setAutoFillBackground(false);
    body->setAttribute(Qt::WA_NoSystemBackground, true);
    body->setStyleSheet("QWidget#ShiftTracePopupBody { background: transparent; border: none; }");
    auto* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    outer->addWidget(body, 1);

    auto* popupLay = new QVBoxLayout(mShiftTracePopupWindow);
    popupLay->setContentsMargins(0, 0, 0, 0);
    popupLay->setSpacing(0);
    popupLay->addWidget(mShiftTracePopupContainer);

    connect(windowHeader, &CosmicWindowHeader::closeRequested, this, [this]() {
        if (mEditor && mEditor->preference()) {
            mEditor->preference()->set(SETTING::SHIFT_TRACE_WINDOW_VISIBLE, false);
        }
    });

    connect(mShiftTracePopupWindow, &QDialog::finished, this, [this](int) {
        if (mEditor && mEditor->preference()) {
            mEditor->preference()->set(SETTING::SHIFT_TRACE_WINDOW_VISIBLE, false);
        }
    });
}

void MainWindow2::updateShiftTracePopupFromPreference()
{
    if (!mEditor || !mEditor->preference()) {
        return;
    }

    const bool wantVisible = mEditor->preference()->isOn(SETTING::SHIFT_TRACE_WINDOW_VISIBLE);

    // Popup visibility must NOT disable Shift & Trace rendering.
    // Animators often close the window for screen space but expect ghosts to remain.
    // We only clear active move mode when the window is hidden to avoid trapping the
    // user in an edit state with no UI.
    if (!wantVisible && mEditor->shiftTrace()) {
        mEditor->shiftTrace()->setActiveMoveEntryId(-1);
        mEditor->updateFrame();
    }

    if (wantVisible) {
        ensureShiftTracePopupInitialized();

        if (mShiftTraceWidget) {
            if (!mShiftTraceDockContent) {
                mShiftTraceDockContent = mShiftTraceWidget->dockContentWidget();
                if (mShiftTraceDockContent) {
                    mShiftTraceDockContentOriginalParent = mShiftTraceDockContent->parentWidget();
                }
            }

            mShiftTraceWidget->setEmbeddedInPopup(true);

            if (mShiftTraceDockContent && mShiftTracePopupContainer) {
                QWidget* body = mShiftTracePopupContainer->findChild<QWidget*>(QStringLiteral("ShiftTracePopupBody"));
                if (body && body->layout() && mShiftTraceDockContent->parentWidget() != body) {
                    mShiftTraceDockContent->setParent(body);
                    body->layout()->addWidget(mShiftTraceDockContent);
                }
            }

            // Migration: legacy dock stays hidden; popup is the only surface.
            mShiftTraceWidget->hide();
        }

        if (mShiftTracePopupWindow) {
            const QPoint center = canvasCenterGlobal();
            mShiftTracePopupWindow->showAt(center, CosmicPopupWindow::Anchor::Center, true);
        }
    } else {
        // Hide popup first.
        if (mShiftTracePopupWindow) {
            mShiftTracePopupWindow->hide();
        }

        // Migration: keep legacy dock hidden and keep content hosted in the popup container.
        if (mShiftTraceWidget) {
            mShiftTraceWidget->hide();
        }
    }
}

MainWindow2::MainWindow2(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow2)
{
    ui->setupUi(this);

    // Ensure the menubar is always above other widgets and offset from any window controls area
    if (ui->menuBar) {
        ui->menuBar->raise();
        ui->menuBar->setContentsMargins(60, 0, 0, 0); // add left padding to avoid overlap with window controls
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
        ui->menuBar->setNativeMenuBar(false);
#endif
    }

    ensureMenubarUnobstructed();

    // Apply dark theme stylesheet scoped to main window and common Qt widgets
    QFile darkQss(":/styles/dark_theme.qss");
    if (darkQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(darkQss.readAll());
        // Apply to this window only to reduce impact on custom widgets
        qWarning() << "[QSS] MainWindow2 applying base dark_theme.qss";
        this->setStyleSheet(qss);

        // Apply a matching dark palette so custom-painted widgets (using palette()) render correctly
        QPalette pal = qApp->palette();
        pal.setColor(QPalette::Window, QColor("#262626"));      // panels / window bg
        pal.setColor(QPalette::Base, QColor("#353535"));        // text inputs / lists (will be updated from prefs)
        pal.setColor(QPalette::AlternateBase, QColor("#353535"));
        pal.setColor(QPalette::Button, QColor("#353535"));
        pal.setColor(QPalette::Text, QColor("#b2b2b2"));
        pal.setColor(QPalette::WindowText, QColor("#b2b2b2"));
        pal.setColor(QPalette::ButtonText, QColor("#b2b2b2"));
        pal.setColor(QPalette::Mid, QColor("#3a3a3a"));
        pal.setColor(QPalette::Dark, QColor("#1f1f1f"));
        pal.setColor(QPalette::Light, QColor("#4a4a4a"));
        pal.setColor(QPalette::Highlight, QColor("#505050"));
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        qApp->setPalette(pal);
    }

    // Initialize order
    // 1. editor 2. object 3. scribble area 4. other widgets
    mEditor = new Editor(this);
    mEditor->setScribbleArea(ui->scribbleArea);
    mEditor->init();

    newObject();

    ui->scribbleArea->setEditor(mEditor);
    ui->scribbleArea->init();

    // Status bar removed — hide the Qt-required QStatusBar widget
    if (statusBar()) statusBar()->hide();

    mCommands = new ActionCommands(this);
    mCommands->setCore(mEditor);

    createDockWidgets();
    createMenus();
    createToolbars();
    applyAccentColorToToolbars();  // Apply accent color after toolbars are created
    setupKeyboardShortcuts();
    createCosmicMenuBar();
    populateCosmicControlCenter();

    readSettings();
    ensureMenubarUnobstructed();

    selectionChanged();
    currentLayerChanged();

    connect(mEditor, &Editor::needSave, this, &MainWindow2::autoSave);

    mEditor->tools()->setDefaultTool();
    // Pencil is the default active tool; Pen is still available internally and is
    // activated via the pinned "Final Line" preset.
    mEditor->tools()->setCurrentTool(PENCIL);

    // Ensure the Pencil tool's canvas cursor overlay is initialized immediately.
    // On startup, the mouse may already be over the canvas, so ScribbleArea may
    // not receive an Enter event and StrokeTool::enterEvent won't run until the
    // first click. This forces a first cursor paint without requiring user input.
    QTimer::singleShot(0, this, [this]() {
        if (!mEditor || !mEditor->tools()) {
            return;
        }
        BaseTool* tool = mEditor->tools()->currentTool();
        if (auto* strokeTool = dynamic_cast<StrokeTool*>(tool)) {
            strokeTool->updateCanvasCursor(true);
        }
        if (ui && ui->scribbleArea) {
            ui->scribbleArea->updateToolCursor();
        }
    });

    if (mToolsToolbar) {
        for (QAction* a : mToolsToolbar->actions()) a->setChecked(false);
        if (mActionPencil) {
            mActionPencil->setChecked(true);
        }
    }
    ui->background->init(mEditor->preference());

    setWindowTitle(PENCIL_WINDOW_TITLE);

    // Install shortcut tap/hold filter on tool actions and canvas
    if (!mShortcutFilter) {
        mShortcutFilter = new ShortcutFilter(ui->scribbleArea, this);
        // Install on all QAction children so tool shortcuts start tracking
        for (QAction* a : findChildren<QAction*>()) {
            a->installEventFilter(mShortcutFilter);
        }
    }

    // Install global event filter to detect attempts on restricted actions
    qApp->installEventFilter(this);
    if (mTimeLine) { mTimeLine->installEventFilter(this); }
}

MainWindow2::~MainWindow2()
{
    delete ui;
}

void MainWindow2::createDockWidgets()
{
    try {
        mTimeLine = new TimeLine(this);
        if (!mTimeLine) throw std::runtime_error("Failed to create TimeLine");
        mTimeLine->setObjectName("TimeLine");
    } catch (const std::exception& e) {
        qCritical() << "MainWindow2: Failed to create TimeLine:" << e.what();
    }

    try {
        mColorBox = new ColorBox(this);
        if (!mColorBox) throw std::runtime_error("Failed to create ColorBox");
        mColorBox->setToolTip(tr("color palette:<br>use <b>(C)</b><br>toggle at cursor"));
        mColorBox->setObjectName("ColorWheel");
    } catch (const std::exception& e) {
        qCritical() << "MainWindow2: Failed to create ColorBox:" << e.what();
    }

    try {
        mColorInspector = new ColorInspector(this);
        if (!mColorInspector) throw std::runtime_error("Failed to create ColorInspector");
        mColorInspector->setToolTip(tr("Color inspector"));
        mColorInspector->setObjectName("Color Inspector");
    } catch (const std::exception& e) {
        qCritical() << "MainWindow2: Failed to create ColorInspector:" << e.what();
    }

    try {
        mColorPalette = new ColorPaletteWidget(this);
        if (!mColorPalette) throw std::runtime_error("Failed to create ColorPaletteWidget");
        if (!mEditor) {
            qCritical() << "MainWindow2: mEditor is null before setCore";
        } else {
            mColorPalette->setCore(mEditor);
        }
        mColorPalette->setObjectName("ColorPalette");
    } catch (const std::exception& e) {
        qCritical() << "MainWindow2: Failed to create ColorPaletteWidget:" << e.what();
    }

    mShiftTraceWidget = new ShiftTraceWidget(this);
    mShiftTraceWidget->setObjectName("Shift & Trace");

    mToolOptions = new ToolOptionWidget(this);
    mToolOptions->setObjectName("ToolOption");

    // Dedicated Brush Factory dock (brush engine panel for Pencil tool)
    mBrushFactoryDock = new BrushFactoryDock(this);
    mBrushFactoryDock->setObjectName("BrushFactoryDock");

    mToolBox = new ToolBoxDockWidget(this);
    mToolBox->setObjectName("ToolBox");

    // Reference Window
    mReferenceWindow = new ReferenceWindow(this);
    mReferenceWindow->setObjectName("Reference Window");

    // Dock widgets are kept as objects (their content is hosted in cosmic panels)
    // but are no longer registered in mDockWidgets or added to the Qt dock system.

    if (mEditor && mEditor->preference()) {
        connect(mEditor->preference(), &PreferenceManager::optionChanged, this, [this](SETTING s) {
            if (s == SETTING::SHIFT_TRACE_WINDOW_VISIBLE) {
                updateShiftTracePopupFromPreference();
            }
        });
    }

    mStartIcon = QIcon(":icons/controls/play.png");
    mStopIcon = QIcon(":icons/controls/stop.png");

    // Initialize all dock widgets individually since they're no longer in mDockWidgets
    const QList<BaseDockWidget*> allDocks = {
        mTimeLine, mColorBox, mColorInspector, mColorPalette,
        mShiftTraceWidget, mToolOptions, mBrushFactoryDock, mToolBox, mReferenceWindow
    };
    for (BaseDockWidget* pWidget : allDocks)
    {
        if (!pWidget) continue;
        pWidget->setEditor(mEditor);
        pWidget->initUI();
        qDebug() << "Init Dock widget: " << pWidget->objectName();
    }

    // Ghost edit mode lock: disable Timeline interactions while a Shift & Trace entry is being edited.
    if (mEditor && mTimeLine) {
        connect(mEditor->shiftTrace(), &ShiftTraceManager::activeMoveEntryChanged, this, [this](int id){
            const bool locked = (id != -1);
            mTimeLine->setEnabled(!locked);
        });
    }

    // All dock content is hosted in cosmic panels - no Qt dock areas needed.
    setDockNestingEnabled(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    makeConnections(mEditor, ui->scribbleArea);
    makeConnections(mEditor);
    makeConnections(mEditor, mTimeLine);
    makeConnections(mEditor, mColorBox);
    makeConnections(mEditor, mColorInspector);
    makeConnections(mEditor, mColorPalette);
    makeConnections(mEditor, mToolOptions);
    
    // Ensure toolbox widget shows the correct active tool after connections are made
    mToolBox->setActiveTool(PEN);

    updateShiftTracePopupFromPreference();

    // Pen palette + Auto Fill are now standard toolbars (no CosmicControlCenter)
    if (mPenPaletteToolbarWidget) {
        connect(mPenPaletteToolbarWidget, &PenColorPalette::colorChanged,
                this, &MainWindow2::onCanvasPaletteColorChanged, Qt::UniqueConnection);
        if (mEditor && mEditor->preference()) {
            mPenPaletteToolbarWidget->connectToPreferenceManager(mEditor->preference());
        }
    }
    if (mEditor && mEditor->color()) {
        connect(mEditor->color(), &ColorManager::colorChanged, this,
                [this](QColor c, int) {
                    if (mPenPaletteToolbarWidget) {
                        // Legacy behavior: if a preset is active, an external color change
                        // (picker/eyedropper/etc) should switch to the current-color slot
                        // so the UI reflects what color is actually in use.
                        if (mPenPaletteToolbarWidget->getActiveButtonIndex() > 0) {
                            mPenPaletteToolbarWidget->activateFirstButtonAndSetColor(c);
                        } else {
                            mPenPaletteToolbarWidget->setCurrentColor(c);
                        }
                    }
                    if (mEditor && mEditor->tools()) mEditor->tools()->clearStrokeColorOverride();
                },
                Qt::UniqueConnection);
    }

    if (ui->background) {
        if (!mCosmicLeftToolsBar) {
            mCosmicLeftToolsBar = new CosmicLeftToolPanel(mEditor, ui->background);
            mCosmicLeftToolsBar->raise();
            mCosmicLeftToolsBar->show();

            if (auto* p = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar)) {
                connect(p, &CosmicLeftToolPanel::reservedWidthChanged, this, [this]() {
                    updateCosmicToolsBarsPosition();
                    updateCanvasPalettePosition();
                    // Re-sync the options pane body visibility whenever the panel
                    // shows or hides (auto-hide cycle).  This ensures the pane body
                    // is un-force-hidden when the panel re-appears after the user
                    // switched tools while it was hidden.
                    if (mToolOptions) {
                        mToolOptions->refreshOptionsPaneState();
                    }
                });
            }
        }
        if (!mCosmicRightToolsBar) {
            mCosmicRightToolsBar = new CosmicRightToolPanel(mEditor, ui->background);
            mCosmicRightToolsBar->raise();
            mCosmicRightToolsBar->show();

            if (auto* p = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar)) {
                connect(p, &CosmicRightToolPanel::reservedWidthChanged, this, [this]() {
                    updateCosmicToolsBarsPosition();
                    updateCanvasPalettePosition();
                });
            }
        }

        if (!mCosmicBottomToolsBar) {
            mCosmicBottomToolsBar = new CosmicBottomToolsBar(ui->background);
            mCosmicBottomToolsBar->raise();
            mCosmicBottomToolsBar->show();

            connect(mCosmicBottomToolsBar, &CosmicBottomToolsBar::preferredHeightChanged, this, [this](int) {
                updateCosmicToolsBarsPosition();
                updateCanvasPalettePosition();
            });
        }

        // Canvas timecode overlay — top-left pill showing current frame
        if (!mCanvasTimecodeOverlay) {
            mCanvasTimecodeOverlay = new CanvasTimecodeOverlay(mEditor, ui->background);
            mCanvasTimecodeOverlay->raise();
            mCanvasTimecodeOverlay->show();
        }

        // Canvas keyframe scrubber — top-center pill
        if (!mCanvasKeyframeScrubber) {
            mCanvasKeyframeScrubber = new CanvasKeyframeScrubber(mEditor, ui->background);
            mCanvasKeyframeScrubber->raise();
            mCanvasKeyframeScrubber->show();
        }

        // Cell Bank floating panel + toggle button (bottom-right of canvas, above bottom bar)
        if (!mCellBankPanelToggle) {
            const QColor accentCol = CosmicColor::accent();
            const QColor inactiveCol(0x1f, 0x1f, 0x1f, 0xff);

            mCellBankPanelToggle = new CosmicCheckboxToggle(ui->background);
            mCellBankPanelToggle->setMode(CosmicCheckboxToggle::Custom);
            mCellBankPanelToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
            mCellBankPanelToggle->setBordered(true);
            mCellBankPanelToggle->setShowText(false);
            mCellBankPanelToggle->setShowIcon(true);
            mCellBankPanelToggle->setFixedSize(32, 32);
            mCellBankPanelToggle->setIconSize(QSize(18, 18));
            mCellBankPanelToggle->setBackgroundEnabled(true);
            mCellBankPanelToggle->setHoverHighlightEnabled(true);
            mCellBankPanelToggle->setToolTip(tr("Cell Bank"));

            // Tint icon: accent when active, dark when inactive
            auto refreshCellBankToggleIcon = [this, accentCol, inactiveCol]() {
                if (!mCellBankPanelToggle) return;
                const QColor tint = mCellBankPanelToggle->isChecked() ? accentCol : inactiveCol;
                QPixmap px = QIcon(QStringLiteral(":/Interaction/CellStack.svg")).pixmap(QSize(18, 18));
                if (!px.isNull()) {
                    QImage img = px.toImage().convertToFormat(QImage::Format_ARGB32);
                    QPainter p(&img);
                    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    p.fillRect(img.rect(), tint);
                    p.end();
                    mCellBankPanelToggle->setIcon(QIcon(QPixmap::fromImage(img)));
                }
            };

            const bool initiallyVisible = mEditor && mEditor->preference()
                                          ? mEditor->preference()->isOn(SETTING::CELL_BANK_VISIBLE)
                                          : false;
            mCellBankPanelToggle->setChecked(initiallyVisible);
            refreshCellBankToggleIcon();

            connect(mCellBankPanelToggle, &CosmicCheckboxToggle::toggled, this,
                    [this, refreshCellBankToggleIcon](bool checked) {
                refreshCellBankToggleIcon();
                if (mEditor && mEditor->preference())
                    mEditor->preference()->set(SETTING::CELL_BANK_VISIBLE, checked);
                if (mCellBankPanel) {
                    if (checked) {
                        updateCellBankPanelPosition();
                        mCellBankPanel->show();
                        mCellBankPanel->raise();
                        mCellBankPanel->updateUI();
                    } else {
                        mCellBankPanel->hide();
                    }
                }
            });

            mCellBankPanelToggle->raise();
            mCellBankPanelToggle->show();
        }

        if (!mCellBankPanel) {
            mCellBankPanel = new CellBankFloatingPanel(ui->background);
            mCellBankPanel->setEditor(mEditor);
            mCellBankPanel->initUI();
            mCellBankPanel->setFixedSize(280, 220);

            const bool initiallyVisible = mEditor && mEditor->preference()
                                          ? mEditor->preference()->isOn(SETTING::CELL_BANK_VISIBLE)
                                          : false;
            mCellBankPanel->setVisible(initiallyVisible);
            if (initiallyVisible) {
                mCellBankPanel->raise();
                mCellBankPanel->updateUI();
            }
        }

        updateCosmicToolsBarsPosition();

        // Track resize of the canvas background so the bars and palette always stay inset.
        ui->background->installEventFilter(this);
    }

    // Migrate existing dock contents into the new left panes (tools + options)
    if (auto* leftPanel = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar)) {
        CosmicToolsPane* toolsPane = leftPanel->toolsPane();
        CosmicToolsPane* optionsPane = leftPanel->toolsOptionsPane();

        if (toolsPane && !toolsPane->contentWidget() && mToolBox) {
            // Tools: migrate the actual toolbox widget.
            QWidget* toolsContent = mToolBox->getToolBoxWidget();
            if (toolsContent) {
                toolsContent->setParent(nullptr);
                toolsPane->setContentWidget(toolsContent);
                mToolBox->hide();
            }
        }

        if (optionsPane && !optionsPane->contentWidget() && mToolOptions) {
            // Options: migrate the *inner* options widget directly into the pane.
            // This avoids nested scrollbars and prevents content cutoff; the outer pane stack
            // already provides scrolling.
            QWidget* optionsContent = nullptr;
            if (auto* sa = qobject_cast<QScrollArea*>(mToolOptions->dockContentWidget())) {
                optionsContent = sa->takeWidget();
            }
            if (!optionsContent) {
                optionsContent = mToolOptions->innerOptionsWidget();
            }
            if (optionsContent) {
                optionsContent->setParent(nullptr);

                // Tools Options: force top alignment by hosting the options widget in a container
                // with an expanding bottom stretch. Keep this behavior specific to the options pane
                // (do not apply globally to all Cosmic panes).
                QWidget* topPinned = new QWidget(nullptr);
                auto* topPinnedLayout = new QVBoxLayout(topPinned);
                topPinnedLayout->setContentsMargins(0, 0, 0, 0);
                topPinnedLayout->setSpacing(0);
                topPinnedLayout->addWidget(optionsContent);
                topPinnedLayout->addStretch(1);
                topPinned->setLayout(topPinnedLayout);

                optionsPane->setContentWidget(topPinned);
                mToolOptions->hide();
            }

            mToolOptions->refreshOptionsPaneState();

            // Header-only when no options: force-hide the pane body if the tool options widget
            // reports that there is nothing visible for the active tool.
            optionsPane->setBodyForcedHidden(!mToolOptions->hasVisibleOptions());
            connect(mToolOptions, &ToolOptionWidget::visibleOptionsChanged, optionsPane, [optionsPane](bool hasOptions) {
                optionsPane->setBodyForcedHidden(!hasOptions);
            });
        }

        // After migrating content, force a deferred recompute. On first-run startup
        // some option blocks/toolbox sizing can settle after restoreState() and tool init.
        QTimer::singleShot(0, this, [this]() {
            auto* leftPanel = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar);
            if (!leftPanel) {
                return;
            }

            CosmicToolsPane* optionsPane = leftPanel->toolsOptionsPane();
            if (mToolOptions && optionsPane) {
                mToolOptions->refreshOptionsPaneState();
                optionsPane->setBodyForcedHidden(!mToolOptions->hasVisibleOptions());
            }

            leftPanel->refreshPaneLayout();

            if (auto* rightPanel = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar)) {
                rightPanel->refreshPaneLayout();
            }

            updateCosmicToolsBarsPosition();
            updateCanvasPalettePosition();
        });

        // Ensure insets update once the panel now contains real content.
        updateCosmicToolsBarsPosition();
        updateCanvasPalettePosition();

        if (mCosmicBottomToolsBar && !mCosmicBottomToolsBar->contentWidget() && mTimeLine) {
            QWidget* timelineContent = mTimeLine->dockContentWidget();
            if (timelineContent) {
                timelineContent->setParent(nullptr);
                mCosmicBottomToolsBar->setContentWidget(timelineContent);
                mTimeLine->hide();

                updateCosmicToolsBarsPosition();
                updateCanvasPalettePosition();
            }
        }
        
        // Migrate Reference Window and Color Palette into the right panel
        if (auto* rightPanel = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar)) {
            CosmicToolsPane* colorWheelPane = rightPanel->colorWheelPane();
            CosmicToolsPane* palettesPane = rightPanel->palettesPane();
            CosmicToolsPane* referencePane = rightPanel->referencePane();

            qDebug() << "Right panel palettesPane:" << palettesPane << "contentWidget:" << (palettesPane ? palettesPane->contentWidget() : nullptr);
            qDebug() << "Right panel referencePane:" << referencePane << "contentWidget:" << (referencePane ? referencePane->contentWidget() : nullptr);
            qDebug() << "mColorPalette:" << mColorPalette;
            qDebug() << "mReferenceWindow:" << mReferenceWindow;

            if (colorWheelPane && !colorWheelPane->contentWidget() && mEditor && mEditor->color()) {
                auto* wheel = new CosmicColorWheelWidget(colorWheelPane);
                wheel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                colorWheelPane->setContentWidget(wheel);
                colorWheelPane->update();

                // Sync from global color manager
                if (mEditor && mEditor->color()) {
                    wheel->setColor(mEditor->color()->frontColor());
                }
                connect(mEditor->color(), &ColorManager::colorChanged,
                        wheel, [wheel](QColor c, int) {
                            wheel->setColor(c);
                        });
                // Push to global color manager
                connect(wheel, &CosmicColorWheelWidget::colorChanged,
                        mEditor->color(), &ColorManager::setFrontColor);
            }

            CosmicToolsPane* colorSlidersPane = rightPanel->colorSlidersPane();
            if (colorSlidersPane && !colorSlidersPane->contentWidget()
                    && mEditor && mEditor->color()) {
                auto* sliders = new CosmicColorSliders(colorSlidersPane);
                sliders->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                colorSlidersPane->setContentWidget(sliders);
                colorSlidersPane->update();

                if (mEditor->color()->frontColor().isValid()) {
                    sliders->setColor(mEditor->color()->frontColor());
                }
                connect(mEditor->color(), &ColorManager::colorChanged,
                        sliders, [sliders](QColor c, int) {
                            sliders->setColor(c);
                        });
                connect(sliders, &CosmicColorSliders::colorChanged,
                        mEditor->color(), &ColorManager::setFrontColor);
            }

            if (palettesPane && !palettesPane->contentWidget() && mColorPalette) {
                // Palettes: extract the actual inner content widget from the dock.
                QWidget* palettesContent = mColorPalette->dockContentWidget();
                if (!palettesContent) {
                    // Fallback: take the dock's inner widget (the container set by BaseDockWidget)
                    palettesContent = mColorPalette->widget();
                    if (palettesContent) {
                        palettesContent->setParent(nullptr);
                        
                        // Apply pill scrollbar styling to match tool panels
                        palettesContent->setStyleSheet(QStringLiteral(
                            "QWidget { background: transparent; border: none; }"
                            "QListWidget { background: transparent; border: none; }"
                            "QScrollBar:vertical {"
                            "  background: #1f1f1f;"
                            "  border: none;"
                            "  border-radius: 6px;"
                            "  width: 12px;"
                            "  margin: 0px;"
                            "}"
                            "QScrollBar::handle:vertical {"
                            "  background: #505050;"
                            "  border-radius: 6px;"
                            "  min-height: 26px;"
                            "}"
                            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
                            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
                            "QScrollBar:horizontal {"
                            "  background: #1f1f1f;"
                            "  border: none;"
                            "  border-radius: 6px;"
                            "  height: 12px;"
                            "  margin: 0px;"
                            "}"
                            "QScrollBar::handle:horizontal {"
                            "  background: #505050;"
                            "  border-radius: 6px;"
                            "  min-width: 26px;"
                            "}"
                            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: transparent; width: 0px; }"
                            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
                        ));
                    }
                }
                if (palettesContent) {
                    qDebug() << "Setting palettesContent as content for Palettes pane:" << palettesContent;
                    
                    // Apply pill scrollbar styling to match tool panels
                    palettesContent->setStyleSheet(QStringLiteral(
                        "QWidget { background: transparent; border: none; }"
                        "QListWidget { background: transparent; border: none; }"
                        "QScrollBar:vertical {"
                        "  background: #1f1f1f;"
                        "  border: none;"
                        "  border-radius: 6px;"
                        "  width: 12px;"
                        "  margin: 0px;"
                        "}"
                        "QScrollBar::handle:vertical {"
                        "  background: #505050;"
                        "  border-radius: 6px;"
                        "  min-height: 26px;"
                        "}"
                        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
                        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
                        "QScrollBar:horizontal {"
                        "  background: #1f1f1f;"
                        "  border: none;"
                        "  border-radius: 6px;"
                        "  height: 12px;"
                        "  margin: 0px;"
                        "}"
                        "QScrollBar::handle:horizontal {"
                        "  background: #505050;"
                        "  border-radius: 6px;"
                        "  min-width: 26px;"
                        "}"
                        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: transparent; width: 0px; }"
                        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
                    ));
                    
                    palettesPane->setContentWidget(palettesContent);
                    palettesPane->update();
                } else {
                    qDebug() << "No palettesContent found";
                }
            }
            if (referencePane && !referencePane->contentWidget() && mReferenceWindow) {
                // Reference: extract the actual inner content widget from the dock.
                QWidget* referenceContent = mReferenceWindow->dockContentWidget();
                if (referenceContent) {
                    qDebug() << "Setting referenceContent as content for Reference pane:" << referenceContent;
                    referencePane->setContentWidget(referenceContent);
                    referencePane->update();
                } else {
                    qDebug() << "No referenceContent found";
                }
            }

            // Ensure the right panel layout updates after content migration
            rightPanel->updateGeometry();
            rightPanel->update();
            rightPanel->show();

            // Hide the original docks since their content is now in the right panel.
            if (mColorPalette && mColorPalette->isVisible()) {
                mColorPalette->hide();
                qDebug() << "Hiding original ColorPalette dock";
            }
            if (mReferenceWindow) {
                mReferenceWindow->setVisible(false);
                qDebug() << "Hiding original ReferenceWindow dock";
            }
        } else {
            qDebug() << "Failed to cast mCosmicRightToolsBar to CosmicRightToolPanel";
        }
        
        // Connect brush preset selection to the brush factory system
        if (auto* brushesPane = leftPanel->brushesPane()) {
            if (auto* brushesContent = qobject_cast<BrushPresetListWidget*>(brushesPane->contentWidget())) {
                // Connect preset selection to change active brush preset
                connect(brushesContent, &BrushPresetListWidget::presetSelected, this, [this](int index) {
                    // Special pinned preset: Final Line (tool switch)
                    if (index < 0) {
                        // Persist a global flag so bitmap tools can switch to the
                        // pixelated (non-antialiased) pipeline without forcing a tool switch.
                        {
                            QSettings settings(PENCIL2D, PENCIL2D);
                            settings.beginGroup("PencilBrushFactory");
                            settings.setValue("FinalLineActive", true);
                            settings.setValue("ActivePresetId", QStringLiteral("__final_line__"));
                            settings.endGroup();
                            settings.sync();
                        }

                        if (mToolBox) {
                            // Only force-switch when the user is currently on Pencil.
                            // Curve/Polyline/BSpline should stay on their tool and just
                            // use the Final Line pipeline when committing.
                            if (mEditor && mEditor->tools()) {
                                if (BaseTool* cur = mEditor->tools()->currentTool()) {
                                    if (cur->type() == PENCIL) {
                                        mToolBox->setActiveTool(PEN);
                                    }
                                }
                            }
                        } else if (mEditor && mEditor->tools()) {
                            if (BaseTool* cur = mEditor->tools()->currentTool()) {
                                if (cur->type() == PENCIL) {
                                    mEditor->tools()->setCurrentTool(PEN);
                                }
                            }
                        }
                        return;
                    }

                    // Any normal preset implies pencil-mode (not Final Line).
                    {
                        QSettings settings(PENCIL2D, PENCIL2D);
                        settings.beginGroup("PencilBrushFactory");
                        settings.setValue("FinalLineActive", false);
                        settings.endGroup();
                        settings.sync();
                    }

                    // UI is source of truth: selecting a normal brush preset means the
                    // brush (Pencil) pipeline should be active. If the user was previously
                    // on Final Line (Pen) and clicks a preset, switch Pen -> Pencil.
                    // Do NOT force-switch Curve/Polyline/BSpline; they commit via PencilTool
                    // while remaining on their own tool.
                    if (mEditor && mEditor->tools()) {
                        if (BaseTool* cur = mEditor->tools()->currentTool()) {
                            if (cur->type() == PEN) {
                                if (mToolBox) {
                                    mToolBox->setActiveTool(PENCIL);
                                } else {
                                    mEditor->tools()->setCurrentTool(PENCIL);
                                }
                            }
                        }
                    }

                    if (mBrushFactoryDock) {
                        // The store's active preset ID was already set by onPresetSelected
                        // before this signal fired. Use the store's index so BrushFactoryDock
                        // always operates on the same list ordering as the store.
                        const int storeIdx = BrushPresetStore::instance()->activeIndex();
                        mBrushFactoryDock->setActivePreset(storeIdx);
                    }
                });

                // Re-apply the active preset whenever the user switches to the Pencil tool
                // (or any tool that commits strokes via PencilTool: Curve, BSpline, Polyline).
                // Without this, the Brushes pane UI state is never replayed on tool entry.
                // Use the store's activeIndex() as the authoritative source rather than
                // brushesContent->activePreset(), which may lag behind in edge cases where
                // a preset was selected without drawing (no "commit" stroke occurred).
                connect(mEditor->tools(), &ToolManager::toolChanged, this, [brushesContent, this](ToolType newTool) {
                    const bool isPencilFamily = (newTool == PENCIL || newTool == CURVE
                                                 || newTool == BSPLINE || newTool == POLYLINE);
                    if (!isPencilFamily) return;
                    if (!brushesContent) return;

                    if (brushesContent->isFinalLineActive()) {
                        // Replay the Final Line click path: persist flags and switch to PEN.
                        {
                            QSettings settings(PENCIL2D, PENCIL2D);
                            settings.beginGroup("PencilBrushFactory");
                            settings.setValue("FinalLineActive", true);
                            settings.setValue("ActivePresetId", QStringLiteral("__final_line__"));
                            settings.endGroup();
                            settings.sync();
                        }
                        // Only force-switch to PEN when landing on Pencil itself.
                        // Curve/BSpline/Polyline handle Final Line internally via the flag.
                        if (newTool == PENCIL) {
                            if (mToolBox) {
                                mToolBox->setActiveTool(PEN);
                            } else if (mEditor && mEditor->tools()) {
                                mEditor->tools()->setCurrentTool(PEN);
                            }
                        }
                    } else {
                        // Replay the normal preset click path using the store as source of truth.
                        {
                            QSettings settings(PENCIL2D, PENCIL2D);
                            settings.beginGroup("PencilBrushFactory");
                            settings.setValue("FinalLineActive", false);
                            settings.endGroup();
                            settings.sync();
                        }
                        if (mBrushFactoryDock) {
                            // Read from the store directly so a preset selected without drawing
                            // is still honoured — brushesContent->activePreset() can be stale
                            // if the selection was never "committed" via a stroke.
                            const int storeIdx = BrushPresetStore::instance()->activeIndex();
                            mBrushFactoryDock->setActivePreset(storeIdx);
                        }
                    }
                });

                // Keep Brushes pane highlight in sync when the active preset is changed elsewhere
                if (mBrushFactoryDock) {
                    connect(mBrushFactoryDock, &BrushFactoryDock::activePresetAboutToChange,
                            brushesContent, [brushesContent](int, int newIndex) {
                        brushesContent->setActivePreset(newIndex);
                    });

                    // Keep Tool Options in sync as well (Opacity is per preset).
                    if (mToolOptions) {
                        connect(mBrushFactoryDock, &BrushFactoryDock::activePresetAboutToChange,
                                mToolOptions, [this](int, int){
                            if (mToolOptions) {
                                // Defer until after BrushFactoryDock::setActivePreset finishes
                                // updating QSettings + applying opacity.
                                QTimer::singleShot(0, mToolOptions, [this]() {
                                    if (mToolOptions) {
                                        mToolOptions->updatePencilOpacityField();
                                    }
                                });
                            }
                        });
                    }
                }
                
                // Connect preset settings request (stub for now)
                connect(brushesContent, &BrushPresetListWidget::presetSettingsRequested, this, [this](int index) {
                    // TODO: Show preset settings dialog/dock
                    qDebug() << "Preset settings requested for index:" << index;
                });

                // Push Final Line pressure settings live to PenTool when changed in the popup.
                // Connect to the widget signal (always available) rather than the lazily-created
                // popup signal, so the connection is never missed.
                connect(brushesContent, &BrushPresetListWidget::finalLinePressureChanged, this,
                        [this](bool enabled, const QVector<QPointF>& pts) {
                    if (!mEditor || !mEditor->tools()) return;
                    auto* pen = qobject_cast<PenTool*>(mEditor->tools()->getTool(PEN));
                    if (!pen) return;
                    pen->setPressureCurveEnabled(enabled);
                    pen->setPressureCurve(pts);
                    pen->saveSettings();
                });

                // Seed the popup with live pen tool state when Final Line settings are opened.
                connect(brushesContent, &BrushPresetListWidget::finalLineSettingsRequested, this,
                        [this, brushesContent]() {
                    if (!mEditor || !mEditor->tools()) return;
                    auto* pen = qobject_cast<PenTool*>(mEditor->tools()->getTool(PEN));
                    if (!pen) return;
                    if (auto* popup = brushesContent->settingsPopup()) {
                        popup->setFinalLineLiveState(pen->isPressureCurveEnabled(), pen->getPressureCurve());
                    }
                });

                // Persist preset mutations (duplicate/rename/delete) into the brush factory (source of truth).
                connect(brushesContent, &BrushPresetListWidget::presetsChanged, this,
                        [this](const QVector<ToolOptionWidget::BrushPreset>& presets, int activeIndex) {
                            qDebug() << "[MainWindow2] Brushes pane presetsChanged count=" << presets.size()
                                     << "activeIndex=" << activeIndex
                                     << "activeName=" << (activeIndex >= 0 && activeIndex < presets.size() ? presets[activeIndex].name : QString());
                            if (mBrushFactoryDock) {
                                mBrushFactoryDock->applyExternalPresets(presets, activeIndex);
                            } else {
                                qDebug() << "[MainWindow2] mBrushFactoryDock is null; cannot applyExternalPresets";
                            }
                        },
                        Qt::UniqueConnection);

                // Store-driven pipeline: BrushFactoryDock follows BrushPresetStore.
                connect(BrushPresetStore::instance(), &BrushPresetStore::presetsUpdated, this,
                        [this](const QVector<ToolOptionWidget::BrushPreset>& presets, const QString& activeId) {
                            if (!mBrushFactoryDock || presets.isEmpty()) {
                                return;
                            }
                            int activeIndex = 0;
                            for (int i = 0; i < presets.size(); ++i) {
                                if (presets[i].id == activeId) {
                                    activeIndex = i;
                                    break;
                                }
                            }
                            mBrushFactoryDock->applyStorePresets(presets, activeIndex);
                        },
                        Qt::UniqueConnection);

                // Restore Final Line (Pen) active state on startup.
                // The widget already highlights the Pen item; here we ensure the actual
                // tool is PEN and the BrushFactoryDock is in sync.
                if (brushesContent->isFinalLineActive()) {
                    QSettings settings(PENCIL2D, PENCIL2D);
                    settings.beginGroup("PencilBrushFactory");
                    settings.setValue("FinalLineActive", true);
                    settings.setValue("ActivePresetId", QStringLiteral("__final_line__"));
                    settings.endGroup();
                    settings.sync();

                    if (mEditor && mEditor->tools()) {
                        mEditor->tools()->setCurrentTool(PEN);
                    }
                }
            }
        }
    }

    // CanvasPaletteOverlay remains available (toggleable) but is no longer used for the top header.

    // Create Gallery overlay (on background) used to show reference images grid over the canvas
    if (!mCanvasGalleryOverlay) {
        if (!ui->background) {
            qCritical() << "MainWindow2: ui->background is null, cannot create CanvasGalleryOverlay";
        } else {
            try {
                mCanvasGalleryOverlay = new CanvasGalleryOverlay(ui->background);
                if (!mCanvasGalleryOverlay) {
                    qCritical() << "MainWindow2: Failed to create CanvasGalleryOverlay";
                } else {
                    mCanvasGalleryOverlay->hideOverlay();
                    mCanvasGalleryOverlay->raise();
                }
            } catch (const std::exception& e) {
                qCritical() << "MainWindow2: Exception creating CanvasGalleryOverlay:" << e.what();
                mCanvasGalleryOverlay = nullptr;
            }
        }
    }

    QSettings dockDefaultsSettings(PENCIL2D, PENCIL2D);

    // Call updateUI on all dock objects (content is hosted in cosmic panels)
    const QList<BaseDockWidget*> allDocksForUpdate = {
        mTimeLine, mColorBox, mColorInspector, mColorPalette,
        mShiftTraceWidget, mToolOptions, mBrushFactoryDock, mToolBox, mReferenceWindow
    };
    for (BaseDockWidget* w : allDocksForUpdate)
    {
        if (!w) continue;
        try {
            w->updateUI();
        } catch (const std::exception& e) {
            qCritical() << "MainWindow2: Exception updating dock widget" << w->objectName() << ":" << e.what();
        }
    }
    
    // After all dock widgets are shown, trigger accent color update to ensure
    // all UI elements sync with the accent color preference where applicable.
    // Tab visual appearance is controlled by the dark_theme.qss stylesheet.
    onColorPreferenceChanged(SETTING::ACCENT_COLOR);
    onColorPreferenceChanged(SETTING::TOOLBOX_SELECTION_COLOR);
    
    // Apply UI theme colors from preferences
    updateUIThemeColors();
    
    // Update toolbox and canvas palette colors
    if (mToolBox && mToolBox->getToolBoxWidget()) {
        mToolBox->getToolBoxWidget()->onColorPreferenceChanged(SETTING::TOOLBOX_SELECTION_COLOR);
    }
    if (mCanvasPaletteOverlay && mCanvasPaletteOverlay->getPalette()) {
        mCanvasPaletteOverlay->getPalette()->onColorPreferenceChanged(SETTING::TOOL_PALETTE_SELECTION_COLOR);
    }

    // Wire ReferenceWindow Gallery to CanvasGalleryOverlay
    if (mReferenceWindow) {
        connect(mReferenceWindow, &ReferenceWindow::galleryContentUpdated, this,
                [this](const QVector<QImage>& imgs, const QStringList& labels, int currentIndex, const QStringList& categories){
            if (mCanvasGalleryOverlay) mCanvasGalleryOverlay->setImages(imgs, labels, currentIndex, categories);
        });
        connect(mReferenceWindow, &ReferenceWindow::galleryToggled, this, [this](bool on){
            if (!mCanvasGalleryOverlay || !ui->background || !ui->scribbleArea) return;
            if (on) {
                const QRect sr = ui->scribbleArea->rect();
                const QPoint centerOnBg = ui->scribbleArea->mapTo(ui->background, sr.center());
                const QPoint globalCenter = ui->background->mapToGlobal(centerOnBg);
                mCanvasGalleryOverlay->showOverlayAt(globalCenter);
            } else {
                mCanvasGalleryOverlay->hideOverlay();
            }
        });
        if (mCanvasGalleryOverlay) {
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::imageActivated,
                    mReferenceWindow, &ReferenceWindow::selectGalleryIndex);
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::requestDeleteSelected,
                    mReferenceWindow, &ReferenceWindow::deleteImagesByIndices);
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::requestClearAll,
                    mReferenceWindow, &ReferenceWindow::clearAllImagesSlot);
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::requestDeleteCategory,
                    mReferenceWindow, &ReferenceWindow::deleteCategorySlot);
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::activeCategoryChanged,
                    mReferenceWindow, &ReferenceWindow::setActiveCategorySlot);
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::requestImportMimeData,
                    this, [this](const QMimeData* mime) {
                        if (mReferenceWindow) mReferenceWindow->importFromMimeDataSlot(mime);
                    });
            connect(mCanvasGalleryOverlay, &CanvasGalleryOverlay::requestClose, this, [this]{
                if (!mCanvasGalleryOverlay) return;
                mCanvasGalleryOverlay->hideOverlay();
                if (mReferenceWindow) mReferenceWindow->setGalleryToggle(false);
            });
        }
        mReferenceWindow->emitGalleryUpdate();
    }

    // Brush Factory dock remains available via the Brushes pane presets; the Tool Options
    // pane no longer exposes a Brush Factory toggle button.
}

void MainWindow2::createMenus()
{
    //--- File Menu ---
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow2::newDocument);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow2::openDocument);
    connect(ui->actionSave_as, &QAction::triggered, this, &MainWindow2::saveAsNewDocument);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow2::saveDocument);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow2::close);

    //--- Export Menu ---
    connect(ui->actionExport_Image, &QAction::triggered, mCommands, &ActionCommands::exportImage);
    // Removed: Current Layer Animation (Image Sequence) export action
    connect(ui->actionExport_Animation, &QAction::triggered, mCommands, &ActionCommands::exportAnimation);
    connect(ui->actionExport_Sprite, &QAction::triggered, mCommands, &ActionCommands::exportSprite);
    connect(ui->actionExport_Gif, &QAction::triggered, mCommands, &ActionCommands::exportGif);
    // Removed: Export Palette action

    //--- Import Menu ---
    //connect( ui->actionExport_Svg_Image, &QAction::triggered, editor, &Editor::saveSvg );
    connect(ui->actionImportLayers_from_pclx, &QAction::triggered, this, &MainWindow2::importLayers);
    connect(ui->actionImport_Image, &QAction::triggered, this, &MainWindow2::importSingleImage);
    connect(ui->actionImport_ImageSeq, &QAction::triggered, this, &MainWindow2::importImageSequenceNew);
    // Remove problematic/non-functional import options (defensive cleanup if any linger)
    if (ui->menuFile && ui->menuImport) {
        QList<QAction*> toRemove;
        for (QAction* a : ui->menuImport->actions()) {
            const QString t = a->text();
            if (t.contains(tr("Movie Video"), Qt::CaseInsensitive)
                || t.contains(tr("Animated Image"), Qt::CaseInsensitive)
                || t.contains(tr("Sound"), Qt::CaseInsensitive)
                || t.contains(tr("Movie Audio"), Qt::CaseInsensitive)) {
                toRemove.append(a);
            }
        }
        for (QAction* a : toRemove) { ui->menuImport->removeAction(a); }
    }

    // Removed: Palette import options (Append/Replace)

    //--- Edit Menu ---
    replaceUndoRedoActions();
    // NOTE: We intentionally do NOT reconnect replaceUndoRedoActions() to
    // activeStackChanged here. The UndoRedoManager::undo()/redo() slots already
    // dispatch to the correct active stack dynamically, so the QAction objects
    // never need to be recreated. Recreating them on every stack switch caused
    // shortcut ambiguity accumulation (multiple Ctrl+Z actions on the window)
    // which broke undo/redo after creating a second layer.
    // updateActions() (called from setActiveCellStack) handles enable/disable correctly.
    connect(ui->actionCut, &QAction::triggered, mEditor, &Editor::copyAndCut);
    connect(ui->actionCopy, &QAction::triggered, mEditor, &Editor::copy);
    connect(ui->actionPaste_Previous, &QAction::triggered, mEditor, &Editor::pasteFromPreviousFrame);
    connect(ui->actionPaste, &QAction::triggered, mEditor, &Editor::paste);
    connect(ui->actionClearFrame, &QAction::triggered, mEditor, &Editor::clearCurrentFrame);
    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &MainWindow2::selectionChanged);
    connect(ui->actionFlip_X, &QAction::triggered, mCommands, &ActionCommands::flipSelectionX);
    connect(ui->actionFlip_Y, &QAction::triggered, mCommands, &ActionCommands::flipSelectionY);
    connect(ui->actionPegbarAlignment, &QAction::triggered, this, &MainWindow2::openPegAlignDialog);
    connect(ui->actionAdd_Transparency_to_paper, &QAction::triggered, this, &MainWindow2::openAddTranspToPaperDialog);
    connect(ui->actionSelect_All, &QAction::triggered, mCommands, &ActionCommands::selectAll);
    connect(ui->actionDeselect_All, &QAction::triggered, mCommands, &ActionCommands::deselectAll);
    connect(ui->actionReposition_Selected_Frames, &QAction::triggered, this, &MainWindow2::openRepositionDialog);
    connect(ui->actionPreference, &QAction::triggered, [=] { preferences(); });

    //--- Layer / Tools / Animation menus removed ---
    if (ui->menuLayer) { ui->menuLayer->menuAction()->setVisible(false); }
    if (ui->menuTools) { ui->menuTools->menuAction()->setVisible(false); }
    if (ui->menuAnimation) { ui->menuAnimation->menuAction()->setVisible(false); }

    //--- Layer Menu (connections kept for toolbar/shortcut use) ---
    connect(ui->actionNew_Bitmap_Layer, &QAction::triggered, mCommands, &ActionCommands::addNewBitmapLayer);
    // Defensive: ensure sound layer menu item is not visible/usable
    if (ui->actionNew_Sound_Layer) {
        ui->menuLayer->removeAction(ui->actionNew_Sound_Layer);
        ui->actionNew_Sound_Layer->setEnabled(false);
        ui->actionNew_Sound_Layer->setVisible(false);
    }
    connect(ui->actionDelete_Current_Layer, &QAction::triggered, mCommands, &ActionCommands::deleteCurrentLayer);
    connect(ui->actionChangeLineColorCurrent_keyframe, &QAction::triggered, mCommands, &ActionCommands::changeKeyframeLineColor);
    connect(ui->actionChangeLineColorAll_keyframes_on_layer, &QAction::triggered, mCommands, &ActionCommands::changeallKeyframeLineColor);
    connect(ui->actionChangeLayerOpacity, &QAction::triggered, this, &MainWindow2::openLayerOpacityDialog);

    QList<QAction*> visibilityActions = ui->menuLayer_Visibility->actions();
    auto visibilityGroup = new QActionGroup(this);
    visibilityGroup->setExclusive(true);
    for (int i = 0; i < visibilityActions.size(); i++) {
        QAction* action = visibilityActions[i];
        visibilityGroup->addAction(action);
        connect(action, &QAction::triggered, [=] { mCommands->setLayerVisibilityIndex(i); });
    }
    visibilityActions[mEditor->preference()->getInt(SETTING::LAYER_VISIBILITY)]->setChecked(true);
    connect(mEditor->preference(), &PreferenceManager::optionChanged, [=](SETTING e) {
        if (e == SETTING::LAYER_VISIBILITY) {
            visibilityActions[mEditor->preference()->getInt(SETTING::LAYER_VISIBILITY)]->setChecked(true);
        }
    });

    // --- View Menu ---
    connect(ui->actionZoom_In, &QAction::triggered, mCommands, &ActionCommands::ZoomIn);
    connect(ui->actionZoom_Out, &QAction::triggered, mCommands, &ActionCommands::ZoomOut);
    connect(ui->actionRotate_Clockwise, &QAction::triggered, mCommands, &ActionCommands::rotateClockwise);
    connect(ui->actionRotate_Anticlockwise, &QAction::triggered, mCommands, &ActionCommands::rotateCounterClockwise);
    connect(ui->actionReset_Rotation, &QAction::triggered, mEditor->view(), &ViewManager::resetRotation);
    connect(ui->actionReset_View, &QAction::triggered, mEditor->view(), &ViewManager::resetView);
    connect(ui->actionCenter_View, &QAction::triggered, mEditor->view(), &ViewManager::centerView);
    connect(ui->actionZoom400, &QAction::triggered, mEditor->view(), &ViewManager::scale400);
    connect(ui->actionZoom300, &QAction::triggered, mEditor->view(), &ViewManager::scale300);
    connect(ui->actionZoom200, &QAction::triggered, mEditor->view(), &ViewManager::scale200);
    connect(ui->actionZoom100, &QAction::triggered, mEditor->view(), &ViewManager::scale100);
    connect(ui->actionZoom50, &QAction::triggered, mEditor->view(), &ViewManager::scale50);
    connect(ui->actionZoom33, &QAction::triggered, mEditor->view(), &ViewManager::scale33);
    connect(ui->actionZoom25, &QAction::triggered, mEditor->view(), &ViewManager::scale25);
    connect(ui->actionHorizontal_Flip, &QAction::triggered, mEditor->view(), &ViewManager::flipHorizontal);
    connect(ui->actionVertical_Flip, &QAction::triggered, mEditor->view(), &ViewManager::flipVertical);
    connect(mEditor->view(), &ViewManager::viewFlipped, this, &MainWindow2::viewFlipped);

    PreferenceManager* prefs = mEditor->preference();
    bindPreferenceSetting(ui->actionSafeAreas, prefs, SETTING::OVERLAY_SAFE);
    bindPreferenceSetting(ui->actionGoldenRatio, prefs, SETTING::OVERLAY_GOLDEN);
    bindPreferenceSetting(ui->actionThirds, prefs, SETTING::OVERLAY_THIRDS);
    bindPreferenceSetting(ui->actionCenter, prefs, SETTING::OVERLAY_CENTER);
    bindPreferenceSetting(ui->actionOnionPrev, prefs, SETTING::PREV_ONION);
    bindPreferenceSetting(ui->actionOnionNext, prefs, SETTING::NEXT_ONION);

    // Removed UI/features: Grid / Invisible Lines / Outlines Only / Perspective overlays.
    // Hide from menus and ensure nothing re-enables them.
    if (ui->actionGrid) {
        ui->actionGrid->setVisible(false);
        ui->actionGrid->setEnabled(false);
    }
    if (ui->actionShowInvisibleLines) {
        ui->actionShowInvisibleLines->setVisible(false);
        ui->actionShowInvisibleLines->setEnabled(false);
    }
    if (ui->actionShowOutlinesOnly) {
        ui->actionShowOutlinesOnly->setVisible(false);
        ui->actionShowOutlinesOnly->setEnabled(false);
    }
    if (ui->actionOnePointPerspective) {
        ui->actionOnePointPerspective->setVisible(false);
        ui->actionOnePointPerspective->setEnabled(false);
    }
    if (ui->actionTwoPointPerspective) {
        ui->actionTwoPointPerspective->setVisible(false);
        ui->actionTwoPointPerspective->setEnabled(false);
    }
    if (ui->actionThreePointPerspective) {
        ui->actionThreePointPerspective->setVisible(false);
        ui->actionThreePointPerspective->setEnabled(false);
    }
    if (ui->menuPerspectiveLinesAngle) {
        ui->menuPerspectiveLinesAngle->menuAction()->setVisible(false);
        ui->menuPerspectiveLinesAngle->menuAction()->setEnabled(false);
    }

    bool enableSafeArea = (prefs->isOn(SETTING::ACTION_SAFE_ON) || prefs->isOn(SETTING::TITLE_SAFE_ON));
    ui->actionSafeAreas->setEnabled(enableSafeArea);

    auto perspectiveLinesAngleGroup = new QActionGroup(this);
    perspectiveLinesAngleGroup->setExclusive(true);
    perspectiveLinesAngleGroup->addAction(ui->action2Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action3Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action5Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action7_5Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action10Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action15Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action20Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action30Degrees);
    connect(perspectiveLinesAngleGroup, &QActionGroup::triggered, this, [&](QAction* action) {
        if (action == ui->action2Degrees)        mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 2);
        else if (action == ui->action3Degrees)   mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 3);
        else if (action == ui->action5Degrees)   mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 5);
        else if (action == ui->action7_5Degrees) mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 7);
        else if (action == ui->action10Degrees)  mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 10);
        else if (action == ui->action15Degrees)  mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 15);
        else if (action == ui->action20Degrees)  mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 20);
        else if (action == ui->action30Degrees)  mEditor->preference()->set(SETTING::OVERLAY_ANGLE, 30);
        else Q_UNREACHABLE();
        emit mEditor->view()->viewChanged();
    });

    switch (prefs->getInt(SETTING::OVERLAY_ANGLE)) {
    case 2: ui->action2Degrees->setChecked(true); break;
    case 3: ui->action3Degrees->setChecked(true); break;
    case 5: ui->action5Degrees->setChecked(true); break;
    case 7: ui->action7_5Degrees->setChecked(true); break;
    case 10: ui->action10Degrees->setChecked(true); break;
    case 15: ui->action15Degrees->setChecked(true); break;
    case 20: ui->action20Degrees->setChecked(true); break;
    case 30: ui->action30Degrees->setChecked(true); break;
    }

    //--- Animation Menu ---
    PlaybackManager* pPlaybackManager = mEditor->playback();
    connect(ui->actionPlay, &QAction::triggered, this, &MainWindow2::onPlayTriggered);

    connect(ui->actionLoop, &QAction::triggered, pPlaybackManager, &PlaybackManager::setLooping);
    connect(ui->actionLoopControl, &QAction::triggered, pPlaybackManager, &PlaybackManager::enableRangedPlayback);
    connect(pPlaybackManager, &PlaybackManager::loopStateChanged, ui->actionLoop, &QAction::setChecked);
    connect(pPlaybackManager, &PlaybackManager::loopStateChanged, mTimeLine, &TimeLine::setLoop);
    connect(pPlaybackManager, &PlaybackManager::rangedPlaybackStateChanged, ui->actionLoopControl, &QAction::setChecked);
    connect(pPlaybackManager, &PlaybackManager::rangedPlaybackStateChanged, mTimeLine, &TimeLine::setRangeState);
    connect(pPlaybackManager, &PlaybackManager::playStateChanged, mTimeLine, &TimeLine::setPlaying);
    connect(pPlaybackManager, &PlaybackManager::playStateChanged, this, &MainWindow2::changePlayState);
    connect(pPlaybackManager, &PlaybackManager::playStateChanged, ui->scribbleArea, &ScribbleArea::onPlayStateChanged);
    connect(ui->actionFlip_inbetween, &QAction::triggered, pPlaybackManager, &PlaybackManager::playFlipInBetween);
    connect(ui->actionFlip_rolling, &QAction::triggered, pPlaybackManager, &PlaybackManager::playFlipRoll);

    connect(ui->actionAdd_Frame, &QAction::triggered, mCommands, &ActionCommands::insertKeyFrameAtCurrentPosition);
    connect(ui->actionRemove_Frame, &QAction::triggered, mCommands, &ActionCommands::removeKey);
    connect(ui->actionAdd_Frame_Exposure, &QAction::triggered, mCommands, &ActionCommands::addExposureToSelectedFrames);
    connect(ui->actionSubtract_Frame_Exposure, &QAction::triggered, mCommands, &ActionCommands::subtractExposureFromSelectedFrames);
    connect(ui->actionNext_Frame, &QAction::triggered, mCommands, &ActionCommands::GotoNextFrame);
    connect(ui->actionPrevious_Frame, &QAction::triggered, mCommands, &ActionCommands::GotoPrevFrame);
    connect(ui->actionNext_KeyFrame, &QAction::triggered, mCommands, &ActionCommands::GotoNextKeyFrame);
    connect(ui->actionPrev_KeyFrame, &QAction::triggered, mCommands, &ActionCommands::GotoPrevKeyFrame);
    connect(ui->actionMove_Frame_Forward, &QAction::triggered, mCommands, &ActionCommands::moveFrameForward);
    connect(ui->actionMove_Frame_Backward, &QAction::triggered, mCommands, &ActionCommands::moveFrameBackward);

    connect(ui->actionReverse_Frames_Order, &QAction::triggered, mCommands, &ActionCommands::reverseSelectedFrames);
    connect(ui->actionRemove_Frames, &QAction::triggered, mCommands, &ActionCommands::removeSelectedFrames);


    mToolsActionGroup = new QActionGroup(this);
    mToolsActionGroup->setExclusive(true);
    mToolsActionGroup->addAction(ui->actionMove);
    mToolsActionGroup->addAction(ui->actionSelect);
    mToolsActionGroup->addAction(ui->actionPolyline);
    mToolsActionGroup->addAction(ui->actionPen);
    mToolsActionGroup->addAction(ui->actionHand);
    mToolsActionGroup->addAction(ui->actionBucket);
    mToolsActionGroup->addAction(ui->actionEyedropper);
    mToolsActionGroup->addAction(ui->actionEraser);
    // Dplate action will be created below and added to toolbar; also add to group
    if (!mActionDplate) mActionDplate = new QAction(QIcon(":/CustomToolIcons/Dplate.svg"), tr("Dplate"), this);
    mToolsActionGroup->addAction(mActionDplate);

    connect(mToolsActionGroup, &QActionGroup::triggered, this, [&](QAction* action) {
        if (action == ui->actionMove) mToolBox->setActiveTool(MOVE);
        else if (action == ui->actionSelect) mToolBox->setActiveTool(SELECT);
        else if (action == ui->actionPolyline) mToolBox->setActiveTool(POLYLINE);
        else if (action == ui->actionPen) mToolBox->setActiveTool(PEN);
        else if (action == ui->actionHand) mToolBox->setActiveTool(HAND);
        else if (action == ui->actionBucket) mToolBox->setActiveTool(BUCKET);
        else if (action == ui->actionEyedropper) mToolBox->setActiveTool(EYEDROPPER);
        else if (action == ui->actionEraser) mToolBox->setActiveTool(ERASER);
        else if (action == mActionDplate) mToolBox->setActiveTool(DPLATE);
        else if (action == mActionLassoSelect) mEditor->tools()->setCurrentTool(LASSO_SELECT);
        else Q_UNREACHABLE();
    });

    //--- Window Menu (now "Toolbars") ---
    // Toolbar toggle actions are added dynamically in createToolbars().
    // No dock toggles, no Reset/Lock Windows needed.

    // Create global actions for shortcut-only commands that may not have a menu item.
    // They must be registered on the main window via addAction() for shortcuts to work.
    if (!mToggleGapCheckAction) {
        mToggleGapCheckAction = new QAction(tr("Toggle Gap Checking"), this);
        mToggleGapCheckAction->setMenuRole(QAction::NoRole);
        mToggleGapCheckAction->setCheckable(true);
        bindPreferenceSetting(mToggleGapCheckAction, prefs, SETTING::GAP_CHECK_MODE);
        addAction(mToggleGapCheckAction);
    }
    if (!mIncreaseSizeAction) {
        mIncreaseSizeAction = new QAction(tr("Increase Size"), this);
        mIncreaseSizeAction->setMenuRole(QAction::NoRole);
        addAction(mIncreaseSizeAction);
        connect(mIncreaseSizeAction, &QAction::triggered, this, [this]() {
            if (!mEditor || !mEditor->tools() || !mEditor->tools()->currentTool()) return;
            BaseTool* tool = mEditor->tools()->currentTool();
            if (!tool->isPropertyEnabled(WIDTH)) return;
            const qreal cur = (tool->properties.width > 0.0) ? tool->properties.width : 1.0;
            const qreal next = qMax<qreal>(1.0, cur + 1.0);
            ToolManager* tm = mEditor->tools();
            tm->beginEditingPropertiesForTool(tool->type());
            tm->setWidth(static_cast<float>(next));
            tm->endEditingProperties();
            // Mirror size between Pen and Pencil (they share the same width)
            if (tool->type() == PEN || tool->type() == PENCIL) {
                const ToolType other = (tool->type() == PEN) ? PENCIL : PEN;
                BaseTool* otherTool = tm->getTool(other);
                if (otherTool) {
                    tm->beginEditingPropertiesForTool(other);
                    tm->setWidth(static_cast<float>(next));
                    tm->endEditingProperties();
                }
            }
            if (mToolOptions) mToolOptions->updateUI();
        });
    }
    if (!mDecreaseSizeAction) {
        mDecreaseSizeAction = new QAction(tr("Decrease Size"), this);
        mDecreaseSizeAction->setMenuRole(QAction::NoRole);
        addAction(mDecreaseSizeAction);
        connect(mDecreaseSizeAction, &QAction::triggered, this, [this]() {
            if (!mEditor || !mEditor->tools() || !mEditor->tools()->currentTool()) return;
            BaseTool* tool = mEditor->tools()->currentTool();
            if (!tool->isPropertyEnabled(WIDTH)) return;
            const qreal cur = (tool->properties.width > 0.0) ? tool->properties.width : 1.0;
            const qreal next = qMax<qreal>(1.0, cur - 1.0);
            ToolManager* tm = mEditor->tools();
            tm->beginEditingPropertiesForTool(tool->type());
            tm->setWidth(static_cast<float>(next));
            tm->endEditingProperties();
            // Mirror size between Pen and Pencil (they share the same width)
            if (tool->type() == PEN || tool->type() == PENCIL) {
                const ToolType other = (tool->type() == PEN) ? PENCIL : PEN;
                BaseTool* otherTool = tm->getTool(other);
                if (otherTool) {
                    tm->beginEditingPropertiesForTool(other);
                    tm->setWidth(static_cast<float>(next));
                    tm->endEditingProperties();
                }
            }
            if (mToolOptions) mToolOptions->updateUI();
        });
    }

    // Increase / Decrease Opacity
    // - Pencil tool (and Curve/BSpline/Polyline when using Pencil pipeline): per-preset opacity
    //   stored in PencilBrushFactoryOpacityById/{presetId}/OpacityPercent, 0-100 integer.
    // - Pen tool ("Final Line"): no opacity concept — silently ignored.
    // - Eraser: no opacity — silently ignored.
    auto adjustOpacity = [this](int delta) {
        if (!mEditor || !mEditor->tools()) return;
        BaseTool* tool = mEditor->tools()->currentTool();
        if (!tool) return;
        const ToolType tt = tool->type();
        // Only Pencil and the curve-family tools (which use the Pencil pipeline) have opacity.
        // Pen ("Final Line") and Eraser do not — skip silently.
        const bool hasPencilOpacity = (tt == PENCIL || tt == CURVE || tt == BSPLINE || tt == POLYLINE);
        if (!hasPencilOpacity) return;

        auto* pencil = qobject_cast<PencilTool*>(mEditor->tools()->getTool(PENCIL));
        if (!pencil) return;

        // Determine current opacity: prefer per-preset value from QSettings.
        const QString presetId = [&]() -> QString {
            if (BrushPresetStore::instance()) {
                BrushPresetStore::instance()->ensureLoaded();
                const QString id = BrushPresetStore::instance()->activePresetId();
                if (!id.isEmpty()) return id;
            }
            QSettings s(PENCIL2D, PENCIL2D);
            s.beginGroup("PencilBrushFactory");
            return s.value("ActivePresetId", QString()).toString();
        }();

        int curPct = 100;
        if (!presetId.isEmpty()) {
            QSettings s(PENCIL2D, PENCIL2D);
            s.beginGroup("PencilBrushFactoryOpacityById");
            s.beginGroup(presetId);
            curPct = qBound(0, s.value("OpacityPercent", 100).toInt(), 100);
        } else {
            curPct = qBound(0, int(qRound(pencil->getPencilOpacity() * 100.0)), 100);
        }

        const int nextPct = qBound(0, curPct + delta, 100);
        if (nextPct == curPct) return;

        // Apply to tool
        pencil->setPencilOpacity(nextPct / 100.0);

        // Persist per-preset if a preset is active
        if (!presetId.isEmpty()) {
            QSettings s(PENCIL2D, PENCIL2D);
            s.beginGroup("PencilBrushFactoryOpacityById");
            s.beginGroup(presetId);
            s.setValue("OpacityPercent", nextPct);
            s.endGroup();
            s.endGroup();
            s.sync();
        }

        // Sync UI
        if (mToolOptions) mToolOptions->updatePencilOpacityField();
    };
    if (!mIncreaseOpacityAction) {
        mIncreaseOpacityAction = new QAction(tr("Increase Opacity"), this);
        mIncreaseOpacityAction->setMenuRole(QAction::NoRole);
        addAction(mIncreaseOpacityAction);
        connect(mIncreaseOpacityAction, &QAction::triggered, this, [adjustOpacity]() { adjustOpacity(+1); });
    }
    if (!mDecreaseOpacityAction) {
        mDecreaseOpacityAction = new QAction(tr("Decrease Opacity"), this);
        mDecreaseOpacityAction->setMenuRole(QAction::NoRole);
        addAction(mDecreaseOpacityAction);
        connect(mDecreaseOpacityAction, &QAction::triggered, this, [adjustOpacity]() { adjustOpacity(-1); });
    }

    // Increase / Decrease Smooth — delegate entirely to the visible smooth field in
    // ToolOptionWidget. The field's existing valueChanged connection handles the
    // tool update, UI sync, and mUnifiedSmoothUiValue bookkeeping for free.
    if (!mIncreaseSmoothAction) {
        mIncreaseSmoothAction = new QAction(tr("Increase Smooth"), this);
        mIncreaseSmoothAction->setMenuRole(QAction::NoRole);
        addAction(mIncreaseSmoothAction);
        connect(mIncreaseSmoothAction, &QAction::triggered, this, [this]() {
            if (mToolOptions) mToolOptions->stepSmoothValue(+1);
        });
    }
    if (!mDecreaseSmoothAction) {
        mDecreaseSmoothAction = new QAction(tr("Decrease Smooth"), this);
        mDecreaseSmoothAction->setMenuRole(QAction::NoRole);
        addAction(mDecreaseSmoothAction);
        connect(mDecreaseSmoothAction, &QAction::triggered, this, [this]() {
            if (mToolOptions) mToolOptions->stepSmoothValue(-1);
        });
    }

    // Toolbar visibility toggles — created here (before setupKeyboardShortcuts)
    // so their shortcuts are applied correctly on first launch.
    // buildWindowsMenuWindow() references these members; it runs after this.
    if (!mCCToggleFileOpsAction) {
        mCCToggleFileOpsAction = new QAction(tr("File Operations"), this);
        mCCToggleFileOpsAction->setCheckable(true);
        mCCToggleFileOpsAction->setChecked(true);
        addAction(mCCToggleFileOpsAction);
    }
    if (!mCCToggleViewAction) {
        mCCToggleViewAction = new QAction(tr("View Controls"), this);
        mCCToggleViewAction->setCheckable(true);
        mCCToggleViewAction->setChecked(true);
        addAction(mCCToggleViewAction);
    }
    if (!mCCToggleOverlaysAction) {
        mCCToggleOverlaysAction = new QAction(tr("Overlays"), this);
        mCCToggleOverlaysAction->setCheckable(true);
        mCCToggleOverlaysAction->setChecked(true);
        addAction(mCCToggleOverlaysAction);
    }
    if (!mCCToggleUndoRedoAction) {
        mCCToggleUndoRedoAction = new QAction(tr("Undo / Redo"), this);
        mCCToggleUndoRedoAction->setCheckable(true);
        mCCToggleUndoRedoAction->setChecked(true);
        addAction(mCCToggleUndoRedoAction);
    }
    if (!mCCToggleClearFrameAction) {
        mCCToggleClearFrameAction = new QAction(tr("Clear Frame"), this);
        mCCToggleClearFrameAction->setCheckable(true);
        mCCToggleClearFrameAction->setChecked(true);
        addAction(mCCToggleClearFrameAction);
    }

    // About action (menu hidden, connection kept for any other triggers)
    connect(ui->actionAbout, &QAction::triggered, mCommands, &ActionCommands::about);
    if (ui->menuAbout) { ui->menuAbout->menuAction()->setVisible(false); }

    //--- Menus ---
    mRecentFileMenu = new RecentFileMenu(tr("Open Recent"), this);
    mRecentFileMenu->loadFromDisk();
    ui->menuFile->insertMenu(ui->actionSave, mRecentFileMenu);

    connect(mRecentFileMenu, &RecentFileMenu::loadRecentFile, this, &MainWindow2::openFile);

    // --- Canvas Menu ---
    QMenu* canvasMenu = new QMenu(tr("Canvas"), this);
    QAction* setCanvasSizeAct = new QAction(tr("Set Canvas Size…"), this);
    connect(setCanvasSizeAct, &QAction::triggered, this, &MainWindow2::openCanvasSizeDialog);
    canvasMenu->addAction(setCanvasSizeAct);
    // Show Canvas Color Palette toggle (persisted)
    mActionShowCanvasPalette = new QAction(tr("Show Canvas Color Palette"), this);
    mActionShowCanvasPalette->setCheckable(true);
    canvasMenu->addAction(mActionShowCanvasPalette);

    QWidget* canvasColorWidget = new QWidget(this);
    QHBoxLayout* ccLayout = new QHBoxLayout(canvasColorWidget);
    ccLayout->setContentsMargins(8, 4, 8, 4);
    QLabel* ccLabel = new QLabel(tr("Canvas color"), canvasColorWidget);
    QPushButton* canvasColorButton = new QPushButton(canvasColorWidget);
    canvasColorButton->setFixedSize(28, 18);
    ccLayout->addWidget(ccLabel);
    ccLayout->addStretch(1);
    ccLayout->addWidget(canvasColorButton);
    QWidgetAction* ccAction = new QWidgetAction(this);
    ccAction->setDefaultWidget(canvasColorWidget);
    canvasMenu->addAction(ccAction);

    QWidget* extraSpaceWidget = new QWidget(this);
    QVBoxLayout* esVLayout = new QVBoxLayout(extraSpaceWidget);
    esVLayout->setContentsMargins(8, 6, 8, 6);
    esVLayout->setSpacing(6);

    QWidget* esStyleRow = new QWidget(extraSpaceWidget);
    QHBoxLayout* esStyleLayout = new QHBoxLayout(esStyleRow);
    esStyleLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* esStyleLabel = new QLabel(tr("Extra Space Style"), esStyleRow);
    CosmicDropDown* esStyleDropdown = new CosmicDropDown(esStyleRow);
    esStyleDropdown->setOptions(QStringList() << tr("Auto") << tr("Manual"));
    esStyleLayout->addWidget(esStyleLabel);
    esStyleLayout->addStretch(1);
    esStyleLayout->addWidget(esStyleDropdown);
    esVLayout->addWidget(esStyleRow);

    QWidget* esAutoRow = new QWidget(extraSpaceWidget);
    QHBoxLayout* esAutoLayout = new QHBoxLayout(esAutoRow);
    esAutoLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* esAutoLabel = new QLabel(tr("Scale"), esAutoRow);
    CosmicDropDown* esScaleDropdown = new CosmicDropDown(esAutoRow);
    QStringList scaleOptions;
    QList<int> scalePercents;
    for (int p = 100; p <= 200; p += 10) {
        const qreal v = static_cast<qreal>(p) / 100.0;
        QString txt;
        if (p == 100 || p == 200) {
            txt = QString::number(v, 'f', 0) + QStringLiteral("x");
        } else {
            txt = QString::number(v, 'f', 1) + QStringLiteral("x");
        }
        scaleOptions << txt;
        scalePercents << p;
    }
    esScaleDropdown->setOptions(scaleOptions);
    esAutoLayout->addWidget(esAutoLabel);
    esAutoLayout->addStretch(1);
    esAutoLayout->addWidget(esScaleDropdown);
    esVLayout->addWidget(esAutoRow);

    QWidget* esManualRow = new QWidget(extraSpaceWidget);
    QHBoxLayout* esManualLayout = new QHBoxLayout(esManualRow);
    esManualLayout->setContentsMargins(0, 0, 0, 0);
    esManualLayout->setSpacing(6);
    QLabel* esWLabel = new QLabel(tr("X"), esManualRow);
    CosmicInputField* esWInput = new CosmicInputField(esManualRow);
    esWInput->setVariant(CosmicInputField::Minimal);
    esWInput->setDecimals(0);
    esWInput->setSingleStep(1.0);
    QLabel* esHLabel = new QLabel(tr("Y"), esManualRow);
    CosmicInputField* esHInput = new CosmicInputField(esManualRow);
    esHInput->setVariant(CosmicInputField::Minimal);
    esHInput->setDecimals(0);
    esHInput->setSingleStep(1.0);
    esManualLayout->addWidget(esWLabel);
    esManualLayout->addWidget(esWInput);
    esManualLayout->addWidget(esHLabel);
    esManualLayout->addWidget(esHInput);
    esVLayout->addWidget(esManualRow);

    QWidgetAction* esAction = new QWidgetAction(this);
    esAction->setDefaultWidget(extraSpaceWidget);
    canvasMenu->addAction(esAction);

    auto updateManualRanges = [prefs, esWInput, esHInput]() {
        const int baseW = qMax(1, prefs->getInt(SETTING::FIELD_W));
        const int baseH = qMax(1, prefs->getInt(SETTING::FIELD_H));
        esWInput->setRange(baseW, baseW * 2);
        esHInput->setRange(baseH, baseH * 2);
    };

    auto updateCanvasColorButton = [prefs, canvasColorButton]() {
        QColor c = QColor::fromRgba(prefs->getInt(SETTING::CANVAS_COLOR));
        if (!c.isValid()) c = Qt::white;
        const QString css = QString("QPushButton { background-color:%1; border: 1px solid #666; }")
                                .arg(c.name(QColor::HexArgb));
        canvasColorButton->setStyleSheet(css);
    };

    auto updateExtraSpaceUi = [prefs, esStyleDropdown, esScaleDropdown, esAutoRow, esManualRow, esWInput, esHInput]() {
        int style = prefs->getInt(SETTING::EXTRA_SPACE_STYLE);
        if (style != 0 && style != 1) style = 0;
        esStyleDropdown->setCurrentIndex(style);
        esAutoRow->setVisible(style == 0);
        esManualRow->setVisible(style == 1);

        const int baseW = qMax(1, prefs->getInt(SETTING::FIELD_W));
        const int baseH = qMax(1, prefs->getInt(SETTING::FIELD_H));

        const int scale = qBound(100, prefs->getInt(SETTING::EXTRA_SPACE_SCALE_PERCENT), 200);
        int index = (scale - 100) / 10;
        index = qBound(0, index, 10);
        esScaleDropdown->setCurrentIndex(index);

        int w = prefs->getInt(SETTING::EXTRA_SPACE_MANUAL_W);
        int h = prefs->getInt(SETTING::EXTRA_SPACE_MANUAL_H);
        w = qBound(baseW, w, baseW * 2);
        h = qBound(baseH, h, baseH * 2);
        esWInput->setValue(w);
        esHInput->setValue(h);
    };

    updateManualRanges();
    updateCanvasColorButton();
    updateExtraSpaceUi();

    QTimer* canvasColorRealtimeTimer = new QTimer(canvasMenu);
    canvasColorRealtimeTimer->setSingleShot(true);
    canvasColorRealtimeTimer->setInterval(16);
    canvasColorRealtimeTimer->setProperty("pendingCanvasColorRgba", QVariant());
    connect(canvasColorRealtimeTimer, &QTimer::timeout, this, [prefs, canvasColorRealtimeTimer]() {
        const QVariant v = canvasColorRealtimeTimer->property("pendingCanvasColorRgba");
        if (!v.isValid()) {
            return;
        }
        const int rgba = v.toInt();
        prefs->set(SETTING::CANVAS_COLOR, rgba);
    });

    connect(canvasColorButton, &QPushButton::clicked, this, [this, prefs, canvasColorButton, canvasColorRealtimeTimer]() {
        QColor initial = QColor::fromRgba(prefs->getInt(SETTING::CANVAS_COLOR));
        if (!initial.isValid()) initial = Qt::white;
        auto* popup = new PopupColorWheelWidget(this);
        popup->setPreferenceManager(prefs);
        popup->setColor(initial);

        connect(popup, &PopupColorWheelWidget::colorChanged, this, [prefs, canvasColorButton](const QColor& c) {
            if (!c.isValid()) return;
            const QString css = QString("QPushButton { background-color:%1; border: 1px solid #666; }")
                                    .arg(c.name(QColor::HexArgb));
            canvasColorButton->setStyleSheet(css);
        });

        connect(popup, &PopupColorWheelWidget::colorChanged, this, [prefs, canvasColorRealtimeTimer](const QColor& c) {
            if (!c.isValid()) return;
            canvasColorRealtimeTimer->setProperty("pendingCanvasColorRgba", static_cast<int>(c.rgba()));
            canvasColorRealtimeTimer->start();
        });

        connect(popup, &PopupColorWheelWidget::colorSelected, this, [prefs, canvasColorButton, canvasColorRealtimeTimer](const QColor& c) {
            if (!c.isValid()) return;
            canvasColorRealtimeTimer->stop();
            canvasColorRealtimeTimer->setProperty("pendingCanvasColorRgba", QVariant());
            prefs->set(SETTING::CANVAS_COLOR, static_cast<int>(c.rgba()));
            const QString css = QString("QPushButton { background-color:%1; border: 1px solid #666; }")
                                    .arg(c.name(QColor::HexArgb));
            canvasColorButton->setStyleSheet(css);
        });

        popup->showAt(QCursor::pos());
    });

    connect(esStyleDropdown, &CosmicDropDown::currentIndexChanged, this, [prefs, esAutoRow, esManualRow, updateManualRanges](int idx) {
        const int style = (idx == 1) ? 1 : 0;
        prefs->set(SETTING::EXTRA_SPACE_STYLE, style);
        esAutoRow->setVisible(style == 0);
        esManualRow->setVisible(style == 1);
        updateManualRanges();
    });

    connect(esScaleDropdown, &CosmicDropDown::currentIndexChanged, this, [prefs, scalePercents](int idx) {
        const int safeIdx = qBound(0, idx, scalePercents.size() - 1);
        prefs->set(SETTING::EXTRA_SPACE_STYLE, 0);
        prefs->set(SETTING::EXTRA_SPACE_SCALE_PERCENT, scalePercents[safeIdx]);
    });

    connect(esWInput, &CosmicInputField::valueCommitted, this, [prefs](double v) {
        const int baseW = qMax(1, prefs->getInt(SETTING::FIELD_W));
        const int w = qBound(baseW, static_cast<int>(qRound(v)), baseW * 2);
        prefs->set(SETTING::EXTRA_SPACE_STYLE, 1);
        prefs->set(SETTING::EXTRA_SPACE_MANUAL_W, w);
    });
    connect(esHInput, &CosmicInputField::valueCommitted, this, [prefs](double v) {
        const int baseH = qMax(1, prefs->getInt(SETTING::FIELD_H));
        const int h = qBound(baseH, static_cast<int>(qRound(v)), baseH * 2);
        prefs->set(SETTING::EXTRA_SPACE_STYLE, 1);
        prefs->set(SETTING::EXTRA_SPACE_MANUAL_H, h);
    });

    connect(prefs, &PreferenceManager::optionChanged, this, [=](SETTING s) {
        if (s == SETTING::CANVAS_COLOR) {
            updateCanvasColorButton();
        } else if (s == SETTING::ACCENT_COLOR) {
            const QColor accent = BlenderStyleSlider::getAccentColor();
            esWInput->setAccentColor(accent);
            esHInput->setAccentColor(accent);
        } else if (s == SETTING::EXTRA_SPACE_STYLE || s == SETTING::EXTRA_SPACE_SCALE_PERCENT ||
                   s == SETTING::EXTRA_SPACE_MANUAL_W || s == SETTING::EXTRA_SPACE_MANUAL_H ||
                   s == SETTING::FIELD_W || s == SETTING::FIELD_H) {
            updateManualRanges();
            updateExtraSpaceUi();
        }
    });

    {
        const QColor accent = BlenderStyleSlider::getAccentColor();
        esWInput->setAccentColor(accent);
        esHInput->setAccentColor(accent);
    }

    // Free Transform action (Selection)
    // Free Transform intentionally not listed in Canvas menu

    ui->menuBar->insertMenu(ui->menuLayer->menuAction(), canvasMenu);

    // Restore persisted value (default true) and connect
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        const bool visible = settings.value("Canvas/ShowPaletteOverlay", true).toBool();
        mActionShowCanvasPalette->setChecked(visible);
        if (mCanvasPaletteOverlay) mCanvasPaletteOverlay->setVisible(visible);
    }
    connect(mActionShowCanvasPalette, &QAction::toggled, this, [this](bool on){
        if (mCanvasPaletteOverlay) mCanvasPaletteOverlay->setVisible(on);
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("Canvas/ShowPaletteOverlay", on);
        settings.sync();
    });

    connect(mRecentFileMenu, &RecentFileMenu::loadRecentFile, this, &MainWindow2::openFile);

    // Hide icons in menus while keeping them in toolbars for a more professional appearance
    QList<QMenu*> allMenus = ui->menuBar->findChildren<QMenu*>();
    allMenus.append(ui->menuFile);
    allMenus.append(ui->menuEdit);
    allMenus.append(ui->menuView);
    allMenus.append(ui->menuLayer);
    allMenus.append(ui->menuAnimation);
    allMenus.append(ui->menuWindows);
    allMenus.append(ui->menuAbout);
    allMenus.append(canvasMenu);
    
    for (QMenu* menu : allMenus) {
        if (!menu) continue;
        for (QAction* action : menu->actions()) {
            if (action && !action->isSeparator()) {
                action->setIconVisibleInMenu(false);
            }
        }
    }
}

void MainWindow2::replaceUndoRedoActions()
{
    // Preserve existing actions' icons and shortcuts
    QAction* oldUndo = ui->actionUndo;
    QAction* oldRedo = ui->actionRedo;
    const QIcon oldUndoIcon = oldUndo ? oldUndo->icon() : QIcon();
    const QIcon oldRedoIcon = oldRedo ? oldRedo->icon() : QIcon();
    const QList<QKeySequence> oldUndoShortcuts = oldUndo ? oldUndo->shortcuts() : QList<QKeySequence>();
    const QList<QKeySequence> oldRedoShortcuts = oldRedo ? oldRedo->shortcuts() : QList<QKeySequence>();

    // Replace in Edit menu
    if (oldUndo) ui->menuEdit->removeAction(oldUndo);
    if (oldRedo) ui->menuEdit->removeAction(oldRedo);
    ui->actionUndo = mEditor->undoRedo()->createUndoAction(this, oldUndoIcon);
    ui->actionRedo = mEditor->undoRedo()->createRedoAction(this, oldRedoIcon);
    if (!oldUndoShortcuts.isEmpty()) ui->actionUndo->setShortcuts(oldUndoShortcuts);
    if (!oldRedoShortcuts.isEmpty()) ui->actionRedo->setShortcuts(oldRedoShortcuts);
    ui->menuEdit->insertAction(ui->actionCut, ui->actionUndo);
    ui->menuEdit->insertAction(ui->actionCut, ui->actionRedo);
    // Hide icons in menu while keeping them in toolbar
    ui->actionUndo->setIconVisibleInMenu(false);
    ui->actionRedo->setIconVisibleInMenu(false);

    // Also update any toolbars that contained the old actions
    const QList<QToolBar*> toolbars = findChildren<QToolBar*>();
    for (QToolBar* tb : toolbars) {
        if (!tb) continue;
        // Track whether toolbar had old actions
        bool hadUndo = false, hadRedo = false;
        for (QAction* a : tb->actions()) {
            if (a == oldUndo) { hadUndo = true; }
            if (a == oldRedo) { hadRedo = true; }
        }
        if (hadUndo && oldUndo) {
            tb->removeAction(oldUndo);
            tb->addAction(ui->actionUndo);
        }
        if (hadRedo && oldRedo) {
            tb->removeAction(oldRedo);
            tb->addAction(ui->actionRedo);
        }
    }

    // Refresh the Cosmic Edit menu so it holds the new undo/redo action pointers.
    // The old actions are destroyed after this function returns, so we must
    // re-insert the new ones before that happens.
    if (mCosmicEditMenu) {
        // Remove old items (they will auto-remove via QAction::destroyed signal,
        // but we need the new ones in place immediately for shortcuts to work).
        // Rebuild the top two items: clear and re-add undo/redo at the front.
        // We do a targeted replace: remove old, insert new at position 0 and 1.
        mCosmicEditMenu->replaceUndoRedo(ui->actionUndo, ui->actionRedo);
    }

    // Re-register new undo/redo actions on the window so shortcuts keep working
    // after the native menubar is hidden.
    // IMPORTANT: remove old actions first to prevent shortcut ambiguity accumulation.
    if (oldUndo) removeAction(oldUndo);
    if (oldRedo) removeAction(oldRedo);
    if (ui->actionUndo) {
        ui->actionUndo->setShortcutContext(Qt::WindowShortcut);
        addAction(ui->actionUndo);
    }
    if (ui->actionRedo) {
        ui->actionRedo->setShortcutContext(Qt::WindowShortcut);
        addAction(ui->actionRedo);
    }

    // Delete old actions now that they've been fully replaced everywhere.
    // They were parented to 'this' so Qt would eventually clean them up,
    // but deleting them immediately prevents dangling shortcut registrations.
    if (oldUndo) oldUndo->deleteLater();
    if (oldRedo) oldRedo->deleteLater();
}

void MainWindow2::setOpacity(int opacity)
{
    mEditor->preference()->set(SETTING::WINDOW_OPACITY, 100 - opacity);
    setWindowOpacity(opacity / 100.0);
}

void MainWindow2::updateSaveState()
{
    const bool hasUnsavedChanges = mEditor->undoRedo()->hasUnsavedChanges();
    setWindowModified(hasUnsavedChanges);
}

void MainWindow2::updateBackupActionState()
{
    mEditor->undoRedo()->updateUndoAction(ui->actionUndo);
    mEditor->undoRedo()->updateRedoAction(ui->actionRedo);
}

void MainWindow2::openPegAlignDialog()
{
    if (mPegAlign != nullptr)
    {
        QMessageBox::information(this, nullptr,
                                 tr("Dialog is already open!"),
                                 QMessageBox::Ok);
        return;
    }

    mPegAlign = new PegBarAlignmentDialog(mEditor, this);
    mPegAlign->setAttribute(Qt::WA_DeleteOnClose);

    Qt::WindowFlags flags = mPegAlign->windowFlags();
    flags |= Qt::WindowStaysOnTopHint;
    flags &= (~Qt::WindowContextHelpButtonHint);
    mPegAlign->setWindowFlags(flags);
    mPegAlign->show();
    connect(mPegAlign, &PegBarAlignmentDialog::finished, [=]
    {
        mPegAlign = nullptr;
    });
}

void MainWindow2::openLayerOpacityDialog()
{
    if (mLayerOpacityDialog != nullptr)
    {
        mLayerOpacityDialog->raise();
        mLayerOpacityDialog->activateWindow();
        return;
    }
    auto* popup = new CosmicLayerOpacityPopup(this);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setCore(mEditor);
    popup->initUI();
    popup->setWindowFlags(popup->windowFlags() | Qt::WindowStaysOnTopHint);
    popup->show();
    mLayerOpacityDialog = popup;

    connect(popup, &CosmicLayerOpacityPopup::finished, [=]
    {
        mLayerOpacityDialog = nullptr;
    });
}

void MainWindow2::openAddTranspToPaperDialog()
{
    if (mAddTranspToPaper)
    {
        mAddTranspToPaper->activateWindow();
        mAddTranspToPaper->raise();
        return;
    }

    mAddTranspToPaper = new AddTransparencyToPaperDialog(this);
    mAddTranspToPaper->setCore(mEditor);
    mAddTranspToPaper->initUI();
    mAddTranspToPaper->setWindowFlag(Qt::WindowStaysOnTopHint);
    mAddTranspToPaper->show();

    connect(mAddTranspToPaper, &AddTransparencyToPaperDialog::finished, [=](int result)
    {
        if (result == QDialog::Accepted)
        {
            mSuppressAutoSaveDialog = true;
            mAddTranspToPaper->traceScannedDrawings();
            mSuppressAutoSaveDialog = false;
        }
        mAddTranspToPaper->deleteLater();
        mAddTranspToPaper = nullptr;
    });
}

void MainWindow2::openRepositionDialog()
{
    if (mEditor->layers()->currentLayer()->getSelectedFramesByPos().count() < 2)
    {
        QMessageBox::information(this, nullptr,
                                 tr("Please select at least 2 frames!"),
                                 QMessageBox::Ok);
        return;
    }
    if (mReposDialog != nullptr)
    {
        return;
    }

    mReposDialog = new RepositionFramesDialog(this);
    mReposDialog->setAttribute(Qt::WA_DeleteOnClose);
    mReposDialog->setWindowFlags(mReposDialog->windowFlags() | Qt::WindowStaysOnTopHint);
    hideQuestionMark(*mReposDialog);
    mReposDialog->setCore(mEditor);
    mReposDialog->initUI();
    mEditor->tools()->setCurrentTool(ToolType::MOVE);
    connect(mReposDialog, &RepositionFramesDialog::finished, this, &MainWindow2::closeRepositionDialog);
    mReposDialog->show();
}

void MainWindow2::closeRepositionDialog()
{
    selectionChanged();
    mReposDialog = nullptr;
}

void MainWindow2::currentLayerChanged()
{
    bool isBitmap = (mEditor->layers()->currentLayer()->type() == Layer::BITMAP);
    ui->menuChange_line_color->setEnabled(isBitmap);

    // UI removed: keep actions hidden/disabled regardless of layer type.
}

void MainWindow2::selectionChanged()
{
    auto sel = mEditor->select();
    // Restrict when: lasso is being drawn (CREATING), lasso selection is finalized (SELECTED),
    // or there is floating content. This prevents layer switching in the timeline and papers
    // pane during any phase of a lasso operation.
    bool finalized = false;
    if (sel) {
        const bool floating = sel->hasFloating() || sel->needsFloatingCommit();
        const bool lassoBuilding = sel->isFromLasso()
                                    && (sel->state() == SelectionManager::SelectionState::CREATING);
        const bool lassoFinal = sel->isFromLasso()
                                 && sel->somethingSelected()
                                 && (sel->state() == SelectionManager::SelectionState::SELECTED);
        finalized = floating || lassoBuilding || lassoFinal;
    }

    // While a floating selection (temporary buffer) is active or awaiting
    // commit, disable global undo/redo actions. This prevents the per-keyframe
    // undo stack from diverging from the floating buffer state. Tool-local
    // handlers (like lasso's Ctrl+Z drag-cancel) still receive raw key events
    // via ScribbleArea and are not affected by this UI-level disable.
    const bool hasFloating = sel && (sel->hasFloating() || sel->needsFloatingCommit());
    if (ui->actionUndo) ui->actionUndo->setEnabled(!hasFloating);
    if (ui->actionRedo) ui->actionRedo->setEnabled(!hasFloating);
    ui->menuSelection->setEnabled(finalized);
    // Activate/deactivate restriction only when state changes
    if (finalized != mLassoRestrictionActive && !mApplyingLassoRestriction) {
        applyLassoRestriction(finalized);
    }
}

bool MainWindow2::hasFloatingSelection() const
{
    if (!mEditor) return false;
    auto sel = mEditor->select();
    return sel && (sel->hasFloating() || sel->needsFloatingCommit());
}

void MainWindow2::viewFlipped()
{
    ui->actionHorizontal_Flip->setChecked(mEditor->view()->isFlipHorizontal());
    ui->actionVertical_Flip->setChecked(mEditor->view()->isFlipVertical());
}

void MainWindow2::closeEvent(QCloseEvent* event)
{
    if (m2ndCloseEvent)
    {
        // https://bugreports.qt.io/browse/QTBUG-43344
        event->accept();
        return;
    }

    if (!maybeSave(true))
    {
        event->ignore();
        return;
    }
    writeSettings();
    // Flush any pending deferred preset saves before the process exits.
    // The 150ms timer may not have fired if the user acted quickly.
    BrushPresetStore::instance()->saveToSettings();
    event->accept();
    m2ndCloseEvent = true;
}

void MainWindow2::tabletEvent(QTabletEvent* event)
{
    event->ignore();
}

void MainWindow2::newDocument()
{
    if (maybeSave() && !tryLoadPreset())
    {
        newObject();
    }
}

void MainWindow2::openDocument()
{
    if (!maybeSave())
    {
        return;
    }
    QString fileName = FileDialog::getOpenFileName(this, FileType::ANIMATION);
    if (!fileName.isEmpty())
    {
        openObject(fileName);
    }
}

bool MainWindow2::saveAsNewDocument()
{
    QString fileName = FileDialog::getSaveFileName(this, FileType::ANIMATION);
    if (fileName.isEmpty())
    {
        return false;
    }
    return saveObject(fileName);
}

void MainWindow2::openStartupFile(const QString& filename)
{
    if (tryRecoverUnsavedProject())
    {
        return;
    }

    if (!filename.isEmpty() && openObject(filename))
    {
        return;
    }

    loadMostRecent() || tryLoadPreset();
}

void MainWindow2::openFile(const QString& filename)
{
    if (maybeSave())
    {
        openObject(filename);
    }
}

bool MainWindow2::openObject(const QString& strFilePath)
{
    CosmicSaveProgressPopup progress(this);
    progress.setTitle(tr("Opening…"));
    progress.setRange(0, 100);
    progress.showCentered();

    const N2DProjectIO::ProgressCallbacks cb{
        [&progress](int p) {
            progress.setValue(p);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        },
        [&progress](int max) {
            progress.setRange(0, max);
        }
    };

    SceneSequenceManager* ssm = (mTimeLine) ? mTimeLine->sceneSequenceManager() : nullptr;
    Status s = N2DProjectIO::loadProject(strFilePath, mEditor, ssm, cb);

    if (!s.ok())
    {
        ErrorDialog errorDialog(s.title(), s.description(), s.details().str());
        errorDialog.exec();
        emptyDocumentWhenErrorOccurred();
        return false;
    }

    if (ssm) {
        ssm->setProjectFilePath(strFilePath);
    }

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(LAST_PCLX_PATH, strFilePath);

    // Add to recent file list, but only if we are
    if (!strFilePath.isEmpty())
    {
        mRecentFileMenu->addRecentFile(strFilePath);
        mRecentFileMenu->saveToDisk();
    }

    closeDialogs();

    setWindowTitle(QStringLiteral("[*]%1").arg(strFilePath));
    setWindowModified(false);

    progress.setValue(100);

    updateSaveState();
    updateBackupActionState();

    // Ensure the tool and brush preset are in a consistent state after loading.
    // ToolManager::load no longer resets to PEN, but be explicit here so
    // opening a project always lands on Pencil with the correct preset applied.
    if (mEditor && mEditor->tools()) {
        mEditor->tools()->setCurrentTool(PENCIL);
    }
    if (mBrushFactoryDock) {
        auto* store = BrushPresetStore::instance();
        store->ensureLoaded();
        mBrushFactoryDock->setActivePreset(store->activeIndex());
    }

    if (!QFileInfo(strFilePath).isWritable())
    {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This program does not currently have permission to write to the file you have selected. "
                                "Please make sure you have write permission for this file before attempting to save it. "
                                "Alternatively, you can use the Save As... menu option to save to a writable location."),
                             QMessageBox::Ok);
    }

    return true;
}

bool MainWindow2::saveObject(QString strSavedFileName)
{
    CosmicSaveProgressPopup progress(this);
    progress.setTitle(tr("Saving…"));
    progress.setRange(0, 100);
    progress.showCentered();

    // Persist any manager state (fps, view, etc.) before snapshotting.
    mEditor->prepareSave();

    const N2DProjectIO::ProgressCallbacks cb{
        [&progress](int p) {
            progress.setValue(p);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        },
        [&progress](int max) {
            progress.setRange(0, max + 3);
        }
    };

    SceneSequenceManager* ssm = (mTimeLine) ? mTimeLine->sceneSequenceManager() : nullptr;
    Status st(Status::OK);
    if (ssm) {
        st = N2DProjectIO::saveProject(strSavedFileName, mEditor, ssm, cb);
    } else {
        // Fallback: allow saving a single-shot project even when SceneSequenceManager is unavailable.
        // This preserves File->Save behavior for legacy/non-scene workflows.
        if (!mEditor || !mEditor->object()) {
            DebugDetails dd;
            dd << QStringLiteral("Missing editor or object");
            st = Status(Status::INVALID_ARGUMENT, dd);
        } else {
            st = N2DProjectIO::saveSingleShotProject(strSavedFileName, mEditor->object(), cb);
        }
    }


    if (!st.ok())
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
        QDir errorLogFolder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
#else
        QDir errorLogFolder(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
#endif
        errorLogFolder.mkpath("./logs");
        errorLogFolder.cd("logs");

        QDateTime dt = QDateTime::currentDateTime();
        dt.setTimeSpec(Qt::UTC);
        QFile eLog(errorLogFolder.absoluteFilePath(QString("error-%1.txt").arg(dt.toString(Qt::ISODate))));
        if (eLog.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&eLog);
            out << st.details().str();
        }
        eLog.close();

        ErrorDialog errorDialog(st.title(),
                                st.description().append(tr("<br><br>An error has occurred and your file may not have saved successfully."
                                                           "\nIf you believe that this error is an issue with Pencil2D, please create a new issue at:"
                                                           "<br><a href='https://github.com/pencil2d/pencil/issues'>https://github.com/pencil2d/pencil/issues</a><br>"
                                                           "Please be sure to include the following details in your issue:")), st.details().html());
        errorDialog.exec();
        return false;
    }

    if (ssm) {
        ssm->setProjectFilePath(strSavedFileName);
    }

    if (mEditor->object()) {
        mEditor->object()->setFilePath(strSavedFileName);
        mEditor->object()->setModified(false);
    }

    mEditor->clearTemporary();

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(LAST_PCLX_PATH, strSavedFileName);

    mRecentFileMenu->addRecentFile(strSavedFileName);
    mRecentFileMenu->saveToDisk();

    mTimeLine->updateContent();

    setWindowTitle(strSavedFileName.prepend("[*]"));
    updateSaveState();

    progress.setValue(100);

    mEditor->resetAutoSaveCounter();
    mEditor->markDocumentClean();

    return true;
}

bool MainWindow2::saveDocument()
{
    SceneSequenceManager* ssm = (mTimeLine) ? mTimeLine->sceneSequenceManager() : nullptr;
    const QString projectPath = ssm ? ssm->projectFilePath() : QString();
    if (!projectPath.isEmpty()) {
        return saveObject(projectPath);
    }

    if (mEditor->object() && !mEditor->object()->filePath().isEmpty()) {
        return saveObject(mEditor->object()->filePath());
    }

    return saveAsNewDocument();
}

bool MainWindow2::maybeSave(bool isQuitting)
{
    if (!mEditor->undoRedo()->hasUnsavedChanges())
    {
        return true;
    }

    // --- Unsaved Changes popup (3-choice: Cancel / Quit Anyway / Save) ---
    // Uses CosmicPopupWindow directly so we can have three distinct actions.
    // Visual language matches CosmicConfirmPopup: question icon, compact layout,
    // CosmicGreen = Save (default), CosmicRed = Quit Anyway, header close = Cancel.
    class UnsavedChangesPopup final : public CosmicPopupWindow
    {
    public:
        enum Choice { Cancel = 0, Discard = 1, Save = 2 };

        explicit UnsavedChangesPopup(bool isQuitting, QWidget* parent = nullptr)
            : CosmicPopupWindow(parent)
        {
            setModalMode(ModalMode::WindowModal);
            setClosePolicy(ClosePolicy::Manual);
            setResizable(false);
            setDraggable(true);
            setFixedWindowSize(QSize(360, 1));
            setShadowEnabled(false);
            setShadowPadding(0);
            setPanelColor(QColor(0x2f, 0x2f, 0x2f));
            setBorderColor(CosmicColor::popupBorder());
            setBorderWidth(2);

            auto* root = new QWidget(this);
            root->setObjectName(QStringLiteral("UnsavedChangesPopupRoot"));
            root->setStyleSheet(QStringLiteral("QWidget#UnsavedChangesPopupRoot{ background: transparent; border: none; }"));

            auto* lay = new QVBoxLayout(root);
            lay->setContentsMargins(12, 6, 12, 8);
            lay->setSpacing(3);

            // Header with close button (= Cancel)
            mHeader = new CosmicWindowHeader(root);
            lay->addWidget(mHeader);

            // Question icon
            mIcon = new QLabel(root);
            mIcon->setFixedSize(32, 32);
            mIcon->setAlignment(Qt::AlignCenter);
            mIcon->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
            {
                const QIcon qi(QStringLiteral(":/Interaction/Question.svg"));
                mIcon->setPixmap(qi.isNull()
                    ? QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(32, 32)
                    : qi.pixmap(32, 32));
            }
            lay->addWidget(mIcon, 0, Qt::AlignHCenter);

            // Body text
            mBody = new QLabel(tr("This animation has been modified.\nDo you want to save your changes?"), root);
            mBody->setWordWrap(true);
            mBody->setAlignment(Qt::AlignCenter);
            mBody->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
            CosmicText::applyToWidget(mBody);
            lay->addWidget(mBody, 0);

            // Button row: [Quit Anyway] [Save]
            const int btnH = CosmicUiMetrics::controlThickness();
            auto* btnRow = new QHBoxLayout();
            btnRow->setContentsMargins(0, 0, 0, 0);
            btnRow->setSpacing(6);

            const QColor red   = CosmicColor::red();
            const QColor green = CosmicColor::green();

            mQuitAnyway = new CosmicButton(isQuitting ? tr("Quit") : tr("No"), root);
            mQuitAnyway->setBordered(false);
            mQuitAnyway->setBackgroundColors(red, red.lighter(115), red.darker(115));
            mQuitAnyway->setTextColor(QColor(0x2f, 0x2f, 0x2f, 0xff));
            mQuitAnyway->setFixedHeight(btnH);
            mQuitAnyway->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btnRow->addWidget(mQuitAnyway, 1);

            mSave = new CosmicButton(isQuitting ? tr("Save") : tr("Yes"), root);
            mSave->setBordered(false);
            mSave->setBackgroundColors(green, green.lighter(115), green.darker(115));
            mSave->setTextColor(QColor(0x2f, 0x2f, 0x2f, 0xff));
            mSave->setFixedHeight(btnH);
            mSave->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btnRow->addWidget(mSave, 1);

            lay->addLayout(btnRow);

            auto* outer = new QVBoxLayout(this);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->setSpacing(0);
            outer->addWidget(root);

            connect(mHeader,    &CosmicWindowHeader::closeRequested, this, [this]() { mChoice = Cancel;  reject(); });
            connect(mQuitAnyway,&QPushButton::clicked,               this, [this]() { mChoice = Discard; accept(); });
            connect(mSave,      &QPushButton::clicked,               this, [this]() { mChoice = Save;    accept(); });
        }

        Choice choice() const { return mChoice; }

    protected:
        void showEvent(QShowEvent* e) override
        {
            CosmicPopupWindow::showEvent(e);

            clearFixedWindowSize();
            setMinimumWidth(360);
            setMaximumWidth(360);
            adjustSize();

            if (mQuitAnyway)
                setDragRegionHeight(qMax(10, mQuitAnyway->geometry().top()));

            setWindowOpacity(0.0);
            const QRect g0 = geometry();
            const QPoint c = g0.center();
            const QSize  s = g0.size();
            const QSize  s0(qMax(1, int(s.width() * 0.92)), qMax(1, int(s.height() * 0.92)));
            const QRect  g1(QPoint(c.x() - s0.width() / 2, c.y() - s0.height() / 2), s0);
            setGeometry(g1);

            auto* opacity = new QPropertyAnimation(this, "windowOpacity", this);
            opacity->setDuration(80);
            opacity->setStartValue(0.0);
            opacity->setEndValue(1.0);

            auto* geom = new QPropertyAnimation(this, "geometry", this);
            geom->setDuration(95);
            geom->setStartValue(g1);
            geom->setEndValue(g0);

            auto* grp = new QParallelAnimationGroup(this);
            grp->addAnimation(opacity);
            grp->addAnimation(geom);
            grp->start(QAbstractAnimation::DeleteWhenStopped);

            if (mSave) mSave->setFocus(Qt::OtherFocusReason);
        }

        void keyPressEvent(QKeyEvent* e) override
        {
            if (!e) { CosmicPopupWindow::keyPressEvent(e); return; }
            switch (e->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                e->accept(); mChoice = Save; accept(); return;
            case Qt::Key_Escape:
                e->accept(); mChoice = Cancel; reject(); return;
            default: break;
            }
            CosmicPopupWindow::keyPressEvent(e);
        }

    private:
        Choice mChoice = Cancel;
        QPointer<CosmicWindowHeader> mHeader;
        QPointer<QLabel>             mIcon;
        QPointer<QLabel>             mBody;
        QPointer<CosmicButton>       mQuitAnyway;
        QPointer<CosmicButton>       mSave;
    };

    UnsavedChangesPopup popup(isQuitting, this);
    popup.exec();

    if (popup.choice() == UnsavedChangesPopup::Save)    return saveDocument();
    if (popup.choice() == UnsavedChangesPopup::Discard) return true;
    return false;
}

bool MainWindow2::autoSave()
{
    SceneSequenceManager* ssm = (mTimeLine) ? mTimeLine->sceneSequenceManager() : nullptr;
    const QString projectPath = ssm ? ssm->projectFilePath() : QString();

    // Case 1: project already has a save path — silent background save
    if (!projectPath.isEmpty()) {
        bool ok = saveObject(projectPath);
        if (ok && mEditor) mEditor->markDocumentClean();
        return ok;
    }

    // Case 2: transient workspace — save snapshot to autosave.n2d silently
    // Only do this for projects that have a known project path (i.e. the user
    // has saved before). A brand-new unsaved project (isTransientProject) must
    // fall through to Case 4 so the user gets the CosmicAutoSavePopup.
    if (ssm && ssm->hasWorkspace() && ssm->workspaceIsTransient() && !ssm->isTransientProject())
    {
        const QString autosavePath = ssm->workspaceAutoSavePath();
        if (!autosavePath.isEmpty())
        {
            const QString prevProjectPath = ssm->projectFilePath();
            const QString prevEditorPath = (mEditor && mEditor->object()) ? mEditor->object()->filePath() : QString();

            CosmicSaveProgressPopup progress(this);
            progress.setTitle(tr("Autosaving…"));
            progress.setRange(0, 100);
            progress.showCentered();

            mEditor->prepareSave();

            const N2DProjectIO::ProgressCallbacks cb{
                [&progress](int p) {
                    progress.setValue(p);
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                },
                [&progress](int max) {
                    progress.setRange(0, max + 3);
                }
            };

            Status st = N2DProjectIO::saveProject(autosavePath, mEditor, ssm, cb);

            // Restore transient state
            if (ssm) {
                ssm->setProjectFilePath(prevProjectPath);
                ssm->setWorkspaceTransient(true);
            }
            if (mEditor && mEditor->object()) {
                mEditor->object()->setFilePath(prevEditorPath);
            }

            progress.setValue(100);
            if (st.ok() && mEditor) mEditor->markDocumentClean();
            return st.ok();
        }
    }

    // Case 3: project has a file path (legacy path)
    if (mEditor->object() && !mEditor->object()->filePath().isEmpty())
    {
        bool ok = saveDocument();
        if (ok && mEditor) mEditor->markDocumentClean();
        return ok;
    }

    // Case 4: unsaved project — show CosmicAutoSavePopup
    if (mSuppressAutoSaveDialog)
        return false;

    CosmicAutoSavePopup popup(this);
    popup.setCenterOnShow(true);
    int result = popup.exec();

    if (result == QDialog::Accepted) {
        // Save Now
        bool ok = saveDocument();
        if (ok && mEditor) mEditor->markDocumentClean();
        return ok;
    }

    // Snooze: restart the timer for the requested snooze duration
    if (mEditor) {
        const int snoozeMsec = (result == CosmicAutoSavePopup::Snooze5) ? (5 * 60 * 1000) : (15 * 60 * 1000);
        // Temporarily restart the timer with the snooze interval; it will
        // revert to the normal interval on the next markDocumentClean() call.
        if (mEditor->autosaveTimer()) {
            mEditor->autosaveTimer()->start(snoozeMsec);
        }
    }
    return false;
}

void MainWindow2::emptyDocumentWhenErrorOccurred()
{
    newObject();

    setWindowTitle(PENCIL_WINDOW_TITLE);
    updateSaveState();
}

void MainWindow2::importLayers()
{
    auto* popup = new CosmicImportLayersPopup(this);
    popup->setCore(mEditor);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    // Refresh timeline & canvas when the dialog closes (whether imported or canceled)
    connect(popup, &CosmicImportLayersPopup::finished, [this](int)
    {
        if (mTimeLine) { mTimeLine->updateContent(); }
        if (mEditor)  { mEditor->updateFrame(); }
    });
    popup->open();
}

void MainWindow2::importSingleImage()
{
    const QString filePath = FileDialog::getOpenFileName(this, FileType::IMAGE);
    if (filePath.isEmpty()) return;

    // Read image at native resolution
    QImageReader reader(filePath);
    QImage img(reader.size(), QImage::Format_ARGB32_Premultiplied);
    if (!reader.read(&img) || img.isNull()) {
        CosmicWarningPopup popup(this);
        popup.setBodyText(tr("Could not read the selected image file."));
        popup.exec();
        return;
    }

    // Create a bitmap layer named after the file
    const QString layerName = mEditor->layers()->nameSuggestLayer(QFileInfo(filePath).baseName());
    LayerBitmap* layer = mEditor->layers()->createBitmapLayer(layerName);
    if (!layer) return;

    // Ensure a keyframe exists at frame 1 (createBitmapLayer already adds one, but be safe)
    if (!layer->keyExists(1))
        layer->addNewKeyFrameAt(1);

    // Paste into the active inner layer, centered on canvas origin
    KeyFrame* kf = layer->getKeyFrameAt(1);
    auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
    if (bkf && layer->cellBank()) {
        const int cid = bkf->cellId();
        BitmapImage* target = layer->cellBank()->getActiveInnerLayerImage(cid);
        if (target) {
            const QPoint topLeft(-img.width() / 2, -img.height() / 2);
            BitmapImage importedBmp(topLeft, img);
            target->paste(&importedBmp);
            CellLayerStack* st = layer->cellBank()->getCellLayerStack(cid);
            if (st) st->invalidateComposite();
            mEditor->setModified(mEditor->layers()->getIndex(layer), 1);
        }
    }

    mEditor->scrubTo(1);
    mEditor->updateUndoContext();
    if (mTimeLine) mTimeLine->updateContent();
    if (mEditor->getScribbleArea()) mEditor->getScribbleArea()->update();
}

void MainWindow2::importImageSequenceNew()
{
    // ── 1. File selection ────────────────────────────────────────────────────
    const QStringList files = FileDialog::getOpenFileNames(this, FileType::IMAGE);
    if (files.isEmpty()) return;

    // ── 2. Parse & group — layer name from first valid hit per group ─────────
    // QMap keeps groups in alphabetical order (deterministic).
    QMap<QString, QList<ImageSequenceFileInfo>> groups;
    for (const QString& fp : files) {
        const ImageSequenceFileInfo p = parseImageSequencePadding(fp);
        if (!p.valid()) continue;
        groups[p.baseName].append(p);
    }

    if (groups.isEmpty()) {
        CosmicConfirmPopup warn(this);
        warn.setBodyText(tr("No valid sequence padding found in the selected files."));
        warn.setCancelText(tr("Close"));
        warn.setConfirmText(tr("Explain"));
        if (warn.exec() == QDialog::Accepted) {
            CosmicWarningPopup help(this);
            help.setWarningIconEnabled(false);
            help.setConfirmGreen(true);
            help.setBodyText(tr(
                "Each file needs a number at the end of its name so the app knows what order to play them in."
            ));
            help.setExamplesText(tr(
                "walk_001.png\n"
                "jump-0042.png\n"
                "run.0010.png\n"
                "\n"
                "Separators accepted:  _  -  .\n"
                "Digit count:  any (1 or more digits)"
            ));
            help.setConfirmText(tr("Got it"));
            help.exec();
        }
        return;
    }

    // ── 3. Count total files for progress ────────────────────────────────────
    int totalFiles = 0;
    for (const auto& seq : groups) totalFiles += seq.size();

    CosmicProgressPopup progress(this);
    progress.setTitle(tr("Importing image sequence…"));
    progress.setRange(0, totalFiles);
    progress.setCancelEnabled(true);
    progress.showCentered();

    int imported = 0;
    bool aborted = false;

    for (auto it = groups.constBegin(); it != groups.constEnd() && !aborted; ++it) {
        if (progress.wasCanceled()) { aborted = true; break; }

        // Sort by frame number so we process in order
        QList<ImageSequenceFileInfo> seq = it.value();
        std::sort(seq.begin(), seq.end(), [](const ImageSequenceFileInfo& a, const ImageSequenceFileInfo& b) {
            return a.frameNumber < b.frameNumber;
        });

        // Create a dedicated bitmap layer for this sequence
        LayerBitmap* layer = nullptr;
        if (mEditor && mEditor->layers()) {
            const QString uniqueName = mEditor->layers()->nameSuggestLayer(it.key());
            layer = mEditor->layers()->createBitmapLayer(uniqueName);
        }
        if (!layer) continue;

        // ── 5. Import each frame directly onto the layer ─────────────────────
        for (const ImageSequenceFileInfo& f : seq) {
            if (progress.wasCanceled()) { aborted = true; break; }

            const int targetFrame = f.frameNumber; // already >= 1

            // Read the image
            QImageReader reader(f.filePath);
            QImage img(reader.size(), QImage::Format_ARGB32_Premultiplied);
            if (!reader.read(&img) || img.isNull()) {
                // Skip unreadable files silently — don't abort the whole import
                ++imported;
                progress.setValue(imported);
                continue;
            }

            // Ensure a keyframe exists at targetFrame (create if absent)
            if (!layer->keyExists(targetFrame))
                layer->addNewKeyFrameAt(targetFrame);

            // Paste into the active inner layer of this cell (not the composite)
            KeyFrame* kf = layer->getKeyFrameAt(targetFrame);
            auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
            if (bkf && layer->cellBank()) {
                const int cid = bkf->cellId();
                BitmapImage* target = layer->cellBank()->getActiveInnerLayerImage(cid);
                if (target) {
                    const QPoint topLeft(-img.width() / 2, -img.height() / 2);
                    BitmapImage importedBmp(topLeft, img);
                    target->paste(&importedBmp);
                    CellLayerStack* st = layer->cellBank()->getCellLayerStack(cid);
                    if (st) st->invalidateComposite();
                    mEditor->setModified(mEditor->layers()->getIndex(layer), targetFrame);
                }
            }

            ++imported;
            progress.setValue(imported);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    progress.close();

    mEditor->updateUndoContext();
    if (mTimeLine) mTimeLine->updateContent();
    if (mEditor->getScribbleArea()) mEditor->getScribbleArea()->update();
}

void MainWindow2::importAnimatedImage()
{
    // Flag this so we don't prompt the user about auto-save in the middle of the import.
    mSuppressAutoSaveDialog = true;

    mCommands->importAnimatedImage();
    mSuppressAutoSaveDialog = false;
}

void MainWindow2::lockWidgets(bool shouldLock)
{
    // No traditional docks remain - all panels are in cosmic tool panels.
    Q_UNUSED(shouldLock)
}

void MainWindow2::lockToolbars(bool locked)
{
    mToolbarsLocked = locked;
    for (QToolBar* tb : mToolbars) {
        if (!tb) continue;
        tb->setMovable(!locked);
        // Hide/show the drag handle by toggling the style sheet grip
        if (locked) {
            tb->setStyleSheet(tb->styleSheet() +
                " QToolBar::handle { width: 0px; height: 0px; image: none; }");
        } else {
            // Remove the handle suppression - reapply base style only
            QString s = tb->styleSheet();
            s.remove(" QToolBar::handle { width: 0px; height: 0px; image: none; }");
            tb->setStyleSheet(s);
        }
    }
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(QStringLiteral("Toolbars/Locked"), locked);
}

void MainWindow2::preferences()
{
    if (mPrefDialog)
    {
        mPrefDialog->activateWindow();
        mPrefDialog->raise();
        return;
    }
    mPrefDialog = new PreferencesDialog(this);
    mPrefDialog->setAttribute(Qt::WA_DeleteOnClose);
    mPrefDialog->setWindowModality(Qt::WindowModal);
    mPrefDialog->init(mEditor->preference());

    connect(mPrefDialog, &PreferencesDialog::windowOpacityChange, this, &MainWindow2::setOpacity);
    connect(mPrefDialog, &PreferencesDialog::soundScrubChanged, mEditor->playback(), &PlaybackManager::setSoundScrubActive);
    connect(mPrefDialog, &PreferencesDialog::soundScrubMsecChanged, mEditor->playback(), &PlaybackManager::setSoundScrubMsec);
    connect(mPrefDialog, &PreferencesDialog::finished, [&]
    {
        clearKeyboardShortcuts();
        setupKeyboardShortcuts();
        mPrefDialog = nullptr;
    });

    mPrefDialog->open();
}

void MainWindow2::resetAndDockAllSubWidgets()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.remove(SETTING_WINDOW_GEOMETRY);
    settings.remove(SETTING_WINDOW_STATE);
    settings.remove("DockFloatingLock");
    // All panels are hosted in cosmic tool panels - nothing to re-dock.
}

void MainWindow2::newObject()
{
    auto object = new Object();
    object->init();

    // default layers
    object->addNewCameraLayer();
    object->addNewBitmapLayer();
    // Layers are counted bottom up
    // 0 - Camera Layer
    // 1 - Bitmap Layer
    object->data()->setCurrentLayer(1);

    mEditor->setObject(object);

    closeDialogs();

    setWindowTitle(PENCIL_WINDOW_TITLE);

    updateBackupActionState();
    
    // Reset to pencil tool and re-apply the active brush preset, matching
    // the same state as app startup. ToolManager::load() resets to PEN
    // (setDefaultTool), so we must explicitly switch back to PENCIL and
    // replay the active preset so the tool pipeline matches the UI.
    if (mEditor && mEditor->tools()) {
        mEditor->tools()->setCurrentTool(PENCIL);
    }
    if (mBrushFactoryDock) {
        auto* store = BrushPresetStore::instance();
        store->ensureLoaded();
        const int activeIdx = store->activeIndex();
        mBrushFactoryDock->setActivePreset(activeIdx);
    }

    // Set the drawing color to black with red hue for consistency with app launch
    // This ensures File/New behaves the same as app startup - black drawing color
    // but with the hue wheel positioned at red (hue=0)
    if (mColorBox) {
        mColorBox->setColor(QColor::fromHsv(0, 0, 0)); // Black with red hue (0)
    }
}

bool MainWindow2::newObjectFromPresets(int presetIndex)
{
    QString presetFilePath = PresetDialog::getPresetPath(presetIndex);

    if (presetFilePath.isEmpty())
    {
        return false;
    }

    Status st = Status::OK;
    const N2DProjectIO::ProgressCallbacks cb;
    std::unique_ptr<Object> object = N2DProjectIO::loadSingleShotProjectAsObject(presetFilePath, cb, &st);
    if (!st.ok() || !object) {
        return false;
    }

    mEditor->setObject(object.release());
    if (mEditor->object()) {
        mEditor->object()->setFilePath(QString());
    }

    // Same tool/preset reset as newObject() — ToolManager::load resets to PEN.
    if (mEditor && mEditor->tools()) {
        mEditor->tools()->setCurrentTool(PENCIL);
    }
    if (mBrushFactoryDock) {
        auto* store = BrushPresetStore::instance();
        store->ensureLoaded();
        mBrushFactoryDock->setActivePreset(store->activeIndex());
    }

    setWindowTitle(PENCIL_WINDOW_TITLE);
    updateSaveState();
    updateBackupActionState();

    return true;
}

bool MainWindow2::loadMostRecent()
{
    if(!mEditor->preference()->isOn(SETTING::LOAD_MOST_RECENT))
    {
        return false;
    }

    QSettings settings(PENCIL2D, PENCIL2D);
    QString myPath = settings.value(LAST_PCLX_PATH, QVariant("")).toString();
    if (myPath.isEmpty() || !QFile::exists(myPath))
    {
        return false;
    }
    return openObject(myPath);
}

bool MainWindow2::tryLoadPreset()
{
    if (!mEditor->preference()->isOn(SETTING::ASK_FOR_PRESET))
    {
        int defaultPreset = mEditor->preference()->getInt(SETTING::DEFAULT_PRESET);
        return newObjectFromPresets(defaultPreset);
    }

    PresetDialog* presetDialog = new PresetDialog(mEditor->preference(), this);
    presetDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(presetDialog, &PresetDialog::finished, [=](int result)
    {
        if (result != QDialog::Accepted)
        {
            return;
        }

        int presetIndex = presetDialog->getPresetIndex();
        if (presetDialog->shouldAlwaysUse())
        {
            mEditor->preference()->set(SETTING::ASK_FOR_PRESET, false);
            mEditor->preference()->set(SETTING::DEFAULT_PRESET, presetIndex);
        }
        if (!newObjectFromPresets(presetIndex))
        {
            newObject();
        }
    });
    presetDialog->open();
    return true;
}

void MainWindow2::closeDialogs()
{
    for (auto dialog : findChildren<QDialog*>(QString(), Qt::FindDirectChildrenOnly)) {
        dialog->close();
    }
}

void MainWindow2::readSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);

    // Defer restoreState() and restoreGeometry() until after the event loop starts.
    QTimer::singleShot(0, this, SLOT(restoreDockState()));

    int opacity = mEditor->preference()->getInt(SETTING::WINDOW_OPACITY);
    setOpacity(100 - opacity);

    bool isWindowsLocked = mEditor->preference()->isOn(SETTING::LAYOUT_LOCK);
    lockWidgets(isWindowsLocked);
}

void MainWindow2::writeSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(SETTING_WINDOW_GEOMETRY, saveGeometry());
    settings.setValue(SETTING_WINDOW_STATE, saveState());

    // No traditional docks remain - floating lock state no longer needed.
}

void MainWindow2::setupKeyboardShortcuts()
{
    checkExistingShortcuts();

    auto cmdKeySeq = [](QString strCommandName) -> QKeySequence
    {
        const QString keyPath = QString("shortcuts/") + strCommandName;
        const QString configuredSequence = pencilSettings().value(keyPath).toString();
        QKeySequence keySequence(configuredSequence);
        if (!configuredSequence.trimmed().isEmpty() && keySequence.isEmpty()) {
            qWarning() << "[KeyboardShortcuts] Invalid key sequence in settings:"
                       << keyPath << configuredSequence;
        }

#ifdef Q_OS_MACOS
        // Qt intentionally maps portable Ctrl shortcuts to Command on macOS.
        // Keep the portable sequence intact; Meta would mean the physical Control key.
        if (strCommandName == QLatin1String(CMD_UNDO) || strCommandName == QLatin1String(CMD_REDO)) {
            qInfo() << "[KeyboardShortcuts] Registered macOS undo/redo shortcut:"
                    << strCommandName << configuredSequence
                    << "->" << keySequence.toString(QKeySequence::NativeText);
        }
#endif
        return keySequence;
    };

    // File menu
    ui->actionNew->setShortcut(cmdKeySeq(CMD_NEW_FILE));
    ui->actionOpen->setShortcut(cmdKeySeq(CMD_OPEN_FILE));
    ui->actionSave->setShortcut(cmdKeySeq(CMD_SAVE_FILE));
    ui->actionSave_as->setShortcut(cmdKeySeq(CMD_SAVE_AS));

    
    // Removed: shortcuts for Import Movie Video and Animated Image
    ui->actionImport_Image->setShortcut(cmdKeySeq(CMD_IMPORT_IMAGE));
    ui->actionImportLayers_from_pclx->setShortcut(cmdKeySeq(CMD_IMPORT_LAYERS));
    // Removed: shortcuts for Import Sound and Import Movie Audio
    // Removed: shortcuts for Import Palette (Append/Replace)

    ui->actionExport_Image->setShortcut(cmdKeySeq(CMD_EXPORT_IMAGE));
    // Removed: shortcut for Current Layer Animation (Image Sequence)
    ui->actionExport_Animation->setShortcut(cmdKeySeq(CMD_EXPORT_ANIMATION));
    ui->actionExport_Sprite->setShortcut(cmdKeySeq(CMD_EXPORT_SPRITE));
    ui->actionExport_Gif->setShortcut(cmdKeySeq(CMD_EXPORT_GIF));
    // Removed: shortcut for Export Palette

    // Restore import image sequence shortcut
    ui->actionImport_ImageSeq->setShortcut(cmdKeySeq(CMD_IMPORT_IMAGE_SEQ));

    // edit menu
    ui->actionUndo->setShortcut(cmdKeySeq(CMD_UNDO));
    ui->actionRedo->setShortcut(cmdKeySeq(CMD_REDO));
    ui->actionCut->setShortcut(cmdKeySeq(CMD_CUT));
    ui->actionCopy->setShortcut(cmdKeySeq(CMD_COPY));
    ui->actionPaste_Previous->setShortcut(cmdKeySeq(CMD_PASTE_FROM_PREVIOUS));
    ui->actionPaste->setShortcut(cmdKeySeq(CMD_PASTE));
    ui->actionClearFrame->setShortcut(cmdKeySeq(CMD_CLEAR_FRAME));
    ui->actionFlip_X->setShortcut(cmdKeySeq(CMD_SELECTION_FLIP_HORIZONTAL));
    ui->actionFlip_Y->setShortcut(cmdKeySeq(CMD_SELECTION_FLIP_VERTICAL));
    ui->actionSelect_All->setShortcut(cmdKeySeq(CMD_SELECT_ALL));
    ui->actionDeselect_All->setShortcut(cmdKeySeq(CMD_DESELECT_ALL));
    ui->actionPegbarAlignment->setShortcut(cmdKeySeq(CMD_PEGBAR_ALIGNMENT));
    ui->actionPreference->setShortcut(cmdKeySeq(CMD_PREFERENCE));

    // Layer menu
    ui->actionNew_Bitmap_Layer->setShortcut(cmdKeySeq(CMD_NEW_BITMAP_LAYER));
    ui->actionDelete_Current_Layer->setShortcut(cmdKeySeq(CMD_DELETE_CUR_LAYER));
    ui->actionChangeLineColorCurrent_keyframe->setShortcut(cmdKeySeq(CMD_CHANGE_LINE_COLOR_KEYFRAME));
    ui->actionChangeLineColorAll_keyframes_on_layer->setShortcut(cmdKeySeq(CMD_CHANGE_LINE_COLOR_LAYER));
    ui->actionChangeLayerOpacity->setShortcut(cmdKeySeq(CMD_CHANGE_LAYER_OPACITY));
    ui->actionVisibilityCurrentLayerOnly->setShortcut(cmdKeySeq(CMD_CURRENT_LAYER_VISIBILITY));
    ui->actionVisibilityRelative->setShortcut(cmdKeySeq(CMD_RELATIVE_LAYER_VISIBILITY));
    ui->actionVisibilityAll->setShortcut(cmdKeySeq(CMD_ALL_LAYER_VISIBILITY));

    // Selection / animation operations from the Animation menu
    ui->actionMove_Frame_Backward->setShortcut(cmdKeySeq(CMD_MOVE_FRAME_BACKWARD));
    ui->actionMove_Frame_Forward->setShortcut(cmdKeySeq(CMD_MOVE_FRAME_FORWARD));
    ui->actionFlip_inbetween->setShortcut(cmdKeySeq(CMD_FLIP_INBETWEEN));
    ui->actionFlip_rolling->setShortcut(cmdKeySeq(CMD_FLIP_ROLLING));
    ui->actionAdd_Frame_Exposure->setShortcut(cmdKeySeq(CMD_SELECTION_ADD_FRAME_EXPOSURE));
    ui->actionSubtract_Frame_Exposure->setShortcut(cmdKeySeq(CMD_SELECTION_SUBTRACT_FRAME_EXPOSURE));
    ui->actionReverse_Frames_Order->setShortcut(cmdKeySeq(CMD_REVERSE_SELECTED_FRAMES));
    ui->actionRemove_Frames->setShortcut(cmdKeySeq(CMD_REMOVE_SELECTED_FRAMES));
    ui->actionReposition_Selected_Frames->setShortcut(cmdKeySeq(CMD_SELECTION_REPOSITION_FRAMES));

    // Misc
    ui->actionResetToolsDefault->setShortcut(cmdKeySeq(CMD_RESET_ALL_TOOLS));
    ui->actionExit->setShortcut(cmdKeySeq(CMD_EXIT));

    // View menu
    ui->actionReset_View->setShortcut(cmdKeySeq(CMD_RESET_ZOOM_ROTATE));
    ui->actionCenter_View->setShortcut(cmdKeySeq(CMD_CENTER_VIEW));
    ui->actionZoom_In->setShortcut(cmdKeySeq(CMD_ZOOM_IN));
    ui->actionZoom_Out->setShortcut(cmdKeySeq(CMD_ZOOM_OUT));
    ui->actionZoom400->setShortcut(cmdKeySeq(CMD_ZOOM_400));
    ui->actionZoom300->setShortcut(cmdKeySeq(CMD_ZOOM_300));
    ui->actionZoom200->setShortcut(cmdKeySeq(CMD_ZOOM_200));
    ui->actionZoom100->setShortcut(cmdKeySeq(CMD_ZOOM_100));
    ui->actionZoom50->setShortcut(cmdKeySeq(CMD_ZOOM_50));
    ui->actionZoom33->setShortcut(cmdKeySeq(CMD_ZOOM_33));
    ui->actionZoom25->setShortcut(cmdKeySeq(CMD_ZOOM_25));
    // Clear default rotate shortcuts regardless of kb.ini defaults
    ui->actionRotate_Clockwise->setShortcut(QKeySequence());
    ui->actionRotate_Anticlockwise->setShortcut(QKeySequence());
    ui->actionReset_Rotation->setShortcut(cmdKeySeq(CMD_RESET_ROTATION));
    ui->actionHorizontal_Flip->setShortcut(cmdKeySeq(CMD_FLIP_HORIZONTAL));
    ui->actionVertical_Flip->setShortcut(cmdKeySeq(CMD_FLIP_VERTICAL));
    ui->actionCenter->setShortcut(cmdKeySeq(CMD_OVERLAY_CENTER));
    ui->actionThirds->setShortcut(cmdKeySeq(CMD_OVERLAY_THIRDS));
    ui->actionGoldenRatio->setShortcut(cmdKeySeq(CMD_OVERLAY_GOLDEN_RATIO));
    ui->actionSafeAreas->setShortcut(cmdKeySeq(CMD_OVERLAY_SAFE_AREAS));
    ui->actionOnionPrev->setShortcut(cmdKeySeq(CMD_ONIONSKIN_PREV));
    ui->actionOnionNext->setShortcut(cmdKeySeq(CMD_ONIONSKIN_NEXT));

    // Animation menu
    ui->actionPlay->setShortcut(cmdKeySeq(CMD_PLAY));
    ui->actionLoop->setShortcut(cmdKeySeq(CMD_LOOP));
    ui->actionLoopControl->setShortcut(cmdKeySeq(CMD_LOOP_CONTROL));
    ui->actionPrevious_Frame->setShortcut(cmdKeySeq(CMD_GOTO_PREV_FRAME));
    ui->actionNext_Frame->setShortcut(cmdKeySeq(CMD_GOTO_NEXT_FRAME));
    ui->actionPrev_KeyFrame->setShortcut(cmdKeySeq(CMD_GOTO_PREV_KEY_FRAME));
    ui->actionNext_KeyFrame->setShortcut(cmdKeySeq(CMD_GOTO_NEXT_KEY_FRAME));
    qInfo() << "[KeyframeNavigation] Drawing navigation initialized; executable="
            << QCoreApplication::applicationFilePath()
            << "previous=" << ui->actionPrevious_Frame->shortcut().toString()
            << "next=" << ui->actionNext_Frame->shortcut().toString();
    ui->actionAdd_Frame->setShortcut(cmdKeySeq(CMD_ADD_FRAME));
    ui->actionDuplicate_Frame->setShortcut(cmdKeySeq(CMD_DUPLICATE_FRAME));
    ui->actionRemove_Frame->setShortcut(cmdKeySeq(CMD_REMOVE_FRAME));

    // Tool shortcuts (Tools menu / toolbar)
    ui->actionMove->setShortcut(cmdKeySeq(CMD_TOOL_MOVE));
    ui->actionSelect->setShortcut(cmdKeySeq(CMD_TOOL_SELECT));
    ui->actionPen->setShortcut(cmdKeySeq(CMD_TOOL_PEN));
    ui->actionHand->setShortcut(cmdKeySeq(CMD_TOOL_HAND));
    ui->actionBucket->setShortcut(cmdKeySeq(CMD_TOOL_BUCKET));
    ui->actionEyedropper->setShortcut(cmdKeySeq(CMD_TOOL_EYEDROPPER));
    ui->actionEraser->setShortcut(cmdKeySeq(CMD_TOOL_ERASER));
    ui->actionPolyline->setShortcut(cmdKeySeq(CMD_TOOL_POLYLINE));

    // Optional tools created programmatically (toolbar-only actions)
    if (mActionPencil)       { mActionPencil->setShortcut(cmdKeySeq(CMD_TOOL_PENCIL)); }
    if (mActionCurve)        { mActionCurve->setShortcut(cmdKeySeq(CMD_TOOL_CURVE)); }
    if (mActionBSpline)      { mActionBSpline->setShortcut(cmdKeySeq(CMD_TOOL_BSPLINE)); }
    if (mActionRectGapFill)  { mActionRectGapFill->setShortcut(cmdKeySeq(CMD_TOOL_RECT_GAP_FILL)); }
    if (mActionLassoGapFill) { mActionLassoGapFill->setShortcut(cmdKeySeq(CMD_TOOL_LASSO_GAP_FILL)); }
    if (mActionLassoSelect)  { mActionLassoSelect->setShortcut(cmdKeySeq(CMD_TOOL_LASSO_SELECT)); }
    if (mActionDplate)       { mActionDplate->setShortcut(cmdKeySeq(CMD_TOOL_DPLATE)); }

    // Add tool actions to MainWindow so Qt routes shortcut events to them.
    // This follows the same pattern as dock toggle actions (lines 2443-2450).
    // Without addAction(), shortcuts are applied but never trigger.

    // UI-defined tool actions (always present)
    ui->actionMove->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionMove);

    ui->actionSelect->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionSelect);

    ui->actionPen->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionPen);

    ui->actionHand->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionHand);

    ui->actionBucket->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionBucket);

    ui->actionEyedropper->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionEyedropper);

    ui->actionEraser->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionEraser);

    ui->actionPolyline->setShortcutContext(Qt::WindowShortcut);
    addAction(ui->actionPolyline);

    // Programmatic tool actions (toolbar-only, may be null if toolbar creation failed)
    if (mActionPencil) {
        mActionPencil->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionPencil);
    }

    if (mActionCurve) {
        mActionCurve->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionCurve);
    }

    if (mActionBSpline) {
        mActionBSpline->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionBSpline);
    }

    if (mActionRectGapFill) {
        mActionRectGapFill->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionRectGapFill);
    }

    if (mActionLassoGapFill) {
        mActionLassoGapFill->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionLassoGapFill);
    }

    if (mActionLassoSelect) {
        mActionLassoSelect->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionLassoSelect);
    }

    if (mActionDplate) {
        mActionDplate->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionDplate);
    }

    // Popup color wheel: use a dedicated QShortcut instead of relying on QAction routing.
    // This avoids interference from menu/tool actions and keeps the behavior explicit.
    QKeySequence popupSeq = cmdKeySeq(CMD_POPUP_COLOR_WHEEL);
    if (popupSeq.isEmpty()) {
        // Fallback default if user has not defined a shortcut in preferences
        popupSeq = QKeySequence(Qt::SHIFT + Qt::Key_C);
    }

    if (!mPopupColorWheelShortcut) {
        mPopupColorWheelShortcut = new QShortcut(popupSeq, this);
        mPopupColorWheelShortcut->setContext(Qt::ApplicationShortcut);
        connect(mPopupColorWheelShortcut, &QShortcut::activated,
                this, &MainWindow2::showPopupColorWheelAtCursor);
        qDebug() << "[MainWindow2] PopupColorWheel QShortcut created with key=" << popupSeq.toString();
    } else {
        mPopupColorWheelShortcut->setKey(popupSeq);
        qDebug() << "[MainWindow2] PopupColorWheel QShortcut updated to key=" << popupSeq.toString();
    }

    // Dock toggle shortcuts removed - panels are now in cosmic tool panels.
    // Cosmic SidePanels shortcuts (left/right toolpanels + per-pane toggles).
    // These commands exist only in the shortcut system, so we bind them here.
    struct CosmicBinding {
        const char* cmd;
        std::function<void()> fn;
    };

    const CosmicBinding cosmicBindings[] = {
        { CMD_COSMIC_LEFT_LOCK_TOGGLE, [this]() {
              auto* p = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar);
              if (!p) return;
              p->toggleLock();
          } },
        { CMD_COSMIC_RIGHT_LOCK_TOGGLE, [this]() {
              auto* p = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar);
              if (!p) return;
              p->toggleLock();
          } },

        { CMD_COSMIC_PANE_TOGGLE_TOOLS, [this]() {
              auto* p = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar);
              if (!p) return;
              p->togglePaneEnabled(QStringLiteral("Tools"));
          } },
        { CMD_COSMIC_PANE_TOGGLE_TOOLS_OPTIONS, [this]() {
              auto* p = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar);
              if (!p) return;
              p->togglePaneEnabled(QStringLiteral("Tools Options"));
          } },
        { CMD_COSMIC_PANE_TOGGLE_BRUSHES, [this]() {
              auto* p = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar);
              if (!p) return;
              p->togglePaneEnabled(QStringLiteral("Brushes"));
          } },

        { CMD_COSMIC_PANE_TOGGLE_COLOR_WHEEL, [this]() {
              auto* p = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar);
              if (!p) return;
              p->togglePaneEnabled(QStringLiteral("Color Wheel"));
          } },
        { CMD_COSMIC_PANE_TOGGLE_PALETTES, [this]() {
              auto* p = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar);
              if (!p) return;
              p->togglePaneEnabled(QStringLiteral("Palettes"));
          } },
        { CMD_COSMIC_PANE_TOGGLE_REFERENCE, [this]() {
              auto* p = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar);
              if (!p) return;
              p->togglePaneEnabled(QStringLiteral("Reference"));
          } },
    };

    for (const CosmicBinding& b : cosmicBindings) {
        QAction* a = new QAction(this);
        a->setShortcut(cmdKeySeq(QString::fromLatin1(b.cmd)));
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, b.fn);
        addAction(a);
    }

    // Animation shortcuts — retime, make unique, layer navigation, papers pane.
    // All default to no key; user-assignable via Preferences > Shortcuts > Timeline.
    struct AnimBinding {
        const char* cmd;
        std::function<void()> fn;
    };

    const AnimBinding animBindings[] = {
        // Retime selected frames (no ripple)
        { CMD_RETIME_1S, [this]() { if (mTimeLine) mTimeLine->retimeSelectedFrames(1, false); } },
        { CMD_RETIME_2S, [this]() { if (mTimeLine) mTimeLine->retimeSelectedFrames(2, false); } },
        { CMD_RETIME_3S, [this]() { if (mTimeLine) mTimeLine->retimeSelectedFrames(3, false); } },
        // Retime selected frames + ripple trailing
        { CMD_RETIME_1S_RIPPLE, [this]() { if (mTimeLine) mTimeLine->retimeSelectedFrames(1, true); } },
        { CMD_RETIME_2S_RIPPLE, [this]() { if (mTimeLine) mTimeLine->retimeSelectedFrames(2, true); } },
        { CMD_RETIME_3S_RIPPLE, [this]() { if (mTimeLine) mTimeLine->retimeSelectedFrames(3, true); } },
        // Make current drawing unique (break shared exposure)
        { CMD_MAKE_UNIQUE_DRAWING, [this]() { if (mCommands) mCommands->makeUniqueDrawing(); } },
        // Select layer above / below in the timeline (shared with papers pane)
        { CMD_SELECT_LAYER_ABOVE, [this]() { if (mTimeLine) mTimeLine->selectLayerAbove(); } },
        { CMD_SELECT_LAYER_BELOW, [this]() { if (mTimeLine) mTimeLine->selectLayerBelow(); } },
        // Papers pane — add / remove / import inner layers
        { CMD_PAPERS_NEW, [this]() {
              if (auto* p = findChild<CosmicLayersPane*>()) emit p->addLayerRequested();
          } },
        { CMD_PAPERS_REMOVE, [this]() {
              if (auto* p = findChild<CosmicLayersPane*>()) emit p->removeLayerRequested();
          } },
        { CMD_PAPERS_IMPORT, [this]() {
              if (auto* p = findChild<CosmicLayersPane*>()) emit p->importImageToLayerRequested();
          } },
        // Papers pane — navigate active inner layer up / down
        { CMD_PAPERS_SELECT_ABOVE, [this]() {
              if (auto* p = findChild<CosmicLayersPane*>()) p->selectPaperAbove();
          } },
        { CMD_PAPERS_SELECT_BELOW, [this]() {
              if (auto* p = findChild<CosmicLayersPane*>()) p->selectPaperBelow();
          } },
    };

    for (const AnimBinding& b : animBindings) {
        QAction* a = new QAction(this);
        a->setShortcut(cmdKeySeq(QString::fromLatin1(b.cmd)));
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, b.fn);
        addAction(a);
    }

    {
        QAction* a = new QAction(this);
        a->setShortcut(cmdKeySeq(CMD_COSMIC_BOTTOM_TIMELINE_LOCK_TOGGLE));
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, [this]() {
            if (mCosmicBottomToolsBar) {
                mCosmicBottomToolsBar->toggleLock();
            }
        });
        addAction(a);
    }

    // Cosmic ControlCenter shortcuts (pen palettes + autofill toggles).
    // These should default to none and only exist in the shortcut system.
    struct ControlCenterBinding {
        const char* cmd;
        std::function<void()> fn;
    };

    const ControlCenterBinding controlCenterBindings[] = {
        { CMD_CONTROL_CENTER_PEN_PALETTE_1, [this]() {
              if (!mPenPaletteToolbarWidget) return;
              const auto buttons = mPenPaletteToolbarWidget->colorButtons();
              if (buttons.size() > 0 && buttons[0]) buttons[0]->click();
          } },
        { CMD_CONTROL_CENTER_PEN_PALETTE_2, [this]() {
              if (!mPenPaletteToolbarWidget) return;
              const auto buttons = mPenPaletteToolbarWidget->colorButtons();
              if (buttons.size() > 1 && buttons[1]) buttons[1]->click();
          } },
        { CMD_CONTROL_CENTER_PEN_PALETTE_3, [this]() {
              if (!mPenPaletteToolbarWidget) return;
              const auto buttons = mPenPaletteToolbarWidget->colorButtons();
              if (buttons.size() > 2 && buttons[2]) buttons[2]->click();
          } },
        { CMD_CONTROL_CENTER_PEN_PALETTE_4, [this]() {
              if (!mPenPaletteToolbarWidget) return;
              const auto buttons = mPenPaletteToolbarWidget->colorButtons();
              if (buttons.size() > 3 && buttons[3]) buttons[3]->click();
          } },
        { CMD_CONTROL_CENTER_PEN_PALETTE_5, [this]() {
              if (!mPenPaletteToolbarWidget) return;
              const auto buttons = mPenPaletteToolbarWidget->colorButtons();
              if (buttons.size() > 4 && buttons[4]) buttons[4]->click();
          } },
        { CMD_CONTROL_CENTER_PEN_PALETTE_6, [this]() {
              if (!mPenPaletteToolbarWidget) return;
              const auto buttons = mPenPaletteToolbarWidget->colorButtons();
              if (buttons.size() > 5 && buttons[5]) buttons[5]->click();
          } },

        { CMD_CONTROL_CENTER_AUTOFILL_MASTER, [this]() {
              if (!mAutoFillToolbarWidget) return;
              if (auto* master = mAutoFillToolbarWidget->masterToggle()) master->click();
          } },
        { CMD_CONTROL_CENTER_AUTOFILL_COLOR_1, [this]() {
              if (!mAutoFillToolbarWidget) return;
              const auto buttons = mAutoFillToolbarWidget->colorButtons();
              if (buttons.size() > 0 && buttons[0]) buttons[0]->click();
          } },
        { CMD_CONTROL_CENTER_AUTOFILL_COLOR_2, [this]() {
              if (!mAutoFillToolbarWidget) return;
              const auto buttons = mAutoFillToolbarWidget->colorButtons();
              if (buttons.size() > 1 && buttons[1]) buttons[1]->click();
          } },
        { CMD_CONTROL_CENTER_AUTOFILL_COLOR_3, [this]() {
              if (!mAutoFillToolbarWidget) return;
              const auto buttons = mAutoFillToolbarWidget->colorButtons();
              if (buttons.size() > 2 && buttons[2]) buttons[2]->click();
          } },
        { CMD_CONTROL_CENTER_AUTOFILL_COLOR_4, [this]() {
              if (!mAutoFillToolbarWidget) return;
              const auto buttons = mAutoFillToolbarWidget->colorButtons();
              if (buttons.size() > 3 && buttons[3]) buttons[3]->click();
          } },
        { CMD_CONTROL_CENTER_AUTOFILL_COLOR_5, [this]() {
              if (!mAutoFillToolbarWidget) return;
              const auto buttons = mAutoFillToolbarWidget->colorButtons();
              if (buttons.size() > 4 && buttons[4]) buttons[4]->click();
          } },
    };

    for (const ControlCenterBinding& b : controlCenterBindings) {
        QAction* a = new QAction(this);
        a->setShortcut(cmdKeySeq(QString::fromLatin1(b.cmd)));
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, b.fn);
        addAction(a);
    }

    if (mToggleGapCheckAction) {
        mToggleGapCheckAction->setShortcut(cmdKeySeq(CMD_TOGGLE_GAP_CHECK));
        mToggleGapCheckAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mCCToggleFileOpsAction) {
        mCCToggleFileOpsAction->setShortcut(cmdKeySeq(CMD_CC_TOGGLE_FILE_OPS));
        mCCToggleFileOpsAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mCCToggleViewAction) {
        mCCToggleViewAction->setShortcut(cmdKeySeq(CMD_CC_TOGGLE_VIEW));
        mCCToggleViewAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mCCToggleOverlaysAction) {
        mCCToggleOverlaysAction->setShortcut(cmdKeySeq(CMD_CC_TOGGLE_OVERLAYS));
        mCCToggleOverlaysAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mCCToggleUndoRedoAction) {
        mCCToggleUndoRedoAction->setShortcut(cmdKeySeq(CMD_CC_TOGGLE_UNDO_REDO));
        mCCToggleUndoRedoAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mCCToggleClearFrameAction) {
        mCCToggleClearFrameAction->setShortcut(cmdKeySeq(CMD_CC_TOGGLE_CLEAR_FRAME));
        mCCToggleClearFrameAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mIncreaseSizeAction) {
        mIncreaseSizeAction->setShortcut(cmdKeySeq(CMD_INCREASE_SIZE));
        mIncreaseSizeAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mDecreaseSizeAction) {
        mDecreaseSizeAction->setShortcut(cmdKeySeq(CMD_DECREASE_SIZE));
        mDecreaseSizeAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mIncreaseOpacityAction) {
        mIncreaseOpacityAction->setShortcut(cmdKeySeq(CMD_INCREASE_OPACITY));
        mIncreaseOpacityAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mDecreaseOpacityAction) {
        mDecreaseOpacityAction->setShortcut(cmdKeySeq(CMD_DECREASE_OPACITY));
        mDecreaseOpacityAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mIncreaseSmoothAction) {
        mIncreaseSmoothAction->setShortcut(cmdKeySeq(CMD_INCREASE_SMOOTH));
        mIncreaseSmoothAction->setShortcutContext(Qt::WindowShortcut);
    }
    if (mDecreaseSmoothAction) {
        mDecreaseSmoothAction->setShortcut(cmdKeySeq(CMD_DECREASE_SMOOTH));
        mDecreaseSmoothAction->setShortcutContext(Qt::WindowShortcut);
    }

    // Canvas menu actions (user-assignable; default empty)
    if (mActionCanvasSize) {
        mActionCanvasSize->setShortcut(cmdKeySeq(CMD_CANVAS_SIZE));
        mActionCanvasSize->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionCanvasSize);
    }
    if (mActionCanvasColor) {
        mActionCanvasColor->setShortcut(cmdKeySeq(CMD_CANVAS_COLOR));
        mActionCanvasColor->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionCanvasColor);
    }
    if (mActionCanvasExtraSpace) {
        mActionCanvasExtraSpace->setShortcut(cmdKeySeq(CMD_CANVAS_EXTRA_SPACE));
        mActionCanvasExtraSpace->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionCanvasExtraSpace);
    }
    if (mActionShowCanvasPalette) {
        mActionShowCanvasPalette->setShortcut(cmdKeySeq(CMD_CANVAS_SHOW_PALETTE));
        mActionShowCanvasPalette->setShortcutContext(Qt::WindowShortcut);
        addAction(mActionShowCanvasPalette);
    }

    // Basic runtime validation hook for shortcut configuration. This logs
    // potential misconfigurations (such as missing actions for known shortcut
    // commands) once at startup without altering runtime behavior.
    auto validateShortcuts = [&]() {
        QSettings defaults(":/resources/kb.ini", QSettings::IniFormat);
        if (defaults.allKeys().isEmpty()) {
            qWarning() << "[MainWindow2] validateShortcuts: kb.ini missing or unreadable";
            return;
        }

        defaults.beginGroup(SHORTCUTS_GROUP);
        const QStringList allCmds = defaults.allKeys();
        defaults.endGroup();

        // Build a lookup of all QAction and QShortcut objects currently
        // registered on the main window so we can detect orphaned commands.
        const QList<QAction*> allActions = findChildren<QAction*>();
        const QList<QShortcut*> allShortcuts = findChildren<QShortcut*>();

        auto hasBindingFor = [&](const QString& cmdName) -> bool {
            const QString fullKey = QStringLiteral("shortcuts/") + cmdName;

            // Check QActions that use our cmdKeySeq mapping
            for (QAction* a : allActions) {
                if (!a) continue;
                const QKeySequence seq = a->shortcut();
                if (seq.isEmpty()) continue;
                if (seq == cmdKeySeq(cmdName)) {
                    return true;
                }
            }

            // Check any dedicated QShortcuts (e.g. popup color wheel)
            for (QShortcut* s : allShortcuts) {
                if (!s) continue;
                const QKeySequence seq = s->key();
                if (seq.isEmpty()) continue;
                if (seq == cmdKeySeq(cmdName)) {
                    return true;
                }
            }
            Q_UNUSED(fullKey);
            return false;
        };

        for (const QString& fullKey : allCmds) {
            const QString cmdName = fullKey;
            if (!hasBindingFor(cmdName)) {
                qWarning() << "[MainWindow2] Orphaned shortcut command in kb.ini:" << cmdName;
            }
        }
    };

    validateShortcuts();

    // Register all menu actions directly on the main window so their shortcuts
    // work even when the native QMenuBar is hidden. Qt only routes QAction
    // shortcuts to the window if the action is either in a visible menu OR
    // explicitly added to the window via addAction().
    const QList<QAction*> menuActions = {
        // File
        ui->actionNew, ui->actionOpen, ui->actionSave, ui->actionSave_as,
        ui->actionImport_Image, ui->actionImportLayers_from_pclx,
        ui->actionImport_ImageSeq, ui->actionExport_Image,
        ui->actionExport_Animation, ui->actionExport_Sprite, ui->actionExit,
        // Edit
        ui->actionUndo, ui->actionRedo,
        ui->actionCut, ui->actionCopy, ui->actionPaste, ui->actionPaste_Previous,
        ui->actionClearFrame, ui->actionFlip_X, ui->actionFlip_Y,
        ui->actionSelect_All, ui->actionDeselect_All,
        ui->actionPegbarAlignment, ui->actionPreference,
        // Layer
        ui->actionNew_Bitmap_Layer, ui->actionDelete_Current_Layer,
        ui->actionChangeLineColorCurrent_keyframe,
        ui->actionChangeLineColorAll_keyframes_on_layer,
        ui->actionChangeLayerOpacity,
        ui->actionVisibilityCurrentLayerOnly, ui->actionVisibilityRelative,
        ui->actionVisibilityAll,
        // Animation
        ui->actionPlay, ui->actionLoop, ui->actionLoopControl,
        ui->actionPrevious_Frame, ui->actionNext_Frame,
        ui->actionPrev_KeyFrame, ui->actionNext_KeyFrame,
        ui->actionAdd_Frame, ui->actionRemove_Frame,
        ui->actionMove_Frame_Forward, ui->actionMove_Frame_Backward,
        ui->actionFlip_inbetween, ui->actionFlip_rolling,
        ui->actionAdd_Frame_Exposure, ui->actionSubtract_Frame_Exposure,
        ui->actionReverse_Frames_Order, ui->actionRemove_Frames,
        ui->actionReposition_Selected_Frames,
        // View
        ui->actionReset_View, ui->actionCenter_View,
        ui->actionZoom_In, ui->actionZoom_Out,
        ui->actionZoom400, ui->actionZoom300, ui->actionZoom200,
        ui->actionZoom100, ui->actionZoom50, ui->actionZoom33, ui->actionZoom25,
        ui->actionReset_Rotation, ui->actionHorizontal_Flip, ui->actionVertical_Flip,
        ui->actionCenter, ui->actionThirds, ui->actionGoldenRatio, ui->actionSafeAreas,
        ui->actionOnionPrev, ui->actionOnionNext,
        // About
        ui->actionAbout,
    };
    for (QAction* a : menuActions) {
        if (a) {
            a->setShortcutContext(Qt::WindowShortcut);
            addAction(a);
        }
    }
}

void MainWindow2::showPopupColorWheelAtCursor()
{
    if (!mEditor) return;

    qDebug() << "[MainWindow2] showPopupColorWheelAtCursor triggered";

    // Obtain current drawing color from color manager
    QColor initial = mEditor->color()->frontColor();

    // Create popup owned by main window so it closes with the app
    auto* popup = new PopupColorWheelWidget(this);
    popup->setPreferenceManager(mEditor->preference());
    if (initial.isValid()) {
        popup->setColor(initial);
    }

    // Live preview while dragging
    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this](const QColor& c){
        if (c.isValid()) {
            mEditor->color()->setFrontColor(c);
        }
    });

    // Commit on release
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this](const QColor& c){
        if (c.isValid()) {
            mEditor->color()->setFrontColor(c);
        }
    });

    popup->showAt(QCursor::pos());
}


void MainWindow2::clearKeyboardShortcuts()
{
    QList<QAction*> actionList = findChildren<QAction*>();
    for (QAction* action : actionList)
    {
        action->setShortcut(QKeySequence(0));
    }
}

void MainWindow2::applyLassoRestriction(bool restricted)
{
    if (mApplyingLassoRestriction) return; // guard re-entrancy
    mApplyingLassoRestriction = true;
    // Track transition for one-time popup when activating
    const bool wasActive = mLassoRestrictionActive;
    Q_UNUSED(wasActive);
    mLassoRestrictionActive = restricted;
    if (restricted) { mRestrictionHintShown = false; }

    // Disable the Timeline dock and all of its child controls explicitly
    if (mTimeLine) {
        mTimeLine->setDisabled(restricted);
        if (QAction* tlToggle = mTimeLine->toggleViewAction()) {
            tlToggle->setEnabled(!restricted ? true : false);
        }
    }

    // Restrict the CosmicLayersPane — disables layer switching, add/delete,
    // opacity, tint, and drag reorder while a lasso selection is active.
    if (auto* layersPane = findChild<CosmicLayersPane*>()) {
        layersPane->setLassoRestricted(restricted);
    }

    QList<QAction*> whitelist;
    whitelist << ui->actionDeselect_All;
    whitelist << ui->actionUndo << ui->actionRedo
              << ui->actionCut << ui->actionCopy
              << ui->actionPaste << ui->actionPaste_Previous;
    whitelist << ui->actionMove << ui->actionSelect << ui->actionPolyline << ui->actionPen
              << ui->actionHand << ui->actionBucket << ui->actionEyedropper << ui->actionEraser;
    if (mActionCurve) whitelist << mActionCurve;
    if (mActionBSpline) whitelist << mActionBSpline;
    if (mActionRectGapFill) whitelist << mActionRectGapFill;
    if (mActionLassoGapFill) whitelist << mActionLassoGapFill;
    if (mActionLassoSelect) whitelist << mActionLassoSelect;
    if (mActionDplate) whitelist << mActionDplate;

    QList<QAction*> allActions = findChildren<QAction*>();
    if (restricted) {
        for (QAction* a : allActions) {
            if (!a) continue;
            if (mTimeLine && a == mTimeLine->toggleViewAction()) { a->setEnabled(false); continue; }
            if (whitelist.contains(a)) { a->setEnabled(true); continue; }
            a->setEnabled(false);
        }
    } else {
        if (mTimeLine) {
            mTimeLine->setDisabled(false);
            if (QAction* tlToggle = mTimeLine->toggleViewAction()) {
                tlToggle->setEnabled(true);
            }
        }
        for (QAction* a : allActions) {
            if (!a) continue;
            a->setEnabled(true);
        }
        updateLayerMenu();
        updateSaveState();
        updateBackupActionState();
        mRestrictionHintShown = false;
    }
    mApplyingLassoRestriction = false;
}

void MainWindow2::applyRoundedMenuMask(QMenu* menu)
{
    if (!menu) return;
    const QRect r = menu->rect();
    if (!r.isValid() || r.width() <= 0 || r.height() <= 0) return;
    menu->setAttribute(Qt::WA_TranslucentBackground, true);
    const qreal desiredRadius = 10.0; 
    const qreal maxRadius = qreal(std::min(r.width(), r.height())) / 2.0;
    const qreal radius = qMax<qreal>(3.0, qMin(desiredRadius, maxRadius));

    QBitmap maskBitmap(r.size());
    maskBitmap.fill(Qt::color0); 
    QPainter p(&maskBitmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::color1); 
    const QRectF drawRect(0.5, 0.5, qreal(r.width()) - 1.0, qreal(r.height()) - 1.0);
    p.drawRoundedRect(drawRect, radius, radius);
    p.end();
    menu->setMask(maskBitmap);
}

QMenu* MainWindow2::createPopupMenu()
{
    QMenu* baseMenu = QMainWindow::createPopupMenu();
    if (!baseMenu) {
        return nullptr;
    }

    // Style the dock panel popup menu to match the Cosmic aesthetic using
    // inline stylesheet — no dependency on CosmicContextMenu.
    const QColor inner(0x2f, 0x2f, 0x2f);
    const QColor border = CosmicColor::popupBorder();
    const QColor accent = BlenderStyleSlider::getAccentColor();
    baseMenu->setStyleSheet(QString(
        "QMenu { background:%1; border:2px solid %2; padding:4px 0; font-size:13px; }"
        "QMenu::item { background:transparent; padding:6px 24px; min-height:28px; }"
        "QMenu::item:selected { background:%3; color:#fff; }"
        "QMenu::item:checked { background:%3; color:#fff; }"
        "QMenu::separator { height:1px; background:%2; margin:4px 0; }"
    ).arg(inner.name(), border.name(), accent.name()));

    return baseMenu;
}

bool MainWindow2::eventFilter(QObject* obj, QEvent* event)
{
#ifdef Q_OS_MACOS
    // Qt's logical key changes with the active input layout (for example Z becomes
    // Cyrillic Я). Route Command+physical-Z directly so Undo/Redo remains layout-independent.
    if (event && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        QWidget* target = qobject_cast<QWidget*>(obj);
        const bool belongsToThisWindow = target && target->window() == this;
        QWidget* focus = QApplication::focusWidget();
        const bool editingText = qobject_cast<QLineEdit*>(focus) ||
                                 qobject_cast<QPlainTextEdit*>(focus) ||
                                 qobject_cast<QTextEdit*>(focus);
        bool pasteClipboard = false;
        if (belongsToThisWindow && !editingText && ui && isMacClipboardShortcut(keyEvent, &pasteClipboard)) {
            if (event->type() == QEvent::ShortcutOverride) {
                keyEvent->accept(); return true;
            }
            QAction* action = pasteClipboard ? ui->actionPaste : ui->actionCopy;
            if (ui->scribbleArea && ui->scribbleArea->isPointerInUse()) {
                qWarning() << "[TimelineClipboard] Shortcut blocked during drawing";
            } else if (action && action->isEnabled()) {
                qInfo() << "[TimelineClipboard] Physical macOS shortcut" << (pasteClipboard ? "Paste" : "Copy")
                        << "logical key" << keyEvent->key();
                action->trigger();
            } else {
                qWarning() << "[TimelineClipboard] Shortcut unavailable:" << (pasteClipboard ? "Paste" : "Copy");
            }
            keyEvent->accept(); return true;
        }
        if (belongsToThisWindow && !editingText && isMacCommandZ(keyEvent)) {
            // Let the canvas retain its in-progress stroke/lasso cancellation behavior.
            if (ui && (!ui->scribbleArea || !ui->scribbleArea->isPointerInUse())) {
                if (event->type() == QEvent::ShortcutOverride) {
                    keyEvent->accept();
                    return true;
                }
                mEditor->updateUndoContext();
                mEditor->undoRedo()->updateActions();
                const bool redo = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
                QAction* action = redo ? ui->actionRedo : ui->actionUndo;
                if (action && action->isEnabled()) {
                    qInfo() << "[KeyboardShortcuts] macOS layout-independent"
                            << (redo ? "Redo" : "Undo")
                            << "triggered; nativeVirtualKey=" << keyEvent->nativeVirtualKey()
                            << "logicalKey=" << keyEvent->key();
                    action->trigger();
                } else {
                    qWarning() << "[KeyboardShortcuts] Command+Z shortcut matched, but action is unavailable"
                               << (redo ? "Redo" : "Undo");
                }
                keyEvent->accept();
                return true;
            }
        }

        bool goNextKeyframe = false;
        if (belongsToThisWindow && !editingText && ui &&
            isMacKeyframeNavigationShortcut(keyEvent, &goNextKeyframe)) {
            // Claim the physical key before Qt can trigger a layout-dependent
            // QAction shortcut. Dispatch only on KeyPress, exactly once.
            if (event->type() == QEvent::ShortcutOverride) {
                keyEvent->accept();
                return true;
            }
            if ((ui->scribbleArea && ui->scribbleArea->isPointerInUse()) ||
                mEditor->shiftTrace()->activeMoveEntryId() != -1) {
                qInfo() << "[KeyboardShortcuts] Drawing navigation blocked during an active edit";
                keyEvent->accept();
                return true;
            }
            QAction* action = goNextKeyframe ? ui->actionNext_KeyFrame : ui->actionPrev_KeyFrame;
            if (action && action->isEnabled()) {
                qInfo() << "[KeyboardShortcuts] macOS layout-independent keyframe navigation"
                        << (goNextKeyframe ? "next" : "previous")
                        << "triggered; nativeVirtualKey=" << keyEvent->nativeVirtualKey()
                        << "logicalKey=" << keyEvent->key()
                        << "modifiers=" << keyEvent->modifiers();
                action->trigger();
            } else {
                qWarning() << "[KeyboardShortcuts] Keyframe navigation shortcut matched, but action is unavailable"
                           << (goNextKeyframe ? "next" : "previous");
            }
            keyEvent->accept();
            return true;
        }
    }
#endif
    // Neutralise any phantom QMainWindow toolbar-area widgets that appear
    // after startup and overlap CosmicMenuBar. We catch them on first
    // MouseButtonPress so the very click that would have been swallowed
    // instead reaches CosmicMenuBar on the next press.
    // IMPORTANT: only check direct children of this MainWindow — deep
    // descendants (ReferenceCanvas, scroll viewports, etc.) are unnamed too
    // and must never be marked transparent.
    if (mCosmicMenuBar && event && event->type() == QEvent::MouseButtonPress) {
        if (QWidget* w = qobject_cast<QWidget*>(obj)) {
            if (w->objectName().isEmpty() &&
                w != mCosmicMenuBar && !mCosmicMenuBar->isAncestorOf(w) &&
                w->parent() == this) {   // direct child of MainWindow only
                const QRect& mr = mCosmicMenuBar->rect();
                const QRect menuGlobal(mCosmicMenuBar->mapToGlobal(mr.topLeft()),
                                       mCosmicMenuBar->mapToGlobal(mr.bottomRight()));
                const QRect& wr = w->rect();
                const QRect wGlobal(w->mapToGlobal(wr.topLeft()),
                                    w->mapToGlobal(wr.bottomRight()));
                if (wGlobal.intersects(menuGlobal)) {
                    w->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                }
            }
        }
    }

    if (ui && ui->background && obj == ui->background && event) {
        if (event->type() == QEvent::Resize) {
            updateCosmicToolsBarsPosition();
            updateCanvasPalettePosition();
        }
    }

    // Suppress Qt's default docking rubber band highlight so that only the
    // reserved dock space is used as a visual indicator when dragging
    // docks. This keeps the drop target area at the normal dock
    // background color instead of adding an extra overlay.
    if (auto* rubberBand = qobject_cast<QRubberBand*>(obj)) {
        // Limit this to dock-related rubber bands by checking for the
        // top-level parent being this main window. This avoids
        // interfering with any other potential QRubberBand usages.
        QWidget* top = rubberBand->parentWidget();
        while (top && top->parentWidget()) {
            top = top->parentWidget();
        }
        if (top == this) {
            switch (event->type()) {
            case QEvent::Show:
            case QEvent::Paint:
            case QEvent::Resize:
                rubberBand->hide();
                return true; // consume visual updates
            default:
                break;
            }
        }
    }

    // Block Qt's default "Alt focuses menubar" accelerator at the shortcut
    // override stage, so the File menu doesn't react to plain Alt. Normal
    // key events (KeyPress/KeyRelease) still reach ScribbleArea and other
    // widgets, preserving the Alt-based canvas eyedropper behavior.
    if (event->type() == QEvent::ShortcutOverride) {
        if (auto* ke = dynamic_cast<QKeyEvent*>(event)) {
            const bool isPlainAlt = (ke->key() == Qt::Key_Alt) &&
                                    (ke->modifiers() == Qt::AltModifier);
            if (isPlainAlt) {
                // Accept the shortcut override so Qt does not treat plain Alt
                // as a menubar accelerator, but do not filter out the event.
                // This allows the subsequent KeyPress/KeyRelease events for
                // Alt to still reach widgets like ScribbleArea.
                event->accept();
            }
        }
    }

    if (QMenu* menu = qobject_cast<QMenu*>(obj)) {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
            applyRoundedMenuMask(menu);
        }
    }

    if (mCanvasGalleryOverlay && mCanvasGalleryOverlay->isVisible()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke && ke->key() == Qt::Key_Escape) {
                if (mReferenceWindow) mReferenceWindow->triggerGalleryToggle(false);
                return true; 
            }
        }
        QWidget* w = qobject_cast<QWidget*>(obj);
        if (!w) return QMainWindow::eventFilter(obj, event);
        bool allow = (w == mCanvasGalleryOverlay) || mCanvasGalleryOverlay->isAncestorOf(w);
        if (!allow && mReferenceWindow) {
            QWidget* tog = mReferenceWindow->galleryToggleWidget();
            if (tog) allow = (w == tog) || tog->isAncestorOf(w);
        }
        // Allow interaction with any modal dialog (e.g. CosmicConfirmPopup) opened
        // from within the gallery overlay — it's a separate top-level window so it
        // won't be a descendant of mCanvasGalleryOverlay.
        if (!allow) {
            QWidget* top = w->window();
            if (top && top != this && qobject_cast<QDialog*>(top) && top->isVisible())
                allow = true;
        }
        if (!allow) {
            switch (event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::Wheel:
            case QEvent::ContextMenu:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::Gesture:
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd:
                return true; 
            default: break;
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    if (!mLassoRestrictionActive) return QMainWindow::eventFilter(obj, event);

    auto isTimelineDescendant = [this](QObject* o) -> bool {
        if (!mTimeLine) return false;
        if (o == mTimeLine) return true;
        QWidget* w = qobject_cast<QWidget*>(o);
        return w && mTimeLine->isAncestorOf(w);
    };

    auto isLayersPaneDescendant = [this](QObject* o) -> bool {
        auto* layersPane = findChild<CosmicLayersPane*>();
        if (!layersPane) return false;
        if (o == layersPane) return true;
        QWidget* w = qobject_cast<QWidget*>(o);
        return w && layersPane->isAncestorOf(w);
    };

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::ContextMenu:
        if (isTimelineDescendant(obj) || isLayersPaneDescendant(obj)) {
            maybeShowRestrictionHint();
            return true;
        }
        break;
    default: break;
    }

    if (event->type() == QEvent::Shortcut) {
        auto* se = static_cast<QShortcutEvent*>(event);
        QKeySequence pressed = se->key();

        QList<QAction*> allowed;
        allowed << ui->actionDeselect_All;
        if (!hasFloatingSelection()) {
            allowed << ui->actionUndo << ui->actionRedo;
        }
        allowed << ui->actionCut << ui->actionCopy
                << ui->actionPaste << ui->actionPaste_Previous
                << ui->actionMove << ui->actionSelect << ui->actionPolyline << ui->actionPen
                << ui->actionHand << ui->actionBucket << ui->actionEyedropper << ui->actionEraser;
        if (mActionCurve) allowed << mActionCurve;
        if (mActionBSpline) allowed << mActionBSpline;
        if (mActionRectGapFill) allowed << mActionRectGapFill;
        if (mActionLassoGapFill) allowed << mActionLassoGapFill;
        if (mActionLassoSelect) allowed << mActionLassoSelect;
        if (mActionDplate) allowed << mActionDplate;

        bool shortcutAllowed = false;
        for (QAction* a : allowed) {
            if (!a) continue;
            for (const QKeySequence& ks : a->shortcuts()) {
                if (!ks.isEmpty() && ks == pressed) { shortcutAllowed = true; break; }
            }
            if (shortcutAllowed) break;
        }

        if (!shortcutAllowed) {
            maybeShowRestrictionHint();
            return true; 
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow2::maybeShowRestrictionHint()
{
    CosmicConfirmPopup popup(this);
    popup.setBodyText(tr("You have an active lasso selection.\n\nApply the transform now so you can continue, or go back and finish your selection first."));
    popup.setConfirmText(tr("Apply"));
    popup.setCancelText(tr("Go Back"));
    popup.setCenterOnShow(true);

    if (popup.exec() == QDialog::Accepted) {
        // Commit the lasso — same path as pressing Enter in LassoSelectTool
        if (mEditor && mEditor->tools()) {
            mEditor->tools()->leavingThisTool();
        }
        if (mEditor) {
            mEditor->deselectAll();
        }
    }
    // "Go Back" — popup closes, user continues with the lasso unchanged.
}

void MainWindow2::exportPalette()
{
    QString filePath = FileDialog::getSaveFileName(this, FileType::PALETTE);
    if (!filePath.isEmpty())
    {
        mEditor->object()->exportPalette(filePath);
    }
}

void MainWindow2::importPalette()
{
    QString filePath = FileDialog::getOpenFileName(this, FileType::PALETTE);
    if (!filePath.isEmpty())
    {
        mEditor->object()->importPalette(filePath);
        mColorPalette->refreshColorList();
        mColorPalette->adjustSwatches();
        if (mEditor->object()->getColorCount() > 0)
        {
            mEditor->color()->setColorNumber(0);
        }
        else
        {
            mEditor->color()->clearSelection();
        }
        mColorPalette->resetPresetSelection();
    }
}

void MainWindow2::openPalette()
{
    for (int i = 0; i < mEditor->object()->getColorCount(); i++)
    {
        if (!mEditor->object()->isColorInUse(i))
        {
            continue;
        }

        QMessageBox msgBox;
        msgBox.setText(tr("Opening a palette will replace the old palette.\n"
                          "Color(s) in strokes will be altered by this action!"));
        msgBox.addButton(tr("Open Palette"), QMessageBox::AcceptRole);
        msgBox.addButton(QMessageBox::Cancel);

        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
        break;
    }

    QString filePath = FileDialog::getOpenFileName(this, FileType::PALETTE);
    if (filePath.isEmpty())
    {
        return;
    }

    mEditor->object()->openPalette(filePath);
    mColorPalette->refreshColorList();
    if (mEditor->object()->getColorCount() > 0)
    {
        mEditor->color()->setColorNumber(0);
    }
    else
    {
        mEditor->color()->clearSelection();
    }
    mColorPalette->resetPresetSelection();
}

void MainWindow2::makeConnections(Editor* editor)
{
    connect(editor->undoRedo(), &UndoRedoManager::didUpdateUndoStack, this, &MainWindow2::updateSaveState);
    connect(editor->undoRedo(), &UndoRedoManager::didUpdateUndoStack, this, &MainWindow2::updateBackupActionState);
    connect(editor->undoRedo(), &UndoRedoManager::didUpdateUndoStack, this, [this]() {
        if (ui && ui->scribbleArea) {
            ui->scribbleArea->updateFrame();
        }
    });
    connect(editor, &Editor::needDisplayInfo, this, &MainWindow2::displayMessageBox);
    connect(editor, &Editor::needDisplayInfoNoTitle, this, &MainWindow2::displayMessageBoxNoTitle);
    connect(editor->layers(), &LayerManager::currentLayerChanged, this, &MainWindow2::currentLayerChanged);
    connect(editor->select(), &SelectionManager::selectionChanged, this, &MainWindow2::selectionChanged);
    connect(editor, &Editor::canCopyChanged, this, [=](bool canCopy) {
        ui->actionCopy->setEnabled(canCopy);
        ui->actionCut->setEnabled(canCopy);
    });
    connect(editor, &Editor::canPasteChanged, ui->actionPaste, &QAction::setEnabled);
    
    connect(editor->preference(), &PreferenceManager::optionChanged, this, &MainWindow2::onColorPreferenceChanged);

    // Apply window close style/position to all live headers whenever the setting changes
    connect(editor->preference(), &PreferenceManager::optionChanged, this, [](SETTING s) {
        if (s == SETTING::WINDOW_CLOSE_STYLE || s == SETTING::WINDOW_CLOSE_ON_LEFT) {
            QSettings prefs(PENCIL2D, PENCIL2D);
            const int  style     = prefs.value(QStringLiteral("WindowCloseStyle"),  0).toInt();
            const bool closeLeft = prefs.value(QStringLiteral("WindowCloseOnLeft"), true).toBool();
            CosmicWindowHeader::applyStyleToAll(style, closeLeft);
            TitleBarWidget::applyStyleToAll(style, closeLeft);
        }
    });
}

void MainWindow2::makeConnections(Editor* editor, ColorBox* colorBox)
{
    connect(editor->color(), &ColorManager::colorChanged, colorBox, &ColorBox::setColor);
}

void MainWindow2::makeConnections(Editor* editor, ColorInspector* colorInspector)
{
    connect(colorInspector, &ColorInspector::colorChanged, editor->color(), &ColorManager::setFrontColor);
    connect(editor->color(), &ColorManager::colorChanged, colorInspector, &ColorInspector::setColor);
}

void MainWindow2::makeConnections(Editor* editor, ScribbleArea* scribbleArea)
{
    connect(editor->tools(), &ToolManager::toolChanged, scribbleArea, &ScribbleArea::updateToolCursor);
    connect(editor->tools(), &ToolManager::toolChanged, mToolBox, &ToolBoxDockWidget::setActiveTool);
    connect(editor->tools(), &ToolManager::toolPropertyChanged, scribbleArea, &ScribbleArea::updateToolCursor);

    // Locked paper warning popup
    connect(scribbleArea, &ScribbleArea::lockedLayerAttempted, this, [this]() {
        // Guard: only show once — don't stack popups
        if (findChild<CosmicConfirmPopup*>(QStringLiteral("lockedLayerPopup")))
            return;
        CosmicConfirmPopup* popup = new CosmicConfirmPopup(this);
        popup->setObjectName(QStringLiteral("lockedLayerPopup"));
        popup->setBodyText(tr("This paper is locked.\nDo you want to unlock it?"));
        popup->setConfirmText(tr("Unlock"));
        popup->setCancelText(tr("Cancel"));
        if (popup->exec() == QDialog::Accepted) {
            // Unlock the active inner layer
            if (mEditor) {
                Layer* lay = mEditor->layers()->currentLayer();
                if (lay && lay->type() == Layer::BITMAP) {
                    auto* bl = static_cast<LayerBitmap*>(lay);
                    const int fr = mEditor->currentFrame();
                    KeyFrame* kf = bl->keyExists(fr) ? bl->getKeyFrameAt(fr)
                                                     : bl->getLastKeyFrameAtPosition(fr);
                    if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf)) {
                        CellLayerStack* st = bl->cellBank() ? bl->cellBank()->getCellLayerStack(bkf->cellId()) : nullptr;
                        if (st && st->activeLayer()) {
                            st->activeLayer()->locked = false;
                            // Notify the layers pane to refresh its lock toggle visuals
                            emit mEditor->frameModified(mEditor->currentFrame());
                        }
                    }
                }
            }
        }
        popup->deleteLater();
    });


    connect(editor->layers(), &LayerManager::currentLayerChanged, scribbleArea, &ScribbleArea::onLayerChanged);
    connect(editor->layers(), &LayerManager::layerDeleted, scribbleArea, &ScribbleArea::onLayerChanged);
    connect(editor, &Editor::scrubbed, scribbleArea, &ScribbleArea::onScrubbed);
    connect(editor, &Editor::frameModified, scribbleArea, &ScribbleArea::onFrameModified);
    connect(editor, &Editor::framesModified, scribbleArea, &ScribbleArea::onFramesModified);
    connect(editor, &Editor::objectLoaded, scribbleArea, &ScribbleArea::onObjectLoaded);
    connect(editor->view(), &ViewManager::viewChanged, scribbleArea, &ScribbleArea::onViewChanged);
}

void MainWindow2::makeConnections(Editor* pEditor, TimeLine* pTimeline)
{
    PlaybackManager* pPlaybackManager = pEditor->playback();
    connect(pTimeline, &TimeLine::soundClick, pPlaybackManager, &PlaybackManager::enableSound);
    connect(pTimeline, &TimeLine::fpsChanged, pPlaybackManager, &PlaybackManager::setFps);
    connect(pTimeline, &TimeLine::fpsChanged, pEditor, &Editor::setFps);

    connect(pTimeline, &TimeLine::insertKeyClick, mCommands, &ActionCommands::insertKeyFrameAtCurrentPosition);
    connect(pTimeline, &TimeLine::removeKeyClick, mCommands, &ActionCommands::removeKey);
    connect(pTimeline, &TimeLine::makeUniqueDrawingClick, mCommands, &ActionCommands::makeUniqueDrawing);

    connect(pTimeline, &TimeLine::newBitmapLayer, mCommands, &ActionCommands::addNewBitmapLayer);
    // Removed: newSoundLayer connection
    connect(mTimeLine, &TimeLine::playButtonTriggered, this, &MainWindow2::onPlayTriggered);
    connect(pTimeline, &TimeLine::deleteCurrentLayerClick, mCommands, &ActionCommands::deleteCurrentLayer);

    // Clipboard state handling
    connect(QApplication::clipboard(), &QClipboard::dataChanged, mEditor, &Editor::clipboardChanged);
    connect(pTimeline, &TimeLine::selectionChanged, this, &MainWindow2::updateCopyCutPasteEnabled);
    connect(this, &MainWindow2::windowActivated, this, &MainWindow2::updateCopyCutPasteEnabled);
    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &MainWindow2::updateCopyCutPasteEnabled);
    connect(mEditor, &Editor::selectedFramesChanged, this, &MainWindow2::updateCopyCutPasteEnabled);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget*) {
        mEditor->updateUndoContext();
    });

    connect(pEditor->layers(), &LayerManager::currentLayerChanged, pTimeline, &TimeLine::updateUI);
    connect(pEditor->layers(), &LayerManager::animationLengthChanged, pTimeline, &TimeLine::extendLength);
    connect(pEditor->sound(), &SoundManager::soundClipDurationChanged, pTimeline, &TimeLine::updateUI);

    // Menu UI changes
    connect(ui->menuEdit, &QMenu::aboutToShow, this, &MainWindow2::updateCopyCutPasteEnabled);

    connect(pEditor, &Editor::objectLoaded, pTimeline, &TimeLine::onObjectLoaded);
    connect(pEditor, &Editor::updateTimeLine, pTimeline, &TimeLine::updateUI);
    connect(pEditor, &Editor::updateTimeLineCached, pTimeline, &TimeLine::updateUICached);

    connect(pEditor->layers(), &LayerManager::currentLayerChanged, this, &MainWindow2::updateLayerMenu);
    connect(pEditor->layers(), &LayerManager::currentLayerChanged, mToolOptions, &ToolOptionWidget::updateUI);
}

void MainWindow2::makeConnections(Editor* editor, ToolOptionWidget* toolOptions)
{
    toolOptions->makeConnectionToEditor(editor);
}

void MainWindow2::makeConnections(Editor* pEditor, ColorPaletteWidget* pColorPalette)
{
    connect(pEditor, &Editor::objectLoaded, pColorPalette, &ColorPaletteWidget::updateUI);

    ColorManager* pColorManager = pEditor->color();
    ScribbleArea* pScribbleArea = pEditor->getScribbleArea();

    connect(pColorPalette, &ColorPaletteWidget::colorNumberChanged, pColorManager, &ColorManager::setColorNumber);
    connect(pColorManager, &ColorManager::colorNumberChanged, pScribbleArea, &ScribbleArea::paletteColorChanged);
    connect(pColorManager, &ColorManager::colorNumberChanged, pColorPalette, &ColorPaletteWidget::selectColorNumber);
}

void MainWindow2::updateCopyCutPasteEnabled()
{
    bool canCopy = mEditor->canCopy();
    bool canPaste = mEditor->canPaste();

    ui->actionCopy->setEnabled(canCopy);
    ui->actionCut->setEnabled(canCopy);
    ui->actionPaste->setEnabled(canPaste);
}

void MainWindow2::onColorPreferenceChanged(SETTING setting)
{
    switch (setting) {
    case SETTING::ACCENT_COLOR:
        // Update global Qt palette highlight color from fixed accent
        if (mEditor && mEditor->preference()) {
            const QColor accent = BlenderStyleSlider::getAccentColor();

            QPalette pal = qApp->palette();
            pal.setColor(QPalette::Highlight, accent);
            pal.setColor(QPalette::HighlightedText,
                         accent.lightness() > 140 ? QColor("#111111") : QColor("#ffffff"));
            qApp->setPalette(pal);
        }

        // Update all BlenderStyleSliders throughout the app
        updateAllBlenderStyleSliders();
        // Update menu styles to use new accent color
        updateMenuStyles();
        // Update all Blender-style components throughout the app
        {
            const QColor accent = BlenderStyleSlider::getAccentColor();
            const QList<BlenderStyleDropDown*> dds = findChildren<BlenderStyleDropDown*>();
            for (BlenderStyleDropDown* dd : dds) {
                if (dd) dd->setAccentColor(accent);
            }
            const QList<CosmicDropDown*> cosmicDds = findChildren<CosmicDropDown*>();
            for (CosmicDropDown* cdd : cosmicDds) {
                if (cdd) cdd->setAccentColor(accent);
            }
            const QList<BlenderStyleToggle*> toggles = findChildren<BlenderStyleToggle*>();
            for (BlenderStyleToggle* toggle : toggles) {
                if (toggle) toggle->setAccentColor(accent);
            }
            const QList<BlenderStyleComboBox*> combos = findChildren<BlenderStyleComboBox*>();
            for (BlenderStyleComboBox* combo : combos) {
                if (combo) combo->setAccentColor(accent);
            }
            // Tab bar visuals are controlled by the global stylesheet.
        }
        break;
    case SETTING::TOOLBOX_SELECTION_COLOR:
        // Update toolbox styles - the ToolBoxDockWidget contains a ToolBoxWidget
        if (mToolBox && mToolBox->getToolBoxWidget()) {
            mToolBox->getToolBoxWidget()->onColorPreferenceChanged(setting);
        }

        // Treat toolbox selection color as the app's effective accent/highlight.
        // This keeps Cosmic widgets (and any palette-highlight driven UI) in sync
        // with the Tools dock active highlight.
        if (mEditor && mEditor->preference()) {
            const int toolboxRgba = mEditor->preference()->getInt(SETTING::TOOLBOX_SELECTION_COLOR);
            const QColor toolboxColor = QColor::fromRgba(toolboxRgba);
            if (toolboxColor.isValid()) {
                QPalette pal = qApp->palette();
                pal.setColor(QPalette::Highlight, toolboxColor);
                pal.setColor(QPalette::HighlightedText,
                             toolboxColor.lightness() > 140 ? QColor("#111111") : QColor("#ffffff"));
                qApp->setPalette(pal);
            }
        }

        // Update all toolbar buttons with new toolbox selection color
        applyAccentColorToToolbars();
        break;
    case SETTING::TOOL_PALETTE_SELECTION_COLOR:
        // Update canvas palette overlay
        if (mCanvasPaletteOverlay && mCanvasPaletteOverlay->getPalette()) {
            mCanvasPaletteOverlay->getPalette()->onColorPreferenceChanged(setting);
        }
        break;
    case SETTING::UI_TITLE_BAR_COLOR:
    case SETTING::UI_DOCK_CONTENT_COLOR:
    case SETTING::UI_SCROLLBAR_COLOR:
        // Update UI theme colors
        updateUIThemeColors();
        break;
    default:
        break;
    }
}

void MainWindow2::updateAllBlenderStyleSliders()
{
    // Find and update all BlenderStyleSliders in the application
    QList<BlenderStyleSlider*> sliders = findChildren<BlenderStyleSlider*>();
    for (BlenderStyleSlider* slider : sliders) {
        slider->updateAccentColor();
    }
}

void MainWindow2::applyAccentColorToToolbars()
{
    // Apply toolbox selection color to all toolbar buttons
    if (!mEditor || !mEditor->preference()) return;
    
    const int toolboxColorRgba = mEditor->preference()->getInt(SETTING::TOOLBOX_SELECTION_COLOR);
    const QColor toolboxColor = QColor::fromRgba(toolboxColorRgba);
    
    auto rgbaInt = [&](int a0to255) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(toolboxColor.red())
            .arg(toolboxColor.green())
            .arg(toolboxColor.blue())
            .arg(qBound(0, a0to255, 255));
    };

    const QString colorHexAlpha = rgbaInt(31);          // ~0.12
    const QString colorHexAlphaHover = rgbaInt(71);     // ~0.28
    const QString colorHexAlphaChecked = rgbaInt(115);  // ~0.45
    const QString colorHexBorder = rgbaInt(89);         // ~0.35
    const QString colorHexBorderChecked = rgbaInt(217); // ~0.85
    const QString colorHexBorderCheckedHover = rgbaInt(255);
    
    QList<QToolBar*> toolbars = findChildren<QToolBar*>();
    for (QToolBar* toolbar : toolbars) {
        if (toolbar) {
            QString toolbarStyle = QString(
                "QToolButton:hover { background: %1; border-color: %2; }"
                "QToolButton:checked { background: qradialgradient(cx:0.5, cy:0.5, radius:0.6, fx:0.5, fy:0.5, stop:0 %3, stop:0.55 %4, stop:1 rgba(0,0,0,0)); border-color: %5; }"
                "QToolButton:checked:hover { background: %6; border-color: %7; }"
            ).arg(colorHexAlpha, colorHexBorder, colorHexAlphaChecked, colorHexAlphaChecked, 
                  colorHexBorderChecked, colorHexAlphaHover, colorHexBorderCheckedHover);
            toolbar->setStyleSheet(toolbarStyle);
        }
    }
}

void MainWindow2::updateUIThemeColors()
{
    if (!mEditor || !mEditor->preference()) return;
    
    // Get colors from preferences
    const int titleBarRgba = mEditor->preference()->getInt(SETTING::UI_TITLE_BAR_COLOR);
    const int dockContentRgba = mEditor->preference()->getInt(SETTING::UI_DOCK_CONTENT_COLOR);
    const int scrollbarRgba = mEditor->preference()->getInt(SETTING::UI_SCROLLBAR_COLOR);
    
    const QColor titleBarColor = QColor::fromRgba(titleBarRgba);
    const QColor dockContentColor = QColor::fromRgba(dockContentRgba);
    const QColor scrollbarColor = QColor::fromRgba(scrollbarRgba);
    
    // Update QPalette colors
    QPalette pal = qApp->palette();
    pal.setColor(QPalette::Base, dockContentColor);
    pal.setColor(QPalette::AlternateBase, dockContentColor);
    pal.setColor(QPalette::Button, dockContentColor);
    qApp->setPalette(pal);
    
    // Build dynamic stylesheet for UI theme colors
    QString dynamicStyles = QString(
        "QMainWindow#MainWindow2 QDockWidget { background: %1; }\n"
        "QMainWindow#MainWindow2 QDockWidget > QWidget { background: %1; }\n"
        "QMainWindow#MainWindow2 QScrollArea { background: %1; }\n"
        "QMainWindow#MainWindow2 QScrollArea QWidget { background: %1; }\n"
        "QMainWindow#MainWindow2 QFrame { background: %1; border-color: #1f1f1f; }\n"
        "QMainWindow#MainWindow2 QGroupBox { background: %1; color: #e6e6e6; border: 1px solid #3a3a3a; margin-top: 8px; }\n"
        "QMainWindow#MainWindow2 QAbstractItemView { background: #2f2f2f; color: #e6e6e6; alternate-background-color: %1; }\n"
        "QMainWindow#MainWindow2 QTabWidget::pane { border: 1px solid #3a3a3a; background: %1; }\n"
        "QMainWindow#MainWindow2 QDockWidget::title { background: %2; color: #e6e6e6; padding-left: 6px; }\n"
        "QMainWindow#MainWindow2 QWidget#TitleBarWidget,\n"
        "QMainWindow#MainWindow2 QWidget[objectName^=\"TitleBar\"] { background: %2; }\n"
        "QMainWindow#MainWindow2 QScrollBar::handle:vertical,\n"
        "QMainWindow#MainWindow2 QScrollBar::handle:horizontal { background: %3; }\n"
    ).arg(dockContentColor.name(), titleBarColor.name(), scrollbarColor.name());
    
    // Don't reload the stylesheet here - instead just append the dynamic styles
    // This preserves any accent color menu styles that were already applied
    QString currentQss = this->styleSheet();
    
    // Remove any previous UI theme color overrides (lines starting with our markers)
    QStringList lines = currentQss.split('\n');
    QStringList filteredLines;
    bool inUIThemeBlock = false;
    for (const QString& line : lines) {
        if (line.contains("/* UI_THEME_COLORS_START */")) {
            inUIThemeBlock = true;
            continue;
        }
        if (line.contains("/* UI_THEME_COLORS_END */")) {
            inUIThemeBlock = false;
            continue;
        }
        if (!inUIThemeBlock) {
            filteredLines.append(line);
        }
    }
    
    // Add new UI theme color overrides with markers
    QString newQss = filteredLines.join('\n');
    newQss += "\n/* UI_THEME_COLORS_START */\n";
    newQss += dynamicStyles;
    newQss += "/* UI_THEME_COLORS_END */\n";

    qWarning() << "[QSS] MainWindow2 applying UI theme dynamicStyles";
    this->setStyleSheet(newQss);
    
    // Update timeline-specific widgets
    if (mTimeLine) {
        // Update layers area
        if (mTimeLine->findChild<QWidget*>("mLayerList")) {
            mTimeLine->findChild<QWidget*>("mLayerList")->setStyleSheet(
                QString("TimeLineCells { background-color: %1; }").arg(titleBarColor.name())
            );
        }
        // Update splitter
        QList<QSplitter*> splitters = mTimeLine->findChildren<QSplitter*>();
        for (QSplitter* splitter : splitters) {
            splitter->setStyleSheet(
                QString("QSplitter::handle { background-color: %1; }").arg(titleBarColor.name())
            );
        }
        // Update toolbar areas
        QList<QWidget*> toolbars = mTimeLine->findChildren<QWidget*>();
        for (QWidget* widget : toolbars) {
            QString objName = widget->objectName();
            if (objName.contains("ToolBar", Qt::CaseInsensitive)) {
                widget->setStyleSheet(QString("QWidget { background-color: %1; }").arg(titleBarColor.name()));
            }
        }
    }
}

void MainWindow2::updateMenuStyles()
{
    if (!mEditor || !mEditor->preference()) return;
    
    const QColor accentColor = BlenderStyleSlider::getAccentColor();
    const QString accentHex = accentColor.name(QColor::HexRgb);
    const QString textColor = QStringLiteral("#ffffff");
    
    // Load the base stylesheet
    QFile darkQss(":/styles/dark_theme.qss");
    if (darkQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString qss = QString::fromUtf8(darkQss.readAll());
        
        // Replace hardcoded menu selection colors with dynamic accent color
        qss.replace("#ffaa40", accentHex);
        
        // Create rgba color strings for toolbar buttons with different alpha values
        auto rgbaInt = [&](int a0to255) {
            return QStringLiteral("rgba(%1, %2, %3, %4)")
                .arg(accentColor.red())
                .arg(accentColor.green())
                .arg(accentColor.blue())
                .arg(qBound(0, a0to255, 255));
        };

        const QString colorHexAlpha = rgbaInt(31);             // ~0.12
        const QString colorHexAlphaHover = rgbaInt(71);        // ~0.28
        const QString colorHexAlphaChecked = rgbaInt(115);     // ~0.45
        const QString colorHexAlphaCheckedHover = rgbaInt(56); // ~0.22
        const QString colorHexBorder = rgbaInt(89);            // ~0.35
        const QString colorHexBorderChecked = rgbaInt(217);    // ~0.85
        const QString colorHexBorderCheckedHover = rgbaInt(255);
        
        // Add dynamic menu and toolbar styling
        QString dynamicMenuStyles = QString(
            "QMainWindow#MainWindow2 QMenu::item:selected { background: %1; color: %2; }"
            "QMainWindow#MainWindow2 QComboBox QAbstractItemView::item:selected { background: %1; color: %2; }"
            "QMainWindow#MainWindow2 QTreeView::item:selected, QMainWindow#MainWindow2 QListView::item:selected, QMainWindow#MainWindow2 QTableView::item:selected { background: %1; color: %2; }"
            "QMenu::item:selected { background: %1; color: %2; }"
            "QAbstractItemView::item:selected { background: %1; color: %2; }"
            "QComboBox QAbstractItemView::item:selected { background: %1; color: %2; }"
            "QMainWindow#MainWindow2 QToolBar QToolButton:hover { background: %3; border-color: %4; }"
            "QMainWindow#MainWindow2 QToolBar QToolButton:checked { background: qradialgradient(cx:0.5, cy:0.5, radius:0.6, fx:0.5, fy:0.5, stop:0 %5, stop:0.55 %6, stop:1 rgba(0,0,0,0)); border-color: %7; }"
            "QMainWindow#MainWindow2 QToolBar QToolButton:checked:hover { background: %8; border-color: %9; }"
        ).arg(accentHex, textColor, colorHexAlpha, colorHexBorder, colorHexAlphaChecked, 
              colorHexAlphaCheckedHover, colorHexBorderChecked, colorHexAlphaHover, colorHexBorderCheckedHover);
        
        qss += dynamicMenuStyles;
        qWarning() << "[QSS] MainWindow2 applying menu/toolbar accent styles";
        this->setStyleSheet(qss);
    }
}

void MainWindow2::updateLayerMenu()
{
    ui->actionDelete_Current_Layer->setEnabled(mEditor->layers()->canDeleteLayer(mEditor->currentLayerIndex()));
}

void MainWindow2::changePlayState(bool isPlaying)
{
    if (isPlaying)
    {
        ui->actionPlay->setText(tr("Stop"));
        ui->actionPlay->setIcon(mStopIcon);
    }
    else
    {
        ui->actionPlay->setText(tr("Play"));
        ui->actionPlay->setIcon(mStartIcon);
    }
    update();
}

void MainWindow2::onPlayTriggered()
{
    if (!mEditor || !mEditor->playback()) {
        return;
    }

    PlaybackManager* playback = mEditor->playback();
    if (playback->isPlaying())
    {
        playback->stop();
        return;
    }

    // Scenes/Shots timeline tab should always start from the first frame.
    if (mTimeLine && mTimeLine->isScenesAndShotsTabActive())
    {
        mEditor->scrubTo(1);
    }

    playback->play();
}

void MainWindow2::importMovieVideo()
{
    // Flag this so we don't prompt the user about auto-save in the middle of the import.
    mSuppressAutoSaveDialog = true;

    mCommands->importMovieVideo();

    mSuppressAutoSaveDialog = false;
}

bool MainWindow2::event(QEvent* event)
{
    if(event->type() == QEvent::WindowActivate) {
        emit windowActivated();
    }
    if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) {
        updateCanvasPalettePosition();
    }
    return QMainWindow::event(event);
}

void MainWindow2::displayMessageBox(const QString& title, const QString& body)
{
    QMessageBox::information(this, tr(qPrintable(title)), tr(qPrintable(body)), QMessageBox::Ok);
}

void MainWindow2::displayMessageBoxNoTitle(const QString& body)
{
    QMessageBox::information(this, nullptr, tr(qPrintable(body)), QMessageBox::Ok);
}

void MainWindow2::updateCanvasPalettePosition()
{
    if (!mCanvasPaletteOverlay) {
        return;
    }
    if (!ui->background) {
        qWarning() << "MainWindow2: ui->background is null";
        return;
    }
    try {
        mCanvasPaletteOverlay->updatePosition();

        // Ensure the Cosmic bars remain interactable even after the palette overlay moves.
        if (mCosmicLeftToolsBar) mCosmicLeftToolsBar->raise();
        if (mCosmicRightToolsBar) mCosmicRightToolsBar->raise();
        if (mCosmicBottomToolsBar) mCosmicBottomToolsBar->raise();
    } catch (const std::exception& e) {
        qCritical() << "MainWindow2: Exception updating canvas palette position:" << e.what();
    }
}

void MainWindow2::createCosmicMenuBar()
{
    // Hide the native menubar visually. Shortcuts are kept alive by the
    // explicit addAction() calls at the end of setupKeyboardShortcuts().
    menuBar()->hide();
    menuBar()->setNativeMenuBar(false);

    mCosmicMenuBar = new CosmicMenuBar(ui->background);

    // Set the decorative logo icon as the first element (Blender-style)
    {
        const int iconH = mCosmicMenuBar->menuBarHeight() - 8; // match bar height minus padding
        QSvgRenderer svgRenderer(QStringLiteral(":/Windows&AppIcons/MenubarIcon 1.svg"));
        if (svgRenderer.isValid()) {
            QSize svgSize = svgRenderer.defaultSize();
            int iconW = (svgSize.height() > 0)
                        ? qRound(svgSize.width() * (double)iconH / svgSize.height())
                        : iconH;
            QPixmap logoPixmap(iconW, iconH);
            logoPixmap.fill(Qt::transparent);
            QPainter logoPainter(&logoPixmap);
            svgRenderer.render(&logoPainter);
            logoPainter.end();
            mCosmicMenuBar->setLogoIcon(logoPixmap);
        }
    }

    mCosmicMenuBar->addEntry(tr("File"),     buildFileMenuWindow(),
                             [this]{ return tr("File"); });
    mCosmicMenuBar->addEntry(tr("Edit"),     buildEditMenuWindow(),
                             [this]{ return tr("Edit"); });
    mCosmicMenuBar->addEntry(tr("View"),     buildViewMenuWindow(),
                             [this]{ return tr("View"); });
    mCosmicMenuBar->addEntry(tr("Canvas"),   buildCanvasMenuWindow(),
                             [this]{ return tr("Canvas"); });
    mCosmicMenuBar->addEntry(tr("Toolbars"), buildWindowsMenuWindow(),
                             [this]{ return tr("Toolbars"); });

    mCosmicMenuBar->setVersionLabel(QStringLiteral("2026"));

    mCosmicControlCenter = new CosmicControlCenter(ui->background);

    updateCosmicToolsBarsPosition();

    // QMainWindow internally creates toolbar-area widgets (separators/grips)
    // even when no toolbars are registered. These phantom widgets can sit
    // directly over CosmicMenuBar (an overlay not in the QMainWindow layout)
    // and intercept mouse events before they reach it. Defer the scan so
    // Qt has finished constructing all internal children.
    QTimer::singleShot(0, this, [this]() {
        if (!mCosmicMenuBar) return;
        const QRect& mr = mCosmicMenuBar->rect();
        const QRect menuGlobal(mCosmicMenuBar->mapToGlobal(mr.topLeft()),
                               mCosmicMenuBar->mapToGlobal(mr.bottomRight()));
        // Only scan DIRECT children of the QMainWindow — the phantom toolbar-area
        // widgets Qt creates internally are always direct children, never deep
        // descendants. Using findChildren<QWidget*>() (recursive) was incorrectly
        // marking deep unnamed widgets (ReferenceCanvas, scroll viewports, etc.)
        // as WA_TransparentForMouseEvents when they hadn't been positioned yet
        // and still sat at (0,0), which overlaps the menu bar rect.
        for (QObject* obj : children()) {
            QWidget* child = qobject_cast<QWidget*>(obj);
            if (!child) continue;
            if (!child->objectName().isEmpty()) continue;
            if (child == mCosmicMenuBar || mCosmicMenuBar->isAncestorOf(child)) continue;
            const QRect& cr = child->rect();
            const QRect childGlobal(child->mapToGlobal(cr.topLeft()),
                                    child->mapToGlobal(cr.bottomRight()));
            if (childGlobal.intersects(menuGlobal)) {
                child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            }
        }
    });
}

void MainWindow2::populateCosmicControlCenter()
{
    if (!mCosmicControlCenter) return;

    auto* cc = mCosmicControlCenter;

    // ── File actions ──────────────────────────────────────────────────────────
    cc->addGroup(QStringLiteral("file_ops"), {
        ui->actionNew,
        ui->actionOpen,
        ui->actionSave
    });

    cc->addSpacer();

    // ── View actions ──────────────────────────────────────────────────────────
    ui->actionReset_Rotation->setIcon(QIcon(QStringLiteral(":/Interaction/ResetShift.svg")));
    cc->addGroup(QStringLiteral("view_ops"), {
        ui->actionZoom_In,
        ui->actionZoom_Out,
        ui->actionReset_View,
        ui->actionReset_Rotation
    });

    cc->addSpacer();

    cc->addGroup(QStringLiteral("flip_ops"), {
        ui->actionHorizontal_Flip,
        ui->actionVertical_Flip
    });

    cc->addSpacer();

    // ── Overlay actions ───────────────────────────────────────────────────────
    ui->actionGoldenRatio->setVisible(false);
    cc->addGroup(QStringLiteral("overlays"), {
        ui->actionCenter,
        ui->actionThirds,
        ui->actionSafeAreas
    });

    cc->addSpacer();

    // ── Pen palette widget (6 cells: 1 custom + 5 fill colors) ───────────────
    if (mPenPaletteToolbarWidget) {
        cc->addWidgetGroup(QStringLiteral("pen_palette"), mPenPaletteToolbarWidget, 6);
    }

    cc->addSpacer();

    // ── Auto fill controls widget (6 cells: 1 master + 5 color toggles) ──────
    if (mAutoFillToolbarWidget) {
        cc->addWidgetGroup(QStringLiteral("autofill"), mAutoFillToolbarWidget, 6);
    }

    cc->addSpacer();
    cc->addSingle(QStringLiteral("undo"),        ui->actionUndo);
    cc->addSingle(QStringLiteral("redo"),        ui->actionRedo);
    cc->addSpacer();
    cc->addSingle(QStringLiteral("clear_frame"), ui->actionClearFrame);

    // Restore any previously saved layout order
    cc->loadLayout();

    // Sync the Toolbars menu toggle actions to match the restored hidden state.
    // The actions were created with checked=true; correct any that are hidden.
    struct { QAction** action; QString groupId; } toggles[] = {
        { &mCCToggleFileOpsAction,    QStringLiteral("file_ops")    },
        { &mCCToggleViewAction,       QStringLiteral("view_ops")    },
        { &mCCToggleOverlaysAction,   QStringLiteral("overlays")    },
        { &mCCToggleUndoRedoAction,   QStringLiteral("undo")        },
        { &mCCToggleClearFrameAction, QStringLiteral("clear_frame") },
    };
    for (const auto& t : toggles) {
        if (*t.action)
            (*t.action)->setChecked(cc->isGroupVisible(t.groupId));
    }
}

CosmicMenuWindow* MainWindow2::buildFileMenuWindow()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionNew);
    w->addAction(ui->actionOpen);
    w->addMenu(tr("Open Recent"), buildRecentSubmenu(),
               [this]{ return tr("Open Recent"); });
    w->addSeparator();
    w->addAction(ui->actionSave);
    w->addAction(ui->actionSave_as);
    w->addSeparator();
    w->addMenu(tr("Import"), buildImportSubmenu(),
               [this]{ return tr("Import"); });
    w->addMenu(tr("Export"), buildExportSubmenu(),
               [this]{ return tr("Export"); });
    w->addSeparator();
    w->addAction(ui->actionExit);
    return w;
}

CosmicMenuWindow* MainWindow2::buildImportSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionImportLayers_from_pclx);
    w->addAction(ui->actionImport_Image);
    w->addAction(ui->actionImport_ImageSeq);
    return w;
}

CosmicMenuWindow* MainWindow2::buildExportSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionExport_Image);
    w->addAction(ui->actionExport_Animation);
    w->addAction(ui->actionExport_Sprite);
    w->addAction(ui->actionExport_Gif);
    return w;
}

CosmicMenuWindow* MainWindow2::buildRecentSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    for (QAction* act : mRecentFileMenu->actions())
        w->addAction(act);
    connect(mRecentFileMenu, &QMenu::aboutToShow, this, [=]() {
        w->clearItems();
        for (QAction* act : mRecentFileMenu->actions())
            w->addAction(act);
    });
    return w;
}

CosmicMenuWindow* MainWindow2::buildEditMenuWindow()
{
    auto* w = new CosmicMenuWindow(this);
    // NOTE: ui->actionUndo and ui->actionRedo are replaced by replaceUndoRedoActions().
    // We store the pointer so we can refresh those two items when the stack switches.
    mCosmicEditMenu = w;
    w->addAction(ui->actionUndo);
    w->addAction(ui->actionRedo);
    w->addSeparator();
    w->addAction(ui->actionCut);
    w->addAction(ui->actionCopy);
    w->addAction(ui->actionPaste);
    w->addAction(ui->actionPaste_Previous);
    w->addAction(ui->actionClearFrame);
    w->addSeparator();
    w->addMenu(tr("Selection"), buildSelectionSubmenu(),
               [this]{ return tr("Selection"); });
    w->addSeparator();
    w->addAction(ui->actionSelect_All);
    w->addAction(ui->actionDeselect_All);
    w->addSeparator();
    w->addAction(ui->actionPreference);
    return w;
}

CosmicMenuWindow* MainWindow2::buildSelectionSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionFlip_X);
    w->addAction(ui->actionFlip_Y);
    return w;
}

CosmicMenuWindow* MainWindow2::buildPrepareScannedSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionPegbarAlignment);
    w->addAction(ui->actionAdd_Transparency_to_paper);
    return w;
}

CosmicMenuWindow* MainWindow2::buildViewMenuWindow()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionReset_View);
    w->addAction(ui->actionCenter_View);
    w->addSeparator();
    w->addAction(ui->actionZoom_In);
    w->addAction(ui->actionZoom_Out);
    w->addMenu(tr("Zoom"), buildZoomSubmenu(),
               [this]{ return tr("Zoom"); });
    w->addSeparator();
    w->addAction(ui->actionRotate_Clockwise);
    w->addAction(ui->actionRotate_Anticlockwise);
    w->addAction(ui->actionReset_Rotation);
    w->addSeparator();
    w->addAction(ui->actionHorizontal_Flip);
    w->addAction(ui->actionVertical_Flip);
    w->addSeparator();
    w->addMenu(tr("Overlays"), buildOverlaysSubmenu(),
               [this]{ return tr("Overlays"); });
    w->addMenu(tr("Layer Visibility"), buildLayerVisibilitySubmenu(),
               [this]{ return tr("Layer Visibility"); });
    return w;
}

CosmicMenuWindow* MainWindow2::buildZoomSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionZoom400);
    w->addAction(ui->actionZoom300);
    w->addAction(ui->actionZoom200);
    w->addAction(ui->actionZoom100);
    w->addAction(ui->actionZoom50);
    w->addAction(ui->actionZoom33);
    w->addAction(ui->actionZoom25);
    return w;
}

CosmicMenuWindow* MainWindow2::buildOnionSkinSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionOnionPrev);
    w->addAction(ui->actionOnionNext);
    return w;
}

CosmicMenuWindow* MainWindow2::buildLayerVisibilitySubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    w->addAction(ui->actionVisibilityCurrentLayerOnly);
    w->addAction(ui->actionVisibilityAll);
    return w;
}

CosmicMenuWindow* MainWindow2::buildOverlaysSubmenu()
{
    auto* w = new CosmicMenuWindow(this);
    // Grid, perspective overlays are intentionally removed (hidden in createMenus)
    w->addAction(ui->actionCenter);
    w->addAction(ui->actionThirds);
    w->addAction(ui->actionGoldenRatio);
    w->addAction(ui->actionSafeAreas);
    return w;
}

CosmicMenuWindow* MainWindow2::buildCanvasMenuWindow()
{
    auto* w = new CosmicMenuWindow(this);

    // Set Canvas Size
    if (!mActionCanvasSize) {
        mActionCanvasSize = new QAction(tr("Canvas Size…"), this);
        connect(mActionCanvasSize, &QAction::triggered, this, &MainWindow2::openCanvasSizeDialog);
    }
    w->addAction(mActionCanvasSize);

    // Canvas Color
    if (!mActionCanvasColor) {
        mActionCanvasColor = new QAction(tr("Canvas Color…"), this);
        connect(mActionCanvasColor, &QAction::triggered, this, &MainWindow2::openCanvasColorDialog);
    }
    w->addAction(mActionCanvasColor);

    // Extra Space
    if (!mActionCanvasExtraSpace) {
        mActionCanvasExtraSpace = new QAction(tr("Extra Space…"), this);
        connect(mActionCanvasExtraSpace, &QAction::triggered, this, &MainWindow2::openExtraSpaceDialog);
    }
    w->addAction(mActionCanvasExtraSpace);

    w->addSeparator();

    // Show Canvas Color Palette toggle
    if (!mActionShowCanvasPalette) {
        mActionShowCanvasPalette = new QAction(tr("Show Canvas Color Palette"), this);
        mActionShowCanvasPalette->setCheckable(true);
        QSettings settings(PENCIL2D, PENCIL2D);
        const bool visible = settings.value("Canvas/ShowPaletteOverlay", true).toBool();
        mActionShowCanvasPalette->setChecked(visible);
        if (mCanvasPaletteOverlay) mCanvasPaletteOverlay->setVisible(visible);
        connect(mActionShowCanvasPalette, &QAction::toggled, this, [this](bool on) {
            if (mCanvasPaletteOverlay) mCanvasPaletteOverlay->setVisible(on);
            QSettings s(PENCIL2D, PENCIL2D);
            s.setValue("Canvas/ShowPaletteOverlay", on);
            s.sync();
        });
    }
    w->addAction(mActionShowCanvasPalette);

    return w;
}

CosmicMenuWindow* MainWindow2::buildWindowsMenuWindow()
{
    auto* w = new CosmicMenuWindow(this);

    // Actions are pre-created in createMenus() so shortcuts work from first launch.
    // Here we wire the toggle connections and add them to the menu.
    // We also register label factories so retranslateCosmicMenuBar() can refresh them.
    struct GroupEntry { std::function<QString()> labelFn; QStringList ids; QAction** member; };
    const QList<GroupEntry> entries = {
        { [this]{ return tr("File Operations"); },  { QStringLiteral("file_ops") },                             &mCCToggleFileOpsAction    },
        { [this]{ return tr("View Controls"); },    { QStringLiteral("view_ops"), QStringLiteral("flip_ops") }, &mCCToggleViewAction       },
        { [this]{ return tr("Overlays"); },         { QStringLiteral("overlays") },                             &mCCToggleOverlaysAction   },
        { [this]{ return tr("Undo / Redo"); },      { QStringLiteral("undo"),     QStringLiteral("redo") },     &mCCToggleUndoRedoAction   },
        { [this]{ return tr("Clear Frame"); },      { QStringLiteral("clear_frame") },                          &mCCToggleClearFrameAction },
    };

    for (const auto& entry : entries) {
        QAction* a = *entry.member;
        if (!a) continue;
        a->setText(entry.labelFn());
        // Register factory so language changes can refresh the action text.
        mCCActionLabelFactories[a] = entry.labelFn;
        connect(a, &QAction::toggled, this, [this, ids = entry.ids](bool on) {
            if (mCosmicControlCenter)
                for (const QString& id : ids)
                    mCosmicControlCenter->setGroupVisible(id, on);
        });
        w->addAction(a);
    }

    w->addSeparator();

    return w;
}

void MainWindow2::retranslateCosmicMenuBar()
{
    if (!mCosmicMenuBar) return;

    // Refresh CC toggle action labels (Toolbars menu items)
    for (auto it = mCCActionLabelFactories.constBegin();
         it != mCCActionLabelFactories.constEnd(); ++it)
    {
        if (it.key() && it.value()) {
            it.key()->setText(it.value()());
        }
    }

    // Also retranslate the canvas actions that were created with tr() at build time
    if (mActionCanvasSize)
        mActionCanvasSize->setText(tr("Canvas Size…"));
    if (mActionCanvasColor)
        mActionCanvasColor->setText(tr("Canvas Color…"));
    if (mActionCanvasExtraSpace)
        mActionCanvasExtraSpace->setText(tr("Extra Space…"));
    if (mActionShowCanvasPalette)
        mActionShowCanvasPalette->setText(tr("Show Canvas Color Palette"));

    // Propagate to the bar (re-evaluates top-level entry labels and all submenu labels)
    mCosmicMenuBar->retranslate();
}

void MainWindow2::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        // Retranslate the .ui-generated actions (standard Qt mechanism)
        ui->retranslateUi(this);
        // Retranslate our custom cosmic menu bar and all its menus
        retranslateCosmicMenuBar();
    }
}

void MainWindow2::updateCosmicToolsBarsPosition()
{
    if (!ui || !ui->background) {
        return;
    }
    QWidget* parent = ui->background;
    if (parent->width() <= 0 || parent->height() <= 0) {
        return;
    }

    const int pad = 2;
    const int barW = 22;
    int yInset = pad;
    const int leftXInset = 0;
    const int rightXInset = 0;
    const int bottomPad = pad;

    if (mCosmicMenuBar) {
        int menuH = mCosmicMenuBar->menuBarHeight();
        mCosmicMenuBar->setGeometry(0, 0, parent->width(), menuH);
        mCosmicMenuBar->raise();
        yInset = menuH;
    }

    // CosmicControlCenter sits full-width directly beneath the menubar.
    // The side panels and canvas area start below it.
    if (mCosmicControlCenter) {
        const int ccH = mCosmicControlCenter->barHeight();
        mCosmicControlCenter->setGeometry(0, yInset, parent->width(), ccH);
        mCosmicControlCenter->raise();
        yInset += ccH;
    }

    const int h = qMax(0, parent->height() - yInset);

    parent->setProperty("CosmicTopReserved", 0);
    if (ui->layout) {
        ui->layout->setContentsMargins(0, 0, 0, 0);
    }

    if (mCosmicLeftToolsBar) {
        int w = barW;
        if (auto* p = qobject_cast<CosmicLeftToolPanel*>(mCosmicLeftToolsBar)) {
            w = qMax(barW, p->reservedWidth());
        }
        mCosmicLeftToolsBar->setGeometry(leftXInset, yInset, w, h);
        mCosmicLeftToolsBar->raise();
        mCosmicLeftToolsBar->update();

        // Expose left reserved inset to other overlays (e.g. CanvasPaletteOverlay)
        parent->setProperty("CosmicLeftReserved", w + leftXInset);
    }
    if (mCosmicRightToolsBar) {
        int w = barW;
        if (auto* p = qobject_cast<CosmicRightToolPanel*>(mCosmicRightToolsBar)) {
            w = qMax(barW, p->reservedWidth());
        }
        mCosmicRightToolsBar->setGeometry(parent->width() - w - rightXInset, yInset, w, h);
        mCosmicRightToolsBar->raise();
        mCosmicRightToolsBar->update();

        parent->setProperty("CosmicRightReserved", w + rightXInset);
    }

    if (mCosmicBottomToolsBar) {
        const int leftRes = parent->property("CosmicLeftReserved").toInt();
        const int rightRes = parent->property("CosmicRightReserved").toInt();

        int pillH = mCosmicBottomToolsBar->preferredHeight();
        if (mCosmicBottomToolsBar->isCollapsed()) {
            pillH = mCosmicBottomToolsBar->height();
        }
        const int x = qMax(0, leftRes + pad);
        const int w = qMax(0, parent->width() - x - (rightRes + pad));
        const int y = qMax(0, parent->height() - pillH);

        mCosmicBottomToolsBar->setGeometry(x, y, w, pillH);
        mCosmicBottomToolsBar->raise();
        mCosmicBottomToolsBar->update();

        parent->setProperty("CosmicBottomReserved", pillH);
    }

    // Keep palette above background but below bars so bars remain visible.
    if (mCanvasPaletteOverlay) {
        mCanvasPaletteOverlay->raise();
    }

    // Position timecode overlay at top-left of the canvas area (below menu/control bars)
    if (mCanvasTimecodeOverlay) {
        const int leftRes = parent->property("CosmicLeftReserved").toInt();
        const int topPad = 8;
        const int leftPad = 8;
        mCanvasTimecodeOverlay->move(leftRes + leftPad, yInset + topPad);
        mCanvasTimecodeOverlay->raise();
    }

    // Position keyframe scrubber pill at top-center of the canvas area
    if (mCanvasKeyframeScrubber) {
        const int leftRes  = parent->property("CosmicLeftReserved").toInt();
        const int rightRes = parent->property("CosmicRightReserved").toInt();
        const int canvasW  = parent->width() - leftRes - rightRes;
        const int cx       = leftRes + (canvasW - mCanvasKeyframeScrubber->width()) / 2;
        mCanvasKeyframeScrubber->move(cx, yInset + 8);
        mCanvasKeyframeScrubber->raise();
    }

    if (mCosmicLeftToolsBar) mCosmicLeftToolsBar->raise();
    if (mCosmicRightToolsBar) mCosmicRightToolsBar->raise();
    if (mCosmicBottomToolsBar) mCosmicBottomToolsBar->raise();
    if (mCosmicControlCenter) mCosmicControlCenter->raise();
    if (mCosmicMenuBar) mCosmicMenuBar->raise();

    // Keep cell bank toggle and panel positioned correctly
    updateCellBankPanelPosition();
}

void MainWindow2::updateCellBankPanelPosition()
{
    if (!ui || !ui->background) return;

    QWidget* bg = ui->background;
    const int rightRes = bg->property("CosmicRightReserved").toInt();
    const int bottomRes = bg->property("CosmicBottomReserved").toInt();
    const int margin = 6;
    const int toggleSize = 32;

    // Toggle sits bottom-right of canvas, just above the bottom bar
    const int toggleX = bg->width() - rightRes - toggleSize - margin;
    const int toggleY = bg->height() - bottomRes - toggleSize - margin;

    if (mCellBankPanelToggle) {
        mCellBankPanelToggle->setGeometry(toggleX, toggleY, toggleSize, toggleSize);
        mCellBankPanelToggle->raise();
    }

    // Panel anchors its bottom-right to the toggle's top-right
    if (mCellBankPanel && mCellBankPanel->isVisible()) {
        const QPoint toggleBottomRight(toggleX + toggleSize, toggleY);
        const int bottomBarTop = bg->height() - bottomRes;
        mCellBankPanel->repositionAbove(toggleBottomRight, bottomBarTop);
        mCellBankPanel->raise();
    }
}

void MainWindow2::onCanvasPaletteColorChanged(const QColor& color)
{
    if (!mEditor) return;
    // Mirror prior ToolOptionWidget PenColorPalette behavior:
    // - If selecting a preset color button, set a temporary stroke override
    // - If selecting the first (custom) button, clear override and set front color
    if (mEditor->tools() && mEditor->color()) {
        // Determine if the chosen color corresponds to one of the palette preset colors
        // via the toolbar-hosted pen palette (delegates to PenColorPalette)
        bool isPreset = mPenPaletteToolbarWidget ? mPenPaletteToolbarWidget->isPresetColor(color) : false;
        if (isPreset) {
            mEditor->tools()->setStrokeColorOverride(color);
        } else {
            mEditor->tools()->clearStrokeColorOverride();
            mEditor->color()->setFrontColor(color);
        }
    }
}

bool MainWindow2::tryRecoverUnsavedProject()
{
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataRoot.isEmpty()) {
        return false;
    }

    const QString workspacesRoot = QDir(appDataRoot).filePath(QStringLiteral("workspaces"));
    QDir wsDir(workspacesRoot);
    if (!wsDir.exists()) {
        return false;
    }

    QString newestAutosave;
    QDateTime newestTime;

    const QStringList candidates = wsDir.entryList(QStringList() << QStringLiteral("untitled_*") , QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& d : candidates)
    {
        const QString autosave = QDir(wsDir.absoluteFilePath(d)).filePath(QStringLiteral("autosave.n2d"));
        QFileInfo fi(autosave);
        if (!fi.exists() || !fi.isFile()) {
            continue;
        }
        if (!newestTime.isValid() || fi.lastModified() > newestTime) {
            newestTime = fi.lastModified();
            newestAutosave = autosave;
        }
    }

    if (newestAutosave.isEmpty()) {
        return false;
    }

    const int ret = QMessageBox::question(this,
                                         tr("Recover Unsaved Project"),
                                         tr("An unsaved project was found. Do you want to recover it?"),
                                         QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return false;
    }

    CosmicProgressPopup progress(this);
    progress.setTitle(tr("Recovering projectâ¦"));
    progress.setRange(0, 100);
    progress.showCentered();

    const N2DProjectIO::ProgressCallbacks cb{
        [&progress](int p) {
            progress.setValue(p);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        },
        [&progress](int max) {
            progress.setRange(0, max);
        }
    };

    SceneSequenceManager* ssm = (mTimeLine) ? mTimeLine->sceneSequenceManager() : nullptr;
    Status s = N2DProjectIO::loadProject(newestAutosave, mEditor, ssm, cb);
    if (!s.ok()) {
        return false;
    }

    // Avoid re-prompting on every startup. The recovered session will continue autosaving
    // into its current transient workspace.
    QFile::remove(newestAutosave);

    if (ssm) {
        ssm->setProjectFilePath(QString());
        ssm->setWorkspaceTransient(true);
    }
    if (mEditor && mEditor->object()) {
        mEditor->object()->setFilePath(QString());
    }

    setWindowTitle(PENCIL_WINDOW_TITLE);
    updateSaveState();
    updateBackupActionState();
    progress.setValue(progress.maximum());
    return true;
}

void MainWindow2::startProjectRecovery(int result)
{
    Q_UNUSED(result);
}

void MainWindow2::createToolbars()
{
    // Qt toolbars for file/view/overlay actions are no longer needed —
    // CosmicControlCenter hosts all of these directly.
    // We keep this function only to create the pen palette and autofill
    // widgets (which are embedded into CosmicControlCenter) and to wire
    // up tool actions and their checked-state sync.

    mMainToolbar    = nullptr;
    mViewToolbar    = nullptr;
    mOverlayToolbar = nullptr;
    mToolsToolbar   = nullptr;

    // ── Tool actions ──────────────────────────────────────────────────────────
    if (!mActionPencil)      mActionPencil      = new QAction(QIcon(":/icons/themes/playful/tools/tool-pencil.svg"), tr("Pencil"), this);
    if (!mActionCurve)       mActionCurve       = new QAction(QIcon(":/CustomToolIcons/Curve.svg"), tr("Curve"), this);
    if (!mActionBSpline)     mActionBSpline     = new QAction(QIcon(":/CustomToolIcons/Spline.svg"), tr("B-Spline"), this);
    if (!mActionRectGapFill) mActionRectGapFill = new QAction(QIcon(":/CustomToolIcons/RectangleFill.svg"), tr("Rect Gap Fill"), this);
    if (!mActionLassoGapFill)mActionLassoGapFill= new QAction(QIcon(":/CustomToolIcons/LassoFill.svg"), tr("Lasso Gap Fill"), this);
    if (!mActionDplate)      mActionDplate      = new QAction(QIcon(":/CustomToolIcons/Dplate.svg"), tr("Dplate"), this);
    if (!mActionLassoSelect) mActionLassoSelect = new QAction(QIcon(":/CustomToolIcons/LassoSelect.svg"), tr("Lasso Select"), this);

    for (QAction* a : {mActionPencil, mActionCurve, mActionBSpline,
                       mActionRectGapFill, mActionLassoGapFill,
                       mActionDplate, mActionLassoSelect})
        a->setCheckable(true);

    if (ui->actionMove)   ui->actionMove->setVisible(false);
    if (ui->actionSelect) ui->actionSelect->setVisible(false);
    if (ui->actionPen)    ui->actionPen->setVisible(false);

    if (mToolsActionGroup) {
        for (QAction* a : {mActionPencil, mActionCurve, mActionBSpline,
                           mActionRectGapFill, mActionLassoGapFill,
                           mActionDplate, mActionLassoSelect})
            mToolsActionGroup->addAction(a);
    }

    connect(mActionPencil,      &QAction::triggered, this, [this]{ mToolBox->setActiveTool(PENCIL); });
    connect(mActionCurve,       &QAction::triggered, this, [this]{ mToolBox->setActiveTool(CURVE); });
    connect(mActionBSpline,     &QAction::triggered, this, [this]{ mToolBox->setActiveTool(BSPLINE); });
    connect(mActionRectGapFill, &QAction::triggered, this, [this]{ mToolBox->setActiveTool(RECT_GAP_FILL); });
    connect(mActionLassoGapFill,&QAction::triggered, this, [this]{ mToolBox->setActiveTool(LASSO_GAP_FILL); });
    connect(mActionDplate,      &QAction::triggered, this, [this]{ mToolBox->setActiveTool(DPLATE); });
    connect(mActionLassoSelect, &QAction::triggered, this, [this]{ mEditor->tools()->setCurrentTool(LASSO_SELECT); });

    connect(mEditor->tools(), &ToolManager::toolChanged, this, [this](ToolType t){
        if (ui->actionSelect)    ui->actionSelect->setChecked(t == SELECT);
        if (ui->actionMove)      ui->actionMove->setChecked(t == MOVE || t == CAMERA);
        if (ui->actionPen)       ui->actionPen->setChecked(false);
        if (mActionPencil)       mActionPencil->setChecked(t == PENCIL);
        if (ui->actionPolyline)  ui->actionPolyline->setChecked(t == POLYLINE);
        if (ui->actionBucket)    ui->actionBucket->setChecked(t == BUCKET);
        if (ui->actionEraser)    ui->actionEraser->setChecked(t == ERASER);
        if (ui->actionHand)      ui->actionHand->setChecked(t == HAND);
        if (ui->actionEyedropper)ui->actionEyedropper->setChecked(t == EYEDROPPER);
        if (mActionCurve)        mActionCurve->setChecked(t == CURVE);
        if (mActionBSpline)      mActionBSpline->setChecked(t == BSPLINE);
        if (mActionRectGapFill)  mActionRectGapFill->setChecked(t == RECT_GAP_FILL);
        if (mActionLassoGapFill) mActionLassoGapFill->setChecked(t == LASSO_GAP_FILL);
        if (mActionDplate)       mActionDplate->setChecked(t == DPLATE);
        if (mActionLassoSelect)  mActionLassoSelect->setChecked(t == LASSO_SELECT);
    });

    // ── Pen Palette widget ────────────────────────────────────────────────────
    if (!mPenPaletteToolbarWidget) {
        mPenPaletteToolbarWidget = new PenColorPalette(this);
        connect(mPenPaletteToolbarWidget, &PenColorPalette::colorChanged,
                this, &MainWindow2::onCanvasPaletteColorChanged, Qt::UniqueConnection);
        if (mEditor && mEditor->preference())
            mPenPaletteToolbarWidget->connectToPreferenceManager(mEditor->preference());
    }

    // ── Auto Fill Controls widget ─────────────────────────────────────────────
    if (!mAutoFillToolbarWidget && mEditor)
        mAutoFillToolbarWidget = new AutoLineColorToggle(mEditor, this);

    // mToolbars is now empty — no Qt toolbars remain
    mToolbars.clear();
    mPenPaletteToolbar = nullptr;
    mAutoFillToolbar   = nullptr;
}

void MainWindow2::openCanvasSizeDialog()
{
    // Find the last-used/nearest camera layer. Fallback to first camera layer.
    Layer* layer = mEditor->layers()->getLastCameraLayer();
    if (!layer) {
        // No camera layer found, nothing to do
        return;
    }
    auto cameraLayer = dynamic_cast<LayerCamera*>(layer);
    Q_ASSERT(cameraLayer);

    CosmicCanvasSizePopup popup(this);
    popup.setInitialSize(cameraLayer->getViewRect().width(), cameraLayer->getViewRect().height());
    popup.setCenterOnShow(true);
    if (popup.exec() != QDialog::Accepted)
    {
        return;
    }

    const int newW = popup.canvasWidth();
    const int newH = popup.canvasHeight();

    // Apply the new size to the camera layer
    if (mEditor && mEditor->preference()) {
        mEditor->preference()->set(SETTING::FIELD_W, newW);
        mEditor->preference()->set(SETTING::FIELD_H, newH);
    } else {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(SETTING_FIELD_W, newW);
        settings.setValue(SETTING_FIELD_H, newH);
    }
    cameraLayer->setViewRect(QRect(-newW / 2, -newH / 2, newW, newH));
    mEditor->view()->forceUpdateViewTransform();
}

void MainWindow2::openCanvasColorDialog()
{
    PreferenceManager* prefs = mEditor ? mEditor->preference() : nullptr;
    QColor initial = Qt::white;
    if (prefs) {
        QColor c = QColor::fromRgba(prefs->getInt(SETTING::CANVAS_COLOR));
        if (c.isValid()) initial = c;
    }

    auto* popup = new PopupColorWheelWidget(this);
    if (prefs) popup->setPreferenceManager(prefs);
    popup->setColor(initial);

    connect(popup, &PopupColorWheelWidget::colorSelected, this, [prefs](const QColor& c) {
        if (!c.isValid() || !prefs) return;
        prefs->set(SETTING::CANVAS_COLOR, static_cast<int>(c.rgba()));
    });

    popup->showAt(QCursor::pos());
}

void MainWindow2::openExtraSpaceDialog()
{
    PreferenceManager* prefs = mEditor ? mEditor->preference() : nullptr;
    CosmicExtraSpacePopup popup(prefs, this);
    popup.setCenterOnShow(true);
    popup.exec();
}

void MainWindow2::ensureMenubarUnobstructed()
{
    if (!ui || !ui->menuBar) return;
    ui->menuBar->raise();
    // No floating docks remain - all panels are in cosmic tool panels.
}

void MainWindow2::toggleDocksVisibility()
{
    // No traditional docks remain - all panels are in cosmic tool panels.
    // Toggle cosmic panels visibility instead if needed in future.
}

void MainWindow2::onDockVisibilityChanged(bool visible)
{
    Q_UNUSED(visible);
    // No traditional docks remain.
}

void MainWindow2::restoreDockState()
{
    QSettings settings(PENCIL2D, PENCIL2D);

    QVariant winState = settings.value(SETTING_WINDOW_STATE);
    restoreState(winState.toByteArray());

    // Restore geometry AFTER dock state is restored.
    QVariant winGeometry = settings.value(SETTING_WINDOW_GEOMETRY);
    restoreGeometry(winGeometry.toByteArray());

    // Validate restored geometry: if it's off-screen or too small, reset to a sensible default
    QRect geom = geometry();
    bool geometryValid = true;
    if (geom.width() < 200 || geom.height() < 200) geometryValid = false;
    if (geometryValid) {
        const auto screens = QGuiApplication::screens();
        bool intersects = false;
        for (QScreen* s : screens) {
            if (s->availableGeometry().intersects(geom)) { intersects = true; break; }
        }
        if (!intersects) geometryValid = false;
    }
    if (!geometryValid) {
        settings.remove(SETTING_WINDOW_GEOMETRY);
        settings.remove(SETTING_WINDOW_STATE);
        QSize defaultSize(1200, 800);
        resize(defaultSize);
        if (QScreen* ps = QGuiApplication::primaryScreen()) {
            QRect av = ps->availableGeometry();
            move(av.x() + (av.width() - width())/2, av.y() + (av.height() - height())/2);
        }
    }

    QTimer::singleShot(0, this, [this]() {
        Q_UNUSED(this); // geometry restore complete
    });
}
