#include "cosmiclefttoolpanel.h"

#include "cosmiclefttoolsbar.h"
#include "cosmictoolspane.h"
#include "brushpresetlistwidget.h"
#include "cosmicrighttoolpanel.h"
#include "cosmiclayerspane.h"

#include <QBoxLayout>
#include <QSettings>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QLayoutItem>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QMouseEvent>
#include <QTimer>
#include <QCursor>
#include <QApplication>
#include <QSvgRenderer>
#include <QIcon>
#include <QPixmap>

#include "../CUI/cosmicuimetrics.h"
#include "cosmicwarningpopup.h"

namespace {
class CosmicDropZoneIndicator : public QWidget
{
public:
    explicit CosmicDropZoneIndicator(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        Q_UNUSED(e)

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);
        const qreal radius = 12.0;

        QPainterPath path;
        path.addRoundedRect(r, radius, radius);

        const QColor accent(CosmicColor::accent());
        const QColor bottom(0x2d, 0x1f, 0x80);

        QLinearGradient lg(r.topLeft(), r.bottomLeft());
        lg.setColorAt(0.0, accent);
        lg.setColorAt(1.0, bottom);

        p.setPen(Qt::NoPen);
        p.setBrush(lg);
        p.drawPath(path);

        // Inner-edge extrusion + blur: draw a soft inner shadow ring (rect/squircle shaped,
        // not circular) by layering inset rounded-rect rings with decreasing opacity.
        p.save();
        p.setClipPath(path);
        p.setCompositionMode(QPainter::CompositionMode_Multiply);

        const int steps = 8;
        const qreal maxInset = 10.0;
        for (int i = 0; i < steps; ++i) {
            const qreal t = (steps <= 1) ? 1.0 : (qreal(i) / qreal(steps - 1));
            const qreal inset = 1.0 + t * maxInset;
            const int alpha = int(45 * (1.0 - t));

            const QRectF innerR = r.adjusted(inset, inset, -inset, -inset);
            if (innerR.width() <= 0.0 || innerR.height() <= 0.0) {
                continue;
            }

            QPainterPath innerPath;
            innerPath.addRoundedRect(innerR, radius, radius);

            QPainterPath ring = path.subtracted(innerPath);
            QLinearGradient shadowLg(r.topLeft(), r.bottomLeft());
            shadowLg.setColorAt(0.0, QColor(0, 0, 0, 0));
            shadowLg.setColorAt(0.70, QColor(0, 0, 0, 0));
            shadowLg.setColorAt(1.0, QColor(0, 0, 0, alpha));
            p.fillPath(ring, shadowLg);
        }

        p.restore();

        QPen pen(accent);
        pen.setWidth(4);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
};

QString paneKeyFromName(const QString& paneName)
{
    if (paneName == QStringLiteral("Tools")) return QStringLiteral("Tools");
    if (paneName == QStringLiteral("Tools Options")) return QStringLiteral("ToolsOptions");
    if (paneName == QStringLiteral("Brushes")) return QStringLiteral("Brushes");
    if (paneName == QStringLiteral("Color Wheel")) return QStringLiteral("ColorWheel");
    if (paneName == QStringLiteral("Palettes")) return QStringLiteral("Palettes");
    if (paneName == QStringLiteral("Reference")) return QStringLiteral("Reference");
    if (paneName == QStringLiteral("Layers")) return QStringLiteral("Layers");
    QString out = paneName;
    out.remove(' ');
    return out;
}

QString globalEnabledKey(const QString& paneName)
{
    return QStringLiteral("CosmicToolsPaneState/Enabled/") + paneKeyFromName(paneName);
}

QString globalExpandedKey(const QString& paneName)
{
    return QStringLiteral("CosmicToolsPaneState/Expanded/") + paneKeyFromName(paneName);
}

QString globalHeightKey(const QString& paneName)
{
    return QStringLiteral("CosmicToolsPaneState/Height/") + paneKeyFromName(paneName);
}

QString globalLeftOrderKey()
{
    return QStringLiteral("CosmicToolsLayout/LeftOrder");
}

QString globalRightOrderKey()
{
    return QStringLiteral("CosmicToolsLayout/RightOrder");
}
}

CosmicLeftToolPanel::CosmicLeftToolPanel(Editor* editor, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("CosmicLeftToolPanel"));

    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    
    // Stack-wide resize handling
    setMouseTracking(true);
    mStackResizeGrip = new QWidget(this);
    mStackResizeGrip->setObjectName(QStringLiteral("CosmicLeftToolPanelStackResizeGrip"));
    mStackResizeGrip->setAttribute(Qt::WA_NoSystemBackground, true);
    mStackResizeGrip->setAutoFillBackground(false);
    mStackResizeGrip->setCursor(Qt::SplitHCursor);
    mStackResizeGrip->installEventFilter(this);
    mStackResizeGrip->raise();

    // Per-pane grips are disabled; we use the stack-wide grip instead.
    // Note: setResizeGripEnabled called after panes are fully constructed.

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mPill = new CosmicLeftToolsBar(this);
    mPill->setFixedWidth(28);
    mPill->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    mPanesScrollArea = new QScrollArea(this);
    mPanesScrollArea->setObjectName(QStringLiteral("CosmicPanesScrollArea"));
    mPanesScrollArea->setWidgetResizable(false);
    mPanesScrollArea->setFrameShape(QFrame::NoFrame);
    mPanesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mPanesScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mPanesScrollArea->setAutoFillBackground(false);
    mPanesScrollArea->setAttribute(Qt::WA_NoSystemBackground, true);
    if (mPanesScrollArea->viewport()) {
        mPanesScrollArea->viewport()->setAutoFillBackground(false);
        mPanesScrollArea->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        mPanesScrollArea->viewport()->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    }
    mPanesScrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    mPaneSlider = new QScrollBar(Qt::Vertical, this);
    mPaneSlider->setObjectName(QStringLiteral("CosmicPaneSlider"));
    mPaneSlider->setFixedWidth(16);
    mPaneSlider->setStyleSheet(QStringLiteral(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 0px;"
        "  width: 16px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 3px;"
        "  border: 1px solid #2a2a2a;"
        "  min-height: 26px;"
        "  margin: 1px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"));

    mPanesContainer = new QWidget(this);
    mPanesContainer->setAttribute(Qt::WA_NoSystemBackground, true);
    mPanesContainer->setAutoFillBackground(false);
    mPanesContainer->setMouseTracking(true);
    mPanesContainer->installEventFilter(this);
    mPanesContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    // Create per-pane resize splitters - one for each pane that could be resizable
    mToolsResizeSplitter = new QWidget(mPanesContainer);
    mToolsResizeSplitter->setObjectName(QStringLiteral("CosmicLeftToolPanelToolsResizeSplitter"));
    mToolsResizeSplitter->setAttribute(Qt::WA_NoSystemBackground, true);
    mToolsResizeSplitter->setAutoFillBackground(false);
    mToolsResizeSplitter->setCursor(Qt::SplitVCursor);
    mToolsResizeSplitter->setVisible(false);
    mToolsResizeSplitter->setEnabled(false);
    mToolsResizeSplitter->installEventFilter(this);
    mToolsResizeSplitter->raise();

    mToolsOptionsResizeSplitter = new QWidget(mPanesContainer);
    mToolsOptionsResizeSplitter->setObjectName(QStringLiteral("CosmicLeftToolPanelToolsOptionsResizeSplitter"));
    mToolsOptionsResizeSplitter->setAttribute(Qt::WA_NoSystemBackground, true);
    mToolsOptionsResizeSplitter->setAutoFillBackground(false);
    mToolsOptionsResizeSplitter->setCursor(Qt::SplitVCursor);
    mToolsOptionsResizeSplitter->setVisible(false);
    mToolsOptionsResizeSplitter->setEnabled(false);
    mToolsOptionsResizeSplitter->installEventFilter(this);
    mToolsOptionsResizeSplitter->raise();

    mBrushesResizeSplitter = new QWidget(mPanesContainer);
    mBrushesResizeSplitter->setObjectName(QStringLiteral("CosmicLeftToolPanelBrushesResizeSplitter"));
    mBrushesResizeSplitter->setAttribute(Qt::WA_NoSystemBackground, true);
    mBrushesResizeSplitter->setAutoFillBackground(false);
    mBrushesResizeSplitter->setCursor(Qt::SplitVCursor);
    mBrushesResizeSplitter->setVisible(false);
    mBrushesResizeSplitter->setEnabled(false);
    mBrushesResizeSplitter->installEventFilter(this);
    mBrushesResizeSplitter->raise();

    mLayersResizeSplitter = new QWidget(mPanesContainer);
    mLayersResizeSplitter->setObjectName(QStringLiteral("CosmicLeftToolPanelLayersResizeSplitter"));
    mLayersResizeSplitter->setAttribute(Qt::WA_NoSystemBackground, true);
    mLayersResizeSplitter->setAutoFillBackground(false);
    mLayersResizeSplitter->setCursor(Qt::SplitVCursor);
    mLayersResizeSplitter->setVisible(false);
    mLayersResizeSplitter->setEnabled(false);
    mLayersResizeSplitter->installEventFilter(this);
    mLayersResizeSplitter->raise();

    mPanesLayout = new QVBoxLayout(mPanesContainer);    mPanesLayout->setContentsMargins(0, 0, 0, 0);
    mPanesLayout->setSpacing(1);
    mPanesLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    mToolsPane = new CosmicToolsPane(QStringLiteral("Tools"), CosmicToolsPane::Side::Left, mPanesContainer);
    mToolsOptionsPane = new CosmicToolsPane(QStringLiteral("Tools Options"), CosmicToolsPane::Side::Left, mPanesContainer);
    mBrushesPane = new CosmicToolsPane(QStringLiteral("Brushes"), CosmicToolsPane::Side::Left, mPanesContainer);
    mLayersPane = new CosmicToolsPane(QStringLiteral("Papers"), CosmicToolsPane::Side::Left, mPanesContainer);

    mToolsPane->setObjectName(QStringLiteral("CosmicToolsPane_Tools"));
    mToolsOptionsPane->setObjectName(QStringLiteral("CosmicToolsPane_ToolsOptions"));
    mBrushesPane->setObjectName(QStringLiteral("CosmicToolsPane_Brushes"));
    mLayersPane->setObjectName(QStringLiteral("CosmicToolsPane_Layers"));

    mToolsPane->setFixedWidth(mPaneWidth);
    mToolsOptionsPane->setFixedWidth(mPaneWidth);
    mBrushesPane->setFixedWidth(mPaneWidth);
    mLayersPane->setFixedWidth(mPaneWidth);

    // Create and set the brushes content widget
    auto* brushesContent = new BrushPresetListWidget(mBrushesPane);
    mBrushesPane->setContentWidget(brushesContent);

    // Create and set the layers content widget
    auto* layersContent = new CosmicLayersPane(editor, mLayersPane);
    mLayersPane->setContentWidget(layersContent);

    mPanesLayout->addWidget(mToolsPane);
    mPanesLayout->addWidget(mToolsOptionsPane);
    mPanesLayout->addWidget(mBrushesPane);
    mPanesLayout->addWidget(mLayersPane);
    mPanesLayout->addStretch(1);

    // Per-pane grips are disabled; we use the stack-wide grip instead.
    mToolsPane->setResizeGripEnabled(false);
    mToolsOptionsPane->setResizeGripEnabled(false);
    mBrushesPane->setResizeGripEnabled(false);
    mLayersPane->setResizeGripEnabled(false);

    // Initialize index system with default order
    mPaneIndexOrder.append("Tools");
    mPaneIndexOrder.append("Tools Options");
    mPaneIndexOrder.append("Brushes");
    mPaneIndexOrder.append("Layers");

    // Dynamic registry for move transfers (includes all known pane icon names)
    mPanes.insert(QStringLiteral("Tools"), mToolsPane);
    mPanes.insert(QStringLiteral("Tools Options"), mToolsOptionsPane);
    mPanes.insert(QStringLiteral("Brushes"), mBrushesPane);
    mPanes.insert(QStringLiteral("Layers"), mLayersPane);

    mPaneIcons.insert(QStringLiteral("Tools"), QStringLiteral("TabTools"));
    mPaneIcons.insert(QStringLiteral("Tools Options"), QStringLiteral("TabToolsOptions"));
    mPaneIcons.insert(QStringLiteral("Brushes"), QStringLiteral("TabBrushPresets"));
    mPaneIcons.insert(QStringLiteral("Layers"), QStringLiteral("TabPapers"));
    mPaneIcons.insert(QStringLiteral("Color Wheel"), QStringLiteral("TabColorWheel"));
    mPaneIcons.insert(QStringLiteral("Color Sliders"), QStringLiteral("rgbSliders"));
    mPaneIcons.insert(QStringLiteral("Palettes"), QStringLiteral("TabPallets"));
    mPaneIcons.insert(QStringLiteral("Reference"), QStringLiteral("TabRefferenceWindow"));

    mPanesContainer->setLayout(mPanesLayout);

    mPanesScrollArea->setWidget(mPanesContainer);

    // Wire the external slider to the scroll area.
    if (auto* internal = mPanesScrollArea->verticalScrollBar()) {
        connect(internal, &QScrollBar::rangeChanged, this, [this, internal](int min, int max) {
            if (mPaneSlider) {
                mPaneSlider->setRange(min, max);
                mPaneSlider->setPageStep(internal->pageStep());
            }
        });
        connect(internal, &QScrollBar::valueChanged, this, [this](int v) {
            if (mPaneSlider && mPaneSlider->value() != v) {
                mPaneSlider->setValue(v);
            }
        });
        connect(mPaneSlider, &QScrollBar::valueChanged, this, [internal](int v) {
            if (internal->value() != v) {
                internal->setValue(v);
            }
        });
        // Initialize
        mPaneSlider->setRange(internal->minimum(), internal->maximum());
        mPaneSlider->setPageStep(internal->pageStep());
        mPaneSlider->setValue(internal->value());
    }

    root->addWidget(mPill);
    root->addWidget(mPaneSlider);
    root->addWidget(mPanesScrollArea);

    setLayout(root);

    connect(mPill, &CosmicLeftToolsBar::lockEnabledChanged, this, [this](bool locked) {
        mAutoHideEnabled = !locked; // Auto-hide when unlocked
        if (locked) {
            showToolPanels(); // Always show when locked
        } else {
            updateAutoHideVisibility(); // Check if we should hide when unlocked
        }
        saveSurvivalSettings();
    });
    connect(mPill, &CosmicLeftToolsBar::toolsPaneEnabledChanged, this, [this](bool) {
        syncPaneVisibility();
        updateSliderVisibility();
        saveSurvivalSettings();
    });
    connect(mPill, &CosmicLeftToolsBar::toolsOptionsPaneEnabledChanged, this, [this](bool) {
        syncPaneVisibility();
        updateSliderVisibility();
        saveSurvivalSettings();
    });
    connect(mPill, &CosmicLeftToolsBar::brushesPaneEnabledChanged, this, [this](bool) {
        syncPaneVisibility();
        updateSliderVisibility();
        saveSurvivalSettings();
    });

    // Dynamic toggles (e.g. panes transferred from the other side) emit paneEnabledChanged.
    // Without this, toggling a transferred pane would not affect visibility.
    connect(mPill, &CosmicLeftToolsBar::paneEnabledChanged, this, [this](const QString&, bool) {
        syncPaneVisibility();
        updateSliderVisibility();
        saveSurvivalSettings();
    });

    if (qApp) {
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
            saveSurvivalSettings();
        });
    }

    // Bind all panes to this panel using centralized helper
    bindPaneToThisPanel(mToolsPane, QStringLiteral("Tools"));
    bindPaneToThisPanel(mToolsOptionsPane, QStringLiteral("Tools Options"));
    bindPaneToThisPanel(mBrushesPane, QStringLiteral("Brushes"));
    bindPaneToThisPanel(mLayersPane, QStringLiteral("Layers"));

    // Set translated display titles now that the translator is installed.
    // The pane constructor receives the settings key (English); we override
    // the visible label here so it shows in the active language from startup.
    if (mToolsPane)        mToolsPane->setDisplayTitle(tr("Tools"));
    if (mToolsOptionsPane) mToolsOptionsPane->setDisplayTitle(tr("Tools Options"));
    if (mBrushesPane)      mBrushesPane->setDisplayTitle(tr("Brushes"));
    if (mLayersPane)       mLayersPane->setDisplayTitle(tr("Papers"));

    connect(mBrushesPane, &CosmicToolsPane::resizeVDragStarted, this, [this](const QPoint& gp) {
        beginPaneVResize(mBrushesPane, gp);
    });
    connect(mBrushesPane, &CosmicToolsPane::resizeVDragMoved, this, [this](const QPoint& gp) {
        updatePaneVResize(gp);
    });
    connect(mBrushesPane, &CosmicToolsPane::resizeVDragEnded, this, [this]() {
        endPaneVResize();
    });

    connect(mLayersPane, &CosmicToolsPane::resizeVDragStarted, this, [this](const QPoint& gp) {
        beginPaneVResize(mLayersPane, gp);
    });
    connect(mLayersPane, &CosmicToolsPane::resizeVDragMoved, this, [this](const QPoint& gp) {
        updatePaneVResize(gp);
    });
    connect(mLayersPane, &CosmicToolsPane::resizeVDragEnded, this, [this]() {
        endPaneVResize();
    });

    // Stack-wide resize handling is now via mStackResizeGrip eventFilter; per-pane resize connections are removed.

    loadSurvivalSettings();

    updateStackResizeGripGeometry();
    updatePaneResizeSplitterGeometry();

    // Setup auto-hide timer
    mAutoHideTimer = new QTimer(this);
    mAutoHideTimer->setSingleShot(true);
    mAutoHideTimer->setInterval(150); // 150ms delay before hiding (more responsive)
    connect(mAutoHideTimer, &QTimer::timeout, this, [this]() {
        if (mAutoHideEnabled && !mMovingPane && !isCursorOverToolArea(QCursor::pos())) {
            hideToolPanels();
        }
    });

    syncPaneVisibility();
    updateSliderVisibility();
}

CosmicLeftToolPanel::~CosmicLeftToolPanel()
{
    saveSurvivalSettings();
}

bool CosmicLeftToolPanel::lockEnabled() const
{
    return mPill && mPill->lockEnabled();
}

void CosmicLeftToolPanel::setLockEnabled(bool enabled)
{
    if (!mPill) {
        return;
    }
    mPill->setLockEnabled(enabled);
}

void CosmicLeftToolPanel::toggleLock()
{
    setLockEnabled(!lockEnabled());
}

bool CosmicLeftToolPanel::paneEnabled(const QString& paneName) const
{
    return mPill && mPill->paneEnabled(paneName);
}

void CosmicLeftToolPanel::setPaneEnabled(const QString& paneName, bool enabled)
{
    if (!mPill) {
        return;
    }
    mPill->setPaneEnabled(paneName, enabled);
}

void CosmicLeftToolPanel::togglePaneEnabled(const QString& paneName)
{
    setPaneEnabled(paneName, !paneEnabled(paneName));
}

void CosmicLeftToolPanel::refreshPaneLayout()
{
    syncPaneVisibility();
    updateSliderVisibility();
    ensureStretchAtBottom();

    if (mPanesContainer && mPanesContainer->layout()) {
        mPanesContainer->layout()->invalidate();
        mPanesContainer->layout()->activate();
    }
    if (mPanesScrollArea && mPanesScrollArea->widget()) {
        mPanesScrollArea->widget()->updateGeometry();
    }

    updateGeometry();
    update();
    emit reservedWidthChanged();
}

CosmicToolsPane* CosmicLeftToolPanel::detachPaneForTransfer(const QString& paneName, bool* enabledOut)
{
    if (enabledOut) {
        *enabledOut = true;
    }
    CosmicToolsPane* pane = mPanes.value(paneName, nullptr);
    if (!pane) {
        return nullptr;
    }

    if (mPill) {
        if (enabledOut) {
            *enabledOut = mPill->paneEnabled(paneName);
        }
        mPill->removeToggle(paneName);
    }

    unbindPaneFromThisPanel(pane);

    if (mPanesLayout) {
        mPanesLayout->removeWidget(pane);
        mPanesLayout->invalidate();
        mPanesLayout->activate();
    }

    mPanes.remove(paneName);
    mPaneIndexOrder.removeAll(paneName);

    pane->setParent(nullptr);
    return pane;
}

void CosmicLeftToolPanel::attachTransferredPane(CosmicToolsPane* pane, const QString& paneName, bool enabled, int insertIdx)
{
    if (!pane || !mPanesLayout || !mPanesContainer) {
        return;
    }

    setPaneFloating(pane, false);
    pane->setSide(CosmicToolsPane::Side::Left);
    bindPaneToThisPanel(pane, paneName);

    // Connect vertical resize for all transferred panes (they need to be resizable)
    connect(pane, &CosmicToolsPane::resizeVDragStarted, this, [this, pane](const QPoint& gp) {
        beginPaneVResize(pane, gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::resizeVDragMoved, this, [this](const QPoint& gp) {
        updatePaneVResize(gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::resizeVDragEnded, this, [this]() {
        endPaneVResize();
    }, Qt::UniqueConnection);

    mPanes.insert(paneName, pane);
    const QString iconName = mPaneIcons.value(paneName);
    if (mPill && !iconName.isEmpty()) {
        mPill->ensureToggle(paneName, iconName);
        mPill->setPaneEnabled(paneName, enabled);
    }

    insertIdx = clampBeforeStretch(insertIdx);
    mPanesLayout->insertWidget(insertIdx, pane);
    mPaneIndexOrder.removeAll(paneName);
    mPaneIndexOrder.insert(insertIdx, paneName);
    mPaneIndexOrder = normalizedPaneIndexOrder(mPaneIndexOrder);
    updatePillToggleOrder();

    {
        QSettings settings(PENCIL2D, PENCIL2D);
        const bool expanded = settings.value(globalExpandedKey(paneName), true).toBool();
        const int h = settings.value(globalHeightKey(paneName), pane->height()).toInt();
        
        // Get pane-specific height bounds
        int minH, maxH;
        if (paneName == QStringLiteral("Color Wheel")) {
            minH = 260; maxH = 900; // Color Wheel needs more minimum height
        } else {
            minH = 140; maxH = 900; // Default bounds for other panes
        }
        
        // Set the last expanded height before restoring expanded state
        pane->setLastExpandedHeight(qBound(minH, h, maxH));
        
        {
            QSignalBlocker b(pane);
            pane->setExpanded(expanded);
        }
        
        // Refresh content widget to ensure it's properly visible after transfer
        pane->refreshContentWidget();
        
        if (pane->isExpanded()) {
            const int expandedH = qBound(minH, h, maxH);
            mLastExpandedHeights.insert(pane, expandedH);
            pane->setFixedHeight(expandedH);
        } else {
            mLastExpandedHeights.insert(pane, qBound(minH, h, maxH));
        }
    }

    syncPaneVisibility();
    updateSliderVisibility();
    saveSurvivalSettings();
    ensureStretchAtBottom();
}

void CosmicLeftToolPanel::loadSurvivalSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);

    const bool lockOn = settings.value(QStringLiteral("CosmicLeftToolPanel/LockEnabled"), false).toBool();
    const int paneW = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneWidth"), mPaneWidth).toInt();
    const bool toolsOnLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/ToolsEnabled"), true).toBool();
    const bool toolsOptionsOnLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/ToolsOptionsEnabled"), true).toBool();
    const bool brushesOnLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/BrushesEnabled"), true).toBool();

    const bool toolsOn = settings.contains(globalEnabledKey(QStringLiteral("Tools")))
                             ? settings.value(globalEnabledKey(QStringLiteral("Tools"))).toBool()
                             : toolsOnLegacy;
    const bool toolsOptionsOn = settings.contains(globalEnabledKey(QStringLiteral("Tools Options")))
                                    ? settings.value(globalEnabledKey(QStringLiteral("Tools Options"))).toBool()
                                    : toolsOptionsOnLegacy;
    const bool brushesOn = settings.contains(globalEnabledKey(QStringLiteral("Brushes")))
                               ? settings.value(globalEnabledKey(QStringLiteral("Brushes"))).toBool()
                               : brushesOnLegacy;

    const bool anyPaneOn = toolsOn || toolsOptionsOn || brushesOn;
    const bool normalizedToolsOn = anyPaneOn ? toolsOn : true;
    const bool normalizedToolsOptionsOn = anyPaneOn ? toolsOptionsOn : false;
    const bool normalizedBrushesOn = anyPaneOn ? brushesOn : false;
    const bool toolsExpandedLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneExpanded/Tools"), true).toBool();
    const bool toolsOptionsExpandedLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneExpanded/ToolsOptions"), true).toBool();
    const bool brushesExpandedLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneExpanded/Brushes"), true).toBool();

    const bool toolsExpanded = settings.contains(globalExpandedKey(QStringLiteral("Tools")))
                                   ? settings.value(globalExpandedKey(QStringLiteral("Tools"))).toBool()
                                   : toolsExpandedLegacy;
    const bool toolsOptionsExpanded = settings.contains(globalExpandedKey(QStringLiteral("Tools Options")))
                                          ? settings.value(globalExpandedKey(QStringLiteral("Tools Options"))).toBool()
                                          : toolsOptionsExpandedLegacy;
    const bool brushesExpanded = settings.contains(globalExpandedKey(QStringLiteral("Brushes")))
                                     ? settings.value(globalExpandedKey(QStringLiteral("Brushes"))).toBool()
                                     : brushesExpandedLegacy;

    const int brushesHLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneHeight/Brushes"), 260).toInt();
    const int brushesH = settings.contains(globalHeightKey(QStringLiteral("Brushes")))
                             ? settings.value(globalHeightKey(QStringLiteral("Brushes")), brushesHLegacy).toInt()
                             : brushesHLegacy;

    const int toolsHLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneHeight/Tools"), 260).toInt();
    const int toolsH = settings.contains(globalHeightKey(QStringLiteral("Tools")))
                           ? settings.value(globalHeightKey(QStringLiteral("Tools")), toolsHLegacy).toInt()
                           : toolsHLegacy;

    const int toolsOptionsHLegacy = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneHeight/ToolsOptions"), 260).toInt();
    const int toolsOptionsH = settings.contains(globalHeightKey(QStringLiteral("Tools Options")))
                                  ? settings.value(globalHeightKey(QStringLiteral("Tools Options")), toolsOptionsHLegacy).toInt()
                                  : toolsOptionsHLegacy;

    QStringList order = settings.value(globalLeftOrderKey()).toStringList();
    if (order.isEmpty()) {
        order = settings.value(QStringLiteral("CosmicLeftToolPanel/PaneOrder")).toStringList();
    }

    // Restore pane width (stack resize state)
    mPaneWidth = qBound(220, paneW, 600);
    if (mToolsPane) {
        mToolsPane->setFixedWidth(mPaneWidth);
    }
    if (mToolsOptionsPane) {
        mToolsOptionsPane->setFixedWidth(mPaneWidth);
    }
    if (mBrushesPane) {
        mBrushesPane->setFixedWidth(mPaneWidth);
    }
    if (mLayersPane) {
        mLayersPane->setFixedWidth(mPaneWidth);
    }

    if (mPill) {
        QSignalBlocker b(mPill);
        mPill->setLockEnabled(lockOn);
        mPill->setToolsPaneEnabled(normalizedToolsOn);
        mPill->setToolsOptionsPaneEnabled(normalizedToolsOptionsOn);
        mPill->setBrushesPaneEnabled(normalizedBrushesOn);
        // Layers toggle must be set inside the same blocker so it doesn't fire
        // paneEnabledChanged (and trigger syncPaneVisibility/saveSurvivalSettings) during load.
        const bool layersOnLoad = settings.value(globalEnabledKey(QStringLiteral("Layers")), true).toBool();
        mPill->setPaneEnabled(QStringLiteral("Layers"), layersOnLoad);
    }

    if (mToolsPane) {
        QSignalBlocker b(mToolsPane);
        mToolsPane->setExpanded(toolsExpanded);
        const int expandedH = qBound(140, toolsH, 900);
        mToolsPane->setLastExpandedHeight(expandedH);
        mLastExpandedHeights.insert(mToolsPane, expandedH);
        if (toolsExpanded) {
            mToolsPane->setFixedHeight(expandedH);
        } else {
            mToolsPane->setFixedHeight(mToolsPane->sizeHint().height());
        }
    }
    if (mToolsOptionsPane) {
        QSignalBlocker b(mToolsOptionsPane);
        mToolsOptionsPane->setExpanded(toolsOptionsExpanded);
        const int expandedH = qBound(140, toolsOptionsH, 900);
        mToolsOptionsPane->setLastExpandedHeight(expandedH);
        mLastExpandedHeights.insert(mToolsOptionsPane, expandedH);
        if (toolsOptionsExpanded) {
            mToolsOptionsPane->setFixedHeight(expandedH);
        } else {
            mToolsOptionsPane->setFixedHeight(mToolsOptionsPane->sizeHint().height());
        }
    }
    if (mBrushesPane) {
        QSignalBlocker b(mBrushesPane);
        mBrushesPane->setExpanded(brushesExpanded);
        if (brushesExpanded) {
            const int expandedH = qBound(140, brushesH, 900);
            mLastExpandedHeights.insert(mBrushesPane, expandedH);
            mBrushesPane->setFixedHeight(expandedH);
        } else {
            // Keep expanded height for later restore, but collapse visually.
            mLastExpandedHeights.insert(mBrushesPane, qBound(140, brushesH, 900));
            mBrushesPane->setFixedHeight(mBrushesPane->sizeHint().height());
        }
    }

    if (mLayersPane) {
        const bool layersExpanded = settings.value(globalExpandedKey(QStringLiteral("Layers")), true).toBool();
        const int layersH = settings.value(globalHeightKey(QStringLiteral("Layers")), 260).toInt();
        QSignalBlocker b(mLayersPane);
        mLayersPane->setExpanded(layersExpanded);
        const int expandedH = qBound(140, layersH, 900);
        mLayersPane->setLastExpandedHeight(expandedH);
        mLastExpandedHeights.insert(mLayersPane, expandedH);
        if (layersExpanded) {
            mLayersPane->setFixedHeight(expandedH);
        } else {
            mLayersPane->setFixedHeight(mLayersPane->sizeHint().height());
        }
    }

    applyPaneIndexOrder(order);    
    // Initialize auto-hide state based on lock setting
    mAutoHideEnabled = !lockOn;
    if (mAutoHideEnabled) {
        // Unlocked startup should begin in the hidden state (pills only). Showing panes
        // is driven by real user hover/interaction (enterEvent), not by a cursor snapshot
        // taken during initialization.
        hideToolPanels();
    } else {
        showToolPanels();
    }

    if (qApp) {
        QTimer::singleShot(0, this, [this]() {
            QSettings s(PENCIL2D, PENCIL2D);
            CosmicRightToolPanel* other = findOtherPanel();
            if (!other) {
                return;
            }

            // Single source of truth: desired ownership + index comes from global ordered lists.
            const QStringList desiredRight = s.value(globalRightOrderKey()).toStringList();

            const QStringList names = mPanes.keys();
            for (const QString& name : names) {
                if (!desiredRight.contains(name)) {
                    continue;
                }

                bool enabled = true;
                CosmicToolsPane* pane = detachPaneForTransfer(name, &enabled);
                if (!pane) {
                    continue;
                }

                int insertIdx = desiredRight.indexOf(name);
                if (insertIdx < 0) {
                    insertIdx = other->getPaneIndexOrder().size();
                }
                other->attachTransferredPane(pane, name, enabled, insertIdx);
            }
        });
    }
}

void CosmicLeftToolPanel::saveSurvivalSettings() const
{
    QSettings settings(PENCIL2D, PENCIL2D);

    const bool lockOn = mPill ? mPill->lockEnabled() : false;

    settings.setValue(QStringLiteral("CosmicLeftToolPanel/LockEnabled"), lockOn);
    settings.setValue(QStringLiteral("CosmicLeftToolPanel/PaneWidth"), mPaneWidth);

    // Global per-pane state: only save panes this panel currently owns.
    if (mPill) {
        for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
            const QString paneName = it.key();
            CosmicToolsPane* pane = it.value();
            if (!pane) {
                continue;
            }
            const bool enabled = mPill->paneEnabled(paneName);
            settings.setValue(globalEnabledKey(paneName), enabled);
            settings.setValue(globalExpandedKey(paneName), pane->isExpanded());

            // Persist expanded height robustly.
            // Some panes can be expanded while their body is force-hidden (header-only).
            // Never let that overwrite the stored expanded height.
            int h = 0;
            if (pane->isExpanded() && !pane->isBodyForcedHidden()) {
                h = pane->height();
            } else {
                h = pane->lastExpandedHeight() > 0
                        ? pane->lastExpandedHeight()
                        : mLastExpandedHeights.value(pane, pane->height());
            }
            settings.setValue(globalHeightKey(paneName), h);
        }
    }

    const QStringList normalized = normalizedPaneIndexOrder(mPaneIndexOrder);
    settings.setValue(QStringLiteral("CosmicLeftToolPanel/PaneOrder"), normalized);
    settings.setValue(globalLeftOrderKey(), normalized);
    settings.sync();
}

QStringList CosmicLeftToolPanel::normalizedPaneIndexOrder(const QStringList& order)
{
    // Ensure we always have a complete, deterministic list with known names.
    // This is resilient to older settings or partial writes.
    QStringList out;
    const auto isKnown = [](const QString& s) -> bool {
        return s == QStringLiteral("Tools")
            || s == QStringLiteral("Tools Options")
            || s == QStringLiteral("Brushes")
            || s == QStringLiteral("Layers")
            || s == QStringLiteral("Color Wheel")
            || s == QStringLiteral("Color Sliders")
            || s == QStringLiteral("Palettes")
            || s == QStringLiteral("Reference");
    };

    for (const QString& s : order) {
        if (isKnown(s) && !out.contains(s)) {
            out.append(s);
        }
    }
    // Ensure panel is never empty.
    if (out.isEmpty()) {
        out.append(QStringLiteral("Tools"));
    }
    return out;
}

void CosmicLeftToolPanel::applyPaneIndexOrder(const QStringList& order)
{
    if (!mPanesLayout) {
        return;
    }

    mPaneIndexOrder = normalizedPaneIndexOrder(order);

    for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
        if (it.value()) {
            mPanesLayout->removeWidget(it.value());
        }
    }

    QStringList placed;
    for (const QString& s : mPaneIndexOrder) {
        CosmicToolsPane* p = mPanes.value(s, nullptr);
        if (!p) {
            continue;
        }
        mPanesLayout->insertWidget(clampBeforeStretch(mPanesLayout->count()), p);
        placed.append(s);
    }

    // Ensure all panes owned by this panel are in the layout even if settings order is missing/partial.
    for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
        if (!it.value()) {
            continue;
        }
        if (placed.contains(it.key())) {
            continue;
        }
        mPanesLayout->insertWidget(clampBeforeStretch(mPanesLayout->count()), it.value());
        mPaneIndexOrder.append(it.key());
    }

    ensureStretchAtBottom();
    updatePillToggleOrder();
    mPanesLayout->invalidate();
    mPanesLayout->activate();
}

int CosmicLeftToolPanel::reservedWidth() const
{
    const int pillW = (mPill) ? mPill->width() : 22;

    // If auto-hide is enabled and panels are hidden, only return pill width
    if (mAutoHideEnabled && mPanesScrollArea && !mPanesScrollArea->isVisible()) {
        return pillW;
    }

    // Only include slider width if it's visible
    const int sliderW = (mPaneSlider && mPaneSlider->isVisible()) ? mPaneSlider->width() : 0;

    int panesW = 0;
    // Use dynamic panes map instead of fixed pointers
    for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
        CosmicToolsPane* pane = it.value();
        if (pane && pane->isVisible()) {
            panesW = qMax(panesW, pane->width());
        }
    }

    if (panesW <= 0) {
        return pillW;
    }

    // Layout order: pill, [slider if visible], panes
    int totalWidth = pillW + panesW;
    if (sliderW > 0) {
        totalWidth += sliderW;
    }
    return totalWidth;
}

void CosmicLeftToolPanel::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    
    // Draw background for the stack area — left corners flat (face the scrollbar), right corners rounded
    if (mPanesScrollArea && mPanesScrollArea->isVisible()) {
        const QRectF rc = mPanesScrollArea->geometry();

        QPainterPath path;
        path.addRect(rc); // all corners flat

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x1f, 0x1f, 0x1f, 0xff));
        p.drawPath(path);
    }
}

void CosmicLeftToolPanel::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateStackResizeGripGeometry();
    updatePaneResizeSplitterGeometry();
}

void CosmicLeftToolPanel::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() != QEvent::LanguageChange) return;

    // Pane keys ("Tools", "Tools Options", etc.) are settings keys — never translate them.
    // Only update the visible title label inside each pane header.
    auto retitle = [](CosmicToolsPane* pane, const char* src) {
        if (!pane) return;
        const QString t = QCoreApplication::translate("CosmicLeftToolPanel", src);
        pane->setDisplayTitle(t.isEmpty() ? QString::fromUtf8(src) : t);
    };
    retitle(mToolsPane,        "Tools");
    retitle(mToolsOptionsPane, "Tools Options");
    retitle(mBrushesPane,      "Brushes");
    retitle(mLayersPane,       "Papers");
}

void CosmicLeftToolPanel::beginPaneVResize(CosmicToolsPane* pane, const QPoint& globalPos)
{
    if (!pane || !pane->isVisible()) {
        return;
    }
    if (!mPanesContainer || pane->parentWidget() != mPanesContainer) {
        return;
    }
    mIsVResizing = true;
    mVResizePane = pane;
    mVResizeStartY = globalPos.y();
    mVResizeStartHeight = pane->height();
}

void CosmicLeftToolPanel::updatePaneVResize(const QPoint& globalPos)
{
    if (!mIsVResizing || !mVResizePane) {
        return;
    }
    if (!mPanesContainer || mVResizePane->parentWidget() != mPanesContainer) {
        mIsVResizing = false;
        mVResizePane = nullptr;
        return;
    }

    const int deltaY = globalPos.y() - mVResizeStartY;
    int newH = mVResizeStartHeight + deltaY;
    
    // Find pane name and use pane-specific height bounds
    QString paneName;
    for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
        if (it.value() == mVResizePane) {
            paneName = it.key();
            break;
        }
    }
    
    int minH, maxH;
    if (paneName == QStringLiteral("Color Wheel")) {
        minH = 260; maxH = 900; // Color Wheel needs more minimum height
    } else {
        minH = 140; maxH = 900; // Default bounds for other panes
    }
    newH = qBound(minH, newH, maxH);

    if (newH == mVResizePane->height()) {
        return;
    }

    mVResizePane->setFixedHeight(newH);
    // Keep expanded-height bookkeeping in sync with interactive resizing.
    mVResizePane->setLastExpandedHeight(newH);
    mLastExpandedHeights.insert(mVResizePane, newH);
    updateGeometry();
    update();
    emit reservedWidthChanged();
}

void CosmicLeftToolPanel::endPaneVResize()
{
    if (!mIsVResizing) {
        return;
    }
    mIsVResizing = false;
    mVResizePane = nullptr;

    if (mPanesContainer) {
        mPanesContainer->updateGeometry();
        mPanesContainer->adjustSize();
    }

    saveSurvivalSettings();
    updatePaneResizeSplitterGeometry();
}

void CosmicLeftToolPanel::updatePaneResizeSplitterGeometry()
{
    if (!mPanesContainer || !mPanesLayout) {
        return;
    }

    // Hide all fixed splitters initially
    if (mToolsResizeSplitter) {
        mToolsResizeSplitter->setVisible(false);
        mToolsResizeSplitter->setEnabled(false);
    }
    if (mToolsOptionsResizeSplitter) {
        mToolsOptionsResizeSplitter->setVisible(false);
        mToolsOptionsResizeSplitter->setEnabled(false);
    }
    if (mBrushesResizeSplitter) {
        mBrushesResizeSplitter->setVisible(false);
        mBrushesResizeSplitter->setEnabled(false);
    }
    if (mLayersResizeSplitter) {
        mLayersResizeSplitter->setVisible(false);
        mLayersResizeSplitter->setEnabled(false);
    }

    const int splitterH = mGap; // Same height as the gap between panes
    const int splitterW = mPanesContainer->width();

    // Update fixed splitters for original panes
    updatePaneSplitter(mToolsPane, mToolsResizeSplitter, splitterW, splitterH);
    updatePaneSplitter(mToolsOptionsPane, mToolsOptionsResizeSplitter, splitterW, splitterH);
    updatePaneSplitter(mBrushesPane, mBrushesResizeSplitter, splitterW, splitterH);
    updatePaneSplitter(mLayersPane, mLayersResizeSplitter, splitterW, splitterH);
}

void CosmicLeftToolPanel::updatePaneSplitter(CosmicToolsPane* pane, QWidget* splitter, int splitterW, int splitterH)
{
    if (!pane || !splitter) {
        return;
    }

    if (!mPanesContainer || pane->parentWidget() != mPanesContainer) {
        return;
    }

    // Only show splitter if pane is visible and has vertical resize enabled
    if (!pane->isVisible() || !pane->isVerticalResizeGripEnabled() || !pane->isExpanded()) {
        return;
    }

    // Position splitter below the pane, aligned with the gap
    const QRect paneRect = pane->geometry();
    const int splitterY = paneRect.bottom() + (mGap - splitterH) / 2;
    
    splitter->setGeometry(0, splitterY, splitterW, splitterH);
    splitter->setVisible(true);
    splitter->setEnabled(true);
    splitter->raise();
}

void CosmicLeftToolPanel::beginStackResize(const QPoint& globalPos)
{
    mIsResizing = true;
    mResizeStartX = globalPos.x();
    mResizeStartWidth = mPaneWidth;
}

void CosmicLeftToolPanel::updateStackResize(const QPoint& globalPos)
{
    if (!mIsResizing) {
        return;
    }

    const int deltaX = globalPos.x() - mResizeStartX;
    int newWidth = mResizeStartWidth + deltaX;

    const int minWidth = 200;
    const int maxWidth = 600;
    newWidth = qBound(minWidth, newWidth, maxWidth);

    if (newWidth == mPaneWidth) {
        return;
    }

    mPaneWidth = newWidth;

    // Apply immediately for realtime resizing feedback.
    if (mPanesContainer) {
        for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
            CosmicToolsPane* pane = it.value();
            if (!pane) {
                continue;
            }
            if (pane->parentWidget() != mPanesContainer) {
                continue;
            }
            pane->setFixedWidth(mPaneWidth);
        }
    }

    updateGeometry();
    update();
    emit reservedWidthChanged();

    updatePaneResizeSplitterGeometry();
}

void CosmicLeftToolPanel::endStackResize()
{
    mIsResizing = false;
    
    // Apply the stored width changes now that drag is complete
    if (mPanesContainer) {
        for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
            CosmicToolsPane* pane = it.value();
            if (!pane) {
                continue;
            }
            if (pane->parentWidget() != mPanesContainer) {
                continue;
            }
            pane->setFixedWidth(mPaneWidth);
        }
    }
    
    updateGeometry();
    update();
    emit reservedWidthChanged();

    saveSurvivalSettings();
}

void CosmicLeftToolPanel::updateStackResizeGripGeometry()
{
    if (!mStackResizeGrip || !mPanesScrollArea) {
        return;
    }
    const QRect sr = mPanesScrollArea->geometry();
    const int gripW = 6;
    // Left panel: grip on the right edge of the scroll area
    mStackResizeGrip->setGeometry(sr.right() - gripW, sr.top(), gripW, sr.height());
    mStackResizeGrip->raise();
}

void CosmicLeftToolPanel::syncPaneVisibility()
{
    if (!mPill) {
        return;
    }

    const auto isNonResizablePane = [](const QString& name) -> bool {
        return name == QStringLiteral("Tools")
            || name == QStringLiteral("Tools Options")
            || name == QStringLiteral("Color Sliders");
    };

    const auto ensureInLayout = [this](const QString& paneName, CosmicToolsPane* pane) {
        if (!pane || !mPanesLayout) {
            return;
        }
        if (mPanesLayout->indexOf(pane) >= 0) {
            return;
        }
        int insertIdx = clampBeforeStretch(mPaneIndexOrder.indexOf(paneName));
        if (insertIdx < 0) {
            insertIdx = clampBeforeStretch(mPanesLayout->count());
        }
        mPanesLayout->insertWidget(insertIdx, pane);
        mPanesLayout->invalidate();
        mPanesLayout->activate();
    };

    for (auto it = mPanes.begin(); it != mPanes.end(); ++it) {
        const QString paneName = it.key();
        CosmicToolsPane* pane = it.value();
        if (!pane) {
            continue;
        }
        const bool on = mPill->paneEnabled(paneName);
        pane->setVisible(on);
        ensureInLayout(paneName, pane);

        const bool nonResizable = isNonResizablePane(paneName);
        pane->setResizeGripEnabled(on && !nonResizable);
        pane->setVerticalResizeGripEnabled(on && !nonResizable);

        if (nonResizable) {
            pane->setMinimumHeight(pane->sizeHint().height());
            pane->setMaximumHeight(QWIDGETSIZE_MAX);
            pane->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        }
    }

    // Nudge the scroll area container so it re-evaluates its total height and the
    // scrollbar shrinks back when panes are hidden or collapsed.
    if (mPanesContainer) {
        mPanesContainer->updateGeometry();
        mPanesContainer->adjustSize();
    }

    updateGeometry();
    update();
    emit reservedWidthChanged();

    updatePaneResizeSplitterGeometry();
}

void CosmicLeftToolPanel::updateSliderVisibility()
{
    // Hide slider if all toggles are OFF (no enabled panes)
    // Check toggle states directly, not pane visibility, since panes start hidden
    bool anyToggleOn = false;
    if (mPill) {
        for (const QString& name : mPaneIndexOrder) {
            if (mPill->paneEnabled(name)) {
                anyToggleOn = true;
                break;
            }
        }
    }
    const bool panesVisible = (mPanesScrollArea && mPanesScrollArea->isVisible());
    const bool locked = (mPill && mPill->lockEnabled());
    
    if (mPaneSlider) {
        if (locked) {
            mPaneSlider->setVisible(true);
        } else {
            mPaneSlider->setVisible(panesVisible && anyToggleOn);
        }
    }
    
    // Update layout and reserved width when slider visibility changes
    updateGeometry();
    update();
    emit reservedWidthChanged();
}

int CosmicLeftToolPanel::stretchIndex() const
{
    if (!mPanesLayout) {
        return -1;
    }
    // We always add a final stretch to keep panes pushed to the top.
    return mPanesLayout->count() - 1;
}

int CosmicLeftToolPanel::clampBeforeStretch(int idx) const
{
    const int sIdx = stretchIndex();
    if (sIdx < 0) {
        return qMax(0, idx);
    }
    return qBound(0, idx, qMax(0, sIdx));
}

void CosmicLeftToolPanel::movePlaceholderTo(int layoutIdx)
{
    if (!mPanesLayout || !mDragPlaceholder) {
        return;
    }

    const int current = layoutIndexOf(mDragPlaceholder);
    const int target = clampBeforeStretch(layoutIdx);
    if (current == target) {
        return;
    }

    mPanesLayout->removeWidget(mDragPlaceholder);
    mPanesLayout->insertWidget(target, mDragPlaceholder);
    mPanesLayout->invalidate();
    mPanesLayout->activate();
    animatePanesToLayout();
}

void CosmicLeftToolPanel::moveExternalPlaceholderTo(int layoutIdx)
{
    if (!mPanesLayout || !mExternalPlaceholder) {
        return;
    }

    const int current = layoutIndexOf(mExternalPlaceholder);
    const int target = clampBeforeStretch(layoutIdx);
    if (current == target) {
        return;
    }

    mPanesLayout->removeWidget(mExternalPlaceholder);
    mPanesLayout->insertWidget(target, mExternalPlaceholder);
    mPanesLayout->invalidate();
    mPanesLayout->activate();
}

void CosmicLeftToolPanel::animatePanesToLayout()
{
    if (!mPanesLayout || !mPanesContainer) {
        return;
    }

    mPanesContainer->layout()->invalidate();
    mPanesContainer->layout()->activate();

    const int durationMs = 70;

    for (int i = 0; i < mPanesLayout->count(); ++i) {
        QLayoutItem* item = mPanesLayout->itemAt(i);
        QWidget* w = item ? item->widget() : nullptr;
        if (!w || !w->isVisible()) {
            continue;
        }
        if (w == mDraggingPane) {
            continue;
        }

        const QRect newGeom = w->geometry();
        const QRect oldGeom = mLastGeometries.value(w, newGeom);
        mLastGeometries.insert(w, newGeom);

        if (oldGeom == newGeom) {
            continue;
        }

        auto* anim = new QPropertyAnimation(w, "geometry", w);
        anim->setDuration(durationMs);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(oldGeom);
        anim->setEndValue(newGeom);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

int CosmicLeftToolPanel::layoutIndexOf(QWidget* w) const
{
    if (!mPanesLayout || !w) {
        return -1;
    }
    return mPanesLayout->indexOf(w);
}

void CosmicLeftToolPanel::ensureReservedInsetUpdated()
{
    emit reservedWidthChanged();
}

void CosmicLeftToolPanel::onPaneDragStarted(CosmicToolsPane* pane, const QPoint& globalPos)
{
    if (!pane || !mPanesContainer || !mPanesLayout) {
        return;
    }
    if (mDraggingPane) {
        return;
    }
    if (!pane->isVisible()) {
        return;
    }

    mDraggingPane = pane;

    // Store original geometries before any layout changes for consistent reorder thresholds
    mOriginalGeometries.clear();
    if (mPanesLayout) {
        for (int i = 0; i < mPanesLayout->count(); ++i) {
            QLayoutItem* item = mPanesLayout->itemAt(i);
            QWidget* w = item ? item->widget() : nullptr;
            if (w) {
                mOriginalGeometries.insert(w, w->geometry());
            }
        }
    }

    mLastGeometries.clear();
    if (mPanesLayout) {
        for (int i = 0; i < mPanesLayout->count(); ++i) {
            QLayoutItem* item = mPanesLayout->itemAt(i);
            QWidget* w = item ? item->widget() : nullptr;
            if (w) {
                mLastGeometries.insert(w, w->geometry());
            }
        }
    }

    const QPoint topLeftGlobal = pane->mapToGlobal(QPoint(0, 0));
    mDragGrabOffset = globalPos - topLeftGlobal;

    const int idx = layoutIndexOf(pane);
    if (idx < 0) {
        mDraggingPane = nullptr;
        return;
    }

    mDragPlaceholder = new QWidget(mPanesContainer);
    mDragPlaceholder->setFixedSize(pane->size());
    mDragPlaceholder->setVisible(true);

    mPanesLayout->insertWidget(clampBeforeStretch(idx), mDragPlaceholder);
    mPanesLayout->removeWidget(pane);
    mPanesLayout->invalidate();
    mPanesLayout->activate();

    // Float the pane above the layout so we can move it smoothly.
    pane->setParent(mPanesContainer);
    pane->raise();
    pane->show();

    const QPoint local = mPanesContainer->mapFromGlobal(globalPos - mDragGrabOffset);
    const int x = pane->x();
    pane->setGeometry(x, local.y(), pane->width(), pane->height());
    pane->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void CosmicLeftToolPanel::onPaneDragMoved(const QPoint& globalPos)
{
    if (!mDraggingPane || !mPanesContainer || !mPanesLayout || !mDragPlaceholder) {
        return;
    }

    const QPoint localTopLeft = mPanesContainer->mapFromGlobal(globalPos - mDragGrabOffset);
    const int x = mDraggingPane->x();
    int y = localTopLeft.y();

    // Clamp to container bounds for stability.
    const int minY = 0;
    const int maxY = qMax(0, mPanesContainer->height() - mDraggingPane->height());
    y = qBound(minY, y, maxY);

    mDraggingPane->setGeometry(x, y, mDraggingPane->width(), mDraggingPane->height());

    // Use the cursor position (within the container) as the ordering driver.
    // This allows intuitive upward reordering even when the dragged pane's
    // geometry is clamped at the top of the container.
    const int cursorY = mPanesContainer->mapFromGlobal(globalPos).y();
    const int placeholderIdx = layoutIndexOf(mDragPlaceholder);
    if (placeholderIdx < 0) {
        return;
    }

    // Threshold swap: when dragged center crosses another pane's original center, move the placeholder.
    // This makes the reorder behavior consistent regardless of live position changes during drag.
    const int sIdx = stretchIndex();
    const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);

    for (int i = 0; i <= maxIdx; ++i) {
        QLayoutItem* item = mPanesLayout->itemAt(i);
        QWidget* w = item ? item->widget() : nullptr;
        if (!w || w == mDragPlaceholder) {
            continue;
        }
        if (!w->isVisible()) {
            continue;
        }

        // Use original geometry for consistent threshold calculation
        const QRect originalRect = mOriginalGeometries.value(w, w->geometry());
        const int otherCenterY = originalRect.center().y();

        if (cursorY < otherCenterY && placeholderIdx > i) {
            movePlaceholderTo(i);
            break;
        }
        if (cursorY > otherCenterY && placeholderIdx < i) {
            movePlaceholderTo(i);  // Use same logic as bottom-to-top case
            break;
        }
    }
}

void CosmicLeftToolPanel::onPaneDragEnded()
{
    if (!mDraggingPane || !mPanesLayout || !mDragPlaceholder) {
        mDraggingPane = nullptr;
        return;
    }

    const int placeholderIdx = layoutIndexOf(mDragPlaceholder);
    if (placeholderIdx < 0) {
        mDraggingPane = nullptr;
        return;
    }

    // Restore the pane to its final position in the layout.
    mPanesLayout->insertWidget(placeholderIdx, mDraggingPane);
    mPanesLayout->removeWidget(mDragPlaceholder);
    mPanesLayout->invalidate();
    mPanesLayout->activate();

    delete mDragPlaceholder;
    mDragPlaceholder = nullptr;

    mDraggingPane->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    mDraggingPane->updateGeometry();
    mDraggingPane = nullptr;

    updateGeometry();
    update();
    ensureReservedInsetUpdated();
    
    // Update index system (single source of truth) and sync pill
    updatePaneIndexOrder();
    updatePillToggleOrder();

    saveSurvivalSettings();
    
    // Ensure stretch stays at bottom to prevent gaps
    ensureStretchAtBottom();
}

QStringList CosmicLeftToolPanel::getCurrentPaneOrder() const
{
    QStringList order;
    if (!mPanesLayout) {
        return order;
    }

    const int sIdx = stretchIndex();
    const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);

    for (int i = 0; i <= maxIdx; ++i) {
        QLayoutItem* item = mPanesLayout->itemAt(i);
        QWidget* w = item ? item->widget() : nullptr;
        if (w && w->isVisible()) {
            if (w == mToolsPane) {
                order.append("Tools");
            } else if (w == mToolsOptionsPane) {
                order.append("Tools Options");
            } else if (w == mBrushesPane) {
                order.append("Brushes");
            } else if (w == mLayersPane) {
                order.append("Layers");
            }
        }
    }

    return order;
}

void CosmicLeftToolPanel::updatePillToggleOrder()
{
    if (!mPill) {
        return;
    }

    // Use the index system as single source of truth
    const QStringList currentOrder = getPaneIndexOrder();
    mPill->updateToggleOrder(currentOrder);
}

QStringList CosmicLeftToolPanel::getPaneIndexOrder() const
{
    return mPaneIndexOrder;
}

void CosmicLeftToolPanel::updatePaneIndexOrder()
{
    // Update the index system based on actual layout positions
    mPaneIndexOrder.clear();
    
    if (!mPanesLayout) {
        return;
    }

    const int sIdx = stretchIndex();
    const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);

    for (int i = 0; i <= maxIdx; ++i) {
        QLayoutItem* item = mPanesLayout->itemAt(i);
        QWidget* w = item ? item->widget() : nullptr;
        if (!w) {
            continue;
        }
        for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
            if (it.value() == w) {
                mPaneIndexOrder.append(it.key());
                break;
            }
        }
    }

    mPaneIndexOrder = normalizedPaneIndexOrder(mPaneIndexOrder);
}

void CosmicLeftToolPanel::ensureStretchAtBottom()
{
    if (!mPanesLayout) {
        return;
    }

    // Check if the last item is a stretch (spacer)
    const int count = mPanesLayout->count();
    if (count <= 0) {
        return;
    }

    // Get the last item
    QLayoutItem* lastItem = mPanesLayout->itemAt(count - 1);
    if (!lastItem) {
        return;
    }

    // If the last item is not a spacer, remove all spacers and add one at the end
    if (lastItem->spacerItem() == nullptr) {
        // Remove any existing spacers
        for (int i = count - 1; i >= 0; --i) {
            QLayoutItem* item = mPanesLayout->itemAt(i);
            if (item && item->spacerItem()) {
                mPanesLayout->removeItem(item);
                delete item;
            }
        }
        
        // Add a new stretch at the bottom
        mPanesLayout->addStretch(1);
        mPanesLayout->invalidate();
        mPanesLayout->activate();
    }
}

bool CosmicLeftToolPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mStackResizeGrip && event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                beginStackResize(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mIsResizing) {
                updateStackResize(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mIsResizing && me->button() == Qt::LeftButton) {
                endStackResize();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    // Handle per-pane resize splitters
    CosmicToolsPane* paneAbove = nullptr;
    if (watched == mToolsResizeSplitter) {
        paneAbove = mToolsPane;
    } else if (watched == mToolsOptionsResizeSplitter) {
        paneAbove = mToolsOptionsPane;
    } else if (watched == mBrushesResizeSplitter) {
        paneAbove = mBrushesPane;
    } else if (watched == mLayersResizeSplitter) {
        paneAbove = mLayersPane;
    }

    if (paneAbove && event) {

        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && paneAbove && paneAbove->isVisible() && paneAbove->isVerticalResizeGripEnabled()) {
                beginPaneVResize(paneAbove, me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mIsVResizing) {
                updatePaneVResize(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mIsVResizing && me->button() == Qt::LeftButton) {
                endPaneVResize();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CosmicLeftToolPanel::enterEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (mAutoHideEnabled) {
        showToolPanels();
        mAutoHideTimer->stop();
    }
}

void CosmicLeftToolPanel::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (mAutoHideEnabled) {
        if (mMovingPane) {
            return;
        }
        // Start timer to hide after delay
        mAutoHideTimer->start();
    }
}

void CosmicLeftToolPanel::updateAutoHideVisibility()
{
    if (mAutoHideEnabled) {
        if (isCursorOverToolArea(QCursor::pos())) {
            showToolPanels();
        } else {
            hideToolPanels();
        }
    } else {
        showToolPanels(); // Always show when locked
    }
}

void CosmicLeftToolPanel::showToolPanels()
{
    if (mPanesScrollArea) {
        mPanesScrollArea->show();
    }
    updateSliderVisibility();
    updateGeometry();
    update();
    emit reservedWidthChanged();
}

void CosmicLeftToolPanel::hideToolPanels()
{
    if (mPanesScrollArea) {
        mPanesScrollArea->hide();
    }
    updateSliderVisibility();
    updateGeometry();
    update();
    emit reservedWidthChanged();
}

bool CosmicLeftToolPanel::isCursorOverToolArea(const QPoint& globalPos) const
{
    // Check if cursor is over the pill area
    if (mPill && mPill->rect().contains(mPill->mapFromGlobal(globalPos))) {
        return true;
    }
    
    // Check if cursor is over the panes area
    if (mPanesScrollArea && mPanesScrollArea->rect().contains(mPanesScrollArea->mapFromGlobal(globalPos))) {
        return true;
    }
    
    // Check if cursor is over the slider
    if (mPaneSlider && mPaneSlider->rect().contains(mPaneSlider->mapFromGlobal(globalPos))) {
        return true;
    }
    
    return false;
}

namespace {
QIcon makeTintedIconFromSvg(const QString& iconName, const QColor& tint, const QSize& size)
{
    const QString path = QStringLiteral(":/Interaction/") + iconName + QStringLiteral(".svg");
    QSvgRenderer r(path);
    if (!r.isValid()) {
        return QIcon();
    }
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        r.render(&p);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(img.rect(), tint);
    }
    return QIcon(QPixmap::fromImage(img));
}
}

CosmicRightToolPanel* CosmicLeftToolPanel::findOtherPanel() const
{
    QWidget* p = parentWidget();
    if (!p) {
        return nullptr;
    }
    return p->findChild<CosmicRightToolPanel*>(QString(), Qt::FindDirectChildrenOnly);
}

bool CosmicLeftToolPanel::acceptsTransferAt(const QPoint& globalPos) const
{
    if (!mPanesScrollArea) {
        return false;
    }
    // Include the pill so dragging over it reveals the hidden panel
    if (mPill && mPill->rect().contains(mPill->mapFromGlobal(globalPos))) {
        return true;
    }
    return mPanesScrollArea->rect().contains(mPanesScrollArea->mapFromGlobal(globalPos));
}

void CosmicLeftToolPanel::clearExternalPlaceholder()
{
    if (mExternalPlaceholder) {
        if (mPanesLayout) {
            mPanesLayout->removeWidget(mExternalPlaceholder);
        }
        delete mExternalPlaceholder;
        mExternalPlaceholder = nullptr;
    }
    mExternalIncomingName.clear();
    mExternalOriginalGeometries.clear();
    hideDropZoneIndicator();
}

void CosmicLeftToolPanel::showDropZoneIndicator(const QRect& area)
{
    if (!mDropZoneIndicator) {
        mDropZoneIndicator = new CosmicDropZoneIndicator(mPanesContainer);
        mDropZoneIndicator->setObjectName(QStringLiteral("DropZoneIndicator"));
    }
    
    mDropZoneIndicator->setGeometry(area);
    mDropZoneIndicator->show();
    mDropZoneIndicator->raise();
}

void CosmicLeftToolPanel::hideDropZoneIndicator()
{
    if (mDropZoneIndicator) {
        mDropZoneIndicator->hide();
        delete mDropZoneIndicator;
        mDropZoneIndicator = nullptr;
    }
}

void CosmicLeftToolPanel::updateDropZoneIndicator(const QPoint& globalPos)
{
    if (!acceptsTransferAt(globalPos)) {
        hideDropZoneIndicator();
        return;
    }

    if (!mPanesLayout || !mPanesContainer) {
        hideDropZoneIndicator();
        return;
    }

    // Only show an indicator for the "dock as last pane" case (blank area below all panes).
    const int sIdx = stretchIndex();
    const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);

    QWidget* lastPane = nullptr;
    for (int i = maxIdx; i >= 0; --i) {
        QWidget* w = mPanesLayout->itemAt(i) ? mPanesLayout->itemAt(i)->widget() : nullptr;
        if (!w || w == mExternalPlaceholder || !w->isVisible()) {
            continue;
        }
        lastPane = w;
        break;
    }

    const int cursorY = mPanesContainer->mapFromGlobal(globalPos).y();

    const int lastBottom = lastPane ? lastPane->geometry().bottom() : 0;
    const int blankHeight = mPanesContainer->height() - lastBottom;

    // Only show when the cursor is actually in the blank region.
    if (blankHeight > 0 && cursorY >= lastBottom) {
        showDropZoneIndicator(QRect(0, lastBottom, mPanesContainer->width(), blankHeight));
    } else {
        hideDropZoneIndicator();
    }
}

void CosmicLeftToolPanel::externalTransferHover(CosmicToolsPane* pane, const QString& paneName, const QPoint& globalPos)
{
    if (!mPanesLayout || !mPanesContainer) {
        return;
    }
    if (mAutoHideEnabled && mPanesScrollArea && !mPanesScrollArea->isVisible()) {
        showToolPanels();
    }
    if (!acceptsTransferAt(globalPos)) {
        clearExternalPlaceholder();
        return;
    }

    if (!mExternalPlaceholder) {
        mExternalIncomingName = paneName;
        mExternalPlaceholder = new QWidget(mPanesContainer);
        const QSize sz = pane ? pane->size() : QSize(mPaneWidth, 120);
        mExternalPlaceholder->setFixedSize(sz);
        mExternalPlaceholder->setVisible(true);
        
        // Capture original geometries of all panes for stable thresholds
        mExternalOriginalGeometries.clear();
        const int sIdx = stretchIndex();
        const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);
        for (int i = 0; i <= maxIdx; ++i) {
            QLayoutItem* item = mPanesLayout->itemAt(i);
            QWidget* w = item ? item->widget() : nullptr;
            if (w && w->isVisible()) {
                mExternalOriginalGeometries.insert(w, w->geometry());
            }
        }
        
        mPanesLayout->insertWidget(clampBeforeStretch(0), mExternalPlaceholder);
        mPanesLayout->invalidate();
        mPanesLayout->activate();
    }

    // Update drop zone indicator to show where pane will be docked
    updateDropZoneIndicator(globalPos);

    // For floating transfers, use cursor position with original geometries for stable thresholds
    const int cursorY = mPanesContainer->mapFromGlobal(globalPos).y();
    const int placeholderIdx = layoutIndexOf(mExternalPlaceholder);
    if (placeholderIdx < 0) {
        return;
    }

    // Use original geometries for consistent threshold calculation (like normal reorder)
    const int sIdx = stretchIndex();
    const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);
    
    int targetIdx = placeholderIdx;
    for (int i = 0; i <= maxIdx; ++i) {
        QWidget* w = mPanesLayout->itemAt(i) ? mPanesLayout->itemAt(i)->widget() : nullptr;
        if (!w || w == mExternalPlaceholder) {
            continue;
        }
        if (!w->isVisible()) {
            continue;
        }

        // Use original geometry for stable threshold calculation
        const QRect originalRect = mExternalOriginalGeometries.value(w, w->geometry());
        const int otherCenterY = originalRect.center().y();

        if (cursorY < otherCenterY && placeholderIdx > i) {
            targetIdx = i;
            break;
        }
        if (cursorY > otherCenterY && placeholderIdx < i) {
            targetIdx = i + 1;
        }
    }

    targetIdx = clampBeforeStretch(targetIdx);
    if (placeholderIdx != targetIdx) {
        moveExternalPlaceholderTo(targetIdx);
    }
}

void CosmicLeftToolPanel::externalTransferLeave()
{
    clearExternalPlaceholder();
}

void CosmicLeftToolPanel::externalTransferDrop(CosmicToolsPane* pane, const QString& paneName, bool enabled)
{
    if (!pane || !mPanesLayout || !mPanesContainer || !mExternalPlaceholder) {
        clearExternalPlaceholder();
        return;
    }

    const int insertIdx = clampBeforeStretch(layoutIndexOf(mExternalPlaceholder));

    mPanesLayout->removeWidget(mExternalPlaceholder);
    delete mExternalPlaceholder;
    mExternalPlaceholder = nullptr;
    mExternalIncomingName.clear();

    // Re-embed the pane using centralized helper
    setPaneFloating(pane, false);

    // Set side and bind to this panel
    pane->setSide(CosmicToolsPane::Side::Left);
    bindPaneToThisPanel(pane, paneName);

    // Connect vertical resize for all transferred panes (they need to be resizable)
    connect(pane, &CosmicToolsPane::resizeVDragStarted, this, [this, pane](const QPoint& gp) {
        beginPaneVResize(pane, gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::resizeVDragMoved, this, [this](const QPoint& gp) {
        updatePaneVResize(gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::resizeVDragEnded, this, [this]() {
        endPaneVResize();
    }, Qt::UniqueConnection);

    // Register pane and create pill toggle
    mPanes.insert(paneName, pane);
    const QString iconName = mPaneIcons.value(paneName);
    if (mPill && !iconName.isEmpty()) {
        mPill->ensureToggle(paneName, iconName);
        mPill->setPaneEnabled(paneName, enabled);
    }

    mPanesLayout->insertWidget(insertIdx, pane);
    mPaneIndexOrder.removeAll(paneName);
    mPaneIndexOrder.insert(insertIdx, paneName);
    mPaneIndexOrder = normalizedPaneIndexOrder(mPaneIndexOrder);
    updatePillToggleOrder();

    if (mPill) {
        mPill->setPaneEnabled(paneName, enabled);
    }

    syncPaneVisibility();
    updateSliderVisibility();
    saveSurvivalSettings();
    ensureStretchAtBottom();

    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(QStringLiteral("CosmicToolsPaneOwnership/") + paneName, QStringLiteral("Left"));
        settings.sync();
    }
}

void CosmicLeftToolPanel::applyTransferCursor(const QString& iconName)
{
    const QColor accent(CosmicColor::accent());
    const QSize sz(26, 26);
    QIcon ic = makeTintedIconFromSvg(iconName, accent, sz);
    if (ic.isNull()) {
        return;
    }
    QPixmap px = ic.pixmap(sz);
    QApplication::setOverrideCursor(QCursor(px, px.width() / 2, px.height() / 2));
}

void CosmicLeftToolPanel::clearTransferCursor()
{
    if (mTransferCursorApplied && QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
        mTransferCursorApplied = false;
    }
}

void CosmicLeftToolPanel::clearMoveState(bool restoreIfNeeded)
{
    CosmicRightToolPanel* other = findOtherPanel();
    if (other) {
        other->externalTransferLeave();
    }
    clearExternalPlaceholder();
    clearTransferCursor();

    mMoveOriginalGeometries.clear();

    if (restoreIfNeeded && mMovingPane && mPanesLayout && mPanesContainer) {
        // Re-embed the pane using centralized helper
        setPaneFloating(mMovingPane, false);
        mMovingPane->setSide(CosmicToolsPane::Side::Left);
        const int idx = clampBeforeStretch(mMoveOriginIndex >= 0 ? mMoveOriginIndex : 0);
        mPanesLayout->insertWidget(idx, mMovingPane);
        mMovingPane->show();
        mMovingPane->raise();
    }

    if (mMovePlaceholder) {
        if (mPanesLayout) {
            mPanesLayout->removeWidget(mMovePlaceholder);
        }
        delete mMovePlaceholder;
        mMovePlaceholder = nullptr;
    }

    mMovingPane = nullptr;
    mMovingPaneName.clear();
    mMovingPaneWasEnabled = true;
    mMoveOriginIndex = -1;
    mMoveOverOther = false;
}

void CosmicLeftToolPanel::onPaneMoveDragStarted(CosmicToolsPane* pane, const QPoint& globalPos)
{
    if (!pane || !mPanesLayout || !mPanesContainer) {
        return;
    }
    if (mMovingPane) {
        return;
    }

    QString paneName;
    for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
        if (it.value() == pane) {
            paneName = it.key();
            break;
        }
    }
    if (paneName.isEmpty()) {
        return;
    }

    // Block undock if no enabled pane would remain visible afterward.
    bool willHaveVisible = false;
    for (auto it = mPanes.constBegin(); it != mPanes.constEnd(); ++it) {
        if (it.value() != pane && mPill && mPill->paneEnabled(it.key())) {
            willHaveVisible = true;
            break;
        }
    }
    if (!willHaveVisible) {
        CosmicWarningPopup* w = new CosmicWarningPopup(this);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setTitleText(tr("Dock Area Empty"));
        w->setBodyText(mPanes.size() > 1
            ? tr("This dock area needs at least one active dock.\nYou have inactive docks — enable at least one before undocking.")
            : tr("This dock area needs at least one dock.\nCannot undock the last remaining dock."));
        w->exec();
        return;
    }

    mMovingPane = pane;
    mMovingPaneName = paneName;
    mMovingPaneWasEnabled = mPill ? mPill->paneEnabled(paneName) : true;
    mMoveOriginIndex = layoutIndexOf(pane);

    mMoveGrabOffset = globalPos - pane->mapToGlobal(QPoint(0, 0));

    mMovePlaceholder = new QWidget(mPanesContainer);
    mMovePlaceholder->setFixedSize(pane->size());
    mMovePlaceholder->setVisible(true);

    const int idx = clampBeforeStretch(mMoveOriginIndex);
    mPanesLayout->insertWidget(idx, mMovePlaceholder);
    mPanesLayout->removeWidget(pane);
    mPanesLayout->invalidate();
    mPanesLayout->activate();

    mMoveOriginalGeometries.clear();
    {
        const int sIdx = stretchIndex();
        const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);
        for (int i = 0; i <= maxIdx; ++i) {
            QLayoutItem* item = mPanesLayout->itemAt(i);
            QWidget* w = item ? item->widget() : nullptr;
            if (!w || w == mMovePlaceholder || !w->isVisible()) {
                continue;
            }
            mMoveOriginalGeometries.insert(w, w->geometry());
        }
    }

    // Seamless detach using centralized helper
    setPaneFloating(pane, true, globalPos);

}

void CosmicLeftToolPanel::onPaneMoveDragMoved(const QPoint& globalPos)
{
    if (!mMovingPane) {
        return;
    }

    mMovingPane->move(globalPos - mMoveGrabOffset);
    
    if (mMovingPane && acceptsTransferAt(globalPos)) {
        updateDropZoneIndicator(globalPos);
        if (mMovePlaceholder && mPanesLayout && mPanesContainer) {
            const int cursorY = mPanesContainer->mapFromGlobal(globalPos).y();
            const int placeholderIdx = layoutIndexOf(mMovePlaceholder);
            if (placeholderIdx >= 0) {
                const int sIdx = stretchIndex();
                const int maxIdx = (sIdx >= 0) ? sIdx - 1 : (mPanesLayout->count() - 1);

                int targetIdx = placeholderIdx;
                for (int i = 0; i <= maxIdx; ++i) {
                    QLayoutItem* item = mPanesLayout->itemAt(i);
                    QWidget* w = item ? item->widget() : nullptr;
                    if (!w || w == mMovePlaceholder || !w->isVisible()) {
                        continue;
                    }

                    const QRect originalRect = mMoveOriginalGeometries.value(w, w->geometry());
                    const int otherCenterY = originalRect.center().y();

                    if (cursorY < otherCenterY && placeholderIdx > i) {
                        targetIdx = i;
                        break;
                    }
                    if (cursorY > otherCenterY && placeholderIdx < i) {
                        targetIdx = i + 1;
                    }
                }

                targetIdx = clampBeforeStretch(targetIdx);
                if (targetIdx != placeholderIdx) {
                    mPanesLayout->removeWidget(mMovePlaceholder);
                    mPanesLayout->insertWidget(targetIdx, mMovePlaceholder);
                    mPanesLayout->invalidate();
                    mPanesLayout->activate();
                }
            }
        }
    } else {
        hideDropZoneIndicator();
    }

    CosmicRightToolPanel* other = findOtherPanel();
    if (!other) {
        return;
    }

    if (other->acceptsTransferAt(globalPos)) {
        other->externalTransferHover(mMovingPane, mMovingPaneName, globalPos);
        mMoveOverOther = true;
    } else {
        if (mMoveOverOther) {
            other->externalTransferLeave();
        }
        mMoveOverOther = false;
    }
}

void CosmicLeftToolPanel::onPaneMoveDragEnded()
{
    if (!mMovingPane) {
        return;
    }

    CosmicRightToolPanel* other = findOtherPanel();
    const QPoint gp = QCursor::pos();

    if (other && mMoveOverOther && other->acceptsTransferAt(gp)) {
        // Commit transfer.
        if (mIsResizing) {
            mIsResizing = false;
        }
        if (mIsVResizing && mVResizePane == mMovingPane) {
            mIsVResizing = false;
            mVResizePane = nullptr;
        }
        other->externalTransferDrop(mMovingPane, mMovingPaneName, mMovingPaneWasEnabled);

        if (mPill) {
            mPill->removeToggle(mMovingPaneName);
        }
        mPanes.remove(mMovingPaneName);
        mPaneIndexOrder.removeAll(mMovingPaneName);

        clearMoveState(false);

        updatePaneIndexOrder();
        updatePillToggleOrder();
        syncPaneVisibility();
        updateSliderVisibility();
        saveSurvivalSettings();
        ensureStretchAtBottom();
        // Source panel may need to auto-hide now that it lost a pane and cursor has left
        if (mAutoHideEnabled) {
            updateAutoHideVisibility();
        }
        return;
    }
    if (acceptsTransferAt(gp) && mMovePlaceholder && mPanesLayout && mPanesContainer) {
        const int insertIdx = clampBeforeStretch(layoutIndexOf(mMovePlaceholder));
        mPanesLayout->removeWidget(mMovePlaceholder);
        delete mMovePlaceholder;
        mMovePlaceholder = nullptr;

        setPaneFloating(mMovingPane, false);
        mMovingPane->setSide(CosmicToolsPane::Side::Left);
        mPanesLayout->insertWidget(insertIdx, mMovingPane);
        mMovingPane->show();
        mMovingPane->raise();

        clearMoveState(false);

        updatePaneIndexOrder();
        updatePillToggleOrder();
        syncPaneVisibility();
        updateSliderVisibility();
        saveSurvivalSettings();
        ensureStretchAtBottom();
        return;
    }

    // Restore.
    clearMoveState(true);
    updatePaneIndexOrder();
    updatePillToggleOrder();
    syncPaneVisibility();
    updateSliderVisibility();
    saveSurvivalSettings();
    ensureStretchAtBottom();
}

void CosmicLeftToolPanel::bindPaneToThisPanel(CosmicToolsPane* pane, const QString& paneName)
{
    if (!pane) {
        return;
    }

    // Disconnect any existing connections from this pane to this panel to avoid duplicates
    unbindPaneFromThisPanel(pane);

    // Expanded state with height management for ALL panes.
    // This must work even in unlocked (auto-hide) mode and after panes are transferred
    // between left/right stacks.
    connect(pane, &CosmicToolsPane::expandedChanged, this, [this, paneName](bool) {
        CosmicToolsPane* targetPane = mPanes.value(paneName);
        if (!targetPane) {
            return;
        }

        if (targetPane->isExpanded()) {
            const int candidate = mLastExpandedHeights.value(
                targetPane,
                targetPane->lastExpandedHeight() > 0 ? targetPane->lastExpandedHeight() : targetPane->height());
            const int restoreH = qBound(140, candidate, 900);
            mLastExpandedHeights.insert(targetPane, restoreH);
            targetPane->setLastExpandedHeight(restoreH);
            targetPane->setFixedHeight(restoreH);
        } else {
            // IMPORTANT: once collapsed, targetPane->height() becomes header-only.
            // The true expanded height is stored on the pane itself (captured before collapse).
            const int candidate = targetPane->lastExpandedHeight() > 0
                                      ? targetPane->lastExpandedHeight()
                                      : mLastExpandedHeights.value(targetPane, 0);
            if (candidate > 0) {
                const int stored = qBound(140, candidate, 900);
                mLastExpandedHeights.insert(targetPane, stored);
                targetPane->setLastExpandedHeight(stored);
            }
            // Collapse visually to header-only height.
            targetPane->setFixedHeight(targetPane->sizeHint().height());
        }

        // Force the scroll area container to re-evaluate its total height so the
        // scrollbar shrinks back when panes are collapsed or removed.
        if (mPanesContainer) {
            mPanesContainer->updateGeometry();
            mPanesContainer->adjustSize();
        }

        updatePaneResizeSplitterGeometry();
        saveSurvivalSettings();
    }, Qt::UniqueConnection);

    // Reorder drag signals
    connect(pane, &CosmicToolsPane::dragStarted, this, [this, pane](const QPoint& gp) {
        onPaneDragStarted(pane, gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::dragMoved, this, [this](const QPoint& gp) {
        onPaneDragMoved(gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::dragEnded, this, [this]() {
        onPaneDragEnded();
    }, Qt::UniqueConnection);

    // Move transfer signals
    connect(pane, &CosmicToolsPane::moveDragStarted, this, [this, pane](const QPoint& gp) {
        onPaneMoveDragStarted(pane, gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::moveDragMoved, this, [this](const QPoint& gp) {
        onPaneMoveDragMoved(gp);
    }, Qt::UniqueConnection);
    connect(pane, &CosmicToolsPane::moveDragEnded, this, [this]() {
        onPaneMoveDragEnded();
    }, Qt::UniqueConnection);
}

void CosmicLeftToolPanel::unbindPaneFromThisPanel(CosmicToolsPane* pane)
{
    if (!pane) {
        return;
    }
    // Disconnect only connections from this pane to this panel
    QObject::disconnect(pane, nullptr, this, nullptr);
}

void CosmicLeftToolPanel::setPaneFloating(CosmicToolsPane* pane, bool floating, const QPoint& globalPos)
{
    if (!pane) {
        return;
    }

    if (floating) {
        // Store previous flags before making floating
        mMovePrevFlags = pane->windowFlags();
        
        // Hide before transition to prevent flicker
        pane->hide();
        
        // Detach from parent and make floating
        pane->setParent(nullptr);
        pane->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        pane->setAttribute(Qt::WA_TranslucentBackground, true);

        // When the pane becomes a top-level window it leaves the QMainWindow#MainWindow2
        // subtree, so the scoped QSS font rules no longer apply. Apply the font size
        // directly so text doesn't revert to the system default (which can appear
        // larger or bolder than the 9pt used inside the panel).
        QFont f = pane->font();
        f.setPointSize(9);
        f.setWeight(QFont::Normal);
        pane->setFont(f);

        // Also apply a stylesheet scoped to the pane so all child widgets (including
        // those with explicit fonts set by setupUi) inherit the correct 9pt size and
        // dark theme colors. Without this, widgets revert to system defaults when the
        // pane is no longer a descendant of QMainWindow#MainWindow2.
        pane->setStyleSheet(QStringLiteral(
            "* { font-size: 9pt; font-weight: 400; }"
            "QWidget { background: transparent; }"
            "QLabel { color: #b3b3b3; background: transparent; }"
            "QCheckBox { color: #b3b3b3; background: transparent; }"
            "QCheckBox::indicator { background: transparent; border: 1px solid #3a3a3a; width: 13px; height: 13px; }"
            "QCheckBox::indicator:unchecked { image: url(:/Interaction/CheckboxEmpty.svg); }"
            "QCheckBox::indicator:checked { image: url(:/Interaction/CheckboxChecked.svg); }"
            "QPushButton, QToolButton { background: #404040; color: #b3b3b3; border: 1px solid #3a3a3a; padding: 4px 8px; }"
            "QPushButton:hover, QToolButton:hover { background: #3a3a3a; }"
            "QLineEdit, QTextEdit, QPlainTextEdit { background: #2f2f2f; color: #b3b3b3; border: 1px solid #3a3a3a; }"
            "QComboBox { background: #2f2f2f; border: 1px solid #3a3a3a; color: #b3b3b3; }"
            "QSpinBox, QDoubleSpinBox { background: #2f2f2f; color: #b3b3b3; border: 1px solid #3a3a3a; }"
            "QAbstractItemView { background: #2f2f2f; color: #b3b3b3; }"
            "QScrollBar:vertical { background: #1f1f1f; border: none; width: 12px; margin: 0px; }"
            "QScrollBar::handle:vertical { background: #505050; border-radius: 6px; min-height: 26px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        ));

        // Don't steal focus or activate when shown — prevents input widgets inside
        // the pane (e.g. HexLineEdit in the colorwheel) from showing a blinking cursor
        // just because the pane was floated.
        pane->setAttribute(Qt::WA_ShowWithoutActivating, true);
        
        // Position and show
        if (!globalPos.isNull()) {
            const QPoint local = globalPos - mMoveGrabOffset;
            pane->move(local);
        }
        pane->show();
        pane->raise();
    } else {
        // Hide before re-embedding
        pane->hide();
        
        // Re-embed in container
        pane->setParent(mPanesContainer);
        pane->setWindowFlags(Qt::Widget);
        pane->setAttribute(Qt::WA_TranslucentBackground, false);
        pane->setFixedWidth(mPaneWidth);

        // Clear the explicit font so the pane inherits from the parent chain again,
        // which will pick up the QMainWindow#MainWindow2 QSS font rules.
        pane->setFont(QFont());

        // Remove the floating stylesheet — the QMainWindow#MainWindow2 QSS takes over again.
        pane->setStyleSheet(QString());

        // Restore normal activation behavior when embedded.
        pane->setAttribute(Qt::WA_ShowWithoutActivating, false);
        
        // Show and ensure proper stacking
        pane->show();
        pane->raise();
    }
}
