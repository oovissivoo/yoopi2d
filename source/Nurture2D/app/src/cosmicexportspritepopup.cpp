#include "cosmicexportspritepopup.h"

#include "cosmicwindowheader.h"
#include "cosmicfilebrowserpopup.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

// ── helpers ───────────────────────────────────────────────────────────────────

namespace {

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

QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff;"
        " font-size: 11px; font-weight: 600; }"));
    return lbl;
}

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

// Creates a full-width toggle button for the vertical layout selector
QPushButton* makeLayoutBtn(const QString& label, QWidget* parent)
{
    auto* btn = new QPushButton(label, parent);
    btn->setCheckable(true);
    btn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Styling is applied dynamically via toggled signal; set base style here
    btn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #252525; color: #aaaaaa;"
        "  border: 1px solid #333333; border-radius: 4px;"
        "  font-size: 11px; }"
        "QPushButton:checked {"
        "  background: #5b5bff; color: #ffffff;"
        "  border: 1px solid #5b5bff; }"
        "QPushButton:hover:!checked {"
        "  background: #2e2e2e; color: #cccccc; }"));
    return btn;
}

} // namespace

// ── constructor ───────────────────────────────────────────────────────────────

CosmicExportSpritePopup::CosmicExportSpritePopup(QWidget* parent)
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

void CosmicExportSpritePopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("ExportSpriteRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#ExportSpriteRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // ── header ────────────────────────────────────────────────────────────────
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Export Sprite Sheet"), root);
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

    // Location row: label + path + browse button
    auto* locRow = new QHBoxLayout();
    locRow->setSpacing(6);
    locRow->addWidget(makeLabel(tr("Location"), destCard));

    mDestLabel = new QLabel(QDir::homePath(), destCard);
    mDestLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #888888; font-size: 11px; }"));
    mDestLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mDestLabel->setWordWrap(false);
    mDestLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    locRow->addWidget(mDestLabel, 1);

    auto* browseBtn = new CosmicButton(destCard);
    browseBtn->setBordered(false);
    browseBtn->setIcon(QIcon(QStringLiteral(":/FileBrowser/Folder.svg")));
    browseBtn->setIconSize(QSize(18, 18));
    browseBtn->setFixedSize(28, 28);
    browseBtn->setToolTip(tr("Browse for destination folder"));
    locRow->addWidget(browseBtn);
    destLay->addLayout(locRow);

    // Name row
    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(10);
    nameRow->addWidget(makeLabel(tr("Name"), destCard));
    mFileNameEdit = new QLineEdit(destCard);
    mFileNameEdit->setPlaceholderText(tr("e.g. spritesheet"));
    mFileNameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{ background: #252525; color: #b3b3b3;"
        " border: 1px solid #333333; border-radius: 4px;"
        " padding: 3px 8px; font-size: 11px; }"));
    mFileNameEdit->setFixedHeight(CosmicUiMetrics::controlThickness());
    nameRow->addWidget(mFileNameEdit, 1);
    destLay->addLayout(nameRow);

    // ── format & frame size card ──────────────────────────────────────────────
    QVBoxLayout* fmtLay = nullptr;
    auto* fmtCard = makeCard(root, fmtLay);
    lay->addWidget(fmtCard);

    fmtLay->addWidget(makeSectionTitle(tr("Format & Frame Size"), fmtCard));
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

    auto* resRow = new QHBoxLayout();
    resRow->setSpacing(10);
    resRow->addWidget(makeLabel(tr("Width"), fmtCard));
    mWidthField = makeIntField(fmtCard, 1, 9999, 1920);
    resRow->addWidget(mWidthField, 1);
    resRow->addWidget(makeLabel(tr("Height"), fmtCard));
    mHeightField = makeIntField(fmtCard, 1, 9999, 1080);
    resRow->addWidget(mHeightField, 1);
    fmtLay->addLayout(resRow);

    // Transparency toggle
    auto* transRow = new QHBoxLayout();
    transRow->setSpacing(10);
    transRow->addWidget(makeLabel(tr("Transparent Background"), fmtCard), 1);
    mTransparencyToggle = new CosmicCheckboxToggle(fmtCard);
    mTransparencyToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mTransparencyToggle->setFixedSize(22, 22);
    transRow->addWidget(mTransparencyToggle);
    fmtLay->addLayout(transRow);

    // ── layout card ───────────────────────────────────────────────────────────
    QVBoxLayout* layoutCardLay = nullptr;
    auto* layoutCard = makeCard(root, layoutCardLay);
    lay->addWidget(layoutCard);

    layoutCardLay->addWidget(makeSectionTitle(tr("Sprite Layout"), layoutCard));
    layoutCardLay->addWidget(makeSeparator(layoutCard));

    mLayoutGroup = new QButtonGroup(this);
    mLayoutGroup->setExclusive(true);

    mBtnHorizontal = makeLayoutBtn(tr("Horizontal"), layoutCard);
    mBtnVertical   = makeLayoutBtn(tr("Vertical"),   layoutCard);
    mBtnGrid       = makeLayoutBtn(tr("Grid"),        layoutCard);

    mBtnHorizontal->setChecked(true);

    mLayoutGroup->addButton(mBtnHorizontal, static_cast<int>(Layout::Horizontal));
    mLayoutGroup->addButton(mBtnVertical,   static_cast<int>(Layout::Vertical));
    mLayoutGroup->addButton(mBtnGrid,       static_cast<int>(Layout::Grid));

    layoutCardLay->addWidget(mBtnHorizontal);
    layoutCardLay->addWidget(mBtnVertical);
    layoutCardLay->addWidget(mBtnGrid);

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
    connect(browseBtn,  &QPushButton::clicked,               this, &CosmicExportSpritePopup::onBrowseDest);
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicExportSpritePopup::onBrowseDest()
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

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicExportSpritePopup::showEvent(QShowEvent* e)
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

void CosmicExportSpritePopup::setResolution(const QSize& size)
{
    if (mWidthField)  mWidthField->setValue(size.width());
    if (mHeightField) mHeightField->setValue(size.height());
}

// ── result accessors ──────────────────────────────────────────────────────────

QString CosmicExportSpritePopup::destDir() const
{
    return mDestLabel ? mDestLabel->text() : QString();
}

QString CosmicExportSpritePopup::fileName() const
{
    return mFileNameEdit ? mFileNameEdit->text() : QString();
}

QString CosmicExportSpritePopup::exportFormat() const
{
    return mFormatDrop ? mFormatDrop->currentText() : QStringLiteral("PNG");
}

QSize CosmicExportSpritePopup::frameSize() const
{
    const int w = mWidthField  ? static_cast<int>(mWidthField->value())  : 1920;
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : 1080;
    return QSize(w, h);
}

CosmicExportSpritePopup::Layout CosmicExportSpritePopup::spriteLayout() const
{
    if (!mLayoutGroup) return Layout::Horizontal;
    switch (mLayoutGroup->checkedId()) {
    case static_cast<int>(Layout::Vertical): return Layout::Vertical;
    case static_cast<int>(Layout::Grid):     return Layout::Grid;
    default:                                 return Layout::Horizontal;
    }
}

bool CosmicExportSpritePopup::transparency() const
{
    return mTransparencyToggle && mTransparencyToggle->isChecked();
}
