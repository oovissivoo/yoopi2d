#include "cosmicrighttoolsbar.h"

#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicuimetrics.h"

#include <QPainter>
#include <QPainterPath>
#include <QBoxLayout>
#include <QSizePolicy>
#include <QSignalBlocker>

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

CosmicRightToolsBar::CosmicRightToolsBar(QWidget* parent)
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
    mTabsLayout = tabsLayout;

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
    mTabLock->setChecked(false);
    updateTabIcon(mTabLock, QStringLiteral("UnlockFloatingState"), false);

    ensureToggle(QStringLiteral("Color Wheel"), QStringLiteral("TabColorWheel"));
    ensureToggle(QStringLiteral("Palettes"), QStringLiteral("TabPallets"));
    ensureToggle(QStringLiteral("Reference"), QStringLiteral("TabRefferenceWindow"));
    ensureToggle(QStringLiteral("Color Sliders"), QStringLiteral("rgbSliders"));
    setPaneOrder(QStringList() << QStringLiteral("Color Wheel") << QStringLiteral("Palettes") << QStringLiteral("Reference") << QStringLiteral("Color Sliders"));

    connect(mTabLock, &QAbstractButton::toggled, this, [this](bool on) {
        if (on) {
            const QIcon base(interactionIconPath(QStringLiteral("LockFloatingState")));
            const QColor accentColor(CosmicColor::accent());
            mTabLock->setIcon(makeTintedIcon(base, accentColor, mTabLock->iconSize()));
        } else {
            updateTabIcon(mTabLock, QStringLiteral("UnlockFloatingState"), false);
        }
        emit lockEnabledChanged(on);
    });

    connect(this, &CosmicRightToolsBar::paneEnabledChanged, this, [this](const QString& paneName, bool on) {
        if (paneName == QStringLiteral("Color Wheel")) {
            emit colorWheelPaneEnabledChanged(on);
        } else if (paneName == QStringLiteral("Palettes")) {
            emit palettesPaneEnabledChanged(on);
        } else if (paneName == QStringLiteral("Reference")) {
            emit referencePaneEnabledChanged(on);
        } else if (paneName == QStringLiteral("Color Sliders")) {
            emit colorSlidersPaneEnabledChanged(on);
        }
    });
}

bool CosmicRightToolsBar::lockEnabled() const
{
    return mTabLock && mTabLock->isChecked();
}

bool CosmicRightToolsBar::colorWheelPaneEnabled() const { return paneEnabled(QStringLiteral("Color Wheel")); }
bool CosmicRightToolsBar::palettesPaneEnabled() const { return paneEnabled(QStringLiteral("Palettes")); }
bool CosmicRightToolsBar::referencePaneEnabled() const { return paneEnabled(QStringLiteral("Reference")); }
bool CosmicRightToolsBar::colorSlidersPaneEnabled() const { return paneEnabled(QStringLiteral("Color Sliders")); }

void CosmicRightToolsBar::setLockEnabled(bool enabled)
{
    if (!mTabLock) {
        return;
    }
    if (mTabLock->isChecked() == enabled) {
        return;
    }
    mTabLock->setChecked(enabled);
}

void CosmicRightToolsBar::setColorWheelPaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Color Wheel"), enabled); }
void CosmicRightToolsBar::setPalettesPaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Palettes"), enabled); }
void CosmicRightToolsBar::setReferencePaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Reference"), enabled); }
void CosmicRightToolsBar::setColorSlidersPaneEnabled(bool enabled) { setPaneEnabled(QStringLiteral("Color Sliders"), enabled); }

CosmicCheckboxToggle* CosmicRightToolsBar::ensureToggle(const QString& paneName, const QString& iconName)
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

    const int insertIdx = qMax(1, mTabsLayout->count() - 1);
    mTabsLayout->insertWidget(insertIdx, t, 0, Qt::AlignHCenter | Qt::AlignTop);
    mTabsLayout->invalidate();
    mTabsLayout->activate();

    if (!mPaneOrder.contains(paneName)) {
        mPaneOrder.append(paneName);
    }
    return t;
}

void CosmicRightToolsBar::removeToggle(const QString& paneName)
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

bool CosmicRightToolsBar::paneEnabled(const QString& paneName) const
{
    CosmicCheckboxToggle* t = mPaneToggles.value(paneName, nullptr);
    return t && t->isChecked();
}

void CosmicRightToolsBar::setPaneEnabled(const QString& paneName, bool enabled)
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

void CosmicRightToolsBar::setPaneOrder(const QStringList& paneOrder)
{
    if (!mTabsLayout) {
        return;
    }

    QLayoutItem* stretchItem = nullptr;
    if (mTabsLayout->count() > 0) {
        QLayoutItem* lastItem = mTabsLayout->itemAt(mTabsLayout->count() - 1);
        if (lastItem && lastItem->spacerItem()) {
            stretchItem = mTabsLayout->takeAt(mTabsLayout->count() - 1);
        }
    }

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

QStringList CosmicRightToolsBar::paneOrder() const
{
    return mPaneOrder;
}

QColor CosmicRightToolsBar::pillDisabledIconColor()
{
    return QColor(0x50, 0x50, 0x50, 0xff);
}

void CosmicRightToolsBar::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor fill(0x1f, 0x1f, 0x1f, 0xff);
    p.fillRect(rect(), fill);
}

void CosmicRightToolsBar::updateToggleOrder(const QStringList& paneOrder)
{
    setPaneOrder(paneOrder);
}

void CosmicRightToolsBar::updateTabIcon(CosmicCheckboxToggle* toggle, const QString& iconName, bool checked)
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
