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

#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include <QWidget>
#include <QString>
class QLineEdit;


class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(QWidget* parent);

    QColor color();

    enum class Mode {
        SquareHSV,
        TriangleHSL
    };

signals:
    void colorSelected(const QColor& color);
    void colorChanged(const QColor& color);
    void slidersToggled(bool enabled);
    
public slots:
    void setColor(QColor color);
    void setColorCanonical(QColor color);
    void setMode(Mode mode);
    Mode mode() const { return mMode; }
    void setSlidersEnabled(bool enabled);
    void setSlidersToggleVisible(bool visible);

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent(QPaintEvent*) override;

private:
    void hueChanged(const int& hue);
    void hueChangedPrecise(const qreal& hueF);
    void saturationChanged(const int& sat);
    void valueChanged(const int& value);

    QColor pickColor(const QPoint& point);
    
    void drawHueIndicator(const int& hue);
    void drawPicker(const QColor& color);

    void drawWheelImage(const QSize& newSize);
    void drawSquareImage(const int& hue);
    void drawTriangleImage(const int& hue);
    void composeWheel(QPixmap& pixmap);

    // Helpers for triangle rendering and interaction
    static QPolygonF makeEquilateralTriangle(const QPointF& center, qreal radius);
    static bool pointInTriangle(const QPointF& p, const QPolygonF& tri);
    QColor sampleTriangleColorAt(const QPoint& p) const;
    QPointF trianglePosFromColor(const QColor& color) const;
    bool isInHueRing(const QPoint& p) const;
    void applyTrianglePick(const QPoint& p, bool emitSignal);
    QPointF clampPointToTriangle(const QPointF& p) const;
    static QString colorToHex(const QColor& color);
    static QColor hexToColor(const QString& text, int fallbackAlpha);
	// Smart vertex snapping helpers
	QPointF snapToTriangleVertex(const QPointF& p, qreal snapDistance = 12.0) const;
	qreal distanceToPoint(const QPointF& a, const QPointF& b) const;
	bool isNearVertex(const QPointF& p, const QPointF& vertex, qreal threshold) const;

private:
    QSize mInitSize{ 20, 20 };
    QImage mWheelImage;
    QImage mSquareImage;
    QImage mTriangleImage;
    QPixmap mWheelPixmap;
    QPixmap mSlidersTogglePixmap;
    QPixmap mModeTogglePixmapSquare;
    QPixmap mModeTogglePixmapTriangle;
   
    int mWheelThickness = 26;
    QRect mWheelRect;
    QRect mSquareRect;
    QRect mTriangleRect;
    QPolygonF mTrianglePoly;
    QPointF mTriWhite; // top-left-ish (white)
    QPointF mTriPure;  // right (pure hue)
    QPointF mTriBlack; // bottom-left-ish (black)
    QRect mToggleRect;
    QRect mSlidersToggleRect;
    QRect mSwatchRect;
    QRect mHexRect;
    QLineEdit* mHexEdit = nullptr;
    QPointF mContentCenter{0,0};
    int mBottomReserve = 0;
    QPoint mTrianglePickPoint;
    bool mHasTrianglePickPoint = false;
	qreal mVertexSnapDistance = 1.5;
    int mLastHueForSquare = -1;
    int mLastHueForTriangle = -1;
    QSize mLastSquareSize;
    QSize mLastTriangleSize;
    QColor mCurrentColor = Qt::red;
    bool mIsInWheel = false;
    bool mIsInSquare = false;
    bool mIsInTriangle = false;
    Mode mMode = Mode::SquareHSV;
    bool mUpdatingHex = false;
    bool mSlidersEnabled = false;
    bool mSlidersToggleVisible = true;
    
};

#endif // COLORWHEEL_H
