#include "cosmicbottomtoolsbar.h"

#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include <QBoxLayout>
#include <QLayoutItem>
#include <QMouseEvent>
#include <QSettings>
#include <QTimer>
#include <QCursor>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>

namespace {
QString interactionIconPath(const QString& iconName)
{
    return QStringLiteral(":/Interaction/") + iconName + QStringLiteral(".svg");
}

QIcon makeTintedIcon(const QIcon& base, const QColor& tint, const QSize& size)
{
    if (!base.isNull()) {
        QPixmap px = base.pixmap(size);
        if (!px.isNull()) {
            QImage img = px.toImage().convertToFormat(QImage::Format_ARGB32);
            QPainter p(&img);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(img.rect(), tint);
            p.end();
            return QIcon(QPixmap::fromImage(img));
        }
    }
    return base;
}
}

QWidget* CosmicBottomToolsBar::contentWidget() const
{
    if (!mContentLayout) {
        return nullptr;
    }
    if (mContentLayout->count() <= 0) {
        return nullptr;
    }
    QLayoutItem* it = mContentLayout->itemAt(0);
    return it ? it->widget() : nullptr;
}

void CosmicBottomToolsBar::setContentWidget(QWidget* w)
{
    if (!mContentLayout) {
        return;
    }

    if (QWidget* existing = contentWidget()) {
        mContentLayout->removeWidget(existing);
        existing->setParent(nullptr);
    }

    if (!w) {
        return;
    }

    w->setParent(mContentHost);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mContentLayout->addWidget(w);
    if (mCollapsed) {
        w->hide();
    } else {
        w->show();
    }
    enforceEndCapsOnTop();
    updateResizeGripGeometry();
    updateGeometry();
}

int CosmicBottomToolsBar::preferredHeight() const
{
    return mPreferredHeight;
}

void CosmicBottomToolsBar::setPreferredHeight(int h)
{
    const int minH = 56;
    const int maxH = 900;
    const int next = qBound(minH, h, maxH);
    if (mPreferredHeight == next) {
        return;
    }
    mPreferredHeight = next;
    saveSurvivalSettings();
    emit preferredHeightChanged(mPreferredHeight);

    if (!mCollapsed) {
        setFixedHeight(mPreferredHeight);
        updateResizeGripGeometry();
    }
}

bool CosmicBottomToolsBar::isCollapsed() const
{
    return mCollapsed;
}

CosmicBottomToolsBar::CosmicBottomToolsBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    setMouseTracking(true);
    mResizeGrip = new QWidget(this);
    mResizeGrip->setObjectName(QStringLiteral("CosmicBottomToolsBarResizeGrip"));
    mResizeGrip->setCursor(Qt::SplitVCursor);
    mResizeGrip->setAttribute(Qt::WA_NoSystemBackground, true);
    mResizeGrip->setAutoFillBackground(false);
    mResizeGrip->installEventFilter(this);
    mResizeGrip->raise();

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    const QSize iconSize(22, 22);

    const int endCapW = 28;

    mLeftCap = new QWidget(this);
    mLeftCap->setObjectName(QStringLiteral("BottomLeftCap"));
    mLeftCap->setFixedWidth(endCapW);
    mLeftCap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* leftCapLayout = new QVBoxLayout(mLeftCap);
    leftCapLayout->setContentsMargins(0, 0, 0, 0);
    leftCapLayout->setSpacing(0);
    leftCapLayout->addStretch(1);

    mLockToggle = new CosmicCheckboxToggle(mLeftCap);
    mLockToggle->setObjectName(QStringLiteral("BottomLeftToggle"));
    mLockToggle->setMode(CosmicCheckboxToggle::Custom);
    mLockToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mLockToggle->setBordered(false);
    mLockToggle->setShowText(false);
    mLockToggle->setShowIcon(true);
    mLockToggle->setIconSize(iconSize);
    mLockToggle->setBackgroundEnabled(false);
    mLockToggle->setCheckable(true);
    mLockToggle->setFocusPolicy(Qt::NoFocus);
    mLockToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mLockToggle->setMinimumSize(endCapW, endCapW);
    mLockToggle->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    mLockToggle->installEventFilter(this);

    leftCapLayout->addWidget(mLockToggle, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    leftCapLayout->addStretch(1);
    mLeftCap->setLayout(leftCapLayout);

    mRightCap = new QWidget(this);
    mRightCap->setObjectName(QStringLiteral("BottomRightCap"));
    mRightCap->setFixedWidth(endCapW);
    mRightCap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* rightCapLayout = new QVBoxLayout(mRightCap);
    rightCapLayout->setContentsMargins(0, 0, 0, 0);
    rightCapLayout->setSpacing(0);
    rightCapLayout->addStretch(1);

    mUnlockToggle = new CosmicCheckboxToggle(mRightCap);
    mUnlockToggle->setObjectName(QStringLiteral("BottomRightToggle"));
    mUnlockToggle->setMode(CosmicCheckboxToggle::Custom);
    mUnlockToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mUnlockToggle->setBordered(false);
    mUnlockToggle->setShowText(false);
    mUnlockToggle->setShowIcon(true);
    mUnlockToggle->setIconSize(iconSize);
    mUnlockToggle->setBackgroundEnabled(false);
    mUnlockToggle->setCheckable(true);
    mUnlockToggle->setFocusPolicy(Qt::NoFocus);
    mUnlockToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mUnlockToggle->setMinimumSize(endCapW, endCapW);
    mUnlockToggle->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    mUnlockToggle->installEventFilter(this);

    rightCapLayout->addWidget(mUnlockToggle, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    rightCapLayout->addStretch(1);
    mRightCap->setLayout(rightCapLayout);

    mContentHost = new QWidget(this);
    mContentHost->setObjectName(QStringLiteral("BottomContentHost"));
    mContentHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mContentHost->setAttribute(Qt::WA_StyledBackground, false);
    mContentLayout = new QVBoxLayout(mContentHost);
    mContentLayout->setContentsMargins(0, 0, 0, 0);
    mContentLayout->setSpacing(0);

    root->addWidget(mLeftCap);
    root->addWidget(mContentHost, 1);
    root->addWidget(mRightCap);

    setLayout(root);

    mAutoHideTimer = new QTimer(this);
    mAutoHideTimer->setSingleShot(true);
    mAutoHideTimer->setInterval(150);
    connect(mAutoHideTimer, &QTimer::timeout, this, [this]() {
        if (mAutoHideEnabled && !rect().contains(mapFromGlobal(QCursor::pos()))) {
            hideContent();
        }
    });

    loadSurvivalSettings();
    updateResizeGripGeometry();

    // NOTE: we drive toggling via eventFilter to ensure both ends respond identically.

    connect(this, &CosmicBottomToolsBar::lockEnabledChanged, this, [this](bool locked) {
        mAutoHideEnabled = !locked;
        applyAutoHideState(true);
        saveSurvivalSettings();
    });

    applyAutoHideState(true);

    enforceEndCapsOnTop();
}

void CosmicBottomToolsBar::toggleLock()
{
    toggleLockState();
}

void CosmicBottomToolsBar::toggleLockState()
{
    const bool next = !lockEnabled();
    setLockEnabled(next);
    emit lockEnabledChanged(next);
}

void CosmicBottomToolsBar::enforceEndCapsOnTop()
{
    // Ensure hit-testing/z-order: caps/toggles must stay above hosted content (timeline).
    if (mContentHost) mContentHost->lower();
    if (mLeftCap) mLeftCap->raise();
    if (mRightCap) mRightCap->raise();
    if (mLockToggle) mLockToggle->raise();
    if (mUnlockToggle) mUnlockToggle->raise();
    if (mResizeGrip) mResizeGrip->raise();
}

QColor CosmicBottomToolsBar::pillDisabledIconColor()
{
    return QColor(0x50, 0x50, 0x50, 0xff);
}

bool CosmicBottomToolsBar::lockEnabled() const
{
    return mLockToggle && mLockToggle->isChecked();
}

void CosmicBottomToolsBar::setLockEnabled(bool enabled)
{
    if (!mLockToggle || !mUnlockToggle) {
        return;
    }

    {
        QSignalBlocker b1(mLockToggle);
        QSignalBlocker b2(mUnlockToggle);
        mLockToggle->setChecked(enabled);
        mUnlockToggle->setChecked(enabled);
    }

    updateToggleIcons();
}

void CosmicBottomToolsBar::loadSurvivalSettings()
{
    QSettings s(PENCIL2D, PENCIL2D);
    const bool lockOn = s.value(QStringLiteral("CosmicBottomToolsBar/LockEnabled"), false).toBool();
    const int h = s.value(QStringLiteral("CosmicBottomToolsBar/Height"), mPreferredHeight).toInt();

    mPreferredHeight = qBound(56, h, 900);
    setLockEnabled(lockOn);

    mAutoHideEnabled = !lockOn;

    if (mAutoHideEnabled) {
        mCollapsed = true;
        setFixedHeight(28);
        if (QWidget* w = contentWidget()) {
            w->hide();
        }
    } else {
        mCollapsed = false;
        setFixedHeight(mPreferredHeight);
        if (QWidget* w = contentWidget()) {
            w->show();
        }
    }

    updateResizeGripGeometry();
    enforceEndCapsOnTop();
}

void CosmicBottomToolsBar::saveSurvivalSettings() const
{
    QSettings s(PENCIL2D, PENCIL2D);
    s.setValue(QStringLiteral("CosmicBottomToolsBar/LockEnabled"), lockEnabled());
    s.setValue(QStringLiteral("CosmicBottomToolsBar/Height"), mPreferredHeight);
    s.sync();
}

void CosmicBottomToolsBar::applyAutoHideState(bool force)
{
    Q_UNUSED(force);
    if (!mAutoHideEnabled) {
        showContent();
        return;
    }

    if (rect().contains(mapFromGlobal(QCursor::pos()))) {
        showContent();
    } else {
        hideContent();
    }
}

void CosmicBottomToolsBar::showContent()
{
    if (!mCollapsed) {
        return;
    }
    mCollapsed = false;
    if (QWidget* w = contentWidget()) {
        w->show();
    }
    setFixedHeight(mPreferredHeight);
    enforceEndCapsOnTop();
    updateResizeGripGeometry();
    updateGeometry();
    update();
    emit preferredHeightChanged(mPreferredHeight);
}

void CosmicBottomToolsBar::hideContent()
{
    if (mCollapsed) {
        return;
    }
    mCollapsed = true;
    // Keep the host in the layout so end caps stay at the same X positions.
    if (QWidget* w = contentWidget()) {
        w->hide();
    }
    const int collapsedH = 28;
    setFixedHeight(collapsedH);
    enforceEndCapsOnTop();
    updateResizeGripGeometry();
    updateGeometry();
    update();
    emit preferredHeightChanged(collapsedH);
}

void CosmicBottomToolsBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateResizeGripGeometry();
    enforceEndCapsOnTop();
}

void CosmicBottomToolsBar::enterEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (mAutoHideEnabled) {
        showContent();
        if (mAutoHideTimer) {
            mAutoHideTimer->stop();
        }
    }
}

void CosmicBottomToolsBar::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (mAutoHideEnabled && mAutoHideTimer) {
        mAutoHideTimer->start();
    }
}

bool CosmicBottomToolsBar::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == mLockToggle || watched == mUnlockToggle) && event) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // Prevent auto-hide from racing the click and ensure the bar stays stable.
                if (mAutoHideTimer) {
                    mAutoHideTimer->stop();
                }
                if (mAutoHideEnabled) {
                    showContent();
                }
                toggleLockState();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick) {
            // Consume to avoid double-toggle from internal QAbstractButton logic.
            return true;
        }
        return false;
    }

    if (watched == mResizeGrip) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                beginResize(me->globalPos());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (mIsResizing) {
                auto* me = static_cast<QMouseEvent*>(event);
                updateResize(me->globalPos());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (mIsResizing) {
                endResize();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CosmicBottomToolsBar::updateResizeGripGeometry()
{
    if (!mResizeGrip) {
        return;
    }
    const int gripH = 6;
    mResizeGrip->setGeometry(0, 0, width(), gripH);
    mResizeGrip->setVisible(!mCollapsed);
    mResizeGrip->raise();
}

void CosmicBottomToolsBar::beginResize(const QPoint& globalPos)
{
    mIsResizing = true;
    mDragStartHeight = height();
    mDragStartGlobalY = globalPos.y();
}

void CosmicBottomToolsBar::updateResize(const QPoint& globalPos)
{
    if (!mIsResizing) {
        return;
    }
    const int dy = mDragStartGlobalY - globalPos.y();
    setPreferredHeight(mDragStartHeight + dy);
}

void CosmicBottomToolsBar::endResize()
{
    mIsResizing = false;
}

void CosmicBottomToolsBar::updateToggleIcons()
{
    if (!mLockToggle || !mUnlockToggle) {
        return;
    }

    const QSize sz = mLockToggle->iconSize();

    const bool locked = lockEnabled();
    const QColor accentColor(CosmicColor::accent());

    const QIcon base(interactionIconPath(locked ? QStringLiteral("LockFloatingState")
                                              : QStringLiteral("UnlockFloatingState")));
    const QColor tint = locked ? accentColor : pillDisabledIconColor();

    const QIcon themed = makeTintedIcon(base, tint, sz);
    mLockToggle->setIcon(themed);
    mUnlockToggle->setIcon(themed);

    mLockToggle->update();
    mUnlockToggle->update();
}

void CosmicBottomToolsBar::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor fill(0x1f, 0x1f, 0x1f, 0xff);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);

    const QRectF rc = rect();
    const qreal radius = qMin<qreal>(rc.height() / 2.0, 14.0);

    QPainterPath path;
    const qreal x0 = rc.left();
    const qreal y0 = rc.top();
    const qreal x1 = rc.right();
    const qreal y1 = rc.bottom();

    // Square bottom corners (flush to window edge), rounded top corners.
    path.moveTo(x0, y1);
    path.lineTo(x0, y0 + radius);
    path.quadTo(x0, y0, x0 + radius, y0);
    path.lineTo(x1 - radius, y0);
    path.quadTo(x1, y0, x1, y0 + radius);
    path.lineTo(x1, y1);
    path.closeSubpath();

    p.drawPath(path);
}
