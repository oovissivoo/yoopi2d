#include "cosmictoolspane.h"
#include "cosmicuimetrics.h"

/* ===== UI SIZE ADJUSTMENT LANDMARKS ===== */
/*
 * COSMIC PANE HEADER SIZE CONTROLS:
 * - Line 27: Header icon button fixed size (22x22) - reduced from 28x28 for tighter layout
 * - Line 75: Header layout margins (8,3,8,3) - reduced vertical padding for compact header
 * - Line 78: Header icon size (14x14) - smaller icons for header buttons
 * - Lines 88-96: Header title font forced to 9pt normal weight to match global QSS
 * 
 * TO ADJUST COSMIC PANE HEADER SIZES:
 * - Modify icon button size on line 27
 * - Adjust layout margins on line 75 (top,bottom values control header height)
 * - Change header icon size on line 78
 * - Update font size on line 92 (keep in sync with dark_theme.qss)
 * 
 * RELATED SIZE ADJUSTMENTS:
 * - dark_theme.qss: Global font size controls (9pt)
 * - toolboxwidget.cpp: Tools pane drawing tool icon sizes
 */

#include <QBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QApplication>

namespace {
QString interactionIconPath(const QString& iconName)
{
    return QStringLiteral(":/Interaction/") + iconName + QStringLiteral(".svg");
}

QToolButton* makeBorderlessIconButton(const QString& objectName, const QString& iconName, const QSize& iconSize, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(objectName);
    b->setAutoRaise(true);
    b->setIcon(QIcon(interactionIconPath(iconName)));
    b->setIconSize(iconSize);
    b->setFixedSize(22, 22);
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::ArrowCursor);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; padding: 0px; margin: 0px; }"
        "QToolButton:hover { background: rgba(255,255,255,18); border-radius: 6px; }"
        "QToolButton:pressed { background: rgba(255,255,255,28); border-radius: 6px; }"));
    return b;
}

QColor paneBg()
{
    return QColor(0x2a, 0x2a, 0x2a, 0xff);
}

QColor headerBg()
{
    return QColor(0x2f, 0x2f, 0x2f, 0xff);
}

QColor titleColor()
{
    return QColor(0xb8, 0xb8, 0xb8, 0xff);
}
}

CosmicToolsPane::CosmicToolsPane(const QString& title, Side side, QWidget* parent)
    : QWidget(parent)
    , mSide(side)
{
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    setMouseTracking(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mHeader = new QWidget(this);
    mHeader->setAttribute(Qt::WA_NoSystemBackground, true);
    mHeader->setAutoFillBackground(false);
    mHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* headerLayout = new QHBoxLayout(mHeader);
    headerLayout->setContentsMargins(8, 3, 8, 3);
    headerLayout->setSpacing(8);

    const QSize iconSz(14, 14);

    // Header controls: expand/collapse belongs near the pill (outer edge).
    // Drag handle belongs on the opposite edge (inner edge).
    mDragButton = makeBorderlessIconButton(QStringLiteral("DragPeg"), QStringLiteral("DragPeg"), iconSz, mHeader);
    mDragButton->installEventFilter(this);

    mTitleLabel = new QLabel(title, mHeader);
    mTitleLabel->setAlignment(Qt::AlignCenter);
    mTitleLabel->setStyleSheet(QStringLiteral("QLabel { color: #b8b8b8; background: transparent; padding: 0px; margin: 0px; }"));
    // Bandaid: force the pane header font size to match the QSS global font size.
    // Before changing this value, verify the stylesheet (dark_theme.qss) uses the same size so they stay in sync.
    {
        QFont f = mTitleLabel->font();
        f.setPointSize(9);
        f.setBold(false);
        f.setWeight(QFont::Normal);
        mTitleLabel->setFont(f);
    }

    mExpandButton = makeBorderlessIconButton(QStringLiteral("PaneExpandCollapse"), QStringLiteral("ArrowUp"), iconSz, mHeader);
    mExpandButton->setCheckable(true);
    mExpandButton->setChecked(true);

    QWidget* leftSlot = new QWidget(mHeader);
    auto* leftLayout = new QHBoxLayout(leftSlot);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    QWidget* centerSlot = new QWidget(mHeader);
    auto* centerLayout = new QHBoxLayout(centerSlot);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    mHeaderCenterHit = centerSlot;
    mHeaderCenterHit->setCursor(Qt::OpenHandCursor);
    mHeaderCenterHit->installEventFilter(this);
    mTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    QWidget* rightSlot = new QWidget(mHeader);
    auto* rightLayout = new QHBoxLayout(rightSlot);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    if (mSide == Side::Left) {
        leftLayout->addWidget(mExpandButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        rightLayout->addWidget(mDragButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    } else {
        leftLayout->addWidget(mDragButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        rightLayout->addWidget(mExpandButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
    centerLayout->addWidget(mTitleLabel, 1, Qt::AlignCenter);

    headerLayout->addWidget(leftSlot, 0);
    headerLayout->addWidget(centerSlot, 1);
    headerLayout->addWidget(rightSlot, 0);

    mHeader->setLayout(headerLayout);
    root->addWidget(mHeader, 0);

    mBody = new QWidget(this);
    mBody->setAttribute(Qt::WA_NoSystemBackground, true);
    mBody->setAutoFillBackground(false);
    mBody->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* bodyLayout = new QVBoxLayout(mBody);
    bodyLayout->setContentsMargins(4, 10, 4, 10);
    bodyLayout->setSpacing(0);
    mBody->setLayout(bodyLayout);

    root->addWidget(mBody, 1);
    setLayout(root);

    // Per-pane resize grip: invisible strip at the outer edge.
    // This is the only area that should show the resize cursor and initiate resizing.
    mResizeGrip = new QWidget(this);
    mResizeGrip->setObjectName(QStringLiteral("CosmicToolsPaneResizeGrip"));
    mResizeGrip->setAttribute(Qt::WA_NoSystemBackground, true);
    mResizeGrip->setAutoFillBackground(false);
    mResizeGrip->setMouseTracking(true);
    mResizeGrip->setCursor(Qt::SplitHCursor);
    mResizeGrip->installEventFilter(this);
    mResizeGrip->raise();

    // Per-pane vertical resize grip: invisible strip at the bottom edge.
    mVerticalResizeGrip = new QWidget(this);
    mVerticalResizeGrip->setObjectName(QStringLiteral("CosmicToolsPaneVerticalResizeGrip"));
    mVerticalResizeGrip->setAttribute(Qt::WA_NoSystemBackground, true);
    mVerticalResizeGrip->setAutoFillBackground(false);
    mVerticalResizeGrip->setMouseTracking(true);
    mVerticalResizeGrip->setCursor(Qt::SplitVCursor);
    mVerticalResizeGrip->setVisible(false);
    mVerticalResizeGrip->setEnabled(false);
    mVerticalResizeGrip->installEventFilter(this);
    mVerticalResizeGrip->raise();

    updateResizeGripGeometry();
    updateVerticalResizeGripGeometry();

    // Ensure the header never grows/shrinks when the pane is vertically resized.
    // The body/content area is the only region that should take extra height.
    if (mHeader) {
        mHeader->setFixedHeight(mHeader->sizeHint().height());
    }

    updateResizeGripGeometry();
    updateVerticalResizeGripGeometry();

    connect(mExpandButton, &QAbstractButton::toggled, this, [this](bool on) {
        setExpanded(on);
    });

    setExpanded(true);
}

void CosmicToolsPane::setFloatingPopupMode(bool on)
{
    if (mFloatingPopupMode == on) {
        return;
    }
    mFloatingPopupMode = on;
    mPaintCacheDirty = true;
    update();

    // Popup panes should not behave like stack panes (no collapse/move transfer logic).
    // They are meant to act as lightweight floating windows.
    if (mExpandButton) {
        mExpandButton->setVisible(!on);
        mExpandButton->setEnabled(!on);
    }
    if (mDragButton) {
        mDragButton->setVisible(!on);
        mDragButton->setEnabled(!on);
    }
    if (mHeaderCenterHit) {
        mHeaderCenterHit->setCursor(on ? Qt::ArrowCursor : Qt::OpenHandCursor);
    }

    // Ensure body stays visible in popup mode.
    if (on) {
        if (!mExpanded) {
            mExpanded = true;
        }
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumHeight(0);
        syncBodyVisibility();
    }

    updateGeometry();
    update();
}

void CosmicToolsPane::setSide(Side side)
{
    if (mSide == side) {
        return;
    }
    mSide = side;
    mPaintCacheDirty = true;
    updateHeaderUi();
    updateResizeGripGeometry();
    update();
}

void CosmicToolsPane::setResizeGripEnabled(bool on)
{
    if (mResizeGripEnabled == on) {
        return;
    }
    mResizeGripEnabled = on;
    if (mResizeGrip) {
        const bool effective = on && mExpanded;
        mResizeGrip->setVisible(effective);
        mResizeGrip->setEnabled(effective);
    }
}

void CosmicToolsPane::setVerticalResizeGripEnabled(bool on)
{
    if (mVerticalResizeGripEnabled == on) {
        return;
    }
    mVerticalResizeGripEnabled = on;
    if (mVerticalResizeGrip) {
        const bool effective = on && mExpanded;
        mVerticalResizeGrip->setVisible(effective);
        mVerticalResizeGrip->setEnabled(effective);
    }
}

void CosmicToolsPane::setBodyForcedHidden(bool hidden)
{
    if (mBodyForcedHidden == hidden) {
        return;
    }
    mBodyForcedHidden = hidden;
    syncBodyVisibility();
    updateGeometry();
    update();
}

void CosmicToolsPane::setDisplayTitle(const QString& title)
{
    if (mTitleLabel) mTitleLabel->setText(title);
}

QString CosmicToolsPane::displayTitle() const
{
    return mTitleLabel ? mTitleLabel->text() : QString();
}

void CosmicToolsPane::setContentWidget(QWidget* w)
{
    if (w == mContent) {
        return;
    }

    if (mContent) {
        mContent->setParent(nullptr);
    }
    mContent = w;

    if (!mBody || !mContent) {
        return;
    }

    // Make embedded content visually blend with the pane (panes already draw their own background).
    mContent->setAutoFillBackground(false);
    mContent->setAttribute(Qt::WA_NoSystemBackground, true);
    if (auto* sa = qobject_cast<QScrollArea*>(mContent)) {
        sa->setFrameShape(QFrame::NoFrame);
        sa->setAutoFillBackground(false);
        sa->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        sa->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        sa->viewport()->setAutoFillBackground(false);
        sa->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        sa->setStyleSheet(QStringLiteral(
            "QScrollArea{background:transparent;border:none;}"
            "QWidget{background:transparent;}"));
    }

    if (!mBody->layout()) {
        auto* bodyLayout = new QVBoxLayout(mBody);
        bodyLayout->setContentsMargins(10, 10, 10, 10);
        bodyLayout->setSpacing(0);
        mBody->setLayout(bodyLayout);
    }

    mContent->setParent(mBody);
    mContent->setVisible(true);
    mBody->layout()->addWidget(mContent);
    mBody->layout()->activate();

    syncBodyVisibility();
    updateGeometry();
}

QWidget* CosmicToolsPane::contentWidget() const
{
    return mContent;
}

void CosmicToolsPane::refreshContentWidget()
{
    if (!mContent || !mBody) {
        return;
    }
    
    // Ensure content is properly parented to the body
    if (mContent->parent() != mBody) {
        mContent->setParent(mBody);
    }
    
    // Ensure content is visible
    mContent->setVisible(true);
    
    // Ensure the body layout contains the content
    if (mBody->layout()) {
        if (mBody->layout()->indexOf(mContent) < 0) {
            mBody->layout()->addWidget(mContent);
        }
        mBody->layout()->activate();
        mBody->layout()->update();
    }
    
    // Force geometry update
    mContent->updateGeometry();
    
    // Sync body visibility to ensure content shows if expanded
    syncBodyVisibility();
}

bool CosmicToolsPane::eventFilter(QObject* watched, QEvent* event)
{
    if (mFloatingPopupMode) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == mResizeGrip && event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mResizeGripEnabled && me->button() == Qt::LeftButton) {
                mResizeActive = true;
                emit resizeDragStarted(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mResizeGripEnabled && mResizeActive) {
                emit resizeDragMoved(me->globalPos());
                event->accept();
                return true;
            }
            // Hover cursor feedback is handled by the grip widget cursor.
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mResizeGripEnabled && mResizeActive && me->button() == Qt::LeftButton) {
                mResizeActive = false;
                emit resizeDragEnded();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (watched == mVerticalResizeGrip && event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mVerticalResizeGripEnabled && me->button() == Qt::LeftButton) {
                mVerticalResizeActive = true;
                emit resizeVDragStarted(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mVerticalResizeGripEnabled && mVerticalResizeActive) {
                emit resizeVDragMoved(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mVerticalResizeGripEnabled && mVerticalResizeActive && me->button() == Qt::LeftButton) {
                mVerticalResizeActive = false;
                emit resizeVDragEnded();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (watched == mDragButton && event) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                mDragActive = true;
                mDragStarted = true;
                mDragPressGlobalPos = me->globalPos();
                emit moveDragStarted(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (mDragActive) {
                auto* me = static_cast<QMouseEvent*>(event);
                emit moveDragMoved(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mDragActive && me->button() == Qt::LeftButton) {
                mDragActive = false;
                mDragStarted = false;
                emit moveDragEnded();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (watched == mHeaderCenterHit && event) {
        switch (event->type()) {
        case QEvent::MouseButtonDblClick: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // Double-click on the header title area toggles expand/collapse.
                // Cancel any pending/active drag so it doesn't conflict.
                mDragActive = false;
                mDragStarted = false;
                setExpanded(!isExpanded());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                mDragActive = true;
                mDragStarted = false;
                mDragPressGlobalPos = me->globalPos();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (mDragActive) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (!mDragStarted) {
                    const int dist = (me->globalPos() - mDragPressGlobalPos).manhattanLength();
                    if (dist >= QApplication::startDragDistance()) {
                        mDragStarted = true;
                        emit dragStarted(mDragPressGlobalPos);
                    } else {
                        event->accept();
                        return true;
                    }
                }
                emit dragMoved(me->globalPos());
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mDragActive && me->button() == Qt::LeftButton) {
                mDragActive = false;
                mDragStarted = false;
                emit dragEnded();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CosmicToolsPane::updateHeaderUi()
{
    if (!mHeader) {
        return;
    }

    // Rebuild the header slots so expand/collapse stays on the outer edge
    // and the move peg stays on the inner edge, depending on side.
    auto* headerLayout = qobject_cast<QHBoxLayout*>(mHeader->layout());
    if (!headerLayout || headerLayout->count() < 3) {
        return;
    }

    QWidget* leftSlot = headerLayout->itemAt(0) ? headerLayout->itemAt(0)->widget() : nullptr;
    QWidget* rightSlot = headerLayout->itemAt(2) ? headerLayout->itemAt(2)->widget() : nullptr;
    if (!leftSlot || !rightSlot) {
        return;
    }

    auto* leftLayout = qobject_cast<QHBoxLayout*>(leftSlot->layout());
    auto* rightLayout = qobject_cast<QHBoxLayout*>(rightSlot->layout());
    if (!leftLayout || !rightLayout) {
        return;
    }

    leftLayout->removeWidget(mDragButton);
    leftLayout->removeWidget(mExpandButton);
    rightLayout->removeWidget(mDragButton);
    rightLayout->removeWidget(mExpandButton);

    if (mSide == Side::Left) {
        leftLayout->addWidget(mExpandButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        rightLayout->addWidget(mDragButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    } else {
        leftLayout->addWidget(mDragButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        rightLayout->addWidget(mExpandButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
}

void CosmicToolsPane::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateResizeGripGeometry();
    updateVerticalResizeGripGeometry();
    mPaintCacheDirty = true;
}

void CosmicToolsPane::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);

    if (e->type() == QEvent::ParentChange) {
        mPaintCacheDirty = true;
        update();
    }
}

void CosmicToolsPane::ensurePaintCache()
{
    const QRect r = rect();
    const QRect hg = mHeader ? mHeader->geometry() : QRect();

    const bool floating = isWindow();
    if (!mPaintCacheDirty && mPaintCacheRect == r && mPaintCacheHeaderGeom == hg && mPaintCacheIsWindow == floating) {
        return;
    }

    mPaintCacheDirty = false;
    mPaintCacheRect = r;
    mPaintCacheHeaderGeom = hg;
    mPaintCacheIsWindow = floating;

    const qreal radius = static_cast<qreal>(CosmicUiMetrics::windowCornerRadius());
    const QRectF rf = QRectF(r);

    auto makeOuterPath = [&](const QRectF& rc) -> QPainterPath {
        QPainterPath path;
        if (isWindow()) {
            path.addRoundedRect(rc, radius, radius);
        } else {
            // Docked inside panel: completely flat, no rounding.
            path.addRect(rc);
        }
        return path;
    };

    mPaintOuterPath = makeOuterPath(rf);

    mPaintHeaderPath = QPainterPath();
    mPaintHeaderClip = QPainterPath();
    if (!hg.isNull()) {
        const QRectF hr = QRectF(hg);
        mPaintHeaderPath = makeOuterPath(hr);

        // Square bottom corners so the header blends into body.
        mPaintHeaderClip.addRect(hr.adjusted(0, 0, 0, radius));
    }
}

void CosmicToolsPane::updateResizeGripGeometry()
{
    if (!mResizeGrip) {
        return;
    }

    // Keep it invisible: no paint, just a hit target + cursor.
    const int gripW = 6;
    const int headerH = mHeader ? mHeader->height() : 0;
    // Offset by 1px so the resize cursor never appears when hovering the header area
    // due to rounding / style differences.
    const int y = qMax(0, headerH + 1);
    const int h = qMax(0, height() - y);
    if (mSide == Side::Left) {
        mResizeGrip->setGeometry(width() - gripW, y, gripW, h);
    } else {
        mResizeGrip->setGeometry(0, y, gripW, h);
    }
    mResizeGrip->raise();
}

void CosmicToolsPane::updateVerticalResizeGripGeometry()
{
    if (!mVerticalResizeGrip) {
        return;
    }

    // Make the vertical resize hit target generous so users don't need pixel-perfect precision.
    const int gripH = 16;
    mVerticalResizeGrip->setGeometry(0, height() - gripH, width(), gripH);
    mVerticalResizeGrip->raise();
}

void CosmicToolsPane::setExpanded(bool expanded)
{
    if (mExpanded == expanded) {
        return;
    }

    // True collapse behavior: collapsed panes must shrink to a fixed header-only height.
    // Also remember the pre-collapse height so expanding restores it.
    const int headerH = (mHeader) ? mHeader->height() : 0;
    const int collapsedH = (headerH > 0) ? headerH : sizeHint().height();

    if (!expanded) {
        // Capture current expanded height before collapsing.
        // Avoid capturing tiny values during early layout by requiring a minimum.
        const int currentH = height();
        if (currentH > collapsedH) {
            mLastExpandedHeight = currentH;
        }

        mExpanded = false;
        if (mExpandButton) {
            mExpandButton->setChecked(false);
        }
        updateExpandIcon();

        syncBodyVisibility();

        // Force header-only height.
        setFixedHeight(collapsedH);
        setMaximumHeight(collapsedH);

        // When collapsed, the pane must be draggable/reorderable without accidental resizes.
        // Hide resize hit targets entirely.
        if (mResizeGrip) {
            mResizeGrip->setVisible(false);
            mResizeGrip->setEnabled(false);
        }

        if (mVerticalResizeGrip) {
            mVerticalResizeGrip->setVisible(false);
            mVerticalResizeGrip->setEnabled(false);
        }

        updateGeometry();
        update();
        emit expandedChanged(false);
        return;
    }

    // Expanding: restore previous height if we have one; otherwise let layout decide.
    mExpanded = true;
    if (mExpandButton) {
        mExpandButton->setChecked(true);
    }
    updateExpandIcon();

    setMaximumHeight(QWIDGETSIZE_MAX);
    if (mLastExpandedHeight > 0) {
        setFixedHeight(mLastExpandedHeight);
    } else {
        // If we don't have a stored expanded height (common on first run, or when
        // a pane was restored collapsed before any real layout occurred), we must
        // still expand to a non-header height; otherwise the body becomes visible
        // but has no space and appears as "no content".
        const int fallback = qMax(collapsedH + 1, sizeHint().height());
        setFixedHeight(fallback);
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    }

    syncBodyVisibility();

    // Restore resize hit targets according to current flags.
    if (mResizeGrip) {
        mResizeGrip->setVisible(mResizeGripEnabled);
        mResizeGrip->setEnabled(mResizeGripEnabled);
    }

    if (mVerticalResizeGrip) {
        const bool on = mExpanded && mVerticalResizeGripEnabled;
        mVerticalResizeGrip->setVisible(on);
        mVerticalResizeGrip->setEnabled(on);
    }

    updateGeometry();
    update();
    emit expandedChanged(true);
}

void CosmicToolsPane::syncBodyVisibility()
{
    if (!mBody) {
        return;
    }
    const bool on = mExpanded && !mBodyForcedHidden;
    mBody->setVisible(on);
    
    // When expanding, ensure content widget is visible and properly laid out
    if (on && mContent) {
        mContent->setVisible(true);
        if (mBody->layout()) {
            mBody->layout()->activate();
            mBody->layout()->update();
        }
        // Force a geometry update to ensure content is properly sized
        mContent->updateGeometry();
    }
}

void CosmicToolsPane::updateExpandIcon()
{
    if (!mExpandButton) {
        return;
    }

    // Expanded uses ArrowUp (collapse), collapsed uses ArrowDown (expand)
    const QString iconName = mExpanded ? QStringLiteral("ArrowUp") : QStringLiteral("ArrowDown");
    mExpandButton->setIcon(QIcon(interactionIconPath(iconName)));
}

void CosmicToolsPane::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    ensurePaintCache();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect());

    // Outer pane
    p.setPen(Qt::NoPen);
    p.setBrush(paneBg());
    p.drawPath(mPaintOuterPath);

    // Subtle outline
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawPath(mPaintOuterPath);
}
