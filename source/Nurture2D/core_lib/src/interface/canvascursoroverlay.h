#ifndef CANVASCURSOROVERLAY_H
#define CANVASCURSOROVERLAY_H

#include <QWidget>
#include <QRect>
#include <QTransform>

#include "canvascursorpainter.h"

class CanvasCursorOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasCursorOverlay(QWidget* parent = nullptr);

    void setCursorState(const CanvasCursorPainterOptions& options, const QTransform& viewTransform, qreal opacity);
    void clearCursor();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRect computeCursorRect(const CanvasCursorPainterOptions& options, const QTransform& viewTransform) const;

    CanvasCursorPainter mPainter;
    CanvasCursorPainterOptions mOptions;
    QTransform mViewTransform;
    QRect mLastRect;
    bool mHasCursor = false;
    qreal mOpacity = 1.0;
};

#endif
