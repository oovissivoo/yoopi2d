#include "cosmicdropdown.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QTimer>
#include <QPointer>
#include <QListWidget>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QCloseEvent>
#include <QPixmap>
#include <QSettings>
#include <QScrollBar>
#include <QMenu>
#include <QAction>

#include "preferencesdef.h"
#include "pencildef.h"
#include "cosmicuimetrics.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

namespace {
static const int kHorizontalPadding = 10;   // left-right padding for text
static const int kArrowSize = 16;           // px, slightly larger for cosmic UI
static const QColor kCosmicBackground(0x1f, 0x1f, 0x1f);
static const QColor kCosmicText(0xb3, 0xb3, 0xb3); // global dock text color b3b3b3ff
static const QColor kCosmicHighlight(0x40, 0x55, 0x5c);
static const QColor kDropDownBase(0x50, 0x50, 0x50);
static const QColor kCosmicBlueViolet = CosmicColor::accent();
}

// Angular highlight delegate for the popup list (no rounded corners)
class CosmicRoundedHighlightDelegate : public QStyledItemDelegate
{
public:
    explicit CosmicRoundedHighlightDelegate(int cornerRadius, const QColor& highlightColor, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , mCornerRadius(cornerRadius)
        , mHighlightColor(highlightColor)
    {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        // Width is managed by the view; we enforce a constant row height
        return QSize(0, CosmicUiMetrics::controlThickness());
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        // Store original rect for highlight calculation
        QRect originalRect = opt.rect;

        // Nudge text slightly to the right
        opt.rect = opt.rect.adjusted(6, 0, 0, 0);

        const bool hovered = (opt.state & QStyle::State_MouseOver);
        const bool selected = (opt.state & QStyle::State_Selected);

        if (hovered || selected) {
            painter->save();
            // Highlight with sharp left corners and rounded right corners using full width
            QRect r = originalRect.adjusted(0, 2, 0, -2); // Use original rect for full width
            QColor fill = mHighlightColor;
            if (hovered && !selected) {
                fill = mHighlightColor.lighter(115);
            }
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            
            // Create path with sharp left corners and rounded right corners
            QPainterPath path;
            const int cr = qMax(0, qMin(mCornerRadius, qMin(r.width(), r.height()) / 2));
            if (cr > 0) {
                painter->setRenderHint(QPainter::Antialiasing, true);
                // Sharp left corners, rounded right corners
                path.moveTo(r.left(), r.top());
                path.lineTo(r.right() - cr, r.top());
                path.quadTo(r.right(), r.top(), r.right(), r.top() + cr);
                path.lineTo(r.right(), r.bottom() - cr);
                path.quadTo(r.right(), r.bottom(), r.right() - cr, r.bottom());
                path.lineTo(r.left(), r.bottom());
                path.lineTo(r.left(), r.top());
                path.closeSubpath();
                painter->drawPath(path);
            } else {
                painter->fillRect(r, fill);
            }
            painter->restore();
        }

        // Suppress default rectangular selection painting
        opt.state &= ~QStyle::State_Selected;

        // Ensure highlighted text is readable
        if (hovered || selected) {
            QPalette pal = opt.palette;
            pal.setColor(QPalette::Text, Qt::white);
            pal.setColor(QPalette::WindowText, Qt::white);
            opt.palette = pal;
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    int mCornerRadius;
    QColor mHighlightColor;
};

// CosmicPopupFrame
CosmicPopupFrame::CosmicPopupFrame(QWidget* parent)
    : QFrame(parent)
{
    // Qt::Tool keeps the popup above its parent window on all platforms.
    // Qt::FramelessWindowHint removes the OS title bar.
    // Qt::WindowStaysOnTopHint ensures it floats above sibling Tool windows.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
}

void CosmicPopupFrame::setCornerRadius(int r)
{
    mCornerRadius = qMax(0, r);
    update();
}

void CosmicPopupFrame::setBackgroundColor(const QColor& c)
{
    mBackgroundColor = c;
    update();
}

void CosmicPopupFrame::setTopClickCloseHeight(int h)
{
    mTopClickCloseHeight = qMax(0, h);
}

void CosmicPopupFrame::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, mCornerRadius > 0);

    QRectF r(rect());
    if (mCornerRadius > 0) {
        r = r.adjusted(0.5, 0.5, -0.5, -0.5);
    }

    const qreal cr = static_cast<qreal>(mCornerRadius);
    QPainterPath path;

    // Square top corners so the list can visually tuck behind the button,
    // and rounded bottom corners for the macOS squircle feel.
    path.moveTo(r.left(), r.top());
    path.lineTo(r.right(), r.top());
    path.lineTo(r.right(), r.bottom() - cr);
    if (cr > 0) {
        path.quadTo(r.right(), r.bottom(), r.right() - cr, r.bottom());
    }
    path.lineTo(r.left() + cr, r.bottom());
    if (cr > 0) {
        path.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - cr);
    }
    path.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(mBackgroundColor.isValid() ? mBackgroundColor : kCosmicBackground);
    p.drawPath(path);
}

void CosmicPopupFrame::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && mTopClickCloseHeight > 0) {
        if (event->pos().y() >= 0 && event->pos().y() < mTopClickCloseHeight) {
            close();
            event->accept();
            return;
        }
    }
    QFrame::mousePressEvent(event);
}

void CosmicPopupFrame::closeEvent(QCloseEvent* event)
{
    emit popupClosed();
    QFrame::closeEvent(event);
}


CosmicDropDown::CosmicDropDown(QWidget* parent)
    : QWidget(parent)
    , mCurrentIndex(-1)
    , mAccentColor(BlenderStyleSlider::getAccentColor())
    , mBackgroundColor(kDropDownBase)
    , mTextColor(kCosmicText)
    , mSelectedTextColor(Qt::white)
    , mCornerRadius(0)
    , mBorderColor(QColor(0x1f, 0x1f, 0x1f))
    , mBorderWidth(1)
    , mHover(false)
    , mPopupVisible(false)
    , mPopupFrame(nullptr)
    , mListWidget(nullptr)
    , mMenuPopup(nullptr)
{
    // Unified cosmic metrics
    mCornerRadius = CosmicUiMetrics::cornerRadius();
    mBorderWidth = 0;
    mBorderColor = QColor();

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(CosmicUiMetrics::controlThickness());
}

CosmicDropDown::~CosmicDropDown()
{
    hidePopup();
}

QMenu* CosmicDropDown::findAncestorMenu() const
{
    QWidget* w = parentWidget();
    while (w) {
        if (QMenu* m = qobject_cast<QMenu*>(w)) {
            return m;
        }
        w = w->parentWidget();
    }
    return nullptr;
}

void CosmicDropDown::showMenuPopup(QMenu* parentMenu)
{
    if (!parentMenu) {
        return;
    }

    // If already open, close.
    if (mMenuPopup && mMenuPopup->isVisible()) {
        mMenuPopup->close();
        return;
    }

    if (mMenuPopup) {
        mMenuPopup->deleteLater();
        mMenuPopup = nullptr;
    }

    QMenu* menu = new QMenu(parentMenu);
    mMenuPopup = menu;
    mPopupVisible = true;
    update();

    QActionGroup* group = new QActionGroup(menu);
    group->setExclusive(true);

    for (int i = 0; i < mOptions.size(); ++i) {
        QAction* a = menu->addAction(mOptions[i]);
        a->setCheckable(true);
        a->setChecked(i == mCurrentIndex);
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, i]() {
            setCurrentIndex(i);
            emit activated(i);
        });
    }

    connect(menu, &QMenu::aboutToHide, this, [this]() {
        mPopupVisible = false;
        if (mMenuPopup) {
            mMenuPopup->deleteLater();
            mMenuPopup = nullptr;
        }
        update();
    });

    // Show adjacent to the dropdown control, like a normal submenu.
    const QPoint globalTop = mapToGlobal(QPoint(0, 0));
    const QPoint popupPos(globalTop.x(), globalTop.y() + height());
    menu->popup(popupPos);
}

void CosmicDropDown::setOptions(const QStringList& options)
{
    mOptions = options;
    if (mCurrentIndex >= mOptions.size()) {
        mCurrentIndex = mOptions.isEmpty() ? -1 : 0;
    }
    if (mListWidget) {
        mListWidget->clear();
        mListWidget->addItems(mOptions);
        if (mCurrentIndex >= 0) {
            mListWidget->setCurrentRow(mCurrentIndex);
        }
    }
    update();
    updateGeometry();
}

void CosmicDropDown::setCurrentIndex(int index)
{
    if (index < 0 || index >= mOptions.size()) {
        return;
    }
    if (mCurrentIndex == index) {
        return;
    }
    mCurrentIndex = index;
    if (mListWidget) {
        mListWidget->setCurrentRow(index);
    }
    emit currentIndexChanged(index);
    update();
}

QString CosmicDropDown::currentText() const
{
    if (mCurrentIndex >= 0 && mCurrentIndex < mOptions.size()) {
        return mOptions[mCurrentIndex];
    }
    return QString();
}

void CosmicDropDown::setAccentColor(const QColor& color)
{
    mAccentColor = color;
    if (mListWidget) {
        mListWidget->viewport()->update();
    }
    update();
}

void CosmicDropDown::setBackgroundColor(const QColor& color)
{
    mBackgroundColor = color;
    update();
}

void CosmicDropDown::setTextColor(const QColor& color)
{
    mTextColor = color;
    update();
}

void CosmicDropDown::setSelectedTextColor(const QColor& color)
{
    mSelectedTextColor = color;
    update();
}

void CosmicDropDown::setCornerRadius(int radius)
{
    mCornerRadius = radius;
    update();
}

QSize CosmicDropDown::sizeHint() const
{
    QFontMetrics fm(font());
    int height = CosmicUiMetrics::controlThickness();
    int width = fm.horizontalAdvance(currentText().isEmpty() ? QStringLiteral(" ") : currentText())
                + kHorizontalPadding * 2 + kArrowSize + 8;
    if (width < 120) {
        width = 120;
    }
    return QSize(width, height);
}

QSize CosmicDropDown::minimumSizeHint() const
{
    return QSize(80, CosmicUiMetrics::controlThickness());
}

QRect CosmicDropDown::arrowRect() const
{
    const int margin = 8;
    int size = kArrowSize;
    int x = width() - margin - size;
    int y = (height() - size) / 2;
    return QRect(x, y, size, size);
}

QRect CosmicDropDown::textRect() const
{
    QRect r = rect().adjusted(kHorizontalPadding, 0, -kHorizontalPadding, 0);
    r.setRight(arrowRect().left() - 6);
    return r;
}

void CosmicDropDown::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background capsule: neutral base (matches mockup); hover/active slightly brighten/darken.
    QColor bg = mBackgroundColor;
    if (mPopupVisible) {
        bg = bg.darker(108);
    } else if (mHover) {
        bg = bg.lighter(110);
    }

    QRectF outerF(rect());
    if (mCornerRadius > 0) {
        outerF = outerF.adjusted(0.5, 0.5, -0.5, -0.5);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    if (mCornerRadius > 0) {
        p.drawRoundedRect(outerF, mCornerRadius, mCornerRadius);
    } else {
        p.drawRect(outerF);
    }

    // Text
    p.setPen(mTextColor);
    p.drawText(textRect(), Qt::AlignVCenter | Qt::AlignLeft, currentText());

    // Arrow icon: Expand.svg when popup is visible, Collapse.svg when hidden
    // Icons already have #808080 color, no tinting needed
    QRect a = arrowRect();
    QString iconPath = mPopupVisible ? 
        QStringLiteral(":/Interaction/Collapse.svg") : 
        QStringLiteral(":/Interaction/Expand.svg");
    QPixmap icon(iconPath);
    if (!icon.isNull()) {
        QPixmap scaled = icon.scaled(a.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // Center the icon within the arrow rect, accounting for any aspect ratio scaling
        int x = a.x() + (a.width() - scaled.width()) / 2;
        int y = a.y() + (a.height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
    }
}

void CosmicDropDown::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        showPopup();
    }
    QWidget::mousePressEvent(event);
}

void CosmicDropDown::enterEvent(QEvent* event)
{
    Q_UNUSED(event);
    mHover = true;
    update();
}

void CosmicDropDown::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    mHover = false;
    update();
}

void CosmicDropDown::ensurePopup()
{
    if (mPopupFrame) {
        return;
    }

    // Use the top-level window as parent so the popup stays above it on all platforms.
    QWidget* topLevel = window();
    mPopupFrame = new CosmicPopupFrame(topLevel);
    mPopupFrame->setAttribute(Qt::WA_TranslucentBackground, true);
    mPopupFrame->setCornerRadius(CosmicUiMetrics::cornerRadius());
    mPopupFrame->setBackgroundColor(mBackgroundColor);

    QVBoxLayout* layout = new QVBoxLayout(mPopupFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    mListWidget = new QListWidget(mPopupFrame);
    mListWidget->setFrameShape(QFrame::NoFrame);
    mListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    mListWidget->setUniformItemSizes(true);
    mListWidget->setMouseTracking(true);
    mListWidget->viewport()->setMouseTracking(true);
    mListWidget->setFocusPolicy(Qt::NoFocus);

    // Force popup list font to match the dropdown button text size.
    mListWidget->setFont(font());

    mListWidget->addItems(mOptions);
    if (mCurrentIndex >= 0) {
        mListWidget->setCurrentRow(mCurrentIndex);
    }

    layout->addWidget(mListWidget);

    // Style: background is painted by popup frame; list remains transparent.
    const int itemH = CosmicUiMetrics::controlThickness();
    const int padV = qMax(0, (itemH - mListWidget->fontMetrics().height()) / 2);
    mListWidget->setStyleSheet(QString(
        "QListWidget{background:transparent;color:%1;border:none;}"
        "QListWidget::item{height:%2px;padding:%3px 12px %3px 16px;}"
        ).arg(kCosmicText.name()).arg(itemH).arg(padV));

    const QColor highlightColor = kCosmicBlueViolet;

    QPalette pal = mListWidget->palette();
    pal.setColor(QPalette::Highlight, highlightColor);
    pal.setColor(QPalette::HighlightedText, mSelectedTextColor);
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Text, kCosmicText);
    mListWidget->setPalette(pal);

    mListWidget->setItemDelegate(new CosmicRoundedHighlightDelegate(mCornerRadius, highlightColor, mListWidget));

    connect(mListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!mListWidget) return;
        int idx = mListWidget->row(item);
        // Commit the selection first, then defer the popup close.
        // Deferring is critical: setCurrentIndex may emit a signal whose slot opens
        // a nested event loop (e.g. QMessageBox). If hidePopup() runs synchronously
        // inside that nested loop it destroys mListWidget while we're still on its
        // call stack, causing a use-after-free crash.
        setCurrentIndex(idx);
        emit activated(idx);
        QPointer<CosmicDropDown> self(this);
        QTimer::singleShot(0, this, [self]() {
            if (self) self->hidePopup();
        });
    });

    connect(mPopupFrame, &CosmicPopupFrame::popupClosed, this, [this]() {
        mPopupVisible = false;
        update();
    });

    // If the frame is ever destroyed externally, clean up our raw pointers and event filter.
    connect(mPopupFrame, &QObject::destroyed, this, [this]() {
        mPopupVisible = false;
        mPopupFrame = nullptr;
        mListWidget = nullptr;
        if (qApp) {
            qApp->removeEventFilter(this);
        }
        update();
    });
}

void CosmicDropDown::showPopup()
{
    if (QMenu* parentMenu = findAncestorMenu()) {
        showMenuPopup(parentMenu);
        return;
    }

    ensurePopup();
    if (!mPopupFrame) {
        return;
    }
    if (mPopupFrame->isVisible()) {
        mPopupFrame->hide();
        mPopupVisible = false;
        qApp->removeEventFilter(this);
        update();
        return;
    }

    mListWidget->clear();
    mListWidget->addItems(mOptions);
    if (mCurrentIndex >= 0) {
        mListWidget->setCurrentRow(mCurrentIndex);
    }

    // Unified geometry: popup shares the same width as the button.
    // It opens below the button so the button remains clickable to close.
    const QPoint globalTop = mapToGlobal(QPoint(0, 0));
    const int popupW = width();
    const int itemH = CosmicUiMetrics::controlThickness();

    // Set maximum visible items before showing scrollbar
    const int maxVisibleItems = 10; // Show scrollbar after 10 items
    const int visibleItems = qMin(mOptions.size(), maxVisibleItems);
    
    int popupH = visibleItems * itemH;
    if (popupH < 0) {
        popupH = 0;
    }

    int xStart = globalTop.x();
    int yStart = globalTop.y() + height();
    
    // Only check screen bounds if we're at the max visible items
    QScreen* screen = QApplication::screenAt(globalTop);
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    QRect screenRect = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    if (mOptions.size() > maxVisibleItems) {
        const int maxH = qMax(itemH, screenRect.bottom() - yStart - 12);
        popupH = qMin(popupH, maxH);
    }

    const int popupHWithPad = popupH;

    if (xStart + popupW > screenRect.right()) {
        xStart = screenRect.right() - popupW;
    }
    if (xStart < screenRect.left()) {
        xStart = screenRect.left();
    }
    if (yStart + popupHWithPad > screenRect.bottom()) {
        yStart = screenRect.bottom() - popupHWithPad;
    }
    if (yStart < screenRect.top()) {
        yStart = screenRect.top();
    }

    mPopupFrame->setGeometry(xStart, yStart, popupW, popupHWithPad);
    mPopupFrame->setTopClickCloseHeight(0);
    mListWidget->setMinimumWidth(popupW);
    mListWidget->setMaximumWidth(popupW);
    
    // Set the list widget height and always show scrollbar for consistency
    mListWidget->setFixedHeight(popupH);
    
    // Always show scrollbar for consistency across all scenarios
    if (mOptions.size() > maxVisibleItems) {
        mListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        mListWidget->verticalScrollBar()->setFixedWidth(12);
        mListWidget->verticalScrollBar()->setStyleSheet(
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
    } else {
        mListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    mListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Force layout update to ensure proper scrollbar behavior
    mListWidget->doItemsLayout();
    mListWidget->updateGeometry();
    mListWidget->update();

    // Position list at the current selection
    if (mCurrentIndex >= 0 && mCurrentIndex < mListWidget->count()) {
        mListWidget->scrollToItem(mListWidget->item(mCurrentIndex), QAbstractItemView::PositionAtCenter);
    }

    // Ensure overlay is above general content but the dropdown itself stays clickable.
    mPopupFrame->raise();
    raise();
    mPopupFrame->show();
    mPopupFrame->raise();  // raise again after show so WM places it on top
    mPopupVisible = true;
    update();

    qApp->installEventFilter(this);
}

void CosmicDropDown::hidePopup()
{
    if (mMenuPopup) {
        mMenuPopup->close();
        mMenuPopup->deleteLater();
        mMenuPopup = nullptr;
    }
    if (mPopupFrame) {
        // Disconnect all signals from the popup frame before destroying it
        // to prevent the popupClosed/destroyed handlers from firing on stale state.
        disconnect(mPopupFrame, nullptr, this, nullptr);
        mPopupFrame->hide();
        mPopupFrame->deleteLater();
        mPopupFrame = nullptr;
        mListWidget = nullptr;
    }
    // Always remove the event filter — it may have been installed by showPopup().
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    mPopupVisible = false;
    update();
}

void CosmicDropDown::keyPressEvent(QKeyEvent* event)
{
    if (mPopupVisible && mListWidget) {
        const int count = mListWidget->count();
        int row = mListWidget->currentRow();
        switch (event->key()) {
        case Qt::Key_Up:
            if (count > 0) {
                row = qMax(0, row - 1);
                mListWidget->setCurrentRow(row);
            }
            event->accept();
            return;
        case Qt::Key_Down:
            if (count > 0) {
                row = qMin(count - 1, row + 1);
                mListWidget->setCurrentRow(row);
            }
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (row >= 0 && row < count) {
                setCurrentIndex(row);
                emit activated(row);
                QPointer<CosmicDropDown> self(this);
                QTimer::singleShot(0, this, [self]() {
                    if (self) self->hidePopup();
                });
            }
            event->accept();
            return;
        case Qt::Key_Escape:
            hidePopup();
            event->accept();
            return;
        default:
            break;
        }
    }

    QWidget::keyPressEvent(event);
}

void CosmicDropDown::wheelEvent(QWheelEvent* event)
{
    if (!mPopupVisible && (event->modifiers() & Qt::ControlModifier)) {
        if (mOptions.isEmpty()) {
            QWidget::wheelEvent(event);
            return;
        }
        int dir = event->angleDelta().y() > 0 ? -1 : 1;
        int idx = mCurrentIndex;
        if (idx < 0) {
            idx = 0;
        }
        idx += dir;
        if (idx < 0) {
            idx = mOptions.size() - 1;
        }
        if (idx >= mOptions.size()) {
            idx = 0;
        }
        setCurrentIndex(idx);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

bool CosmicDropDown::eventFilter(QObject* obj, QEvent* event)
{
    if (mPopupVisible && mPopupFrame) {
        if (event->type() == QEvent::MouseButtonPress) {
            const QPoint gpos = static_cast<QMouseEvent*>(event)->globalPos();

            const QRect buttonRect(mapToGlobal(QPoint(0, 0)), size());
            const QRect popupRect = mPopupFrame->geometry();

            // Check if the click is on any child widget of the popup (including scrollbar)
            QWidget* clickedWidget = qApp->widgetAt(gpos);
            bool isPopupChild = false;
            if (clickedWidget) {
                QWidget* parent = clickedWidget;
                while (parent) {
                    if (parent == mPopupFrame || parent == mListWidget) {
                        isPopupChild = true;
                        break;
                    }
                    parent = parent->parentWidget();
                }
            }

            // If the click is outside button, popup, and popup children, close.
            if (!buttonRect.contains(gpos) && !popupRect.contains(gpos) && !isPopupChild) {
                hidePopup();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
