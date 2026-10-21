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
#include "canvaspaletteoverlay.h"

#include "../CUI/cosmicuimetrics.h"

#include <QPainter>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QFrame>
#include <QStyleOption>
#include <QPainterPath>
#include <QRegion>
#include <QDebug>
#include <QEvent>
#include <QContextMenuEvent>
#include <QWidgetAction>
#include <QLabel>
#include <QMenu>
#include <QSettings>

#include "pencolorpalette.h"
#include "autolinecolortoggle.h"
#include "editor.h"
#include "../core_lib/src/util/pencildef.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicdropdown.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

namespace {

// Custom expand/collapse toggle for the overlay header: draws a flat, solid
// background bar (color #1f1f1f) with no hover / press / checked highlights;
// the Show/Hide icons alone indicate state.
class OverlayExpandToggle : public CosmicCheckboxToggle
{
public:
    explicit OverlayExpandToggle(QWidget* parent = nullptr)
        : CosmicCheckboxToggle(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        // No background - minimalist icon-only design
        // Just draw the centered icon
        QRect r = rect();
        if (!icon().isNull()) {
            QSize is = iconSize();
            if (is.isEmpty()) {
                const int s = qMin(r.width(), r.height()) - 4;
                is = QSize(qMax(8, s), qMax(8, s));
            }
            const int x = r.x() + (r.width() - is.width()) / 2;
            const int y = r.y() + (r.height() - is.height()) / 2;
            QRect iconRect(x, y, is.width(), is.height());
            icon().paint(&p, iconRect);
        }
    }
};

} // anonymous namespace

// Custom context menu for the canvas palette overlay — plain QMenu with
// Cosmic styling applied inline. No CosmicContextMenu dependency.
class OverlayOptionsMenu : public QMenu
{
public:
    explicit OverlayOptionsMenu(QWidget* parent = nullptr)
        : QMenu(parent)
    {
        const QColor inner(0x2f, 0x2f, 0x2f);
        const QColor border = CosmicColor::popupBorder();
        const QColor accent = BlenderStyleSlider::getAccentColor();
        setStyleSheet(QString(
            "QMenu { background:%1; border:2px solid %2; padding:4px 0; font-size:13px; }"
            "QMenu::item { background:transparent; padding:6px 12px; min-height:28px; }"
            "QMenu::item:selected { background:%3; color:#fff; }"
            "QMenu::separator { height:1px; background:%2; margin:4px 0; }"
        ).arg(inner.name(), border.name(), accent.name()));
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        mHadOpenDropdownOnPress = false;
        if (event && event->button() == Qt::LeftButton) {
            const QList<CosmicDropDown*> dropdowns = findChildren<CosmicDropDown*>();
            for (CosmicDropDown* dd : dropdowns) {
                if (dd && dd->isPopupVisible()) { mHadOpenDropdownOnPress = true; break; }
            }
        }
        QMenu::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event && event->button() == Qt::LeftButton && mHadOpenDropdownOnPress) {
            mHadOpenDropdownOnPress = false;
            QWidget::mouseReleaseEvent(event);
            return;
        }
        mHadOpenDropdownOnPress = false;
        QMenu::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        const QList<CosmicDropDown*> dropdowns = findChildren<CosmicDropDown*>();
        for (CosmicDropDown* dd : dropdowns) {
            if (dd && dd->isPopupVisible()) { QMenu::leaveEvent(event); return; }
        }
        QMenu::leaveEvent(event);
        close();
    }

private:
    bool mHadOpenDropdownOnPress = false;
};

CanvasPaletteOverlay::CanvasPaletteOverlay(Editor* editor, QWidget* parent)
    : QWidget(parent)
    , mEditor(editor)
{
    if (!parent) {
        qWarning() << "CanvasPaletteOverlay: Created without parent widget";
    } else {
        parent->installEventFilter(this);
    }

    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);

    // Restore persisted dock/orientation/swap settings. On a true first run
    // (no keys stored yet) seed them with sensible defaults and persist them
    // so future launches use the user's choices instead of hard-coded values.
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.beginGroup("CanvasPaletteOverlay");

        QVariant dockVar = settings.value("DockLocation");
        QVariant orientVar = settings.value("Orientation");
        QVariant swapVar = settings.value("SwapLayout");
        QVariant expandedVar = settings.value("Expanded");

        bool expanded = true;

        const bool hasAny = dockVar.isValid() || orientVar.isValid() || swapVar.isValid();
        if (!hasAny) {
            // First run (or first time this feature exists): use the desired
            // initial layout and persist it once.
            mDockLocation = DockLocation::Right;
            mOrientation = Orientation::Vertical;
            mSwapLayout = true;
            expanded = true;

            settings.setValue("DockLocation", static_cast<int>(mDockLocation));
            settings.setValue("Orientation", static_cast<int>(mOrientation));
            settings.setValue("SwapLayout", mSwapLayout);
            settings.setValue("Expanded", expanded);
        } else {
            if (dockVar.isValid()) {
                const int v = dockVar.toInt();
                mDockLocation = (v == static_cast<int>(DockLocation::Right))
                                    ? DockLocation::Right
                                    : DockLocation::Left;
            }
            if (orientVar.isValid()) {
                const int v = orientVar.toInt();
                mOrientation = (v == static_cast<int>(Orientation::Vertical))
                                    ? Orientation::Vertical
                                    : Orientation::Horizontal;
            }
            if (swapVar.isValid()) {
                mSwapLayout = swapVar.toBool();
            }
            if (expandedVar.isValid()) {
                expanded = expandedVar.toBool();
            }
        }

        settings.endGroup();

        // Apply persisted expanded/collapsed state to the toggle after it is created.
        // (mExpandToggle is constructed just below this block.)
        // We defer actually showing/hiding children until after the layout is built.
        mHeaderHeight = mHeaderHeight; // no-op to keep compiler happy if unused elsewhere
        // We'll set the real checked state once the toggle exists.
        // For now, stash the value in a local and use it immediately after creation.
        // expanded is used below when configuring mExpandToggle.
        if (!expanded) {
            // We'll set the toggle's checked state to 'expanded' below and
            // then let the toggled handler handle child visibility.
        }
    }

    mPalette = new PenColorPalette(this);
    if (!mPalette) {
        qCritical() << "CanvasPaletteOverlay: Failed to create PenColorPalette";
        return;
    }

    // Create main layout for the overlay dock (HDockLeft-style panel)
    auto* layout = new QVBoxLayout(this);
    mMainLayout = layout;
    if (!layout) {
        qCritical() << "CanvasPaletteOverlay: Failed to create layout";
        delete mPalette;
        mPalette = nullptr;
        return;
    }

    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Expand / collapse toggle for the auto-line controls. This row is always
    // visible; it uses a flat overlay-specific toggle with Show/Hide icons and
    // stretches to match the panel width.
    mExpandToggle = new OverlayExpandToggle(this);
    mExpandToggle->setMode(CosmicCheckboxToggle::Custom);
    // Thin horizontal bar: normal scale, fixed small height, full-width.
    mExpandToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mExpandToggle->setBordered(false);
    mExpandToggle->setShowText(false);
    mExpandToggle->setShowIcon(true);
    mExpandToggle->setBackgroundEnabled(false);
    mExpandToggle->setCheckable(true);
    mExpandToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mExpandToggle->setIconSize(QSize(16, 16));
    mExpandToggle->setFixedHeight(22);

    const QIcon showIcon(":/Interaction/Show.svg");
    const QIcon hideIcon(":/Interaction/Hide.svg");

    auto updateExpandIcon = [this, showIcon, hideIcon](bool expanded) {
        if (!mExpandToggle)
            return;
        // When expanded, show the "Show" glyph (panel is currently shown);
        // when collapsed, show the "Hide" glyph to indicate it is hidden.
        mExpandToggle->setIcon(expanded ? showIcon : hideIcon);
    };
    updateExpandIcon(true);

    // Let the layout control the full available width using the expanding
    // horizontal size policy. No explicit alignment so it stretches across
    // the dock instead of remaining a square.
    layout->addWidget(mExpandToggle);

    // Content container holds auto-line controls, separator and pen palette.
    mContentContainer = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(mContentContainer);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);
    mContentLayout = contentLayout;

    // Top row: auto-line fill controls container
    mControlsContainer = new QWidget(mContentContainer);
    mControlsContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* controlsLayout = new QHBoxLayout(mControlsContainer);
    controlsLayout->setContentsMargins(8, 4, 8, 4);
    controlsLayout->setSpacing(6);
    mControlsLayout = controlsLayout;
    mControlsContainer->setLayout(controlsLayout);

    if (mEditor) {
        mAutoLineToggle = new AutoLineColorToggle(mEditor, mControlsContainer);
        controlsLayout->addWidget(mAutoLineToggle, 0, Qt::AlignLeft | Qt::AlignVCenter);
    }

    // Bottom row: pen color palette container
    mPenContainer = new QWidget(mContentContainer);
    mPenContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* penLayout = new QHBoxLayout(mPenContainer);
    penLayout->setContentsMargins(8, 4, 8, 4);
    penLayout->setSpacing(6);
    penLayout->addWidget(mPalette, 0, Qt::AlignLeft | Qt::AlignVCenter);
    mPenLayout = penLayout;

    // Separator between the bucket row and the pen row. Use a plain widget so
    // the style system can't override the color (no QFrame HLine).
    mRowSeparator = new QWidget(this);
    mRowSeparator->setFixedHeight(4);
    mRowSeparator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mRowSeparator->setStyleSheet("background-color:#1f1f1f;");

    contentLayout->addWidget(mControlsContainer, 0, Qt::AlignLeft | Qt::AlignTop);
    contentLayout->addWidget(mRowSeparator);
    contentLayout->addWidget(mPenContainer, 0, Qt::AlignLeft | Qt::AlignTop);

    layout->addWidget(mContentContainer);

    // Wire expand/collapse behavior: toggle shows/hides the auto-line controls
    // row and its separator, but the toggle itself remains visible.
    connect(mExpandToggle, &QAbstractButton::toggled, this,
            [this, updateExpandIcon](bool expanded) {
                // Remember the current width so collapsing keeps the same
                // horizontal size. This runs before updatePosition adjusts
                // geometry.
                mExpandedWidth = width();
                if (mControlsContainer) {
                    mControlsContainer->setVisible(expanded);
                }
                if (mRowSeparator) {
                    mRowSeparator->setVisible(expanded);
                }
                if (mPenContainer) {
                    mPenContainer->setVisible(expanded);
                }
                // Persist expanded/collapsed state
                {
                    QSettings settings(PENCIL2D, PENCIL2D);
                    settings.beginGroup("CanvasPaletteOverlay");
                    settings.setValue("Expanded", expanded);
                    settings.endGroup();
                }
                updateExpandIcon(expanded);
                updatePosition();
            });

    // Restore persisted expanded/collapsed state (default: expanded)
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.beginGroup("CanvasPaletteOverlay");
        const bool expanded = settings.value("Expanded", true).toBool();
        settings.endGroup();
        mExpandToggle->setChecked(expanded);
    }

    // Forward palette color changes
    connect(mPalette, &PenColorPalette::colorChanged, this, &CanvasPaletteOverlay::colorChanged);

    // Allow right-click anywhere on the overlay or on the expand toggle to
    // open the options menu.
    auto connectContextMenu = [this](QWidget* w) {
        if (!w) return;
        w->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(w, &QWidget::customContextMenuRequested, this,
                         [this, w](const QPoint& pos) {
                             showOptionsMenu(w->mapToGlobal(pos));
                         });
    };
    connectContextMenu(this);
    connectContextMenu(mExpandToggle);

    int hintHeight = layout->sizeHint().height();
    if (hintHeight > 0) {
        const int extra = 8;
        if (hintHeight + extra > mHeaderHeight) {
            mHeaderHeight = hintHeight + extra;
        }
    }

    // Set initial orientation and size/position based on contents. Make sure
    // the child widgets (buckets and pens) also start in the correct
    // horizontal/vertical layout.
    const bool vertical = (mOrientation == Orientation::Vertical);
    if (mAutoLineToggle) {
        mAutoLineToggle->setVerticalLayoutEnabled(vertical);
    }
    if (mPalette) {
        mPalette->setVerticalLayoutEnabled(vertical);
    }

    applyOrientation();
    resize(layout->sizeHint());
    updatePosition();
}

void CanvasPaletteOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor bg(0x2f, 0x2f, 0x2f, 0xff);      // inner background 2f2f2fff
    const QColor borderColor = CosmicColor::popupBorder();   // light grey border

    // Single rectangular dock container for the HDockLeft UI
    // Draw border slightly inset so a thicker stroke is fully visible.
    QRect panelRect = rect().adjusted(2, 2, -3, -3);
    
    // Modern macOS-style squircle radius - proportional to widget size
    const qreal radius = 16.0; // Modern squircle corner radius

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(panelRect, radius, radius);

    p.setPen(QPen(borderColor, 3.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(panelRect, radius, radius);
}

void CanvasPaletteOverlay::resizeEvent(QResizeEvent*)
{
    updatePosition();
}

void CanvasPaletteOverlay::contextMenuEvent(QContextMenuEvent* event)
{
    if (!event) {
        return;
    }
    showOptionsMenu(event->globalPos());
}

bool CanvasPaletteOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(watched, event);
}

void CanvasPaletteOverlay::updatePosition()
{
    if (!parentWidget()) {
        qWarning() << "CanvasPaletteOverlay: No parent widget for positioning";
        return;
    }
    QWidget* parent = parentWidget();
    if (parent->width() <= 0 || parent->height() <= 0) {
        qWarning() << "CanvasPaletteOverlay: Parent has invalid dimensions";
        return;
    }

    // Position the dock as a compact panel in the canvas with a small inset.
    const int defaultInset = 16;
    int leftReserved = 0;
    int rightReserved = 0;
    int topReserved = 0;
    {
        const QVariant lv = parent->property("CosmicLeftReserved");
        const QVariant rv = parent->property("CosmicRightReserved");
        const QVariant tv = parent->property("CosmicTopReserved");
        if (lv.isValid()) {
            leftReserved = qMax(0, lv.toInt());
        }
        if (rv.isValid()) {
            rightReserved = qMax(0, rv.toInt());
        }
        if (tv.isValid()) {
            topReserved = qMax(0, tv.toInt());
        }
    }
    // Fallback when no dynamic properties exist.
    if (leftReserved <= 0 || rightReserved <= 0) {
        const int cosmicBarPad = 2;
        const int cosmicBarW = 22;
        const int reservedForCosmicBar = cosmicBarPad * 2 + cosmicBarW;
        if (leftReserved <= 0) leftReserved = reservedForCosmicBar;
        if (rightReserved <= 0) rightReserved = reservedForCosmicBar;
    }

    const int offsetLeft = qMax(defaultInset, leftReserved);
    const int offsetRight = qMax(defaultInset, rightReserved);
    const int offsetY = qMax(defaultInset, topReserved + defaultInset);

    QSize hint = sizeHint();
    int w = hint.width();
    // When collapsed, keep the last known expanded width so the dock does not
    // shrink horizontally; orientation changes will still clamp against the
    // current parent width below.
    if (mExpandToggle && !mExpandToggle->isChecked() && mExpandedWidth > 0) {
        w = mExpandedWidth;
    }
    int h = hint.height();
    if (w <= 0) {
        w = 200;
    }
    if (h <= 0) {
        h = mHeaderHeight;
    }

    // Clamp so the panel always fits inside the parent
    w = qMin(w, parent->width() - offsetLeft - offsetRight);
    h = qMin(h, parent->height() - offsetY - defaultInset);

    int x = offsetLeft;
    if (mDockLocation == DockLocation::Right) {
        x = parent->width() - w - offsetRight;
        if (x < offsetLeft) {
            x = offsetLeft;
        }
    }

    setGeometry(x, offsetY, w, h);
    raise();

    // The panel bounds already match the interactive area; no extra masking required.
    clearMask();
}

void CanvasPaletteOverlay::applyOrientation()
{
    if (!mContentLayout || !mControlsContainer || !mPenContainer || !mRowSeparator) {
        return;
    }

    // Remove all widgets from the content layout without deleting them so we
    // can reinsert in the desired order/orientation.
    while (QLayoutItem* item = mContentLayout->takeAt(0)) {
        // We only care about keeping the widgets alive; Qt will re-parent them
        // via addWidget below.
        if (item->widget()) {
            item->widget()->setParent(mContentContainer);
        }
        delete item;
    }

    // Decide ordering based on swap-layout flag: by default the buckets
    // (controls) are on top/left and pens on bottom/right; when swapped we
    // invert that relationship.
    QWidget* first = mSwapLayout ? mPenContainer : mControlsContainer;
    QWidget* second = mSwapLayout ? mControlsContainer : mPenContainer;

    if (mOrientation == Orientation::Horizontal) {
        mContentLayout->setDirection(QBoxLayout::TopToBottom);
        if (mControlsLayout) {
            mControlsLayout->setDirection(QBoxLayout::LeftToRight);
        }
        if (mPenLayout) {
            mPenLayout->setDirection(QBoxLayout::LeftToRight);
        }
        // Horizontal bar spanning the full width between the two rows.
        mRowSeparator->setMinimumWidth(0);
        mRowSeparator->setMaximumWidth(QWIDGETSIZE_MAX);
        mRowSeparator->setFixedHeight(4);
        mRowSeparator->setMinimumHeight(4);
        mRowSeparator->setMaximumHeight(4);
        mRowSeparator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        mContentLayout->addWidget(first, 0, Qt::AlignLeft | Qt::AlignTop);
        mContentLayout->addWidget(mRowSeparator);
        mContentLayout->addWidget(second, 0, Qt::AlignLeft | Qt::AlignTop);
    } else {
        // Vertical mode: arrange buckets and pens as two columns with a
        // vertical separator between them.
        mContentLayout->setDirection(QBoxLayout::LeftToRight);
        if (mControlsLayout) {
            mControlsLayout->setDirection(QBoxLayout::TopToBottom);
        }
        if (mPenLayout) {
            mPenLayout->setDirection(QBoxLayout::TopToBottom);
        }
        // Vertical bar spanning the full height between the two stacks.
        mRowSeparator->setFixedWidth(4);
        mRowSeparator->setMinimumWidth(4);
        mRowSeparator->setMaximumWidth(4);
        mRowSeparator->setMinimumHeight(0);
        mRowSeparator->setMaximumHeight(QWIDGETSIZE_MAX);
        mRowSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        mContentLayout->addWidget(first, 0, Qt::AlignLeft | Qt::AlignTop);
        mContentLayout->addWidget(mRowSeparator);
        mContentLayout->addWidget(second, 0, Qt::AlignLeft | Qt::AlignTop);
    }

    updateGeometry();
    update();
    updatePosition();
}

void CanvasPaletteOverlay::showOptionsMenu(const QPoint& globalPos)
{
    OverlayOptionsMenu menu(this);

    // Swap layout row (first entry): dual layout with 50/50 split. Left area
    // holds the label (left aligned), right area holds the checkbox (left
    // aligned, fixed width so it does not expand).
    QWidget* swapRow = new QWidget(&menu);
    QHBoxLayout* swapLayout = new QHBoxLayout(swapRow);
    swapLayout->setContentsMargins(8, 4, 8, 4);
    swapLayout->setSpacing(8);

    QWidget* swapLeft = new QWidget(swapRow);
    QHBoxLayout* swapLeftLayout = new QHBoxLayout(swapLeft);
    swapLeftLayout->setContentsMargins(0, 0, 0, 0);
    swapLeftLayout->setSpacing(4);

    QWidget* swapRight = new QWidget(swapRow);
    QHBoxLayout* swapRightLayout = new QHBoxLayout(swapRight);
    swapRightLayout->setContentsMargins(0, 0, 0, 0);
    swapRightLayout->setSpacing(4);

    QLabel* swapLabel = new QLabel(tr("Swap layout"), swapLeft);
    swapLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e6e6e6; background: transparent; font-size: 13px; }"));
    swapLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    CosmicCheckboxToggle* swapToggle = new CosmicCheckboxToggle(swapRight);
    swapToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    swapToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    swapToggle->setCheckable(true);
    swapToggle->setChecked(mSwapLayout);
    swapToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    swapLeftLayout->addWidget(swapLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    swapLeftLayout->addStretch(1);

    swapRightLayout->addWidget(swapToggle, 0, Qt::AlignLeft | Qt::AlignVCenter);
    swapRightLayout->addStretch(1);

    swapLayout->addWidget(swapLeft, 1);
    swapLayout->addWidget(swapRight, 1);
    swapRow->setLayout(swapLayout);

    QWidgetAction* swapAction = new QWidgetAction(&menu);
    swapAction->setDefaultWidget(swapRow);
    menu.addAction(swapAction);

    QObject::connect(swapToggle, &QAbstractButton::toggled, this,
                     [this](bool on) {
                         mSwapLayout = on;

                         QSettings settings(PENCIL2D, PENCIL2D);
                         settings.beginGroup("CanvasPaletteOverlay");
                         settings.setValue("SwapLayout", mSwapLayout);
                         settings.endGroup();

                         applyOrientation();
                     });

    // Dock location row: dual layout 50/50 with left-aligned label and
    // expanding dropdown in the right half.
    QWidget* dockRow = new QWidget(&menu);
    QHBoxLayout* dockLayout = new QHBoxLayout(dockRow);
    dockLayout->setContentsMargins(8, 4, 8, 4);
    dockLayout->setSpacing(8);

    QWidget* dockLeft = new QWidget(dockRow);
    QHBoxLayout* dockLeftLayout = new QHBoxLayout(dockLeft);
    dockLeftLayout->setContentsMargins(0, 0, 0, 0);
    dockLeftLayout->setSpacing(4);

    QWidget* dockRight = new QWidget(dockRow);
    QHBoxLayout* dockRightLayout = new QHBoxLayout(dockRight);
    dockRightLayout->setContentsMargins(0, 0, 0, 0);
    dockRightLayout->setSpacing(4);

    QLabel* dockLabel = new QLabel(tr("Dock location"), dockLeft);
    dockLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e6e6e6; background: transparent; font-size: 13px; }"));
    dockLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    CosmicDropDown* dockDropDown = new CosmicDropDown(dockRight);
    dockDropDown->setOptions(QStringList() << tr("Left") << tr("Right"));
    dockDropDown->setCurrentIndex(mDockLocation == DockLocation::Left ? 0 : 1);
    dockDropDown->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    dockLeftLayout->addWidget(dockLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    dockLeftLayout->addStretch(1);

    dockRightLayout->addWidget(dockDropDown, 1, Qt::AlignLeft | Qt::AlignVCenter);

    dockLayout->addWidget(dockLeft, 1);
    dockLayout->addWidget(dockRight, 1);
    dockRow->setLayout(dockLayout);

    QWidgetAction* dockAction = new QWidgetAction(&menu);
    dockAction->setDefaultWidget(dockRow);
    menu.addAction(dockAction);

    QObject::connect(dockDropDown, &CosmicDropDown::activated, this,
                     [this](int index) {
                         mDockLocation = (index == 1) ? DockLocation::Right : DockLocation::Left;
                         QSettings settings(PENCIL2D, PENCIL2D);
                         settings.beginGroup("CanvasPaletteOverlay");
                         settings.setValue("DockLocation", static_cast<int>(mDockLocation));
                         settings.endGroup();
                         updatePosition();
                     });

    // Orientation row: same dual layout pattern as Dock location.
    QWidget* orientRow = new QWidget(&menu);
    QHBoxLayout* orientLayout = new QHBoxLayout(orientRow);
    orientLayout->setContentsMargins(8, 4, 8, 4);
    orientLayout->setSpacing(8);

    QWidget* orientLeft = new QWidget(orientRow);
    QHBoxLayout* orientLeftLayout = new QHBoxLayout(orientLeft);
    orientLeftLayout->setContentsMargins(0, 0, 0, 0);
    orientLeftLayout->setSpacing(4);

    QWidget* orientRight = new QWidget(orientRow);
    QHBoxLayout* orientRightLayout = new QHBoxLayout(orientRight);
    orientRightLayout->setContentsMargins(0, 0, 0, 0);
    orientRightLayout->setSpacing(4);

    QLabel* orientLabel = new QLabel(tr("Orientation"), orientLeft);
    orientLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e6e6e6; background: transparent; font-size: 13px; }"));
    orientLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    CosmicDropDown* orientDropDown = new CosmicDropDown(orientRight);
    orientDropDown->setOptions(QStringList() << tr("Horizontal") << tr("Vertical"));
    orientDropDown->setCurrentIndex(mOrientation == Orientation::Horizontal ? 0 : 1);
    orientDropDown->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    orientLeftLayout->addWidget(orientLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    orientLeftLayout->addStretch(1);

    orientRightLayout->addWidget(orientDropDown, 1, Qt::AlignLeft | Qt::AlignVCenter);

    orientLayout->addWidget(orientLeft, 1);
    orientLayout->addWidget(orientRight, 1);
    orientRow->setLayout(orientLayout);

    QWidgetAction* orientAction = new QWidgetAction(&menu);
    orientAction->setDefaultWidget(orientRow);
    menu.addAction(orientAction);

    QObject::connect(orientDropDown, &CosmicDropDown::activated, this,
                     [this](int index) {
                         mOrientation = (index == 1) ? Orientation::Vertical
                                                    : Orientation::Horizontal;
                         const bool vertical = (mOrientation == Orientation::Vertical);
                         // Propagate orientation into child widgets so each
                         // side (buckets and pens) can reflow its own rows.
                         if (mAutoLineToggle) {
                             mAutoLineToggle->setVerticalLayoutEnabled(vertical);
                         }
                         if (mPalette) {
                             mPalette->setVerticalLayoutEnabled(vertical);
                         }

                         QSettings settings(PENCIL2D, PENCIL2D);
                         settings.beginGroup("CanvasPaletteOverlay");
                         settings.setValue("Orientation", static_cast<int>(mOrientation));
                         settings.endGroup();
                         applyOrientation();
                     });

    menu.exec(globalPos);
}

bool CanvasPaletteOverlay::isPresetColor(const QColor& color) const
{
    if (!mPalette) {
        qWarning() << "CanvasPaletteOverlay: mPalette is null";
        return false;
    }
    return mPalette->isPresetColor(color);
}

void CanvasPaletteOverlay::setColorFromExternal(const QColor& color)
{
    if (!mPalette) {
        qWarning() << "CanvasPaletteOverlay: mPalette is null";
        return;
    }
    if (!color.isValid()) {
        qWarning() << "CanvasPaletteOverlay: Invalid color provided";
        return;
    }
    // Block forwarding to avoid feedback loops with ColorManager::colorChanged
    QSignalBlocker blocker(this);
    // If currently on any of the 5 preset buttons and an external color is set,
    // switch to the first (custom) button and apply the color. Otherwise just set current.
    if (mPalette->getActiveButtonIndex() > 0) {
        mPalette->activateFirstButtonAndSetColor(color);
    } else {
        mPalette->setCurrentColor(color);
    }
}


