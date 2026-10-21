#include "cosmicautosavepopup.h"

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

CosmicAutoSavePopup::CosmicAutoSavePopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setFixedWindowSize(QSize(380, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicAutoSavePopupRoot"));
    root->setStyleSheet(QStringLiteral("QWidget#CosmicAutoSavePopupRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(12, 6, 12, 10);
    lay->setSpacing(4);

    // Header with close button (close = snooze 15)
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // Save icon
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

    // Body text
    mBody = new QLabel(tr("Your project has unsaved changes.\nWould you like to save now?"), root);
    mBody->setWordWrap(true);
    mBody->setAlignment(Qt::AlignCenter);
    mBody->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; margin: 0px; padding: 0px; }"));
    CosmicText::applyToWidget(mBody);
    lay->addWidget(mBody, 0);

    lay->addSpacing(4);

    // Button row: [Snooze 15 min] [Snooze 5 min] [Save Now]
    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);

    const int btnH = CosmicUiMetrics::controlThickness();
    const QColor cosmicGreen = CosmicColor::green();

    mSnooze15 = new CosmicButton(tr("Snooze 15 min"), root);
    mSnooze15->setBordered(false);
    mSnooze15->setFixedHeight(btnH);
    mSnooze15->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnRow->addWidget(mSnooze15, 1);

    mSnooze5 = new CosmicButton(tr("Snooze 5 min"), root);
    mSnooze5->setBordered(false);
    mSnooze5->setFixedHeight(btnH);
    mSnooze5->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnRow->addWidget(mSnooze5, 1);

    mSaveNow = new CosmicButton(tr("Save Now"), root);
    mSaveNow->setBordered(false);
    mSaveNow->setBackgroundColors(cosmicGreen, cosmicGreen.lighter(115), cosmicGreen.darker(115));
    mSaveNow->setTextColor(QColor(0x2f, 0x2f, 0x2f, 0xff));
    mSaveNow->setFixedHeight(btnH);
    mSaveNow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnRow->addWidget(mSaveNow, 1);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    // Close button on header = snooze 15
    connect(mHeader,   &CosmicWindowHeader::closeRequested, this, [this]() { done(Snooze15); });
    connect(mSnooze15, &QPushButton::clicked,               this, [this]() { done(Snooze15); });
    connect(mSnooze5,  &QPushButton::clicked,               this, [this]() { done(Snooze5); });
    connect(mSaveNow,  &QPushButton::clicked,               this, &QDialog::accept);
}

void CosmicAutoSavePopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(380);
    setMaximumWidth(380);
    adjustSize();

    if (mSnooze15) {
        setDragRegionHeight(qMax(10, mSnooze15->geometry().top()));
    }

    if (mSaveNow) mSaveNow->setFocus(Qt::OtherFocusReason);

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

void CosmicAutoSavePopup::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        accept(); // Save Now
        return;
    case Qt::Key_Escape:
        done(Snooze15);
        return;
    default:
        break;
    }
    CosmicPopupWindow::keyPressEvent(e);
}
