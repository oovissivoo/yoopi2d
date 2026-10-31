#include "cosmicexportmoviepopup.h"

#include "cosmicwindowheader.h"
#include "filedialog.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
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

// ── shared helpers (local) ────────────────────────────────────────────────────

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
        "QLabel{ background: transparent; color: #ffffff; font-size: 11px; font-weight: 600; }"));
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

CosmicExportMoviePopup::CosmicExportMoviePopup(QWidget* parent, FileType fileType)
    : CosmicPopupWindow(parent)
    , mFileType(fileType)
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

void CosmicExportMoviePopup::buildUI()
{
    const bool isGif = (mFileType == FileType::GIF);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("ExportMovieRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#ExportMovieRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // ── header ────────────────────────────────────────────────────────────────
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(isGif ? tr("Export Animated GIF") : tr("Export Animation — MP4 / H.264"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── destination card ──────────────────────────────────────────────────────
    QVBoxLayout* destLay = nullptr;
    auto* destCard = makeCard(root, destLay);
    lay->addWidget(destCard);
    destLay->addWidget(makeSectionTitle(tr("Destination"), destCard));
    destLay->addWidget(makeLabel((isGif ? tr("Choose where to save the exported file.") : tr("MP4 / H.264: fast export with a compact file size.")),
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

    // ── camera card (hidden for GIF) ──────────────────────────────────────────
    if (!isGif) {
        QVBoxLayout* camLay = nullptr;
        auto* camCard = makeCard(root, camLay);
        lay->addWidget(camCard);
        camLay->addWidget(makeSectionTitle(tr("Camera"), camCard));
        camLay->addWidget(makeLabel(tr("Select which camera layer to render from."),
                                    camCard, 10, true));
        camLay->addWidget(makeSeparator(camCard));

        auto* camRow = new QHBoxLayout();
        camRow->setSpacing(10);
        camRow->addWidget(makeLabel(tr("Camera Layer"), camCard));
        mCameraDrop = new CosmicDropDown(camCard);
        mCameraDrop->setFixedHeight(CosmicUiMetrics::controlThickness());
        camRow->addWidget(mCameraDrop, 1);
        camLay->addLayout(camRow);
    }

    // ── resolution card ───────────────────────────────────────────────────────
    QVBoxLayout* resLay = nullptr;
    auto* resCard = makeCard(root, resLay);
    lay->addWidget(resCard);
    resLay->addWidget(makeSectionTitle(tr("Resolution"), resCard));
    resLay->addWidget(makeLabel(tr("Output dimensions in pixels. MP4 requires even numbers."),
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

    mResWarning = makeLabel(tr("Width and height must be even numbers for MP4."),
                            resCard, 10, false);
    mResWarning->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffb21a; font-size: 10px; }"));
    mResWarning->setVisible(false);
    resLay->addWidget(mResWarning);

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

    // include sounds toggle
    auto* soundRow = new QHBoxLayout();
    soundRow->setSpacing(10);
    auto* soundLblWrap = new QWidget(rangeCard);
    auto* soundLblLay = new QVBoxLayout(soundLblWrap);
    soundLblLay->setContentsMargins(0,0,0,0); soundLblLay->setSpacing(2);
    soundLblLay->addWidget(makeLabel(tr("Extend to Include Sounds"), rangeCard));
    soundLblLay->addWidget(makeLabel(tr("Extend end frame to cover all audio tracks."),
                                     rangeCard, 10, true));
    soundRow->addWidget(soundLblWrap, 1);
    mIncludeSoundsToggle = new CosmicCheckboxToggle(rangeCard);
    mIncludeSoundsToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mIncludeSoundsToggle->setFixedSize(22, 22);
    soundRow->addWidget(mIncludeSoundsToggle);
    rangeLay->addLayout(soundRow);

    // ── playback options card ─────────────────────────────────────────────────
    QVBoxLayout* pbLay = nullptr;
    auto* pbCard = makeCard(root, pbLay);
    lay->addWidget(pbCard);
    pbLay->addWidget(makeSectionTitle(tr("Playback"), pbCard));

    // loop toggle (GIF/APNG only — shown always, enabled conditionally)
    auto* loopRow = new QHBoxLayout();
    loopRow->setSpacing(10);
    auto* loopLblWrap = new QWidget(pbCard);
    auto* loopLblLay = new QVBoxLayout(loopLblWrap);
    loopLblLay->setContentsMargins(0,0,0,0); loopLblLay->setSpacing(2);
    loopLblLay->addWidget(makeLabel(tr("Loop"), pbCard));
    loopLblLay->addWidget(makeLabel(tr("Repeat playback continuously (GIF / APNG only)."),
                                    pbCard, 10, true));
    loopRow->addWidget(loopLblWrap, 1);
    mLoopToggle = new CosmicCheckboxToggle(pbCard);
    mLoopToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mLoopToggle->setFixedSize(22, 22);
    loopRow->addWidget(mLoopToggle);
    pbLay->addLayout(loopRow);

    pbLay->addWidget(makeSeparator(pbCard));

    // transparency toggle (APNG/WebM only)
    auto* transRow = new QHBoxLayout();
    transRow->setSpacing(10);
    auto* transLblWrap = new QWidget(pbCard);
    auto* transLblLay = new QVBoxLayout(transLblWrap);
    transLblLay->setContentsMargins(0,0,0,0); transLblLay->setSpacing(2);
    transLblLay->addWidget(makeLabel(tr("Transparent Background"), pbCard));
    transLblLay->addWidget(makeLabel(tr("Preserve alpha channel (APNG / WebM only)."),
                                     pbCard, 10, true));
    transRow->addWidget(transLblWrap, 1);
    mTransparencyToggle = new CosmicCheckboxToggle(pbCard);
    mTransparencyToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mTransparencyToggle->setFixedSize(22, 22);
    transRow->addWidget(mTransparencyToggle);
    pbLay->addLayout(transRow);

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
    connect(browseBtn,  &QPushButton::clicked,               this, &CosmicExportMoviePopup::onBrowse);
    connect(mPathEdit,  &QLineEdit::textChanged,             this, &CosmicExportMoviePopup::onFilePathChanged);

    if (mCameraDrop)
        connect(mCameraDrop, &CosmicDropDown::currentIndexChanged,
                this, &CosmicExportMoviePopup::onCameraChanged);

    if (mWidthField)
        connect(mWidthField, &CosmicInputField::valueChanged,
                this, [this](double){ validateResolution(); });
    if (mHeightField)
        connect(mHeightField, &CosmicInputField::valueChanged,
                this, [this](double){ validateResolution(); });

    if (mIncludeSoundsToggle)
        connect(mIncludeSoundsToggle, &CosmicCheckboxToggle::toggled,
                this, [this](bool on) {
            if (mEndField)
                mEndField->setValue(on ? mEndFrameWithSounds : mEndFrame);
        });

    // seed path
    const QString lastPath = FileDialog::getLastSavePath(mFileType);
    if (!lastPath.isEmpty()) mPathEdit->setText(lastPath);

    updateLoopState();
    updateTransparencyState();
    validateResolution();
}

// ── public setters ────────────────────────────────────────────────────────────

void CosmicExportMoviePopup::setCamerasInfo(
    const std::vector<std::pair<QString, QSize>>& cameras)
{
    mCameraData = cameras;
    if (!mCameraDrop) { validateResolution(); return; }
    QStringList names;
    for (const auto& c : cameras) names << c.first;
    mCameraDrop->setOptions(names);
    if (!cameras.empty()) {
        mCameraDrop->setCurrentIndex(0);
        onCameraChanged(0);
    }
    validateResolution();
}

void CosmicExportMoviePopup::setDefaultRange(int startFrame, int endFrame, int endFrameWithSounds)
{
    mEndFrame = endFrame;
    mEndFrameWithSounds = endFrameWithSounds;
    if (mStartField) mStartField->setValue(startFrame);
    if (mEndField)   mEndField->setValue(endFrame);
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicExportMoviePopup::onBrowse()
{
    const QString path = FileDialog::getSaveFileName(this, mFileType);
    if (!path.isEmpty()) mPathEdit->setText(path);
}

void CosmicExportMoviePopup::onCameraChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(mCameraData.size())) return;
    const QSize sz = mCameraData[static_cast<size_t>(index)].second;
    if (mWidthField)  { QSignalBlocker b(mWidthField);  mWidthField->setValue(sz.width()); }
    if (mHeightField) { QSignalBlocker b(mHeightField); mHeightField->setValue(sz.height()); }
    validateResolution();
}

void CosmicExportMoviePopup::onFilePathChanged()
{
    updateLoopState();
    updateTransparencyState();
    validateResolution();
}

void CosmicExportMoviePopup::validateResolution()
{
    if (!mResWarning) return;
    const QString fp = filePath().toLower();
    const bool isMp4 = fp.endsWith(".mp4");
    const int w = mWidthField  ? static_cast<int>(mWidthField->value())  : 0;
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : 0;
    const bool bad = isMp4 && (w % 2 != 0 || h % 2 != 0);
    mResWarning->setVisible(bad);
    if (mExportBtn) mExportBtn->setEnabled(!bad && w > 0 && h > 0 && !filePath().trimmed().isEmpty() && !selectedCameraName().isEmpty());
}

void CosmicExportMoviePopup::updateLoopState()
{
    if (!mLoopToggle) return;
    const QString fp = filePath().toLower();
    const bool canLoop = fp.endsWith(".gif") || fp.endsWith(".apng");
    mLoopToggle->setEnabled(canLoop);
    if (!canLoop) mLoopToggle->setChecked(false);
}

void CosmicExportMoviePopup::updateTransparencyState()
{
    if (!mTransparencyToggle) return;
    const QString fp = filePath().toLower();
    const bool canTrans = fp.endsWith(".apng") || fp.endsWith(".webm");
    mTransparencyToggle->setEnabled(canTrans);
    if (!canTrans) mTransparencyToggle->setChecked(false);
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicExportMoviePopup::showEvent(QShowEvent* e)
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

QString CosmicExportMoviePopup::filePath() const
{
    return mPathEdit ? mPathEdit->text() : QString();
}

QString CosmicExportMoviePopup::selectedCameraName() const
{
    if (mCameraData.empty()) return QString();
    return mCameraDrop ? mCameraDrop->currentText() : mCameraData.front().first;
}

QSize CosmicExportMoviePopup::exportSize() const
{
    const int w = mWidthField  ? static_cast<int>(mWidthField->value())  : 1920;
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : 1080;
    return QSize(w, h);
}

int CosmicExportMoviePopup::startFrame() const
{
    return mStartField ? static_cast<int>(mStartField->value()) : 1;
}

int CosmicExportMoviePopup::endFrame() const
{
    return mEndField ? static_cast<int>(mEndField->value()) : 1;
}

bool CosmicExportMoviePopup::loop() const
{
    return mLoopToggle && mLoopToggle->isChecked();
}

bool CosmicExportMoviePopup::transparency() const
{
    return mTransparencyToggle && mTransparencyToggle->isChecked();
}
