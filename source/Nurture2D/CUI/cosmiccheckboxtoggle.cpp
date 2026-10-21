#include "cosmiccheckboxtoggle.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QMouseEvent>
#include <QApplication>
#include <QTimer>
#include <QEvent>

#include "cosmicuimetrics.h"

namespace {
static const QColor kBaseBorderedBg(0x4d, 0x4d, 0x4d, 0xfe);   // 4d4d4dfe (hover/base color)
static const QColor kPressBg(0x99, 0x99, 0x99, 0xff);           // 999999ff
static const QColor kAccentBg(0x50, 0x50, 0x50, 0xff);          // 40555cff (cosmic accent)
static const QColor kCosmicText(0xb3, 0xb3, 0xb3, 0xff);        // b3b3b3ff global text
static const int kPaddingH = 6;   // horizontal padding inside control
static const int kPaddingV = 2;   // vertical padding inside control
static const int kIconSize = 16;
static const int kSpacing = 4;    // spacing between icon and text
// kCheckboxSize is no longer a compile-time constant — use CosmicUiMetrics::controlThickness() at runtime.
static const int kPressFlashMs = 140; // duration of pressed highlight (ms)
}

CosmicCheckboxToggle::CosmicCheckboxToggle(QWidget* parent)
    : QAbstractButton(parent)
    , mAccentColor(kAccentBg)
    , mCheckIcon(QIcon(":/Interaction/CheckboxChecked.svg"))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);

    QPalette pal = palette();
    pal.setColor(QPalette::ButtonText, kCosmicText);
    pal.setColor(QPalette::WindowText, kCosmicText);
    setPalette(pal);

    // Default to Big custom mode: borderless, textless, icon-only.
    // Call setters so any future logic tied to them is applied consistently.
    setMode(Custom);
    setScaleMode(ScaleBig);
    setBordered(false);
    setShowText(false);
    setShowIcon(true);

     refreshAccentFromPaletteIfNeeded();
}

void CosmicCheckboxToggle::setBaseBackgroundColor(const QColor& color)
{
    mBaseBgOverridden = true;
    mBaseBg = color;
    update();
}

void CosmicCheckboxToggle::clearBaseBackgroundColor()
{
    mBaseBgOverridden = false;
    update();
}

void CosmicCheckboxToggle::setTextColor(const QColor& color)
{
    mTextColorOverridden = true;
    mTextColor = color;
    update();
}

void CosmicCheckboxToggle::clearTextColor()
{
    mTextColorOverridden = false;
    update();
}

void CosmicCheckboxToggle::setCheckedTextColor(const QColor& color)
{
    mCheckedTextColorOverridden = true;
    mCheckedTextColor = color;
    update();
}

void CosmicCheckboxToggle::clearCheckedTextColor()
{
    mCheckedTextColorOverridden = false;
    update();
}

void CosmicCheckboxToggle::refreshAccentFromPaletteIfNeeded()
{
    if (mAccentColorOverridden)
        return;

    const QColor accent = palette().color(QPalette::Highlight);
    if (accent.isValid() && accent != mAccentColor) {
        mAccentColor = accent;
        update();
    }
}

void CosmicCheckboxToggle::setMode(Mode mode)
{
    if (mMode == mode)
        return;
    mMode = mode;
    update();
}

void CosmicCheckboxToggle::setScaleMode(ScaleMode mode)
{
    if (mScaleMode == mode)
        return;
    mScaleMode = mode;
    update();
}

void CosmicCheckboxToggle::setBordered(bool bordered)
{
    if (mBordered == bordered)
        return;
    mBordered = bordered;
    update();
}

void CosmicCheckboxToggle::setShowText(bool show)
{
    if (mShowText == show)
        return;
    mShowText = show;
    update();
}

void CosmicCheckboxToggle::setShowIcon(bool show)
{
    if (mShowIcon == show)
        return;
    mShowIcon = show;
    update();
}

void CosmicCheckboxToggle::setAccentColor(const QColor& color)
{
    mAccentColorOverridden = true;
    mAccentColor = color;
    update();
}

void CosmicCheckboxToggle::changeEvent(QEvent* event)
{
    QAbstractButton::changeEvent(event);
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

void CosmicCheckboxToggle::setBackgroundEnabled(bool enabled)
{
    if (mBackgroundEnabled == enabled)
        return;
    mBackgroundEnabled = enabled;
    update();
}

void CosmicCheckboxToggle::setHoverHighlightEnabled(bool enabled)
{
    if (mHoverHighlightEnabled == enabled)
        return;
    mHoverHighlightEnabled = enabled;
    update();
}

void CosmicCheckboxToggle::setIconDimWhenUnchecked(bool dim)
{
    if (mIconDimWhenUnchecked == dim)
        return;
    mIconDimWhenUnchecked = dim;
    update();
}

void CosmicCheckboxToggle::setCheckIcon(const QIcon& icon)
{
    mCheckIcon = icon;
    update();
}

void CosmicCheckboxToggle::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (mMode == Custom) {
        paintCustom(p);
    } else {
        paintDefaultCheckbox(p);
    }
}

void CosmicCheckboxToggle::paintCustom(QPainter& p)
{
    QRect r = rect();

    // Determine background color based on state.
    QColor bg(0, 0, 0, 0); // fully transparent default
    if (mBackgroundEnabled) {
        if (mPressed) {
            // Pressed state overrides everything else visually
            bg = kPressBg;
        } else if (isChecked()) {
            // Enabled state uses accent color (only when not pressed)
            bg = mAccentColor;
        } else if (mHover && mHoverHighlightEnabled) {
            // Hover color, regardless of mode/border, only when not checked
            bg = kBaseBorderedBg;
        } else if (mBordered) {
            // Non-hover, non-checked, bordered base
            bg = mBaseBgOverridden ? mBaseBg : kBaseBorderedBg;
        }
    }

    if (bg.alpha() > 0) {
        // Mac-like squircle rounded corners
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        const int cr = CosmicUiMetrics::cornerRadius();
        p.drawRoundedRect(r, cr, cr, Qt::AbsoluteSize);
    }

    // Layout icon + text content.
    QFontMetrics fm = fontMetrics();

    // Custom mode thickness is unified with the CosmicIntensityBox height.
    // For horizontal cosmic toggles (ScaleBig), increase padding so the
    // overall height matches other cosmic controls. Icon size is primarily
    // driven by the button's iconSize() property when available so callers
    // (e.g. the Tools dock) can control icon scaling precisely.
    int baseIconSize = kIconSize;
    if (!icon().isNull()) {
        const QSize configured = iconSize();
        if (!configured.isEmpty()) {
            baseIconSize = qMax(configured.width(), configured.height());
        }
    }

    int iconSize = baseIconSize;
    // ScaleBig is intentionally identical to ScaleNormal — no extra scaling applied.

    const int paddingH = kPaddingH;
    const int paddingV = kPaddingV;

    int contentWidth = 0;
    if (mShowIcon && !icon().isNull()) {
        contentWidth += iconSize;
    }
    if (mShowText && !text().isEmpty()) {
        int textWidth = fm.horizontalAdvance(text());
        if (contentWidth > 0 && textWidth > 0)
            contentWidth += kSpacing;
        contentWidth += textWidth;
    }

    QRect inner = r.adjusted(paddingH, paddingV, -paddingH, -paddingV);
    int x = inner.x() + (inner.width() - contentWidth) / 2;
    int centerY = inner.center().y();

    // Draw icon
    if (mShowIcon && !icon().isNull()) {
        QRect iconRect(x, centerY - iconSize / 2, iconSize, iconSize);
        const bool dimIcon = mIconDimWhenUnchecked && !isChecked() && !mPressed;
        if (dimIcon) p.setOpacity(0.35);
        icon().paint(&p, iconRect);
        if (dimIcon) p.setOpacity(1.0);
        x += iconSize;
        if (mShowText && !text().isEmpty())
            x += kSpacing;
    }

    // Draw text
    if (mShowText && !text().isEmpty()) {
        const QColor penColor = isChecked()
                                     ? (mCheckedTextColorOverridden ? mCheckedTextColor
                                                                   : (mTextColorOverridden ? mTextColor : kCosmicText))
                                     : (mTextColorOverridden ? mTextColor : kCosmicText);
        p.setPen(penColor);
        QRect textRect(x, inner.y(), inner.right() - x, inner.height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

void CosmicCheckboxToggle::paintDefaultCheckbox(QPainter& p)
{
    const QRect r = rect();
    const int thickness = CosmicUiMetrics::controlThickness();
    const int boxSide = (mScaleMode == ScaleNormal) ? qMax(12, thickness * 3 / 5) : thickness;
    const int boxY = r.y() + (r.height() - boxSide) / 2;
    const int boxX = r.x();  // widget width == boxSide, so no offset needed
    const QRect boxRect(boxX, boxY, boxSide, boxSide);

    // Icon rect: slightly inset for ScaleBigBox, same as box otherwise.
    QRect iconRect = boxRect;
    if (mScaleMode == ScaleBigBox && boxSide > 12) {
        const int delta = boxSide / 6;
        iconRect.adjust(delta, delta, -delta, -delta);
    }

    const int cr = qMax(2, CosmicUiMetrics::cornerRadius());

    if (mPressed) {
        p.setPen(Qt::NoPen);
        p.setBrush(kPressBg);
        p.drawRoundedRect(boxRect, cr, cr, Qt::AbsoluteSize);
    } else if (!isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(kBaseBorderedBg);
        p.drawRoundedRect(boxRect, cr, cr, Qt::AbsoluteSize);
    }

    if (isChecked()) {
        if (!mCheckIcon.isNull()) {
            mCheckIcon.paint(&p, iconRect);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(mAccentColor);
            p.drawRoundedRect(boxRect, cr, cr, Qt::AbsoluteSize);
        }
    }

    if (!text().isEmpty()) {
        p.setPen(kCosmicText);
        const int textX = boxRect.right() + kSpacing;
        p.drawText(QRect(textX, r.y(), r.right() - textX, r.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

void CosmicCheckboxToggle::enterEvent(QEvent* event)
{
    mHover = true;
    QAbstractButton::enterEvent(event);
    update();
}

void CosmicCheckboxToggle::leaveEvent(QEvent* event)
{
    mHover = false;
    QAbstractButton::leaveEvent(event);
    update();
}

void CosmicCheckboxToggle::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        // Step 1: show press color for a single frame
        mPressed = true;
        update();

        // Capture the new checked state at press time.
        const bool newChecked = !isChecked();

        // Step 2: after a short delay, apply the toggle and
        // let accent / checked visuals take over while mouse is still down.
        QTimer::singleShot(kPressFlashMs, this, [this, newChecked]() {
            if (!this->isEnabled())
                return;

            this->setChecked(newChecked);
            emit this->toggled(newChecked);
            emit this->clicked(newChecked);

            this->mPressed = false;
            this->update();
        });

        event->accept();
        return;
    }
    QAbstractButton::mousePressEvent(event);
}

void CosmicCheckboxToggle::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Nothing to do; we already toggled on press and cleared mPressed
        // via the single-shot timer.
        event->accept();
        return;
    }
    QAbstractButton::mouseReleaseEvent(event);
}

QSize CosmicCheckboxToggle::sizeHint() const
{
    QFontMetrics fm = fontMetrics();
    int w = 0;
    int h = 0;

    if (mMode == Custom) {
        int contentWidth = 0;

        int baseIconSize = kIconSize;
        if (!icon().isNull()) {
            const QSize configured = iconSize();
            if (!configured.isEmpty()) {
                baseIconSize = qMax(configured.width(), configured.height());
            }
        }
        int iconSize = baseIconSize;
        // ScaleBig is intentionally identical to ScaleNormal — no extra scaling applied.

        const int paddingH = kPaddingH;
        const int paddingV = kPaddingV;
        if (mShowIcon && !icon().isNull()) {
            contentWidth += iconSize;
            h = qMax(h, iconSize);
        }
        if (mShowText && !text().isEmpty()) {
            int textWidth = fm.horizontalAdvance(text());
            if (contentWidth > 0 && textWidth > 0)
                contentWidth += kSpacing;
            contentWidth += textWidth;
            h = qMax(h, fm.height());
        }
        w = contentWidth + 2 * paddingH;
        h = h + 2 * paddingV;
    } else {
        // DefaultCheckbox mode: widget is exactly the square size — no extra padding.
        // The layout right-aligns it so it sits flush with the right edge.
        const int thickness = CosmicUiMetrics::controlThickness();
        const int boxSide = (mScaleMode == ScaleNormal) ? qMax(12, thickness * 3 / 5) : thickness;
        h = boxSide + 2 * kPaddingV;
        w = boxSide;
        if (!text().isEmpty()) {
            w += kSpacing + fm.horizontalAdvance(text());
            h = qMax(h, fm.height() + 2 * kPaddingV);
        }
    }

    return QSize(w, h);
}
