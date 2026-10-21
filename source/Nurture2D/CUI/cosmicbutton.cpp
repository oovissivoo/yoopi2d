#include "cosmicbutton.h"
#include "cosmicuimetrics.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>
#include <QMouseEvent>
#include <QEvent>

namespace {
static const QColor kDefaultNormalBg(0, 0, 0, 0);              // fully transparent
static const QColor kDefaultHoverBg(0x4d, 0x4d, 0x4d, 0xff);   // 4d4d4dff
static const QColor kDefaultPressedBg(0x50, 0x50, 0x50, 0xff); // 40555cff (accent)
static const QColor kCosmicText(0xb3, 0xb3, 0xb3, 0xff);       // b3b3b3ff
static const QColor kBorderedBaseBg(0x4d, 0x4d, 0x4d, 0xfe);   // 4d4d4dfe
}

CosmicButton::CosmicButton(QWidget* parent)
    : QPushButton(parent)
    , mNormalBg(kDefaultNormalBg)
    , mHoverBg(kDefaultHoverBg)
    , mPressedBg(kDefaultPressedBg)
    , mAccentColor(kDefaultPressedBg)
    , mTextColor(kCosmicText)
{
    setFlat(true);
    setAutoFillBackground(false);
    setCursor(Qt::PointingHandCursor);

    // Remove native borders and padding; background is painted manually.
    setStyleSheet("QPushButton { border: none; padding: 0px; background-color: transparent; }");

    // Ensure text uses the global dock text color.
    QPalette pal = palette();
    pal.setColor(QPalette::ButtonText, kCosmicText);
    setPalette(pal);

    refreshAccentFromPaletteIfNeeded();
}

void CosmicButton::refreshAccentFromPaletteIfNeeded()
{
    if (mAccentColorOverridden)
        return;

    const QColor accent = palette().color(QPalette::Highlight);
    if (accent.isValid() && accent != mAccentColor) {
        mAccentColor = accent;
        if (mBordered) {
            mPressedBg = mAccentColor;
        } else {
            // In borderless mode, use accent for pressed/checked state.
            mPressedBg = mAccentColor;
        }
        update();
    }
}

CosmicButton::CosmicButton(const QString& text, QWidget* parent)
    : CosmicButton(parent)
{
    setText(text);
}

void CosmicButton::setTextColor(const QColor& color)
{
    mTextColor = color;
    mTextColorOverridden = true;
    update();
}

void CosmicButton::setBackgroundColors(const QColor& normal,
                                       const QColor& hover,
                                       const QColor& pressed)
{
    mNormalBg = normal;
    mHoverBg = hover;
    mPressedBg = pressed;
    update();
}

void CosmicButton::setBordered(bool bordered)
{
    if (mBordered == bordered)
        return;

    mBordered = bordered;

    if (mBordered) {
        // Bordered mode: solid base background and accent-colored press state.
        mNormalBg = kBorderedBaseBg;
        mHoverBg = kBorderedBaseBg;
        mPressedBg = mAccentColor;
    } else {
        // Restore default borderless cosmic behavior.
        mNormalBg = kDefaultNormalBg;
        mHoverBg = kDefaultHoverBg;
        mPressedBg = kDefaultPressedBg;
    }

    update();
}

void CosmicButton::setAccentColor(const QColor& color)
{
    mAccentColorOverridden = true;
    mAccentColor = color;
    if (mBordered) {
        mPressedBg = mAccentColor;
    }
    update();
}

void CosmicButton::setTextAlignment(Qt::Alignment alignment)
{
    mTextAlignment = alignment;
    update();
}

void CosmicButton::setCornerRadius(int px)
{
    mCornerRadiusOverride = px;
    update();
}

void CosmicButton::setCornerRadii(bool topLeft, bool topRight, bool bottomLeft, bool bottomRight)
{
    mRadiusTL = topLeft;
    mRadiusTR = topRight;
    mRadiusBL = bottomLeft;
    mRadiusBR = bottomRight;
    update();
}

void CosmicButton::changeEvent(QEvent* event)
{
    QPushButton::changeEvent(event);
    if (!event) {
        return;
    }

    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
    case QEvent::StyleChange:
        refreshAccentFromPaletteIfNeeded();
        break;
    default:
        break;
    }
}

void CosmicButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = mNormalBg;
    if (!isEnabled()) {
        // Fade disabled background if any
        bg.setAlphaF(bg.alphaF() * 0.3);
    } else if (isCheckable() && isChecked()) {
        bg = mPressedBg;
    } else if (mPressed) {
        bg = mPressedBg;
    } else if (mHover) {
        bg = mHoverBg;
    }

    if (bg.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);

        // If all corners are rounded, use the simple fast path
        if (mRadiusTL && mRadiusTR && mRadiusBL && mRadiusBR) {
            const qreal r = (mCornerRadiusOverride >= 0) ? mCornerRadiusOverride : CosmicUiMetrics::cornerRadius();
            p.drawRoundedRect(rect(), r, r, Qt::AbsoluteSize);
        } else {
            // Build a path with per-corner radii
            const qreal r = (mCornerRadiusOverride >= 0) ? mCornerRadiusOverride : CosmicUiMetrics::cornerRadius();
            const QRectF rc = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
            QPainterPath path;
            path.moveTo(rc.left() + (mRadiusTL ? r : 0), rc.top());
            path.lineTo(rc.right() - (mRadiusTR ? r : 0), rc.top());
            if (mRadiusTR) path.arcTo(rc.right() - 2*r, rc.top(), 2*r, 2*r, 90, -90);
            else path.lineTo(rc.right(), rc.top());
            path.lineTo(rc.right(), rc.bottom() - (mRadiusBR ? r : 0));
            if (mRadiusBR) path.arcTo(rc.right() - 2*r, rc.bottom() - 2*r, 2*r, 2*r, 0, -90);
            else path.lineTo(rc.right(), rc.bottom());
            path.lineTo(rc.left() + (mRadiusBL ? r : 0), rc.bottom());
            if (mRadiusBL) path.arcTo(rc.left(), rc.bottom() - 2*r, 2*r, 2*r, 270, -90);
            else path.lineTo(rc.left(), rc.bottom());
            path.lineTo(rc.left(), rc.top() + (mRadiusTL ? r : 0));
            if (mRadiusTL) path.arcTo(rc.left(), rc.top(), 2*r, 2*r, 180, -90);
            else path.lineTo(rc.left(), rc.top());
            path.closeSubpath();
            p.drawPath(path);
        }
    }

    // Draw icon + text directly — bypasses the style engine entirely so
    // mTextColor is always respected regardless of focus/default state or QSS.
    const QColor textCol = mTextColor;

    if (!icon().isNull()) {
        const QSize iSz = iconSize().isValid() ? iconSize() : QSize(16, 16);
        const QPixmap px = icon().pixmap(iSz);
        if (!text().isEmpty()) {
            // icon left, text right — honour mTextAlignment
            const int gap = 4;
            const int leftPad = 8;
            int x;
            if (mTextAlignment == Qt::AlignLeft) {
                x = leftPad;
            } else {
                const int totalW = iSz.width() + gap + fontMetrics().horizontalAdvance(text());
                x = (width() - totalW) / 2;
            }
            const int iy = (height() - iSz.height()) / 2;
            p.drawPixmap(x, iy, px);
            x += iSz.width() + gap;
            p.setPen(textCol);
            p.drawText(QRect(x, 0, width() - x - leftPad, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        } else {
            p.drawPixmap((width() - iSz.width()) / 2, (height() - iSz.height()) / 2, px);
        }
    } else if (!text().isEmpty()) {
        p.setPen(textCol);
        if (mTextAlignment == Qt::AlignLeft) {
            p.drawText(rect().adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text());
        } else {
            p.drawText(rect().adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignHCenter, text());
        }
    }
}

void CosmicButton::enterEvent(QEvent* event)
{
    mHover = true;
    QPushButton::enterEvent(event);
    update();
}

void CosmicButton::leaveEvent(QEvent* event)
{
    mHover = false;
    QPushButton::leaveEvent(event);
    update();
}

void CosmicButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mPressed = true;
        update();
    }
    QPushButton::mousePressEvent(event);
}

void CosmicButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mPressed = false;
        update();
    }
    QPushButton::mouseReleaseEvent(event);
}

QSize CosmicButton::sizeHint() const
{
    // Preserve default width from QPushButton but clamp the height to the
    // unified cosmic thickness so it aligns with other horizontal cosmic widgets.
    // Add horizontal padding (8px each side) so text is never flush with the edge.
    QSize base = QPushButton::sizeHint();
    const int t = CosmicUiMetrics::controlThickness();
    const int hPad = 16; // 8px left + 8px right
    return QSize(base.width() + hPad, qMax(t, base.height()));
}

QSize CosmicButton::minimumSizeHint() const
{
    QSize base = QPushButton::minimumSizeHint();
    const int t = CosmicUiMetrics::controlThickness();
    const int hPad = 16;
    return QSize(base.width() + hPad, qMax(t - 2, base.height()));
}
