#include "timelinesettingspopup.h"

#include "cosmicwindowheader.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicuimetrics.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

TimelineSettingsPopup::TimelineSettingsPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::NonModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(34);
    setFixedWindowSize(QSize(320, 530));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    const int cardRadius = CosmicUiMetrics::windowCornerRadius();

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("TLSettingsRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#TLSettingsRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);
    connect(mHeader, &CosmicWindowHeader::closeRequested, this, &QDialog::reject);

    auto* titleLabel = new QLabel(tr("Timeline Settings"), root);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 13px; }"));
    lay->addWidget(titleLabel);

    // ── Card: Playback Range ─────────────────────────────────────────────
    {
        auto* card = new QWidget(root);
        card->setObjectName(QStringLiteral("TLCard"));
        card->setStyleSheet(QString(
            "QWidget#TLCard{ background: #1e1e1e; border-radius: %1px; }").arg(cardRadius));

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 10);
        cardLay->setSpacing(8);

        auto* sectionLbl = new QLabel(tr("PLAYBACK RANGE"), card);
        sectionLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #606060; font-size: 9px; }"));
        cardLay->addWidget(sectionLbl);

        auto* rangeRow = new QHBoxLayout();
        rangeRow->setContentsMargins(0, 0, 0, 0);
        rangeRow->setSpacing(8);

        auto* startLbl = new QLabel(tr("Start"), card);
        startLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #b3b3b3; font-size: 11px; }"));
        rangeRow->addWidget(startLbl);

        mLoopStartField = new CosmicInputField(card);
        mLoopStartField->setVariant(CosmicInputField::Minimal);
        mLoopStartField->setRange(1, 240);
        mLoopStartField->setDecimals(0);
        mLoopStartField->setSingleStep(1);
        mLoopStartField->setValue(1);
        mLoopStartField->setFixedHeight(26);
        rangeRow->addWidget(mLoopStartField, 1);

        auto* endLbl = new QLabel(tr("End"), card);
        endLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #b3b3b3; font-size: 11px; }"));
        rangeRow->addWidget(endLbl);

        mLoopEndField = new CosmicInputField(card);
        mLoopEndField->setVariant(CosmicInputField::Minimal);
        mLoopEndField->setRange(2, 240);
        mLoopEndField->setDecimals(0);
        mLoopEndField->setSingleStep(1);
        mLoopEndField->setValue(2);
        mLoopEndField->setFixedHeight(26);
        rangeRow->addWidget(mLoopEndField, 1);

        cardLay->addLayout(rangeRow);
        lay->addWidget(card);
    }

    // ── Card: Display ────────────────────────────────────────────────────
    {
        auto* card = new QWidget(root);
        card->setObjectName(QStringLiteral("TLCard"));
        card->setStyleSheet(QString(
            "QWidget#TLCard{ background: #1e1e1e; border-radius: %1px; }").arg(cardRadius));

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 10);
        cardLay->setSpacing(8);

        auto* sectionLbl = new QLabel(tr("DISPLAY"), card);
        sectionLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #606060; font-size: 9px; }"));
        cardLay->addWidget(sectionLbl);

        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);

        auto* lbl = new QLabel(tr("Timecode"), card);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #b3b3b3; font-size: 11px; }"));
        row->addWidget(lbl);

        mTimestampDropDown = new CosmicDropDown(card);
        mTimestampDropDown->setOptions(QStringList()
            << tr("No Text")
            << tr("Frame Number")
            << tr("SMPTE  MM:SS:FF")
            << tr("FPS  S:FF"));
        mTimestampDropDown->setFixedHeight(26);
        row->addWidget(mTimestampDropDown, 1);

        cardLay->addLayout(row);
        lay->addWidget(card);
    }

    // ── Card: Scrollbars (3×2 grid of WindowOptionTiles) ─────────────────
    {
        auto* card = new QWidget(root);
        card->setObjectName(QStringLiteral("TLCard"));
        card->setStyleSheet(QString(
            "QWidget#TLCard{ background: #1e1e1e; border-radius: %1px; }").arg(cardRadius));

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(12, 10, 12, 10);
        cardLay->setSpacing(8);

        auto* sectionLbl = new QLabel(tr("SCROLLBARS"), card);
        sectionLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #606060; font-size: 9px; }"));
        cardLay->addWidget(sectionLbl);

        // Row labels
        auto* hLbl = new QLabel(tr("Horizontal"), card);
        hLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #b3b3b3; font-size: 10px; }"));

        auto* vLbl = new QLabel(tr("Vertical"), card);
        vLbl->setStyleSheet(QStringLiteral(
            "QLabel{ background: transparent; color: #b3b3b3; font-size: 10px; }"));

        // Tiles — values match hMode/vMode: 0=Bottom/Left, 1=Top/Right, 2=Both
        // Square tiles: icons are 1:1, 3 per row in ~276px usable width
        const int tW = 72, tH = 72;
        mHScrollBottom = new WindowOptionTile(
            QStringLiteral(":/Windows&AppIcons/ScrollbarBottom.svg"), 0, card);
        mHScrollBottom->setTileSize(tW, tH);
        mHScrollTop    = new WindowOptionTile(
            QStringLiteral(":/Windows&AppIcons/ScrollbarTop.svg"),    1, card);
        mHScrollTop->setTileSize(tW, tH);
        mHScrollBoth   = new WindowOptionTile(
            QStringLiteral(":/Windows&AppIcons/ScrollbarTop+Bottom.svg"), 2, card);
        mHScrollBoth->setTileSize(tW, tH);

        mVScrollLeft   = new WindowOptionTile(
            QStringLiteral(":/Windows&AppIcons/ScrollbarLeft.svg"),   0, card);
        mVScrollLeft->setTileSize(tW, tH);
        mVScrollRight  = new WindowOptionTile(
            QStringLiteral(":/Windows&AppIcons/ScrollbarRight.svg"),  1, card);
        mVScrollRight->setTileSize(tW, tH);
        mVScrollBoth   = new WindowOptionTile(
            QStringLiteral(":/Windows&AppIcons/ScrollbarLeft+Right.svg"), 2, card);
        mVScrollBoth->setTileSize(tW, tH);

        // 3×2 grid: col 0-2 = tiles, row 0 = H label+tiles, row 1 = V label+tiles
        auto* grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(6);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 1);

        grid->addWidget(hLbl,          0, 0, 1, 3);
        grid->addWidget(mHScrollBottom, 1, 0);
        grid->addWidget(mHScrollTop,    1, 1);
        grid->addWidget(mHScrollBoth,   1, 2);

        grid->addWidget(vLbl,          2, 0, 1, 3);
        grid->addWidget(mVScrollLeft,   3, 0);
        grid->addWidget(mVScrollRight,  3, 1);
        grid->addWidget(mVScrollBoth,   3, 2);

        cardLay->addLayout(grid);
        lay->addWidget(card);
    }

    lay->addStretch(1);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);
}

// ── showEvent — exact same fade+scale as CosmicExtraSpacePopup ───────────────

void TimelineSettingsPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);   // ← this centers on screen

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize  s = g0.size();
    const QSize  s0(qMax(1, int(s.width()  * 0.92)),
                    qMax(1, int(s.height() * 0.92)));
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
