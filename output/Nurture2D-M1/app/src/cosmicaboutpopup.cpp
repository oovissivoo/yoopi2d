#include "cosmicaboutpopup.h"

#include "cosmicwindowheader.h"
#include "pencildef.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QSysInfo>
#include <QVBoxLayout>

namespace {

QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
{
    const int r = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString(
        "QWidget{ background: #1e1e1e; border-radius: %1px; }").arg(r));
    outLay = new QVBoxLayout(card);
    outLay->setContentsMargins(14, 12, 14, 12);
    outLay->setSpacing(6);
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

} // namespace

// ── constructor ───────────────────────────────────────────────────────────────

CosmicAboutPopup::CosmicAboutPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(380, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI();
}

void CosmicAboutPopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("AboutRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#AboutRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── app identity ──────────────────────────────────────────────────────────
    auto* logoRow = new QHBoxLayout();
    logoRow->setSpacing(12);
    logoRow->setAlignment(Qt::AlignCenter);

    auto* logoLbl = new QLabel(root);
    const QPixmap logo(QStringLiteral(":/icons/logo.png"));
    if (!logo.isNull())
        logoLbl->setPixmap(logo.scaled(52, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLbl->setStyleSheet(QStringLiteral("QLabel{ background: transparent; }"));
    logoRow->addWidget(logoLbl);

    auto* nameCol = new QVBoxLayout();
    nameCol->setSpacing(2);
    auto* appName = new QLabel(QStringLiteral("Nurture2D"), root);
    appName->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-size: 16px; font-weight: 700; }"));
    nameCol->addWidget(appName);
    mVersionLabel = makeLabel(QString(), root, 11, true);
    nameCol->addWidget(mVersionLabel);
    logoRow->addLayout(nameCol);
    lay->addLayout(logoRow);

    // ── system info card ──────────────────────────────────────────────────────
    QVBoxLayout* sysLay = nullptr;
    auto* sysCard = makeCard(root, sysLay);
    lay->addWidget(sysCard);
    sysLay->addWidget(makeSectionTitle(tr("System Information"), sysCard));
    sysLay->addWidget(makeSeparator(sysCard));
    mSysInfoLabel = makeLabel(QString(), sysCard, 10, true);
    mSysInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sysLay->addWidget(mSysInfoLabel);

    // ── action row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch(1);

    mCopyBtn = new CosmicButton(tr("Copy Info"), root);
    mCopyBtn->setBordered(true);
    mCopyBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mCopyBtn);

    mCloseBtn = new CosmicButton(tr("Close"), root);
    mCloseBtn->setBordered(true);
    mCloseBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mCloseBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,   &CosmicWindowHeader::closeRequested, this, &QDialog::accept);
    connect(mCloseBtn, &QPushButton::clicked,               this, &QDialog::accept);
    connect(mCopyBtn,  &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(mDevText.join("\n"));
    });
}

void CosmicAboutPopup::init()
{
#if defined(PENCIL2D_RELEASE_BUILD)
    mDevText << tr("Version: %1").arg(APP_VERSION);
#elif defined(PENCIL2D_NIGHTLY_BUILD)
    mDevText << QStringLiteral("Nightly Build ") + __DATE__;
#else
    mDevText << QStringLiteral("Development Build ") + __DATE__;
#endif

    mDevText << QString();

#if defined(GIT_EXISTS)
    mDevText << QStringLiteral("commit: ") S__GIT_COMMIT_HASH
             << QStringLiteral("date: ")   S__GIT_TIMESTAMP
             << QString();
#endif

    mDevText << QStringLiteral("OS: ") + QSysInfo::prettyProductName()
             << QStringLiteral("CPU: ") + QSysInfo::buildCpuArchitecture();

    if (QString(qVersion()) == QT_VERSION_STR) {
        mDevText << QStringLiteral("Qt: ") + QT_VERSION_STR;
    } else {
        mDevText << QStringLiteral("Qt (compile): ") + QT_VERSION_STR
                 << QStringLiteral("Qt (runtime): ") + qVersion();
    }

    if (mVersionLabel) mVersionLabel->setText(mDevText.first());
    if (mSysInfoLabel) mSysInfoLabel->setText(mDevText.join("\n"));
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicAboutPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(380);
    setMaximumWidth(380);
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
