#include "windowsscrollbarspage.h"

#include "preferencemanager.h"
#include "cosmicwindowheader.h"
#include "titlebarwidget.h"
#include "../../CUI/cosmicuimetrics.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QSvgRenderer>
#include <QApplication>
#include <QFile>
#include <QEvent>

// ─── WindowOptionTile ────────────────────────────────────────────────────────

static constexpr int kTileW        = 130;  // wide enough for 3:1 aspect SVGs
static constexpr int kTileH        = 44;   // icon area height (96×32 → ~130×43)
static constexpr int kUnderlineH   = 3;
static constexpr int kUnderlineMargin = 8;

WindowOptionTile::WindowOptionTile(const QString& iconPath, int value, QWidget* parent)
    : QWidget(parent), mIconPath(iconPath), mValue(value)
{
    setCursor(Qt::PointingHandCursor);
    setFixedSize(kTileW, kTileH + kUnderlineH + 6);
}

void WindowOptionTile::setTileSize(int w, int h)
{
    mTileW = w;
    mTileH = h;
    setFixedSize(mTileW, mTileH + kUnderlineH + 6);
}

void WindowOptionTile::setActive(bool on, bool animate){
    if (mActive == on) return;
    mActive = on;

    if (mAnim) mAnim->stop();

    if (animate) {
        mAnim = new QPropertyAnimation(this, "highlight", this);
        mAnim->setDuration(200);
        mAnim->setEasingCurve(QEasingCurve::OutCubic);
        mAnim->setStartValue(mHighlight);
        mAnim->setEndValue(on ? 1.0 : 0.0);
        mAnim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        setHighlight(on ? 1.0 : 0.0);
    }
}

void WindowOptionTile::setHighlight(qreal v)
{
    mHighlight = v;
    update();
}

void WindowOptionTile::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        emit clicked(mValue);
}

void WindowOptionTile::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QColor accent    = CosmicColor::accent();
    const QRectF bounds    = rect();
    const qreal  iconAreaH = mTileH;

    if (!mIconPath.isEmpty()) {
        QFile f(mIconPath);
        QSvgRenderer renderer;
        if (f.open(QIODevice::ReadOnly))
            renderer.load(f.readAll());
        else
            renderer.load(mIconPath);

        if (renderer.isValid()) {
            // Fit inside (bounds.width × iconAreaH) preserving aspect ratio
            const QSize native = renderer.defaultSize();
            const qreal aspect = (native.height() > 0)
                ? qreal(native.width()) / qreal(native.height()) : 1.0;

            qreal fitW = bounds.width();
            qreal fitH = fitW / aspect;
            if (fitH > iconAreaH) { fitH = iconAreaH; fitW = fitH * aspect; }

            // Slight scale-up when active, clamped to available space
            const qreal scale = 1.0 + 0.06 * mHighlight;
            fitW = qMin(fitW * scale, bounds.width());
            fitH = qMin(fitH * scale, iconAreaH);

            const qreal iconX = (bounds.width() - fitW) * 0.5;
            const qreal iconY = (iconAreaH       - fitH) * 0.5;
            renderer.render(&p, QRectF(iconX, iconY, fitW, fitH));
        }
    }

    // Accent underline — fades in with highlight, sits below the icon
    if (mHighlight > 0.001) {
        const qreal lineW  = bounds.width() - kUnderlineMargin * 2;
        const qreal lineX  = kUnderlineMargin;
        const qreal lineY  = mTileH + 4;
        const qreal radius = kUnderlineH * 0.5;

        // Soft glow behind the line
        QColor glow = accent;
        glow.setAlphaF(0.35 * mHighlight);
        QPainterPath glowPath;
        glowPath.addRoundedRect(lineX - 4, lineY - 2, lineW + 8, kUnderlineH + 4, radius + 2, radius + 2);
        p.fillPath(glowPath, glow);

        // Solid accent line
        QColor line = accent;
        line.setAlphaF(mHighlight);
        QPainterPath linePath;
        linePath.addRoundedRect(lineX, lineY, lineW, kUnderlineH, radius, radius);
        p.fillPath(linePath, line);
    }
}

QSize WindowOptionTile::sizeHint() const
{
    return QSize(mTileW, mTileH + kUnderlineH + 6);
}

// ─── WindowsScrollbarsPage ───────────────────────────────────────────────────

WindowsScrollbarsPage::WindowsScrollbarsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    if (QScrollBar* vsb = scroll->verticalScrollBar()) {
        vsb->setFixedWidth(12);
        vsb->setStyleSheet(
            "QScrollBar:vertical {"
            "  background: #1f1f1f;"
            "  border: none;"
            "  border-radius: 6px;"
            "  width: 12px;"
            "  margin: 0px;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: #505050;"
            "  border-radius: 6px;"
            "  min-height: 26px;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        );
    }

    auto* container = new QWidget;
    container->setStyleSheet("background: transparent;");
    auto* vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    // ── Windows card ──────────────────────────────────────────────────────────
    {
        auto* cardContent = new QWidget;
        auto* cardLayout  = new QVBoxLayout(cardContent);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(18);

        // Button Type row
        {
            auto* rowWidget = new QWidget;
            auto* rowLayout = new QVBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);

            auto* rowLabel = new QLabel(tr("Button Type"), rowWidget);
            rowLabel->setStyleSheet("color: #b3b3b3; font-size: 11px; font-weight: 600;");
            mButtonTypeLabel = rowLabel;

            auto* tilesRow = new QHBoxLayout;
            tilesRow->setSpacing(16);
            tilesRow->setContentsMargins(0, 0, 0, 0);

            mStyleDefault = new WindowOptionTile(
                ":/Windows&AppIcons/WindowButtonStyleDefault.svg", 0, this);
            mStyleMac = new WindowOptionTile(
                ":/Windows&AppIcons/WindowButtonStyleMac.svg", 1, this);

            tilesRow->addWidget(mStyleDefault);
            tilesRow->addWidget(mStyleMac);
            tilesRow->addStretch(1);

            rowLayout->addWidget(rowLabel);
            rowLayout->addLayout(tilesRow);

            connect(mStyleDefault, &WindowOptionTile::clicked, this, &WindowsScrollbarsPage::onCloseStyleChanged);
            connect(mStyleMac,     &WindowOptionTile::clicked, this, &WindowsScrollbarsPage::onCloseStyleChanged);

            cardLayout->addWidget(rowWidget);
        }

        // Separator
        {
            auto* sep = new QFrame;
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("background: #3a3a3a; border: none;");
            sep->setFixedHeight(1);
            cardLayout->addWidget(sep);
        }

        // Button Position row
        {
            auto* rowWidget = new QWidget;
            auto* rowLayout = new QVBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);

            auto* rowLabel = new QLabel(tr("Button Position"), rowWidget);
            rowLabel->setStyleSheet("color: #b3b3b3; font-size: 11px; font-weight: 600;");
            mButtonPositionLabel = rowLabel;

            auto* tilesRow = new QHBoxLayout;
            tilesRow->setSpacing(16);
            tilesRow->setContentsMargins(0, 0, 0, 0);

            // value 1 = left, 0 = right
            mPosLeft = new WindowOptionTile(
                ":/Windows&AppIcons/WindowButtonLocationLeft.svg", 1, this);
            mPosRight = new WindowOptionTile(
                ":/Windows&AppIcons/WindowButtonLocationRight.svg", 0, this);

            tilesRow->addWidget(mPosLeft);
            tilesRow->addWidget(mPosRight);
            tilesRow->addStretch(1);

            rowLayout->addWidget(rowLabel);
            rowLayout->addLayout(tilesRow);

            connect(mPosLeft,  &WindowOptionTile::clicked, this, &WindowsScrollbarsPage::onClosePositionChanged);
            connect(mPosRight, &WindowOptionTile::clicked, this, &WindowsScrollbarsPage::onClosePositionChanged);

            cardLayout->addWidget(rowWidget);
        }

        vbox->addWidget(makeCard(tr("Windows"), cardContent));
    }

    vbox->addStretch(1);
    scroll->setWidget(container);
    outerLayout->addWidget(scroll);
}

QWidget* WindowsScrollbarsPage::makeCard(const QString& title, QWidget* content)
{
    auto* card = new QGroupBox;
    card->setFlat(true);
    card->setTitle(QString());
    card->setStyleSheet(
        "QGroupBox {"
        "  background-color: #252525;"
        "  border: 1px solid #333333;"
        "  border-radius: 10px;"
        "}"
    );

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setAlignment(Qt::AlignHCenter);
    titleLabel->setStyleSheet(
        "color: #e0e0e0; font-size: 12px; font-weight: 700;"
        "background: transparent; border: none;"
    );
    // Store so we can retranslate it on language change.
    mCardTitleLabel = titleLabel;
    layout->addWidget(titleLabel);

    auto* sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background: #3a3a3a; border: none;");
    sep->setFixedHeight(1);
    layout->addWidget(sep);

    layout->addWidget(content);
    return card;
}

void WindowsScrollbarsPage::updateValues()
{
    if (!mManager) return;

    const int  style     = mManager->getInt(SETTING::WINDOW_CLOSE_STYLE);
    const bool closeLeft = mManager->isOn(SETTING::WINDOW_CLOSE_ON_LEFT);

    mStyleDefault->setActive(style == 0, false);
    mStyleMac->setActive(style == 1, false);
    mPosLeft->setActive(closeLeft,   false);
    mPosRight->setActive(!closeLeft, false);
}

void WindowsScrollbarsPage::onCloseStyleChanged(int value)
{
    mStyleDefault->setActive(value == 0);
    mStyleMac->setActive(value == 1);

    if (mManager)
        mManager->set(SETTING::WINDOW_CLOSE_STYLE, value);

    // Apply to all live CosmicWindowHeaders (popups/dialogs)
    CosmicWindowHeader::applyStyleToAll(value,
        mManager ? mManager->isOn(SETTING::WINDOW_CLOSE_ON_LEFT) : true);
    // Apply to all live TitleBarWidgets (dock panels)
    TitleBarWidget::applyStyleToAll(value,
        mManager ? mManager->isOn(SETTING::WINDOW_CLOSE_ON_LEFT) : true);
}

void WindowsScrollbarsPage::onClosePositionChanged(int value)
{
    const bool closeLeft = (value == 1);
    mPosLeft->setActive(closeLeft);
    mPosRight->setActive(!closeLeft);

    if (mManager)
        mManager->set(SETTING::WINDOW_CLOSE_ON_LEFT, closeLeft);

    // Apply to all live CosmicWindowHeaders (popups/dialogs)
    CosmicWindowHeader::applyStyleToAll(
        mManager ? mManager->getInt(SETTING::WINDOW_CLOSE_STYLE) : 0,
        closeLeft);
    // Apply to all live TitleBarWidgets (dock panels)
    TitleBarWidget::applyStyleToAll(
        mManager ? mManager->getInt(SETTING::WINDOW_CLOSE_STYLE) : 0,
        closeLeft);
}

void WindowsScrollbarsPage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslatePageStrings();
    }
}

void WindowsScrollbarsPage::retranslatePageStrings()
{
    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    if (mCardTitleLabel)      mCardTitleLabel->setText(safetr("Windows"));
    if (mButtonTypeLabel)     mButtonTypeLabel->setText(safetr("Button Type"));
    if (mButtonPositionLabel) mButtonPositionLabel->setText(safetr("Button Position"));
}
