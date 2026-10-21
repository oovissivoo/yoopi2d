#include "referencecanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QApplication>
#include <cmath>

ReferenceCanvas::ReferenceCanvas(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void ReferenceCanvas::setImage(const QImage& img)
{
    mImage = img;
    if (!mImage.isNull()) {
        // Update widget size according to current zoom
        resize(mImage.size() * mZoomScale);
    } else {
        // Shrink to a minimal size so the scroll area background shows through
        resize(1, 1);
    }
    update();

    // Also repaint the scroll area viewport so the background redraws immediately
    QWidget* p = parentWidget();
    while (p && !qobject_cast<QAbstractScrollArea*>(p)) p = p->parentWidget();
    if (auto* sa = qobject_cast<QAbstractScrollArea*>(p))
        sa->viewport()->update();
}

void ReferenceCanvas::setZoomScale(double scale)
{
    if (scale <= 0) return;

    // Find enclosing QAbstractScrollArea (the scroll area we were placed into)
    QWidget* p = this->parentWidget();
    while (p && !qobject_cast<QAbstractScrollArea*>(p)) p = p->parentWidget();
    QAbstractScrollArea* sa = qobject_cast<QAbstractScrollArea*>(p);

    // If we have a scroll area, compute the image point currently at the
    // viewport center (in image coordinates), then after resizing keep that
    // same image point centered by adjusting scrollbars.
    double oldScale = mZoomScale;
    QPointF viewCenter;
    QPointF imagePoint;
    if (sa && !mImage.isNull()) {
        // Use scrollbar offsets + viewport center to get the center point in
        // canvas widget coordinates. This is more robust than mapFrom when
        // alignment or parenting changes.
        int hVal = sa->horizontalScrollBar()->value();
        int vVal = sa->verticalScrollBar()->value();
        int vpW = sa->viewport()->width();
        int vpH = sa->viewport()->height();
        viewCenter = QPointF(hVal + vpW / 2.0, vVal + vpH / 2.0);

        QSizeF oldDispSize(mImage.width() * oldScale, mImage.height() * oldScale);
        QPointF oldImageTopLeft((width() - oldDispSize.width()) / 2.0, (height() - oldDispSize.height()) / 2.0);
        imagePoint = QPointF((viewCenter.x() - oldImageTopLeft.x()) / oldScale,
                             (viewCenter.y() - oldImageTopLeft.y()) / oldScale);
    }

    // Apply new scale and resize widget
    mZoomScale = scale;
    if (!mImage.isNull()) resize(mImage.size() * mZoomScale);
    update();

    // Adjust scrollbars so the same image point remains at the viewport center
    if (sa && !mImage.isNull()) {
        QSizeF newDispSize(mImage.width() * mZoomScale, mImage.height() * mZoomScale);
        QPointF newImageTopLeft((width() - newDispSize.width()) / 2.0, (height() - newDispSize.height()) / 2.0);
        QPointF newViewCenter = QPointF(newImageTopLeft.x() + imagePoint.x() * mZoomScale,
                                        newImageTopLeft.y() + imagePoint.y() * mZoomScale);

        // Desired scrollbar values that place newViewCenter at viewport center
        int vpW = sa->viewport()->width();
        int vpH = sa->viewport()->height();
        int desiredH = static_cast<int>(std::round(newViewCenter.x() - vpW / 2.0));
        int desiredV = static_cast<int>(std::round(newViewCenter.y() - vpH / 2.0));

        // Clamp to valid scrollbar range
        desiredH = qBound(sa->horizontalScrollBar()->minimum(), desiredH, sa->horizontalScrollBar()->maximum());
        desiredV = qBound(sa->verticalScrollBar()->minimum(), desiredV, sa->verticalScrollBar()->maximum());

        sa->horizontalScrollBar()->setValue(desiredH);
        sa->verticalScrollBar()->setValue(desiredV);
    }
}

void ReferenceCanvas::fitToView()
{
    if (mImage.isNull()) return;
    // Find enclosing QAbstractScrollArea (the scroll area we were placed into)
    QWidget* p = this->parentWidget();
    while (p && !qobject_cast<QAbstractScrollArea*>(p)) p = p->parentWidget();
    QSize vp;
    if (auto* sa = qobject_cast<QAbstractScrollArea*>(p)) {
        vp = sa->viewport()->size();
    } else {
        // Fallback to our own size
        vp = size();
    }
    QSize img = mImage.size();
    double sx = double(vp.width()) / double(img.width());
    double sy = double(vp.height()) / double(img.height());
    double s = qMin(sx, sy);
    if (s <= 0) s = 1.0;
    setZoomScale(s);
}

void ReferenceCanvas::paintEvent(QPaintEvent* ev)
{
    Q_UNUSED(ev)
    QPainter p(this);
    // Match the dark gray background used across the app's canvas areas
    QColor bg(40,40,40);
    p.fillRect(rect(), bg);
    if (mImage.isNull()) return;

    // Draw the image centered within this widget and scaled by mZoomScale.
    QSizeF dispSize(mImage.width() * mZoomScale, mImage.height() * mZoomScale);
    QPointF topLeft((width() - dispSize.width()) / 2.0, (height() - dispSize.height()) / 2.0);
    QRectF targetRect(topLeft, dispSize);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(targetRect, mImage);
}

void ReferenceCanvas::mousePressEvent(QMouseEvent* ev)
{
    // Color sampling is now handled by Alt+hold, no need for click sampling

    if (ev->button() == Qt::LeftButton) {
        // Start panning; track global position for 1:1 movement with cursor
        mIsPanning = true;
        mPanLastPos = ev->globalPos();
        setCursor(Qt::ClosedHandCursor);
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void ReferenceCanvas::mouseMoveEvent(QMouseEvent* ev)
{
    if (mIsPanning) {
        QPoint delta = ev->globalPos() - mPanLastPos;
        mPanLastPos = ev->globalPos();
        // forward to scrollbars on parent scroll area if present
        QWidget* p = parentWidget();
        // climb the parent chain to find the QAbstractScrollArea
        while (p && !qobject_cast<QAbstractScrollArea*>(p)) p = p->parentWidget();
        if (auto* sa = qobject_cast<QAbstractScrollArea*>(p)) {
            sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() - delta.x());
            sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - delta.y());
        }
        ev->accept();
        return;
    }
    
    // Handle continuous color sampling while Alt is held (or when sampling state active)
    if (mColorSampleEnabled && (mIsSampling || (QApplication::keyboardModifiers() & Qt::AltModifier))) {
        // If Alt is held but we haven't entered sampling, enter now
        if (!mIsSampling && (QApplication::keyboardModifiers() & Qt::AltModifier)) {
            mIsSampling = true;
            // show eyedropper cursor immediately
            QPixmap icon(":icons/general/cursor-eyedropper.svg");
            QPixmap pixmap(32, 32);
            pixmap.fill(Qt::transparent);
            QPainter p(&pixmap);
            p.drawPixmap(0, 0, icon);
            p.setBrush(Qt::white);
            p.drawRect(17, 17, 13, 13);
            p.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::black);
            p.drawRect(16, 16, 15, 15);
            p.end();
            setCursor(QCursor(pixmap, 0, 15));
        }

        // Only sample if Alt is actually held down
        if (QApplication::keyboardModifiers() & Qt::AltModifier) {
            QPoint imgPt = QPoint(static_cast<int>(ev->pos().x() / mZoomScale), static_cast<int>(ev->pos().y() / mZoomScale));
            if (mImage.rect().contains(imgPt)) {
                QColor c = QColor::fromRgba(mImage.pixel(imgPt));
                if (c.isValid()) {
                    emit colorSampled(c);
                    // Update cursor with sampled color
                    updateEyedropperCursor(c);
                }
            }
        } else {
            // Alt no longer held; exit sampling state
            if (mIsSampling) {
                mIsSampling = false;
                unsetCursor();
            }
        }
        ev->accept();
        return;
    }
    
    QWidget::mouseMoveEvent(ev);
}

void ReferenceCanvas::mouseReleaseEvent(QMouseEvent* ev)
{
    if (mIsPanning && ev->button() == Qt::LeftButton) {
        mIsPanning = false;
        setCursor(Qt::ArrowCursor);
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void ReferenceCanvas::wheelEvent(QWheelEvent* ev)
{
    // Zoom by wheel while Ctrl pressed
    if (ev->modifiers() & Qt::ControlModifier) {
        double numSteps = ev->angleDelta().y() / 120.0;
        double factor = std::pow(1.1, numSteps);
        
        // Find enclosing QAbstractScrollArea (the scroll area we were placed into)
        QWidget* p = parentWidget();
        while (p && !qobject_cast<QAbstractScrollArea*>(p)) p = p->parentWidget();
        QAbstractScrollArea* sa = qobject_cast<QAbstractScrollArea*>(p);

        // Determine the pivot point in this widget's coordinates. Prefer the
        // center of the visible viewport (what the user is actually looking at).
        // If we don't have a scroll area, fall back to the widget center.
        QPointF viewCenter;
        if (sa) {
            QPoint vpCenter = sa->viewport()->rect().center();
            viewCenter = mapFrom(sa->viewport(), vpCenter);
        } else {
            viewCenter = QPointF(width() / 2.0, height() / 2.0);
        }

        // Calculate the image's top-left position (centered within widget)
        QSizeF dispSize(mImage.width() * mZoomScale, mImage.height() * mZoomScale);
        QPointF imageTopLeft((width() - dispSize.width()) / 2.0, (height() - dispSize.height()) / 2.0);
        
        // Convert view center to image coordinates (for future use)
        Q_UNUSED(QPointF((viewCenter.x() - imageTopLeft.x()) / mZoomScale, 
                        (viewCenter.y() - imageTopLeft.y()) / mZoomScale));
        
        // Apply zoom
        double newScale = mZoomScale * factor;
        newScale = qBound(0.1, newScale, 10.0); // reasonable limits
        setZoomScale(newScale);
        
        // Let setZoomScale handle resizing and scrollbar adjustments.
        setZoomScale(newScale);
        ev->accept();
        return;
    }
    QWidget::wheelEvent(ev);
}

void ReferenceCanvas::updateEyedropperCursor(const QColor& color)
{
    // Create eyedropper cursor with color preview
    QPixmap icon(":icons/general/cursor-eyedropper.svg");
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.drawPixmap(0, 0, icon);
    
    // Add color preview square
    painter.setBrush(Qt::white);
    painter.drawRect(17, 17, 13, 13);
    painter.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    painter.drawRect(16, 16, 15, 15);
    painter.end();
    
    setCursor(QCursor(pixmap, 0, 15));
}

void ReferenceCanvas::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Alt) {
        // Begin sampling mode on Alt press
        if (mColorSampleEnabled && !mIsSampling) {
            mIsSampling = true;
            // Use eyedropper cursor with color preview
            QPixmap icon(":icons/general/cursor-eyedropper.svg");
            QPixmap pixmap(32, 32);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.drawPixmap(0, 0, icon);
            // Add color preview square (will be updated on mouse move)
            painter.setBrush(Qt::white);
            painter.drawRect(17, 17, 13, 13);
            painter.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::black); // Default color until we sample
            painter.drawRect(16, 16, 15, 15);
            painter.end();
            setCursor(QCursor(pixmap, 0, 15));
            
            // Immediately sample color at current cursor position
            QPoint globalPos = QCursor::pos();
            QPoint localPos = mapFromGlobal(globalPos);
            if (rect().contains(localPos)) {
                QPoint imgPt = QPoint(static_cast<int>(localPos.x() / mZoomScale), static_cast<int>(localPos.y() / mZoomScale));
                if (mImage.rect().contains(imgPt)) {
                    QColor c = QColor::fromRgba(mImage.pixel(imgPt));
                    emit colorSampled(c);
                    updateEyedropperCursor(c);
                }
            }
            
            // Ensure we keep focus to receive mouse move events
            setFocus();
        }
        ev->accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}

void ReferenceCanvas::keyReleaseEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Alt) {
        // End sampling mode on Alt release
        if (mIsSampling) {
            mIsSampling = false;
            unsetCursor();
        }
        ev->accept();
        return;
    }
    QWidget::keyReleaseEvent(ev);
}



