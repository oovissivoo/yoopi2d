#include "cosmiccolorwheelwidget.h"

#include "pencildef.h"

#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include <QtMath>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>

namespace {

struct SvTriangle
{
    QPointF white;
    QPointF black;
    QPointF hue;
};

static SvTriangle svTriangleForWheelInnerRim(const QRectF& wheelRect, int wheelThickness)
{
    // Exact legacy geometry (ColorWheel::drawTriangleImage):
    // Vertices lie on the INNER rim of the hue ring at angles 0/120/240 degrees.
    const QPointF center = wheelRect.center();
    const qreal outerR = qMin(wheelRect.width(), wheelRect.height()) * 0.5;
    // Match the SV-square padding so the triangle doesn't touch the hue ring.
    const qreal svPad = qMax<qreal>(4.0, wheelThickness * 0.6);
    const qreal innerR = qMax<qreal>(0.0, outerR - wheelThickness - svPad);

    SvTriangle t;
    t.hue = QPointF(center.x() + innerR, center.y());

    const qreal angWhite = qDegreesToRadians(120.0);
    t.white = QPointF(center.x() + innerR * qCos(angWhite), center.y() - innerR * qSin(angWhite));

    const qreal angBlack = qDegreesToRadians(240.0);
    t.black = QPointF(center.x() + innerR * qCos(angBlack), center.y() - innerR * qSin(angBlack));
    return t;
}

static QPointF closestPointOnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal ab2 = QPointF::dotProduct(ab, ab);
    if (ab2 <= 0.0) {
        return a;
    }
    const qreal t = qBound<qreal>(0.0, QPointF::dotProduct(p - a, ab) / ab2, 1.0);
    return a + ab * t;
}

static bool barycentricWeights(const QPointF& p,
                              const QPointF& a,
                              const QPointF& b,
                              const QPointF& c,
                              qreal& wa,
                              qreal& wb,
                              qreal& wc)
{
    // Compute barycentric coordinates (wa for a, wb for b, wc for c)
    const qreal denom = (b.y() - c.y()) * (a.x() - c.x()) + (c.x() - b.x()) * (a.y() - c.y());
    if (qFuzzyIsNull(denom)) {
        wa = wb = wc = 0.0;
        return false;
    }

    wa = ((b.y() - c.y()) * (p.x() - c.x()) + (c.x() - b.x()) * (p.y() - c.y())) / denom;
    wb = ((c.y() - a.y()) * (p.x() - c.x()) + (a.x() - c.x()) * (p.y() - c.y())) / denom;
    wc = 1.0 - wa - wb;
    return true;
}

static bool isInsideTriangleBary(qreal w0, qreal w1, qreal w2)
{
    // Looser epsilon keeps triangle edges/tips solid (avoids clipped-looking saturation vertex)
    const qreal eps = -1e-3;
    return w0 >= eps && w1 >= eps && w2 >= eps;
}

static QPointF clampPointToTriangle(const QPointF& p, const SvTriangle& t)
{
    qreal wW = 0.0, wB = 0.0, wH = 0.0;
    if (barycentricWeights(p, t.white, t.black, t.hue, wW, wB, wH) && isInsideTriangleBary(wW, wB, wH)) {
        return p;
    }

    const QPointF pWB = closestPointOnSegment(p, t.white, t.black);
    const QPointF pBH = closestPointOnSegment(p, t.black, t.hue);
    const QPointF pHW = closestPointOnSegment(p, t.hue, t.white);

    const qreal dWB = QPointF::dotProduct(p - pWB, p - pWB);
    const qreal dBH = QPointF::dotProduct(p - pBH, p - pBH);
    const qreal dHW = QPointF::dotProduct(p - pHW, p - pHW);

    if (dWB <= dBH && dWB <= dHW) return pWB;
    if (dBH <= dHW) return pBH;
    return pHW;
}

static bool legacyTriangleUV(const QPointF& p, const SvTriangle& t, qreal& uOut, qreal& vOut)
{
    // Legacy ColorWheel mapping uses vertices W(white), C(pure hue), B(black)
    const QPointF W = t.white;
    const QPointF C = t.hue;
    const QPointF B = t.black;

    const QPointF v0 = B - W;
    const QPointF v1 = C - W;
    const QPointF v2 = p - W;

    const qreal dot00 = QPointF::dotProduct(v0, v0);
    const qreal dot01 = QPointF::dotProduct(v0, v1);
    const qreal dot02 = QPointF::dotProduct(v0, v2);
    const qreal dot11 = QPointF::dotProduct(v1, v1);
    const qreal dot12 = QPointF::dotProduct(v1, v2);

    const qreal denom = (dot00 * dot11 - dot01 * dot01);
    if (qFuzzyIsNull(denom)) {
        uOut = vOut = 0.0;
        return false;
    }
    const qreal invDen = 1.0 / denom;

    // u along W->B, v along W->C
    const qreal u = (dot11 * dot02 - dot01 * dot12) * invDen;
    const qreal v = (dot00 * dot12 - dot01 * dot02) * invDen;

    uOut = u;
    vOut = v;
    return true;
}

class HexLineEdit final : public QLineEdit
{
public:
    explicit HexLineEdit(QWidget* parent = nullptr) : QLineEdit(parent)
    {
        setMaxLength(6);
        setAlignment(Qt::AlignCenter);
        setFrame(false);
        applyStyle();

        // Reserve space for the # prefix + padding so text doesn't overlap
        const QFontMetrics fm = fontMetrics();
        const int hashWidth = fm.horizontalAdvance(QStringLiteral("#"));
        const int padding = 2;
        const int leftMargin = hashWidth + padding;
        setTextMargins(leftMargin, 0, 0, 0);
    }

    void applyStyle()
    {
        setStyleSheet(CosmicInlineEdit::stylesheet());
    }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        QLineEdit::paintEvent(e);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        const QFontMetrics fm = fontMetrics();
        const QString hash = QStringLiteral("#");
        const int textWidth = fm.horizontalAdvance(text());
        const int totalWidth = fm.horizontalAdvance(hash) + 2 + textWidth;
        const int x = (width() - totalWidth) / 2;
        const int y = (height() + fm.ascent() - fm.descent()) / 2;

        p.setPen(palette().color(QPalette::Text));
        p.drawText(x, y, hash);
    }
};

class CosmicColorPreviewSquircle final : public QWidget
{
public:
    explicit CosmicColorPreviewSquircle(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(22, 22);
    }

    void setColor(const QColor& c)
    {
        if (c == mColor) {
            return;
        }
        mColor = c;
        update();
    }

    QColor color() const { return mColor; }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        Q_UNUSED(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = qMax<qreal>(3.0, qMin(r.width(), r.height()) * 0.32);

        QPainterPath path;
        path.addRoundedRect(r, radius, radius);

        p.setPen(Qt::NoPen);
        p.setBrush(mColor);
        p.drawPath(path);

        QPen outline(QColor(0, 0, 0, 140));
        outline.setWidthF(1.0);
        p.setPen(outline);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

private:
    QColor mColor;
};

}

CosmicColorWheelWidget::CosmicColorWheelWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(140, 140);
    mCurrent = mCurrent.toHsv();

    {
        QSettings settings(PENCIL2D, PENCIL2D);
        mTriangleMode = settings.value(QStringLiteral("CosmicColorWheel/UseTriangle"), false).toBool();
    }

    mFooter = new QWidget(this);
    mFooter->setAttribute(Qt::WA_NoSystemBackground, true);
    mFooter->setAutoFillBackground(false);

    mColorPreview = new CosmicColorPreviewSquircle(mFooter);
    static_cast<CosmicColorPreviewSquircle*>(mColorPreview)->setColor(mCurrent);

    mHexEdit = new HexLineEdit(mFooter);
    mHexEdit->setText(colorToHex(mCurrent).mid(1));

    // Initialize UI from single source of truth
    mUpdatingHex = true;
    mHexEdit->setText(colorToHex(mCurrent).mid(1));
    mUpdatingHex = false;
    static_cast<CosmicColorPreviewSquircle*>(mColorPreview)->setColor(mCurrent);

    mModeToggle = new CosmicCheckboxToggle(mFooter);
    mModeToggle->setObjectName(QStringLiteral("CosmicColorWheel_ModeToggle"));
    mModeToggle->setMode(CosmicCheckboxToggle::Custom);
    mModeToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mModeToggle->setBordered(false);
    mModeToggle->setShowText(false);
    mModeToggle->setShowIcon(true);
    mModeToggle->setBackgroundEnabled(false);
    mModeToggle->setHoverHighlightEnabled(true);
    mModeToggle->setIconSize(QSize(18, 18));
    mModeToggle->setCheckable(true);
    mModeToggle->setChecked(mTriangleMode);
    mModeToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(mModeToggle, &QAbstractButton::toggled, this, [this](bool on) {
        mTriangleMode = on;

        {
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue(QStringLiteral("CosmicColorWheel/UseTriangle"), mTriangleMode);
            settings.sync();
        }

        // Show icon for the *other* mode (what you'll switch to)
        const QString iconPath = on ? ":/Interaction/SwitchSquare.svg" : ":/Interaction/SwitchTri.svg";
        mModeToggle->setIcon(makeWhiteIcon(iconPath));
        mModeToggle->update();

        // Force SV texture regeneration for the new mode
        mSvHueCache = -1;
        mSvDprCache = 0.0;
        update();
    });

    // Set initial white icon (show the other mode)
    mModeToggle->setIcon(makeWhiteIcon(mTriangleMode ? QStringLiteral(":/Interaction/SwitchSquare.svg")
                                                     : QStringLiteral(":/Interaction/SwitchTri.svg")));

    connect(mHexEdit, &QLineEdit::textChanged, this, [this](const QString& raw) {
        if (mUpdatingHex) {
            return;
        }

        QString t = raw.trimmed();
        t.remove(QRegularExpression("[^A-Fa-f0-9]"));
        if (t.size() > 6) {
            t = t.left(6);
        }

        if (t != raw) {
            QSignalBlocker b(mHexEdit);
            mHexEdit->setText(t.toUpper());
            return;
        }

        if (t.size() != 6) {
            return;
        }

        const QColor c = hexToColor(t, mCurrent.alpha());
        if (!c.isValid()) {
            return;
        }

        setColor(c);
        emit colorChanged(mCurrent);
    });

    connect(mHexEdit, &QLineEdit::editingFinished, this, [this]() {
        if (mUpdatingHex) return;
        QString t = mHexEdit->text().trimmed();
        t.remove(QRegularExpression("[^A-Fa-f0-9]"));
        if (t.size() > 0 && t.size() < 6) {
            const QChar last = t.at(t.size() - 1);
            while (t.size() < 6) {
                t.append(last);
            }
            mUpdatingHex = true;
            mHexEdit->setText(t.toUpper());
            mUpdatingHex = false;
        }
        const QColor c = hexToColor(t, mCurrent.alpha());
        if (c.isValid()) {
            setColor(c);
            emit colorChanged(mCurrent);
        }
    });

    auto* footerLayout = new QHBoxLayout(mFooter);
    footerLayout->setContentsMargins(10, 0, 10, 0);
    footerLayout->setSpacing(8);
    footerLayout->addWidget(mColorPreview, 0, Qt::AlignVCenter);

    auto* hexHost = new QWidget(mFooter);
    auto* hexLayout = new QHBoxLayout(hexHost);
    hexLayout->setContentsMargins(0, 0, 0, 0);
    hexLayout->setSpacing(4);
    hexLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    hexLayout->addWidget(mHexEdit, 1);
    hexHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    footerLayout->addWidget(hexHost, 1, Qt::AlignVCenter);

    footerLayout->addWidget(mModeToggle, 0, Qt::AlignVCenter);
}

QColor CosmicColorWheelWidget::color() const
{
return mCurrent;
}

QIcon CosmicColorWheelWidget::makeWhiteIcon(const QString& path)
{
QIcon original(path);
QPixmap pixmap = original.pixmap(QSize(18, 18));
QPainter painter(&pixmap);
painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
painter.fillRect(pixmap.rect(), QColor(255, 255, 255));
return QIcon(pixmap);
}

void CosmicColorWheelWidget::setColor(const QColor& color)
{
    QColor hsv = color.toHsv();

    if (hsv.hue() == -1) {
        const int preserved = (mCurrent.hue() == -1) ? 0 : mCurrent.hue();
        hsv.setHsv(preserved, hsv.saturation(), hsv.value(), hsv.alpha());
    } else {
        // Only accept the new hue if it differs by more than 1 degree from current.
        // This prevents floating-point rounding in HSL→HSV conversion from jittering
        // the hue ring picker when only S/L/R/G/B channels are being changed.
        const int oldHue = (mCurrent.hue() < 0) ? 0 : mCurrent.hue();
        const int newHue = hsv.hue();
        const int diff = qAbs(newHue - oldHue);
        const int wrappedDiff = qMin(diff, 360 - diff);
        if (wrappedDiff <= 1 && hsv.saturation() > 0 && hsv.value() > 0) {
            // Snap to current hue to avoid jitter
            hsv.setHsv(oldHue, hsv.saturation(), hsv.value(), hsv.alpha());
        }
    }

    if (hsv == mCurrent) {
        return;
    }

    const int oldHue = mCurrent.hue();
    mCurrent = hsv;

    // Update all UI elements from single source of truth
    mUpdatingHex = true;
    mHexEdit->setText(colorToHex(mCurrent).mid(1));
    mUpdatingHex = false;

    static_cast<CosmicColorPreviewSquircle*>(mColorPreview)->setColor(mCurrent);

    if (oldHue != mCurrent.hue()) {
        mSvHueCache = -1;
        ensureSvImage();
    }

    update();
}

void CosmicColorWheelWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }

    const QPointF p = QPointF(e->pos());

    if (isInHueRing(p)) {
        mDragMode = DragMode::Hue;
        applyHueFromPoint(p, true);
        e->accept();
        return;
    }

    if (isInSvSquare(p)) {
        mDragMode = DragMode::Sv;
        applySvFromPoint(p, true);
        e->accept();
        return;
    }

    QWidget::mousePressEvent(e);
}

void CosmicColorWheelWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPointF p = QPointF(e->pos());

    if ((e->buttons() & Qt::LeftButton) == 0) {
        QWidget::mouseMoveEvent(e);
        return;
    }

    if (mDragMode == DragMode::Hue) {
        applyHueFromPoint(p, true);
        e->accept();
        return;
    }

    if (mDragMode == DragMode::Sv) {
        applySvFromPoint(p, true);
        e->accept();
        return;
    }

    QWidget::mouseMoveEvent(e);
}

void CosmicColorWheelWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && mDragMode != DragMode::None) {
        emit colorSelected(mCurrent);
        mDragMode = DragMode::None;
        e->accept();
        return;
    }

    QWidget::mouseReleaseEvent(e);
}

void CosmicColorWheelWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateLayoutRects();
    updateFooterGeometry();
    mSvHueCache = -1;
    mSvDprCache = 0.0;
    mHueRingThicknessCache = -1;
    update();
}

void CosmicColorWheelWidget::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    updateLayoutRects();
    ensureSvImage();
    ensureHueRingPixmap();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPointF center = mWheelRect.center();

    // Important: QPainter draws arcs on the centerline of the pen.
    // To keep the ring fully inside our rect and keep the strip centerline stable,
    // inset the arc rect by half the thickness.
    const qreal outerR = qMin(mWheelRect.width(), mWheelRect.height()) * 0.5;
    const qreal halfT = mWheelThickness * 0.5;
    const qreal centerlineR = qMax<qreal>(0.0, outerR - halfT);

    // Hue wheel (cached pixmap, regenerated only when size/thickness/DPR changes)
    if (!mHueRingPixmap.isNull()) {
        p.drawPixmap(mWheelRect.topLeft().toPoint(), mHueRingPixmap);
    }

    // SV square / triangle
    p.setPen(Qt::NoPen);
    if (!mSvPixmap.isNull()) {
        p.drawPixmap(mSvRect.topLeft().toPoint(), mSvPixmap);
    } else {
        p.drawImage(mSvRect.topLeft(), mSvImage);
    }

    // SV outline
    {
        p.setBrush(Qt::NoBrush);

        if (!mTriangleMode) {
            QPen outline(QColor(0, 0, 0, 180));
            outline.setWidthF(1.0);
            p.setPen(outline);
            p.drawRect(mSvRect);
        } else {
            const SvTriangle t = svTriangleForWheelInnerRim(mWheelRect, mWheelThickness);
            QPainterPath path;
            path.moveTo(t.white);
            path.lineTo(t.hue);
            path.lineTo(t.black);
            path.closeSubpath();

            // Legacy-style edge cleanup: first overdraw with background color to eliminate any fringe,
            // then draw the final crisp outline.
            {
                QPen cleanup(palette().color(QPalette::Window));
                cleanup.setWidthF(2.0);
                cleanup.setJoinStyle(Qt::MiterJoin);
                p.setPen(cleanup);
                p.drawPath(path);
            }
            {
                QPen outline(QColor(0, 0, 0, 180));
                outline.setWidthF(1.0);
                outline.setJoinStyle(Qt::MiterJoin);
                p.setPen(outline);
                p.drawPath(path);
            }
        }
    }

    // Hue selector circle inside the ring, with black+white outlines
    {
        int hue = mCurrent.hue();
        if (hue < 0) hue = 0;

        const qreal angleRad = qDegreesToRadians(static_cast<qreal>(hue));
        const qreal midR = centerlineR;
        const QPointF dir(qCos(angleRad), -qSin(angleRad));
        const QPointF pos = center + dir * midR;

        // Keep the picker fully inside the strip (avoid touching inner/outer edges).
        const qreal r = qMax<qreal>(5.0, mWheelThickness * 0.42);

        p.setBrush(QColor::fromHsv(hue, 255, 255));
        p.setPen(Qt::NoPen);
        p.drawEllipse(pos, r, r);

        QPen black(QColor(0, 0, 0, 230));
        black.setWidthF(2.0);
        p.setPen(black);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pos, r, r);

        QPen white(QColor(255, 255, 255, 230));
        white.setWidthF(1.0);
        p.setPen(white);
        p.drawEllipse(pos, r - 1.0, r - 1.0);
    }

    // SV picker circle with black+white outlines
    {
        QPointF pick;
        if (!mTriangleMode) {
            const qreal s = mCurrent.hsvSaturationF();
            const qreal v = mCurrent.valueF();
            pick = QPointF(mSvRect.left() + s * mSvRect.width(),
                           mSvRect.top() + (1.0 - v) * mSvRect.height());
        } else {
            // Legacy inverse mapping (ColorWheel::trianglePosFromColor)
            const SvTriangle t = svTriangleForWheelInnerRim(mWheelRect, mWheelThickness);
            const qreal S = mCurrent.hsvSaturationF();
            const qreal V = mCurrent.valueF();
            const qreal b = 1.0 - V;
            const qreal c = S * V;
            const qreal w = qBound<qreal>(0.0, 1.0 - b - c, 1.0);
            pick = t.white * w + t.hue * qBound<qreal>(0.0, c, 1.0) + t.black * qBound<qreal>(0.0, b, 1.0);
        }

        const qreal radius = 6.0;

        QPen black(QColor(0, 0, 0, 220));
        black.setWidthF(2.0);
        p.setPen(black);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pick, radius, radius);

        QPen white(QColor(255, 255, 255, 220));
        white.setWidthF(1.0);
        p.setPen(white);
        p.drawEllipse(pick, radius - 1.0, radius - 1.0);
    }
}

void CosmicColorWheelWidget::updateLayoutRects()
{
    const int pad = 10;
    const int footerReserve = mFooterHeight;
    const int availableH = qMax(0, height() - footerReserve);
    const int s = qMin(width(), availableH) - pad * 2;
    QRectF content((width() - s) * 0.5, (availableH - s) * 0.5, s, s);

    // Make the ring much thinner (roughly 80% thinner than the previous fixed 28px at typical sizes).
    // Use a size-relative thickness so it scales well with pane resizing.
    const int prevThickness = mWheelThickness;
    mWheelThickness = qMax(8, static_cast<int>(qRound(s * 0.0715)));
    if (prevThickness != mWheelThickness) {
        mHueRingThicknessCache = -1;
    }

    mWheelRect = content;

    if (!mTriangleMode) {
        // Ensure the SV square is fully inside the inner circle (corners never outside the ring).
        // Largest square that fits inside a circle of radius R has side = sqrt(2) * R.
        const qreal innerDiameter = qMax<qreal>(0.0, content.width() - 2.0 * mWheelThickness);
        const qreal innerRadius = innerDiameter * 0.5;
        const qreal squarePad = qMax<qreal>(4.0, mWheelThickness * 0.6);
        const qreal effectiveR = qMax<qreal>(0.0, innerRadius - squarePad);
        const qreal squareSize = qMax<qreal>(0.0, M_SQRT2 * effectiveR);
        const QPointF c = content.center();
        mSvRect = QRectF(c.x() - squareSize * 0.5, c.y() - squareSize * 0.5, squareSize, squareSize);
    } else {
        // Exact legacy layout: SV area is the bounding rect of the inner-rim triangle.
        const SvTriangle t = svTriangleForWheelInnerRim(mWheelRect, mWheelThickness);
        QRectF triBounds;
        triBounds.setTopLeft(QPointF(qMin(qMin(t.white.x(), t.black.x()), t.hue.x()), qMin(qMin(t.white.y(), t.black.y()), t.hue.y())));
        triBounds.setBottomRight(QPointF(qMax(qMax(t.white.x(), t.black.x()), t.hue.x()), qMax(qMax(t.white.y(), t.black.y()), t.hue.y())));
        mSvRect = triBounds.toAlignedRect();
    }
}

void CosmicColorWheelWidget::updateFooterGeometry()
{
    if (!mFooter) {
        return;
    }
    mFooter->setGeometry(0, height() - mFooterHeight, width(), mFooterHeight);
}

QString CosmicColorWheelWidget::colorToHex(const QColor& color)
{
    const QColor c = color.toRgb();
    return QStringLiteral("#%1%2%3")
        .arg(c.red(), 2, 16, QLatin1Char('0'))
        .arg(c.green(), 2, 16, QLatin1Char('0'))
        .arg(c.blue(), 2, 16, QLatin1Char('0'))
        .toUpper();
}

QColor CosmicColorWheelWidget::hexToColor(const QString& text, int fallbackAlpha)
{
    QString t = text.trimmed();
    if (t.startsWith('#')) {
        t.remove(0, 1);
    }
    QRegularExpression re(QStringLiteral("^([A-Fa-f0-9]{6})$"));
    const auto m = re.match(t);
    if (!m.hasMatch()) {
        return QColor();
    }
    const QString rgb = m.captured(1);
    bool ok = false;
    const int r = rgb.mid(0, 2).toInt(&ok, 16);
    if (!ok) return QColor();
    const int g = rgb.mid(2, 2).toInt(&ok, 16);
    if (!ok) return QColor();
    const int b = rgb.mid(4, 2).toInt(&ok, 16);
    if (!ok) return QColor();
    return QColor(r, g, b, fallbackAlpha);
}

void CosmicColorWheelWidget::ensureHueRingPixmap()
{
    const QSize sz = mWheelRect.size().toSize();
    if (sz.isEmpty()) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    if (!mHueRingPixmap.isNull()
        && mHueRingSizeCache == sz
        && mHueRingThicknessCache == mWheelThickness
        && qFuzzyCompare(mHueRingDprCache, dpr)) {
        return;
    }

    mHueRingSizeCache = sz;
    mHueRingThicknessCache = mWheelThickness;
    mHueRingDprCache = dpr;

    QPixmap pm(sz * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center(sz.width() * 0.5, sz.height() * 0.5);
    const qreal halfT = mWheelThickness * 0.5;
    const QRectF arcRect(QPointF(halfT, halfT), QSizeF(sz.width() - 2.0 * halfT, sz.height() - 2.0 * halfT));

    QConicalGradient grad(center, 0.0);
    const int stopStep = 5;
    for (int h = 0; h <= 360; h += stopStep) {
        const qreal t = static_cast<qreal>(h) / 360.0;
        const int hue = (h == 360) ? 0 : h;
        grad.setColorAt(t, QColor::fromHsv(hue, 255, 255));
    }

    QPen pen(QBrush(grad), mWheelThickness, Qt::SolidLine, Qt::FlatCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(arcRect);

    mHueRingPixmap = pm;
}

void CosmicColorWheelWidget::ensureSvImage()
{
    const int hue = (mCurrent.hue() < 0) ? 0 : mCurrent.hue();
    const QSize sz = mSvRect.size().toSize();

    const qreal dpr = devicePixelRatioF();

    if (sz.isEmpty()) {
        return;
    }

    if (mSvHueCache == hue && mSvSizeCache == sz && qFuzzyCompare(mSvDprCache, dpr)
        && !mSvImage.isNull() && !mSvPixmap.isNull()) {
        return;
    }

    mSvHueCache = hue;
    mSvSizeCache = sz;
    mSvDprCache = dpr;

    mSvImage = QImage(sz, QImage::Format_ARGB32_Premultiplied);
    mSvImage.fill(Qt::transparent);

    if (!mTriangleMode) {
        for (int y = 0; y < sz.height(); ++y) {
            const qreal v = 1.0 - (sz.height() <= 1 ? 0.0 : (static_cast<qreal>(y) / (sz.height() - 1)));
            QRgb* scan = reinterpret_cast<QRgb*>(mSvImage.scanLine(y));
            for (int x = 0; x < sz.width(); ++x) {
                const qreal s = (sz.width() <= 1 ? 0.0 : (static_cast<qreal>(x) / (sz.width() - 1)));
                scan[x] = QColor::fromHsvF(hue / 360.0, s, v, 1.0).rgba();
            }
        }
    } else {
        // True legacy render pipeline:
        // - Triangle vertices are in widget space on the inner rim.
        // - Build a supersampled AA Alpha8 mask from the vector triangle.
        // - Compute per-pixel HSV using legacy u/v mapping in widget space.
        // - Downscale with SmoothTransformation for crisp edges.

        const SvTriangle t = svTriangleForWheelInnerRim(mWheelRect, mWheelThickness);
        const int ss = 3;
        const QSize hiSize(sz.width() * ss, sz.height() * ss);

        QImage hiImage(hiSize, QImage::Format_ARGB32_Premultiplied);
        hiImage.fill(Qt::transparent);

        QImage mask(hiSize, QImage::Format_Alpha8);
        mask.fill(0);
        {
            QPainter mp(&mask);
            mp.setRenderHint(QPainter::Antialiasing, true);
            mp.setPen(Qt::NoPen);
            mp.setBrush(Qt::white);
            QPainterPath path;
            const QPointF offset = -mSvRect.topLeft();
            path.moveTo((t.white + offset) * ss);
            path.lineTo((t.hue + offset) * ss);
            path.lineTo((t.black + offset) * ss);
            path.closeSubpath();
            mp.drawPath(path);
        }

        const qreal eps = 1e-6;
        for (int y = 0; y < hiSize.height(); ++y) {
            const uchar* maskLine = mask.constScanLine(y);
            QRgb* out = reinterpret_cast<QRgb*>(hiImage.scanLine(y));
            for (int x = 0; x < hiSize.width(); ++x) {
                const int a = maskLine[x];
                if (a == 0) {
                    out[x] = qRgba(0, 0, 0, 0);
                    continue;
                }

                const QPointF p(mSvRect.left() + (x + 0.5) / ss, mSvRect.top() + (y + 0.5) / ss);
                qreal u = 0.0, v = 0.0;
                if (!legacyTriangleUV(p, t, u, v)) {
                    out[x] = qRgba(0, 0, 0, 0);
                    continue;
                }
                if (u < -eps || v < -eps || (u + v) > 1.0 + eps) {
                    out[x] = qRgba(0, 0, 0, 0);
                    continue;
                }

                qreal V = 1.0 - u;
                qreal S = (V > 1e-6) ? (v / V) : 0.0;
                S = qBound<qreal>(0.0, S, 1.0);
                V = qBound<qreal>(0.0, V, 1.0);

                const QColor c = QColor::fromHsvF(hue / 360.0, S, V, 1.0);
                // hiImage is premultiplied; write premultiplied pixels to avoid dark fringes on scaling.
                out[x] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), a));
            }
        }

        mSvImage = hiImage.scaled(sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // Convert to a DPR-aware pixmap for fast painting.
    mSvPixmap = QPixmap::fromImage(mSvImage);
    mSvPixmap.setDevicePixelRatio(dpr);
}

bool CosmicColorWheelWidget::isInHueRing(const QPointF& p) const
{
    const QPointF c = mWheelRect.center();
    const qreal dx = p.x() - c.x();
    const qreal dy = p.y() - c.y();
    const qreal dist = qSqrt(dx * dx + dy * dy);

    const qreal outerR = qMin(mWheelRect.width(), mWheelRect.height()) * 0.5;
    const qreal innerR = qMax<qreal>(0.0, outerR - mWheelThickness);

    return dist <= outerR && dist >= innerR;
}

bool CosmicColorWheelWidget::isInSvSquare(const QPointF& p) const
{
    if (!mSvRect.contains(p)) {
        return false;
    }
    if (!mTriangleMode) {
        return true;
    }

    const SvTriangle t = svTriangleForWheelInnerRim(mWheelRect, mWheelThickness);
    qreal u = 0.0, v = 0.0;
    if (!legacyTriangleUV(p, t, u, v)) {
        return false;
    }
    const qreal eps = 1e-6;
    return !(u < -eps || v < -eps || (u + v) > 1.0 + eps);
}

void CosmicColorWheelWidget::applyHueFromPoint(const QPointF& p, bool emitSignal)
{
    const QPointF c = mWheelRect.center();
    const qreal dx = p.x() - c.x();
    const qreal dy = p.y() - c.y();

    qreal deg = qAtan2(-dy, dx) * 180.0 / M_PI;
    deg = std::fmod(deg + 360.0, 360.0);

    int h = static_cast<int>(qRound(deg));
    if (h < 0) h = 0;
    if (h > 359) h = 359;

    if (h == mCurrent.hue()) {
        return;
    }

    QColor newColor = mCurrent;
    newColor.setHsv(h, newColor.saturation(), newColor.value(), newColor.alpha());
    setColor(newColor);

    if (emitSignal) {
        emit colorChanged(mCurrent);
    }
}

void CosmicColorWheelWidget::applySvFromPoint(const QPointF& p, bool emitSignal)
{
    if (mSvRect.isEmpty()) {
        return;
    }

    if (mTriangleMode) {
        const SvTriangle t = svTriangleForWheelInnerRim(mWheelRect, mWheelThickness);
        const QPointF clamped = clampPointToTriangle(p, t);

        qreal u = 0.0, v = 0.0;
        if (!legacyTriangleUV(clamped, t, u, v)) {
            return;
        }

        qreal V = 1.0 - u;
        qreal S = (V > 1e-6) ? (v / V) : 0.0;
        S = qBound<qreal>(0.0, S, 1.0);
        V = qBound<qreal>(0.0, V, 1.0);

        int hue = mCurrent.hue();
        if (hue < 0) hue = 0;

        QColor newColor = QColor::fromHsvF(hue / 360.0, S, V, mCurrent.alphaF());
        if (newColor == mCurrent) {
            return;
        }

        setColor(newColor);
        if (emitSignal) {
            emit colorChanged(mCurrent);
        }
        return;
    }

    const qreal x = clampF((p.x() - mSvRect.left()) / mSvRect.width(), 0.0, 1.0);
    const qreal y = clampF((p.y() - mSvRect.top()) / mSvRect.height(), 0.0, 1.0);

    const int sat = clampInt(static_cast<int>(qRound(x * 255.0)), 0, 255);
    const int val = clampInt(static_cast<int>(qRound((1.0 - y) * 255.0)), 0, 255);

    if (sat == mCurrent.saturation() && val == mCurrent.value()) {
        return;
    }

    int hue = mCurrent.hue();
    if (hue < 0) hue = 0;

    QColor newColor = mCurrent;
    newColor.setHsv(hue, sat, val, newColor.alpha());
    setColor(newColor);

    if (emitSignal) {
        emit colorChanged(mCurrent);
    }
}

int CosmicColorWheelWidget::clampInt(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

qreal CosmicColorWheelWidget::clampF(qreal v, qreal lo, qreal hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
