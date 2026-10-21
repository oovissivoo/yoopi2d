#include "cosmiccheckupdatespopup.h"

#include "cosmicwindowheader.h"
#include "pencildef.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QParallelAnimationGroup>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QShowEvent>
#include <QUrl>
#include <QVersionNumber>
#include <QVBoxLayout>
#include <QXmlStreamReader>

namespace {

QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
{
    const int r = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString(
        "QWidget{ background: #1e1e1e; border-radius: %1px; }").arg(r));
    outLay = new QVBoxLayout(card);
    outLay->setContentsMargins(14, 12, 14, 12);
    outLay->setSpacing(8);
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

} // namespace

// ── constructor ───────────────────────────────────────────────────────────────

CosmicCheckUpdatesPopup::CosmicCheckUpdatesPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::ApplicationModal);
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

CosmicCheckUpdatesPopup::~CosmicCheckUpdatesPopup()
{
    if (mNetworkManager) mNetworkManager->deleteLater();
}

void CosmicCheckUpdatesPopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CheckUpdatesRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#CheckUpdatesRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Check for Updates"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── status card ───────────────────────────────────────────────────────────
    QVBoxLayout* statusLay = nullptr;
    auto* statusCard = makeCard(root, statusLay);
    lay->addWidget(statusCard);

    mStatusLabel = new QLabel(tr("Checking for updates…"), statusCard);
    mStatusLabel->setAlignment(Qt::AlignCenter);
    mStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-size: 12px; font-weight: 600; }"));
    mStatusLabel->setWordWrap(true);
    statusLay->addWidget(mStatusLabel);

    mDetailLabel = makeLabel(QString(), statusCard, 11, true);
    mDetailLabel->setAlignment(Qt::AlignCenter);
    mDetailLabel->setOpenExternalLinks(true);
    statusLay->addWidget(mDetailLabel);

    mProgress = new QProgressBar(statusCard);
    mProgress->setRange(0, 0);
    mProgress->setTextVisible(false);
    mProgress->setFixedHeight(4);
    mProgress->setStyleSheet(QStringLiteral(
        "QProgressBar{ background: #2a2a2a; border: none; border-radius: 2px; }"
        "QProgressBar::chunk{ background: #6a4cff; border-radius: 2px; }"));
    statusLay->addWidget(mProgress);

    // ── action row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch(1);

    mDownloadBtn = new CosmicButton(tr("Download"), root);
    mDownloadBtn->setBordered(true);
    const QColor accent = CosmicColor::accent();
    mDownloadBtn->setBackgroundColors(accent, accent.lighter(115), accent.darker(115));
    mDownloadBtn->setTextColor(QColor(0xff, 0xff, 0xff));
    mDownloadBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    mDownloadBtn->setEnabled(false);
    btnRow->addWidget(mDownloadBtn);

    mCloseBtn = new CosmicButton(tr("Close"), root);
    mCloseBtn->setBordered(true);
    mCloseBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(mCloseBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,      &CosmicWindowHeader::closeRequested, this, &QDialog::accept);
    connect(mCloseBtn,    &QPushButton::clicked,               this, &QDialog::accept);
    connect(mDownloadBtn, &QPushButton::clicked,               this, &CosmicCheckUpdatesPopup::gotoDownloadPage);
}

// ── public ────────────────────────────────────────────────────────────────────

void CosmicCheckUpdatesPopup::startChecking()
{
#ifdef PENCIL2D_NIGHTLY_BUILD
    nightlyBuildCheck();
#else
    regularBuildCheck();
#endif
}

// ── private ───────────────────────────────────────────────────────────────────

void CosmicCheckUpdatesPopup::regularBuildCheck()
{
    mNetworkManager = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral("https://github.com/pencil2d/pencil/releases.atom")));
    req.setRawHeader("User-Agent", "Mozilla Firefox");
    mNetworkManager->get(req);
    connect(mNetworkManager, &QNetworkAccessManager::finished,
            this, &CosmicCheckUpdatesPopup::networkRequestFinished);
}

void CosmicCheckUpdatesPopup::nightlyBuildCheck()
{
    if (mStatusLabel) mStatusLabel->setText(tr("You are using a nightly build"));
    if (mDetailLabel) {
        mDetailLabel->setText(tr("Visit %1 here %2 to check for new nightly builds.")
            .arg(QStringLiteral("<a href=\"https://www.pencil2d.org/download/#nightlybuild\">"),
                 QStringLiteral("</a>")));
    }
    if (mProgress) { mProgress->setRange(0, 1); mProgress->setValue(1); }
    if (mDownloadBtn) mDownloadBtn->setEnabled(false);
}

void CosmicCheckUpdatesPopup::showError(const QString& detail)
{
    if (mStatusLabel) mStatusLabel->setText(tr("Could not check for updates"));
    if (mDetailLabel) mDetailLabel->setText(detail);
    if (mProgress) { mProgress->setRange(0, 1); mProgress->setValue(1); }
    if (mDownloadBtn) mDownloadBtn->setEnabled(false);
}

void CosmicCheckUpdatesPopup::showResult(bool isNewer, const QString& latestVersion)
{
    if (mProgress) { mProgress->setRange(0, 1); mProgress->setValue(1); }
    if (isNewer) {
        if (mStatusLabel) mStatusLabel->setText(tr("A new version is available"));
        if (mDetailLabel) mDetailLabel->setText(
            tr("Nurture2D %1 is available — you have %2.")
                .arg(latestVersion, APP_VERSION));
        if (mDownloadBtn) mDownloadBtn->setEnabled(true);
    } else {
        if (mStatusLabel) mStatusLabel->setText(tr("Nurture2D is up to date"));
        if (mDetailLabel) mDetailLabel->setText(tr("Version %1").arg(APP_VERSION));
        if (mDownloadBtn) mDownloadBtn->setEnabled(false);
    }
}

void CosmicCheckUpdatesPopup::networkRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        showError(tr("Check your internet connection and try again."));
        return;
    }
    const QString xml = QString::fromUtf8(reply->readAll()).trimmed();
    if (xml.isEmpty()) { showError(tr("Empty response from server.")); return; }
    const QString ver = getVersionFromXml(xml);
    if (ver == QStringLiteral("0.0.1")) { showError(tr("Could not parse version info.")); return; }
    showResult(compareVersion(APP_VERSION, ver), ver);
}

bool CosmicCheckUpdatesPopup::compareVersion(const QString& current, const QString& latest)
{
    return QVersionNumber::fromString(current) < QVersionNumber::fromString(latest);
}

QString CosmicCheckUpdatesPopup::getVersionFromXml(const QString& xml)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd() && !reader.hasError()) {
        if (reader.readNext() == QXmlStreamReader::StartElement
            && reader.name() == QLatin1String("entry")) {
            while (!reader.atEnd() && !reader.hasError()) {
                reader.readNext();
                if (reader.name() == QLatin1String("title"))
                    return reader.readElementText().remove(QRegularExpression(QStringLiteral("^v")));
            }
        }
    }
    return QStringLiteral("0.0.1");
}

void CosmicCheckUpdatesPopup::gotoDownloadPage()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://pencil2d.org/download")));
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicCheckUpdatesPopup::showEvent(QShowEvent* e)
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
