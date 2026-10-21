#include "cosmiccanvassizepopup.h"

#include "cosmicwindowheader.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicbutton.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

CosmicCanvasSizePopup::CosmicCanvasSizePopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(34);
    setFixedWindowSize(QSize(340, 160));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CosmicCanvasSizePopupRoot"));
    root->setStyleSheet(QStringLiteral("QWidget#CosmicCanvasSizePopupRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    // --- window chrome ---
    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // --- centered title ---
    auto* titleLabel = new QLabel(tr("Canvas Size"), root);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 13px; }"));
    lay->addWidget(titleLabel);

    // --- dark card containing the two input fields ---
    const int cardRadius = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(root);
    card->setObjectName(QStringLiteral("CanvasSizeCard"));
    card->setStyleSheet(QString(
        "QWidget#CanvasSizeCard{ background: #1e1e1e; border-radius: %1px; }").arg(cardRadius));

    auto* cardLay = new QHBoxLayout(card);
    cardLay->setContentsMargins(12, 10, 12, 10);
    cardLay->setSpacing(0);

    // X label + field
    auto* xLabel = new QLabel(tr("X"), card);
    xLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 12px; }"));
    xLabel->setFixedWidth(14);
    cardLay->addWidget(xLabel);
    cardLay->addSpacing(6);

    mWidthField = new CosmicInputField(card);
    mWidthField->setVariant(CosmicInputField::Minimal);
    mWidthField->setRange(1, 10000);
    mWidthField->setDecimals(0);
    mWidthField->setSingleStep(1);
    mWidthField->setValue(1920);
    cardLay->addWidget(mWidthField, 1);

    // rounded accent separator
    const QColor accent = CosmicColor::accent();
    auto* sep = new QFrame(card);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedWidth(2);
    sep->setStyleSheet(QString(
        "QFrame{ background: %1; border: none; border-radius: 1px; }").arg(accent.name()));
    cardLay->addSpacing(10);
    cardLay->addWidget(sep);
    cardLay->addSpacing(10);

    // Y label + field
    auto* yLabel = new QLabel(tr("Y"), card);
    yLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #b3b3b3; font-size: 12px; }"));
    yLabel->setFixedWidth(14);
    cardLay->addWidget(yLabel);
    cardLay->addSpacing(6);

    mHeightField = new CosmicInputField(card);
    mHeightField->setVariant(CosmicInputField::Minimal);
    mHeightField->setRange(1, 10000);
    mHeightField->setDecimals(0);
    mHeightField->setSingleStep(1);
    mHeightField->setValue(1080);
    cardLay->addWidget(mHeightField, 1);

    lay->addWidget(card);

    // --- confirm / cancel row ---
    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(10);
    btnRow->addStretch(1);

    auto* cancelBtn = new CosmicButton(tr("Cancel"), root);
    cancelBtn->setBordered(true);
    btnRow->addWidget(cancelBtn);

    auto* confirmBtn = new CosmicButton(tr("Apply"), root);
    confirmBtn->setBordered(true);
    btnRow->addWidget(confirmBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,    &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(cancelBtn,  &QPushButton::clicked,               this, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked,               this, &QDialog::accept);
}

void CosmicCanvasSizePopup::setInitialSize(int width, int height)
{
    if (mWidthField)  mWidthField->setValue(width);
    if (mHeightField) mHeightField->setValue(height);
}

int CosmicCanvasSizePopup::canvasWidth() const
{
    return mWidthField ? static_cast<int>(mWidthField->value()) : 1920;
}

int CosmicCanvasSizePopup::canvasHeight() const
{
    return mHeightField ? static_cast<int>(mHeightField->value()) : 1080;
}

void CosmicCanvasSizePopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

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

    if (mWidthField) mWidthField->setFocus(Qt::OtherFocusReason);
}
