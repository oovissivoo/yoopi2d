#include "curvetapercontrol.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QStyleOption>
#include <QLineEdit>
#include <QDoubleValidator>

#include "../CUI/cosmicuimetrics.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

CurveTaperControl::CurveTaperControl(QWidget* parent)
    : QWidget(parent)
{
    mAccentColor = BlenderStyleSlider::getAccentColor();
    setMouseTracking(true);
}

QSize CurveTaperControl::minimumSizeHint() const
{
    return QSize(160, 72);
}

QSize CurveTaperControl::sizeHint() const
{
    return QSize(220, 80);
}

void CurveTaperControl::setStartValue(qreal value)
{
    value = qBound(0.0, value, 200.0);
    if (qFuzzyCompare(mStartValue, value)) {
        return;
    }
    mStartValue = value;
    updateLayoutRects();
    update();
}

void CurveTaperControl::setEndValue(qreal value)
{
    value = qBound(0.0, value, 200.0);
    if (qFuzzyCompare(mEndValue, value)) {
        return;
    }
    mEndValue = value;
    updateLayoutRects();
    update();
}

void CurveTaperControl::setAccentColor(const QColor& color)
{
    mAccentColor = color;
    update();
}

void CurveTaperControl::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    updateLayoutRects();

    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal midY = bounds.center().y();

    // Base colors
    const QColor trackColor = palette().color(QPalette::Mid).darker(115);
    const QColor trackOutlineColor = palette().color(QPalette::Dark);
    const QColor textColor = palette().color(QPalette::Text);
    const QColor cubeBase = palette().color(QPalette::Base).darker(130);
    const QColor cubeBorder = palette().color(QPalette::Shadow);
    const QColor cubeGlow = mAccentColor.lighter(125);

    const QColor circleFill = mAccentColor;
    const QColor circleOutline = mAccentColor.darker(140);
    const QColor fillColor = mAccentColor.lighter(110);

    auto drawFilledLane = [&](const QRectF& laneRect, const QRectF& circleRect, bool isStartSide) {
        if (laneRect.isNull()) return;

        const qreal laneThickness = 4.0;
        QRectF baseRect = laneRect.adjusted(0, -laneThickness * 0.5, 0, laneThickness * 0.5);

        // Base track
        p.setPen(QPen(trackOutlineColor, 1.0));
        p.setBrush(trackColor);
        p.drawRoundedRect(baseRect, 3, 3);

        // Filled portion from outer edge toward handle
        if (!circleRect.isNull()) {
            const qreal handleX = circleRect.center().x();
            QRectF filledRect;
            if (isStartSide) {
                // Left lane: fill from outer left to handle
                if (handleX > laneRect.left()) {
                    filledRect = QRectF(laneRect.left(), baseRect.top(),
                                        handleX - laneRect.left(), baseRect.height());
                }
            } else {
                // Right lane: fill from handle to outer right
                if (handleX < laneRect.right()) {
                    filledRect = QRectF(handleX, baseRect.top(),
                                        laneRect.right() - handleX, baseRect.height());
                }
            }

            if (!filledRect.isNull() && filledRect.width() > 0.5) {
                p.setPen(Qt::NoPen);
                p.setBrush(fillColor);
                QPainterPath filledPath;
                filledPath.addRoundedRect(filledRect, 3, 3);
                p.drawPath(filledPath);
            }
        }
    };

    drawFilledLane(mLeftLaneRect, mStartCircleRect, true);
    drawFilledLane(mRightLaneRect, mEndCircleRect, false);

    // Draw center cube halves
    auto drawCubeHalf = [&](const QRectF& cubeRect, bool glow) {
        if (cubeRect.isNull()) return;
        QPainterPath path;
        path.addRoundedRect(cubeRect, 4, 4);
        QColor fill = glow ? cubeGlow : cubeBase;
        p.setPen(QPen(cubeBorder, 1.0));
        p.setBrush(fill);
        p.drawPath(path);
    };

    const bool leftMax = mStartValue >= 199.9;
    const bool rightMax = mEndValue >= 199.9;
    drawCubeHalf(mLeftCubeRect, leftMax);
    drawCubeHalf(mRightCubeRect, rightMax);

    // Divider line between cube halves
    if (!mLeftCubeRect.isNull() && !mRightCubeRect.isNull()) {
        const qreal x = mLeftCubeRect.right();
        p.setPen(QPen(cubeBorder.lighter(130), 1.0));
        p.drawLine(QPointF(x, mLeftCubeRect.top() + 2.0),
                   QPointF(x, mLeftCubeRect.bottom() - 2.0));
    }

    // Draw circles and numeric values
    auto drawCircle = [&](const QRectF& circleRect, qreal value, const QString& label) {
        if (circleRect.isNull()) return;

        p.setPen(QPen(circleOutline, 1.0));
        p.setBrush(circleFill);
        p.drawEllipse(circleRect);

        // Value text (integer pixels only)
        const int intVal = qRound(value);
        const QString valueText = QString::number(intVal);

        p.setPen(textColor);
        QFont valueFont = p.font();
        valueFont.setBold(true);
        p.setFont(valueFont);
        p.drawText(circleRect, Qt::AlignCenter, valueText);
    };

    drawCircle(mStartCircleRect, mStartValue, tr("Start Taper (px)"));
    drawCircle(mEndCircleRect, mEndValue, tr("End Taper (px)"));
}

void CurveTaperControl::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Ctrl+click for inline numeric edit
    if (event->modifiers() & Qt::ControlModifier) {
        if (mStartCircleRect.contains(event->pos())) {
            beginInlineEdit(StartSide, event->pos());
            return;
        } else if (mEndCircleRect.contains(event->pos())) {
            beginInlineEdit(EndSide, event->pos());
            return;
        }
    }

    if (mLeftLaneRect.contains(event->pos()) || mStartCircleRect.contains(event->pos())) {
        mActiveSide = StartSide;
    } else if (mRightLaneRect.contains(event->pos()) || mEndCircleRect.contains(event->pos())) {
        mActiveSide = EndSide;
    } else {
        mActiveSide = None;
    }

    if (mActiveSide != None) {
        mDragging = true;
        emit interactionBegun();
        updateValueFromPos(event->pos());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void CurveTaperControl::mouseMoveEvent(QMouseEvent* event)
{
    if (!mDragging || mActiveSide == None) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    updateValueFromPos(event->pos());
}

void CurveTaperControl::mouseReleaseEvent(QMouseEvent* event)
{
    if (mDragging && mActiveSide != None && event->button() == Qt::LeftButton) {
        updateValueFromPos(event->pos());
        mDragging = false;
        mActiveSide = None;
        emit interactionEnded();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void CurveTaperControl::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayoutRects();
}

void CurveTaperControl::updateLayoutRects()
{
    const QRectF r = rect().adjusted(mHorizontalMargin, 10.0, -mHorizontalMargin, -24.0);
    if (r.width() <= 0 || r.height() <= 0) {
        mLeftLaneRect = QRectF();
        mRightLaneRect = QRectF();
        mLeftCubeRect = QRectF();
        mRightCubeRect = QRectF();
        mStartCircleRect = QRectF();
        mEndCircleRect = QRectF();
        return;
    }

    const qreal midY = r.center().y();
    const qreal fullCubeWidth = mCubeWidth;
    const qreal halfCubeWidth = fullCubeWidth * 0.5;

    const qreal cubeLeft = r.center().x() - halfCubeWidth;
    const qreal cubeRight = r.center().x() + halfCubeWidth;

    mLeftCubeRect = QRectF(cubeLeft, midY - mCubeHeight * 0.5, halfCubeWidth, mCubeHeight);
    mRightCubeRect = QRectF(cubeLeft + halfCubeWidth, midY - mCubeHeight * 0.5, halfCubeWidth, mCubeHeight);

    const qreal laneY = midY;
    const qreal laneThickness = 4.0;

    const qreal leftLaneStart = r.left() + mCircleRadius;
    qreal leftLaneEnd = mLeftCubeRect.left() - (mCircleRadius + mHandleGapFromCube);
    if (leftLaneEnd < leftLaneStart) {
        leftLaneEnd = leftLaneStart;
    }

    qreal rightLaneStart = mRightCubeRect.right() + (mCircleRadius + mHandleGapFromCube);
    const qreal rightLaneEnd = r.right() - mCircleRadius;
    if (rightLaneStart > rightLaneEnd) {
        rightLaneStart = rightLaneEnd;
    }

    if (leftLaneEnd <= leftLaneStart) {
        mLeftLaneRect = QRectF();
    } else {
        mLeftLaneRect = QRectF(leftLaneStart, laneY - laneThickness * 0.5,
                               leftLaneEnd - leftLaneStart, laneThickness);
    }

    if (rightLaneEnd <= rightLaneStart) {
        mRightLaneRect = QRectF();
    } else {
        mRightLaneRect = QRectF(rightLaneStart, laneY - laneThickness * 0.5,
                                rightLaneEnd - rightLaneStart, laneThickness);
    }

    auto computeCircleRect = [&](qreal value, const QRectF& laneRect, bool isStart) -> QRectF {
        if (laneRect.isNull()) {
            return QRectF();
        }
        const qreal t = qBound<qreal>(0.0, value / 200.0, 1.0);
        qreal x;
        if (isStart) {
            // 0 at outer edge, 200 at cube side
            x = laneRect.left() + t * laneRect.width();
        } else {
            // 0 at outer edge (right), 200 at cube side (left)
            x = laneRect.right() - t * laneRect.width();
        }
        const qreal y = laneRect.center().y();
        return QRectF(x - mCircleRadius, y - mCircleRadius,
                      mCircleRadius * 2.0, mCircleRadius * 2.0);
    };

    mStartCircleRect = computeCircleRect(mStartValue, mLeftLaneRect, true);
    mEndCircleRect = computeCircleRect(mEndValue, mRightLaneRect, false);
}

void CurveTaperControl::applyStartValue(qreal value)
{
    value = qBound(0.0, value, 200.0);
    if (qFuzzyCompare(mStartValue, value)) {
        return;
    }
    mStartValue = value;
    updateLayoutRects();
    update();
    emit startTaperChanged(mStartValue);
}

void CurveTaperControl::applyEndValue(qreal value)
{
    value = qBound(0.0, value, 200.0);
    if (qFuzzyCompare(mEndValue, value)) {
        return;
    }
    mEndValue = value;
    updateLayoutRects();
    update();
    emit endTaperChanged(mEndValue);
}

void CurveTaperControl::updateValueFromPos(const QPoint& pos)
{
    if (mActiveSide == StartSide && !mLeftLaneRect.isNull()) {
        const qreal x = qBound(mLeftLaneRect.left(), static_cast<qreal>(pos.x()), mLeftLaneRect.right());
        const qreal t = (mLeftLaneRect.width() > 0.0) ? (x - mLeftLaneRect.left()) / mLeftLaneRect.width() : 0.0;
        applyStartValue(t * 200.0);
    } else if (mActiveSide == EndSide && !mRightLaneRect.isNull()) {
        const qreal x = qBound(mRightLaneRect.left(), static_cast<qreal>(pos.x()), mRightLaneRect.right());
        const qreal t = (mRightLaneRect.width() > 0.0) ? (mRightLaneRect.right() - x) / mRightLaneRect.width() : 0.0;
        applyEndValue(t * 200.0);
    }
}

void CurveTaperControl::beginInlineEdit(ActiveSide side, const QPoint& mousePos)
{
    Q_UNUSED(mousePos);

    QRectF targetCircle;
    qreal currentValue = 0.0;

    if (side == StartSide) {
        targetCircle = mStartCircleRect;
        currentValue = mStartValue;
    } else if (side == EndSide) {
        targetCircle = mEndCircleRect;
        currentValue = mEndValue;
    } else {
        return;
    }

    if (targetCircle.isNull()) {
        return;
    }

    destroyInlineEditor();

    mInlineEditorSide = side;
    mInlineEditor = new QLineEdit(this);
    mInlineEditor->setFrame(false);
    mInlineEditor->setValidator(new QDoubleValidator(0.0, 200.0, 1, mInlineEditor));
    mInlineEditor->setText(QString::number(qRound(currentValue)));
    mInlineEditor->setAlignment(Qt::AlignCenter);
    mInlineEditor->setTextMargins(0, 0, 0, 0);
    mInlineEditor->setStyleSheet(CosmicInlineEdit::stylesheet());

    const int w = static_cast<int>(targetCircle.width()) + 12;
    const int h = fontMetrics().height() + 6;
    mInlineEditor->resize(w, h);
    const QPoint center = targetCircle.center().toPoint();
    mInlineEditor->move(center.x() - w / 2, center.y() - h / 2);

    connect(mInlineEditor, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        const double v = mInlineEditor->text().toDouble(&ok);
        if (ok) {
            const qreal clamped = qBound(0.0, static_cast<qreal>(v), 200.0);
            if (mInlineEditorSide == StartSide) {
                applyStartValue(clamped);
            } else if (mInlineEditorSide == EndSide) {
                applyEndValue(clamped);
            }
        }
        destroyInlineEditor();
    });

    mInlineEditor->show();
    mInlineEditor->setFocus();
    mInlineEditor->selectAll();
}

void CurveTaperControl::destroyInlineEditor()
{
    if (mInlineEditor) {
        mInlineEditor->deleteLater();
        mInlineEditor = nullptr;
        mInlineEditorSide = None;
    }
}
