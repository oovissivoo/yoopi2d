#include "cosmicwindowheader.h"

#include "pencildef.h"

#include "../CUI/cosmicbutton.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QSettings>
#include <QStyle>
#include <QStyleOptionTitleBar>

namespace {
bool systemCloseButtonOnLeft()
{
    QStyleOptionTitleBar opt;
    opt.rect = QRect(0, 0, 200, 40);
    const QRect r = QApplication::style()->subControlRect(
        QStyle::CC_TitleBar, &opt, QStyle::SC_TitleBarCloseButton, nullptr);
    if (!r.isValid()) return true;
    return r.center().x() < opt.rect.center().x();
}
} // namespace

CosmicWindowHeader::CosmicWindowHeader(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(28);
    setStyleSheet("QWidget { background: transparent; }");

    QSettings s(PENCIL2D, PENCIL2D);
    mCloseStyle  = s.value(QStringLiteral("WindowCloseStyle"), 0).toInt();
    mCloseOnLeft = s.contains(QStringLiteral("WindowCloseOnLeft"))
                   ? s.value(QStringLiteral("WindowCloseOnLeft"), true).toBool()
                   : systemCloseButtonOnLeft();

    mCloseBtn = new CosmicButton(this);
    mCloseBtn->setBordered(false);
    mCloseBtn->setFixedSize(28, 28);
    mCloseBtn->setIconSize(QSize(16, 16));
    mCloseBtn->setToolTip(tr("Close"));
    updateIcon();

    connect(mCloseBtn, &QPushButton::clicked, this, &CosmicWindowHeader::closeRequested);
    rebuildLayout();
}

void CosmicWindowHeader::setCloseOnLeft(bool on)
{
    mCloseOnLeft = on;
    rebuildLayout();
}

void CosmicWindowHeader::setCloseStyle(int style)
{
    mCloseStyle = style;
    updateIcon();
}

void CosmicWindowHeader::updateIcon()
{
    if (!mCloseBtn) return;
    // style 1 = mac red circle button; 0 = default X icon
    const QString path = (mCloseStyle == 1)
        ? QStringLiteral(":/Windows&AppIcons/MacCloseButton.svg")
        : QStringLiteral(":/Interaction/CloseWindow.svg");
    mCloseBtn->setIcon(QIcon(path));
}

// static — force-applies to every live CosmicWindowHeader in the process,
// including those in separate top-level windows.
void CosmicWindowHeader::applyStyleToAll(int style, bool closeOnLeft)
{
    // Walk every top-level widget and recurse into all children.
    // qApp->findChildren only finds children of QApplication itself,
    // not widgets inside separate top-level windows.
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* top : tops) {
        const QList<CosmicWindowHeader*> headers =
            top->findChildren<CosmicWindowHeader*>();
        for (CosmicWindowHeader* h : headers) {
            h->setCloseStyle(style);
            h->setCloseOnLeft(closeOnLeft);
        }
        // Also check if the top-level itself is a CosmicWindowHeader (unlikely but safe)
        if (auto* h = qobject_cast<CosmicWindowHeader*>(top)) {
            h->setCloseStyle(style);
            h->setCloseOnLeft(closeOnLeft);
        }
    }
}

void CosmicWindowHeader::rebuildLayout()
{
    delete layout();

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    if (mCloseOnLeft) {
        lay->addWidget(mCloseBtn);
        lay->setAlignment(mCloseBtn, Qt::AlignLeft | Qt::AlignTop);
        lay->addStretch(1);
    } else {
        lay->addStretch(0);
        lay->addWidget(mCloseBtn);
        lay->setAlignment(mCloseBtn, Qt::AlignRight | Qt::AlignTop);
    }

    setLayout(lay);
}
