/*

Pencil2D - Traditional Animation Software

*/
#ifndef LASSOGAPFILLTOOL_H
#define LASSOGAPFILLTOOL_H

#include "basetool.h"
#include <QPainterPath>

class LassoGapFillTool : public BaseTool
{
    Q_OBJECT
public:
    explicit LassoGapFillTool(QObject* parent = nullptr);
    ToolType type() override { return LASSO_GAP_FILL; }
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
    void fillTransparentInPath(const QPainterPath& path, const QColor& color);

    bool m_isBuilding = false;
    QPainterPath m_path;
    QColor m_sampledColor;
    bool m_useActiveColor = false;
};

#endif // LASSOGAPFILLTOOL_H 