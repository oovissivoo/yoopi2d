#include "cosmicsaveprogresspopup.h"

#include "../CUI/cosmicprogressbar.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QLabel>
#include <QApplication>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// CosmicSaveProgressPopup
// ---------------------------------------------------------------------------
CosmicSaveProgressPopup::CosmicSaveProgressPopup(QWidget* parent)
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

    setFixedWindowSize(QSize(320, 1)); // height resolved in showEvent

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicSaveProgressRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#CosmicSaveProgressRoot { background: transparent; border: none; }"
    ));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(6);

    // Title
    mTitleLabel = new QLabel(tr("Saving\xe2\x80\xa6"), root); // "Saving…"
    mTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mTitleLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; border: none; color: #e0e0e0; font-size: 13px; font-weight: 600; }"
    ));
    lay->addWidget(mTitleLabel);

    // Status
    mStatusLabel = new QLabel(QString(), root);
    mStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; border: none; color: #777777; font-size: 11px; }"
    ));
    lay->addWidget(mStatusLabel);

    lay->addSpacing(6);

    // Progress bar
    auto* bar = new CosmicProgressBar(root);
    mBar = bar;
    lay->addWidget(bar);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);
}

void CosmicSaveProgressPopup::setTitle(const QString& title)
{
    if (mTitleLabel) mTitleLabel->setText(title);
}

void CosmicSaveProgressPopup::setStatus(const QString& status)
{
    if (mStatusLabel) mStatusLabel->setText(status);
}

void CosmicSaveProgressPopup::setRange(int minimum, int maximum)
{
    if (mBar) mBar->setRange(minimum, maximum);
}

void CosmicSaveProgressPopup::setValue(int value)
{
    if (mBar) mBar->setValue(value);
}
void CosmicSaveProgressPopup::showCentered()
{
    setCenterOnShow(true);
    show();
    raise();
    activateWindow();
}

void CosmicSaveProgressPopup::showEvent(QShowEvent* e)
{
    // Resolve natural height before the base class centers us.
    clearFixedWindowSize();
    setMinimumWidth(320);
    setMaximumWidth(320);
    adjustSize();

    CosmicPopupWindow::showEvent(e);

    // No entrance animation — the popup must be fully opaque and painted
    // before the save operation begins so the user gets immediate feedback.
    setWindowOpacity(1.0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}
