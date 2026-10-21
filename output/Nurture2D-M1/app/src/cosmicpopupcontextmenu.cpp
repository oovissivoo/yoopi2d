#include "cosmicpopupcontextmenu.h"

#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicuimetrics.h"
#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace {
static QColor cosmicPanel() { return QColor(0x2f, 0x2f, 0x2f); }
static QColor cosmicBorder() { return CosmicColor::popupBorder(); }

static QPointer<CosmicPopupContextMenu> gActiveContextMenu;
}

CosmicPopupContextMenu::CosmicPopupContextMenu(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Behave like a real popup menu: take focus, and auto-dismiss when the user interacts elsewhere.
    setWindowFlags(windowFlags() | Qt::Popup);

    setModalMode(ModalMode::NonModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setShadowEnabled(false);
    setShadowPadding(0);

    setPanelColor(cosmicPanel());
    setBorderColor(cosmicBorder());
    setBorderWidth(2);
    setPanelRadius(CosmicUiMetrics::windowCornerRadius());

    buildUi();
}

CosmicPopupContextMenu::~CosmicPopupContextMenu()
{
    if (gActiveContextMenu == this) {
        gActiveContextMenu.clear();
    }
}

void CosmicPopupContextMenu::buildUi()
{
    mRoot = new QWidget(this);
    mRoot->setObjectName(QStringLiteral("CosmicPopupContextMenuRoot"));
    mRoot->setMinimumWidth(200);

    mLayout = new QVBoxLayout(mRoot);
    mLayout->setContentsMargins(4, 4, 4, 4);
    mLayout->setSpacing(1);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(mRoot);
}

void CosmicPopupContextMenu::addItem(const QString& text, const QString& id, bool enabled, const QIcon& icon)
{
    if (!mLayout) {
        return;
    }

    CosmicButton* b = new CosmicButton(text, mRoot);
    b->setBordered(false);
    b->setFixedHeight(26);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setEnabled(enabled);
    b->setTextAlignment(Qt::AlignLeft);
    // Inner radius = outer popup radius − layout inset (4px), per macOS nested rounding rule
    b->setCornerRadius(CosmicUiMetrics::nestedCornerRadius(CosmicUiMetrics::windowCornerRadius(), 4));

    if (!icon.isNull()) {
        b->setIcon(icon);
        b->setIconSize(QSize(16, 16));
    }

    b->setStyleSheet(QString(
        "QPushButton { border: none; padding: 0px; background-color: transparent; }"
    ));

    connect(b, &QPushButton::clicked, this, [this, id]() {
        if (!sender()) {
            return;
        }
        if (auto* w = qobject_cast<QWidget*>(sender())) {
            if (!w->isEnabled()) {
                return;
            }
        }
        mChosenId = id;
        reject();
    });

    mLayout->addWidget(b);
}

void CosmicPopupContextMenu::addCheckableItem(const QString& text, const QString& id, bool checked, bool enabled)
{
    if (!mLayout) return;

    // Render as a plain button with a leading check indicator — same look as
    // addItem, no legacy widgets. "✓ Text" when checked, "  Text" when not.
    const QString label = checked
        ? QStringLiteral("✓  ") + text
        : QStringLiteral("    ") + text;

    CosmicButton* b = new CosmicButton(label, mRoot);
    b->setBordered(false);
    b->setFixedHeight(26);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setEnabled(enabled);
    b->setTextAlignment(Qt::AlignLeft);
    b->setCornerRadius(CosmicUiMetrics::nestedCornerRadius(CosmicUiMetrics::windowCornerRadius(), 4));
    if (checked) b->setTextColor(QColor(0xff, 0xff, 0xff));

    b->setStyleSheet(QStringLiteral(
        "QPushButton { border: none; padding: 0px; background-color: transparent; }"
    ));

    connect(b, &QPushButton::clicked, this, [this, id]() {
        mChosenId = id;
        reject();
    });

    mLayout->addWidget(b);
}

void CosmicPopupContextMenu::addSeparator()
{
    if (!mLayout) {
        return;
    }

    QFrame* sep = new QFrame(mRoot);
    sep->setFixedHeight(1);
    sep->setContentsMargins(0, 2, 0, 2);
    sep->setStyleSheet(QStringLiteral("QFrame{ background:#3a3a3a; border: none; }"));
    mLayout->addWidget(sep);
}

QString CosmicPopupContextMenu::execAt(const QPoint& globalPos)
{
    if (gActiveContextMenu && gActiveContextMenu != this) {
        gActiveContextMenu->reject();
    }
    gActiveContextMenu = this;

    mChosenId.clear();

    // Global filter to dismiss on outside interaction (click elsewhere, etc.).
    qApp->installEventFilter(this);

    adjustSize();
    showAt(globalPos, Anchor::TopLeft, true);
    show();

    QEventLoop loop;
    connect(this, &QDialog::finished, &loop, &QEventLoop::quit);
    loop.exec();

    qApp->removeEventFilter(this);

    if (gActiveContextMenu == this) {
        gActiveContextMenu.clear();
    }

    return mChosenId;
}

bool CosmicPopupContextMenu::event(QEvent* e)
{
    if (e) {
        if (e->type() == QEvent::WindowDeactivate || e->type() == QEvent::FocusOut) {
            reject();
            return true;
        }
    }
    return CosmicPopupWindow::event(e);
}

bool CosmicPopupContextMenu::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return CosmicPopupWindow::eventFilter(watched, event);
    }

    // If the user clicks somewhere else, close the menu.
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me) {
            const QPoint local = mapFromGlobal(me->globalPos());
            if (!rect().contains(local)) {
                reject();
                return false;
            }
        }
    }

    // Dismiss on wheel scroll outside the menu.
    if (event->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(event);
        if (we) {
            const QPoint local = mapFromGlobal(we->globalPos());
            if (!rect().contains(local)) {
                reject();
                return false;
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke && ke->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
    }

    return CosmicPopupWindow::eventFilter(watched, event);
}

void CosmicPopupContextMenu::leaveEvent(QEvent* e)
{
    CosmicPopupWindow::leaveEvent(e);
    // Do not auto-dismiss on mouse leave — the user may be moving to a submenu
    // item or just briefly leaving the bounds. Dismiss only on outside click or Escape.
}

void CosmicPopupContextMenu::keyPressEvent(QKeyEvent* e)
{
    if (e && e->key() == Qt::Key_Escape) {
        e->accept();
        reject();
        return;
    }

    CosmicPopupWindow::keyPressEvent(e);
}
