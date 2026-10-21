/*

Pencil2D - Traditional Animation Software

*/
#ifndef RECTGAPFILLTOOL_H
#define RECTGAPFILLTOOL_H

#include "basetool.h"
#include <QPointF>
#include <QRectF>

class RectGapFillTool : public BaseTool
{
    Q_OBJECT
public:
    explicit RectGapFillTool(QObject* parent = nullptr);
    ToolType type() override { return RECT_GAP_FILL; }
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;

    void setUseActiveColor(bool enabled) { m_useActiveColor = enabled; }
    bool useActiveColor() const { return m_useActiveColor; }

protected:
    bool keyPressEvent(QKeyEvent* event) override;

private:
    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void drawPreview();
    void fillTransparentInRect(const QRect& rect, const QColor& color);

    bool m_isSelecting = false;
    QPointF m_pressPos;
    QPointF m_currPos;
    QColor m_sampledColor;
    bool m_useActiveColor = false;
};

#endif // RECTGAPFILLTOOL_H 