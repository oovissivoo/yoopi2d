#include "cosmicignorecolor.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

#include "cosmiccheckboxtoggle.h"

// Simple square widget that displays the current ignore color. Click
// handling is delegated via an event filter installed by
// CosmicIgnoreColor so we don't need a separate QObject meta-object.
class ColorSwatch : public QWidget
{
public:
    explicit ColorSwatch(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(22, 22);
    }

    void setColor(const QColor& c)
    {
        mColor = c.isValid() ? c : Qt::black;
        update();
    }

    void setActive(bool on)
    {
        if (mActive == on) return;
        mActive = on;
        update();
    }
protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = r.width() * 0.28; // squircle-ish corner radius
        const QColor border(0x1f, 0x1f, 0x1f, 0xff);
        QColor fill = mColor.isValid() ? mColor : Qt::black;

        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);

        p.setPen(QPen(border, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, radius, radius);

        if (mActive) {
            QColor activeBorder = Qt::white;
            activeBorder.setAlpha(80);
            p.setPen(QPen(activeBorder, 2));
            p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), radius * 0.7, radius * 0.7);
        }
    }

    QSize sizeHint() const override
    {
        return QSize(22, 22);
    }

    QSize minimumSizeHint() const override
    {
        return QSize(20, 20);
    }

private:
    QColor mColor;
    bool   mActive = false;
};

CosmicIgnoreColor::CosmicIgnoreColor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Enable checkbox: standard DefaultCheckbox in ScaleBig mode.
    mEnableToggle = new CosmicCheckboxToggle(this);
    mEnableToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mEnableToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
    mEnableToggle->setShowText(false);
    mEnableToggle->setShowIcon(true);
    mEnableToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Color swatch: square widget using the ignore color as fill.
    mColorSwatch = new ColorSwatch(this);
    mColorSwatch->installEventFilter(this);

    // Keep both widgets grouped at the right edge. Show color box
    // first for consistency, with the enable checkbox at the far
    // right edge.
    layout->addStretch(1);
    layout->addWidget(mColorSwatch, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(mEnableToggle, 0, Qt::AlignRight | Qt::AlignVCenter);

    setLayout(layout);
    // Let this widget expand horizontally so that its internal
    // stretch can push the checkbox + swatch group all the way
    // to the right edge of the split layout column.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Default color: opaque black
    setIgnoreColor(Qt::black);

    connect(mEnableToggle, &QAbstractButton::toggled,
            this, &CosmicIgnoreColor::ignoreEnabledChanged);
}

bool CosmicIgnoreColor::ignoreEnabled() const
{
    return mEnableToggle && mEnableToggle->isChecked();
}

bool CosmicIgnoreColor::colorPickerActive() const
{
    return mPickerActive;
}

void CosmicIgnoreColor::setIgnoreEnabled(bool on)
{
    if (!mEnableToggle) return;
    QSignalBlocker b(mEnableToggle);
    mEnableToggle->setChecked(on);
}

void CosmicIgnoreColor::setIgnoreColor(const QColor& color)
{
    mColor = color.isValid() ? color : Qt::black;
    if (!mColorSwatch) return;
    mColorSwatch->setColor(mColor);
}

void CosmicIgnoreColor::setColorPickerActive(bool on)
{
    mPickerActive = on;
    if (!mColorSwatch) return;
    mColorSwatch->setActive(on);
}

bool CosmicIgnoreColor::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mColorSwatch && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            mPickerActive = !mPickerActive;
            if (mColorSwatch) mColorSwatch->setActive(mPickerActive);
            emit colorPickToggled(mPickerActive);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
