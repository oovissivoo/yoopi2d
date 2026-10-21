#include "cosmicwarningpopup.h"

#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

CosmicWarningPopup::CosmicWarningPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(34);
    setFixedWindowSize(QSize(360, 1)); // placeholder; real height set after layout in showEvent
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicWarningPopupRoot"));
    root->setStyleSheet(QStringLiteral("QWidget#CosmicWarningPopupRoot{ background: transparent; border: none; }"));

    // Three-row vertical layout: icon | body text | close button
    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(20, 8, 20, 8);
    lay->setSpacing(3);

    // --- Warning icon (top, centered) ---
    mIcon = new QLabel(root);
    mIcon->setFixedSize(32, 32);
    mIcon->setAlignment(Qt::AlignCenter);
    mIcon->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
    lay->addWidget(mIcon, 0, Qt::AlignHCenter);

    // --- Body text (center, word-wrapped) ---
    mBody = new QLabel(QString(), root);
    mBody->setWordWrap(true);
    mBody->setAlignment(Qt::AlignCenter);
    mBody->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
    CosmicText::applyToWidget(mBody);
    lay->addWidget(mBody, 0);

    // --- Examples card (darker inset, hidden until setExamplesText is called) ---
    mExamplesCard = new QWidget(root);
    mExamplesCard->setStyleSheet(QStringLiteral(
        "QWidget { background: #1a1a1a; border-radius: 4px; border: 1px solid #111; }"));
    auto* cardLay = new QVBoxLayout(mExamplesCard);
    cardLay->setContentsMargins(10, 8, 10, 8);
    cardLay->setSpacing(0);
    mExamples = new QLabel(QString(), mExamplesCard);
    mExamples->setWordWrap(true);
    mExamples->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mExamples->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; border: none; color: #b3b3b3; font-family: monospace; }"));
    cardLay->addWidget(mExamples);
    mExamplesCard->hide();
    lay->addWidget(mExamplesCard, 0);

    // --- Close button (bottom, full width, CosmicRed background, dark text) ---
    const QColor cosmicRed = CosmicColor::red();
    mConfirm = new CosmicButton(tr("Close"), root);
    mConfirm->setBordered(false);
    mConfirm->setBackgroundColors(cosmicRed, cosmicRed.lighter(115), cosmicRed.darker(115));
    mConfirm->setFixedHeight(CosmicUiMetrics::controlThickness());
    mConfirm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mConfirm->setTextColor(QColor(0x2f, 0x2f, 0x2f, 0xff));
    lay->addWidget(mConfirm);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mConfirm, &QPushButton::clicked, this, &QDialog::accept);

    rebuildWarningIcon();
}

void CosmicWarningPopup::setTitleText(const QString& /*title*/)
{
    // Title label removed in new layout; kept for API compatibility.
}

void CosmicWarningPopup::setBodyText(const QString& body)
{
    if (mBody) {
        mBody->setText(body);
    }
}

void CosmicWarningPopup::setConfirmText(const QString& text)
{
    if (mConfirm) {
        mConfirm->setText(text);
        mConfirm->setDefault(true);
    }
}

void CosmicWarningPopup::setWarningIconEnabled(bool on)
{
    if (mWarningIconEnabled == on) return;
    mWarningIconEnabled = on;
    rebuildWarningIcon();
}

void CosmicWarningPopup::setConfirmGreen(bool green)
{
    if (!mConfirm) return;
    const QColor c = green ? CosmicColor::green() : CosmicColor::red();
    mConfirm->setBackgroundColors(c, c.lighter(115), c.darker(115));
    mConfirm->setTextColor(QColor(0x2f, 0x2f, 0x2f, 0xff));
}

void CosmicWarningPopup::setExamplesText(const QString& text)
{
    if (!mExamples || !mExamplesCard) return;
    mExamples->setText(text);
    mExamplesCard->setVisible(!text.isEmpty());
}
void CosmicWarningPopup::rebuildWarningIcon()
{
    if (!mIcon) return;

    if (!mWarningIconEnabled) {
        mIcon->hide();
        mIcon->setPixmap(QPixmap());
        return;
    }

    mIcon->show();

    QPixmap pm;
    const QIcon appWarn(QStringLiteral(":/Interaction/Warning.svg"));
    if (!appWarn.isNull()) {
        pm = appWarn.pixmap(QSize(32, 32));
    } else {
        pm = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(QSize(32, 32));
    }

    mIcon->setPixmap(pm);
    mIcon->setAlignment(Qt::AlignCenter);
}

void CosmicWarningPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    // Let the layout determine the natural height, fix only the width.
    clearFixedWindowSize();
    setMaximumWidth(360);
    setMinimumWidth(360);
    adjustSize();

    // Entire top area (icon + text) is the drag region; button zone is not.
    if (mConfirm) {
        setDragRegionHeight(qMax(10, mConfirm->geometry().top()));
    }

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize s = g0.size();
    const QSize s0(qMax(1, int(s.width() * 0.92)), qMax(1, int(s.height() * 0.92)));
    const QRect g1(QPoint(c.x() - s0.width() / 2, c.y() - s0.height() / 2), s0);
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

    if (mConfirm) {
        mConfirm->setDefault(true);
        mConfirm->setFocus(Qt::TabFocusReason);
    }
}
