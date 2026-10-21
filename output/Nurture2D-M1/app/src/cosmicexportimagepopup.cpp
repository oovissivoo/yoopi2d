#include "cosmicexportimagepopup.h"

#include "cosmicwindowheader.h"
#include "filedialog.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

// ── helpers ──────────────────────────────────────────────────────────────────

static QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
{
    const int r = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString(
        "QWidget{ background: #1e1e1e; border-radius: %1px; }").arg(r));
    outLay = new QVBoxLayout(card);
    outLay->setContentsMargins(14, 12, 14, 12);
    outLay->setSpacing(10);
    return card;
}

static QLabel* makeLabel(const QString& text, QWidget* parent,
                         int fontSize = 11, bool muted = false)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "QLabel{ background: transparent; color: %1; font-size: %2px; }")
        .arg(muted ? QStringLiteral("#777777") : QStringLiteral("#b3b3b3"))
        .arg(fontSize));
    lbl->setWordWrap(true);
    return lbl;
}

static QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-size: 11px; font-weight: 600; }"));
    return lbl;
}

static QFrame* makeSeparator(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("QFrame{ background: #2a2a2a; border: none; }"));
    return sep;
}

// ── constructor ───────────────────────────────────────────────────────────────

CosmicExportImagePopup::CosmicExportImagePopup(QWidget* parent, FileType fileType)
    : CosmicPopupWindow(parent)
    , mFileType(fileType)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(420, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI();
}

void CosmicExportImagePopup::buildUI()
{
    const bool isSeq = (mFileType == FileType::IMAGE_SEQUENCE);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("ExportImageRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#ExportImageRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // ── header ────────────────────────────────────────────────────────────────
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(
        isSeq ? tr("Export Image Sequence") : tr("Export Image"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── destination card ──────────────────────────────────────────────────────
    QVBoxLayout* destCardLay = nullptr;
    auto* destCard = makeCard(root, destCardLay);

    lay->addWidget(destCard);
    destCardLay->addWidget(makeSectionTitle(tr("Destination"), destCard));
    destCardLay->addWidget(makeLabel(
        isSeq ? tr("Choose where to save the exported frames.")
              : tr("Choose where to save the exported image."),
        destCard, 10, true));
    destCardLay->addWidget(makeSeparator(destCard));

    // path row
    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    mPathEdit = new QLineEdit(destCard);
    mPathEdit->setPlaceholderText(tr("Output path…"));
    mPathEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{ background: #2a2a2a; color: #b3b3b3; border: 1px solid #3a3a3a;"
        " border-radius: 4px; padding: 4px 8px; font-size: 11px; }"));
    mPathEdit->setMinimumHeight(26);
    pathRow->addWidget(mPathEdit, 1);

    auto* browseBtn = new CosmicButton(tr("Browse"), destCard);
    browseBtn->setBordered(true);
    browseBtn->setFixedHeight(26);
    pathRow->addWidget(browseBtn);
    destCardLay->addLayout(pathRow);

    // ── options card ──────────────────────────────────────────────────────────
    QVBoxLayout* optCardLay = nullptr;
    auto* optCard = makeCard(root, optCardLay);
    lay->addWidget(optCard);
    optCardLay->addWidget(makeSectionTitle(tr("Options"), optCard));

    // format row
    auto* fmtRow = new QHBoxLayout();
    fmtRow->setSpacing(10);
    fmtRow->addWidget(makeLabel(tr("Format"), optCard));
    mFormatDrop = new CosmicDropDown(optCard);
    mFormatDrop->setOptions({QStringLiteral("PNG"), QStringLiteral("JPG"),
                             QStringLiteral("BMP"), QStringLiteral("TIFF"),
                             QStringLiteral("WEBP")});
    mFormatDrop->setCurrentIndex(0);
    mFormatDrop->setFixedHeight(CosmicUiMetrics::controlThickness());
    fmtRow->addWidget(mFormatDrop, 1);
    optCardLay->addLayout(fmtRow);

    optCardLay->addWidget(makeSeparator(optCard));

    // transparency toggle
    auto* transRow = new QHBoxLayout();
    transRow->setSpacing(10);
    auto* transLbl = new QWidget(optCard);
    auto* transLblLay = new QVBoxLayout(transLbl);
    transLblLay->setContentsMargins(0,0,0,0);
    transLblLay->setSpacing(2);
    transLblLay->addWidget(makeLabel(tr("Transparent Background"), optCard));
    transLblLay->addWidget(makeLabel(tr("Preserve alpha channel in the exported file."),
                                     optCard, 10, true));
    transRow->addWidget(transLbl, 1);

    mTransparencyToggle = new CosmicCheckboxToggle(optCard);
    mTransparencyToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mTransparencyToggle->setFixedSize(22, 22);
    transRow->addWidget(mTransparencyToggle);
    optCardLay->addLayout(transRow);

    // image-sequence-only options
    if (isSeq) {
        optCardLay->addWidget(makeSeparator(optCard));

        // export specific toggle
        auto* specRow = new QHBoxLayout();
        specRow->setSpacing(10);
        auto* specLbl = new QWidget(optCard);
        auto* specLblLay = new QVBoxLayout(specLbl);
        specLblLay->setContentsMargins(0,0,0,0);
        specLblLay->setSpacing(2);
        specLblLay->addWidget(makeLabel(tr("Export Specific Frames"), optCard));
        specLblLay->addWidget(makeLabel(tr("Limit export to keyframes or unique cells only."),
                                        optCard, 10, true));
        specRow->addWidget(specLbl, 1);

        mExportSpecificToggle = new CosmicCheckboxToggle(optCard);
        mExportSpecificToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        mExportSpecificToggle->setFixedSize(22, 22);
        specRow->addWidget(mExportSpecificToggle);
        optCardLay->addLayout(specRow);

        // specific mode row (shown only when toggle is on)
        mSpecificModeRow = new QWidget(optCard);
        auto* smrLay = new QHBoxLayout(mSpecificModeRow);
        smrLay->setContentsMargins(0, 0, 0, 0);
        smrLay->setSpacing(10);
        smrLay->addWidget(makeLabel(tr("Mode"), mSpecificModeRow));
        mSpecificModeDrop = new CosmicDropDown(mSpecificModeRow);
        mSpecificModeDrop->setOptions({tr("Keyframes"), tr("Cells Only")});
        mSpecificModeDrop->setCurrentIndex(0);
        mSpecificModeDrop->setFixedHeight(CosmicUiMetrics::controlThickness());
        smrLay->addWidget(mSpecificModeDrop, 1);
        mSpecificModeRow->setVisible(false);
        optCardLay->addWidget(mSpecificModeRow);

        connect(mExportSpecificToggle, &CosmicCheckboxToggle::toggled,
                this, [this](bool on) {
            if (mSpecificModeRow) mSpecificModeRow->setVisible(on);
            clearFixedWindowSize();
            adjustSize();
        });
    }

    // ── action row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch(1);

    mCancelBtn = new CosmicButton(tr("Cancel"), root);
    mCancelBtn->setBordered(true);
    mCancelBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mCancelBtn);

    mExportBtn = new CosmicButton(tr("Export"), root);
    mExportBtn->setBordered(true);
    const QColor accent = CosmicColor::accent();
    mExportBtn->setBackgroundColors(accent, accent.lighter(115), accent.darker(115));
    mExportBtn->setTextColor(QColor(0xff, 0xff, 0xff));
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
    connect(browseBtn,  &QPushButton::clicked,               this, &CosmicExportImagePopup::onBrowse);
    connect(mFormatDrop, &CosmicDropDown::currentIndexChanged,
            this, &CosmicExportImagePopup::onFormatChanged);

    // seed path from last used
    const QString lastPath = (mFileType == FileType::IMAGE_SEQUENCE)
        ? FileDialog::getLastSavePath(FileType::IMAGE_SEQUENCE)
        : FileDialog::getLastSavePath(FileType::IMAGE);
    if (!lastPath.isEmpty()) mPathEdit->setText(lastPath);

    updateTransparencyState();
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicExportImagePopup::onBrowse()
{
    const QString path = FileDialog::getSaveFileName(this, mFileType);
    if (!path.isEmpty()) mPathEdit->setText(path);
}

void CosmicExportImagePopup::onFormatChanged(int)
{
    // Update path extension to match chosen format
    const QString fmt = exportFormat().toLower();
    if (mPathEdit && !mPathEdit->text().isEmpty()) {
        QFileInfo fi(mPathEdit->text());
        mPathEdit->setText(fi.path() + "/" + fi.completeBaseName() + "." + fmt);
    }
    updateTransparencyState();
}

void CosmicExportImagePopup::updateTransparencyState()
{
    if (!mTransparencyToggle) return;
    const QString fmt = exportFormat();
    const bool supported = (fmt != "JPG" && fmt != "BMP");
    mTransparencyToggle->setEnabled(supported);
    if (!supported) mTransparencyToggle->setChecked(false);
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicExportImagePopup::showEvent(QShowEvent* e)
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

// ── result accessors ──────────────────────────────────────────────────────────

QString CosmicExportImagePopup::filePath() const
{
    return mPathEdit ? mPathEdit->text() : QString();
}

QString CosmicExportImagePopup::exportFormat() const
{
    return mFormatDrop ? mFormatDrop->currentText() : QStringLiteral("PNG");
}

bool CosmicExportImagePopup::transparency() const
{
    return mTransparencyToggle && mTransparencyToggle->isChecked();
}

bool CosmicExportImagePopup::isExportSpecificEnabled() const
{
    return mExportSpecificToggle && mExportSpecificToggle->isChecked();
}

CosmicExportImagePopup::ExportSpecificMode CosmicExportImagePopup::exportSpecificMode() const
{
    const int idx = mSpecificModeDrop ? mSpecificModeDrop->currentIndex() : 0;
    return (idx == 1) ? ExportSpecificMode::CellsOnly : ExportSpecificMode::Keyframes;
}
