#pragma once

#include "basetool.h"
#include "undoredomanager.h"
#include <QColor>
#include <QPainterPath>
#include <QRectF>

class DplateTool : public BaseTool {
    Q_OBJECT
public:
    explicit DplateTool(QObject* parent = nullptr);
    ToolType type() override { return DPLATE; }
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void resetToDefault() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void setWidth(const qreal width) override;
    bool isActive() const override { return mDragging || mPreviewActive; }

    // External UI hooks
    void showCenterPreview(qreal unit);
    void clearPreview();
    void setStrokeColor(const QColor& c) { mStrokeColor = c; }

    // Box count controls (persisted)
    int bigBoxCount() const { return mBigBoxCount; }
    void setBigBoxCount(int count);

    int smallBoxCount() const { return mSmallBoxCount; }
    void setSmallBoxCount(int count);

private:
    QPainterPath buildShapePath(const QPointF& topLeft, qreal unit) const;
    void drawPreviewAt(const QPointF& topLeft, qreal unit);

    QPointF mStartPoint;
    bool mDragging = false;
    bool mPreviewActive = false;
    QColor mStrokeColor = Qt::black;
    QColor mFillColor = QColor(248, 248, 248);

    // Box counts (big = downward column of u×u squares, small = upward column of (u/2)×(u/2) squares)
    int mBigBoxCount = 3;   // 0..3
    int mSmallBoxCount = 0; // 0..3

    // Track previous preview bounds (in canvas coordinates) to properly invalidate old areas
    QRectF mLastPreviewCanvasBounds;
    bool mHasLastPreviewBounds = false;

    // Undo/Redo support
    const UndoSaveState* mUndoSaveState = nullptr;
}; 