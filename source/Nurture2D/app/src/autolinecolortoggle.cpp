/*
Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "autolinecolortoggle.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QFontMetrics>
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QFrame>

#include "editor.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "n2duimetrics.h"

namespace {

// Create a tinted icon by applying the given color through the source alpha
// of the base icon. This keeps the bucket silhouette but matches the
// per-color swatch in the HDockLeft UI.
static QIcon makeTintedIcon(const QIcon& base, const QColor& tint, const QSize& targetSize)
{
    if (base.isNull() || !targetSize.isValid()) {
        return base;
    }

    QPixmap pm = base.pixmap(targetSize);
    if (pm.isNull()) {
        return base;
    }

    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(img.rect(), tint);
    p.end();

    return QIcon(QPixmap::fromImage(img));
}

} // namespace

AutoLineColorToggle::AutoLineColorToggle(Editor* editor, QWidget* parent)
    : QWidget(parent)
    , mEditor(editor)
{
    // Initialize bucket icons from Interaction qrc aliases
    mOpenBucketIcon = QIcon(":/Interaction/Bucket.svg");
    mClosedBucketIcon = QIcon(":/Interaction/BucketClosed.svg");

    // Initialize with the 5 default auto-line colors (logical set)
    mColors = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::magenta};
    mEnabled.resize(mColors.size());
    mColorValues.resize(mColors.size());

    for (int i = 0; i < mColors.size(); ++i) {
        mEnabled[i] = true;
        mColorValues[i] = qPremultiply(mColors[i].rgba());
    }

    bool masterInitiallyEnabled = true;

    // Mirror any existing editor auto-line state into the toggles. If the
    // editor has no saved colors, fall back to the logical default (all
    // enabled, master ON) that was initialized above.
    if (mEditor) {
        masterInitiallyEnabled = mEditor->autoLineFillEnabled();
        const QVector<QRgb>& editorColors = mEditor->autoLineColors();
        if (!editorColors.isEmpty()) {
            for (int i = 0; i < mEnabled.size(); ++i) {
                mEnabled[i] = editorColors.contains(mColorValues[i]);
            }
        }
    }
    
    // This widget should take exactly as much space as its contents
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Create box layout for the master toggle + bucket buttons. We keep a
    // pointer so we can switch between horizontal (row) and vertical (stacked)
    // orientations for the HDockLeft overlay.
    auto* layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    mLayout = layout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // Master toggle: CosmicCheckboxToggle in Custom icon-only mode, normal scale, borderless
    const int iconPixelSize = N2DUiMetrics::TOOLBAR_EMBEDDED_ICON_PX;
    const QSize iconSize(iconPixelSize, iconPixelSize);
    const QColor accent = BlenderStyleSlider::getAccentColor();

    mMasterToggle = new CosmicCheckboxToggle(this);
    mMasterToggle->setMode(CosmicCheckboxToggle::Custom);
    mMasterToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mMasterToggle->setBordered(false);
    mMasterToggle->setShowText(false);
    mMasterToggle->setShowIcon(true);
    mMasterToggle->setIconSize(iconSize);
    // Master uses no background highlight to avoid glow effect
    mMasterToggle->setBackgroundEnabled(false);
    mMasterToggle->setCheckable(true);
    mMasterToggle->setToolTip(tr("Enable/disable the entire auto-line fill feature"));
    mMasterToggle->setChecked(masterInitiallyEnabled);
    mMasterToggle->setIcon(masterInitiallyEnabled ? mOpenBucketIcon : mClosedBucketIcon);
    mMasterToggle->setAccentColor(accent);
    connect(mMasterToggle, &CosmicCheckboxToggle::toggled, this, &AutoLineColorToggle::onMasterToggled);

    layout->addWidget(mMasterToggle, 0, Qt::AlignLeft | Qt::AlignVCenter);

    // Separator between master bucket and per-color buckets. Use a plain
    // widget so we get a solid bar with our exact color.
    auto* masterSeparator = new QWidget(this);
    masterSeparator->setFixedWidth(N2DUiMetrics::TOOLBAR_SEPARATOR_THICKNESS_PX);
    masterSeparator->setFixedHeight(iconPixelSize);
    masterSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    masterSeparator->setStyleSheet("background-color:#1f1f1f;");
    mMasterSeparator = masterSeparator;
    layout->addWidget(masterSeparator, 0, Qt::AlignVCenter);

    // Create per-color bucket toggles
    mColorButtons.reserve(mColors.size());
    for (int i = 0; i < mColors.size(); ++i) {
        auto* b = new CosmicCheckboxToggle(this);
        b->setMode(CosmicCheckboxToggle::Custom);
        b->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
        b->setBordered(false);
        b->setShowText(false);
        b->setShowIcon(true);
        b->setCheckable(true);
        b->setIconSize(iconSize);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        // Do NOT override accentColor; let the Cosmic toggle use the global
        // app accent for its hover/press/checked background so the highlight
        // color is consistent with the rest of the UI. The bucket icon itself
        // remains tinted per-color via makeTintedIcon.
        // Remove background highlight to avoid glow effect
        b->setBackgroundEnabled(false);
        b->setAccentColor(accent);
        layout->addWidget(b, 0, Qt::AlignLeft | Qt::AlignVCenter);
        mColorButtons.push_back(b);

        const int colorIndex = i;
        connect(b, &QAbstractButton::clicked, this, [this, colorIndex, b]() {
            if (!mMasterToggle || !mMasterToggle->isChecked()) {
                return; // when master is off, individual presets are read-only
            }
            if (colorIndex < 0 || colorIndex >= mEnabled.size()) {
                return;
            }
            mEnabled[colorIndex] = !mEnabled[colorIndex];
            updateEnabledColors();
            emit colorToggled(colorIndex, mEnabled[colorIndex]);
            // Immediately refresh only this button's visuals so the UI feels
            // snappy without re-tinting every bucket icon on each click.
            const bool masterOn = mMasterToggle && mMasterToggle->isChecked();
            if (colorIndex >= 0 && colorIndex < mColorButtons.size()) {
                auto* btn = mColorButtons[colorIndex];
                if (btn) {
                    const bool colorEnabled = mEnabled[colorIndex];
                    const bool useOpenShape = masterOn && colorEnabled;
                    const QIcon& baseIcon = useOpenShape ? mOpenBucketIcon : mClosedBucketIcon;
                    QIcon icon;
                    if (colorIndex >= 0 && colorIndex < mColors.size()) {
                        QSize sz = btn->iconSize();
                        icon = makeTintedIcon(baseIcon, mColors[colorIndex], sz);
                    } else {
                        icon = baseIcon;
                    }
                    btn->setEnabled(masterOn);
                    btn->setChecked(false);
                    btn->setIcon(icon);
                    btn->update();
                }
            }
            // If this color was just turned ON, show a short Cosmic
            // background pulse using its accent color, then return to
            // icon-only.
            const bool nowEnabled = (colorIndex >= 0 && colorIndex < mEnabled.size()) ? mEnabled[colorIndex] : false;
            if (nowEnabled && b) {
                // Briefly show the Cosmic "checked" highlight using the
                // button's accent color, then return to unchecked so the
                // bucket remains icon-only in its resting state.
                b->setChecked(true);
                b->update();
                QTimer::singleShot(200, b, [b]() {
                    b->setChecked(false);
                    b->update();
                });
            }
        });
    }
    
    setLayout(layout);
    setMouseTracking(false);

    // Make sure the visual state matches the logical one at startup without
    // overwriting the editor's persisted configuration.
    onMasterToggled(masterInitiallyEnabled);
}

QSize AutoLineColorToggle::sizeHint() const
{
    return QWidget::sizeHint(); // layout-based sizing
}

void AutoLineColorToggle::paintEvent(QPaintEvent* e)
{
    QWidget::paintEvent(e); // all drawing handled by child buttons and BlenderStyleToggle
}

void AutoLineColorToggle::setVerticalLayoutEnabled(bool vertical)
{
    if (!mLayout || !mMasterToggle || mColorButtons.isEmpty()) {
        return;
    }

    // Rebuild the layout order without destroying widgets. Logical order:
    // [master][separator][color0..N-1].
    while (QLayoutItem* item = mLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->setParent(this);
        }
        delete item;
    }

    if (!vertical) {
        // Horizontal row: master, vertical separator, then per-color buckets.
        mLayout->setDirection(QBoxLayout::LeftToRight);

        mLayout->addWidget(mMasterToggle, 0, Qt::AlignLeft | Qt::AlignVCenter);

        if (mMasterSeparator) {
            mMasterSeparator->setFixedWidth(N2DUiMetrics::TOOLBAR_SEPARATOR_THICKNESS_PX);
            mMasterSeparator->setFixedHeight(mMasterToggle->iconSize().height());
            mMasterSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            mLayout->addWidget(mMasterSeparator, 0, Qt::AlignVCenter);
        }

        for (CosmicCheckboxToggle* b : qAsConst(mColorButtons)) {
            if (b) {
                mLayout->addWidget(b, 0, Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
    } else {
        // Vertical stack: one row per entry, with a horizontal bar directly
        // beneath the master bucket.
        mLayout->setDirection(QBoxLayout::TopToBottom);

        mLayout->addWidget(mMasterToggle, 0, Qt::AlignLeft | Qt::AlignVCenter);

        if (mMasterSeparator) {
            mMasterSeparator->setFixedHeight(N2DUiMetrics::TOOLBAR_SEPARATOR_THICKNESS_PX);
            mMasterSeparator->setFixedWidth(mMasterToggle->iconSize().width());
            mMasterSeparator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            mLayout->addWidget(mMasterSeparator, 0, Qt::AlignLeft | Qt::AlignVCenter);
        }

        for (CosmicCheckboxToggle* b : qAsConst(mColorButtons)) {
            if (b) {
                mLayout->addWidget(b, 0, Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
    }

    mLayout->invalidate();
    updateGeometry();
    update();
}

void AutoLineColorToggle::detachForExternalLayout()
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

// Manual mouse handling and hit testing removed; handled by child circle buttons

void AutoLineColorToggle::setColorEnabled(int index, bool enabled)
{
    if (index >= 0 && index < mEnabled.size()) {
        mEnabled[index] = enabled;
        updateEnabledColors();
        // Refresh icons and check states so this external change is reflected
        onMasterToggled(isMasterEnabled());
    }
}

bool AutoLineColorToggle::isColorEnabled(int index) const
{
    if (index >= 0 && index < mEnabled.size()) {
        return mEnabled[index];
    }
    return false;
}

void AutoLineColorToggle::updateEnabledColors()
{
    if (!mEditor) return;
    
    QVector<QRgb> enabledColors;
    for (int i = 0; i < mEnabled.size(); ++i) {
        if (mEnabled[i]) {
            enabledColors.append(mColorValues[i]);
        }
    }
    
    mEditor->setAutoLineColors(enabledColors);
}

void AutoLineColorToggle::onMasterToggled(bool enabled)
{
    if (mEditor) {
        mEditor->setAutoLineFillEnabled(enabled);
    }
    // Enable/disable color buttons and update visuals
    if (mMasterToggle) {
        mMasterToggle->setIcon(enabled ? mOpenBucketIcon : mClosedBucketIcon);
    }

    for (int i = 0; i < mColorButtons.size(); ++i) {
        auto* b = mColorButtons[i];
        if (!b) continue;
        b->setEnabled(enabled);

        const bool colorEnabled = (i >= 0 && i < mEnabled.size()) ? mEnabled[i] : false;
        const bool effectiveOn = enabled && colorEnabled;
        Q_UNUSED(effectiveOn);
        // Keep per-color buckets visually icon-driven in steady state: do not
        // leave them in a persistent "checked" state. Checked will only be
        // used briefly for the one-shot selection pulse when a color is
        // turned ON.
        b->setChecked(false);

        bool useOpenShape = enabled && colorEnabled;
        const QIcon& baseIcon = useOpenShape ? mOpenBucketIcon : mClosedBucketIcon;

        QIcon icon;
        if (i >= 0 && i < mColors.size()) {
            QSize sz = b->iconSize();
            icon = makeTintedIcon(baseIcon, mColors[i], sz);
        } else {
            icon = baseIcon;
        }
        b->setIcon(icon);
        b->update();
    }
}

void AutoLineColorToggle::setMasterEnabled(bool enabled)
{
    if (mMasterToggle) {
        mMasterToggle->setChecked(enabled);
    }
    onMasterToggled(enabled);
}

bool AutoLineColorToggle::isMasterEnabled() const
{
    return mMasterToggle ? mMasterToggle->isChecked() : false;
}
