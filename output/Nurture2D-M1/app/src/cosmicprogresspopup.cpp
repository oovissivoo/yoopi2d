#include "cosmicprogresspopup.h"

#include "../CUI/cosmicprogressbar.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicuimetrics.h"

#include <QApplication>
#include <QLabel>
#include <QShowEvent>
#include <QVBoxLayout>

CosmicProgressPopup::CosmicProgressPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(false);
    setShadowEnabled(true);
    setShadowPadding(14);
    setShadowStrength(90);
    setPanelColor(QColor(0x1e, 0x1e, 0x1e));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(1);
    setPanelRadius(CosmicUiMetrics::windowCornerRadius());
    setCenterOnShow(true);
    setFixedWindowSize(QSize(320, 1));

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicProgressRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#CosmicProgressRoot { background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(6);

    mTitleLabel = new QLabel(QString(), root);
    mTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mTitleLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; border: none;"
        " color: #e0e0e0; font-size: 13px; font-weight: 600; }"));
    lay->addWidget(mTitleLabel);

    mStatusLabel = new QLabel(QString(), root);
    mStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; border: none;"
        " color: #777777; font-size: 11px; }"));
    lay->addWidget(mStatusLabel);

    lay->addSpacing(6);

    mBar = new CosmicProgressBar(root);
    lay->addWidget(mBar);

    // Cancel button — hidden by default
    mCancelBtn = new CosmicButton(tr("Cancel"), root);
    mCancelBtn->setBordered(true);
    mCancelBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    mCancelBtn->hide();
    lay->addSpacing(4);
    lay->addWidget(mCancelBtn);

    connect(mCancelBtn, &QPushButton::clicked, this, [this] {
        mCanceled = true;
        emit canceled();
    });

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);
}

void CosmicProgressPopup::setTitle(const QString& title)
{
    if (mTitleLabel) mTitleLabel->setText(title);
}

void CosmicProgressPopup::setStatus(const QString& status)
{
    if (mStatusLabel) mStatusLabel->setText(status);
}

void CosmicProgressPopup::setRange(int minimum, int maximum)
{
    if (mBar) mBar->setRange(minimum, maximum);
}

void CosmicProgressPopup::setValue(int value)
{
    if (mBar) mBar->setValue(value);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void CosmicProgressPopup::setCancelEnabled(bool on)
{
    if (mCancelBtn) mCancelBtn->setVisible(on);
    clearFixedWindowSize();
    adjustSize();
}

int CosmicProgressPopup::maximum() const { return mBar ? mBar->maximum() : 100; }
int CosmicProgressPopup::minimum() const { return mBar ? mBar->minimum() : 0; }

void CosmicProgressPopup::showCentered()
{
    setCenterOnShow(true);
    show();
    raise();
    activateWindow();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void CosmicProgressPopup::showEvent(QShowEvent* e)
{
    clearFixedWindowSize();
    setMinimumWidth(320);
    setMaximumWidth(320);
    adjustSize();

    CosmicPopupWindow::showEvent(e);

    // No entrance animation — must be visible immediately before work starts
    setWindowOpacity(1.0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}
