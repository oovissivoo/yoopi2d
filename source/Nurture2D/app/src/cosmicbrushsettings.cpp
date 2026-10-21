#include "cosmicbrushsettings.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QStyleOption>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <functional>

// Get accent color from the existing BlenderStyle components
#include "../CUI/blenderstylemodalpopup.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicassetgallery.h"

#include "managers/brushpresetstore.h"
#include "../CUI/cosmichslider.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicenumselector.h"
#include "../CUI/cosmicslidercard.h"
#include "managers/brushassetstore.h"
#include "beziercurveeditor.h"
#include "filedialog.h"

#include "cosmicwindowheader.h"

namespace {
// No custom delegate needed — tab buttons are styled QPushButtons.
} // namespace

CosmicBrushSettings::CosmicBrushSettings(int presetIndex, QWidget* parent)
    : CosmicPopupWindow(parent)
    , mPresetIndex(presetIndex)
{
    setupUI();

    // This popup is created once and reused by BrushPresetListWidget.
    // CosmicPopupWindow defaults to WA_DeleteOnClose=true, which would turn close() into a delete
    // and leave the owner with a dangling pointer.
    setAttribute(Qt::WA_DeleteOnClose, false);

    connect(BrushPresetStore::instance(), &BrushPresetStore::presetsUpdated,
            this, &CosmicBrushSettings::onPresetsUpdated,
            Qt::UniqueConnection);
    
    setResizable(true);
    setMinimumSize(300, 200);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);
    setShadowEnabled(false);
    setShadowPadding(0);
    setPersistentSizeKey(QStringLiteral("CosmicBrushPresetSettings/Size"));
}

CosmicBrushSettings::~CosmicBrushSettings()
{
}

void CosmicBrushSettings::setPresetId(const QString& id)
{
    mPresetId = id;

    // If the popup is open, immediately refresh controls for the new preset.
    if (!mSyncQueued && !mSyncInProgress) {
        mSyncQueued = true;
        QTimer::singleShot(0, this, [this]() {
            mSyncQueued = false;
            syncControlsFromStore();
        });
    }
}

QString CosmicBrushSettings::presetId() const
{
    return mPresetId;
}

void CosmicBrushSettings::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    mMainLayout->setContentsMargins(16, 10, 16, 16);
    mMainLayout->setSpacing(8);

    mWindowHeader = new CosmicWindowHeader(this);
    connect(mWindowHeader, &CosmicWindowHeader::closeRequested, this, &QDialog::close);
    mMainLayout->addWidget(mWindowHeader);

    mPresetHeaderLabel = new QLabel(this);
    mPresetHeaderLabel->setText(tr("Settings"));
    mPresetHeaderLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    mPresetHeaderLabel->setFixedHeight(26);
    mPresetHeaderLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; font-weight: 600; font-size: 13px; background: transparent; }"));
    mMainLayout->addWidget(mPresetHeaderLabel);

    // --- Tab selector (CosmicEnumSelector) ---
    mTabSelector = new CosmicEnumSelector(this);
    mTabSelector->setShowBorder(false);
    mMainLayout->addWidget(mTabSelector);

    // Thin separator line below tab selector
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #1a1a1a; border: none;");
    mMainLayout->addWidget(sep);

    // --- Stacked pages ---
    mPageStack = new QStackedWidget(this);
    mPageStack->setStyleSheet("background: transparent;");
    mMainLayout->addWidget(mPageStack, 1);

    setupTabBar();
    applyBrushesSliderStyle(this);
}

void CosmicBrushSettings::setupTabBar()
{
    // Clear existing pages from stack
    while (mPageStack->count() > 0) {
        auto* w = mPageStack->widget(0);
        mPageStack->removeWidget(w);
        delete w;
    }

    const QStringList cats = QStringList{
        tr("Stroke"),
        tr("Tip"),
        tr("Texture"),
        tr("Color"),
        tr("Pressure")
    };

    mTabCategories = cats;

    if (mTabSelector) {
        QSignalBlocker blocker(mTabSelector);
        mTabSelector->setOptions(cats);

        if (mFinalLineMode) {
            // Disable every tab except Pressure (index 4) and force it selected
            for (int i = 0; i < cats.size(); ++i)
                mTabSelector->setItemEnabled(i, i == 4);
            mTabSelector->setCurrentIndex(4);
        } else {
            for (int i = 0; i < cats.size(); ++i)
                mTabSelector->setItemEnabled(i, true);
            // Stay on Pressure when coming from Final Line mode, otherwise default to Stroke
            const int currentIdx = mTabSelector->currentIndex();
            const int startIdx = (currentIdx == 4) ? 4 : 0;
            mTabSelector->setCurrentIndex(startIdx);
        }

        connect(mTabSelector, &CosmicEnumSelector::currentIndexChanged,
                this, &CosmicBrushSettings::onTabSelected,
                Qt::UniqueConnection);
    }

    // Build one scroll-wrapped page per category
    for (int i = 0; i < cats.size(); ++i) {
        auto* page = new QWidget();
        page->setStyleSheet("background: transparent;");
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 12, 0, 0);
        pageLayout->setSpacing(15);
        pageLayout->addStretch();

        auto* scroll = new QScrollArea();
        scroll->setWidget(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
        applyBrushesSliderStyle(scroll);

        mPageStack->addWidget(scroll);
    }

    // Activate correct starting tab
    const int startTab = mFinalLineMode ? 4 : (mTabSelector ? mTabSelector->currentIndex() : 0);
    onTabSelected(startTab);
}

void CosmicBrushSettings::setPresetName(const QString& name)
{
    mPresetName = name;
    if (mPresetHeaderLabel) {
        if (mPresetName.isEmpty()) {
            mPresetHeaderLabel->setText(tr("Settings"));
        } else {
            mPresetHeaderLabel->setText(tr("Settings - %1").arg(mPresetName));
        }
    }
}

void CosmicBrushSettings::setFinalLineLiveState(bool enabled, const QVector<QPointF>& curvePoints)
{
    mFinalLineLiveEnabled = enabled;
    mFinalLineLiveCurve = curvePoints.isEmpty()
        ? QVector<QPointF>{ QPointF(0.0, 0.0), QPointF(0.5, 0.5), QPointF(1.0, 1.0) }
        : curvePoints;

    // If the popup is already showing Final Line content, sync the controls immediately.
    if (mFinalLineMode) {
        if (mSizePressureToggle) {
            QSignalBlocker b(mSizePressureToggle);
            mSizePressureToggle->setChecked(mFinalLineLiveEnabled);
        }
        if (mSizeCurveEditor) {
            QSignalBlocker b(mSizeCurveEditor);
            mSizeCurveEditor->setControlPoints(mFinalLineLiveCurve);
        }
    }
}

void CosmicBrushSettings::setFinalLineMode(bool finalLine)
{
    if (mFinalLineMode == finalLine) return;
    mFinalLineMode = finalLine;
    setupTabBar();
}

void CosmicBrushSettings::showEvent(QShowEvent* event)
{
    CosmicPopupWindow::showEvent(event);

    // Ensure the active page content is built on first show
    if (mPageStack && mPageStack->currentIndex() >= 0) {
        const int idx = mPageStack->currentIndex();
        if (idx < mTabCategories.size()) {
            updateContentForCategory(mTabCategories[idx]);
        }
    }
}

void CosmicBrushSettings::resizeEvent(QResizeEvent* event)
{
    CosmicPopupWindow::resizeEvent(event);
}

void CosmicBrushSettings::changeEvent(QEvent* event)
{
    CosmicPopupWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        // Update the persistent header label.
        if (mPresetHeaderLabel) {
            if (mPresetName.isEmpty()) {
                mPresetHeaderLabel->setText(tr("Settings"));
            } else {
                mPresetHeaderLabel->setText(tr("Settings - %1").arg(mPresetName));
            }
        }
        // Rebuild the tab bar with translated tab names and refresh the current page.
        setupTabBar();
    }
}

void CosmicBrushSettings::updateCategoryStyles()
{
    // Handled by CosmicEnumSelector's own paint logic.
}

void CosmicBrushSettings::onTabSelected(int index)
{
    if (index < 0 || index >= mTabCategories.size()) return;

    // Keep the enum selector in sync (e.g. when called programmatically)
    if (mTabSelector && mTabSelector->currentIndex() != index) {
        QSignalBlocker b(mTabSelector);
        mTabSelector->setCurrentIndex(index);
    }

    // Switch stacked page
    if (mPageStack)
        mPageStack->setCurrentIndex(index);

    // Get the page's inner widget (inside the scroll area)
    auto* scroll = qobject_cast<QScrollArea*>(mPageStack->widget(index));
    if (!scroll) return;
    mSettingsContentWidget = scroll->widget();

    updateContentForCategory(mTabCategories[index]);
}

void CosmicBrushSettings::updateContentForCategory(const QString& category)
{
    // Guard against re-entrant calls (e.g. a slider valueChanged fires store->updatePreset
    // which emits presetsUpdated which calls syncControlsFromStore while we're mid-rebuild).
    if (mSyncInProgress) return;

    const QSize keepSize = size();
    const bool keepSizeValid = (keepSize.width() >= 600 && keepSize.height() >= 420);

    // Clear existing content.
    // Nullify QPointer members FIRST so no in-flight signal can reach a half-deleted widget,
    // then delete synchronously (not deleteLater) so the heap is clean before we rebuild.
    mSpacingSlider = nullptr;
    mScatterSlider = nullptr;
    mRotationSlider = nullptr;
    mSizeJitterSlider = nullptr;
    mColorJitterSlider = nullptr;
    mSpacingCard    = nullptr;
    mScatterCard    = nullptr;
    mRotationCard   = nullptr;
    mSizeJitterCard = nullptr;
    mColorJitterCard = nullptr;
    mTipModeSelector  = nullptr;
    mTipPreviewLabel  = nullptr;
    mTipNameLabel     = nullptr;
    mTipContrastCard  = nullptr;
    mTipContrastSlider = nullptr;
    mTextureScaleCard    = nullptr;
    mTextureContrastCard = nullptr;
    mTextureContrastSlider = nullptr;
    mTexturePreviewLabel = nullptr;
    mTextureNameLabel    = nullptr;
    mAddTextureButton    = nullptr;
    mRemoveTextureButton = nullptr;
    mSizePressureToggle = nullptr;
    mFlowPressureToggle = nullptr;
    mSizeCurveEditor = nullptr;
    mFlowCurveEditor = nullptr;

    auto* layout = qobject_cast<QVBoxLayout*>(mSettingsContentWidget->layout());
    if (!layout) return;

    // Remove and immediately delete all items except the trailing stretch.
    while (layout->count() > 1) {
        auto* item = layout->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->hide();
            delete w;
        }
        delete item;
    }
    
    // Add category-specific content — dispatch by index, not by translated name.
    const int catIndex = mTabCategories.indexOf(category);
    if (catIndex == 0) {
        rebuildStrokeUI(layout);
        syncControlsFromStore();
    } else if (catIndex == 1) {
        rebuildTipUI(layout);
        syncControlsFromStore();
    } else if (catIndex == 2) {
        rebuildTextureUI(layout);
        syncControlsFromStore();
    } else if (catIndex == 3) {
        rebuildColorUI(layout);
        syncControlsFromStore();
    } else if (catIndex == 4) {
        rebuildPressureUI(layout);
        syncControlsFromStore();
    } else {
        // Stub categories (Tip and any future additions)
        auto* placeholderLabel = new QLabel(tr("%1 settings coming soon").arg(category), mSettingsContentWidget);
        placeholderLabel->setStyleSheet("color: #555; font-size: 12px;");
        placeholderLabel->setAlignment(Qt::AlignCenter);
        layout->insertWidget(0, placeholderLabel);
    }
    
    // Apply brushes slider styling to all scrollbars in this widget
    applyBrushesSliderStyle(mSettingsContentWidget);

    // Keep the floating window size stable.
    if (keepSizeValid && size() != keepSize) {
        resize(keepSize);
    }
}

void CosmicBrushSettings::rebuildStrokeUI(QVBoxLayout* layout)
{
    // ── Normal mode: 2-column grid of CosmicSliderCards ──────────────────────
    auto* store = BrushPresetStore::instance();

    const std::function<void(const std::function<void(ToolOptionWidget::BrushPreset&)>&)>
        applyPresetMutation = [this, store](const std::function<void(ToolOptionWidget::BrushPreset&)>& mutator) {
        if (mPresetId.isEmpty()) return;
        ToolOptionWidget::BrushPreset p;
        if (!loadPresetFromStore(mPresetId, p)) return;
        mutator(p);
        store->updatePreset(mPresetId, p);
    };

    mSpacingCard    = new CosmicSliderCard(tr("Spacing"),         mSettingsContentWidget);
    mScatterCard    = new CosmicSliderCard(tr("Scatter"),         mSettingsContentWidget);
    mRotationCard   = new CosmicSliderCard(tr("Random Rotation"), mSettingsContentWidget);
    mSizeJitterCard = new CosmicSliderCard(tr("Random Size"),     mSettingsContentWidget);

    mSpacingCard->setRange(5, 1000);
    mScatterCard->setRange(0, 300);
    mRotationCard->setRange(0, 12);
    mSizeJitterCard->setRange(0, 50);

    // Cache raw slider pointers for isSliderDown() checks in syncControlsFromStore
    mSpacingSlider    = mSpacingCard->slider();
    mScatterSlider    = mScatterCard->slider();
    mRotationSlider   = mRotationCard->slider();
    mSizeJitterSlider = mSizeJitterCard->slider();

    connect(mSpacingCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mSpacingSlider) mSpacingSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.spacingMult = qBound<qreal>(0.05, v / 100.0, 10.0);
        });
    });
    connect(mScatterCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mScatterSlider) mScatterSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.scatter = qBound<qreal>(0.0, v / 100.0, 3.0);
        });
    });
    connect(mRotationCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mRotationSlider) mRotationSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.rotationDeg = qBound<qreal>(0.0, (qreal)v, 12.0); // 0–12 angle count
        });
    });
    connect(mSizeJitterCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mSizeJitterSlider) mSizeJitterSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.sizeJitter = qBound<qreal>(0.0, v / 100.0, 0.5);
        });
    });

    // Row 0: Spacing | Random Rotation
    auto* row0 = new QWidget(mSettingsContentWidget);
    row0->setStyleSheet("background: transparent;");
    auto* row0L = new QHBoxLayout(row0);
    row0L->setContentsMargins(0, 0, 0, 0);
    row0L->setSpacing(8);
    row0L->addWidget(mSpacingCard,  1);
    row0L->addWidget(mRotationCard, 1);

    // Row 1: Scatter | Random Size
    auto* row1 = new QWidget(mSettingsContentWidget);
    row1->setStyleSheet("background: transparent;");
    auto* row1L = new QHBoxLayout(row1);
    row1L->setContentsMargins(0, 0, 0, 0);
    row1L->setSpacing(8);
    row1L->addWidget(mScatterCard,    1);
    row1L->addWidget(mSizeJitterCard, 1);

    int idx = 0;
    layout->insertWidget(idx++, row0);
    layout->insertWidget(idx++, row1);
}

void CosmicBrushSettings::rebuildColorUI(QVBoxLayout* layout)
{
    auto* store = BrushPresetStore::instance();

    const std::function<void(const std::function<void(ToolOptionWidget::BrushPreset&)>&)>
        applyPresetMutation = [this, store](const std::function<void(ToolOptionWidget::BrushPreset&)>& mutator) {
        if (mPresetId.isEmpty()) return;
        ToolOptionWidget::BrushPreset p;
        if (!loadPresetFromStore(mPresetId, p)) return;
        mutator(p);
        store->updatePreset(mPresetId, p);
    };

    mColorJitterCard = new CosmicSliderCard(tr("Color Jitter"), mSettingsContentWidget);
    mColorJitterCard->setRange(0, 100);
    mColorJitterSlider = mColorJitterCard->slider();

    connect(mColorJitterCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mColorJitterSlider) mColorJitterSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.colorJitter = qBound<qreal>(0.0, v / 100.0, 1.0);
        });
    });

    layout->insertWidget(0, mColorJitterCard);
}

void CosmicBrushSettings::rebuildTipUI(QVBoxLayout* layout)
{
    auto* store = BrushPresetStore::instance();

    const std::function<void(const std::function<void(ToolOptionWidget::BrushPreset&)>&)>
        applyPresetMutation = [this, store](const std::function<void(ToolOptionWidget::BrushPreset&)>& mutator) {
        if (mPresetId.isEmpty()) return;
        ToolOptionWidget::BrushPreset p;
        if (!loadPresetFromStore(mPresetId, p)) return;
        mutator(p);
        store->updatePreset(mPresetId, p);
    };

    // Helper: load tip preview from QRC for a given TipMode
    auto tipPixmapForMode = [](ToolOptionWidget::BrushPreset::TipMode mode, const QSize& sz) -> QPixmap {
        QString path;
        switch (mode) {
        case ToolOptionWidget::BrushPreset::TipMode::Soft:
            path = QStringLiteral(":/BrushTips/CosmicDefaultTip_Soft.png"); break;
        case ToolOptionWidget::BrushPreset::TipMode::Custom:
            path = QStringLiteral(":/BrushTips/CosmicDefaultTip_Medium.png"); break;
        default:
            path = QStringLiteral(":/BrushTips/CosmicDefaultTip_Hard.png"); break;
        }
        QImage img(path);
        if (img.isNull()) return QPixmap();
        return QPixmap::fromImage(img).scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    };

    // ── Header card: name label + import controls ────────────────────────────
    auto* headerRow = new QWidget(mSettingsContentWidget);
    headerRow->setStyleSheet(
        "QWidget#tipHeaderRow { background: #1a1a1a; border-radius: " +
        QString::number(CosmicUiMetrics::cornerRadius()) + "px; }");
    headerRow->setObjectName("tipHeaderRow");

    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(10, 4, 6, 4);
    headerLayout->setSpacing(6);

    mTipNameLabel = new QLabel(tr("Hard"), headerRow);
    mTipNameLabel->setStyleSheet("color: #888888; font-size: 12px; background: transparent;");
    mTipNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(mTipNameLabel, 1);

    // Add (import custom tip)
    auto* addTipButton = new CosmicButton(headerRow);
    addTipButton->setIcon(QIcon(":/Interaction/Add.svg"));
    addTipButton->setIconSize(QSize(16, 16));
    addTipButton->setToolTip(tr("Import custom tip mask"));
    addTipButton->setFixedSize(28, 28);

    // Gallery button — opens CosmicAssetGallery filtered to tips.
    mGalleryTipButton = new CosmicButton(headerRow);
    mGalleryTipButton->setIcon(QIcon(":/Interaction/ReffWindowGallery.svg"));
    mGalleryTipButton->setIconSize(QSize(16, 16));
    mGalleryTipButton->setToolTip(tr("Tip gallery"));
    mGalleryTipButton->setFixedSize(28, 28);

    // Remove (revert to built-in)
    auto* removeTipButton = new CosmicButton(headerRow);
    removeTipButton->setIcon(QIcon(":/Interaction/trash.svg"));
    removeTipButton->setIconSize(QSize(16, 16));
    removeTipButton->setToolTip(tr("Remove custom tip (revert to Hard)"));
    removeTipButton->setFixedSize(28, 28);

    headerLayout->addWidget(addTipButton);
    headerLayout->addWidget(mGalleryTipButton);
    headerLayout->addStretch();
    headerLayout->addWidget(removeTipButton);

    // ── Body row: [left: preview thumbnail] [right: mode selector + contrast] ─
    mTipPreviewLabel = new QLabel(mSettingsContentWidget);
    mTipPreviewLabel->setAlignment(Qt::AlignCenter);
    mTipPreviewLabel->setStyleSheet(
        "background: #1a1a1a; border-radius: " +
        QString::number(CosmicUiMetrics::cornerRadius()) + "px;");
    mTipPreviewLabel->setFixedSize(160, 160);

    // Mode selector (Hard / Soft / Custom)
    mTipModeSelector = new CosmicEnumSelector(mSettingsContentWidget);
    mTipModeSelector->setOptions({ tr("Hard"), tr("Soft"), tr("Custom") });
    mTipModeSelector->setShowBorder(false);

    // Contrast card
    mTipContrastCard = new CosmicSliderCard(tr("Contrast"), mSettingsContentWidget);
    mTipContrastCard->setRange(0, 100);
    mTipContrastSlider = mTipContrastCard->slider();

    auto* rightCol = new QWidget(mSettingsContentWidget);
    rightCol->setStyleSheet("background: transparent;");
    auto* rightColLayout = new QVBoxLayout(rightCol);
    rightColLayout->setContentsMargins(0, 0, 0, 0);
    rightColLayout->setSpacing(8);
    rightColLayout->addWidget(mTipModeSelector);
    rightColLayout->addWidget(mTipContrastCard);
    rightColLayout->addStretch();

    auto* bodyRow = new QWidget(mSettingsContentWidget);
    bodyRow->setStyleSheet("background: transparent;");
    auto* bodyLayout = new QHBoxLayout(bodyRow);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);
    bodyLayout->addWidget(mTipPreviewLabel, 0);
    bodyLayout->addWidget(rightCol, 1);

    // ── Wire signals ─────────────────────────────────────────────────────────
    connect(mTipModeSelector, &CosmicEnumSelector::currentIndexChanged, this,
            [this, applyPresetMutation, tipPixmapForMode](int idx) {
        using TipMode = ToolOptionWidget::BrushPreset::TipMode;

        if (idx == 2) {
            // Custom selected — just switch the mode. The user can import a tip
            // via the + button or pick one from the gallery. No auto file-browser.
            ToolOptionWidget::BrushPreset current;
            const bool hasData = loadPresetFromStore(mPresetId, current) && !current.tipAssetId.isEmpty();

            applyPresetMutation([](ToolOptionWidget::BrushPreset& p) {
                p.tipMode = TipMode::Custom;
            });

            if (hasData) {
                if (mTipNameLabel) mTipNameLabel->setText(tr("Custom"));
                if (mTipPreviewLabel) {
                    QImage img;
                    if (img.loadFromData(BrushAssetStore::instance()->loadTip(current.tipAssetId)))
                        mTipPreviewLabel->setPixmap(
                            QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                           Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            } else {
                // No tip imported yet — show placeholder; user imports via + or gallery.
                if (mTipNameLabel)    mTipNameLabel->setText(tr("None"));
                if (mTipPreviewLabel) mTipPreviewLabel->setPixmap(QPixmap());
            }
            return;
        }

        // Hard (0) or Soft (1)
        const TipMode mode = (idx == 1) ? TipMode::Soft : TipMode::Hard;
        const QString name = (idx == 1) ? tr("Soft") : tr("Hard");
        if (mTipNameLabel)    mTipNameLabel->setText(name);
        if (mTipPreviewLabel) mTipPreviewLabel->setPixmap(
            tipPixmapForMode(mode, QSize(160, 160)));
        applyPresetMutation([mode](ToolOptionWidget::BrushPreset& p) { p.tipMode = mode; });
    });

    connect(mTipContrastCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mTipContrastSlider) mTipContrastSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.tipContrastPercent = qBound(0, v, 100);
        });
    });

    // Import custom tip mask (also works as replace when already in Custom)
    connect(addTipButton, &CosmicButton::clicked, this, [this, applyPresetMutation, tipPixmapForMode]() {
        const QString path = FileDialog::getOpenFileName(this, FileType::IMAGE, tr("Import Tip Mask"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        const QByteArray data = f.readAll();
        f.close();

        applyPresetMutation([data, path](ToolOptionWidget::BrushPreset& p) {
            p.tipAssetId = BrushAssetStore::instance()->importTip(data, path);
            p.tipMode = ToolOptionWidget::BrushPreset::TipMode::Custom;
        });

        // Sync selector to Custom without re-triggering the signal
        if (mTipModeSelector) {
            QSignalBlocker b(mTipModeSelector);
            mTipModeSelector->setCurrentIndex(2);
        }
        if (mTipNameLabel) mTipNameLabel->setText(QFileInfo(path).fileName());
        if (mTipPreviewLabel) {
            QImage img;
            if (img.loadFromData(data))
                mTipPreviewLabel->setPixmap(
                    QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                   Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });

    // Remove custom tip — revert to Hard
    connect(removeTipButton, &CosmicButton::clicked, this, [this, applyPresetMutation, tipPixmapForMode]() {
        applyPresetMutation([](ToolOptionWidget::BrushPreset& p) {
            p.tipAssetId.clear();
            p.tipMode = ToolOptionWidget::BrushPreset::TipMode::Hard;
        });
        if (mTipModeSelector) {
            QSignalBlocker b(mTipModeSelector);
            mTipModeSelector->setCurrentIndex(0, true);
        }
        if (mTipNameLabel)    mTipNameLabel->setText(tr("Hard"));
        if (mTipPreviewLabel) mTipPreviewLabel->setPixmap(
            tipPixmapForMode(ToolOptionWidget::BrushPreset::TipMode::Hard, QSize(160, 160)));
    });

    // Gallery — open tip asset gallery and apply the selected tip.
    connect(mGalleryTipButton, &CosmicButton::clicked, this, [this, applyPresetMutation, tipPixmapForMode]() {
        auto* gallery = new CosmicAssetGallery(CosmicAssetGallery::AssetType::Tip, this);
        connect(gallery, &CosmicAssetGallery::assetSelected, this,
                [this, applyPresetMutation](const QString& id) {
            const QByteArray data = BrushAssetStore::instance()->loadTip(id);
            applyPresetMutation([id](ToolOptionWidget::BrushPreset& p) {
                p.tipAssetId = id;
                p.tipMode = ToolOptionWidget::BrushPreset::TipMode::Custom;
            });
            if (mTipModeSelector) {
                QSignalBlocker b(mTipModeSelector);
                mTipModeSelector->setCurrentIndex(2);
            }
            const auto tips = BrushAssetStore::instance()->listTips();
            for (const auto& e : tips) {
                if (e.id == id) {
                    if (mTipNameLabel) mTipNameLabel->setText(e.displayName);
                    break;
                }
            }
            if (mTipPreviewLabel && !data.isEmpty()) {
                QImage img;
                if (img.loadFromData(data))
                    mTipPreviewLabel->setPixmap(
                        QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        });
        gallery->exec();
        gallery->deleteLater();
        // Clear stale hover state — the button never got a leaveEvent while the modal was open.
        if (mGalleryTipButton) {
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(mGalleryTipButton, &leave);
        }
    });

    int idx = 0;
    layout->insertWidget(idx++, headerRow);
    layout->insertWidget(idx++, bodyRow);
}

void CosmicBrushSettings::rebuildTextureUI(QVBoxLayout* layout)
{
    auto* store = BrushPresetStore::instance();

    const std::function<void(const std::function<void(ToolOptionWidget::BrushPreset&)>&)>
        applyPresetMutation = [this, store](const std::function<void(ToolOptionWidget::BrushPreset&)>& mutator) {
        if (mPresetId.isEmpty()) return;
        ToolOptionWidget::BrushPreset p;
        if (!loadPresetFromStore(mPresetId, p)) return;
        mutator(p);
        store->updatePreset(mPresetId, p);
    };

    // ── Header row: [left: name label] [right: Add | Gallery | ... | Remove] ─
    auto* headerRow = new QWidget(mSettingsContentWidget);
    headerRow->setStyleSheet(
        "QWidget#texHeaderRow { background: #1a1a1a; border-radius: " +
        QString::number(CosmicUiMetrics::cornerRadius()) + "px; }");
    headerRow->setObjectName("texHeaderRow");

    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(10, 4, 6, 4);
    headerLayout->setSpacing(6);

    mTextureNameLabel = new QLabel(tr("None"), headerRow);
    mTextureNameLabel->setStyleSheet(
        "color: #888888; font-size: 12px; background: transparent;");
    mTextureNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerLayout->addWidget(mTextureNameLabel, 1);

    // Add button
    mAddTextureButton = new CosmicButton(headerRow);
    mAddTextureButton->setIcon(QIcon(":/Interaction/Add.svg"));
    mAddTextureButton->setIconSize(QSize(16, 16));
    mAddTextureButton->setToolTip(tr("Import texture"));
    mAddTextureButton->setFixedSize(28, 28);

    // Gallery button — opens CosmicAssetGallery filtered to textures.
    mGalleryTextureButton = new CosmicButton(headerRow);
    mGalleryTextureButton->setIcon(QIcon(":/Interaction/ReffWindowGallery.svg"));
    mGalleryTextureButton->setIconSize(QSize(16, 16));
    mGalleryTextureButton->setToolTip(tr("Texture gallery"));
    mGalleryTextureButton->setFixedSize(28, 28);

    // Remove button — far right
    mRemoveTextureButton = new CosmicButton(headerRow);
    mRemoveTextureButton->setIcon(QIcon(":/Interaction/trash.svg"));
    mRemoveTextureButton->setIconSize(QSize(16, 16));
    mRemoveTextureButton->setToolTip(tr("Remove texture from preset"));
    mRemoveTextureButton->setFixedSize(28, 28);

    headerLayout->addWidget(mAddTextureButton);
    headerLayout->addWidget(mGalleryTextureButton);
    headerLayout->addStretch();
    headerLayout->addWidget(mRemoveTextureButton);

    // ── Body row: [left: thumbnail] [right: Scale card + Contrast card] ──────
    // Left — thumbnail (fixed square, same height as two stacked cards)
    mTexturePreviewLabel = new QLabel(mSettingsContentWidget);
    mTexturePreviewLabel->setAlignment(Qt::AlignCenter);
    mTexturePreviewLabel->setStyleSheet(
        "background: #1a1a1a; border-radius: " +
        QString::number(CosmicUiMetrics::cornerRadius()) + "px;");
    mTexturePreviewLabel->setFixedSize(160, 160);

    // Right — two CosmicSliderCards stacked
    mTextureScaleCard    = new CosmicSliderCard(tr("Scale"),    mSettingsContentWidget);
    mTextureContrastCard = new CosmicSliderCard(tr("Contrast"), mSettingsContentWidget);

    // Scale: map 0.25–16.0 → integer 1–64 (×0.25 steps) for the slider
    mTextureScaleCard->setRange(1, 64);
    // Pre-seed label with the widest possible value so the label column never resizes
    mTextureScaleCard->slider()->setLabel(QStringLiteral("16.00x"));
    mTextureContrastCard->setRange(0, 100);
    mTextureContrastSlider = mTextureContrastCard->slider();

    auto* rightCol = new QWidget(mSettingsContentWidget);
    rightCol->setStyleSheet("background: transparent;");
    auto* rightColLayout = new QVBoxLayout(rightCol);
    rightColLayout->setContentsMargins(0, 0, 0, 0);
    rightColLayout->setSpacing(8);
    rightColLayout->addWidget(mTextureScaleCard);
    rightColLayout->addWidget(mTextureContrastCard);

    auto* bodyRow = new QWidget(mSettingsContentWidget);
    bodyRow->setStyleSheet("background: transparent;");
    auto* bodyLayout = new QHBoxLayout(bodyRow);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);
    bodyLayout->addWidget(mTexturePreviewLabel, 0);
    bodyLayout->addWidget(rightCol, 1);

    // ── Wire signals ─────────────────────────────────────────────────────────
    connect(mAddTextureButton, &CosmicButton::clicked, this, [this, applyPresetMutation]() {
        QString path = FileDialog::getOpenFileName(this, FileType::IMAGE, tr("Open Texture"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QByteArray data = f.readAll();
        f.close();

        applyPresetMutation([data, path](ToolOptionWidget::BrushPreset& p) { p.textureAssetId = BrushAssetStore::instance()->importTexture(data, path); });

        QImage img;
        if (img.loadFromData(data) && mTexturePreviewLabel) {
            mTexturePreviewLabel->setPixmap(
                QPixmap::fromImage(img).scaled(QSize(160, 160),
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        if (mTextureNameLabel)
            mTextureNameLabel->setText(QFileInfo(path).fileName());
    });

    connect(mRemoveTextureButton, &CosmicButton::clicked, this, [this, applyPresetMutation]() {
        applyPresetMutation([](ToolOptionWidget::BrushPreset& p) { p.textureAssetId.clear(); });
        if (mTexturePreviewLabel) mTexturePreviewLabel->setPixmap(QPixmap());
        if (mTextureNameLabel)    mTextureNameLabel->setText(tr("None"));
    });

    // Gallery — open texture asset gallery and apply the selected texture.
    connect(mGalleryTextureButton, &CosmicButton::clicked, this, [this, applyPresetMutation]() {
        auto* gallery = new CosmicAssetGallery(CosmicAssetGallery::AssetType::Texture, this);
        connect(gallery, &CosmicAssetGallery::assetSelected, this,
                [this, applyPresetMutation](const QString& id) {
            const QByteArray data = BrushAssetStore::instance()->loadTexture(id);
            applyPresetMutation([id](ToolOptionWidget::BrushPreset& p) {
                p.textureAssetId = id;
            });
            if (mTexturePreviewLabel && !data.isEmpty()) {
                QImage img;
                if (img.loadFromData(data))
                    mTexturePreviewLabel->setPixmap(
                        QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            // Update name label from store listing.
            const auto textures = BrushAssetStore::instance()->listTextures();
            for (const auto& e : textures) {
                if (e.id == id) {
                    if (mTextureNameLabel) mTextureNameLabel->setText(e.displayName);
                    break;
                }
            }
        });
        gallery->exec();
        gallery->deleteLater();
        // Clear stale hover state — the button never got a leaveEvent while the modal was open.
        if (mGalleryTextureButton) {
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(mGalleryTextureButton, &leave);
        }
    });

    connect(mTextureScaleCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        // slider int → real scale: v * 0.25
        const qreal scale = qBound<qreal>(0.25, v * 0.25, 16.0);
        mTextureScaleCard->slider()->setLabel(QString::number(scale, 'f', 2) + "x");
        applyPresetMutation([scale](ToolOptionWidget::BrushPreset& p) {
            p.textureTileScale = scale;
        });
    });

    connect(mTextureContrastCard, &CosmicSliderCard::valueChanged, this, [this, applyPresetMutation](int v) {
        if (mTextureContrastSlider) mTextureContrastSlider->setLabel(QString::number(v));
        applyPresetMutation([v](ToolOptionWidget::BrushPreset& p) {
            p.textureContrastPercent = qBound(0, v, 100);
        });
    });

    int idx = 0;
    layout->insertWidget(idx++, headerRow);
    layout->insertWidget(idx++, bodyRow);
}

void CosmicBrushSettings::rebuildPressureUI(QVBoxLayout* layout)
{
    auto* store = BrushPresetStore::instance();

    const std::function<void(const std::function<void(ToolOptionWidget::BrushPreset&)>&)>
        applyPresetMutation = [this, store](const std::function<void(ToolOptionWidget::BrushPreset&)>& mutator) {
        if (mPresetId.isEmpty()) return;
        ToolOptionWidget::BrushPreset p;
        if (!loadPresetFromStore(mPresetId, p)) return;
        mutator(p);
        store->updatePreset(mPresetId, p);
    };

    auto makeToggle = [this]() {
        auto* t = new CosmicCheckboxToggle(mSettingsContentWidget);
        t->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        t->setScaleMode(CosmicCheckboxToggle::ScaleBigBox);
        t->setShowText(false);
        t->setShowIcon(false);
        t->setAccentColor(BlenderStyleModalPopup::getAccentColor());
        return t;
    };

    // Helper: vertical 1px separator bar
    auto makeSep = [](QWidget* parent) -> QWidget* {
        auto* sep = new QFrame(parent);
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setStyleSheet("background: #333333; border: none;");
        sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        return sep;
    };

    // Helper: toggle + label cell
    auto makeToggleCell = [](CosmicCheckboxToggle* toggle, const QString& label, QWidget* parent) -> QWidget* {
        auto* cell = new QWidget(parent);
        cell->setStyleSheet("background: transparent;");
        auto* h = new QHBoxLayout(cell);
        h->setContentsMargins(10, 1, 10, 1);
        h->setSpacing(6);
        h->addWidget(toggle);
        auto* lbl = new QLabel(label, cell);
        lbl->setStyleSheet("color: #cccccc; font-size: 13px; font-weight: 500; background: transparent;");
        h->addWidget(lbl);
        h->addStretch(1);
        return cell;
    };

    mSizePressureToggle  = makeToggle();
    mFlowPressureToggle  = makeToggle();

    // ── Top card: toggle row ─────────────────────────────────────────────────
    auto* topCard = new QWidget(mSettingsContentWidget);
    topCard->setStyleSheet(
        "QWidget#topCard { background: #1a1a1a; border-radius: " +
        QString::number(CosmicUiMetrics::cornerRadius()) + "px; }");
    topCard->setObjectName("topCard");

    auto* topCardLayout = new QHBoxLayout(topCard);
    topCardLayout->setContentsMargins(0, 0, 0, 0);
    topCardLayout->setSpacing(0);
    topCardLayout->addWidget(makeToggleCell(mSizePressureToggle, tr("Size"),    topCard), 1);

    // Right half of top card — separator + opacity cell, dimmed in Final Line mode
    auto* topRightHalf = new QWidget(topCard);
    topRightHalf->setStyleSheet("background: transparent;");
    auto* topRightLayout = new QHBoxLayout(topRightHalf);
    topRightLayout->setContentsMargins(0, 0, 0, 0);
    topRightLayout->setSpacing(0);
    topRightLayout->addWidget(makeSep(topRightHalf));
    topRightLayout->addWidget(makeToggleCell(mFlowPressureToggle, tr("Opacity"), topRightHalf), 1);
    if (mFinalLineMode) topRightHalf->setVisible(false);
    topCardLayout->addWidget(topRightHalf, 1);

    // ── Bottom card: curve editors ───────────────────────────────────────────
    auto* bottomCard = new QWidget(mSettingsContentWidget);
    bottomCard->setStyleSheet(
        "QWidget#bottomCard { background: #1a1a1a; border-radius: " +
        QString::number(CosmicUiMetrics::cornerRadius()) + "px; }");
    bottomCard->setObjectName("bottomCard");

    mSizeCurveEditor = new BezierCurveEditor(bottomCard);
    mSizeCurveEditor->setMinimumSize(120, 150);
    mSizeCurveEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    mFlowCurveEditor = new BezierCurveEditor(bottomCard);
    mFlowCurveEditor->setMinimumSize(120, 150);
    mFlowCurveEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* bottomCardLayout = new QHBoxLayout(bottomCard);
    bottomCardLayout->setContentsMargins(8, 8, 8, 8);
    bottomCardLayout->setSpacing(0);
    bottomCardLayout->addWidget(mSizeCurveEditor, 1);

    // Right half of bottom card — separator + flow curve, dimmed in Final Line mode
    auto* bottomRightHalf = new QWidget(bottomCard);
    bottomRightHalf->setStyleSheet("background: transparent;");
    auto* bottomRightLayout = new QHBoxLayout(bottomRightHalf);
    bottomRightLayout->setContentsMargins(0, 0, 0, 0);
    bottomRightLayout->setSpacing(0);
    bottomRightLayout->addWidget(makeSep(bottomRightHalf));
    bottomRightLayout->addWidget(mFlowCurveEditor, 1);
    if (mFinalLineMode) bottomRightHalf->setVisible(false);
    bottomCardLayout->addWidget(bottomRightHalf, 1);

    // ── Seed Final Line live state into freshly-created widgets ─────────────
    // setupTabBar() recreates the widgets, so mFinalLineLiveCurve/Enabled (which
    // were set by setFinalLineLiveState before the rebuild) must be re-applied here.
    if (mFinalLineMode) {
        if (mSizePressureToggle) {
            QSignalBlocker b(mSizePressureToggle);
            mSizePressureToggle->setChecked(mFinalLineLiveEnabled);
        }
        if (mSizeCurveEditor && !mFinalLineLiveCurve.isEmpty()) {
            QSignalBlocker b(mSizeCurveEditor);
            mSizeCurveEditor->setControlPoints(mFinalLineLiveCurve);
        }
    }

    // ── Wire signals ─────────────────────────────────────────────────────────
    if (mFinalLineMode) {
        connect(mSizePressureToggle, &CosmicCheckboxToggle::toggled, this, [this](bool on) {
            mFinalLineLiveEnabled = on;
            emit finalLinePressureChanged(mFinalLineLiveEnabled, mFinalLineLiveCurve);
        });
        connect(mSizeCurveEditor, &BezierCurveEditor::curveCommitted, this,
                [this](const QVector<QPointF>& pts) {
            mFinalLineLiveCurve = pts;
            emit finalLinePressureChanged(mFinalLineLiveEnabled, mFinalLineLiveCurve);
        });
    } else {
        connect(mSizePressureToggle, &CosmicCheckboxToggle::toggled, this, [applyPresetMutation](bool on) {
            applyPresetMutation([on](ToolOptionWidget::BrushPreset& p) { p.thicknessCurveEnabled = on; });
        });
        connect(mSizeCurveEditor, &BezierCurveEditor::curveCommitted, this,
                [applyPresetMutation](const QVector<QPointF>& pts) {
            applyPresetMutation([pts](ToolOptionWidget::BrushPreset& p) { p.thicknessCurvePoints = pts; });
        });
        connect(mFlowPressureToggle, &CosmicCheckboxToggle::toggled, this, [applyPresetMutation](bool on) {
            applyPresetMutation([on](ToolOptionWidget::BrushPreset& p) { p.opacityCurveEnabled = on; });
        });
        connect(mFlowCurveEditor, &BezierCurveEditor::curveCommitted, this,
                [applyPresetMutation](const QVector<QPointF>& pts) {
            applyPresetMutation([pts](ToolOptionWidget::BrushPreset& p) { p.opacityCurvePoints = pts; });
        });
    }

    int idx = 0;
    layout->insertWidget(idx++, topCard);
    layout->insertWidget(idx++, bottomCard);
}

bool CosmicBrushSettings::loadPresetFromStore(const QString& presetId, ToolOptionWidget::BrushPreset& outPreset) const
{
    if (presetId.isEmpty()) {
        return false;
    }

    // Final Line reads from the cached live pen tool state, not QSettings.
    if (presetId == QStringLiteral("__final_line__")) {
        outPreset.id = QStringLiteral("__final_line__");
        outPreset.name = QStringLiteral("Pen");
        outPreset.thicknessCurveEnabled = mFinalLineLiveEnabled;
        outPreset.thicknessCurvePoints = mFinalLineLiveCurve.isEmpty()
            ? QVector<QPointF>{ QPointF(0.0, 0.0), QPointF(0.5, 0.5), QPointF(1.0, 1.0) }
            : mFinalLineLiveCurve;
        return true;
    }

    auto* store = BrushPresetStore::instance();
    store->ensureLoaded();
    const auto presets = store->presets();
    for (const auto& p : presets) {
        if (p.id == presetId) {
            outPreset = p;
            return true;
        }
    }
    return false;
}

void CosmicBrushSettings::syncControlsFromStore()
{
    if (mSyncInProgress) {
        return;
    }
    mSyncInProgress = true;

    if (mPresetId.isEmpty()) {
        mSyncInProgress = false;
        return;
    }

    // Avoid fighting user interaction: if user is dragging any slider, don't sync.
    if ((mSpacingSlider && mSpacingSlider->isSliderDown())
        || (mScatterSlider && mScatterSlider->isSliderDown())
        || (mRotationSlider && mRotationSlider->isSliderDown())
        || (mSizeJitterSlider && mSizeJitterSlider->isSliderDown())
        || (mColorJitterSlider && mColorJitterSlider->isSliderDown())
        || (mTextureContrastSlider && mTextureContrastSlider->isSliderDown())) {
        mSyncInProgress = false;
        return;
    }

    ToolOptionWidget::BrushPreset p;
    if (!loadPresetFromStore(mPresetId, p)) {
        mSyncInProgress = false;
        return;
    }

    // --- Stroke ---
    if (mSpacingSlider) {
        QSignalBlocker b(mSpacingSlider);
        mSpacingSlider->setValue(qRound(qBound<qreal>(0.05, p.spacingMult, 10.0) * 100.0));
        mSpacingSlider->setLabel(QString::number(mSpacingSlider->value()));
    }
    if (mScatterSlider) {
        QSignalBlocker b(mScatterSlider);
        mScatterSlider->setValue(qRound(qBound<qreal>(0.0, p.scatter, 3.0) * 100.0));
        mScatterSlider->setLabel(QString::number(mScatterSlider->value()));
    }
    if (mRotationSlider) {
        QSignalBlocker b(mRotationSlider);
        mRotationSlider->setValue(qRound(qBound<qreal>(0.0, p.rotationDeg, 12.0)));
        mRotationSlider->setLabel(QString::number(mRotationSlider->value()));
    }
    if (mSizeJitterSlider) {
        const int v = qBound(0, qRound(p.sizeJitter * 100.0), 50);
        QSignalBlocker b(mSizeJitterSlider);
        mSizeJitterSlider->setValue(v);
        mSizeJitterSlider->setLabel(QString::number(v));
    }
    if (mColorJitterSlider) {
        const int v = qBound(0, qRound(p.colorJitter * 100.0), 100);
        QSignalBlocker b(mColorJitterSlider);
        mColorJitterSlider->setValue(v);
        mColorJitterSlider->setLabel(QString::number(v));
    }

    // --- Tip ---
    if (mTipModeSelector) {
        using TipMode = ToolOptionWidget::BrushPreset::TipMode;
        // If Custom but no data, treat as Hard (fallback)
        const TipMode effectiveMode = (p.tipMode == TipMode::Custom && p.tipAssetId.isEmpty())
                                    ? TipMode::Hard : p.tipMode;
        const int tipIdx = (effectiveMode == TipMode::Soft)   ? 1
                         : (effectiveMode == TipMode::Custom) ? 2 : 0;
        QSignalBlocker b(mTipModeSelector);
        mTipModeSelector->setCurrentIndex(tipIdx);
        const QString name = (tipIdx == 2) ? tr("Custom")
                           : (tipIdx == 1) ? tr("Soft") : tr("Hard");
        if (mTipNameLabel) mTipNameLabel->setText(name);
        if (mTipPreviewLabel) {
            if (effectiveMode == TipMode::Custom && !p.tipAssetId.isEmpty()) {
                QImage img;
                if (img.loadFromData(BrushAssetStore::instance()->loadTip(p.tipAssetId)))
                    mTipPreviewLabel->setPixmap(
                        QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                QString path;
                switch (effectiveMode) {
                case TipMode::Soft:   path = QStringLiteral(":/BrushTips/CosmicDefaultTip_Soft.png"); break;
                default:              path = QStringLiteral(":/BrushTips/CosmicDefaultTip_Hard.png"); break;
                }
                QImage img(path);
                if (!img.isNull())
                    mTipPreviewLabel->setPixmap(
                        QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    }
    if (mTipContrastSlider) {
        QSignalBlocker b(mTipContrastSlider);
        mTipContrastSlider->setValue(qBound(0, p.tipContrastPercent, 100));
        mTipContrastSlider->setLabel(QString::number(mTipContrastSlider->value()));
    }

    // --- Texture ---
    if (mTextureScaleCard) {
        QSignalBlocker b(mTextureScaleCard->slider());
        const int sliderVal = qBound(1, qRound(p.textureTileScale / 0.25), 64);
        mTextureScaleCard->setValue(sliderVal);
        mTextureScaleCard->slider()->setLabel(
            QString::number(sliderVal * 0.25, 'f', 2) + "x");
    }
    if (mTextureContrastSlider) {
        QSignalBlocker b(mTextureContrastSlider);
        mTextureContrastSlider->setValue(qBound(0, p.textureContrastPercent, 100));
        mTextureContrastSlider->setLabel(QString::number(mTextureContrastSlider->value()));
    }
    if (mTexturePreviewLabel) {
        if (!p.textureAssetId.isEmpty()) {
            QImage img;
            if (img.loadFromData(BrushAssetStore::instance()->loadTexture(p.textureAssetId))) {
                mTexturePreviewLabel->setPixmap(
                    QPixmap::fromImage(img).scaled(QSize(160, 160),
                                                   Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        } else {
            mTexturePreviewLabel->setPixmap(QPixmap());
        }
    }
    if (mTextureNameLabel) {
        mTextureNameLabel->setText(p.textureAssetId.isEmpty() ? tr("None") : tr("Custom"));
    }

    // --- Pressure ---
    if (mSizePressureToggle) {
        QSignalBlocker b(mSizePressureToggle);
        mSizePressureToggle->setChecked(p.thicknessCurveEnabled);
    }
    if (mFlowPressureToggle) {
        QSignalBlocker b(mFlowPressureToggle);
        mFlowPressureToggle->setChecked(p.opacityCurveEnabled);
    }
    if (mSizeCurveEditor) {
        QSignalBlocker b(mSizeCurveEditor);
        mSizeCurveEditor->setControlPoints(p.thicknessCurvePoints);
    }
    if (mFlowCurveEditor) {
        QSignalBlocker b(mFlowCurveEditor);
        mFlowCurveEditor->setControlPoints(p.opacityCurvePoints);
    }

    mSyncInProgress = false;
}

void CosmicBrushSettings::onPresetsUpdated(const QVector<ToolOptionWidget::BrushPreset>& presets, const QString& activePresetId)
{
    Q_UNUSED(presets);
    Q_UNUSED(activePresetId);
    // Final Line settings are not in the store — ignore store updates when in Final Line mode.
    if (mFinalLineMode) return;
    // Don't queue a sync if we're already mid-rebuild or mid-sync.
    if (mSyncInProgress || mSyncQueued) return;
    mSyncQueued = true;
    QTimer::singleShot(0, this, [this]() {
        mSyncQueued = false;
        syncControlsFromStore();
    });
}

void CosmicBrushSettings::applyBrushesSliderStyle(QWidget* widget)
{
    if (!widget) {
        return;
    }

    if (widget->property("cosmicBrushesSliderStyleApplied").toBool()) {
        return;
    }
    widget->setProperty("cosmicBrushesSliderStyleApplied", true);

    const QString sliderStyle = QStringLiteral(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 26px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  height: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-width: 26px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: transparent; width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
    );

    // Apply to the widget itself.
    // Avoid concatenating existing styles to prevent unbounded growth and Qt instability.
    widget->setStyleSheet(sliderStyle);
}
