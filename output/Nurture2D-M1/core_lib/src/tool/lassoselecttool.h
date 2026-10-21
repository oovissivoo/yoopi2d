#ifndef LASSOSELECTTOOL_H
#define LASSOSELECTTOOL_H

#include "basetool.h"
#include <QPainterPath>
#include <QCursor>
#include <QTransform>
#include "managers/undoredomanager.h"

class LassoSelectTool : public BaseTool
{
    Q_OBJECT
public:
    explicit LassoSelectTool(QObject* parent = nullptr);
    ToolType type() override { return LASSO_SELECT; }
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void setAA(const int useAA) override;
    bool handleDeselectAll() override;

protected:
    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    bool leavingThisTool() override;
    bool keyPressEvent(QKeyEvent* event) override;

private:
    // State for building lasso path
    bool m_isBuilding = false;
    QPainterPath m_path;

    // State for moving content within selection (Ctrl+drag)
    bool m_isDraggingMove = false;
    QPointF m_pressPoint;
    QPointF m_pressViewportPoint;
    // State for scaling/rotation of temporary overlay via handles
    bool m_isScaling = false;
    enum class Handle { None, TopLeft, TopRight, BottomRight, BottomLeft };
    Handle m_activeHandle = Handle::None;
    static constexpr int HANDLE_SIZE = 12; // device-space square — matches shift-trace gizmo corner handles
    QRectF m_initialRect; // canvas-space rect at scaling start
    QPointF m_scalePressViewportPoint;
    QTransform m_initialSelectionTransformForScale;
    qreal m_scaleStartX = 1.0;
    qreal m_scaleStartY = 1.0;

    // Rotation state (device-space handle above top center)
    bool m_isRotating = false;
    qreal m_startRotationAngle = 0.0; // canvas-space angle at rotation start
    qreal m_initialRotationAngle = 0.0;
    int mRotationIncrement = 0;       // snap increment (degrees) from preferences
    static constexpr int ROTATION_HANDLE_OFFSET = 30; // device-space pixels above top center

    // Undo state used to coalesce the move into one record
    const UndoSaveState* mUndoState = nullptr;
    bool mUndoCapturedForDrag = false;
    bool mDidStartDragCut = false;
    bool mContentWasMoved = false;  // Track if content was actually moved (for undo)

    // Cached gizmo canvas rect (selection path bounds + padding converted from device px)
    QRectF m_lastGizmoCanvasRect;
    // Cached gizmo screen rect (ring outer circle + handles, device space)
    QRect m_lastGizmoScreenRect;
};

#endif // LASSOSELECTTOOL_H
 