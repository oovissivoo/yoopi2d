/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2024-2099 Oliver S. Larsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "toolboxwidget.h"
#include "ui_toolboxwidget.h"
#include "pencolorpalette.h"
#include "colormanager.h"
#include <QEvent>

/* ===== UI SIZE ADJUSTMENT LANDMARKS ===== */
/*
 * TOOLS PANE DRAWING TOOL ICON SIZE CONTROLS:
 * - Line 157: Tool icon size multiplier (0.7774) - sets icons to 77.74% of original size
 *   This affects ALL drawing tools in the Tools pane (pen, pencil, eraser, bucket, etc.)
 *   NOT the cosmic pane header icons or other UI elements.
 * 
 * TO ADJUST TOOLS PANE ICON SIZES:
 * - Modify the multiplier on line 157:
 *   - 1.0 = 100% (original size)
 *   - 0.5 = 50% (half size)
 *   - 0.7774 = 77.74% (current setting)
 * 
 * SIZE ADJUSTMENT HISTORY:
 * - Started at 100% (original)
 * - Reduced to 50% (half size)
 * - Increased to 65% (+15% from 50%)
 * - Increased to 84.5% (+30% from 65%)
 * - Reduced to 77.74% (-8% from 84.5%) - CURRENT SETTING
 * 
 * RELATED SIZE ADJUSTMENTS:
 * - dark_theme.qss: Global font size controls (9pt)
 * - cosmictoolspane.cpp: Cosmic pane header layout and icon button sizes
 */

#include <QScrollBar>
#include <QResizeEvent>
#include <QDebug>
#include <QButtonGroup>
#include <QIcon>
#include <QSettings>
#include <QMainWindow>
#include <QAbstractButton>
#include <QToolButton>

#include "../CUI/cosmiccheckboxtoggle.h"

#include "layermanager.h"
#include "toolmanager.h"
#include "editor.h"
#include "pencilsettings.h"
#include "basedockwidget.h"
#include "preferencesdef.h"
#include "pencildef.h"
#include "preferencemanager.h"

// ----------------------------------------------------------------------------------
QString GetToolTips(QString strCommandName)
{
    strCommandName = QString("shortcuts/") + strCommandName;
    QKeySequence keySequence(pencilSettings().value(strCommandName).toString());
    // Use NativeText so the tooltip matches what the Shortcuts UI displays
    if (keySequence.isEmpty()) return QString();
    return QString("<b>%1</b>").arg(keySequence.toString(QKeySequence::NativeText)); // don't tr() this string.
}

ToolBoxWidget::ToolBoxWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::ToolBoxWidget)
{
    ui->setupUi(this);
    // Tag for stylesheet targeting (tool dock buttons)
    setObjectName(QStringLiteral("ToolBoxWidget"));
}

ToolBoxWidget::~ToolBoxWidget()
{
    delete ui;
}

void ToolBoxWidget::initUI()
{

    // Toolbox styling is now handled by updateToolboxStyles() which uses preference colors
    // This ensures consistent styling across all platforms and respects user color preferences

    retranslateTooltips();

    // Create CosmicCheckboxToggle instances for each visible tool button, using
    // the existing QToolButton icons and texts as the source.
    auto createToolToggle = [this](QToolButton* source, const char* objectName) -> CosmicCheckboxToggle*
    {
        if (!source) {
            return nullptr;
        }

        auto* toggle = new CosmicCheckboxToggle(this);
        toggle->setObjectName(QLatin1String(objectName));
        toggle->setIcon(source->icon());
        QSize originalSize = source->iconSize();
        toggle->setIconSize(QSize(qRound(originalSize.width() * 0.7774), qRound(originalSize.height() * 0.7774)));
        toggle->setToolTip(source->toolTip());
        toggle->setWhatsThis(source->whatsThis());
        toggle->setCheckable(true);
        toggle->setFocusPolicy(Qt::NoFocus);
        // Custom mode, borderless, icon-only is the default; enforce explicitly.
        toggle->setMode(CosmicCheckboxToggle::Custom);
        toggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
        toggle->setBordered(false);
        toggle->setShowText(false);
        toggle->setShowIcon(true);
        return toggle;
    };

    mEraserToggle       = createToolToggle(ui->eraserButton,      "eraserToggle");
    mLassoSelectToggle  = createToolToggle(ui->lassoSelectButton, "lassoSelectToggle");
    mPenToggle          = nullptr;
    mPencilToggle       = createToolToggle(ui->pencilButton,      "pencilToggle");
    mHandToggle         = createToolToggle(ui->handButton,        "handToggle");
    mPolylineToggle     = createToolToggle(ui->polylineButton,    "polylineToggle");
    mCurveToggle        = createToolToggle(ui->curveButton,       "curveToggle");
    mBsplineToggle      = createToolToggle(ui->bsplineButton,     "bsplineToggle");
    mBucketToggle       = createToolToggle(ui->bucketButton,      "bucketToggle");
    mRectGapFillToggle  = createToolToggle(ui->rectGapFillButton, "rectGapFillToggle");
    mLassoGapFillToggle = createToolToggle(ui->lassoGapFillButton,"lassoGapFillToggle");
    mEyedropperToggle   = createToolToggle(ui->eyedropperButton,  "eyedropperToggle");
    mDplateToggle       = createToolToggle(ui->dplateButton,      "dplateToggle");

    // Hide the legacy tool buttons so only the cosmic toggles are visible.
    ui->eraserButton->hide();
    if (ui->lassoSelectButton) ui->lassoSelectButton->hide();
    ui->selectButton->hide();
    ui->moveButton->hide();
    ui->penButton->hide();
    ui->pencilButton->hide();
    ui->handButton->hide();
    ui->polylineButton->hide();
    ui->curveButton->hide();
    ui->bsplineButton->hide();
    ui->bucketButton->hide();
    ui->rectGapFillButton->hide();
    ui->lassoGapFillButton->hide();
    ui->eyedropperButton->hide();
    if (ui->dplateButton) ui->dplateButton->hide();

    if (mEraserToggle)      connect(mEraserToggle,      &QAbstractButton::clicked, this, &ToolBoxWidget::eraserOn);
    if (mLassoSelectToggle) connect(mLassoSelectToggle, &QAbstractButton::clicked, this, &ToolBoxWidget::lassoSelectOn);
    if (mPencilToggle)      connect(mPencilToggle,      &QAbstractButton::clicked, this, &ToolBoxWidget::pencilOn);
    if (mHandToggle)        connect(mHandToggle,        &QAbstractButton::clicked, this, &ToolBoxWidget::handOn);
    if (mPolylineToggle)    connect(mPolylineToggle,    &QAbstractButton::clicked, this, &ToolBoxWidget::polylineOn);
    if (mCurveToggle)       connect(mCurveToggle,       &QAbstractButton::clicked, this, &ToolBoxWidget::curveOn);
    if (mBsplineToggle)     connect(mBsplineToggle,     &QAbstractButton::clicked, this, &ToolBoxWidget::bsplineOn);
    if (mBucketToggle)      connect(mBucketToggle,      &QAbstractButton::clicked, this, &ToolBoxWidget::bucketOn);
    if (mRectGapFillToggle) connect(mRectGapFillToggle, &QAbstractButton::clicked, this, [this]() { toolOn(RECT_GAP_FILL, mRectGapFillToggle); });
    if (mLassoGapFillToggle)connect(mLassoGapFillToggle,&QAbstractButton::clicked, this, [this]() { toolOn(LASSO_GAP_FILL, mLassoGapFillToggle); });
    if (mEyedropperToggle)  connect(mEyedropperToggle,  &QAbstractButton::clicked, this, &ToolBoxWidget::eyedropperOn);
    if (mDplateToggle)      connect(mDplateToggle,      &QAbstractButton::clicked, this, &ToolBoxWidget::dplateOn);

    mFlowlayout = new ToolBoxLayout(nullptr, 3,3,3);

    // Order synced with toolbar: LassoSelect, Select, Move, Pen, Pencil, Curve, Polyline, BSpline,
    // Bucket, RectGapFill, LassoGapFill, Eraser, Hand, Eyedropper, Dplate
    if (mLassoSelectToggle) mFlowlayout->addWidget(mLassoSelectToggle);
    // Hide legacy Select and Move buttons; Lasso Select is the primary
    // selection tool in the dock.
    ui->selectButton->hide();
    ui->moveButton->hide();

    // New visual order for Tools dock:
    // Lasso Select, Pen, Pencil, Eraser, Curve, BSpline, Polyline, Bucket,
    // Lasso Gap Fill, Rect Gap Fill, Dplate, Eyedropper, Hand
    if (mPencilToggle)       mFlowlayout->addWidget(mPencilToggle);
    if (mEraserToggle)       mFlowlayout->addWidget(mEraserToggle);
    if (mCurveToggle)        mFlowlayout->addWidget(mCurveToggle);
    if (mBsplineToggle)      mFlowlayout->addWidget(mBsplineToggle);
    if (mPolylineToggle)     mFlowlayout->addWidget(mPolylineToggle);
    if (mBucketToggle)       mFlowlayout->addWidget(mBucketToggle);
    if (mLassoGapFillToggle) mFlowlayout->addWidget(mLassoGapFillToggle);
    if (mRectGapFillToggle)  mFlowlayout->addWidget(mRectGapFillToggle);
    if (mDplateToggle)       mFlowlayout->addWidget(mDplateToggle);
    if (mEyedropperToggle)   mFlowlayout->addWidget(mEyedropperToggle);
    if (mHandToggle)         mFlowlayout->addWidget(mHandToggle);

    delete ui->scrollAreaWidgetContents_2->layout();
    ui->scrollAreaWidgetContents_2->setLayout(mFlowlayout);

    // Important to set the proper minimumSize;
    ui->scrollArea->setMinimumSize(QSize(1,1));
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setFrameShape(QFrame::NoFrame);
    setMinimumSize(mFlowlayout->minimumSize());

    QButtonGroup* buttonGroup = new QButtonGroup(this);
    if (mEraserToggle)      buttonGroup->addButton(mEraserToggle);
    if (mLassoSelectToggle) buttonGroup->addButton(mLassoSelectToggle);
    if (mPencilToggle)      buttonGroup->addButton(mPencilToggle);
    if (mHandToggle)        buttonGroup->addButton(mHandToggle);
    if (mPolylineToggle)    buttonGroup->addButton(mPolylineToggle);
    if (mCurveToggle)       buttonGroup->addButton(mCurveToggle);
    if (mBsplineToggle)     buttonGroup->addButton(mBsplineToggle);
    if (mBucketToggle)      buttonGroup->addButton(mBucketToggle);
    if (mEyedropperToggle)  buttonGroup->addButton(mEyedropperToggle);
    if (mRectGapFillToggle) buttonGroup->addButton(mRectGapFillToggle);
    if (mLassoGapFillToggle)buttonGroup->addButton(mLassoGapFillToggle);
    if (mDplateToggle)      buttonGroup->addButton(mDplateToggle);

    // Explicitly hide the buttons
    // (brush and smudge buttons removed)
    
    // Update toolbox styles with current preferences
    updateToolboxStyles();
    
    // Connect to preference changes for real-time updates
    if (mEditor && mEditor->preference()) {
        connect(mEditor->preference(), &PreferenceManager::optionChanged, this, &ToolBoxWidget::onColorPreferenceChanged);
    }
}

int ToolBoxWidget::getMinHeightForWidth(int width) const
{
    return mFlowlayout->heightForWidth(width);
}

QSize ToolBoxWidget::sizeHint() const
{
    return minimumSizeHint();
}

QSize ToolBoxWidget::minimumSizeHint() const
{
    int minWidth = mFlowlayout->minimumSize().width();
    return QSize(minWidth, getMinHeightForWidth(width()));
}

void ToolBoxWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    updateLayoutAlignment();
}

void ToolBoxWidget::updateLayoutAlignment()
{
    mFlowlayout->invalidate();
    if (mFlowlayout->rows() > 1) {
        mFlowlayout->setAlignment(Qt::AlignJustify);
    } else {
        mFlowlayout->setAlignment(Qt::AlignHCenter);
    }

    mFlowlayout->activate();

    // When the tools are arranged in a single horizontal row (top/bottom dock),
    // constrain only the inner contents to the row height so the dock itself
    // can shrink further without being blocked by outer min/max constraints.
    int desiredHeight = mFlowlayout->heightForWidth(width());
    if (mFlowlayout->rows() == 1) {
        ui->scrollAreaWidgetContents_2->setMinimumHeight(desiredHeight);
        ui->scrollAreaWidgetContents_2->setMaximumHeight(desiredHeight);
        ui->scrollArea->setMinimumHeight(desiredHeight);
        ui->scrollArea->setMaximumHeight(desiredHeight);
        // Let the outer widget/dock decide; avoid enforcing our own max height
        this->setMinimumHeight(1);
        this->setMaximumHeight(QWIDGETSIZE_MAX);

        // When the dock is locked (titlebar hidden), clamp the dock height
        // itself to eliminate any leftover strip.
        if (auto dock = qobject_cast<BaseDockWidget*>(parentWidget())) {
            if (dock->isLocked()) {
                dock->setMinimumHeight(desiredHeight);
                dock->setMaximumHeight(desiredHeight);
            } else {
                dock->setMinimumHeight(1);
                dock->setMaximumHeight(QWIDGETSIZE_MAX);
            }
        }
    } else {
        ui->scrollAreaWidgetContents_2->setMinimumHeight(1);
        ui->scrollAreaWidgetContents_2->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->scrollArea->setMinimumHeight(1);
        ui->scrollArea->setMaximumHeight(QWIDGETSIZE_MAX);
        this->setMinimumHeight(mFlowlayout->minimumSize().height());
        this->setMaximumHeight(QWIDGETSIZE_MAX);
        if (auto dock = qobject_cast<BaseDockWidget*>(parentWidget())) {
            dock->setMinimumHeight(1);
            dock->setMaximumHeight(QWIDGETSIZE_MAX);
        }
    }
}

void ToolBoxWidget::updateUI()
{
}

void ToolBoxWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateTooltips();
    }
}

void ToolBoxWidget::retranslateTooltips()
{
    // ── Tooltips on source QToolButtons ──────────────────────────────────────
    ui->selectButton->setToolTip(
        tr("Select Tool (%1): Drag to draw a rectangular selection. Drag a corner to resize. Drag inside to move contents. Drag the boundary to reposition the outline. Releases auto-switches to Move tool.")
        .arg(GetToolTips(CMD_TOOL_SELECT)));
    ui->moveButton->setToolTip(
        tr("Move Tool (%1): Drag inside selection to move. Drag a corner handle to scale. Drag outside to rotate. Hold Shift while rotating to snap to fixed increments.")
        .arg(GetToolTips(CMD_TOOL_MOVE)));
    ui->handButton->setToolTip(
        tr("Hand Tool (%1): Drag to pan. Hold Ctrl or right-drag to zoom. Hold Alt or Shift to rotate.")
        .arg(GetToolTips(CMD_TOOL_HAND)));
    ui->penButton->setToolTip(
        tr("Pen Tool (%1): Draw a pressure-sensitive stroke. Hold Ctrl+Shift to temporarily erase. Hold Alt to temporarily pick a color.")
        .arg(GetToolTips(CMD_TOOL_PEN)));
    ui->pencilButton->setToolTip(
        tr("Pencil Tool (%1): Draw with the current brush preset. Hold Alt to temporarily pick a color.")
        .arg(GetToolTips(CMD_TOOL_PENCIL)));
    ui->eraserButton->setToolTip(
        tr("Eraser Tool (%1): Paint to erase. Enable Lasso mode in tool options to draw a freeform erase region.")
        .arg(GetToolTips(CMD_TOOL_ERASER)));
    ui->polylineButton->setToolTip(
        tr("Polyline Tool (%1): Click to place points. Drag a point to reposition it. RMB or Backspace removes the last point. Double-click or Enter to commit. Ctrl+click the first point to close and commit. Alt+click to commit immediately. Escape to discard.")
        .arg(GetToolTips(CMD_TOOL_POLYLINE)));
    ui->curveButton->setToolTip(
        tr("Curve Tool (%1): Click and drag to set start and end points. Release enters curve-adjust mode — move cursor to bend the curve, then click or press Enter to commit. Escape to discard.")
        .arg(GetToolTips(CMD_TOOL_CURVE)));
    ui->bsplineButton->setToolTip(
        tr("B-Spline Tool: Click to place control points. Drag a point to reposition it. RMB or Backspace removes the last point. Enter to commit. Ctrl+click the first point (3+ points) to close and commit. Alt+click to commit immediately. Escape to discard."));
    if (ui->lassoSelectButton)
        ui->lassoSelectButton->setToolTip(
            tr("Lasso Select Tool (%1): Draw a freeform selection. Drag inside to move contents. Drag a corner handle to scale. Drag outside or the rotation handle to rotate. Hold Shift while rotating to snap. Hold Ctrl to start a new selection. Backspace/Delete removes floating content.")
            .arg(GetToolTips(CMD_TOOL_LASSO_SELECT)));
    if (ui->rectGapFillButton)
        ui->rectGapFillButton->setToolTip(
            tr("Rect Gap Fill Tool (%1): Drag to define a rectangle. Transparent color islands fully enclosed by the rectangle are filled. Hold Alt to pick the ignore color.")
            .arg(GetToolTips(CMD_TOOL_RECT_GAP_FILL)));
    if (ui->lassoGapFillButton)
        ui->lassoGapFillButton->setToolTip(
            tr("Lasso Gap Fill Tool (%1): Draw a freeform region. Transparent color islands fully enclosed by the lasso are filled on release. Hold Alt to pick the ignore color.")
            .arg(GetToolTips(CMD_TOOL_LASSO_GAP_FILL)));
    ui->bucketButton->setToolTip(
        tr("Paint Bucket Tool (%1): Click to flood-fill with the current color. Hold Ctrl and drag to draw a gap-closing stroke behind existing pixels. Hold Alt to pick the ignore color.")
        .arg(GetToolTips(CMD_TOOL_BUCKET)));
    ui->eyedropperButton->setToolTip(
        tr("Eyedropper Tool (%1): Click to sample a color from the canvas and set it as the active color. Reference mode (from view or current layer) is set in tool options.")
        .arg(GetToolTips(CMD_TOOL_EYEDROPPER)));
    if (ui->dplateButton)
        ui->dplateButton->setToolTip(
            tr("Dplate Tool (%1): Click and drag to stamp a D-plate registration shape at the release point. The connecting line is drawn behind the shape.")
            .arg(GetToolTips(CMD_TOOL_DPLATE)));

    // ── WhatsThis (short names) ───────────────────────────────────────────────
    ui->selectButton->setWhatsThis(tr("Select Tool (%1)").arg(GetToolTips(CMD_TOOL_SELECT)));
    if (ui->lassoSelectButton)
        ui->lassoSelectButton->setWhatsThis(tr("Lasso Select Tool (%1)").arg(GetToolTips(CMD_TOOL_LASSO_SELECT)));
    ui->moveButton->setWhatsThis(tr("Move Tool (%1)").arg(GetToolTips(CMD_TOOL_MOVE)));
    ui->handButton->setWhatsThis(tr("Hand Tool (%1)").arg(GetToolTips(CMD_TOOL_HAND)));
    ui->penButton->setWhatsThis(tr("Pen Tool (%1)").arg(GetToolTips(CMD_TOOL_PEN)));
    ui->pencilButton->setWhatsThis(tr("Pencil Tool (%1)").arg(GetToolTips(CMD_TOOL_PENCIL)));
    ui->eraserButton->setWhatsThis(tr("Eraser Tool (%1)").arg(GetToolTips(CMD_TOOL_ERASER)));
    ui->polylineButton->setWhatsThis(tr("Polyline Tool (%1)").arg(GetToolTips(CMD_TOOL_POLYLINE)));
    ui->curveButton->setWhatsThis(tr("Curve Tool (%1)").arg(GetToolTips(CMD_TOOL_CURVE)));
    ui->bsplineButton->setWhatsThis(tr("B-Spline Tool"));
    ui->bucketButton->setWhatsThis(tr("Paint Bucket Tool (%1)").arg(GetToolTips(CMD_TOOL_BUCKET)));
    ui->eyedropperButton->setWhatsThis(tr("Eyedropper Tool (%1)").arg(GetToolTips(CMD_TOOL_EYEDROPPER)));
    if (ui->dplateButton)
        ui->dplateButton->setWhatsThis(tr("Dplate Tool (%1)").arg(GetToolTips(CMD_TOOL_DPLATE)));

    // ── Sync tooltips/whatsThis to CosmicCheckboxToggle instances ────────────
    // Toggles copy from source buttons — re-sync after source buttons are updated.
    auto syncToggle = [](CosmicCheckboxToggle* toggle, QToolButton* source) {
        if (toggle && source) {
            toggle->setToolTip(source->toolTip());
            toggle->setWhatsThis(source->whatsThis());
        }
    };
    syncToggle(mEraserToggle,       ui->eraserButton);
    syncToggle(mLassoSelectToggle,  ui->lassoSelectButton);
    syncToggle(mPencilToggle,       ui->pencilButton);
    syncToggle(mHandToggle,         ui->handButton);
    syncToggle(mPolylineToggle,     ui->polylineButton);
    syncToggle(mCurveToggle,        ui->curveButton);
    syncToggle(mBsplineToggle,      ui->bsplineButton);
    syncToggle(mBucketToggle,       ui->bucketButton);
    syncToggle(mRectGapFillToggle,  ui->rectGapFillButton);
    syncToggle(mLassoGapFillToggle, ui->lassoGapFillButton);
    syncToggle(mEyedropperToggle,   ui->eyedropperButton);
    syncToggle(mDplateToggle,       ui->dplateButton);
}

void ToolBoxWidget::setActiveTool(ToolType toolType)
{
    switch (toolType) {
    case ToolType::BSPLINE:
        bsplineOn();
        break;
    case ToolType::PEN:
        penOn();
        break;
    case ToolType::PENCIL:
        pencilOn();
        break;
    case ToolType::SELECT:
        selectOn();
        break;
    case ToolType::HAND:
        handOn();
        break;
    case ToolType::MOVE:
    case ToolType::CAMERA:
        moveOn();
        break;
    case ToolType::ERASER:
        eraserOn();
        break;
    case ToolType::POLYLINE:
        polylineOn();
        break;
    case ToolType::RECT_GAP_FILL:
        toolOn(RECT_GAP_FILL,
               mRectGapFillToggle ? static_cast<QAbstractButton*>(mRectGapFillToggle)
                                  : static_cast<QAbstractButton*>(ui->rectGapFillButton));
        break;
    case ToolType::LASSO_GAP_FILL:
        toolOn(LASSO_GAP_FILL,
               mLassoGapFillToggle ? static_cast<QAbstractButton*>(mLassoGapFillToggle)
                                   : static_cast<QAbstractButton*>(ui->lassoGapFillButton));
        break;
    case ToolType::CURVE:
        curveOn();
        break;
    case ToolType::BUCKET:
        bucketOn();
        break;
    case ToolType::EYEDROPPER:
        eyedropperOn();
        break;
    case ToolType::DPLATE:
        dplateOn();
        break;
    case ToolType::LASSO_SELECT:
        lassoSelectOn();
        break;
    default:
        break;
    }
}


void ToolBoxWidget::eraserOn()
{
    toolOn(ERASER,
           mEraserToggle ? static_cast<QAbstractButton*>(mEraserToggle)
                         : static_cast<QAbstractButton*>(ui->eraserButton));
}

void ToolBoxWidget::selectOn()
{
    toolOn(SELECT,
           static_cast<QAbstractButton*>(ui->selectButton));
}

void ToolBoxWidget::moveOn()
{
    QAbstractButton* moveBtn = static_cast<QAbstractButton*>(ui->moveButton);
    if (mEditor->layers()->currentLayer()->type() == Layer::CAMERA) {
        toolOn(CAMERA, moveBtn);
    } else {
        toolOn(MOVE, moveBtn);
    }
}

void ToolBoxWidget::penOn()
{
    toolOn(PEN,
           mPenToggle ? static_cast<QAbstractButton*>(mPenToggle)
                      : static_cast<QAbstractButton*>(ui->penButton));
}

void ToolBoxWidget::pencilOn()
{
    toolOn(PENCIL,
           mPencilToggle ? static_cast<QAbstractButton*>(mPencilToggle)
                         : static_cast<QAbstractButton*>(ui->pencilButton));
}

void ToolBoxWidget::handOn()
{
    toolOn(HAND,
           mHandToggle ? static_cast<QAbstractButton*>(mHandToggle)
                       : static_cast<QAbstractButton*>(ui->handButton));
}

void ToolBoxWidget::bsplineOn()
{
    toolOn(BSPLINE,
           mBsplineToggle ? static_cast<QAbstractButton*>(mBsplineToggle)
                          : static_cast<QAbstractButton*>(ui->bsplineButton));
}

void ToolBoxWidget::polylineOn()
{
    toolOn(POLYLINE,
           mPolylineToggle ? static_cast<QAbstractButton*>(mPolylineToggle)
                           : static_cast<QAbstractButton*>(ui->polylineButton));
}

void ToolBoxWidget::curveOn()
{
    toolOn(CURVE,
           mCurveToggle ? static_cast<QAbstractButton*>(mCurveToggle)
                        : static_cast<QAbstractButton*>(ui->curveButton));
}

void ToolBoxWidget::bucketOn()
{
    toolOn(BUCKET,
           mBucketToggle ? static_cast<QAbstractButton*>(mBucketToggle)
                         : static_cast<QAbstractButton*>(ui->bucketButton));
}

void ToolBoxWidget::eyedropperOn()
{
    toolOn(EYEDROPPER,
           mEyedropperToggle ? static_cast<QAbstractButton*>(mEyedropperToggle)
                             : static_cast<QAbstractButton*>(ui->eyedropperButton));
}

void ToolBoxWidget::dplateOn()
{
    if (!ui->dplateButton && !mDplateToggle) {
        return;
    }
    toolOn(DPLATE,
           mDplateToggle ? static_cast<QAbstractButton*>(mDplateToggle)
                         : static_cast<QAbstractButton*>(ui->dplateButton));
}

void ToolBoxWidget::lassoSelectOn()
{
    if (!ui->lassoSelectButton && !mLassoSelectToggle) {
        return;
    }
    toolOn(LASSO_SELECT,
           mLassoSelectToggle ? static_cast<QAbstractButton*>(mLassoSelectToggle)
                              : static_cast<QAbstractButton*>(ui->lassoSelectButton));
}

void ToolBoxWidget::deselectAllTools()
{
    // Clear state on both the legacy buttons (for safety) and the visible
    // CosmicCheckboxToggle controls.
    ui->eraserButton->setChecked(false);
    if (ui->lassoSelectButton) ui->lassoSelectButton->setChecked(false);
    ui->selectButton->setChecked(false);
    ui->moveButton->setChecked(false);
    ui->handButton->setChecked(false);
    ui->penButton->setChecked(false);
    ui->pencilButton->setChecked(false);
    ui->polylineButton->setChecked(false);
    ui->curveButton->setChecked(false);
    ui->bsplineButton->setChecked(false);
    ui->bucketButton->setChecked(false);
    ui->eyedropperButton->setChecked(false);
    if (ui->dplateButton) ui->dplateButton->setChecked(false);

    if (mEraserToggle)      mEraserToggle->setChecked(false);
    if (mLassoSelectToggle) mLassoSelectToggle->setChecked(false);
    if (mPenToggle)         mPenToggle->setChecked(false);
    if (mPencilToggle)      mPencilToggle->setChecked(false);
    if (mHandToggle)        mHandToggle->setChecked(false);
    if (mPolylineToggle)    mPolylineToggle->setChecked(false);
    if (mCurveToggle)       mCurveToggle->setChecked(false);
    if (mBsplineToggle)     mBsplineToggle->setChecked(false);
    if (mBucketToggle)      mBucketToggle->setChecked(false);
    if (mRectGapFillToggle) mRectGapFillToggle->setChecked(false);
    if (mLassoGapFillToggle)mLassoGapFillToggle->setChecked(false);
    if (mEyedropperToggle)  mEyedropperToggle->setChecked(false);
    if (mDplateToggle)      mDplateToggle->setChecked(false);
}

void ToolBoxWidget::toolOn(ToolType toolType, QAbstractButton* toolButton)
{
    if (!toolButton) {
        return;
    }
    if (mEditor->tools()->currentTool()->type() == toolType) {
        // Prevent un-checking the current tool and do nothing
        toolButton->setChecked(true);
        return;
    }
    if (!mEditor->tools()->leavingThisTool())
    {
        toolButton->setChecked(false);
        return;
    }
    mEditor->tools()->setCurrentTool(toolType);
}

void ToolBoxWidget::updateToolboxStyles()
{
    // Get the toolbox selection color from preferences
    QSettings settings(PENCIL2D, PENCIL2D);
    const int toolboxRgba = settings.value(SETTING_TOOLBOX_SELECTION_COLOR, 0xFFFFAA40).toInt();
    const QColor toolboxColor = QColor::fromRgba(toolboxRgba);

    // Create dynamic stylesheet for toolbox buttons
    const QString colorHex = toolboxColor.name(QColor::HexRgb);
    auto rgbaInt = [&](int a0to255) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(toolboxColor.red())
            .arg(toolboxColor.green())
            .arg(toolboxColor.blue())
            .arg(qBound(0, a0to255, 255));
    };

    const QString colorHexAlpha = rgbaInt(31);          // ~0.12
    const QString colorHexAlphaHover = rgbaInt(71);        // ~0.28
    const QString colorHexAlphaChecked = rgbaInt(115);     // ~0.45
    const QString colorHexAlphaCheckedHover = rgbaInt(56); // ~0.22
    const QString colorHexBorder = rgbaInt(89);            // ~0.35
    const QString colorHexBorderChecked = rgbaInt(217);    // ~0.85
    const QString colorHexBorderCheckedHover = rgbaInt(255);

    // Create a more robust stylesheet that works across all platforms
    const QString style = QString(
        "QToolButton {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  padding: 6px;"
        "  border-radius: 14px;"
        "}"
        "QToolButton:hover {"
        "  background: %1;"
        "  border-color: %2;"
        "}"
        "QToolButton:checked {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.6,"
        "                              fx:0.5, fy:0.5,"
        "                              stop:0 %3,"
        "                              stop:0.55 %4,"
        "                              stop:1 rgba(0,0,0,0));"
        "  border-color: %5;"
        "}"
        "QToolButton:checked:hover {"
        "  background: %6;"
        "  border-color: %7;"
        "}"
    ).arg(colorHexAlpha, colorHexBorder, colorHexAlphaChecked, colorHexAlphaCheckedHover,
          colorHexBorderChecked, colorHexAlphaHover, colorHexBorderCheckedHover);

    // Apply the stylesheet directly to this widget to ensure it takes precedence
    this->setStyleSheet(style);

    // Also propagate the toolbox accent color to the CosmicCheckboxToggle
    // controls used for the tool buttons so they visually match the
    // configured selection color.
    auto applyAccent = [&toolboxColor](CosmicCheckboxToggle* toggle) {
        if (!toggle) {
            return;
        }
        toggle->setAccentColor(toolboxColor);
    };

    applyAccent(mEraserToggle);
    applyAccent(mLassoSelectToggle);
    applyAccent(mPenToggle);
    applyAccent(mPencilToggle);
    applyAccent(mHandToggle);
    applyAccent(mPolylineToggle);
    applyAccent(mCurveToggle);
    applyAccent(mBsplineToggle);
    applyAccent(mBucketToggle);
    applyAccent(mRectGapFillToggle);
    applyAccent(mLassoGapFillToggle);
    applyAccent(mEyedropperToggle);
    applyAccent(mDplateToggle);
}

void ToolBoxWidget::onColorPreferenceChanged(SETTING setting)
{
    if (setting == SETTING::TOOLBOX_SELECTION_COLOR) {
        updateToolboxStyles();
    }
}
