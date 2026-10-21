#include "rectgapfilltool.h"

#include <QCursor>
#include <QPixmap>
#include <QPainter>
#include "pointerevent.h"
#include "editor.h"
#include "layermanager.h"
#include "layer.h"
#include "layerbitmap.h"
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

RectGapFillTool::RectGapFillTool(QObject* parent) : BaseTool(parent)
{
}

void RectGapFillTool::loadSettings()
{
    // Enable AA option for nicer preview lines
    mPropertyEnabled[ANTI_ALIASING] = true;
    SettingsManager* settings = SettingsManager::instance();
    m_useActiveColor = settings->getValue("GapFill/UseActiveColor", false).toBool();

    // Ensure preview selection does not reappear after undo/redo when this tool is active
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

void RectGapFillTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();
    settings->deferredSetValue("GapFill/UseActiveColor", m_useActiveColor);
}

QCursor RectGapFillTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":/icons/themes/playful/general/cursor-crosshair.svg"), 8, 8);
    }
    return Qt::CrossCursor;
}

bool RectGapFillTool::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt) {
        mEditor->tools()->setEyedropperSampleOnly(true);
        if (mEditor->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_Alt)) {
            if (auto* dropper = qobject_cast<EyedropperTool*>(mEditor->tools()->getTool(EYEDROPPER))) {
                // One-shot connection to set ignore color from sample
                auto colorConn = std::make_shared<QMetaObject::Connection>();
                *colorConn = QObject::connect(dropper, &EyedropperTool::sampledColor, this, [this, colorConn](const QColor& picked){
                    if (!picked.isValid()) { QObject::disconnect(*colorConn); return; }
                    mEditor->setIgnoreFillColor(qPremultiply(picked.rgba()));
                    QObject::disconnect(*colorConn);
                }, Qt::UniqueConnection);
            }
            // Ensure sample-only is disabled when temp tool ends
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

void RectGapFillTool::pointerPressEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer) return;

    // Start with a clean overlay selection
    mEditor->select()->resetSelectionProperties();

    m_isSelecting = true;
    m_pressPos = event->canvasPos();
    m_currPos = m_pressPos;

    // Sample color at press point from current bitmap
    if (layer->type() == Layer::BITMAP) {
        BitmapImage* bmi = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
        if (bmi)
        {
            QRgb px = bmi->pixel(m_pressPos.toPoint());
            m_sampledColor = QColor::fromRgba(px);
        }
    }

    // Seed a minimal selection rect so the user sees feedback immediately
    mEditor->select()->setSelection(QRectF(m_pressPos, QSizeF(1,1)), /*roundPixels*/ true);
    mScribbleArea->updateFrame();
    event->accept();
}

void RectGapFillTool::pointerMoveEvent(PointerEvent* event)
{
    if (!m_isSelecting) return;
    m_currPos = event->canvasPos();
    drawPreview();
    event->accept();
}

void RectGapFillTool::pointerReleaseEvent(PointerEvent* event)
{
    if (!m_isSelecting || event->button() != Qt::LeftButton) return;
    m_isSelecting = false;

    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        // Always clear preview selection
        mEditor->select()->resetSelectionProperties();
        mScribbleArea->updateFrame();
        return;
    }
    BitmapImage* bmi = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
    if (!bmi || !bmi->isLoaded()) {
        mEditor->select()->resetSelectionProperties();
        mScribbleArea->updateFrame();
        return;
    }

    QRect rect = QRect(m_pressPos.toPoint(), m_currPos.toPoint()).normalized();
    rect = rect.intersected(bmi->bounds());
    if (!rect.isValid() || rect.isEmpty()) {
        // Clear the preview even if we did not fill
        mEditor->select()->resetSelectionProperties();
        mScribbleArea->updateFrame();
        event->accept();
        return;
    }

    // Capture undo, perform fill, then record
    const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    const QColor fillColor = m_useActiveColor ? mEditor->color()->frontColor() : m_sampledColor;
    fillTransparentInRect(rect, fillColor);
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    if (mEditor && mEditor->undoRedo()) mEditor->undoRedo()->record(saveState, typeName());

    // Clear the preview selection overlay
    mEditor->select()->resetSelectionProperties();
    mScribbleArea->updateFrame();
    event->accept();
}

void RectGapFillTool::drawPreview()
{
    if (!m_isSelecting) return;
    QRectF rf(m_pressPos, m_currPos);
    mEditor->select()->setSelection(rf.normalized(), /*roundPixels*/ true);
    mScribbleArea->updateFrame();
}

void RectGapFillTool::fillTransparentInRect(const QRect& rect, const QColor& color)
{
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    BitmapImage* bmi = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
    if (!bmi) return;

    const QRect bounds = bmi->bounds();
    if (rect.isEmpty() || !bounds.intersects(rect)) return;

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

    // We will traverse connected components by exact color match and recolor only
    // those components that are FULLY contained by the selection rectangle.
    // To support components extending outside the rectangle while we test containment
    // we keep a visited bitmap for the whole canvas bounds.
    QVector<uchar> visited(static_cast<qsizetype>(width) * static_cast<qsizetype>(height));

    const QRgb fill = color.rgba();

    const int selLeft = rect.left();
    const int selRight = rect.right();
    const int selTop = rect.top();
    const int selBottom = rect.bottom();

    auto inSelection = [&](int x, int y) -> bool {
        return x >= selLeft && x <= selRight && y >= selTop && y <= selBottom;
    };

    for (int y = selTop; y <= selBottom; ++y) {
        for (int x = selLeft; x <= selRight; ++x) {
            if (x < left || x > right || y < top || y > bottom) continue; // outside image
            const qsizetype id0 = index(x, y);
            if (visited[id0]) continue;

            QRgb base = bmi->pixel(x, y);
            if (ignoreEnabled && ignoreColor != 0 && base == ignoreColor) { visited[id0] = 1; continue; }
            // No barrier check – all colors are eligible

            // Flood the entire color-connected component (4-connected)
            QVector<QPoint> stack;
            stack.reserve(256);
            QVector<QPoint> insidePixels; // pixels of this component that are inside the selection
            insidePixels.reserve(256);
            bool touchesOutsideSelection = !inSelection(x, y) ? true : false;

            const QRgb componentColor = base;

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

                if (inSelection(cx, cy)) insidePixels.append(p); else touchesOutsideSelection = true;

                const int nx[4] = { cx + 1, cx - 1, cx, cx };
                const int ny[4] = { cy, cy, cy + 1, cy - 1 };
                for (int i = 0; i < 4; ++i) tryPush(nx[i], ny[i]);
            }

            // Recolor only if the whole color component lies within the selection
            if (!touchesOutsideSelection && !insidePixels.isEmpty()) {
                for (const QPoint& p : insidePixels) {
                    bmi->setPixel(p.x(), p.y(), fill);
                }
            }
        }
    }
} 