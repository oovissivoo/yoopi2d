#include "cosmicexportanimationpopup.h"

#include "cosmicwindowheader.h"
#include "cosmicfilebrowserpopup.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicenumselector.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

// ── helpers ───────────────────────────────────────────────────────────────────

namespace {

// Dark rounded card container
QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
{
    auto* card = new QWidget(parent);
    card->setStyleSheet(QStringLiteral(
        "QWidget{ background: #1e1e1e; border-radius: 8px; }"));
    outLay = new QVBoxLayout(card);
    outLay->setContentsMargins(14, 12, 14, 12);
    outLay->setSpacing(10);
    return card;
}

// Centered bold section title
QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff;"
        " font-size: 11px; font-weight: 600; }"));
    return lbl;
}

// Muted secondary label
QLabel* makeHint(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #666666; font-size: 10px; }"));
    lbl->setWordWrap(true);
    return lbl;
}

// Regular label
QLabel* makeLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 11px; }"));
    return lbl;
}

QFrame* makeSeparator(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("QFrame{ background: #2a2a2a; border: none; }"));
    return sep;
}

CosmicInputField* makeIntField(QWidget* parent, int min, int max, int val)
{
    auto* f = new CosmicInputField(parent);
    f->setVariant(CosmicInputField::Minimal);
    f->setRange(min, max);
    f->setDecimals(0);
    f->setSingleStep(1);
    f->setValue(val);
    f->setFixedHeight(CosmicUiMetrics::controlThickness());
    return f;
}

} // namespace

// ── constructor ───────────────────────────────────────────────────────────────

CosmicExportAnimationPopup::CosmicExportAnimationPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(420, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2a, 0x2a, 0x2a));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI();
}

void CosmicExportAnimationPopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("ExportAnimRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#ExportAnimRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // ── header ────────────────────────────────────────────────────────────────
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Export Animation"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff;"
        " font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── destination card ──────────────────────────────────────────────────────
    QVBoxLayout* destLay = nullptr;
    auto* destCard = makeCard(root, destLay);
    lay->addWidget(destCard);

    destLay->addWidget(makeSectionTitle(tr("Destination"), destCard));
    destLay->addWidget(makeSeparator(destCard));

    // Path label + borderless folder-icon Browse button
    auto* destRow = new QHBoxLayout();
    destRow->setSpacing(6);

    mDestLabel = new QLabel(QDir::homePath(), destCard);
    mDestLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #888888; font-size: 11px; }"));
    mDestLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mDestLabel->setWordWrap(false);
    mDestLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    destRow->addWidget(mDestLabel, 1);

    auto* browseBtn = new CosmicButton(destCard);
    browseBtn->setBordered(false);
    browseBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/Folder.svg")));
    browseBtn->setIconSize(QSize(18, 18));
    browseBtn->setFixedSize(28, 28);
    browseBtn->setToolTip(tr("Browse for destination folder"));
    destRow->addWidget(browseBtn);
    destLay->addLayout(destRow);

    // ── name (main folder, always created) ───────────────────────────────────
    auto* folderRow = new QHBoxLayout();
    folderRow->setSpacing(10);
    folderRow->addWidget(makeLabel(tr("Name"), destCard));
    mFolderNameEdit = new QLineEdit(destCard);
    mFolderNameEdit->setPlaceholderText(tr("e.g. frames"));
    mFolderNameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{ background: #252525; color: #b3b3b3;"
        " border: 1px solid #333333; border-radius: 4px;"
        " padding: 3px 8px; font-size: 11px; }"));
    mFolderNameEdit->setFixedHeight(CosmicUiMetrics::controlThickness());
    mFolderNameEdit->setToolTip(tr(
        "Leave blank to place each layer's frames directly in the destination folder.\n"
        "When filled, all layer folders are grouped inside this named folder."));
    folderRow->addWidget(mFolderNameEdit, 1);
    destLay->addLayout(folderRow);

    // ── format & resolution card ──────────────────────────────────────────────
    QVBoxLayout* fmtLay = nullptr;
    auto* fmtCard = makeCard(root, fmtLay);
    lay->addWidget(fmtCard);

    fmtLay->addWidget(makeSectionTitle(tr("Format & Resolution"), fmtCard));
    fmtLay->addWidget(makeSeparator(fmtCard));

    auto* fmtRow = new QHBoxLayout();
    fmtRow->setSpacing(10);
    fmtRow->addWidget(makeLabel(tr("Format"), fmtCard));
    mFormatDrop = new CosmicDropDown(fmtCard);
    mFormatDrop->setOptions({QStringLiteral("PNG"), QStringLiteral("JPG"),
                             QStringLiteral("BMP"), QStringLiteral("TIFF"),
                             QStringLiteral("WEBP")});
    mFormatDrop->setCurrentIndex(0);
    mFormatDrop->setFixedHeight(CosmicUiMetrics::controlThickness());
    fmtRow->addWidget(mFormatDrop, 1);
    fmtLay->addLayout(fmtRow);

    mResCard = new QWidget(fmtCard);
    auto* resLay = new QHBoxLayout(mResCard);
    resLay->setContentsMargins(0, 0, 0, 0);
    resLay->setSpacing(10);
    resLay->addWidget(makeLabel(tr("Width"), mResCard));
    mWidthField = makeIntField(mResCard, 1, 9999, 1920);
    resLay->addWidget(mWidthField, 1);
    resLay->addWidget(makeLabel(tr("Height"), mResCard));
    mHeightField = makeIntField(mResCard, 1, 9999, 1080);
    resLay->addWidget(mHeightField, 1);
    fmtLay->addWidget(mResCard);

    // Transparent background toggle
    auto* transRow = new QHBoxLayout();
    transRow->setSpacing(10);
    transRow->addWidget(makeLabel(tr("Transparent Background"), fmtCard), 1);
    mTransparencyToggle = new CosmicCheckboxToggle(fmtCard);
    mTransparencyToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mTransparencyToggle->setFixedSize(22, 22);
    transRow->addWidget(mTransparencyToggle);
    fmtLay->addLayout(transRow);

    // ── layer mode card ───────────────────────────────────────────────────────
    QVBoxLayout* modeLay = nullptr;
    auto* modeCard = makeCard(root, modeLay);
    lay->addWidget(modeCard);

    modeLay->addWidget(makeSectionTitle(tr("Layer Mode"), modeCard));
    modeLay->addWidget(makeSeparator(modeCard));

    auto* activeRow = new QHBoxLayout();
    activeRow->setSpacing(10);
    activeRow->addWidget(makeLabel(tr("Active Layer Only"), modeCard), 1);
    mActiveLayerToggle = new CosmicCheckboxToggle(modeCard);
    mActiveLayerToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mActiveLayerToggle->setFixedSize(22, 22);
    activeRow->addWidget(mActiveLayerToggle);
    modeLay->addLayout(activeRow);

    modeLay->addWidget(makeSeparator(modeCard));

    // Frame mode — replaces "Export Specific Frames" toggle + dropdown
    modeLay->addWidget(makeSectionTitle(tr("Frames"), modeCard));
    mFrameModeSelector = new CosmicEnumSelector(modeCard);
    mFrameModeSelector->setOptions({tr("Sequence"), tr("Keyframes Only"), tr("Drawings Only")});
    mFrameModeSelector->setCurrentIndex(0);
    modeLay->addWidget(mFrameModeSelector);
    modeLay->addWidget(makeHint(
        tr("Sequence exports every frame. Keyframes Only exports frames with a keyframe. "
           "Drawings Only exports each unique drawing once."),
        modeCard));

    // ── action row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch(1);

    mCancelBtn = new CosmicButton(tr("Cancel"), root);
    mCancelBtn->setBordered(true);
    mCancelBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mCancelBtn);

    mExportBtn = new CosmicButton(tr("Export"), root);
    mExportBtn->setBordered(false);
    const QColor accent = CosmicColor::accent();
    mExportBtn->setBackgroundColors(accent, accent.lighter(115), accent.darker(115));
    mExportBtn->setTextColor(Qt::white);
    mExportBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mExportBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    // ── connections ───────────────────────────────────────────────────────────
    connect(mHeader,    &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(mCancelBtn, &QPushButton::clicked,               this, &QDialog::reject);
    connect(mExportBtn, &QPushButton::clicked,               this, &QDialog::accept);
    connect(browseBtn,  &QPushButton::clicked,               this, &CosmicExportAnimationPopup::onBrowseDest);
    connect(mActiveLayerToggle, &CosmicCheckboxToggle::toggled,
            this, &CosmicExportAnimationPopup::onActiveLayerToggled);
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicExportAnimationPopup::onBrowseDest()
{
    const QString current = mDestLabel ? mDestLabel->text() : QDir::homePath();
    CosmicFileBrowserPopup browser(
        CosmicFileBrowserPopup::Mode::OpenFile,
        tr("Select destination folder"),
        current,
        QStringList(),
        this);
    browser.setFolderMode(true);
    const CosmicFileBrowserPopup::Result r = browser.execAndGetResult();
    if (r.accepted && !r.selectedPaths.isEmpty()) {
        if (mDestLabel) mDestLabel->setText(r.selectedPaths.first());
    }
}

void CosmicExportAnimationPopup::onActiveLayerToggled(bool on)
{
    if (mResCard) mResCard->setVisible(!on);
    clearFixedWindowSize();
    adjustSize();
}



// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicExportAnimationPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(420);
    setMaximumWidth(420);
    adjustSize();

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize  s = g0.size();
    const QSize  s0(qMax(1, int(s.width() * 0.93)), qMax(1, int(s.height() * 0.93)));
    const QRect  g1(QPoint(c.x() - s0.width()/2, c.y() - s0.height()/2), s0);
    setGeometry(g1);

    auto* op = new QPropertyAnimation(this, "windowOpacity", this);
    op->setDuration(80); op->setStartValue(0.0); op->setEndValue(1.0);
    auto* gm = new QPropertyAnimation(this, "geometry", this);
    gm->setDuration(95); gm->setStartValue(g1); gm->setEndValue(g0);
    auto* grp = new QParallelAnimationGroup(this);
    grp->addAnimation(op); grp->addAnimation(gm);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── public setters ────────────────────────────────────────────────────────────

void CosmicExportAnimationPopup::setResolution(const QSize& size)
{
    if (mWidthField)  mWidthField->setValue(size.width());
    if (mHeightField) mHeightField->setValue(size.height());
}

// ── result accessors ──────────────────────────────────────────────────────────

QString CosmicExportAnimationPopup::destDir() const
{
    return mDestLabel ? mDestLabel->text() : QString();
}

QString CosmicExportAnimationPopup::mainFolderName() const
{
    return mFolderNameEdit ? mFolderNameEdit->text() : QString();
}

QString CosmicExportAnimationPopup::exportFormat() const
{
    return mFormatDrop ? mFormatDrop->currentText() : QStringLiteral("PNG");
}

QSize CosmicExportAnimationPopup::exportSize() const
{
    const int w = mWidthField  ? static_cast<int>(mWidthField->value())  : 1920;
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : 1080;
    return QSize(w, h);
}

bool CosmicExportAnimationPopup::transparency() const
{
    return mTransparencyToggle && mTransparencyToggle->isChecked();
}

bool CosmicExportAnimationPopup::isActiveLayerMode() const
{
    return mActiveLayerToggle && mActiveLayerToggle->isChecked();
}

CosmicExportAnimationPopup::FrameMode CosmicExportAnimationPopup::frameMode() const
{
    if (!mFrameModeSelector) return FrameMode::Sequence;
    switch (mFrameModeSelector->currentIndex()) {
    case 1:  return FrameMode::KeyframesOnly;
    case 2:  return FrameMode::DrawingsOnly;
    default: return FrameMode::Sequence;
    }
}

// Legacy API compat — callers that used isExportSpecificEnabled / exportSpecificMode
bool CosmicExportAnimationPopup::isExportSpecificEnabled() const
{
    return frameMode() != FrameMode::Sequence;
}

CosmicExportAnimationPopup::ExportSpecificMode
CosmicExportAnimationPopup::exportSpecificMode() const
{
    return (frameMode() == FrameMode::DrawingsOnly)
        ? ExportSpecificMode::CellsOnly
        : ExportSpecificMode::Keyframes;
}
