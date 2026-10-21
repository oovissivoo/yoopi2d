#include "cosmicinputpopup.h"

#include "cosmicwindowheader.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmictext.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

CosmicInputPopup::CosmicInputPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(34);
    setFixedWindowSize(QSize(420, 150));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicInputPopupRoot"));
    root->setStyleSheet(QStringLiteral("QWidget#CosmicInputPopupRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(8);

    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    mTitle = new QLabel(root);
    mTitle->setStyleSheet(QStringLiteral("QLabel{ background: transparent; color: #ffffff; font-weight: 700; }"));
    lay->addWidget(mTitle);

    mBody = new QLabel(root);
    mBody->setWordWrap(true);
    mBody->setStyleSheet(QStringLiteral("QLabel{ background: transparent; border: none; }"));
    CosmicText::applyToWidget(mBody);
    mBody->hide(); // hidden until setBodyText is called
    lay->addWidget(mBody);

    mEdit = new QLineEdit(root);
    mEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  background: #1f1f1f;"
        "  border: 1px solid #3a3a3a;"
        "  border-radius: %1px;"
        "  padding: 4px 8px;"
        "  color: #e6e6e6;"
        "  font-size: 13px;"
        "  selection-background-color: %2;"
        "  selection-color: #ffffff;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid %2;"
        "}"
    ).arg(CosmicUiMetrics::windowCornerRadius())
     .arg(CosmicColor::accentHex()));
    mEdit->setFixedHeight(30);
    lay->addWidget(mEdit);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(10);
    btnRow->addStretch(1);

    mCancel = new CosmicButton(tr("Cancel"), root);
    mCancel->setBordered(true);
    btnRow->addWidget(mCancel);

    mConfirm = new CosmicButton(tr("OK"), root);
    mConfirm->setBordered(true);
    btnRow->addWidget(mConfirm);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,  &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(mCancel,  &QPushButton::clicked,               this, &QDialog::reject);
    connect(mConfirm, &QPushButton::clicked,               this, &QDialog::accept);

    // Enter key confirms
    connect(mEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
}

void CosmicInputPopup::setTitleText(const QString& title)
{
    if (mTitle) mTitle->setText(title);
}

void CosmicInputPopup::setBodyText(const QString& body)
{
    if (!mBody) return;
    mBody->setText(body);
    mBody->setVisible(!body.isEmpty());
}

void CosmicInputPopup::setInitialText(const QString& text)
{
    if (mEdit) {
        mEdit->setText(text);
        mEdit->selectAll();
    }
}

void CosmicInputPopup::setPlaceholderText(const QString& text)
{
    if (mEdit) mEdit->setPlaceholderText(text);
}

void CosmicInputPopup::setConfirmText(const QString& text)
{
    if (mConfirm) {
        mConfirm->setText(text);
        mConfirm->setDefault(true);
    }
}

void CosmicInputPopup::setCancelText(const QString& text)
{
    if (mCancel) mCancel->setText(text);
}

QString CosmicInputPopup::text() const
{
    return mEdit ? mEdit->text().trimmed() : QString();
}

void CosmicInputPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

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

    if (mEdit) {
        mEdit->setFocus(Qt::OtherFocusReason);
        mEdit->selectAll();
    }
    if (mConfirm) mConfirm->setDefault(true);
}
