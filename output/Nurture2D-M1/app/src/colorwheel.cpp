/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include <QVBoxLayout>
#include <QtMath>
#include <QPainter>
#include <QResizeEvent>
#include <QStyleOption>
#include <QRect>
#include <QDebug>
#include <QIcon>
#include <QSvgRenderer>
#include <QLineEdit>
#include <QRegularExpression>
#include <QPainterPath>
#include <QSettings>
#include "pencildef.h"

#include "colorwheel.h"
#include <limits>

ColorWheel::ColorWheel(QWidget* parent) : QWidget(parent)
{
    setWindowTitle(tr("Color Wheel", "Color Wheel's window title"));
    mCurrentColor = mCurrentColor.toHsv();
    setMinimumHeight(100);

    // Load last used wheel mode
    QSettings settings(PENCIL2D, PENCIL2D);
    int savedMode = settings.value("UI/ColorWheelMode", 0).toInt();
    mMode = (savedMode == 1) ? Mode::TriangleHSL : Mode::SquareHSV;
    // Create hex editor shown at bottom-center
    mHexEdit = new QLineEdit(this);
    mHexEdit->setMaxLength(7); // #RRGGBB
    mHexEdit->setPlaceholderText("#RRGGBB");
    mHexEdit->setAlignment(Qt::AlignCenter);
    mHexEdit->setFixedHeight(22);
    mHexEdit->setText(colorToHex(mCurrentColor));
    // Add rounded corners and a dedicated 353535 background so the hex
    // field reads as a slightly recessed control inside the 2f2f2f dock
    // panel.
    mHexEdit->setStyleSheet("QLineEdit { border-radius: 4px; background-color: #353535; }");
    // Update color when user edits and presses Enter or focus leaves
    connect(mHexEdit, &QLineEdit::editingFinished, this, [this]() {
        if (mUpdatingHex) return;
        QString text = mHexEdit->text().trimmed();
        QColor c = hexToColor(text, mCurrentColor.alpha());
        if (c.isValid()) {
            setColor(c);
            emit colorChanged(mCurrentColor);
            emit colorSelected(mCurrentColor);
        } else {
            // revert to current value if invalid
            mUpdatingHex = true;
            mHexEdit->setText(colorToHex(mCurrentColor));
            mUpdatingHex = false;
        }
    });
}

QColor ColorWheel::color()
{
    return mCurrentColor;
}

// Use this when originating color from UI (hue ring/triangle/square) to preserve current hue
void ColorWheel::setColor(QColor color)
{
    // this is a UI updating function, never emit any signals
    // and don't call any functions that will emit signals

    // Preserve hue when the change originates from S/V selection to avoid hue drift
    color = color.toHsv();

    if (color == mCurrentColor)
    {
        return;
    }

    if (color.hue() == -1) // grayscale color, keep the current hue
    {
        int preservedHue = mCurrentColor.hue();
        if (preservedHue == -1) {
            preservedHue = 0; // Default to red if no hue is set
        }
        color.setHsv(preservedHue, color.saturation(), color.value(), color.alpha());
    }

    mCurrentColor = color;

    if (mMode == Mode::SquareHSV)
    {
        drawSquareImage(color.hue());
    }
    else
    {
        drawTriangleImage(color.hue());
        // keep the triangle indicator in sync with the current color
        QPointF p = trianglePosFromColor(mCurrentColor);
        mTrianglePickPoint = p.toPoint();
        mHasTrianglePickPoint = true;
    }
    // keep hex field in sync for all paths (wheel, square, triangle, external)
    if (mHexEdit) {
        mUpdatingHex = true;
        mHexEdit->setText(colorToHex(mCurrentColor));
        mUpdatingHex = false;
    }
    update();
}

// Use this when color comes from external/global sources (sliders, popup close, hex): canonicalize
void ColorWheel::setColorCanonical(QColor color)
{
    color = color.toRgb();
    QColor canonicalRgb(color.red(), color.green(), color.blue(), color.alpha());
    QColor hsv = canonicalRgb.toHsv();
    
    // Safeguard: preserve hue even when saturation or value is 0
    if (hsv.hue() == -1 && mCurrentColor.hue() != -1) {
        hsv.setHsv(mCurrentColor.hue(), hsv.saturation(), hsv.value(), hsv.alpha());
    }
    
    if (hsv == mCurrentColor) return;
    mCurrentColor = hsv;
    if (mMode == Mode::SquareHSV)
        drawSquareImage(mCurrentColor.hue());
    else {
        drawTriangleImage(mCurrentColor.hue());
        QPointF p = trianglePosFromColor(mCurrentColor);
        mTrianglePickPoint = p.toPoint();
        mHasTrianglePickPoint = true;
    }
    if (mHexEdit) { mUpdatingHex = true; mHexEdit->setText(colorToHex(mCurrentColor)); mUpdatingHex = false; }
    update();
}

void ColorWheel::setMode(Mode mode)
{
    if (mMode == mode) return;
    mMode = mode;
    // reset stored pick point when switching modes
    mHasTrianglePickPoint = false;
    if (mMode == Mode::SquareHSV)
    {
        drawSquareImage(mCurrentColor.hue());
    }
    else
    {
        drawTriangleImage(mCurrentColor.hue());
        // sync indicator to current color
        QPointF p = trianglePosFromColor(mCurrentColor);
        mTrianglePickPoint = p.toPoint();
        mHasTrianglePickPoint = true;
    }
    // Persist selection
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("UI/ColorWheelMode", mMode == Mode::TriangleHSL ? 1 : 0);
    update();
}

void ColorWheel::setSlidersEnabled(bool enabled)
{
    if (mSlidersEnabled != enabled) {
        mSlidersEnabled = enabled;
        update();
    }
}

void ColorWheel::setSlidersToggleVisible(bool visible)
{
    if (mSlidersToggleVisible != visible) {
        mSlidersToggleVisible = visible;
        update();
    }
}

QColor ColorWheel::pickColor(const QPoint& point)
{
    if (!mWheelPixmap.rect().contains(point))
    {
        return QColor();
    }
    if (mIsInWheel)
    {
        qreal hue = 0;
        QPoint center(width() / 2, (height() - mBottomReserve) / 2);
        QPoint diff = point - center;

        hue = qAtan2(-diff.y(), diff.x()) / M_PI * 180;
        hue = fmod((hue + 360), 360); // shift -180~180 to 0~360

        //QString strDebug = "";
        //strDebug += QString("Radius=%1").arg(r);
        //strDebug += QString(" Atan2=%1").arg(qAtan2(diff.y(), diff.x()));
        //strDebug += QString(" Hue=%1").arg(hue);
        //qDebug() << strDebug;

        hue = (hue > 359) ? 359 : hue;
        hue = (hue < 0) ? 0 : hue;

        return QColor::fromHsvF(hue / 360.0,
                                mCurrentColor.hsvSaturationF(),
                                mCurrentColor.valueF());
    }
    else if (mIsInSquare)
    {
        QPointF p = point - mSquareRect.topLeft();
        //qDebug("TopRight(%d, %d) Point(%d, %d)", rect.topRight().x(), rect.topRight().y(), point.x(), point.y());

        //qDebug("p(%d, %d), Region(%.1f, %.1f)", p.x(), p.y(), regionSize.width(), regionSize.height());
        return QColor::fromHsvF(mCurrentColor.hueF(),
                                p.x() / (mSquareRect.width() - 1),
                                1.0 - (p.y() / (mSquareRect.height()-1)));
    }
    else if (mIsInTriangle)
    {
        return sampleTriangleColorAt(point).toHsv();
    }
    return QColor();
}

bool ColorWheel::isInHueRing(const QPoint& p) const
{
    // precise annulus hit test
    QPointF center(width() / 2.0, (height() - mBottomReserve) / 2.0);
    qreal dx = p.x() - center.x();
    qreal dy = p.y() - center.y();
    qreal dist = qSqrt(dx*dx + dy*dy);
    qreal outer = qMin(width(), height() - mBottomReserve) / 2.0; // outer radius
    qreal inner = outer - mWheelThickness;       // inner radius
    return dist <= outer && dist >= inner;
}

void ColorWheel::mousePressEvent(QMouseEvent *event)
{
    QPoint lastPos = event->pos();

    // Toggle button in bottom-right
    if (mToggleRect.contains(lastPos))
    {
        setMode(mMode == Mode::SquareHSV ? Mode::TriangleHSL : Mode::SquareHSV);
        return;
    }

    // Sliders toggle button (to the left of mode toggle) - only if visible
    if (mSlidersToggleVisible && mSlidersToggleRect.contains(lastPos))
    {
        mSlidersEnabled = !mSlidersEnabled;
        update();
        emit slidersToggled(mSlidersEnabled);
        return;
    }

    // Always allow hue ring interaction regardless of mode
    if (isInHueRing(lastPos))
    {
        mIsInWheel = true;
        mIsInSquare = false;
        mIsInTriangle = false;
        grabMouse();
        QColor color = pickColor(lastPos);
        hueChangedPrecise(color.hueF());
        return;
    }

    if (mMode == Mode::SquareHSV && mSquareRect.contains(lastPos))
    {
        mIsInWheel = false;
        mIsInSquare = true;
        mIsInTriangle = false;
        grabMouse();
        QColor color = pickColor(lastPos);
        saturationChanged(color.saturation());
        valueChanged(color.value());

    }
    else if (mMode == Mode::TriangleHSL)
    {
        qreal snap = mVertexSnapDistance * devicePixelRatioF();
        bool inside = pointInTriangle(lastPos, mTrianglePoly);
        bool nearVertex = isNearVertex(lastPos, mTriWhite, snap)
                       || isNearVertex(lastPos, mTriPure,  snap)
                       || isNearVertex(lastPos, mTriBlack, snap);
        if (inside || nearVertex)
        {
            mIsInWheel = false;
            mIsInSquare = false;
            mIsInTriangle = true;
            grabMouse();
            // Snap initial press to nearest vertex if within threshold for precise corners
            QPointF snapped = snapToTriangleVertex(lastPos, snap);
            QPoint usePoint = (!qFuzzyCompare(snapped.x(), lastPos.x()) || !qFuzzyCompare(snapped.y(), lastPos.y()))
                ? snapped.toPoint() : lastPos;
            // Unify path with move/release by using applyTrianglePick
            applyTrianglePick(usePoint, true);
        }
    }
}

void ColorWheel::mouseMoveEvent(QMouseEvent* event)
{
    QPoint lastPos = event->pos();
    if (event->buttons() == Qt::NoButton)
    {
        return;
    }
    if (mIsInSquare)
    {
        if (lastPos.x() < mSquareRect.topLeft().x())
        {
            lastPos.setX(mSquareRect.topLeft().x());
        }
        else if (lastPos.x() > mSquareRect.bottomRight().x())
        {
            lastPos.setX(mSquareRect.bottomRight().x());
        }

        if (lastPos.y() < mSquareRect.topLeft().y())
        {
            lastPos.setY(mSquareRect.topLeft().y());
        }
        else if (lastPos.y() > mSquareRect.bottomRight().y())
        {
            lastPos.setY(mSquareRect.bottomRight().y());
        }

        QColor color = pickColor(lastPos);
        saturationChanged(color.saturation());
        valueChanged(color.value());
    }
    else if (mIsInTriangle)
    {
        // Clamp to triangle boundary with vertex snapping for precise corners
        QPointF p = pointInTriangle(lastPos, mTrianglePoly) ? lastPos : clampPointToTriangle(lastPos);
        // Additional snap even when inside triangle
        QPointF s = snapToTriangleVertex(p, mVertexSnapDistance * devicePixelRatioF());
        if (!qFuzzyCompare(s.x(), p.x()) || !qFuzzyCompare(s.y(), p.y())) {
            p = s;
        }
        applyTrianglePick(p.toPoint(), true);
    }
    else if (mIsInWheel)
    {
        // Allow drag anywhere: compute hue by angle relative to center
        QPoint center(width() / 2, (height() - mBottomReserve) / 2);
        QPoint diff = lastPos - center;
        qreal hue = qAtan2(-diff.y(), diff.x()) / M_PI * 180;
        hue = fmod((hue + 360), 360);
        // Use precise hue value instead of converting to int
        QColor color = QColor::fromHsvF(hue / 360.0, mCurrentColor.hsvSaturationF(), mCurrentColor.valueF());
        hueChangedPrecise(color.hueF());
    }
}

void ColorWheel::mouseReleaseEvent(QMouseEvent *)
{
    mIsInWheel = false;
    mIsInSquare = false;
    mIsInTriangle = false;
    if (hasMouseTracking()) {}
    releaseMouse();
    if (mMode == Mode::TriangleHSL) {
        // If we have a stored pick point, commit from it; otherwise sample under cursor center of triangle
        QPoint p = mHasTrianglePickPoint ? mTrianglePickPoint : trianglePosFromColor(mCurrentColor).toPoint();
        applyTrianglePick(p, true);
        emit colorSelected(mCurrentColor);
    } else {
        emit colorChanged(mCurrentColor);
        emit colorSelected(mCurrentColor);
    }
}

void ColorWheel::applyTrianglePick(const QPoint& p, bool emitSignal)
{
    QColor color = sampleTriangleColorAt(p);
    if (!color.isValid()) return;
    mTrianglePickPoint = p;
    mHasTrianglePickPoint = true;
    // Preserve exact current hue; only update S/V to avoid hue drift due to rounding
    const int hue = mCurrentColor.hsvHue();
    const int alpha = mCurrentColor.alpha();
    const int sat = color.hsvSaturation();
    const int val = color.value();
    mCurrentColor.setHsv(hue, sat, val, alpha);
    if (mMode == Mode::SquareHSV) {
        drawSquareImage(hue);
    }
    // Sync hex
    if (mHexEdit) {
        mUpdatingHex = true;
        mHexEdit->setText(colorToHex(mCurrentColor));
        mUpdatingHex = false;
    }
    update();
    if (emitSignal) {
        emit colorChanged(mCurrentColor);
    }
}

void ColorWheel::resizeEvent(QResizeEvent* event)
{
    mWheelPixmap = QPixmap(event->size());
    mWheelPixmap.fill(palette().window().color());
    drawWheelImage(event->size());
    if (mMode == Mode::SquareHSV) {
        drawSquareImage(mCurrentColor.hue());
    } else {
        drawTriangleImage(mCurrentColor.hue());
        // Ensure the triangle selector starts at the current color position
        QPointF p = trianglePosFromColor(mCurrentColor);
        mTrianglePickPoint = p.toPoint();
        mHasTrianglePickPoint = true;
    }

    // keep hex field in sync
    if (mHexEdit) {
        mUpdatingHex = true;
        mHexEdit->setText(colorToHex(mCurrentColor));
        mUpdatingHex = false;
    }
    update();
}

void ColorWheel::paintEvent(QPaintEvent*)
{
    QPainter painter;

    painter.begin(this);
    QStyleOption opt;
    opt.initFrom(this);

    composeWheel(mWheelPixmap);
    painter.translate(width() / 2, height() / 2);
    painter.translate(-mWheelPixmap.width() / 2, -mWheelPixmap.height() / 2);
    painter.drawPixmap(0, 0, mWheelPixmap);

    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void ColorWheel::drawWheelImage(const QSize &newSize)
{
    // Reserve space at bottom for controls
    const int controlSize = 24;
    const int margin = 6;
    mBottomReserve = controlSize + margin * 2;
    int r = qMin(newSize.width(), newSize.height() - mBottomReserve);

    QStyleOption option;
    option.initFrom(this);

    QBrush backgroundBrush = option.palette.window();

    mWheelImage = QImage(newSize, QImage::Format_ARGB32_Premultiplied);
    const int ss = 2;
    QSize hiSize(newSize.width() * ss, newSize.height() * ss);
    QImage hiImage(hiSize, QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&hiImage);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(hiImage.rect(), backgroundBrush);
    painter.scale(ss, ss);

    QConicalGradient conicalGradient(0, 0, 0);

    for (int hue = 0; hue < 360; hue += 1)
    {
        conicalGradient.setColorAt(hue / 360.0, QColor::fromHsv(hue, 255, 255));
    }

    qreal ir = r - mWheelThickness;

    /* outer circle */
    painter.translate(width() / 2, (height() - mBottomReserve) / 2);

    QBrush brush(conicalGradient);
    painter.setPen(Qt::NoPen);
    painter.setBrush(brush);
    painter.drawEllipse(QPoint(0, 0), r / 2, r / 2);

    /* inner circle */
    painter.setBrush(backgroundBrush);
    painter.drawEllipse(QPoint(0, 0), r / 2 - mWheelThickness, r / 2 - mWheelThickness);

    // Center of wheel
    qreal m1 = (mWheelPixmap.width() / 2) - (ir / qSqrt(2));
    qreal m2 = ((mWheelPixmap.height() - mBottomReserve) / 2) - (ir / qSqrt(2));

    // Calculate size of wheel width
    qreal wheelWidth = 2 * ir / qSqrt(2);

    // Calculate wheel region
    mWheelRect = QRectF(m1, m2, wheelWidth, wheelWidth).toAlignedRect();

    // Compute toggle and swatch rects
    // Sliders toggle is on the far right
    mSlidersToggleRect = QRect(width() - controlSize - margin, height() - controlSize - margin, controlSize, controlSize);
    // Mode toggle position depends on whether sliders toggle is visible
    const int toggleSpacing = 4;
    if (mSlidersToggleVisible) {
        // Mode toggle is to the left of sliders toggle with spacing
        mToggleRect = QRect(width() - 2*controlSize - margin - toggleSpacing, height() - controlSize - margin, controlSize, controlSize);
    } else {
        // Mode toggle is on the far right (where sliders toggle would be)
        mToggleRect = QRect(width() - controlSize - margin, height() - controlSize - margin, controlSize, controlSize);
    }
    // Swatch on the left
    mSwatchRect = QRect(margin, height() - controlSize - margin, controlSize, controlSize);
    // Hex field positioned between swatch and mode toggle with proper spacing
    const int hexMargin = 4;
    int availableSpace = mToggleRect.left() - mSwatchRect.right() - 2*hexMargin;
    int hexWidth = availableSpace; // Use all available space
    int hexX = mSwatchRect.right() + hexMargin;
    int hexY = height() - controlSize - margin + (controlSize - mHexEdit->height())/2;
    mHexRect = QRect(hexX, hexY, hexWidth, mHexEdit->height());
    if (mHexEdit) mHexEdit->setGeometry(mHexRect);

    // Precompute icon pixmaps for bottom controls based on current geometry
    if (mSlidersToggleVisible) {
        QIcon slidersIcon(QString::fromUtf8(":/Interaction/rgbSliders.svg"));
        mSlidersTogglePixmap = slidersIcon.pixmap(mSlidersToggleRect.size());
    } else {
        mSlidersTogglePixmap = QPixmap();
    }

    QIcon modeTriIcon(QString::fromUtf8(":/Interaction/SwitchTri.svg"));
    QIcon modeSquareIcon(QString::fromUtf8(":/Interaction/SwitchSquare.svg"));
    mModeTogglePixmapTriangle = modeTriIcon.pixmap(mToggleRect.size());
    mModeTogglePixmapSquare = modeSquareIcon.pixmap(mToggleRect.size());

    // Triangle geometry is computed in drawTriangleImage(); nothing to do here
    mWheelImage = hiImage.scaled(newSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void ColorWheel::drawSquareImage(const int &hue)
{
    // region of the widget
    int w = qMin(width(), height() - mBottomReserve);
    // radius of outer circle
    qreal r = w / 2.0;
    // radius of inner circle
    qreal ir = r - mWheelThickness;

    // top left of square
    qreal m1 = (width() / 2.0) - (ir / qSqrt(2.1));
    qreal m2 = ((height() - mBottomReserve) / 2.0) - (ir / qSqrt(2.1));

    qreal squareWidthF = 2 * ir / qSqrt(2.1);
    int squareSide = qMax(1, qRound(squareWidthF));
    mSquareRect = QRectF(m1, m2, squareSide, squareSide).toAlignedRect();

    const int clampedHue = qBound(0, hue, 359);
    const QSize targetSize = mSquareRect.size();
    if (mLastHueForSquare == clampedHue && mLastSquareSize == targetSize && !mSquareImage.isNull())
    {
        return;
    }
    mLastHueForSquare = clampedHue;
    mLastSquareSize = targetSize;

    const int ss = 2;
    QSize hiSize(targetSize.width() * ss, targetSize.height() * ss);
    QImage hiImage(hiSize, QImage::Format_ARGB32_Premultiplied);

    QLinearGradient colorGradient = QLinearGradient(0, 0, hiImage.width(), 0);
    colorGradient.setColorAt(0, QColor(255,255,255));

    // color square should always use full value and saturation
    colorGradient.setColorAt(1, QColor::fromHsv(hue, 255, 255));

    QLinearGradient blackGradient = QLinearGradient(0, 0, 0, hiImage.height());
    blackGradient.setColorAt(0, QColor(0,0,0,0));
    blackGradient.setColorAt(1, QColor(0,0,0,255));

    QBrush colorGradiantBrush = QBrush(colorGradient);
    QBrush blackGradiantBrush = QBrush(blackGradient);

    QPainter painter(&hiImage);

    painter.fillRect(hiImage.rect(), colorGradiantBrush);
    painter.fillRect(hiImage.rect(), blackGradiantBrush);

    mSquareImage = hiImage.scaled(mSquareRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

}

QPolygonF ColorWheel::makeEquilateralTriangle(const QPointF& center, qreal radius)
{
    QPolygonF poly;
    // point to the right (apex)
    QPointF p1(center.x() + radius, center.y());
    // top-left
    QPointF p2(center.x() - radius * 0.5, center.y() - radius * qSqrt(3) / 2.0);
    // bottom-left
    QPointF p3(center.x() - radius * 0.5, center.y() + radius * qSqrt(3) / 2.0);
    poly << p1 << p2 << p3;
    return poly;
}

bool ColorWheel::pointInTriangle(const QPointF& p, const QPolygonF& tri)
{
    if (tri.size() != 3)
    {
        return false;
    }

    const QPointF a = tri.at(0);
    const QPointF b = tri.at(1);
    const QPointF c = tri.at(2);

    const QPointF v0 = b - a;
    const QPointF v1 = c - a;
    const QPointF v2 = p - a;

    const qreal dot00 = QPointF::dotProduct(v0, v0);
    const qreal dot01 = QPointF::dotProduct(v0, v1);
    const qreal dot02 = QPointF::dotProduct(v0, v2);
    const qreal dot11 = QPointF::dotProduct(v1, v1);
    const qreal dot12 = QPointF::dotProduct(v1, v2);

    const qreal denom = (dot00 * dot11 - dot01 * dot01);
    if (qFuzzyIsNull(denom))
    {
        return false;
    }

    const qreal invDen = 1.0 / denom;
    const qreal u = (dot11 * dot02 - dot01 * dot12) * invDen;
    const qreal v = (dot00 * dot12 - dot01 * dot02) * invDen;

    const qreal eps = 1e-6;
    return u >= -eps && v >= -eps && (u + v) <= 1.0 + eps;
}

void ColorWheel::drawTriangleImage(const int &hue)
{
    // Compute triangle inside the inner circle similar to square sizing
    int w = qMin(width(), height() - mBottomReserve);
    qreal r = w / 2.0;                 // outer radius
    qreal ir = r - mWheelThickness;    // inner radius (distance from center to inner rim)

    // Place triangle vertices exactly on the inner rim of the hue ring
    QPointF center(width() / 2.0, (height() - mBottomReserve) / 2.0);
    // Right (pure hue) at angle 0° on inner rim
    mTriPure = QPointF(center.x() + ir, center.y());
    // Top-left (white) at angle 120° on inner rim
    qreal angWhite = qDegreesToRadians(120.0);
    mTriWhite = QPointF(center.x() + ir * qCos(angWhite), center.y() - ir * qSin(angWhite));
    // Bottom-left (black) at angle 240° on inner rim
    qreal angBlack = qDegreesToRadians(240.0);
    mTriBlack = QPointF(center.x() + ir * qCos(angBlack), center.y() - ir * qSin(angBlack));

    QPolygonF tri;
    tri << mTriWhite << mTriPure << mTriBlack;
    mTrianglePoly = tri;
    mTriangleRect = tri.boundingRect().toAlignedRect();

    // Supersampling factor kept at 1 to minimize per-pixel work while
    // still relying on smooth scaling for crisp output
    const int ss = 1;
    const QSize targetSize = mTriangleRect.size();
    const int clampedHue = qBound(0, hue, 359);
    if (mLastHueForTriangle == clampedHue && mLastTriangleSize == targetSize && !mTriangleImage.isNull())
    {
        return;
    }
    mLastHueForTriangle = clampedHue;
    mLastTriangleSize = targetSize;

    QSize hiSize(targetSize.width() * ss, targetSize.height() * ss);

    QImage hiImage(hiSize, QImage::Format_ARGB32_Premultiplied);
    hiImage.fill(Qt::transparent);

    // Build an anti-aliased alpha mask using vector triangle geometry so edges are smooth
    QImage mask(hiSize, QImage::Format_Alpha8);
    mask.fill(0);
    {
        QPainter maskPainter(&mask);
        maskPainter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath triPath;
        QPointF offset = -mTriangleRect.topLeft();
        // Scale coordinates into supersampled space
        triPath.moveTo((mTriWhite + offset) * ss);
        triPath.lineTo((mTriPure  + offset) * ss);
        triPath.lineTo((mTriBlack + offset) * ss);
        triPath.closeSubpath();
        maskPainter.fillPath(triPath, Qt::white);
    }

    // Precompute barycentric denominator for HSV mapping (same as sampleTriangleColorAt)
    QPointF W = mTriWhite;
    QPointF C = mTriPure;
    QPointF B = mTriBlack;
    QPointF v0 = B - W;
    QPointF v1 = C - W;
    qreal dot00 = QPointF::dotProduct(v0, v0);
    qreal dot01 = QPointF::dotProduct(v0, v1);
    qreal dot11 = QPointF::dotProduct(v1, v1);
    qreal denom = (dot00 * dot11 - dot01 * dot01);
    if (qFuzzyIsNull(denom)) {
        return;
    }
    qreal invDen = 1.0 / denom;

    const qreal eps = 1e-6;

    for (int y = 0; y < hiImage.height(); ++y) {
        const uchar* maskLine = mask.constScanLine(y);
        for (int x = 0; x < hiImage.width(); ++x) {
            uchar alpha = maskLine[x];
            if (alpha == 0) {
                continue; // fully outside triangle
            }

            // Map supersampled pixel back to widget space
            QPointF p(mTriangleRect.left() + (x + 0.5) / ss,
                      mTriangleRect.top() + (y + 0.5) / ss);
            QPointF v2 = p - W;
            qreal dot02 = QPointF::dotProduct(v0, v2);
            qreal dot12 = QPointF::dotProduct(v1, v2);
            qreal u = (dot11 * dot02 - dot01 * dot12) * invDen; // along W->B
            qreal v = (dot00 * dot12 - dot01 * dot02) * invDen; // along W->C

            if (u < -eps || v < -eps || (u + v) > 1.0 + eps) {
                continue;
            }

            // HSV mapping from barycentric per reference: V = 1 - u; S = v / (1 - u)
            qreal V = 1.0 - u;
            qreal S = (V > 1e-6) ? (v / V) : 0.0;
            S = qBound(0.0, S, 1.0);
            V = qBound(0.0, V, 1.0);

            QColor hsv = QColor::fromHsvF(hue / 360.0, S, V, 1.0);
            hsv.setAlpha(alpha);
            hiImage.setPixelColor(x, y, hsv);
        }
    }

    // Downscale to final size with smooth filtering for very crisp edges
    mTriangleImage = hiImage.scaled(mTriangleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QPointF ColorWheel::trianglePosFromColor(const QColor &color) const
{
    // Convert HSV to barycentric weights then map to triangle vertices
    qreal S = color.hsvSaturationF();
    qreal V = color.valueF();
    // Inverse of mapping: from HSV to barycentric
    qreal b = 1.0 - V;      // black component
    qreal c = S * V;        // pure hue component
    qreal w = 1.0 - b - c;  // white component
    w = qBound(0.0, w, 1.0); c = qBound(0.0, c, 1.0); b = qBound(0.0, b, 1.0);
    return mTriWhite * w + mTriPure * c + mTriBlack * b;
}

QColor ColorWheel::sampleTriangleColorAt(const QPoint& p) const
{
    if (mTrianglePoly.size() != 3) return QColor();
    // Special-case: if point is at a vertex, return the exact vertex color using device-independent snap
    const qreal vertexTolerance = mVertexSnapDistance * devicePixelRatioF();
    QPointF pf = p;
    QPointF snapped = snapToTriangleVertex(pf, vertexTolerance);
    if (!qFuzzyCompare(snapped.x(), pf.x()) || !qFuzzyCompare(snapped.y(), pf.y())) {
        if (snapped == mTriWhite) {
            return QColor::fromHsvF(mCurrentColor.hueF(), 0.0, 1.0, mCurrentColor.alphaF());
        }
        if (snapped == mTriBlack) {
            return QColor::fromHsvF(mCurrentColor.hueF(), 0.0, 0.0, mCurrentColor.alphaF());
        }
        if (snapped == mTriPure) {
            return QColor::fromHsvF(mCurrentColor.hueF(), 1.0, 1.0, mCurrentColor.alphaF());
        }
    }
    // Barycentric with vertices W, C, B
    QPointF W = mTriWhite;
    QPointF C = mTriPure;
    QPointF B = mTriBlack;

    // Convert to barycentric (u,v) relative to W
    QPointF v0 = B - W;
    QPointF v1 = C - W;
    QPointF v2 = p - W;
    qreal dot00 = QPointF::dotProduct(v0, v0);
    qreal dot01 = QPointF::dotProduct(v0, v1);
    qreal dot02 = QPointF::dotProduct(v0, v2);
    qreal dot11 = QPointF::dotProduct(v1, v1);
    qreal dot12 = QPointF::dotProduct(v1, v2);
    qreal denom = (dot00 * dot11 - dot01 * dot01);
    if (qFuzzyIsNull(denom)) return QColor();
    qreal invDen = 1.0 / denom;
    qreal u = (dot11 * dot02 - dot01 * dot12) * invDen; // along W->B
    qreal v = (dot00 * dot12 - dot01 * dot02) * invDen; // along W->C
    // Allow points on edges with small epsilon to avoid false negatives
    const qreal eps = 1e-6;
    if (u < -eps || v < -eps || (u + v) > 1.0 + eps) return QColor();

    // HSV mapping from barycentric per reference: V = 1 - u; S = v / (1 - u)
    qreal V = 1.0 - u;
    qreal S = (V > 1e-6) ? (v / V) : 0.0;
    S = qBound(0.0, S, 1.0);
    V = qBound(0.0, V, 1.0);

    QColor hsv = QColor::fromHsvF(mCurrentColor.hueF(), S, V, mCurrentColor.alphaF());
    return hsv;
}

// Clamp a point to the closest point inside the triangle area
QPointF ColorWheel::clampPointToTriangle(const QPointF& p) const
{
    // First, try snapping to a triangle vertex for precise selection
    QPointF snapped = snapToTriangleVertex(p, mVertexSnapDistance * devicePixelRatioF());
    if (!qFuzzyCompare(snapped.x(), p.x()) || !qFuzzyCompare(snapped.y(), p.y())) {
        return snapped;
    }
    if (pointInTriangle(p, mTrianglePoly)) return p;
    // Project onto each edge and choose the closest
    auto project = [](const QPointF& a, const QPointF& b, const QPointF& p){
        QPointF ab = b - a;
        double t = QPointF::dotProduct(p - a, ab) / QPointF::dotProduct(ab, ab);
        t = qBound(0.0, t, 1.0);
        return a + ab * t;
    };
    QPointF e1 = project(mTriWhite, mTriPure, p);
    QPointF e2 = project(mTriPure, mTriBlack, p);
    QPointF e3 = project(mTriBlack, mTriWhite, p);
    auto dist2 = [](const QPointF& a, const QPointF& b){ QPointF d = a - b; return d.x()*d.x() + d.y()*d.y(); };
    QPointF best = e1;
    double dBest = dist2(best, p);
    double d2 = dist2(e2, p); if (d2 < dBest) { best = e2; dBest = d2; }
    d2 = dist2(e3, p); if (d2 < dBest) { best = e3; }
    return best;
}

// Detect if point is within snap distance of any triangle vertex and return that vertex
QPointF ColorWheel::snapToTriangleVertex(const QPointF& p, qreal snapDistance) const
{
    // Check all three vertices
    const QPointF vertices[3] = { mTriWhite, mTriPure, mTriBlack };
    qreal bestDist = std::numeric_limits<qreal>::max();
    QPointF bestVertex = p;
    for (const QPointF& v : vertices) {
        qreal d = distanceToPoint(p, v);
        if (d < bestDist) {
            bestDist = d;
            bestVertex = v;
        }
    }
    if (bestDist <= snapDistance) {
        return bestVertex;
    }
    return p;
}

qreal ColorWheel::distanceToPoint(const QPointF& a, const QPointF& b) const
{
    qreal dx = a.x() - b.x();
    qreal dy = a.y() - b.y();
    return qSqrt(dx*dx + dy*dy);
}

bool ColorWheel::isNearVertex(const QPointF& p, const QPointF& vertex, qreal threshold) const
{
    return distanceToPoint(p, vertex) <= threshold;
}

// computeTriangleGeometry removed: triangle geometry now derived solely in drawTriangleImage()

void ColorWheel::drawHueIndicator(const int &hue)
{
    QPainter painter(&mWheelPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    if (hue > 20 && hue < 200)
    {
        // Prefer palette text color for dark themes when available
        QColor penColor = palette().color(QPalette::WindowText);
        if (penColor.lightness() > 160) penColor = Qt::black; // ensure contrast on light segments
        painter.setPen(penColor);
    }
    else
    {
        QColor penColor = palette().color(QPalette::WindowText);
        if (penColor.lightness() < 96) penColor = Qt::white; // ensure contrast on dark segments
        painter.setPen(penColor);
    }
    painter.setBrush(Qt::NoBrush);

    QPen pen = painter.pen();
    pen.setWidth(3);
    painter.setPen(pen);
    qreal r = qMin(width(), height() - mBottomReserve);
    painter.translate(width() / 2, (height() - mBottomReserve) / 2);
    painter.rotate(-hue);

    r = r / 2.0 - mWheelThickness / 2;
    painter.drawEllipse(QPointF(r, 0), 7, 7);
}

void ColorWheel::drawPicker(const QColor& color)
{
    QPainter painter(&mWheelPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    int ellipseSize = 9;

    if (mMode == Mode::SquareHSV)
    {
        QPoint squareTopLeft = mSquareRect.topLeft();
        QSize squareSize = mSquareRect.size();
        qreal S = color.hsvSaturationF() * (squareSize.width()-1);
        qreal V = (squareSize.height() - (color.valueF() * squareSize.height()-1));
        QPen pen;
        pen.setWidth(1);
        if (color.hsvSaturation() > 30 || color.value() < 50)
        {
            pen.setColor(Qt::white);
        }
        painter.setPen(pen);
        QTransform transform;
        transform.translate(-ellipseSize/2,-ellipseSize/2);
        transform.translate(squareTopLeft.x(),squareTopLeft.y()-1);
        painter.setTransform(transform);
        painter.drawEllipse(static_cast<int>(S), static_cast<int>(V), ellipseSize, ellipseSize);
    }
    else
    {
        // Triangle picker: draw exactly at the last picked point; if none, compute from current color
        QPoint found = mHasTrianglePickPoint ? mTrianglePickPoint : trianglePosFromColor(color).toPoint();
        QPen pen;
        pen.setWidth(1);
        QColor c = (color.lightness() < 100) ? Qt::white : Qt::black;
        pen.setColor(c);
        painter.setPen(pen);
        painter.drawEllipse(found, ellipseSize/2, ellipseSize/2);
    }
}

void ColorWheel::composeWheel(QPixmap& pixmap)
{
    QPainter composePainter(&pixmap);
    composePainter.drawImage(0, 0, mWheelImage);
    // Clip any residual hue strip bleed by repainting the inner disk with background,
    // then draw crisp vector outlines on top.
    composePainter.setRenderHint(QPainter::Antialiasing, true);
    int w = qMin(width(), height() - mBottomReserve);
    qreal outerRad = w / 2.0;
    qreal ringInnerRad = outerRad - mWheelThickness;
    QPointF center(width() / 2.0, (height() - mBottomReserve) / 2.0);

    // Repaint inner disk with background so the hue strip cannot overflow inward
    QBrush bgBrush = palette().window();
    composePainter.setPen(Qt::NoPen);
    composePainter.setBrush(bgBrush);
    composePainter.drawEllipse(center, ringInnerRad, ringInnerRad);

    // Now draw subtle vector outlines for the hue ring (outer and inner edges)
    QPen wheelPen(QColor(0, 0, 0, 180));
    wheelPen.setWidthF(1.0);
    composePainter.setPen(wheelPen);
    composePainter.setBrush(Qt::NoBrush);
    composePainter.drawEllipse(center, outerRad, outerRad);
    // Slightly offset inner outline outward to hide any AA seam
    composePainter.drawEllipse(center, ringInnerRad + 0.5, ringInnerRad + 0.5);
    if (mMode == Mode::SquareHSV)
    {
        composePainter.drawImage(mSquareRect, mSquareImage);
        composePainter.setRenderHint(QPainter::Antialiasing, true);
        QPen squarePen(QColor(0, 0, 0, 180));
        squarePen.setWidthF(1.0);
        composePainter.setPen(squarePen);
        composePainter.setBrush(Qt::NoBrush);
        QRectF outlineRect = mSquareRect.adjusted(0.5, 0.5, -0.5, -0.5);
        composePainter.drawRect(outlineRect);
    }
    else
    {
        composePainter.drawImage(mTriangleRect.topLeft(), mTriangleImage);
        // Draw a subtle triangle outline for a crisp, professional edge
        composePainter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath triPath;
        triPath.moveTo(mTriWhite);
        triPath.lineTo(mTriBlack);
        triPath.lineTo(mTriPure);
        triPath.closeSubpath();
        QPen triPen(QColor(0, 0, 0, 180));
        triPen.setWidthF(1.0);
        composePainter.setPen(triPen);
        composePainter.setBrush(Qt::NoBrush);
        composePainter.drawPath(triPath);
    }
    // Draw swatch bottom-left
    composePainter.setPen(Qt::black);
    composePainter.setBrush(mCurrentColor);
    composePainter.drawRect(mSwatchRect.adjusted(0,0,-1,-1));
    
    // Draw sliders toggle icon (to the left of mode toggle) - only if visible
    if (mSlidersToggleVisible) {
        // Highlight with accent color when enabled
        if (mSlidersEnabled) {
            QColor accentColor(91, 165, 255); // Blender-style blue accent
            composePainter.setPen(Qt::NoPen);
            composePainter.setBrush(accentColor);
            composePainter.drawRoundedRect(mSlidersToggleRect, 4, 4);
        }
        if (!mSlidersTogglePixmap.isNull()) {
            composePainter.drawPixmap(mSlidersToggleRect.topLeft(), mSlidersTogglePixmap);
        } else {
            QIcon slidersIcon(QString::fromUtf8(":/Interaction/rgbSliders.svg"));
            QPixmap slidersPm = slidersIcon.pixmap(mSlidersToggleRect.size());
            composePainter.drawPixmap(mSlidersToggleRect.topLeft(), slidersPm);
        }
    }
    
    // Draw mode toggle icon bottom-right
    const QPixmap* modePm = nullptr;
    if (mMode == Mode::SquareHSV) {
        modePm = &mModeTogglePixmapTriangle;
    } else {
        modePm = &mModeTogglePixmapSquare;
    }
    if (modePm && !modePm->isNull()) {
        composePainter.drawPixmap(mToggleRect.topLeft(), *modePm);
    } else {
        const char* iconPath = (mMode == Mode::SquareHSV)
            ? ":/Interaction/SwitchTri.svg"
            : ":/Interaction/SwitchSquare.svg";
        QIcon icon(QString::fromUtf8(iconPath));
        QPixmap pm = icon.pixmap(mToggleRect.size());
        composePainter.drawPixmap(mToggleRect.topLeft(), pm);
    }
    composePainter.end();
    drawHueIndicator(mCurrentColor.hsvHue());
    drawPicker(mCurrentColor);
}

void ColorWheel::hueChanged(const int &hue)
{
    if (hue < 0 || hue > 359)
    {
        return;
    }
    int s = mCurrentColor.hsvSaturation();
    int v = mCurrentColor.value();
    int a = mCurrentColor.alpha();

    mCurrentColor.setHsv(hue, s, v, a);

    if (!isVisible())
    {
        return;
    }

    if (mMode == Mode::SquareHSV) {
        drawSquareImage(hue);
    } else {
        drawTriangleImage(hue);
        // keep the pick point inside bounds if it was set
        QPointF p = trianglePosFromColor(mCurrentColor);
        mTrianglePickPoint = p.toPoint();
        mHasTrianglePickPoint = true;
    }

    if (mHexEdit) { mUpdatingHex = true; mHexEdit->setText(colorToHex(mCurrentColor)); mUpdatingHex = false; }
    update();
    emit colorChanged(mCurrentColor);
}

void ColorWheel::hueChangedPrecise(const qreal &hueF)
{
    if (hueF < 0.0 || hueF > 1.0)
    {
        return;
    }
    qreal s = mCurrentColor.hsvSaturationF();
    qreal v = mCurrentColor.valueF();
    qreal a = mCurrentColor.alphaF();

    mCurrentColor.setHsvF(hueF, s, v, a);

    if (!isVisible())
    {
        return;
    }

    if (mMode == Mode::SquareHSV) {
        drawSquareImage(static_cast<int>(hueF * 360));
    } else {
        drawTriangleImage(static_cast<int>(hueF * 360));
        // keep the pick point inside bounds if it was set
        QPointF p = trianglePosFromColor(mCurrentColor);
        mTrianglePickPoint = p.toPoint();
        mHasTrianglePickPoint = true;
    }

    if (mHexEdit) { mUpdatingHex = true; mHexEdit->setText(colorToHex(mCurrentColor)); mUpdatingHex = false; }
    update();
    emit colorChanged(mCurrentColor);
}

void ColorWheel::saturationChanged(const int &sat)
{
    int hue = mCurrentColor.hsvHue();
    int value = mCurrentColor.value();
    int alpha = mCurrentColor.alpha();

    // Safeguard: preserve hue even when saturation is 0
    if (hue == -1) {
        hue = 0; // Default to red if no hue is set
    }

    mCurrentColor.setHsv(hue, sat, value, alpha);

    if (mHexEdit) { mUpdatingHex = true; mHexEdit->setText(colorToHex(mCurrentColor)); mUpdatingHex = false; }
    update();
    emit colorChanged(mCurrentColor);
}

void ColorWheel::valueChanged(const int &value)
{
    int hue = mCurrentColor.hsvHue();
    int sat = mCurrentColor.hsvSaturation();
    int alpha = mCurrentColor.alpha();

    // Safeguard: preserve hue even when value is 0
    if (hue == -1) {
        hue = 0; // Default to red if no hue is set
    }

    mCurrentColor.setHsv(hue, sat, value, alpha);

    if (mHexEdit) { mUpdatingHex = true; mHexEdit->setText(colorToHex(mCurrentColor)); mUpdatingHex = false; }
    update();
    emit colorChanged(mCurrentColor);
}

QString ColorWheel::colorToHex(const QColor &color)
{
    QColor c = color.toRgb();
    QString hex = QString("#%1%2%3")
            .arg(c.red(),   2, 16, QLatin1Char('0'))
            .arg(c.green(), 2, 16, QLatin1Char('0'))
            .arg(c.blue(),  2, 16, QLatin1Char('0')).toUpper();
    return hex;
}

QColor ColorWheel::hexToColor(const QString &text, int fallbackAlpha)
{
    QString t = text.trimmed();
    if (!t.startsWith('#')) t.prepend('#');
    QRegularExpression re("^#([A-Fa-f0-9]{6})$");
    auto m = re.match(t);
    if (!m.hasMatch()) return QColor();
    QString rgb = m.captured(1);
    bool ok = false;
    int r = rgb.mid(0,2).toInt(&ok,16); if (!ok) return QColor();
    int g = rgb.mid(2,2).toInt(&ok,16); if (!ok) return QColor();
    int b = rgb.mid(4,2).toInt(&ok,16); if (!ok) return QColor();
    int alpha = fallbackAlpha;
    return QColor(r,g,b,alpha);
}
