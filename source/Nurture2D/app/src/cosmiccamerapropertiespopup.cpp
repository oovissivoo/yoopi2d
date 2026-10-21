#include "cosmiccamerapropertiespopup.h"

#include "cosmicwindowheader.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmicuimetrics.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
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

CosmicCameraPropertiesPopup::CosmicCameraPropertiesPopup(
    const QString& name, int width, int height, QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::WindowModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(360, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI(name, width, height);
}

void CosmicCameraPropertiesPopup::buildUI(const QString& name, int width, int height)
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("CameraPropsRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#CameraPropsRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Camera Properties"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── identity card ─────────────────────────────────────────────────────────
    QVBoxLayout* idLay = nullptr;
    auto* idCard = makeCard(root, idLay);
    lay->addWidget(idCard);
    idLay->addWidget(makeSectionTitle(tr("Identity"), idCard));
    idLay->addWidget(makeLabel(tr("The name shown in the layer panel and export dialogs."),
                               idCard, 10, true));
    idLay->addWidget(makeSeparator(idCard));

    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(10);
    nameRow->addWidget(makeLabel(tr("Name"), idCard));
    mNameEdit = new QLineEdit(idCard);
    mNameEdit->setText(name);
    mNameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{ background: #2a2a2a; color: #b3b3b3; border: 1px solid #3a3a3a;"
        " border-radius: 4px; padding: 4px 8px; font-size: 11px; }"));
    mNameEdit->setMinimumHeight(26);
    nameRow->addWidget(mNameEdit, 1);
    idLay->addLayout(nameRow);

    // ── canvas size card ──────────────────────────────────────────────────────
    QVBoxLayout* sizeLay = nullptr;
    auto* sizeCard = makeCard(root, sizeLay);
    lay->addWidget(sizeCard);
    sizeLay->addWidget(makeSectionTitle(tr("Canvas Size"), sizeCard));
    sizeLay->addWidget(makeLabel(tr("Output resolution in pixels for this camera layer."),
                                 sizeCard, 10, true));
    sizeLay->addWidget(makeSeparator(sizeCard));

    auto* sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(10);
    sizeRow->addWidget(makeLabel(tr("Width"), sizeCard));

    mWidthField = new CosmicInputField(sizeCard);
    mWidthField->setVariant(CosmicInputField::Minimal);
    mWidthField->setRange(1, 9999);
    mWidthField->setDecimals(0);
    mWidthField->setSingleStep(1);
    mWidthField->setValue(width);
    mWidthField->setFixedHeight(CosmicUiMetrics::controlThickness());
    sizeRow->addWidget(mWidthField, 1);

    sizeRow->addWidget(makeLabel(tr("Height"), sizeCard));

    mHeightField = new CosmicInputField(sizeCard);
    mHeightField->setVariant(CosmicInputField::Minimal);
    mHeightField->setRange(1, 9999);
    mHeightField->setDecimals(0);
    mHeightField->setSingleStep(1);
    mHeightField->setValue(height);
    mHeightField->setFixedHeight(CosmicUiMetrics::controlThickness());
    sizeRow->addWidget(mHeightField, 1);

    sizeLay->addLayout(sizeRow);

    // ── action row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch(1);

    auto* cancelBtn = new CosmicButton(tr("Cancel"), root);
    cancelBtn->setBordered(true);
    cancelBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(cancelBtn);

    auto* applyBtn = new CosmicButton(tr("Apply"), root);
    applyBtn->setBordered(true);
    const QColor accent = CosmicColor::accent();
    applyBtn->setBackgroundColors(accent, accent.lighter(115), accent.darker(115));
    applyBtn->setTextColor(QColor(0xff, 0xff, 0xff));
    applyBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    btnRow->addWidget(applyBtn);

    lay->addLayout(btnRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,   &CosmicWindowHeader::closeRequested, this, &QDialog::reject);
    connect(cancelBtn, &QPushButton::clicked,               this, &QDialog::reject);
    connect(applyBtn,  &QPushButton::clicked,               this, &QDialog::accept);
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void CosmicCameraPropertiesPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(360);
    setMaximumWidth(360);
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

    if (mNameEdit) mNameEdit->setFocus(Qt::OtherFocusReason);
}

// ── result accessors ──────────────────────────────────────────────────────────

QString CosmicCameraPropertiesPopup::cameraName() const
{
    return mNameEdit ? mNameEdit->text() : QString();
}

int CosmicCameraPropertiesPopup::cameraWidth() const
{
    return mWidthField ? static_cast<int>(mWidthField->value()) : 1920;
}

int CosmicCameraPropertiesPopup::cameraHeight() const
{
    return mHeightField ? static_cast<int>(mHeightField->value()) : 1080;
}
