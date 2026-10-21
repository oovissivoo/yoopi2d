/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2025 Pencil2D Contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "shifttracewidget.h"

#include <functional>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSettings>

#include "editor.h"
#include "util.h"
#include "structure/object.h"
#include "structure/layer.h"
#include "managers/shifttracemanager.h"
#include "managers/layermanager.h"
#include "scribblearea.h"

#include "pencildef.h"
#include "../CUI/blenderstyletoggle.h"
#include "../CUI/cosmicvslider.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicdropdown.h"
#include "cosmiccolorwheelwidget.h"
#include "popupcolorwheelwidget.h"

namespace {
static constexpr int kThumbW = 64;
static constexpr int kThumbH = 48;
static constexpr int kShiftTraceSlotItemW = 20;
static constexpr int kShiftTraceSlotCornerRadius = 14;

static const char kShiftTracePrevTintColorKey[] = "ShiftTracePrevTintColor";
static const char kShiftTraceNextTintColorKey[] = "ShiftTraceNextTintColor";
static const char kShiftTracePrevTintEnabledKey[] = "ShiftTracePrevTintEnabled";
static const char kShiftTraceNextTintEnabledKey[] = "ShiftTraceNextTintEnabled";
static const char kShiftTracePrevChainOpacityKey[] = "ShiftTracePrevChainOpacity";
static const char kShiftTraceNextChainOpacityKey[] = "ShiftTraceNextChainOpacity";

// Compute display label for a slot index such that:
// - Left group (indices < mid) counts down toward the center
// - Right group (indices >= mid) counts up away from the center
inline int slotLabelForIndex(int slotIndex)
{
    const int maxEntries = ShiftTraceManager::kMaxEntries;
    const int mid = maxEntries / 2;
    if (slotIndex < mid) {
        // Left side: descending toward center (e.g. 3,2,1 for 6 slots)
        return mid - slotIndex;
    }
    // Right side: ascending away from center (e.g. 1,2,3 for 6 slots)
    return slotIndex - mid + 1;
}

inline QPixmap dimmedPixmap(const QPixmap& src, qreal opacity)
{
    if (src.isNull()) return QPixmap();
    QPixmap dst(src.size());
    dst.fill(Qt::transparent);
    QPainter p(&dst);
    p.setOpacity(qBound<qreal>(0.0, opacity, 1.0));
    p.drawPixmap(0, 0, src);
    return dst;
}

inline QPixmap tintedPixmap(const QPixmap& src, const QColor& tint)
{
    if (src.isNull()) return QPixmap();
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(img.rect(), tint);
    return QPixmap::fromImage(img);
}

inline QIcon iconWithDimmedDisabledState(const QString& resourcePath, qreal disabledOpacity)
{
    QIcon icon(resourcePath);
    const QSize renderSize(18, 18);
    QPixmap base = icon.pixmap(renderSize, QIcon::Normal, QIcon::Off);
    if (!base.isNull()) {
        const int a = qBound(0, qRound(255.0 * disabledOpacity), 255);
        const QColor disabledTint(220, 64, 64, a);
        QPixmap disabledPx = tintedPixmap(base, disabledTint);
        icon.addPixmap(disabledPx, QIcon::Disabled, QIcon::Off);
        icon.addPixmap(disabledPx, QIcon::Disabled, QIcon::On);
    }
    return icon;
}
}

static const QString kColorOrange = "#FF8800";
static const QString kColorOrangeHover = "#FFA020";
static const QString kColorGray = "#666";
static const QString kColorGrayHover = "#777";
static const QString kColorGold = "#FFD700";
static const QString kColorBlue = "#4DA3FF";
static const QString kColorDarkBg = "#1a1a1a";
static const QString kColorMediumBg = "#2a2a2a";

// ---------------------------------------------------------------------------
// HoldGhost tint popup — Qt::Popup window with mode dropdown + color wheel
// ---------------------------------------------------------------------------
class HoldGhostTintPopup : public QWidget
{
public:
    explicit HoldGhostTintPopup(QWidget* parent,
                                std::function<void(const QColor&)> onColorChanged,
                                std::function<void(bool)> onModeChanged)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint)
        , mOnColorChanged(std::move(onColorChanged))
        , mOnModeChanged(std::move(onModeChanged))
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
        setMinimumWidth(220);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        mModeDropdown = new CosmicDropDown(this);
        mModeDropdown->setOptions({tr("Image"), tr("Color")});
        mModeDropdown->setCurrentIndex(0);
        mModeDropdown->setFixedHeight(26);
        root->addWidget(mModeDropdown);

        mWheel = new CosmicColorWheelWidget(this);
        mWheel->setFixedSize(210, 220);
        root->addWidget(mWheel);

        adjustSize();

        connect(mModeDropdown, &CosmicDropDown::currentIndexChanged, this, [this](int idx){
            if (mOnModeChanged) mOnModeChanged(idx == 1);
        });
        connect(mWheel, &CosmicColorWheelWidget::colorChanged, this, [this](const QColor& c){
            if (mOnColorChanged) mOnColorChanged(c);
        });

        // Auto-close on outside click via app-level event filter
        if (qApp) qApp->installEventFilter(this);
    }

    ~HoldGhostTintPopup()
    {
        if (qApp) qApp->removeEventFilter(this);
    }

    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            const QPoint gpos = static_cast<QMouseEvent*>(event)->globalPos();
            // Check if click is inside this popup or any of its children
            QWidget* clicked = qApp->widgetAt(gpos);
            bool inside = false;
            QWidget* w = clicked;
            while (w) {
                if (w == this) { inside = true; break; }
                w = w->parentWidget();
            }
            if (!inside && !frameGeometry().contains(gpos)) {
                close();
            }
        }
        return false; // never consume
    }

    void setColor(const QColor& c) { mWheel->setColor(c); }
    void setTintEnabled(bool on) { mModeDropdown->setCurrentIndex(on ? 1 : 0); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2a, 0x2a, 0x2a, 0xf0));
        p.drawRoundedRect(rect(), 8, 8);
    }

private:
    CosmicDropDown* mModeDropdown = nullptr;
    CosmicColorWheelWidget* mWheel = nullptr;
    std::function<void(const QColor&)> mOnColorChanged;
    std::function<void(bool)> mOnModeChanged;
};

ShiftTraceCircleToggle::ShiftTraceCircleToggle(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

bool ShiftTraceCircleToggle::isChecked() const
{
    return mChecked;
}

void ShiftTraceCircleToggle::setChecked(bool checked)
{
    if (mChecked == checked) {
        return;
    }
    mChecked = checked;
    update();
}

void ShiftTraceCircleToggle::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);
    p.setPen(QPen(QColor(0x99, 0x99, 0x99, 0xff), 2.0));
    p.setBrush(QColor(0x33, 0x33, 0x33, 0xff));
    p.drawEllipse(r);

    if (mChecked) {
        const qreal innerScale = 0.58;
        const qreal w = r.width() * innerScale;
        const qreal h = r.height() * innerScale;
        const QRectF inner(r.center().x() - w * 0.5, r.center().y() - h * 0.5, w, h);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x00, 0x3A, 0xFF, 0xff));
        p.drawEllipse(inner);
    }
}

void ShiftTraceCircleToggle::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        mChecked = !mChecked;
        update();
        emit toggled(mChecked);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

ShiftTraceWidget::ShiftTraceWidget(QWidget* parent)
    : BaseDockWidget(parent)
{
    setWindowTitle(tr("Shift & Trace"));
}

ShiftTraceWidget::~ShiftTraceWidget() = default;

void ShiftTraceWidget::setEmbeddedInPopup(bool on)
{
    if (mEmbeddedInPopup == on) {
        return;
    }
    mEmbeddedInPopup = on;
    applyEmbeddedPopupVisuals(on);
}

void ShiftTraceWidget::initUI()
{
    mRoot = new QWidget(this);
    setDockContentWidget(mRoot);
    mRoot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // Styling is controlled by applyEmbeddedPopupVisuals() to ensure the widget
    // can be hosted in the CosmicPopupWindow without forcing legacy colors.

    mRootLayout = new QVBoxLayout(mRoot);
    mRootLayout->setContentsMargins(6, 6, 6, 6);
    mRootLayout->setSpacing(4);

    // Header bar: [prev color] [prev checkbox] [prev All] [prev chain] | [next chain] [next All] [next checkbox] [next color]
    mHeaderBar = new QWidget(mRoot);
    auto headerLayout = new QHBoxLayout(mHeaderBar);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    mPrevTintColor = QColor(0, 0, 255);
    mNextTintColor = QColor(255, 136, 0);

    mLeftColorBox = new QWidget(mHeaderBar);
    mLeftColorBox->setFixedSize(48, 14);
    mLeftColorBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mLeftColorBox->setCursor(Qt::PointingHandCursor);
    mLeftColorBox->installEventFilter(this);
    headerLayout->addWidget(mLeftColorBox, 0, Qt::AlignVCenter);

    mPrevTintToggle = new CosmicCheckboxToggle(mHeaderBar);
    mPrevTintToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mPrevTintToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mPrevTintToggle->setBordered(true);
    mPrevTintToggle->setShowText(false);
    mPrevTintToggle->setShowIcon(false);
    mPrevTintToggle->setChecked(false);
    headerLayout->addWidget(mPrevTintToggle, 0, Qt::AlignVCenter);

    mPrevResetAllButton = new CosmicButton(mHeaderBar);
    mPrevResetAllButton->setBordered(true);
    mPrevResetAllButton->setAccentColor(BlenderStyleToggle::getAccentColor());
    mPrevResetAllButton->setText(tr("All"));
    mPrevResetAllButton->setIcon(QIcon(":/Interaction/ResetShift.svg"));
    mPrevResetAllButton->setIconSize(QSize(14, 14));
    mPrevResetAllButton->setFixedSize(52, 24);
    headerLayout->addWidget(mPrevResetAllButton, 0, Qt::AlignVCenter);

    mPrevChainOpacityToggle = new CosmicCheckboxToggle(mHeaderBar);
    mPrevChainOpacityToggle->setMode(CosmicCheckboxToggle::Custom);
    mPrevChainOpacityToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mPrevChainOpacityToggle->setBordered(true);
    mPrevChainOpacityToggle->setShowText(false);
    mPrevChainOpacityToggle->setShowIcon(true);
    mPrevChainOpacityToggle->setBaseBackgroundColor(QColor(0x50, 0x50, 0x50, 0xff));
    mPrevChainOpacityToggle->setAccentColor(QColor(0x6B, 0x46, 0xC1, 0xff));
    mPrevChainOpacityToggle->setIcon(QIcon("Interaction/ChainOpacity.svg"));
    mPrevChainOpacityToggle->setIconSize(QSize(16, 16));
    mPrevChainOpacityToggle->setChecked(false);
    mPrevChainOpacityToggle->setFixedSize(24, 24);
    headerLayout->addWidget(mPrevChainOpacityToggle, 0, Qt::AlignVCenter);

    // Add stretch to push left and right groups apart
    headerLayout->addStretch(1);

    // mHideAllToggle has been moved to the timeline transport bar as a dedicated button pair.
    // It is no longer shown inside the popup header.

    mNextChainOpacityToggle = new CosmicCheckboxToggle(mHeaderBar);
    mNextChainOpacityToggle->setMode(CosmicCheckboxToggle::Custom);
    mNextChainOpacityToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mNextChainOpacityToggle->setBordered(true);
    mNextChainOpacityToggle->setShowText(false);
    mNextChainOpacityToggle->setShowIcon(true);
    mNextChainOpacityToggle->setBaseBackgroundColor(QColor(0x50, 0x50, 0x50, 0xff));
    mNextChainOpacityToggle->setAccentColor(QColor(0x6B, 0x46, 0xC1, 0xff));
    mNextChainOpacityToggle->setIcon(QIcon("Interaction/ChainOpacity.svg"));
    mNextChainOpacityToggle->setIconSize(QSize(16, 16));
    mNextChainOpacityToggle->setChecked(false);
    mNextChainOpacityToggle->setFixedSize(24, 24);
    headerLayout->addWidget(mNextChainOpacityToggle, 0, Qt::AlignVCenter);

    mNextResetAllButton = new CosmicButton(mHeaderBar);
    mNextResetAllButton->setBordered(true);
    mNextResetAllButton->setAccentColor(BlenderStyleToggle::getAccentColor());
    mNextResetAllButton->setText(tr("All"));
    mNextResetAllButton->setIcon(QIcon(":/Interaction/ResetShift.svg"));
    mNextResetAllButton->setIconSize(QSize(14, 14));
    mNextResetAllButton->setFixedSize(52, 24);
    headerLayout->addWidget(mNextResetAllButton, 0, Qt::AlignVCenter);

    mNextTintToggle = new CosmicCheckboxToggle(mHeaderBar);
    mNextTintToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mNextTintToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mNextTintToggle->setBordered(true);
    mNextTintToggle->setShowText(false);
    mNextTintToggle->setShowIcon(false);
    mNextTintToggle->setChecked(false);
    headerLayout->addWidget(mNextTintToggle, 0, Qt::AlignVCenter);

    mRightColorBox = new QWidget(mHeaderBar);
    mRightColorBox->setFixedSize(48, 14);
    mRightColorBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mRightColorBox->setCursor(Qt::PointingHandCursor);
    mRightColorBox->installEventFilter(this);
    headerLayout->addWidget(mRightColorBox, 0, Qt::AlignVCenter);

    mRootLayout->addWidget(mHeaderBar, 0);

    auto* slotsArea = new QWidget(mRoot);
    auto* slotsAreaLayout = new QVBoxLayout(slotsArea);
    slotsAreaLayout->setContentsMargins(0, 0, 0, 0);
    slotsAreaLayout->setSpacing(0);

    mNumbersBar = new QFrame(mRoot);
    mNumbersBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mNumbersBar->setStyleSheet("background-color: rgba(17, 17, 17, 255); border: 0px; padding: 0px; border-radius: 4px;");
    mNumbersLayout = new QHBoxLayout(mNumbersBar);
    mNumbersLayout->setContentsMargins(0, 0, 0, 0);
    mNumbersLayout->setSpacing(1);
    mNumbersLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    {
        const int itemW = kShiftTraceSlotItemW;
        const int colLR = 2;
        const int colW = itemW + (2 * colLR);
        for (int slotIndex = 0; slotIndex < ShiftTraceManager::kMaxEntries; ++slotIndex) {
            if (slotIndex == ShiftTraceManager::kMaxEntries / 2) {
                QWidget* gap = new QWidget(mNumbersBar);
                gap->setFixedWidth(kShiftTraceSlotItemW);
                gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                gap->setStyleSheet("background-color: rgba(17, 17, 17, 255); border: 0px; padding: 0px;");
                mNumbersLayout->addWidget(gap);
            }

            QWidget* cell = new QWidget(mNumbersBar);
            cell->setMinimumWidth(colW);
            cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            auto* cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->setSpacing(0);

            auto* label = new QLabel(QString::number(slotLabelForIndex(slotIndex)), cell);
            label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            label->setStyleSheet("color: #b3b3b3; background: transparent;");
            cellLayout->addWidget(label);

            mNumbersLayout->addWidget(cell, 1);
        }
    }

    slotsAreaLayout->addWidget(mNumbersBar, 0);
    slotsAreaLayout->addSpacing(1);

    // Fixed-size panel: place items container directly (no scroll area)
    mScroll = nullptr;
    mScrollContents = new QWidget(mRoot);
    mScrollContents->setObjectName(QStringLiteral("ShiftTraceItemsContainer"));
    mScrollContents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mScrollContents->setStyleSheet("QWidget { background-color: #2a2a2a; }");

    mItemsLayout = new QHBoxLayout(mScrollContents);
    mItemsLayout->setContentsMargins(0, 0, 0, 0);
    mItemsLayout->setSpacing(1);
    mItemsLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    slotsAreaLayout->addWidget(mScrollContents, 1);

    // Tint footer bar — 20 colored strips aligned with slot columns
    mTintFooterBar = new QFrame(mRoot);
    mTintFooterBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mTintFooterBar->setStyleSheet("background-color: rgba(17, 17, 17, 255); border: 0px; padding: 0px; border-radius: 4px;");
    mTintFooterLayout = new QHBoxLayout(mTintFooterBar);
    mTintFooterLayout->setContentsMargins(0, 0, 0, 0);
    mTintFooterLayout->setSpacing(1);
    mTintFooterLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    {
        const int itemW = kShiftTraceSlotItemW;
        const int colLR = 2;
        const int colW = itemW + (2 * colLR);
        for (int slotIndex = 0; slotIndex < ShiftTraceManager::kMaxEntries; ++slotIndex) {
            if (slotIndex == ShiftTraceManager::kMaxEntries / 2) {
                QWidget* gap = new QWidget(mTintFooterBar);
                gap->setFixedWidth(kShiftTraceSlotItemW);
                gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                gap->setStyleSheet("background-color: rgba(17, 17, 17, 255); border: 0px; padding: 0px;");
                mTintFooterLayout->addWidget(gap);
            }

            QWidget* cell = new QWidget(mTintFooterBar);
            cell->setMinimumWidth(colW);
            cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            auto* cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(2, 2, 2, 2);
            cellLayout->setSpacing(0);

            auto* strip = new QFrame(cell);
            strip->setFixedHeight(6);
            strip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            strip->setCursor(Qt::PointingHandCursor);
            strip->setStyleSheet("background-color: #1a1a1a; border-radius: 2px;"); // default: dark gray
            strip->setProperty("slotIndex", slotIndex);
            strip->installEventFilter(this);
            cellLayout->addWidget(strip);

            mTintFooterLayout->addWidget(cell, 1);
        }
    }

    slotsAreaLayout->addSpacing(1);
    slotsAreaLayout->addWidget(mTintFooterBar, 0);

    mRootLayout->addWidget(slotsArea, 1);

    // No bottom bar: Shift & Trace slots are now fully managed via selection logic

    // Connections
    auto stm = editor()->shiftTrace();
    connect(stm, &ShiftTraceManager::entriesChanged, this, &ShiftTraceWidget::onEntriesChanged);
    connect(stm, &ShiftTraceManager::entryChanged, this, &ShiftTraceWidget::onEntryChanged);
    connect(stm, &ShiftTraceManager::activeMoveEntryChanged, this, &ShiftTraceWidget::onActiveMoveEntryChanged);
    connect(stm, &ShiftTraceManager::needsRedraw, this, [this]{
        updateGhostControls();
    });

    {
        QSettings settings(PENCIL2D, PENCIL2D);

        const int prevRgba = settings.value(kShiftTracePrevTintColorKey, static_cast<int>(mPrevTintColor.rgba())).toInt();
        const int nextRgba = settings.value(kShiftTraceNextTintColorKey, static_cast<int>(mNextTintColor.rgba())).toInt();
        const QColor prevLoaded = QColor::fromRgba(static_cast<QRgb>(prevRgba));
        const QColor nextLoaded = QColor::fromRgba(static_cast<QRgb>(nextRgba));
        if (prevLoaded.isValid()) mPrevTintColor = prevLoaded;
        if (nextLoaded.isValid()) mNextTintColor = nextLoaded;

        if (mPrevChainOpacityToggle) {
            mPrevChainOpacityToggle->setChecked(settings.value(kShiftTracePrevChainOpacityKey, false).toBool());
        }
        if (mNextChainOpacityToggle) {
            mNextChainOpacityToggle->setChecked(settings.value(kShiftTraceNextChainOpacityKey, false).toBool());
        }

        if (mPrevTintToggle) {
            mPrevTintToggle->setChecked(settings.value(kShiftTracePrevTintEnabledKey, false).toBool());
        }
        if (mNextTintToggle) {
            mNextTintToggle->setChecked(settings.value(kShiftTraceNextTintEnabledKey, false).toBool());
        }
    }

    // mHideAllToggle connect removed - moved to timeline transport bar

    connect(mPrevTintToggle, &CosmicCheckboxToggle::toggled, this, [this](bool on){
        editor()->shiftTrace()->setPrevTintEnabled(on);
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(kShiftTracePrevTintEnabledKey, on);
        updateTintUI();
        editor()->updateFrame();
    });
    connect(mNextTintToggle, &CosmicCheckboxToggle::toggled, this, [this](bool on){
        editor()->shiftTrace()->setNextTintEnabled(on);
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(kShiftTraceNextTintEnabledKey, on);
        updateTintUI();
        editor()->updateFrame();
    });

    connect(mPrevChainOpacityToggle, &CosmicCheckboxToggle::toggled, this, [](bool on){
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(kShiftTracePrevChainOpacityKey, on);
    });
    connect(mNextChainOpacityToggle, &CosmicCheckboxToggle::toggled, this, [](bool on){
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(kShiftTraceNextChainOpacityKey, on);
    });
    
    // Connect to editor signals to detect frame changes
    connect(editor(), &Editor::frameModified, this, &ShiftTraceWidget::validateAndRemoveDeletedFrames);
    connect(editor(), &Editor::framesModified, this, &ShiftTraceWidget::validateAndRemoveDeletedFrames);

    connect(editor(), &Editor::scrubbed, this, [this](int){
        updateGhostControls();
    });
    connect(editor(), &Editor::updateTimeLine, this, [this]{
        updateGhostControls();
    });

    editor()->shiftTrace()->setPrevTintColor(mPrevTintColor);
    editor()->shiftTrace()->setNextTintColor(mNextTintColor);
    editor()->shiftTrace()->setPrevTintEnabled(mPrevTintToggle && mPrevTintToggle->isChecked());
    editor()->shiftTrace()->setNextTintEnabled(mNextTintToggle && mNextTintToggle->isChecked());
    updateTintUI();
    updateUI();

    connect(mPrevResetAllButton, &QPushButton::clicked, this, [this](){
        const int mid = ShiftTraceManager::kMaxEntries / 2;
        const auto& entries = editor()->shiftTrace()->entries();
        for (int i = 0; i < entries.size() && i < mid; ++i) {
            editor()->shiftTrace()->resetEntryTransform(entries[i].id);
        }
        editor()->updateFrame();
    });
    connect(mNextResetAllButton, &QPushButton::clicked, this, [this](){
        const int mid = ShiftTraceManager::kMaxEntries / 2;
        const auto& entries = editor()->shiftTrace()->entries();
        for (int i = mid; i < entries.size(); ++i) {
            editor()->shiftTrace()->resetEntryTransform(entries[i].id);
        }
        editor()->updateFrame();
    });

    applyEmbeddedPopupVisuals(mEmbeddedInPopup);
}

void ShiftTraceWidget::applyEmbeddedPopupVisuals(bool on)
{
    if (!mRoot) {
        return;
    }

    mRoot->setAutoFillBackground(false);
    mRoot->setAttribute(Qt::WA_NoSystemBackground, true);

    if (mRootLayout) {
        if (on) {
            mRootLayout->setContentsMargins(0, 0, 0, 0);
            mRootLayout->setSpacing(4);
        } else {
            mRootLayout->setContentsMargins(6, 6, 6, 6);
            mRootLayout->setSpacing(4);
        }
    }

    if (on) {
        mRoot->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    } else {
        mRoot->setStyleSheet(QStringLiteral("QWidget { background-color: #2a2a2a; color: white; }"));
    }

    QWidget* transparentWidgets[] = {mHeaderBar, mScroll, mScrollContents, mBottomBar};
    for (QWidget* w : transparentWidgets) {
        if (!w) {
            continue;
        }
        w->setAutoFillBackground(false);
        w->setAttribute(Qt::WA_NoSystemBackground, true);
        if (on) {
            w->setStyleSheet("QWidget { background: transparent; border: none; }");
        } else {
            w->setStyleSheet(QString());
        }
    }

    if (mScroll) {
        if (on) {
            mScroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QScrollArea > QWidget { background: transparent; }");
        } else {
            mScroll->setStyleSheet(
                "QScrollArea { background-color: #2a2a2a; border: none; }"
                "QWidget { background-color: #2a2a2a; }"
            );
        }
        if (QWidget* vp = mScroll->viewport()) {
            vp->setAutoFillBackground(false);
            vp->setAttribute(Qt::WA_NoSystemBackground, true);
            if (on) {
                vp->setStyleSheet("QWidget { background: transparent; }");
            } else {
                vp->setStyleSheet(QString());
            }
        }
    }

    update();
}

void ShiftTraceWidget::openHoldGhostTintPopup(int slotIndex, QWidget* anchor)
{
    auto* stm = editor()->shiftTrace();

    auto onColorChanged = [this, slotIndex](const QColor& c) {
        editor()->shiftTrace()->setSlotTintColor(slotIndex, c);
        const bool on = editor()->shiftTrace()->slotTintEnabled(slotIndex);
        updateFooterTintStrip(slotIndex, true);
        Q_UNUSED(on);
    };

    auto onModeChanged = [this, slotIndex](bool tintOn) {
        editor()->shiftTrace()->setSlotTintEnabled(slotIndex, tintOn);
        updateFooterTintStrip(slotIndex, true);
        editor()->updateFrame();
    };

    auto* popup = new HoldGhostTintPopup(nullptr,
                                          std::move(onColorChanged),
                                          std::move(onModeChanged));
    popup->setColor(stm->slotTintColor(slotIndex));
    popup->setTintEnabled(stm->slotTintEnabled(slotIndex));

    // Position above the anchor strip
    popup->adjustSize();
    const QPoint globalPos = anchor->mapToGlobal(QPoint(anchor->width() / 2, 0));
    popup->move(globalPos.x() - popup->width() / 2, globalPos.y() - popup->height() - 4);
    popup->show();
}

void ShiftTraceWidget::updateFooterTintStrip(int slotIndex, bool held)
{
    if (!mTintFooterBar) return;

    // Find the strip widget in the footer by slotIndex property
    const auto strips = mTintFooterBar->findChildren<QFrame*>();
    for (QFrame* strip : strips) {
        if (strip->property("slotIndex").toInt() == slotIndex) {
            if (held) {
                const QColor tc = editor()->shiftTrace()->slotTintColor(slotIndex);
                const bool tintOn = editor()->shiftTrace()->slotTintEnabled(slotIndex);
                const QColor displayColor = tintOn ? tc : QColor(0x40, 0x40, 0x40);
                strip->setStyleSheet(QString("background-color: %1; border-radius: 2px;").arg(displayColor.name()));
            } else {
                strip->setStyleSheet("background-color: #1a1a1a; border-radius: 2px;");
            }
            break;
        }
    }
}

void ShiftTraceWidget::updateTintUI()
{
    // Always show full color - checkbox state already indicates enabled/disabled
    if (mLeftColorBox) {
        mLeftColorBox->setStyleSheet(QString("background-color: %1; border: none; border-radius: 4px;").arg(mPrevTintColor.name()));
    }
    if (mRightColorBox) {
        mRightColorBox->setStyleSheet(QString("background-color: %1; border: none; border-radius: 4px;").arg(mNextTintColor.name()));
    }
}

void ShiftTraceWidget::openTintColorPopup(bool isPrev)
{
    QWidget* anchor = isPrev ? mLeftColorBox : mRightColorBox;
    if (!anchor) return;

    auto* popup = new PopupColorWheelWidget(nullptr);
    popup->setWindowFlags(popup->windowFlags() | Qt::Tool | Qt::WindowStaysOnTopHint);
    popup->setPreferenceManager(editor()->preference());
    popup->setColor(isPrev ? mPrevTintColor : mNextTintColor);

    connect(popup, &PopupColorWheelWidget::colorChanged, this, [this, isPrev](const QColor& c){
        if (isPrev) {
            mPrevTintColor = c;
            editor()->shiftTrace()->setPrevTintColor(c);
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue(kShiftTracePrevTintColorKey, static_cast<int>(c.rgba()));
        } else {
            mNextTintColor = c;
            editor()->shiftTrace()->setNextTintColor(c);
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue(kShiftTraceNextTintColorKey, static_cast<int>(c.rgba()));
        }
        updateTintUI();
        editor()->updateFrame();
    });
    connect(popup, &PopupColorWheelWidget::colorSelected, this, [this, isPrev](const QColor& c){
        if (isPrev) {
            mPrevTintColor = c;
            editor()->shiftTrace()->setPrevTintColor(c);
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue(kShiftTracePrevTintColorKey, static_cast<int>(c.rgba()));
        } else {
            mNextTintColor = c;
            editor()->shiftTrace()->setNextTintColor(c);
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue(kShiftTraceNextTintColorKey, static_cast<int>(c.rgba()));
        }
        updateTintUI();
        editor()->updateFrame();
    });

    popup->showAt(QCursor::pos());
    popup->raise();
    popup->activateWindow();
}

void ShiftTraceWidget::updateUI()
{
    rebuildItems();
    updateButtonStates();
    updateGhostControls();
}

void ShiftTraceWidget::onEntriesChanged()
{
    rebuildItems();
    updateButtonStates();
    updateGhostControls();
}

void ShiftTraceWidget::onEntryChanged(int id)
{
    auto stm = editor()->shiftTrace();
    for (const auto& e : stm->entries()) {
        if (e.id == id) {
            auto it = mItemsById.find(id);
            if (it != mItemsById.end()) {
                refreshItem(it.value(), e);
            }
            break;
        }
    }
    updateGhostControls();
}

void ShiftTraceWidget::onActiveMoveEntryChanged(int id)
{
    Q_UNUSED(id);
    // Ensure AdjustGhost toggles reflect the new active entry
    updateActiveMoveVisuals();
    updateGhostControls();
}

void ShiftTraceWidget::rebuildItems()
{
    // Clear existing item widgets
    QLayoutItem* child = nullptr;
    while ((child = mItemsLayout->takeAt(0)) != nullptr) {
        if (auto w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }
    qDeleteAll(mItemsById);
    mItemsById.clear();
    qDeleteAll(mEmptySlots);
    mEmptySlots.clear();
    mSelectedIds.clear();

    // Always create exactly kMaxEntries slots, with a vertical separator between left and right groups
    const auto& list = editor()->shiftTrace()->entries();
    for (int slotIndex = 0; slotIndex < ShiftTraceManager::kMaxEntries; ++slotIndex) {
        // Insert a central gap between left and right slot groups
        if (slotIndex == ShiftTraceManager::kMaxEntries / 2) {
            QWidget* gap = new QWidget(mScrollContents);
            gap->setFixedWidth(kShiftTraceSlotItemW);
            gap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            gap->setStyleSheet("background-color: rgba(17, 17, 17, 255); border: 0px; padding: 0px;");
            mItemsLayout->addWidget(gap);
        }

        if (slotIndex < list.size()) {
            createItemForEntry(list[slotIndex], slotIndex);
        } else {
            createEmptySlot(slotIndex);
        }
    }

    // Ensure visuals reflect the current active move entry after rebuilding
    updateActiveMoveVisuals();
}

void ShiftTraceWidget::createItemForEntry(const ShiftTraceManager::Entry& entry, int slotIndex)
{
    auto* col = new QFrame(mScrollContents);
    auto* colLayout = new QVBoxLayout(col);
    colLayout->setContentsMargins(0, 0, 0, 0);
    colLayout->setSpacing(0);
    // Unified minimums for aligned look: 20x20 toggles and 20px-wide slider
    const int itemW = kShiftTraceSlotItemW;           // width & height of toggles, and slider column width
    const int colLR = 2;            // left/right margins inside column
    const int colW = itemW + (2 * colLR);
    col->setMinimumWidth(colW);
    col->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const int mid = ShiftTraceManager::kMaxEntries / 2;
    const bool roundLeft = (slotIndex == 0) || (slotIndex == mid);
    const bool roundRight = (slotIndex == mid - 1) || (slotIndex == ShiftTraceManager::kMaxEntries - 1);
    QString colStyle = QStringLiteral("background-color: #303030; border: 1px solid #000000; padding: 0px;");
    if (roundLeft) {
        colStyle += QStringLiteral(" border-top-left-radius: %1px; border-bottom-left-radius: %1px;").arg(kShiftTraceSlotCornerRadius);
    }
    if (roundRight) {
        colStyle += QStringLiteral(" border-top-right-radius: %1px; border-bottom-right-radius: %1px;").arg(kShiftTraceSlotCornerRadius);
    }
    col->setStyleSheet(colStyle);

    auto* topStrip = new QFrame(col);
    topStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topStrip->setStyleSheet("background: transparent; border: 0px; padding: 0px;");
    auto* topStripLayout = new QVBoxLayout(topStrip);
    const int leftInset = roundLeft ? 1 : 0;
    const int rightInset = roundRight ? 1 : 0;
    topStripLayout->setContentsMargins(leftInset, 0, rightInset, 0);
    topStripLayout->setSpacing(0);

    auto* topStripBg = new QFrame(topStrip);
    topStripBg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    {
        const int r = kShiftTraceSlotCornerRadius - 1;
        QString bgStyle = QStringLiteral("background-color: rgba(31, 31, 31, 255); border: 0px; padding: 0px;"
                                         " border-bottom-left-radius: 0px; border-bottom-right-radius: 0px;");
        if (roundLeft)  bgStyle += QStringLiteral(" border-top-left-radius: %1px;").arg(r);
        if (roundRight) bgStyle += QStringLiteral(" border-top-right-radius: %1px;").arg(r);
        topStripBg->setStyleSheet(bgStyle);
    }
    auto* topStripBgLayout = new QVBoxLayout(topStripBg);
    topStripBgLayout->setContentsMargins(0, 0, 0, 0);
    topStripBgLayout->setSpacing(2);
    topStripLayout->addWidget(topStripBg);

    auto* slotToggle = new ShiftTraceCircleToggle(topStrip);
    slotToggle->setChecked(entry.visible);
    slotToggle->setEnabled(true);
    slotToggle->setFixedSize(itemW, itemW);
    topStripBgLayout->addWidget(slotToggle, 0, Qt::AlignHCenter);

    // AdjustGhost toggle at top strip (borderless Blender-style toggle)
    auto* adjustGhost = new CosmicButton(topStrip);
    adjustGhost->setBordered(false);
    adjustGhost->setToolTip(tr("Edit ghost transforms"));
    adjustGhost->setText("");
    adjustGhost->setIcon(iconWithDimmedDisabledState(":/Interaction/Hand.svg", 0.35));
    adjustGhost->setIconSize(QSize(18, 18));
    adjustGhost->setCheckable(true);
    const bool hasImage = (entry.frameNumber >= 1 && entry.celKey != 0);
    adjustGhost->setChecked(hasImage && (editor()->shiftTrace()->activeMoveEntryId() == entry.id));
    {
        QColor accent = BlenderStyleToggle::getAccentColor();
        QColor normal(0, 0, 0, 0);
        QColor hover = accent.darker(180);
        adjustGhost->setBackgroundColors(normal, hover, accent);
    }
    adjustGhost->setEnabled(hasImage);
    adjustGhost->setFixedSize(itemW, itemW);
    topStripBgLayout->addWidget(adjustGhost, 0, Qt::AlignHCenter);

    colLayout->addWidget(topStrip, 0);

    auto* body = new QWidget(col);
    body->setStyleSheet("border: 0px; background: transparent;");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(2, 4, 2, 4);
    bodyLayout->setSpacing(0);
    colLayout->addWidget(body, 1);

    // Opacity vertical slider (Cosmic style)
    auto* slider = new CosmicVSlider(body);
    slider->setRange(0, 100);
    slider->setValue(entry.opacityPercent > 0 ? entry.opacityPercent : 70);
    slider->setMinimumWidth(itemW);
    slider->setFixedWidth(itemW);
    slider->setFixedHeight(120);
    bodyLayout->addWidget(slider, 0, Qt::AlignHCenter);

    bodyLayout->addSpacing(1);

    auto* resetGhost = new CosmicButton(body);
    resetGhost->setBordered(false);
    resetGhost->setToolTip(tr("Reset ghost transform"));
    resetGhost->setText("");
    resetGhost->setIcon(iconWithDimmedDisabledState(":/Interaction/ResetShift.svg", 0.35));
    resetGhost->setIconSize(QSize(18, 18));
    resetGhost->setEnabled(hasImage);
    resetGhost->setFixedSize(itemW, itemW);
    bodyLayout->addWidget(resetGhost, 0, Qt::AlignHCenter);

    // HoldGhost lock/unlock toggle (Task 5.2)
    // Always enabled — setSlotHeld guards against no current frame internally
    auto* holdToggle = new CosmicCheckboxToggle(body);
    holdToggle->setMode(CosmicCheckboxToggle::Custom);
    holdToggle->setBordered(false);
    holdToggle->setShowText(false);
    holdToggle->setShowIcon(true);
    holdToggle->setIcon(QIcon(":/Interaction/UnlockFloatingState.svg")); // unchecked = unlocked
    holdToggle->setIconDimWhenUnchecked(true);
    holdToggle->setChecked(false);
    holdToggle->setEnabled(true);
    holdToggle->setFixedSize(itemW, itemW);
    bodyLayout->addWidget(holdToggle, 0, Qt::AlignHCenter);

    // Keep the stack anchored to top when dock is tall
    bodyLayout->addStretch(1);

    const int idVisible = entry.id;
    connect(slotToggle, &ShiftTraceCircleToggle::toggled, this, [this, idVisible](bool on){
        editor()->shiftTrace()->setEntryVisible(idVisible, on);
        editor()->updateFrame();
    });

    // Wire up
    const int idOpacity = entry.id;
    const int slotIndexOpacity = slotIndex;
    connect(slider, &CosmicVSlider::valueChanged, this, [this, idOpacity, slotIndexOpacity](int v){
        if (mApplyingChainedOpacity) {
            editor()->shiftTrace()->setEntryOpacity(idOpacity, v);
            editor()->updateFrame();
            return;
        }

        mApplyingChainedOpacity = true;
        editor()->shiftTrace()->setEntryOpacity(idOpacity, v);

        const int mid = ShiftTraceManager::kMaxEntries / 2;
        const bool isPrev = (slotIndexOpacity < mid);
        CosmicCheckboxToggle* chainToggle = isPrev ? mPrevChainOpacityToggle : mNextChainOpacityToggle;
        if (chainToggle && chainToggle->isChecked()) {
            const auto& entries = editor()->shiftTrace()->entries();
            auto applyNeighbor = [&](int neighborIndex, qreal w) {
                if (neighborIndex < 0 || neighborIndex >= entries.size()) return;
                if (isPrev && neighborIndex >= mid) return;
                if (!isPrev && neighborIndex < mid) return;

                const int nid = entries[neighborIndex].id;
                const int cur = entries[neighborIndex].opacityPercent;

                // Target for this neighbor is w * activeValue (e.g. 0.75 or 0.5 of active),
                // and w also controls how strongly we interpolate toward that target.
                const qreal targetVal = v * w;          // "ideal" neighbor value in [0,100]
                const qreal alpha = w;                  // interpolation strength
                const qreal blended = cur + (targetVal - cur) * alpha;

                const int result = qBound(0, qRound(blended), 100);
                editor()->shiftTrace()->setEntryOpacity(nid, result);
            };

            applyNeighbor(slotIndexOpacity - 1, 0.75);
            applyNeighbor(slotIndexOpacity + 1, 0.75);
            applyNeighbor(slotIndexOpacity - 2, 0.50);
            applyNeighbor(slotIndexOpacity + 2, 0.50);
            applyNeighbor(slotIndexOpacity - 3, 0.25);
            applyNeighbor(slotIndexOpacity + 3, 0.25);
        }

        mApplyingChainedOpacity = false;
        editor()->updateFrame();
    });

    const int idAdjust = entry.id;
    connect(adjustGhost, &QPushButton::clicked, this, [this, idAdjust](){
        const int cur = editor()->shiftTrace()->activeMoveEntryId();
        if (cur == idAdjust) {
            editor()->shiftTrace()->setActiveMoveEntryId(-1);
        } else {
            editor()->shiftTrace()->setActiveMoveEntryId(idAdjust);
        }
        editor()->updateFrame();
    });

    const int idReset = entry.id;
    connect(resetGhost, &QPushButton::clicked, this, [this, idReset](){
        editor()->shiftTrace()->resetEntryTransform(idReset);
        editor()->updateFrame();
    });

    const int captureSlotIndex = slotIndex;
    connect(holdToggle, &CosmicCheckboxToggle::toggled, this, [this, captureSlotIndex, holdToggle](bool on){
        holdToggle->setIcon(QIcon(on
            ? QStringLiteral(":/Interaction/LockFloatingState.svg")
            : QStringLiteral(":/Interaction/UnlockFloatingState.svg")));
        editor()->shiftTrace()->setSlotHeld(captureSlotIndex, on);
        editor()->updateFrame();
    });

    // Store
    ItemWidgets* iw = new ItemWidgets;
    iw->container = col;
    iw->slotToggle = slotToggle;
    iw->opacity = slider;
    iw->adjustGhostButton = adjustGhost;
    iw->resetGhostButton = resetGhost;
    iw->holdGhostToggle = holdToggle;
    iw->entryId = entry.id;
    iw->slotIndex = slotIndex;
    iw->isEmpty = false;
    mItemsById.insert(entry.id, iw);

    mItemsLayout->addWidget(col, 1);
}

void ShiftTraceWidget::createEmptySlot(int slotIndex)
{
    auto* col = new QFrame(mScrollContents);
    auto* colLayout = new QVBoxLayout(col);
    colLayout->setContentsMargins(0, 0, 0, 0);
    colLayout->setSpacing(0);
    // Match column width policy with populated items (expandable)
    const int itemW = kShiftTraceSlotItemW;
    const int colLR = 2;
    const int colW = itemW + (2 * colLR);
    col->setMinimumWidth(colW);
    col->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const int mid = ShiftTraceManager::kMaxEntries / 2;
    const bool roundLeft = (slotIndex == 0) || (slotIndex == mid);
    const bool roundRight = (slotIndex == mid - 1) || (slotIndex == ShiftTraceManager::kMaxEntries - 1);
    QString colStyle = QStringLiteral("background-color: #303030; border: 1px solid #000000; padding: 0px;");
    if (roundLeft) {
        colStyle += QStringLiteral(" border-top-left-radius: %1px; border-bottom-left-radius: %1px;").arg(kShiftTraceSlotCornerRadius);
    }
    if (roundRight) {
        colStyle += QStringLiteral(" border-top-right-radius: %1px; border-bottom-right-radius: %1px;").arg(kShiftTraceSlotCornerRadius);
    }
    col->setStyleSheet(colStyle);

    auto* topStrip = new QFrame(col);
    topStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topStrip->setStyleSheet("background: transparent; border: 0px; padding: 0px;");
    auto* topStripLayout = new QVBoxLayout(topStrip);
    const int leftInset = roundLeft ? 1 : 0;
    const int rightInset = roundRight ? 1 : 0;
    topStripLayout->setContentsMargins(leftInset, 0, rightInset, 0);
    topStripLayout->setSpacing(0);

    auto* topStripBg = new QFrame(topStrip);
    topStripBg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    {
        const int r = kShiftTraceSlotCornerRadius - 1;
        QString bgStyle = QStringLiteral("background-color: rgba(31, 31, 31, 255); border: 0px; padding: 0px;"
                                         " border-bottom-left-radius: 0px; border-bottom-right-radius: 0px;");
        if (roundLeft)  bgStyle += QStringLiteral(" border-top-left-radius: %1px;").arg(r);
        if (roundRight) bgStyle += QStringLiteral(" border-top-right-radius: %1px;").arg(r);
        topStripBg->setStyleSheet(bgStyle);
    }
    auto* topStripBgLayout = new QVBoxLayout(topStripBg);
    topStripBgLayout->setContentsMargins(0, 0, 0, 0);
    topStripBgLayout->setSpacing(2);
    topStripLayout->addWidget(topStripBg);

    auto* slotToggle = new ShiftTraceCircleToggle(topStripBg);
    slotToggle->setChecked(false);
    slotToggle->setEnabled(true);
    slotToggle->setFixedSize(itemW, itemW);
    topStripBgLayout->addWidget(slotToggle, 0, Qt::AlignHCenter);

    // AdjustGhost toggle (interactive UI only, no logic wiring for empty slots)
    auto* adjustGhost = new CosmicButton(topStripBg);
    adjustGhost->setBordered(false);
    adjustGhost->setToolTip(tr("Edit ghost transforms"));
    adjustGhost->setText("");
    adjustGhost->setIcon(iconWithDimmedDisabledState(":/Interaction/Hand.svg", 0.35));
    adjustGhost->setIconSize(QSize(18, 18));
    adjustGhost->setCheckable(true);
    adjustGhost->setChecked(false);
    {
        QColor accent = BlenderStyleToggle::getAccentColor();
        QColor normal(0, 0, 0, 0);
        QColor hover = accent.darker(180);
        adjustGhost->setBackgroundColors(normal, hover, accent);
    }
    adjustGhost->setEnabled(false);
    adjustGhost->setFixedSize(itemW, itemW);
    topStripBgLayout->addWidget(adjustGhost, 0, Qt::AlignHCenter);

    colLayout->addWidget(topStrip, 0);

    auto* body = new QWidget(col);
    body->setStyleSheet("border: 0px; background: transparent;");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(2, 4, 2, 4);
    bodyLayout->setSpacing(0);
    colLayout->addWidget(body, 1);

    // Opacity vertical slider (interactive UI only, no logic wiring for empty slots)
    auto* slider = new CosmicVSlider(body);
    slider->setRange(0, 100);
    slider->setValue(0);
    slider->setEnabled(true);
    slider->setMinimumWidth(itemW);
    slider->setFixedWidth(itemW);
    slider->setFixedHeight(120);
    bodyLayout->addWidget(slider, 0, Qt::AlignHCenter);

    bodyLayout->addSpacing(1);

    auto* resetGhost = new CosmicButton(body);
    resetGhost->setBordered(false);
    resetGhost->setToolTip(tr("Reset ghost transform"));
    resetGhost->setText("");
    resetGhost->setIcon(iconWithDimmedDisabledState(":/Interaction/ResetShift.svg", 0.35));
    resetGhost->setIconSize(QSize(18, 18));
    resetGhost->setEnabled(false);
    resetGhost->setFixedSize(itemW, itemW);
    bodyLayout->addWidget(resetGhost, 0, Qt::AlignHCenter);

    // HoldGhost lock/unlock toggle (disabled for empty slots, Task 5.3)
    auto* holdToggle = new CosmicCheckboxToggle(body);
    holdToggle->setMode(CosmicCheckboxToggle::Custom);
    holdToggle->setBordered(false);
    holdToggle->setShowText(false);
    holdToggle->setShowIcon(true);
    holdToggle->setIcon(QIcon(":/Interaction/UnlockFloatingState.svg"));
    holdToggle->setIconDimWhenUnchecked(true);
    holdToggle->setChecked(false);
    holdToggle->setEnabled(false);
    holdToggle->setFixedSize(itemW, itemW);
    bodyLayout->addWidget(holdToggle, 0, Qt::AlignHCenter);

    bodyLayout->addStretch(1);

    // Store in empty slots vector
    ItemWidgets* iw = new ItemWidgets;
    iw->container = col;
    iw->slotToggle = slotToggle;
    iw->opacity = slider;
    iw->adjustGhostButton = adjustGhost;
    iw->resetGhostButton = resetGhost;
    iw->holdGhostToggle = holdToggle;
    iw->entryId = -1000 - slotIndex; // Unique negative ID for empty slots
    iw->slotIndex = slotIndex;
    iw->isEmpty = true;
    mEmptySlots.push_back(iw);

    mItemsLayout->addWidget(col, 1);
}

void ShiftTraceWidget::refreshItem(ItemWidgets* item, const ShiftTraceManager::Entry& entry)
{
    if (item->isEmpty) return;

    if (item->slotToggle) {
        QSignalBlocker b0(item->slotToggle);
        item->slotToggle->setChecked(entry.visible);
    }

    QSignalBlocker b1(item->opacity);
    item->opacity->setValue(entry.opacityPercent);

    const bool isActive = (editor()->shiftTrace()->activeMoveEntryId() == entry.id);
    if (item->adjustGhostButton) {
        QSignalBlocker b2(item->adjustGhostButton);
        const bool hasImage = (entry.frameNumber >= 1 && entry.celKey != 0);
        item->adjustGhostButton->setChecked(hasImage && isActive);
    }

    // Sync hold ghost toggle state (Task 5.4)
    if (item->holdGhostToggle) {
        const bool held = editor()->shiftTrace()->isSlotHeld(item->slotIndex);
        {
            QSignalBlocker b3(item->holdGhostToggle);
            item->holdGhostToggle->setChecked(held);
            // Swap icon: lock when held, unlock when free
            item->holdGhostToggle->setIcon(QIcon(held
                ? QStringLiteral(":/Interaction/LockFloatingState.svg")
                : QStringLiteral(":/Interaction/UnlockFloatingState.svg")));
        }

        // Update footer tint strip for this slot
        updateFooterTintStrip(item->slotIndex, held);

        // Visual distinction: colored border when held, normal when free
        if (item->container) {
            const int mid = ShiftTraceManager::kMaxEntries / 2;
            const int slotIndex = item->slotIndex;
            const bool roundLeft = (slotIndex == 0) || (slotIndex == mid);
            const bool roundRight = (slotIndex == mid - 1) || (slotIndex == ShiftTraceManager::kMaxEntries - 1);

            QString colStyle;
            if (held) {
                colStyle = QStringLiteral("background-color: #303030; border: 1px solid #40555c; padding: 0px;");
            } else {
                colStyle = QStringLiteral("background-color: #303030; border: 1px solid #000000; padding: 0px;");
            }
            if (roundLeft) {
                colStyle += QStringLiteral(" border-top-left-radius: %1px; border-bottom-left-radius: %1px;").arg(kShiftTraceSlotCornerRadius);
            }
            if (roundRight) {
                colStyle += QStringLiteral(" border-top-right-radius: %1px; border-bottom-right-radius: %1px;").arg(kShiftTraceSlotCornerRadius);
            }
            item->container->setStyleSheet(colStyle);
        }
    }
}

void ShiftTraceWidget::updateGhostControls()
{
    auto stm = editor()->shiftTrace();
    const auto& entries = stm->entries();
    const int activeId = stm->activeMoveEntryId();

    bool activeHasImage = true;
    if (activeId != -1) {
        activeHasImage = false;
        for (const auto& e : entries) {
            if (e.id == activeId) {
                activeHasImage = (e.frameNumber >= 1 && e.celKey != 0);
                break;
            }
        }
    }

    for (const auto& e : entries) {
        auto it = mItemsById.find(e.id);
        if (it == mItemsById.end()) continue;
        ItemWidgets* item = it.value();
        if (!item || item->isEmpty) continue;

        const bool hasImage = (e.frameNumber >= 1 && e.celKey != 0);

        if (item->adjustGhostButton) {
            QSignalBlocker b(item->adjustGhostButton);
            item->adjustGhostButton->setEnabled(hasImage);
            if (!hasImage) {
                item->adjustGhostButton->setChecked(false);
            }
        }
        if (item->resetGhostButton) {
            item->resetGhostButton->setEnabled(hasImage);
        }
        if (item->holdGhostToggle && !stm->isSlotHeld(item->slotIndex)) {
            // Hold toggle is always enabled — setSlotHeld guards internally
            item->holdGhostToggle->setEnabled(true);
        }
    }

    for (ItemWidgets* item : mEmptySlots) {
        if (!item) continue;
        if (item->adjustGhostButton) {
            QSignalBlocker b(item->adjustGhostButton);
            item->adjustGhostButton->setEnabled(false);
            item->adjustGhostButton->setChecked(false);
        }
        if (item->resetGhostButton) {
            item->resetGhostButton->setEnabled(false);
        }
    }

    if (activeId != -1 && !activeHasImage) {
        stm->setActiveMoveEntryId(-1);
        editor()->updateFrame();
    }
}

void ShiftTraceWidget::updateActiveMoveVisuals()
{
    const int activeId = editor()->shiftTrace()->activeMoveEntryId();

    for (auto it = mItemsById.begin(); it != mItemsById.end(); ++it) {
        ItemWidgets* iw = it.value();
        if (!iw->adjustGhostButton) {
            continue;
        }

        const bool isActive = (it.key() == activeId);
        QSignalBlocker b(iw->adjustGhostButton);
        iw->adjustGhostButton->setChecked(isActive && iw->adjustGhostButton->isEnabled());
    }
}

bool ShiftTraceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == mLeftColorBox) {
            openTintColorPopup(true);
            return true;
        }
        if (watched == mRightColorBox) {
            openTintColorPopup(false);
            return true;
        }

        // Tint strip click — open per-slot tint popup
        auto* watchedWidget = qobject_cast<QWidget*>(watched);
        if (watchedWidget) {
            const QVariant slotProp = watchedWidget->property("slotIndex");
            if (slotProp.isValid()) {
                const int slotIdx = slotProp.toInt();
                if (editor()->shiftTrace()->isSlotHeld(slotIdx)) {
                    openHoldGhostTintPopup(slotIdx, watchedWidget);
                    return true;
                }
            }
        }
    }
    return BaseDockWidget::eventFilter(watched, event);
}

void ShiftTraceWidget::onAddFromSelection()
{
    int remaining = ShiftTraceManager::kMaxEntries - editor()->shiftTrace()->entries().size();
    if (remaining <= 0) {
        QMessageBox::information(this, tr("Shift & Trace"), 
                                 tr("Maximum of %1 entries reached. Please remove an entry before adding more.")
                                 .arg(ShiftTraceManager::kMaxEntries));
        return;
    }

    auto* lay = editor()->layers()->currentLayer();
    if (!lay) return;
    QVector<QPair<int,int>> toAdd;
    const int currentLayerIdx = editor()->layers()->currentLayerIndex();
    const int currentFrame = editor()->currentFrame();
    for (int pos : lay->getSelectedFramesByPos()) {
        if (pos == currentFrame) continue;
        toAdd.push_back({currentLayerIdx, pos});
    }
    
    // After building toAdd:
    if (toAdd.size() > remaining) {
        QMessageBox::information(this, tr("Shift & Trace"),
            tr("Only %1 of %2 selected frames were added due to the %3-slot limit.")
            .arg(remaining).arg(toAdd.size()).arg(ShiftTraceManager::kMaxEntries));
    }
    // Optionally trim toAdd to remaining before addEntries
    if (toAdd.size() > remaining) toAdd.resize(remaining);
    
    if (!toAdd.isEmpty()) {
        editor()->shiftTrace()->addEntries(toAdd);
        editor()->updateFrame();
    }
}

void ShiftTraceWidget::onRemoveSelected()
{
    if (mSelectedIds.isEmpty()) return;
    editor()->shiftTrace()->removeByIds(mSelectedIds.values().toVector());
    mSelectedIds.clear();
    editor()->updateFrame();
}

void ShiftTraceWidget::updateButtonStates()
{
    // Buttons were removed; keep function for potential future use but no-op for now.
}

void ShiftTraceWidget::onHideAllToggled(bool hideAll)
{
    // UI semantics: checked = visible/enabled
    const bool hide = !hideAll;
    mHideAll = hide;
    editor()->shiftTrace()->setHideAll(hide);
}

void ShiftTraceWidget::validateAndRemoveDeletedFrames()
{
    // Check each entry to see if its frame still exists
    QVector<int> idsToRemove;
    auto stm = editor()->shiftTrace();
    
    // In slot mode, entries are exposure-offset slots and should never be removed.
    // When a computed frame is out of range, we simply render nothing for that slot.
    if (!stm->entries().isEmpty() && stm->entries().size() == ShiftTraceManager::kMaxEntries && stm->entries().front().exposureOffset != 0) {
        return;
    }
    for (const auto& entry : stm->entries()) {
        Layer* layer = editor()->layers()->getLayer(entry.layerIndex);
        if (!layer || !layer->keyExists(entry.frameNumber)) {
            // Frame was deleted or layer was deleted
            idsToRemove.push_back(entry.id);
        }
    }
    
    if (!idsToRemove.isEmpty()) {
        stm->removeByIds(idsToRemove);
    }
} 

