#include "ignorefillsplittoggle.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QFontMetrics>
#include <QtMath>

#include "../CUI/blenderstyletoggle.h" // for getAccentColor()

IgnoreFillSplitToggle::IgnoreFillSplitToggle(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    initializeDefaultColors();
    // Expand horizontally like other Blender-style toggles, keep compact height
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(24);
}

void IgnoreFillSplitToggle::initializeDefaultColors()
{
    mAccentColor = BlenderStyleToggle::getAccentColor();
    mBackgroundColor = QColor(50, 50, 50);
    mTextColor = QColor(220, 220, 220);
    // selected text color computed in paint for contrast
    mSelectedTextColor = QColor(17, 17, 17);
}

void IgnoreFillSplitToggle::setChecked(bool checked)
{
    if (mChecked == checked) return;
    mChecked = checked;
    emit toggled(mChecked);
    update();
}

void IgnoreFillSplitToggle::setText(const QString& text)
{
    if (mText == text) return;
    mText = text;
    updateGeometry();
    update();
}

void IgnoreFillSplitToggle::setColor(const QColor& c)
{
    if (mColor == c) return;
    mColor = c;
    update();
}

void IgnoreFillSplitToggle::setAccentColor(const QColor& c)
{
    mAccentColor = c;
    update();
}

void IgnoreFillSplitToggle::setBackgroundColor(const QColor& c)
{
    mBackgroundColor = c;
    update();
}

void IgnoreFillSplitToggle::setTextColor(const QColor& c)
{
    mTextColor = c;
    update();
}

void IgnoreFillSplitToggle::setSelectedTextColor(const QColor& c)
{
    mSelectedTextColor = c;
    update();
}

void IgnoreFillSplitToggle::setCornerRadius(int r)
{
    mCornerRadius = r;
    update();
}

void IgnoreFillSplitToggle::setColorAreaRatio(qreal r)
{
    qreal clamped = qBound<qreal>(0.15, r, 0.5); // sensible bounds
    if (!qFuzzyCompare(clamped, mColorAreaRatio)) {
        mColorAreaRatio = clamped;
        update();
    }
}

QRect IgnoreFillSplitToggle::colorAreaRect() const
{
    const int w = width();
    const int h = height();
    const int leftW = qRound(w * mColorAreaRatio);
    return QRect(0, 0, leftW, h);
}

QSize IgnoreFillSplitToggle::sizeHint() const
{
    int h = 26;
    QFontMetrics fm(font());
    int textW = fm.horizontalAdvance(mText);
    int leftW = qMax(26, qRound((textW + 32) * mColorAreaRatio));
    int rightW = qMax(60, textW + 18); // padding for text
    return QSize(leftW + rightW, h);
}

QSize IgnoreFillSplitToggle::minimumSizeHint() const
{
    return QSize(120, 24);
}

void IgnoreFillSplitToggle::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect();

    // Background color
    QColor bg = mBackgroundColor;
    if (!mEnabled) {
        bg = mBackgroundColor.darker(120);
    } else if (mChecked) {
        bg = mAccentColor;
    } else if (mHovered) {
        bg = mBackgroundColor.lighter(115);
    }

    // Rounded background
    QPainterPath bgPath;
    bgPath.addRoundedRect(r, mCornerRadius, mCornerRadius);
    p.fillPath(bgPath, bg);

    // Border
    p.setPen(QPen(mBackgroundColor.darker(150), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bgPath);

    // Left color area
    QRect left = colorAreaRect();

    // Draw a subtle separator line between left and right
    p.setPen(QPen(QColor(0,0,0,90), 1));
    p.drawLine(left.right(), r.top()+2, left.right(), r.bottom()-2);

    // Color circle inside left area
    int margin = 5;
    int dia = qMin(left.width() - margin*2, height() - margin*2);
    if (dia > 0) {
        QRect circleRect(left.left() + (left.width()-dia)/2, r.top() + (r.height()-dia)/2, dia, dia);
        QPainterPath clip; clip.addEllipse(circleRect);
        // Border stroke
        p.setPen(QPen(QColor(220, 220, 220, 200), 2));
        p.setBrush(mColor);
        if (!mEnabled) {
            p.setOpacity(0.6);
        }
        p.drawEllipse(circleRect);
        p.fillPath(clip, mColor);
        p.setOpacity(1.0);
    }

    // Text on the right area
    QRect right(left.right()+1, r.top(), r.width() - left.width() - 1, r.height());
    QString t = mText;
    if (!t.isEmpty()) {
        QFontMetrics fm(font());
        QColor textCol = mTextColor;
        if (mChecked) {
            // contrast for accent background
            textCol = (mAccentColor.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);
        } else if (!mEnabled) {
            textCol = textCol.darker(160);
        }
        p.setPen(textCol);
        int textW = fm.horizontalAdvance(t);
        int x = right.left() + (right.width() - textW)/2;
        int y = right.top() + (right.height() + fm.ascent() - fm.descent())/2;
        p.drawText(x, y, t);
    }
}

void IgnoreFillSplitToggle::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && mEnabled) {
        mPressed = true;
        mPressPos = e->pos();
        mPressOnLeftArea = colorAreaRect().contains(mPressPos);
        update();
    }
    QWidget::mousePressEvent(e);
}

void IgnoreFillSplitToggle::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && mEnabled) {
        if (mPressed) {
            bool releasedOnLeft = colorAreaRect().contains(e->pos());
            if (mPressOnLeftArea && releasedOnLeft) {
                emit clicked();
                emit colorAreaClicked();
            } else if (!mPressOnLeftArea) {
                // Treat as toggle when released within widget
                if (rect().contains(e->pos())) {
                    setChecked(!mChecked);
                    emit clicked();
                }
            }
        }
        mPressed = false;
        update();
    }
    QWidget::mouseReleaseEvent(e);
}

void IgnoreFillSplitToggle::mouseMoveEvent(QMouseEvent* e)
{
    bool prevHovered = mHovered;
    mHovered = rect().contains(e->pos());
    if (prevHovered != mHovered) update();
    QWidget::mouseMoveEvent(e);
}

void IgnoreFillSplitToggle::leaveEvent(QEvent* e)
{
    mHovered = false;
    mPressed = false;
    update();
    QWidget::leaveEvent(e);
}

void IgnoreFillSplitToggle::setEnabled(bool enabled)
{
    if (mEnabled == enabled) return;
    mEnabled = enabled;
    QWidget::setEnabled(enabled);
    update();
}
