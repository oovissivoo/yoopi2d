#include "cosmicextraspacepopup.h"

#include "cosmicwindowheader.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicbutton.h"
#include "preferencemanager.h"
#include "preferencesdef.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

CosmicExtraSpacePopup::CosmicExtraSpacePopup(PreferenceManager* prefs, QWidget* parent)
    : CosmicPopupWindow(parent)
    , mPrefs(prefs)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(34);
    setFixedWindowSize(QSize(340, 200));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicExtraSpacePopupRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#CosmicExtraSpacePopupRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // Header
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // Title
    auto* titleLabel = new QLabel(tr("Extra Space"), root);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 13px; }"));
    lay->addWidget(titleLabel);

    const int cardRadius = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(root);
    card->setObjectName(QStringLiteral("ExtraSpaceCard"));
    card->setStyleSheet(QString(
        "QWidget#ExtraSpaceCard{ background: #1e1e1e; border-radius: %1px; }").arg(cardRadius));

    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(12, 10, 12, 10);
    cardLay->setSpacing(8);

    // Style row
    auto* styleRow = new QWidget(card);
    auto* styleLayout = new QHBoxLayout(styleRow);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    auto* styleLabel = new QLabel(tr("Style"), styleRow);
    styleLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 12px; }"));
    mStyleDropdown = new CosmicDropDown(styleRow);
    mStyleDropdown->setOptions(QStringList() << tr("Auto") << tr("Manual"));
    styleLayout->addWidget(styleLabel);
    styleLayout->addStretch(1);
    styleLayout->addWidget(mStyleDropdown);
    cardLay->addWidget(styleRow);

    // Auto row: scale percentage
    mAutoRow = new QWidget(card);
    auto* autoLayout = new QHBoxLayout(mAutoRow);
    autoLayout->setContentsMargins(0, 0, 0, 0);
    auto* scaleLabel = new QLabel(tr("Scale"), mAutoRow);
    scaleLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 12px; }"));
    mScaleField = new CosmicInputField(mAutoRow);
    mScaleField->setVariant(CosmicInputField::Minimal);
    mScaleField->setRange(100, 200);
    mScaleField->setDecimals(0);
    mScaleField->setSingleStep(10);
    mScaleField->setSuffix(QStringLiteral("%"));
    autoLayout->addWidget(scaleLabel);
    autoLayout->addStretch(1);
    autoLayout->addWidget(mScaleField);
    cardLay->addWidget(mAutoRow);

    // Manual row: W + H inputs
    mManualRow = new QWidget(card);
    auto* manualLayout = new QHBoxLayout(mManualRow);
    manualLayout->setContentsMargins(0, 0, 0, 0);
    manualLayout->setSpacing(6);
    auto* xLabel = new QLabel(tr("X"), mManualRow);
    xLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 12px; }"));
    xLabel->setFixedWidth(14);
    mWField = new CosmicInputField(mManualRow);
    mWField->setVariant(CosmicInputField::Minimal);
    mWField->setDecimals(0);
    mWField->setSingleStep(1);
    auto* yLabel = new QLabel(tr("Y"), mManualRow);
    yLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 12px; }"));
    yLabel->setFixedWidth(14);
    mHField = new CosmicInputField(mManualRow);
    mHField->setVariant(CosmicInputField::Minimal);
    mHField->setDecimals(0);
    mHField->setSingleStep(1);
    manualLayout->addWidget(xLabel);
    manualLayout->addWidget(mWField, 1);
    manualLayout->addSpacing(8);
    manualLayout->addWidget(yLabel);
    manualLayout->addWidget(mHField, 1);
    cardLay->addWidget(mManualRow);

    lay->addWidget(card);

    // Buttons
    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(10);
    btnRow->addStretch(1);

    auto* cancelBtn = new CosmicButton(tr("Cancel"), root);
    cancelBtn->setBordered(true);
    btnRow->addWidget(cancelBtn);

    auto* applyBtn = new CosmicButton(tr("Apply"), root);
    applyBtn->setBordered(true);
    btnRow->addWidget(applyBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    // Connections
    connect(mHeader,   &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked,               this, &QDialog::reject);

    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        if (!mPrefs) { accept(); return; }
        int style = mStyleDropdown ? mStyleDropdown->currentIndex() : 0;
        mPrefs->set(SETTING::EXTRA_SPACE_STYLE, style);
        if (style == 0) {
            // Auto
            int scale = mScaleField ? static_cast<int>(mScaleField->value()) : 150;
            scale = qBound(100, scale, 200);
            mPrefs->set(SETTING::EXTRA_SPACE_SCALE_PERCENT, scale);
        } else {
            // Manual
            int baseW = qMax(1, mPrefs->getInt(SETTING::FIELD_W));
            int baseH = qMax(1, mPrefs->getInt(SETTING::FIELD_H));
            int w = mWField ? static_cast<int>(mWField->value()) : baseW;
            int h = mHField ? static_cast<int>(mHField->value()) : baseH;
            w = qBound(baseW, w, baseW * 2);
            h = qBound(baseH, h, baseH * 2);
            mPrefs->set(SETTING::EXTRA_SPACE_MANUAL_W, w);
            mPrefs->set(SETTING::EXTRA_SPACE_MANUAL_H, h);
        }
        accept();
    });

    connect(mStyleDropdown, &CosmicDropDown::currentIndexChanged, this, [this](int idx) {
        if (mAutoRow)   mAutoRow->setVisible(idx == 0);
        if (mManualRow) mManualRow->setVisible(idx == 1);
    });
}

void CosmicExtraSpacePopup::syncFromPrefs()
{
    if (!mPrefs) return;

    int style = mPrefs->getInt(SETTING::EXTRA_SPACE_STYLE);
    if (style != 0 && style != 1) style = 0;

    if (mStyleDropdown) mStyleDropdown->setCurrentIndex(style);
    if (mAutoRow)   mAutoRow->setVisible(style == 0);
    if (mManualRow) mManualRow->setVisible(style == 1);

    int scale = qBound(100, mPrefs->getInt(SETTING::EXTRA_SPACE_SCALE_PERCENT), 200);
    if (mScaleField) mScaleField->setValue(scale);

    int baseW = qMax(1, mPrefs->getInt(SETTING::FIELD_W));
    int baseH = qMax(1, mPrefs->getInt(SETTING::FIELD_H));
    int w = qBound(baseW, mPrefs->getInt(SETTING::EXTRA_SPACE_MANUAL_W), baseW * 2);
    int h = qBound(baseH, mPrefs->getInt(SETTING::EXTRA_SPACE_MANUAL_H), baseH * 2);
    if (mWField) { mWField->setRange(baseW, baseW * 2); mWField->setValue(w); }
    if (mHField) { mHField->setRange(baseH, baseH * 2); mHField->setValue(h); }
}

void CosmicExtraSpacePopup::showEvent(QShowEvent* e)
{
    syncFromPrefs();
    CosmicPopupWindow::showEvent(e);

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize  s = g0.size();
    const QSize  s0(qMax(1, int(s.width() * 0.92)), qMax(1, int(s.height() * 0.92)));
    const QRect  g1(QPoint(c.x() - s0.width() / 2, c.y() - s0.height() / 2), s0);
    setGeometry(g1);

    auto* opacity = new QPropertyAnimation(this, "windowOpacity", this);
    opacity->setDuration(80);
    opacity->setStartValue(0.0);
    opacity->setEndValue(1.0);

    auto* geom = new QPropertyAnimation(this, "geometry", this);
    geom->setDuration(95);
    geom->setStartValue(g1);
    geom->setEndValue(g0);

    auto* grp = new QParallelAnimationGroup(this);
    grp->addAnimation(opacity);
    grp->addAnimation(geom);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}
