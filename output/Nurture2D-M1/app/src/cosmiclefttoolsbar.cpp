#include "cosmiclefttoolsbar.h"

#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include <QPainter>
#include <QPainterPath>
#include <QBoxLayout>
#include <QSizePolicy>

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

CosmicLeftToolsBar::CosmicLeftToolsBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* tabsContainer = new QWidget(this);
    tabsContainer->setAttribute(Qt::WA_NoSystemBackground, true);
    tabsContainer->setAutoFillBackground(false);

    auto* tabsLayout = new QVBoxLayout(tabsContainer);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(12);
    mTabsLayout = tabsLayout; // Store reference for reordering

    const QSize iconSize(22, 22);

    // Lock toggle (separate system, always first)
    mTabLock = new CosmicCheckboxToggle(tabsContainer);
    mTabLock->setObjectName(QStringLiteral("TabLock"));
    mTabLock->setMode(CosmicCheckboxToggle::Custom);
    mTabLock->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mTabLock->setBordered(false);
    mTabLock->setShowText(false);
    mTabLock->setShowIcon(true);
    mTabLock->setIconSize(iconSize);
    mTabLock->setBackgroundEnabled(false);
    mTabLock->setCheckable(true);
    mTabLock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    tabsLayout->addWidget(mTabLock, 0, Qt::AlignHCenter | Qt::AlignTop);
    tabsLayout->addStretch(1);

    tabsContainer->setLayout(tabsLayout);
    root->addWidget(tabsContainer);
    setLayout(root);

    // Initialize icons and checked state.
    mTabLock->setChecked(false); // Default to unlocked
    updateTabIcon(mTabLock, QStringLiteral("UnlockFloatingState"), false);

    ensureToggle(QStringLiteral("Tools"), QStringLiteral("TabTools"));
    ensureToggle(QStringLiteral("Tools Options"), QStringLiteral("TabToolsOptions"));
    ensureToggle(QStringLiteral("Brushes"), QStringLiteral("TabBrushPresets"));
    ensureToggle(QStringLiteral("Layers"), QStringLiteral("TabPapers"));

    setPaneOrder(QStringList() << QStringLiteral("Tools") << QStringLiteral("Tools Options") << QStringLiteral("Brushes") << QStringLiteral("Layers"));

    connect(mTabLock, &QAbstractButton::toggled, this, [this](bool on) {
        if (on) {
            // Use blue-violet accent color for locked state
            const QIcon base(interactionIconPath(QStringLiteral("LockFloatingState")));
            const QColor accentColor(CosmicColor::accent());
            mTabLock->setIcon(makeTintedIcon(base, accentColor, mTabLock->iconSize()));
        } else {
            // Use normal disabled color for unlocked state
            updateTabIcon(mTabLock, QStringLiteral("UnlockFloatingState"), false);
        }
        emit lockEnabledChanged(on);
    });

    connect(this, &CosmicLeftToolsBar::paneEnabledChanged, this, [this](const QString& paneName, bool on) {
        if (paneName == QStringLiteral("Tools")) {
            emit toolsPaneEnabledChanged(on);
        } else if (paneName == QStringLiteral("Tools Options")) {
            emit toolsOptionsPaneEnabledChanged(on);
        } else if (paneName == QStringLiteral("Brushes")) {
            emit brushesPaneEnabledChanged(on);
        }
    });
}

bool CosmicLeftToolsBar::lockEnabled() const
{
    return mTabLock && mTabLock->isChecked();
}

bool CosmicLeftToolsBar::toolsPaneEnabled() const { return paneEnabled(QStringLiteral("Tools")); }
bool CosmicLeftToolsBar::toolsOptionsPaneEnabled() const { return paneEnabled(QStringLiteral("Tools Options")); }
bool CosmicLeftToolsBar::brushesPaneEnabled() const { return paneEnabled(QStringLiteral("Brushes")); }

void CosmicLeftToolsBar::setLockEnabled(bool enabled)
{
    if (!mTabLock) {
        return;
    }
    if (mTabLock->isChecked() == enabled) {
        return;
    }
    mTabLock->setChecked(enabled);
}

void CosmicLeftToolsBar::setToolsPaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Tools"), enabled); }

void CosmicLeftToolsBar::setToolsOptionsPaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Tools Options"), enabled); }

void CosmicLeftToolsBar::setBrushesPaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Brushes"), enabled); }

QColor CosmicLeftToolsBar::pillDisabledIconColor()
{
    return QColor(0x50, 0x50, 0x50, 0xff);
}

void CosmicLeftToolsBar::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor fill(0x1f, 0x1f, 0x1f, 0xff);
    p.fillRect(rect(), fill);
}

CosmicCheckboxToggle* CosmicLeftToolsBar::ensureToggle(const QString& paneName, const QString& iconName)
{
    if (!mTabsLayout) {
        return nullptr;
    }
    if (paneName.isEmpty()) {
        return nullptr;
    }
    if (CosmicCheckboxToggle* existing = mPaneToggles.value(paneName, nullptr)) {
        return existing;
    }

    const QSize iconSize(22, 22);

    auto* t = new CosmicCheckboxToggle(this);
    t->setObjectName(paneName);
    t->setMode(CosmicCheckboxToggle::Custom);
    t->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    t->setBordered(false);
    t->setShowText(false);
    t->setShowIcon(true);
    t->setIconSize(iconSize);
    t->setBackgroundEnabled(false);
    t->setCheckable(true);
    t->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    t->setChecked(true);
    updateTabIcon(t, iconName, true);

    connect(t, &QAbstractButton::toggled, this, [this, paneName, iconName, t](bool on) {
        if (!on) {
            int enabledCount = 0;
            for (auto it = mPaneToggles.constBegin(); it != mPaneToggles.constEnd(); ++it) {
                CosmicCheckboxToggle* w = it.value();
                if (w && w->isChecked()) {
                    ++enabledCount;
                }
            }
            if (enabledCount <= 0) {
                QSignalBlocker b(t);
                t->setChecked(true);
                updateTabIcon(t, iconName, true);
                return;
            }
        }

        updateTabIcon(t, iconName, on);
        emit paneEnabledChanged(paneName, on);
    });

    mPaneToggles.insert(paneName, t);

    // Default insert before stretch, after lock.
    const int insertIdx = qMax(1, mTabsLayout->count() - 1);
    mTabsLayout->insertWidget(insertIdx, t, 0, Qt::AlignHCenter | Qt::AlignTop);
    mTabsLayout->invalidate();
    mTabsLayout->activate();

    if (!mPaneOrder.contains(paneName)) {
        mPaneOrder.append(paneName);
    }
    return t;
}

void CosmicLeftToolsBar::removeToggle(const QString& paneName)
{
    CosmicCheckboxToggle* t = mPaneToggles.take(paneName);
    if (!t) {
        return;
    }
    if (mTabsLayout) {
        mTabsLayout->removeWidget(t);
    }
    t->deleteLater();
    mPaneOrder.removeAll(paneName);
}

bool CosmicLeftToolsBar::paneEnabled(const QString& paneName) const
{
    CosmicCheckboxToggle* t = mPaneToggles.value(paneName, nullptr);
    return t && t->isChecked();
}

void CosmicLeftToolsBar::setPaneEnabled(const QString& paneName, bool enabled)
{
    CosmicCheckboxToggle* t = mPaneToggles.value(paneName, nullptr);
    if (!t) {
        return;
    }
    if (t->isChecked() == enabled) {
        return;
    }
    t->setChecked(enabled);
}

void CosmicLeftToolsBar::setPaneOrder(const QStringList& paneOrder)
{
    if (!mTabsLayout) {
        return;
    }

    // Remove stretch item temporarily.
    QLayoutItem* stretchItem = nullptr;
    if (mTabsLayout->count() > 0) {
        QLayoutItem* lastItem = mTabsLayout->itemAt(mTabsLayout->count() - 1);
        if (lastItem && lastItem->spacerItem()) {
            stretchItem = mTabsLayout->takeAt(mTabsLayout->count() - 1);
        }
    }

    // Remove all known pane toggles.
    for (auto it = mPaneToggles.constBegin(); it != mPaneToggles.constEnd(); ++it) {
        if (it.value()) {
            mTabsLayout->removeWidget(it.value());
        }
    }

    mPaneOrder.clear();
    for (const QString& name : paneOrder) {
        CosmicCheckboxToggle* t = mPaneToggles.value(name, nullptr);
        if (!t) {
            continue;
        }
        mPaneOrder.append(name);
        mTabsLayout->insertWidget(mTabsLayout->count(), t, 0, Qt::AlignHCenter | Qt::AlignTop);
    }

    // Append any toggles not listed, preserving existing order.
    for (auto it = mPaneToggles.constBegin(); it != mPaneToggles.constEnd(); ++it) {
        const QString name = it.key();
        CosmicCheckboxToggle* t = it.value();
        if (!t || mPaneOrder.contains(name)) {
            continue;
        }
        mPaneOrder.append(name);
        mTabsLayout->insertWidget(mTabsLayout->count(), t, 0, Qt::AlignHCenter | Qt::AlignTop);
    }

    if (stretchItem) {
        mTabsLayout->addItem(stretchItem);
    } else {
        mTabsLayout->addStretch(1);
    }

    mTabsLayout->invalidate();
    mTabsLayout->activate();
}

QStringList CosmicLeftToolsBar::paneOrder() const
{
    return mPaneOrder;
}

void CosmicLeftToolsBar::updateToggleOrder(const QStringList& paneOrder)
{
    setPaneOrder(paneOrder);
}

void CosmicLeftToolsBar::updateTabIcon(CosmicCheckboxToggle* toggle, const QString& iconName, bool checked)
{
    if (!toggle) {
        return;
    }

    const QIcon base(interactionIconPath(iconName));
    if (checked) {
        toggle->setIcon(base);
        toggle->update();
        return;
    }

    const QColor dim = pillDisabledIconColor();
    toggle->setIcon(makeTintedIcon(base, dim, toggle->iconSize()));
    toggle->update();
}
