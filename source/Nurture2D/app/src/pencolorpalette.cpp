#include "pencolorpalette.h"
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QDebug>
#include <QSettings>
#include <QFrame>
#include "preferencesdef.h"
#include "pencildef.h"
#include "preferencemanager.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "n2duimetrics.h"

namespace {

// Create a tinted pen icon from the shared Interaction/Tool_Pen.svg asset.
// If the SVG cannot be loaded, fall back to a simple colored circle.
static QIcon createPenIcon(const QColor& color, const QSize& size)
{
    if (!size.isValid()) {
        return QIcon();
    }

    QSvgRenderer renderer(QString(":/Interaction/Tool_Pen.svg"));
    if (!renderer.isValid()) {
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        const int margin = qMax(2, size.width() / 8);
        painter.drawEllipse(margin, margin,
                            size.width() - 2 * margin,
                            size.height() - 2 * margin);
        painter.end();
        return QIcon(pixmap);
    }

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Render the pen glyph, then tint it using the color via SourceIn.
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();

    return QIcon(pixmap);
}

// Variant used for the first (current color) button: draws a rectangular
// highlight bar beneath the icon instead of relying on the cosmic background.
static QIcon createPenIconWithRect(const QColor& color, const QSize& size)
{
    QIcon base = createPenIcon(color, size);
    QPixmap pm(size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw a rectangle near the bottom as the highlight.
    const int rectHeight = qMax(2, size.height() / 5);
    QRect rect(0, size.height() - rectHeight - 1, size.width(), rectHeight);
    p.fillRect(rect, color);

    // Draw the pen icon on top, centered.
    base.paint(&p, QRect(0, 0, size.width(), size.height()));
    p.end();

    return QIcon(pm);
}

} // namespace

PenColorPalette::PenColorPalette(QWidget* parent)
    : QWidget(parent)
    , mLayout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    , mActiveButton(nullptr)
    , mCurrentColor(Qt::black)
    , mSavedColor(Qt::black)
{
    // Match spacing with AutoLineColorToggle so buckets and pens align in a
    // clean grid.
    mLayout->setSpacing(6);
    mLayout->setContentsMargins(0, 0, 0, 0);

    // Add default drawing color (initially black but changeable)
    mColors.append(mSavedColor);
    createColorButton(mSavedColor, true);

    // Separator between the first (current color) pen and the 5 presets. Use a
    // plain widget (not QFrame VLine) so we fully control color and thickness.
    auto* firstSeparator = new QWidget(this);
    firstSeparator->setFixedWidth(N2DUiMetrics::TOOLBAR_SEPARATOR_THICKNESS_PX);
    firstSeparator->setFixedHeight(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);
    firstSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    firstSeparator->setStyleSheet("background-color:#1f1f1f;");
    mFirstSeparator = firstSeparator;
    mLayout->addWidget(firstSeparator);

    // Add the 5 fill tool colors
    for (const QColor& color : FILL_COLORS) {
        mColors.append(color);
        createColorButton(color, false);
    }

    // Set the first button as active by default
    if (!mColorButtons.isEmpty()) {
        setActiveButton(mColorButtons.first());
    }

    setLayout(mLayout);
}

void PenColorPalette::createColorButton(const QColor& color, bool isDefault)
{
    CosmicCheckboxToggle* button = new CosmicCheckboxToggle(this);
    button->setMode(CosmicCheckboxToggle::Custom);
    button->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    button->setBordered(false);
    button->setShowText(false);
    button->setShowIcon(true);
    // First button (current color) is icon-only (no cosmic background); the
    // 5 presets behave like normal cosmic custom-icon toggles with the
    // standard accent background.
    button->setBackgroundEnabled(!isDefault);
    button->setCheckable(true);

    // Use a single icon silhouette for all buttons, tinted per color. Match
    // the bucket icon size (28x28) so rows line up in a clean grid.
    const QSize iconSize(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX, N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);
    if (isDefault) {
        button->setIcon(createPenIcon(color, iconSize));
    } else {
        button->setIcon(createPenIcon(color, iconSize));
    }
    button->setIconSize(iconSize);

    // Ensure the cosmic background highlight for these pen controls uses the
    // global app accent color instead of any legacy hard-coded value.
    const QColor accent = BlenderStyleSlider::getAccentColor();
    button->setAccentColor(accent);

    // Initial visual state
    updateButtonStyle(button, color, false, isDefault);

    // Connect signal
    connect(button, &QAbstractButton::clicked, this, &PenColorPalette::onColorButtonClicked);

    mColorButtons.append(button);
    mLayout->addWidget(button);
}

void PenColorPalette::setVerticalLayoutEnabled(bool vertical)
{
    if (!mLayout) {
        return;
    }

    // Rebuild the layout order without destroying the existing widgets.
    // We know the logical order: [button0][separator][button1..N-1].
    if (mColorButtons.isEmpty()) {
        return;
    }

    // Detach all items from the layout; keep widgets alive.
    while (QLayoutItem* item = mLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->setParent(this);
        }
        delete item;
    }

    // Horizontal: single row, vertical separator between first and presets.
    if (!vertical) {
        mLayout->setDirection(QBoxLayout::LeftToRight);

        CosmicCheckboxToggle* firstButton = mColorButtons.first();
        if (firstButton) {
            mLayout->addWidget(firstButton);
        }

        if (mFirstSeparator) {
            mFirstSeparator->setFixedWidth(N2DUiMetrics::TOOLBAR_SEPARATOR_THICKNESS_PX);
            mFirstSeparator->setFixedHeight(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);
            mFirstSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            mLayout->addWidget(mFirstSeparator);
        }

        for (int i = 1; i < mColorButtons.size(); ++i) {
            if (mColorButtons[i]) {
                mLayout->addWidget(mColorButtons[i]);
            }
        }
    } else {
        // Vertical: one row per entry stacked top-to-bottom, with the
        // separator drawn as a horizontal bar beneath the first entry.
        mLayout->setDirection(QBoxLayout::TopToBottom);

        CosmicCheckboxToggle* firstButton = mColorButtons.first();
        if (firstButton) {
            mLayout->addWidget(firstButton);
        }

        if (mFirstSeparator) {
            mFirstSeparator->setFixedHeight(N2DUiMetrics::TOOLBAR_SEPARATOR_THICKNESS_PX);
            mFirstSeparator->setFixedWidth(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);
            mFirstSeparator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            mLayout->addWidget(mFirstSeparator);
        }

        for (int i = 1; i < mColorButtons.size(); ++i) {
            if (mColorButtons[i]) {
                mLayout->addWidget(mColorButtons[i]);
            }
        }
    }

    mLayout->invalidate();
    updateGeometry();
    update();
}

void PenColorPalette::detachForExternalLayout()
{
    if (!mLayout) {
        return;
    }

    while (QLayoutItem* item = mLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->setParent(this);
        }
        delete item;
    }
    mLayout->invalidate();
    updateGeometry();
    update();
}

void PenColorPalette::updateButtonStyle(CosmicCheckboxToggle* button, const QColor& color, bool isActive, bool isDefault)
{
    if (!button) return;

    const QSize iconSize = !button->iconSize().isEmpty()
        ? button->iconSize()
        : QSize(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX, N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);

    if (isDefault) {
        // First (current color) button: highlight is a rectangle in the icon.
        if (isActive) {
            button->setIcon(createPenIconWithRect(color, iconSize));
        } else {
            button->setIcon(createPenIcon(color, iconSize));
        }
        // No cosmic background; accent is informational only.
        button->setAccentColor(color);
        button->setChecked(isActive);
    } else {
        // Preset buttons: behave like normal cosmic custom-icon toggles.
        // We keep the icon tinted by color, but rely on the default cosmic
        // accent background for the active state.
        if (isActive) {
            button->setIcon(createPenIcon(color, iconSize));
        } else {
            // Inactive presets use the same icon; background communicates
            // state.
            button->setIcon(createPenIcon(color, iconSize));
        }
        // Do NOT override accentColor here; keep the default cosmic accent
        // from the control's constructor so the background uses the normal
        // cosmic highlight color.
        button->setChecked(isActive);
    }
}

void PenColorPalette::onColorPreferenceChanged(SETTING setting)
{
    // No longer needed - we use palette colors for selection now
    Q_UNUSED(setting);
}

void PenColorPalette::connectToPreferenceManager(PreferenceManager* prefs)
{
    if (prefs) {
        connect(prefs, &PreferenceManager::optionChanged, this, &PenColorPalette::onColorPreferenceChanged);
    }
}

void PenColorPalette::setActiveButton(CosmicCheckboxToggle* button)
{
    if (mActiveButton) {
        mActiveButton->setChecked(false);
        int index = mColorButtons.indexOf(mActiveButton);
        if (index >= 0 && index < mColors.size()) {
            updateButtonStyle(mActiveButton, mColors[index], false, index == 0);
        }
    }

    mActiveButton = button;
    if (mActiveButton) {
        mActiveButton->setChecked(true);
        int index = mColorButtons.indexOf(mActiveButton);
        if (index >= 0 && index < mColors.size()) {
            updateButtonStyle(mActiveButton, mColors[index], true, index == 0);
            mCurrentColor = mColors[index];
        }
    }
}

void PenColorPalette::onColorButtonClicked()
{
    if (mColorButtons.isEmpty()) return;

    CosmicCheckboxToggle* clickedButton = qobject_cast<CosmicCheckboxToggle*>(sender());
    if (!clickedButton) return;

    int index = mColorButtons.indexOf(clickedButton);
    if (index < 0 || index >= mColors.size()) return;

    int prevIndex = mColorButtons.indexOf(mActiveButton);

    if (index == 0) {
        setActiveButton(clickedButton);
        mCurrentColor = mSavedColor;
        emit colorChanged(mSavedColor);
    } else {
        if (prevIndex == 0) {
            mSavedColor = mCurrentColor;
            mColors[0] = mSavedColor;
            CosmicCheckboxToggle* firstButton = mColorButtons.first();
            const QSize iconSize(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX, N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);
            firstButton->setIcon(createPenIcon(mSavedColor, iconSize));
            firstButton->setIconSize(iconSize);
        }
        setActiveButton(clickedButton);
        mCurrentColor = mColors[index];
        emit colorChanged(mColors[index]);
    }
}

void PenColorPalette::setCurrentColor(const QColor& color)
{
    if (mColorButtons.isEmpty() || !mActiveButton || !color.isValid()) return;

    mCurrentColor = color;
    if (!mColorButtons.isEmpty() && mActiveButton == mColorButtons.first()) {
        mSavedColor = color;
        mColors[0] = color;
        CosmicCheckboxToggle* firstButton = mColorButtons.first();
        if (!firstButton) return;
        const QSize iconSize(N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX, N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX);
        firstButton->setIcon(createPenIcon(color, iconSize));
        firstButton->setIconSize(iconSize);

        updateButtonStyle(firstButton, color, true, true);
    }
}

QColor PenColorPalette::getCurrentColor() const
{
    return mCurrentColor;
}

int PenColorPalette::getActiveButtonIndex() const
{
    return mColorButtons.indexOf(mActiveButton);
}

void PenColorPalette::activateFirstButtonAndSetColor(const QColor& color)
{
    if (mColorButtons.isEmpty()) return;
    CosmicCheckboxToggle* firstButton = mColorButtons.first();
    setActiveButton(firstButton);
    setCurrentColor(color);
    emit colorChanged(color);
}

bool PenColorPalette::isPresetColor(const QColor& color) const
{
    for (const QColor& preset : FILL_COLORS) {
        if (preset == color) return true;
    }
    return false;
}

void PenColorPalette::activatePresetButton(const QColor& color)
{
    for (int i = 1; i < FILL_COLORS.size() + 1; ++i) {
        if (i >= mColorButtons.size()) break; // Safety check
        if (FILL_COLORS[i - 1] == color) {
            setActiveButton(mColorButtons[i]);
            break;
        }
    }
}