#include "cosmicexportgifpopup.h"

#include "cosmicwindowheader.h"
#include "filedialog.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

// ── local helpers ─────────────────────────────────────────────────────────────

namespace {

QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
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

QLabel* makeLabel(const QString& text, QWidget* parent,
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

QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff;"
        " font-size: 11px; font-weight: 600; }"));
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

CosmicExportGifPopup::CosmicExportGifPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(440, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI();
}

// ── buildUI ───────────────────────────────────────────────────────────────────

void CosmicExportGifPopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("ExportGifRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#ExportGifRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // ── header ────────────────────────────────────────────────────────────────
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Export GIF"), root);
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
    destLay->addWidget(makeLabel(tr("Choose where to save the exported GIF file."),
                                 destCard, 10, true));
    destLay->addWidget(makeSeparator(destCard));

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
    destLay->addLayout(pathRow);

    // ── resolution card ───────────────────────────────────────────────────────
    QVBoxLayout* resLay = nullptr;
    auto* resCard = makeCard(root, resLay);
    lay->addWidget(resCard);
    resLay->addWidget(makeSectionTitle(tr("Resolution"), resCard));
    resLay->addWidget(makeLabel(tr("Output dimensions in pixels."),
                                resCard, 10, true));
    resLay->addWidget(makeSeparator(resCard));

    auto* resRow = new QHBoxLayout();
    resRow->setSpacing(10);
    resRow->addWidget(makeLabel(tr("Width"), resCard));
    mWidthField = makeIntField(resCard, 1, 9999, 1920);
    resRow->addWidget(mWidthField, 1);
    resRow->addWidget(makeLabel(tr("Height"), resCard));
    mHeightField = makeIntField(resCard, 1, 9999, 1080);
    resRow->addWidget(mHeightField, 1);
    resLay->addLayout(resRow);

    // ── frame range card ──────────────────────────────────────────────────────
    QVBoxLayout* rangeLay = nullptr;
    auto* rangeCard = makeCard(root, rangeLay);
    lay->addWidget(rangeCard);
    rangeLay->addWidget(makeSectionTitle(tr("Frame Range"), rangeCard));
    rangeLay->addWidget(makeLabel(tr("Set the start and end frames to export."),
                                  rangeCard, 10, true));
    rangeLay->addWidget(makeSeparator(rangeCard));

    auto* rangeRow = new QHBoxLayout();
    rangeRow->setSpacing(10);
    rangeRow->addWidget(makeLabel(tr("Start"), rangeCard));
    mStartField = makeIntField(rangeCard, 1, 99999, 1);
    rangeRow->addWidget(mStartField, 1);
    rangeRow->addWidget(makeLabel(tr("End"), rangeCard));
    mEndField = makeIntField(rangeCard, 1, 99999, 1);
    rangeRow->addWidget(mEndField, 1);
    rangeLay->addLayout(rangeRow);

    // cap warning label (hidden by default)
    mCapWarning = new QLabel(
        tr("GIF export is limited to 10 seconds. End frame has been clamped."),
        rangeCard);
    mCapWarning->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffb21a; font-size: 10px; }"));
    mCapWarning->setWordWrap(true);
    mCapWarning->setVisible(false);
    rangeLay->addWidget(mCapWarning);

    // ── options card ──────────────────────────────────────────────────────────
    QVBoxLayout* optLay = nullptr;
    auto* optCard = makeCard(root, optLay);
    lay->addWidget(optCard);
    optLay->addWidget(makeSectionTitle(tr("Options"), optCard));

    // transparent background toggle
    auto* transRow = new QHBoxLayout();
    transRow->setSpacing(10);
    auto* transLblWrap = new QWidget(optCard);
    auto* transLblLay = new QVBoxLayout(transLblWrap);
    transLblLay->setContentsMargins(0, 0, 0, 0);
    transLblLay->setSpacing(2);
    transLblLay->addWidget(makeLabel(tr("Transparent Background"), optCard));
    transLblLay->addWidget(makeLabel(tr("Map fully-transparent pixels to GIF transparency index."),
                                     optCard, 10, true));
    transRow->addWidget(transLblWrap, 1);
    mTransparencyToggle = new CosmicCheckboxToggle(optCard);
    mTransparencyToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mTransparencyToggle->setFixedSize(22, 22);
    transRow->addWidget(mTransparencyToggle);
    optLay->addLayout(transRow);

    optLay->addWidget(makeSeparator(optCard));

    // loop toggle
    auto* loopRow = new QHBoxLayout();
    loopRow->setSpacing(10);
    auto* loopLblWrap = new QWidget(optCard);
    auto* loopLblLay = new QVBoxLayout(loopLblWrap);
    loopLblLay->setContentsMargins(0, 0, 0, 0);
    loopLblLay->setSpacing(2);
    loopLblLay->addWidget(makeLabel(tr("Loop"), optCard));
    loopLblLay->addWidget(makeLabel(tr("Repeat playback continuously (loop count 0)."),
                                    optCard, 10, true));
    loopRow->addWidget(loopLblWrap, 1);
    mLoopToggle = new CosmicCheckboxToggle(optCard);
    mLoopToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mLoopToggle->setFixedSize(22, 22);
    mLoopToggle->setChecked(true);  // loop on by default
    loopRow->addWidget(mLoopToggle);
    optLay->addLayout(loopRow);

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
    connect(browseBtn,  &QPushButton::clicked,               this, &CosmicExportGifPopup::onBrowse);

    connect(mExportBtn, &QPushButton::clicked, this, [this]() {
        if (mPathEdit && mPathEdit->text().trimmed().isEmpty()) {
            // Highlight path field and refuse to accept
            mPathEdit->setStyleSheet(QStringLiteral(
                "QLineEdit{ background: #2a2a2a; color: #b3b3b3;"
                " border: 1px solid #ff7733;"
                " border-radius: 4px; padding: 4px 8px; font-size: 11px; }"));
            mPathEdit->setFocus();
            return;
        }
        accept();
    });

    // Restore normal border when user starts typing
    connect(mPathEdit, &QLineEdit::textChanged, this, [this](const QString&) {
        if (mPathEdit) {
            mPathEdit->setStyleSheet(QStringLiteral(
                "QLineEdit{ background: #2a2a2a; color: #b3b3b3;"
                " border: 1px solid #3a3a3a;"
                " border-radius: 4px; padding: 4px 8px; font-size: 11px; }"));
        }
    });

    connect(mStartField, &CosmicInputField::valueChanged,
            this, [this](double) { onRangeChanged(); });
    connect(mEndField,   &CosmicInputField::valueChanged,
            this, [this](double) { onRangeChanged(); });

    // Pre-populate destination from last save path
    const QString lastPath = FileDialog::getLastSavePath(FileType::GIF);
    if (!lastPath.isEmpty()) {
        mPathEdit->setText(lastPath);
    }
}

// ── public setters ────────────────────────────────────────────────────────────

void CosmicExportGifPopup::setResolution(const QSize& size)
{
    if (mWidthField)  { QSignalBlocker b(mWidthField);  mWidthField->setValue(size.width()); }
    if (mHeightField) { QSignalBlocker b(mHeightField); mHeightField->setValue(size.height()); }
}

void CosmicExportGifPopup::setDefaultRange(int startFrame, int endFrame, int fps)
{
    mFps = qMax(1, fps);
    if (mStartField) { QSignalBlocker b(mStartField); mStartField->setValue(startFrame); }
    if (mEndField)   { QSignalBlocker b(mEndField);   mEndField->setValue(endFrame); }
    // Apply cap after setting both fields
    updateCapWarning();
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicExportGifPopup::onBrowse()
{
    const QString path = FileDialog::getSaveFileName(this, FileType::GIF);
    if (!path.isEmpty() && mPathEdit) {
        mPathEdit->setText(path);
    }
}

void CosmicExportGifPopup::onRangeChanged()
{
    updateCapWarning();
}

// ── private helpers ───────────────────────────────────────────────────────────

void CosmicExportGifPopup::updateCapWarning()
{
    if (!mStartField || !mEndField || !mCapWarning) return;

    const int start = static_cast<int>(mStartField->value());
    const int end   = static_cast<int>(mEndField->value());
    const int maxFrames = mFps * 10;
    const int cappedEnd = start + maxFrames - 1;

    if ((end - start + 1) > maxFrames) {
        // Clamp end field
        QSignalBlocker b(mEndField);
        mEndField->setValue(cappedEnd);
        mCapWarning->setVisible(true);
    } else {
        mCapWarning->setVisible(false);
    }
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicExportGifPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(440);
    setMaximumWidth(440);
    adjustSize();

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize  s = g0.size();
    const QSize  s0(qMax(1, int(s.width() * 0.93)), qMax(1, int(s.height() * 0.93)));
    const QRect  g1(QPoint(c.x() - s0.width() / 2, c.y() - s0.height() / 2), s0);
    setGeometry(g1);

    auto* op = new QPropertyAnimation(this, "windowOpacity", this);
    op->setDuration(80);
    op->setStartValue(0.0);
    op->setEndValue(1.0);
    auto* gm = new QPropertyAnimation(this, "geometry", this);
    gm->setDuration(95);
    gm->setStartValue(g1);
    gm->setEndValue(g0);
    auto* grp = new QParallelAnimationGroup(this);
    grp->addAnimation(op);
    grp->addAnimation(gm);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── result accessors ──────────────────────────────────────────────────────────

QString CosmicExportGifPopup::filePath() const
{
    return mPathEdit ? mPathEdit->text().trimmed() : QString();
}

QSize CosmicExportGifPopup::exportSize() const
{
    const int w = mWidthField  ? static_cast<int>(mWidthField->value())  : 1920;
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : 1080;
    return QSize(w, h);
}

int CosmicExportGifPopup::startFrame() const
{
    return mStartField ? static_cast<int>(mStartField->value()) : 1;
}

int CosmicExportGifPopup::endFrame() const
{
    return mEndField ? static_cast<int>(mEndField->value()) : 1;
}

bool CosmicExportGifPopup::loop() const
{
    return mLoopToggle && mLoopToggle->isChecked();
}

bool CosmicExportGifPopup::transparency() const
{
    return mTransparencyToggle && mTransparencyToggle->isChecked();
}
