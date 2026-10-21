#include "blenderstyledropdown.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOption>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QListWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QCloseEvent>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"

// Implementation of PopupFrame

PopupFrame::PopupFrame(QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
}

void PopupFrame::closeEvent(QCloseEvent* event)
{
    emit popupClosed();
    QFrame::closeEvent(event);
}

namespace {
static const int kHorizontalPadding = 10;  // left-right padding for text
static const int kArrowSize = 10;          // px
static const int kPopupMargin = 2;         // margin around list inside popup
}

// Angular highlight delegate (no rounded corners) to provide selection visuals
class RoundedHighlightDelegate : public QStyledItemDelegate
{
public:
    explicit RoundedHighlightDelegate(int cornerRadius, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), mCornerRadius(cornerRadius) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        // Nudge text a bit to the right for better left spacing
        opt.rect = opt.rect.adjusted(6, 0, 0, 0);

        const bool hovered = (opt.state & QStyle::State_MouseOver);
        const bool selected = (opt.state & QStyle::State_Selected);

        // Draw rectangular background highlight
        if (hovered || selected) {
            painter->save();
            QRect r = opt.rect.adjusted(4, 2, -4, -2);
            QColor fill = opt.palette.highlight().color();
            if (hovered && !selected) fill = fill.lighter(115);
            painter->fillRect(r, fill);
            painter->restore();
        }

        // Suppress default rectangular selection painting
        opt.state &= ~QStyle::State_Selected;

        // Adjust text color when highlighted
        if (hovered || selected) {
            QPalette pal = opt.palette;
            pal.setColor(QPalette::Text, opt.palette.highlightedText().color());
            pal.setColor(QPalette::WindowText, opt.palette.highlightedText().color());
            opt.palette = pal;
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    int mCornerRadius;
};

BlenderStyleDropDown::BlenderStyleDropDown(QWidget* parent)
    : QWidget(parent)
    , mCurrentIndex(-1)
    , mCornerRadius(0)
    , mHover(false)
    , mPopupVisible(false)
    , mPopupFrame(nullptr)
    , mListWidget(nullptr)
{
    mAccentColor = getAccentColor();
    mBackgroundColor = QColor(50, 50, 50);
    mTextColor = QColor(220, 220, 220);
    // Dynamic text color based on accent color lightness
    QColor accent = getAccentColor();
    mSelectedTextColor = (accent.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

BlenderStyleDropDown::~BlenderStyleDropDown()
{
    hidePopup();
}

QColor BlenderStyleDropDown::getAccentColor()
{
    // Use the global accent color from the shared Blender-style helper
    return BlenderStyleSlider::getAccentColor();
}

void BlenderStyleDropDown::setOptions(const QStringList& options)
{
    mOptions = options;
    if (mCurrentIndex >= mOptions.size()) {
        mCurrentIndex = mOptions.isEmpty() ? -1 : 0;
    }
    if (mListWidget) {
        mListWidget->clear();
        mListWidget->addItems(mOptions);
        if (mCurrentIndex >= 0) mListWidget->setCurrentRow(mCurrentIndex);
    }
    update();
    updateGeometry();
}

void BlenderStyleDropDown::setCurrentIndex(int index)
{
    if (index < 0 || index >= mOptions.size()) return;
    if (mCurrentIndex == index) return;
    mCurrentIndex = index;
    if (mListWidget) mListWidget->setCurrentRow(index);
    emit currentIndexChanged(index);
    update();
}

QString BlenderStyleDropDown::currentText() const
{
    if (mCurrentIndex >= 0 && mCurrentIndex < mOptions.size()) return mOptions[mCurrentIndex];
    return QString();
}

void BlenderStyleDropDown::setAccentColor(const QColor& color)
{
    mAccentColor = color;
    // Recalculate text color based on new accent color lightness
    mSelectedTextColor = (color.lightness() > 128) ? QColor(17, 17, 17) : QColor(255, 255, 255);
    if (mListWidget) mListWidget->viewport()->update();
    update();
}

void BlenderStyleDropDown::setBackgroundColor(const QColor& color)
{
    mBackgroundColor = color;
    update();
}

void BlenderStyleDropDown::setTextColor(const QColor& color)
{
    mTextColor = color;
    update();
}

void BlenderStyleDropDown::setSelectedTextColor(const QColor& color)
{
    mSelectedTextColor = color;
    update();
}

void BlenderStyleDropDown::setCornerRadius(int radius)
{
    mCornerRadius = radius;
    update();
}

QSize BlenderStyleDropDown::sizeHint() const
{
    QFontMetrics fm(font());
    int height = fm.height() + 8;
    int width = fm.horizontalAdvance(currentText().isEmpty() ? QStringLiteral(" ") : currentText())
                + kHorizontalPadding * 2 + kArrowSize + 8;
    if (width < 120) width = 120;
    return QSize(width, height);
}

QSize BlenderStyleDropDown::minimumSizeHint() const
{
    QFontMetrics fm(font());
    int height = fm.height() + 8;
    return QSize(80, height);
}

QRect BlenderStyleDropDown::arrowRect() const
{
    const int margin = 8;
    int size = kArrowSize;
    int x = width() - margin - size;
    int y = (height() - size) / 2;
    return QRect(x, y, size, size);
}

QRect BlenderStyleDropDown::textRect() const
{
    QRect r = rect().adjusted(kHorizontalPadding, 0, -kHorizontalPadding, 0);
    r.setRight(arrowRect().left() - 6);
    return r;
}

void BlenderStyleDropDown::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background capsule
    QColor bg = mBackgroundColor;
    if (mHover && !mPopupVisible) bg = bg.lighter(115);
    p.setPen(QPen(bg.darker(150), 1));
    p.setBrush(bg);
    // Angular capsule: draw rectangle without rounded corners
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // Text
    p.setPen(mTextColor);
    p.drawText(textRect(), Qt::AlignVCenter | Qt::AlignLeft, currentText());

    // Arrow using style primitive
    QRect a = arrowRect();
    QStyleOption opt;
    opt.rect = a;
    opt.state = QStyle::State_Enabled;
    if (mPopupVisible) opt.state |= QStyle::State_Sunken;
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &opt, &p, this);
}

void BlenderStyleDropDown::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        showPopup();
    }
    QWidget::mousePressEvent(event);
}

void BlenderStyleDropDown::keyPressEvent(QKeyEvent* event)
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
                hidePopup();
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

void BlenderStyleDropDown::wheelEvent(QWheelEvent* event)
{
    if (!mPopupVisible && (event->modifiers() & Qt::ControlModifier)) {
        if (mOptions.isEmpty()) { QWidget::wheelEvent(event); return; }
        int dir = event->angleDelta().y() > 0 ? -1 : 1; // typical: positive = wheel up => previous
        int idx = mCurrentIndex;
        if (idx < 0) idx = 0;
        idx += dir;
        if (idx < 0) idx = mOptions.size() - 1;
        if (idx >= mOptions.size()) idx = 0;
        setCurrentIndex(idx); // do not emit activated here
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void BlenderStyleDropDown::enterEvent(QEvent* event)
{
    Q_UNUSED(event);
    mHover = true;
    update();
}

void BlenderStyleDropDown::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    mHover = false;
    update();
}

void BlenderStyleDropDown::ensurePopup()
{
    if (mPopupFrame) return;

    mPopupFrame = new PopupFrame(nullptr);
    mPopupFrame->setAttribute(Qt::WA_TranslucentBackground, true);
    mPopupFrame->setObjectName("BlenderStyleDropDownPopup");

    // Container layout and list
    QVBoxLayout* layout = new QVBoxLayout(mPopupFrame);
    layout->setContentsMargins(kPopupMargin, kPopupMargin, kPopupMargin, kPopupMargin);
    layout->setSpacing(0);

    mListWidget = new QListWidget(mPopupFrame);
    mListWidget->setFrameShape(QFrame::NoFrame);
    mListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    mListWidget->setUniformItemSizes(true);
    mListWidget->setMouseTracking(true);
    mListWidget->viewport()->setMouseTracking(true);

    // Populate
    mListWidget->addItems(mOptions);
    if (mCurrentIndex >= 0) mListWidget->setCurrentRow(mCurrentIndex);

    layout->addWidget(mListWidget);

    // Style via palette and custom delegate for rounded highlight
    mListWidget->setStyleSheet(QString(
        "QListWidget{background:%1;color:%2;border:none;}"
        "QListWidget::item{padding:6px 12px 6px 16px;}" // nudge text to the right
        ).arg(QColor(45,45,45).name()).arg(mTextColor.name()));

    // Custom highlight via item delegate paint by using the palette Highlight color
    QPalette pal = mListWidget->palette();
    pal.setColor(QPalette::Highlight, mAccentColor);
    pal.setColor(QPalette::HighlightedText, mSelectedTextColor);
    mListWidget->setPalette(pal);

    // Custom rounded highlight delegate
    mListWidget->setItemDelegate(new RoundedHighlightDelegate(mCornerRadius, mListWidget));

    // Connect selection and closing behavior
    connect(mListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item){
        int idx = mListWidget->row(item);
        setCurrentIndex(idx);
        emit activated(idx);
        hidePopup();
    });

    // Sync visibility flag when popup closes by any reason
    connect(mPopupFrame, &PopupFrame::popupClosed, this, [this]{ mPopupVisible = false; update(); });
}

void BlenderStyleDropDown::showPopup()
{
    ensurePopup();
    if (!mPopupFrame) return;
    if (mPopupFrame->isVisible()) {
        mPopupFrame->hide();
        mPopupVisible = false;
    }
    if (mPopupVisible) return;

    // Sync options and selection
    mListWidget->clear();
    mListWidget->addItems(mOptions);
    if (mCurrentIndex >= 0) mListWidget->setCurrentRow(mCurrentIndex);

    // Position directly below the widget; clamp to screen
    const QPoint globalPos = mapToGlobal(QPoint(0, height()));
    const int popupWidth = width();
    int popupHeight = qMin(300, qMax(120, mListWidget->sizeHintForRow(0) * qMin(10, mOptions.size()) + kPopupMargin * 2));
    mPopupFrame->resize(popupWidth, popupHeight);

    QRect screenRect = QApplication::desktop()->availableGeometry(this);
    QPoint pos = globalPos;
    if (pos.x() + popupWidth > screenRect.right()) pos.setX(screenRect.right() - popupWidth);
    if (pos.y() + popupHeight > screenRect.bottom()) pos = mapToGlobal(QPoint(0, -popupHeight));
    mPopupFrame->move(pos);

    mPopupFrame->show();
    mPopupVisible = true;

    // Close when losing focus or clicking elsewhere
    connect(mPopupFrame, &QFrame::destroyed, this, [this]{ mPopupVisible = false; mPopupFrame = nullptr; mListWidget = nullptr; });
}

void BlenderStyleDropDown::hidePopup()
{
    if (mPopupFrame) {
        mPopupFrame->hide();
        mPopupFrame->deleteLater();
        mPopupFrame = nullptr;
        mListWidget = nullptr;
    }
    mPopupVisible = false;
    update();
}


