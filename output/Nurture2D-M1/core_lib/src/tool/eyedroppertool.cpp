/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "eyedroppertool.h"

#include <QPainter>
#include <QPixmap>
#include <QBitmap>
#include <QtMath>
#include "pointerevent.h"

#include "vectorimage.h"
#include "layervector.h"
#include "layerbitmap.h"
#include "colormanager.h"
#include "object.h"
#include "editor.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "util.h"
#include "settingsmanager.h"
#include "viewmanager.h"

EyedropperTool::EyedropperTool(QObject* parent) : BaseTool(parent)
{
}

void EyedropperTool::loadSettings()
{
    mPropertyEnabled[EYEDROPPER_REFERENCE_MODE] = true;

    properties.width = -1;
    properties.feather = -1;
    properties.useFeather = false;
    properties.useAA = -1;

    SettingsManager* settings = SettingsManager::instance();
    properties.eyedropperReferenceMode = settings->getValue("Tool/Eyedropper/ReferenceMode", static_cast<int>(EyedropperReferenceMode::FROM_VIEW)).toInt();
}

void EyedropperTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();
    settings->deferredSetValue("Tool/Eyedropper/ReferenceMode", properties.eyedropperReferenceMode);
    clearSettingsDirty();
}

QCursor EyedropperTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-eyedropper.svg"), 0, 15);
    }
    else
    {
        return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
    }
}

QCursor EyedropperTool::cursor(const QColor color)
{
    QPixmap icon(":icons/general/cursor-eyedropper.svg");

    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.drawPixmap(0, 0, icon);

    painter.setBrush(Qt::white);
    painter.drawRect(17, 17, 13, 13);
    painter.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    painter.drawRect(16, 16, 15, 15);
    painter.end();

    return QCursor(pixmap, 0, 15);
}

void EyedropperTool::pointerPressEvent(PointerEvent*)
{}

void EyedropperTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    const EyedropperReferenceMode mode = referenceMode();

    if (mode == EyedropperReferenceMode::FROM_VIEW) {
        const QColor pickedColor = sampleCompositedColor(event->viewportPos());

        // In continuous-pick mode (canvas Alt-hold), update the front color
        // directly as the cursor moves. Otherwise only preview via cursor.
        if (mContinuousPick && pickedColor.isValid()) {
            mEditor->color()->setFrontColor(pickedColor);
        }

        if (pickedColor.isValid()) {
            mScribbleArea->setCursor(cursor(pickedColor));
        } else {
            mScribbleArea->setCursor(cursor());
        }
        return;
    }

    if (layer->type() == Layer::BITMAP)
    {
        QColor pickedColor = getBitmapColor(static_cast<LayerBitmap*>(layer), event->canvasPos());

        if (mContinuousPick && pickedColor.isValid()) {
            mEditor->color()->setFrontColor(pickedColor);
        }

        if (pickedColor.isValid())
        {
            mScribbleArea->setCursor(cursor(pickedColor));
        }
        else
        {
            mScribbleArea->setCursor(cursor());
        }
    }
    if (layer->type() == Layer::VECTOR)
    {
        int pickedColor = getVectorColor(static_cast<LayerVector*>(layer), event->canvasPos());

        if (mContinuousPick && pickedColor >= 0)
        {
            mEditor->color()->setFrontColor(mEditor->object()->getColor(pickedColor).color);
        }

        if (pickedColor >= 0)
        {
            mScribbleArea->setCursor(cursor(mEditor->object()->getColor(pickedColor).color));
        }
        else
        {
            mScribbleArea->setCursor(cursor());
        }
    }
}

void EyedropperTool::pointerReleaseEvent(PointerEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        const EyedropperReferenceMode mode = referenceMode();
        const QColor composite = (mode == EyedropperReferenceMode::FROM_VIEW)
                                 ? sampleCompositedColor(event->viewportPos())
                                 : QColor();

        if (mSampleOnly) {
            if (mode == EyedropperReferenceMode::FROM_VIEW) {
                if (composite.isValid()) emit sampledColor(composite);
            } else {
                Layer* layer = mEditor->layers()->currentLayer();
                if (layer && layer->type() == Layer::BITMAP) {
                    QColor picked = getBitmapColor(static_cast<LayerBitmap*>(layer), event->canvasPos());
                    if (picked.isValid()) emit sampledColor(picked);
                } else if (layer && layer->type() == Layer::VECTOR) {
                    int idx = getVectorColor(static_cast<LayerVector*>(layer), event->canvasPos());
                    if (idx >= 0) emit sampledColor(mEditor->object()->getColor(idx).color);
                }
            }
        } else {
            if (mode == EyedropperReferenceMode::FROM_VIEW) {
                if (composite.isValid()) {
                    mEditor->color()->setFrontColor(composite);
                }
            } else {
                updateFrontColor(event->canvasPos());
            }
        }

        // reset cursor
        mScribbleArea->setCursor(cursor());
    }
}

void EyedropperTool::updateFrontColor(const QPointF& pos)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (referenceMode() == EyedropperReferenceMode::FROM_VIEW) {
        const QPointF vp = mEditor->view()->mapCanvasToScreen(pos);
        const QColor composited = sampleCompositedColor(vp);
        if (composited.isValid()) {
            mEditor->color()->setFrontColor(composited);
        }
        return;
    }

    if (layer->type() == Layer::BITMAP)
    {
        QColor pickedColor = getBitmapColor(static_cast<LayerBitmap*>(layer), pos);
        if (pickedColor.isValid())
        {
            mEditor->color()->setFrontColor(pickedColor);
        }
    }
    else if (layer->type() == Layer::VECTOR)
    {
        int pickedColor = getVectorColor(static_cast<LayerVector*>(layer), pos);
        if (pickedColor >= 0)
        {
            mEditor->color()->setColorNumber(pickedColor);
        }
    }
}

QColor EyedropperTool::getBitmapColor(LayerBitmap* layer, const QPointF& pos)
{
    BitmapImage* targetImage = layer->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
    if (targetImage == nullptr || !targetImage->contains(pos)) return QColor();

    QColor pickedColour;
    const QRgb pixelColor = targetImage->constScanLine(qFloor(pos.x()),
                                                       qFloor(pos.y()));
    pickedColour.setRgba(pixelColor);

    if (pickedColour.alpha() <= 0) pickedColour = QColor();
    return pickedColour;
}

QColor EyedropperTool::sampleCompositedColor(const QPointF& viewportPos) const
{
    if (!mScribbleArea) return QColor();

    const int x = qFloor(viewportPos.x());
    const int y = qFloor(viewportPos.y());
    if (x < 0 || y < 0) return QColor();

    QPixmap grab = mScribbleArea->grab(QRect(QPoint(x, y), QSize(1, 1)));
    if (grab.isNull()) return QColor();
    QImage img = grab.toImage();
    if (img.isNull() || img.width() < 1 || img.height() < 1) return QColor();
    QColor c(img.pixel(0, 0));
    if (c.alpha() <= 0) return QColor();
    return c;
}

int EyedropperTool::getVectorColor(LayerVector* layer, const QPointF& pos)
{
    auto vectorImage = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
    if (vectorImage == nullptr) return -1;

    // Check curves
    const qreal toleranceDistance = 10.0;
    const QList<int> closestCurves = vectorImage->getCurvesCloseTo(pos, toleranceDistance);
    const QList<int> visibleClosestCurves = filter(closestCurves, [vectorImage](int i) { return vectorImage->isCurveVisible(i); });

    if (!visibleClosestCurves.isEmpty())
    {
        return vectorImage->getCurvesColor(visibleClosestCurves.last());
    }

    // Check fills
    int colorNumber = vectorImage->getColorNumber(pos);
    return colorNumber;
}
