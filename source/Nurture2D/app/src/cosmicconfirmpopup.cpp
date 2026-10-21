#include "cosmicconfirmpopup.h"

#include "cosmicwindowheader.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

CosmicConfirmPopup::CosmicConfirmPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setFixedWindowSize(QSize(360, 1)); // width fixed; height resolved in showEvent
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicConfirmPopupRoot"));
    root->setStyleSheet(QStringLiteral("QWidget#CosmicConfirmPopupRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(12, 6, 12, 8);
    lay->setSpacing(3);

    // --- Header with close button ---
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // --- Question icon (centered) ---
    mIcon = new QLabel(root);
    mIcon->setFixedSize(32, 32);
    mIcon->setAlignment(Qt::AlignCenter);
    mIcon->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
    {
        const QIcon qi(QStringLiteral(":/Interaction/Question.svg"));
        mIcon->setPixmap(qi.isNull()
            ? QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(32, 32)
            : qi.pixmap(32, 32));
    }
    lay->addWidget(mIcon, 0, Qt::AlignHCenter);

    // --- Body text (centered, word-wrapped) ---
    mBody = new QLabel(QString(), root);
    mBody->setWordWrap(true);
    mBody->setAlignment(Qt::AlignCenter);
    mBody->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
    CosmicText::applyToWidget(mBody);
    lay->addWidget(mBody, 0);

    // --- 50/50 button row: Cancel (left) | Confirm (right) ---
    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);

    const int btnH = CosmicUiMetrics::controlThickness();
    const QColor cosmicGreen = CosmicColor::green();

    mCancel = new CosmicButton(tr("Cancel"), root);
    mCancel->setBordered(false);
    mCancel->setFixedHeight(btnH);
    mCancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnRow->addWidget(mCancel, 1);

    mConfirm = new CosmicButton(tr("OK"), root);
    mConfirm->setBordered(false);
    mConfirm->setBackgroundColors(cosmicGreen, cosmicGreen.lighter(115), cosmicGreen.darker(115));
    mConfirm->setTextColor(QColor(0x2f, 0x2f, 0x2f, 0xff));
    mConfirm->setFixedHeight(btnH);
    mConfirm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnRow->addWidget(mConfirm, 1);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,  &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(mCancel,  &QPushButton::clicked,               this, &QDialog::reject);
    connect(mConfirm, &QPushButton::clicked,               this, &QDialog::accept);
}

void CosmicConfirmPopup::setBodyText(const QString& body)
{
    if (mBody) mBody->setText(body);
}

void CosmicConfirmPopup::setConfirmText(const QString& text)
{
    if (mConfirm) mConfirm->setText(text);
}

void CosmicConfirmPopup::setCancelText(const QString& text)
{
    if (mCancel) mCancel->setText(text);
}

void CosmicConfirmPopup::focusConfirm()
{
    mConfirmFocused = true;
    if (mConfirm) mConfirm->setFocus(Qt::OtherFocusReason);
    // Visual: highlight confirm, dim cancel
    if (mConfirm && mCancel) {
        const QColor g = CosmicColor::green();
        mConfirm->setBackgroundColors(g, g.lighter(115), g.darker(115));
        mCancel->setBackgroundColors(QColor(0x4d,0x4d,0x4d,0xff),
                                     QColor(0x5a,0x5a,0x5a,0xff),
                                     QColor(0x40,0x40,0x40,0xff));
    }
}

void CosmicConfirmPopup::focusCancel()
{
    mConfirmFocused = false;
    if (mCancel) mCancel->setFocus(Qt::OtherFocusReason);
    // Visual: highlight cancel, dim confirm
    if (mConfirm && mCancel) {
        const QColor g = CosmicColor::green();
        // Dim confirm to a muted green
        mConfirm->setBackgroundColors(g.darker(140), g.darker(120), g.darker(160));
        mCancel->setBackgroundColors(QColor(0x5a,0x5a,0x5a,0xff),
                                     QColor(0x6a,0x6a,0x6a,0xff),
                                     QColor(0x4a,0x4a,0x4a,0xff));
    }
}

void CosmicConfirmPopup::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (mConfirmFocused) accept();
        else                 reject();
        return;
    case Qt::Key_Escape:
        reject();
        return;
    case Qt::Key_Left:
    case Qt::Key_Right:
        // Toggle between the two buttons — wraps in both directions
        if (mConfirmFocused) focusCancel();
        else                 focusConfirm();
        return;
    default:
        break;
    }
    CosmicPopupWindow::keyPressEvent(e);
}

void CosmicConfirmPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    // Let layout determine height, fix only width
    clearFixedWindowSize();
    setMinimumWidth(360);
    setMaximumWidth(360);
    adjustSize();

    // Drag region = everything above the button row
    if (mCancel) {
        setDragRegionHeight(qMax(10, mCancel->geometry().top()));
    }

    // Confirm is default focus
    focusConfirm();

    // Entrance animation
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
