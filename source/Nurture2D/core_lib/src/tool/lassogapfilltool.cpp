#include "lassogapfilltool.h"

#include <QCursor>
#include <QPixmap>
#include <QPainter>
#include <QImage>
#include <QLineF>
#include "pointerevent.h"
#include "editor.h"
#include "layermanager.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "structure/celllayerstack.h"
#include "scribblearea.h"
#include "graphics/bitmap/bitmapimage.h"
#include "managers/colormanager.h"
#include "util/pencilsettings.h"
#include <QSettings>
#include "managers/settingsmanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "managers/undoredomanager.h"
#include "managers/toolmanager.h"
#include "eyedroppertool.h"

LassoGapFillTool::LassoGapFillTool(QObject* parent) : BaseTool(parent)
{
}

void LassoGapFillTool::loadSettings()
{
    mPropertyEnabled[ANTI_ALIASING] = true;
    SettingsManager* settings = SettingsManager::instance();
    m_useActiveColor = settings->getValue("GapFill/UseActiveColor", false).toBool();

    // Clear preview selection after undo/redo when this tool is active
    static bool connected = false;
    if (!connected) {
        connected = QObject::connect(mEditor->undoRedo(), &UndoRedoManager::didUpdateUndoStack, this, [this]{
            if (mEditor->tools()->currentTool()->type() == type()) {
                mEditor->select()->resetSelectionProperties();
                mScribbleArea->updateFrame();
            }
        });
    }
}

void LassoGapFillTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();
    settings->deferredSetValue("GapFill/UseActiveColor", m_useActiveColor);
}

QCursor LassoGapFillTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":/icons/themes/playful/general/cursor-crosshair.svg"), 8, 8);
    }
    return Qt::CrossCursor;
}

bool LassoGapFillTool::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt) {
        mEditor->tools()->setEyedropperSampleOnly(true);
        if (mEditor->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_Alt)) {
            if (auto* dropper = qobject_cast<EyedropperTool*>(mEditor->tools()->getTool(EYEDROPPER))) {
                auto colorConn = std::make_shared<QMetaObject::Connection>();
                *colorConn = QObject::connect(dropper, &EyedropperTool::sampledColor, this, [this, colorConn](const QColor& picked){
                    if (!picked.isValid()) { QObject::disconnect(*colorConn); return; }
                    mEditor->setIgnoreFillColor(qPremultiply(picked.rgba()));
                    QObject::disconnect(*colorConn);
                }, Qt::UniqueConnection);
            }
            auto endConn = std::make_shared<QMetaObject::Connection>();
            *endConn = QObject::connect(mEditor->tools(), &ToolManager::toolChanged, this, [this, endConn](ToolType){
                mEditor->tools()->setEyedropperSampleOnly(false);
                QObject::disconnect(*endConn);
            }, Qt::UniqueConnection);
            return true;
        }
    }
    return BaseTool::keyPressEvent(event);
}

void LassoGapFillTool::pointerPressEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer) return;

    // Clear previous overlay and start a new path selection
    mEditor->select()->resetSelectionProperties();

    m_isBuilding = true;
    QPointF p = event->canvasPos();
    m_path = QPainterPath(p);
    // Use SelectionManager lasso lifecycle so state==CREATING and ants draw the actual path
    mEditor->select()->beginLassoAt(p);

    if (!m_useActiveColor && layer->type() == Layer::BITMAP)
    {
        BitmapImage* bmi = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
        if (bmi)
        {
            m_sampledColor = QColor::fromRgba(bmi->pixel(p.toPoint()));
        }
    }
    mScribbleArea->updateFrame();
    event->accept();
}

void LassoGapFillTool::pointerMoveEvent(PointerEvent* event)
{
    if (!m_isBuilding) return;
    QPointF p = event->canvasPos();
    if (QLineF(m_path.currentPosition(), p).length() > 0.5)
    {
        m_path.lineTo(p);
        // Keep selection manager path in sync so marching ants render the lasso path
        mEditor->select()->appendLassoPoint(p);
        mScribbleArea->updateFrame();
    }
    event->accept();
}

void LassoGapFillTool::pointerReleaseEvent(PointerEvent* event)
{
    if (!m_isBuilding || event->button() != Qt::LeftButton) return;
    m_isBuilding = false;

    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        mEditor->select()->resetSelectionProperties();
        mScribbleArea->updateFrame();
        return;
    }
    BitmapImage* bmi = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
    if (!bmi || !bmi->isLoaded()) {
        mEditor->select()->resetSelectionProperties();
        mScribbleArea->updateFrame();
        event->accept();
        return;
    }

    // Finalize lasso so ScribbleArea briefly switches to bounding box mode if needed
    mEditor->select()->closeLasso();
    // Use the exact lasso path (canvas space) for fill
    QPainterPath closed = mEditor->select()->selectionPathWithOffsets();

    QColor fillColor = m_useActiveColor ? mEditor->color()->frontColor() : m_sampledColor;

    const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    fillTransparentInPath(closed, fillColor);
    if (mEditor && mEditor->undoRedo()) mEditor->undoRedo()->record(saveState, typeName());
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());

    // Clear overlay preview after commit/cancel
    mEditor->select()->resetSelectionProperties();
    mScribbleArea->updateFrame();
    event->accept();
}

void LassoGapFillTool::drawPreview()
{
    // Unused now — SelectionManager overlay handles drawing
}

void LassoGapFillTool::fillTransparentInPath(const QPainterPath& path, const QColor& color)
{
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
    const int frameNumber = mEditor->currentFrame();

    // Task 8.3 (Req 8.6): Gap boundaries are read from the Cell_Composite (all visible
    // inner layers merged) so the fill respects lines drawn on any inner layer.
    // Fill pixels are written to the Active_Inner_Layer only.
    BitmapImage* bmi = bitmapLayer->getLastBitmapImageAtFrame(frameNumber, 0); // composite — for gap detection
    if (!bmi) return;

    // Resolve the active inner layer as the write target
    BitmapImage* writeTarget = nullptr;
    CellLayerStack* stack = nullptr;
    {
        KeyFrame* kf = bitmapLayer->keyExists(frameNumber)
                       ? bitmapLayer->getKeyFrameAt(frameNumber)
                       : bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            CellBank* bank = bitmapLayer->cellBank();
            writeTarget = bank->getActiveInnerLayerImage(bkf->cellId());
            stack = bank->getCellLayerStack(bkf->cellId());
        }
    }
    if (!writeTarget) {
        // Fallback: write to composite (legacy path)
        writeTarget = bmi;
    }

    // Build selection mask covering the lasso path region
    const QRect bounds = bmi->bounds();
    QImage mask(bounds.size(), QImage::Format_Alpha8);
    mask.fill(0);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath local = path.translated(-bounds.topLeft());
        p.fillPath(local, Qt::white);
    }

    const int left = bounds.left();
    const int right = bounds.right();
    const int top = bounds.top();
    const int bottom = bounds.bottom();
    const int width = bounds.width();
    const int height = bounds.height();

    auto index = [left, top, width](int x, int y) -> qsizetype { return (y - top) * width + (x - left); };

    // Use editor-wide Ignore In Fill settings (shared with Bucket tool)
    const bool ignoreEnabled = mEditor->ignoreFillEnabled();
    const QRgb ignoreColor = mEditor->ignoreFillColor();

    auto inMask = [&](int x, int y) -> bool {
        if (x < left || x > right || y < top || y > bottom) return false;
        const uchar a = mask.constScanLine(y - top)[x - left];
        return a > 0; // inside lasso region
    };

    QVector<uchar> visited(static_cast<qsizetype>(width) * static_cast<qsizetype>(height));

    const QRgb fill = color.rgba();

    // Iterate through every pixel inside the lasso; for each unseen pixel, flood its color component
    for (int y = top; y <= bottom; ++y) {
        const uchar* mrow = reinterpret_cast<const uchar*>(mask.constScanLine(y - top));
        for (int x = left; x <= right; ++x) {
            if (mrow[x - left] == 0) continue; // only consider pixels inside lasso
            const qsizetype id0 = index(x, y);
            if (visited[id0]) continue;

            QRgb base = bmi->pixel(x, y);
            if (ignoreEnabled && ignoreColor != 0 && base == ignoreColor) { visited[id0] = 1; continue; }
            // All colors are eligible for recolor

            const QRgb componentColor = base;

            QVector<QPoint> stack;
            stack.reserve(256);
            QVector<QPoint> insidePixels;
            insidePixels.reserve(256);
            bool touchesOutsideMask = !inMask(x, y) ? true : false;

            auto tryPush = [&](int cx, int cy) {
                if (cx < left || cx > right || cy < top || cy > bottom) return;
                const qsizetype id = index(cx, cy);
                if (visited[id]) return;
                const QRgb px = bmi->pixel(cx, cy);
                if (ignoreEnabled && ignoreColor != 0 && px == ignoreColor) { visited[id] = 1; return; }
                if (px == componentColor) {
                    visited[id] = 1;
                    stack.append(QPoint(cx, cy));
                }
            };

            tryPush(x, y);

            while (!stack.isEmpty()) {
                const QPoint p = stack.takeLast();
                const int cx = p.x();
                const int cy = p.y();

                if (inMask(cx, cy)) insidePixels.append(p); else touchesOutsideMask = true;

                const int nx[4] = { cx + 1, cx - 1, cx, cx };
                const int ny[4] = { cy, cy, cy + 1, cy - 1 };
                for (int i = 0; i < 4; ++i) tryPush(nx[i], ny[i]);
            }

            if (!touchesOutsideMask && !insidePixels.isEmpty()) {
                for (const QPoint& p : insidePixels) {
                    // Write fill pixels to the active inner layer only (Req 8.6)
                    writeTarget->setPixel(p.x(), p.y(), fill);
                }
            }
        }
    }

    // Invalidate composite so CanvasPainter recomputes it on next render (Req 8.6)
    if (stack) stack->invalidateComposite();
} 