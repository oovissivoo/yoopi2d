#include "cosmiccolorsliders.h"

#include "pencildef.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QPixmap>
#include <QtMath>

#include <QSettings>
#include <QTimer>

#include "cosmictoolspane.h"

#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

// ─────────────────────────────────────────────────────────────────────────────
// GradientSlider
// ─────────────────────────────────────────────────────────────────────────────

GradientSlider::GradientSlider(Channel channel, QWidget* parent)
    : QWidget(parent)
    , mChannel(channel)
    , mContextColor(Qt::white)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(CosmicUiMetrics::controlThickness());
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void GradientSlider::setChannel(Channel ch)
{
    mChannel = ch;
    update();
}

GradientSlider::Channel GradientSlider::channel() const
{
    return mChannel;
}

double GradientSlider::value() const
{
    return mValue;
}

void GradientSlider::setValue(double v)
{
    mValue = qBound(0.0, v, maxValue());
    update();
}

void GradientSlider::setContextColor(const QColor& c)
{
    mContextColor = c;
    update();
}

double GradientSlider::maxValue() const
{
    switch (mChannel) {
    case Channel::H: return 359.0;  // 0-359, wraps seamlessly
    case Channel::S: return 100.0;
    case Channel::L: return 100.0;
    case Channel::R: return 255.0;
    case Channel::G: return 255.0;
    case Channel::B: return 255.0;
    }
    return 255.0;
}

double GradientSlider::posToValue(int x) const
{
    if (width() <= 0)
        return 0.0;
    double t = static_cast<double>(x) / static_cast<double>(width());
    t = qBound(0.0, t, 1.0);
    return t * maxValue();
}

int GradientSlider::valueToPos(double v) const
{
    if (width() <= 0)
        return 0;
    double t = v / maxValue();
    t = qBound(0.0, t, 1.0);
    return static_cast<int>(t * width());
}

QColor GradientSlider::sampleGradientAt(int x) const
{
    const double t = (width() > 0) ? qBound(0.0, static_cast<double>(x) / width(), 1.0) : 0.0;

    switch (mChannel) {
    case Channel::H: {
        // Fixed hue spectrum 0-300 (6 segments of 60°), seamless
        const double hues[7] = { 0.0, 60.0, 120.0, 180.0, 240.0, 300.0, 360.0 };
        const int n = 6;
        double seg = t * n;
        int idx = qBound(0, static_cast<int>(seg), n - 1);
        double frac = seg - idx;
        double hue = hues[idx] + frac * (hues[idx + 1] - hues[idx]);
        return QColor::fromHsl(static_cast<int>(hue) % 360, 220, 128);
    }
    case Channel::S: {
        // Fixed gray → saturated red
        return QColor(
            static_cast<int>(128 + t * (255 - 128)),
            static_cast<int>(128 - t * 128),
            static_cast<int>(128 - t * 128)
        );
    }
    case Channel::L: {
        // Fixed black → mid-gray → white
        const int v = static_cast<int>(t * 255);
        return QColor(v, v, v);
    }
    case Channel::R: {
        return QColor(static_cast<int>(t * 255), mContextColor.green(), mContextColor.blue());
    }
    case Channel::G: {
        return QColor(mContextColor.red(), static_cast<int>(t * 255), mContextColor.blue());
    }
    case Channel::B: {
        return QColor(mContextColor.red(), mContextColor.green(), static_cast<int>(t * 255));
    }
    }
    return Qt::black;
}

QColor GradientSlider::handleColor() const
{
    const int handleX = valueToPos(mValue);
    const QColor sample = sampleGradientAt(handleX);

    const double r = sample.red()   / 255.0;
    const double g = sample.green() / 255.0;
    const double b = sample.blue()  / 255.0;

    const double luminance = 0.2126 * qPow(r, 2.2)
                           + 0.7152 * qPow(g, 2.2)
                           + 0.0722 * qPow(b, 2.2);

    return (luminance < 0.5) ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0x00, 0x00);
}

void GradientSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    const int radius = CosmicUiMetrics::cornerRadius();

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1f, 0x1f, 0x1f));
    p.drawRoundedRect(r, radius, radius);

    // Gradient
    QLinearGradient grad(r.left(), 0, r.right(), 0);

    switch (mChannel) {
    case Channel::H:
        // Fixed full hue spectrum — 6 stops ending at 300° (magenta).
        // Qt interpolates the last segment back toward red, giving a seamless strip.
        grad.setColorAt(0.0 / 6.0, QColor::fromHsl(0,   220, 128));
        grad.setColorAt(1.0 / 6.0, QColor::fromHsl(60,  220, 128));
        grad.setColorAt(2.0 / 6.0, QColor::fromHsl(120, 220, 128));
        grad.setColorAt(3.0 / 6.0, QColor::fromHsl(180, 220, 128));
        grad.setColorAt(4.0 / 6.0, QColor::fromHsl(240, 220, 128));
        grad.setColorAt(5.0 / 6.0, QColor::fromHsl(300, 220, 128));
        grad.setColorAt(6.0 / 6.0, QColor::fromHsl(359, 220, 128));
        break;
    case Channel::S:
        // Fixed gray → fully saturated red — never affected by current color
        grad.setColorAt(0.0, QColor(128, 128, 128));
        grad.setColorAt(1.0, QColor(255, 0,   0));
        break;
    case Channel::L:
        // Fixed black → mid-gray → white — never affected by current color
        grad.setColorAt(0.0, QColor(0,   0,   0));
        grad.setColorAt(0.5, QColor(128, 128, 128));
        grad.setColorAt(1.0, QColor(255, 255, 255));
        break;
    case Channel::R:
        grad.setColorAt(0.0, QColor(0,   mContextColor.green(), mContextColor.blue()));
        grad.setColorAt(1.0, QColor(255, mContextColor.green(), mContextColor.blue()));
        break;
    case Channel::G:
        grad.setColorAt(0.0, QColor(mContextColor.red(), 0,   mContextColor.blue()));
        grad.setColorAt(1.0, QColor(mContextColor.red(), 255, mContextColor.blue()));
        break;
    case Channel::B:
        grad.setColorAt(0.0, QColor(mContextColor.red(), mContextColor.green(), 0));
        grad.setColorAt(1.0, QColor(mContextColor.red(), mContextColor.green(), 255));
        break;
    }

    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.save();
    p.setClipPath(clip);
    p.fillRect(r, grad);
    p.restore();

    // Handle — rounded rect (squircle look)
    const int handleW = 6;
    const int handleH = r.height() - 4;
    const int handleX = valueToPos(mValue) - handleW / 2;
    const int handleY = r.top() + (r.height() - handleH) / 2;
    const QRect handleRect(handleX, handleY, handleW, handleH);

    p.setPen(Qt::NoPen);
    p.setBrush(handleColor());
    p.drawRoundedRect(handleRect, 2, 2);
}

void GradientSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mDragging = true;
        mValue = qBound(0.0, posToValue(event->pos().x()), maxValue());
        update();
        emit valueChanged(mValue);
    }
}

void GradientSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (mDragging && (event->buttons() & Qt::LeftButton)) {
        mValue = qBound(0.0, posToValue(event->pos().x()), maxValue());
        update();
        emit valueChanged(mValue);
    }
}

void GradientSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && mDragging) {
        mDragging = false;
        emit valueCommitted(mValue);
    }
}

void GradientSlider::wheelEvent(QWheelEvent* event)
{
    const double delta = (event->angleDelta().y() > 0) ? 1.0 : -1.0;
    mValue = qBound(0.0, mValue + delta, maxValue());
    update();
    emit valueChanged(mValue);
    emit valueCommitted(mValue);
    event->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// SliderSection
// ─────────────────────────────────────────────────────────────────────────────

SliderSection::SliderSection(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Header
    mHeader = new QWidget(this);
    mHeader->setCursor(Qt::PointingHandCursor);
    mHeader->setFixedHeight(CosmicUiMetrics::controlThickness());
    mHeader->installEventFilter(this);

    auto* headerLayout = new QHBoxLayout(mHeader);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(4);

    // Icon label: shows Collapse.svg when expanded, Expand.svg when collapsed
    mIconLabel = new QLabel(mHeader);
    mIconLabel->setFixedSize(12, 12);
    mIconLabel->setAlignment(Qt::AlignCenter);
    mIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mIconLabel->setPixmap(QIcon(QStringLiteral(":/Interaction/Collapse.svg")).pixmap(12, 12));

    mLabel = new QLabel(title, mHeader);
    CosmicText::applyToWidget(mLabel);

    headerLayout->addWidget(mIconLabel);
    headerLayout->addWidget(mLabel);
    headerLayout->addStretch();

    // Body — 1px spacing between rows, 1px top margin for gap after header
    mBody = new QWidget(this);
    mBodyLayout = new QVBoxLayout(mBody);
    mBodyLayout->setContentsMargins(0, 1, 0, 0);
    mBodyLayout->setSpacing(1);

    outerLayout->addWidget(mHeader);
    outerLayout->addWidget(mBody);
}

bool SliderSection::isExpanded() const
{
    return mExpanded;
}

void SliderSection::setExpanded(bool expanded)
{
    if (mExpanded == expanded)
        return;
    mExpanded = expanded;
    mBody->setVisible(mExpanded);
    if (mIconLabel) {
        const QString iconPath = mExpanded
            ? QStringLiteral(":/Interaction/Collapse.svg")
            : QStringLiteral(":/Interaction/Expand.svg");
        mIconLabel->setPixmap(QIcon(iconPath).pixmap(12, 12));
    }
    emit expandedChanged(mExpanded);
}

void SliderSection::addRow(QWidget* row)
{
    mBodyLayout->addWidget(row);
}

void SliderSection::onHeaderClicked()
{
    setExpanded(!mExpanded);
}

bool SliderSection::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mHeader && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            onHeaderClicked();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// CosmicColorSliders
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Helper: create a single channel row widget
struct ChannelRow {
    QWidget*          widget = nullptr;
    QLabel*           label  = nullptr;
    CosmicInputField* input  = nullptr;
    GradientSlider*   slider = nullptr;
};

static ChannelRow makeChannelRow(const QString& labelText,
                                 GradientSlider::Channel channel,
                                 double rangeMin, double rangeMax,
                                 QWidget* parent)
{
    ChannelRow row;
    row.widget = new QWidget(parent);
    row.widget->setFixedHeight(CosmicUiMetrics::controlThickness());

    auto* layout = new QHBoxLayout(row.widget);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(4);

    row.label = new QLabel(labelText, row.widget);
    row.label->setFixedWidth(16);
    row.label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    CosmicText::applyToWidget(row.label);

    row.input = new CosmicInputField(row.widget);
    row.input->setRange(rangeMin, rangeMax);
    row.input->setDecimals(0);
    row.input->setFixedWidth(90);

    row.slider = new GradientSlider(channel, row.widget);

    layout->addWidget(row.label);
    layout->addWidget(row.input);
    layout->addWidget(row.slider, 1);

    return row;
}

} // namespace

CosmicColorSliders::CosmicColorSliders(QWidget* parent)
    : QWidget(parent)
    , mCurrentColor(Qt::white)
    , mStoredH(0.0)
    , mStoredS(0.0)
    , mStoredL(100.0)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── HSL section ──────────────────────────────────────────────────────────
    auto* hslSection = new SliderSection(QStringLiteral("HSL"), this);
    mHslSection = hslSection;

    auto hRow = makeChannelRow(QStringLiteral("H"), GradientSlider::Channel::H, 0, 359, hslSection->body());
    auto sRow = makeChannelRow(QStringLiteral("S"), GradientSlider::Channel::S, 0, 100, hslSection->body());
    auto lRow = makeChannelRow(QStringLiteral("L"), GradientSlider::Channel::L, 0, 100, hslSection->body());

    mHSlider = hRow.slider; mHInput = hRow.input;
    mSSlider = sRow.slider; mSInput = sRow.input;
    mLSlider = lRow.slider; mLInput = lRow.input;

    hslSection->addRow(hRow.widget);
    hslSection->addRow(sRow.widget);
    hslSection->addRow(lRow.widget);

    // ── RGB section ──────────────────────────────────────────────────────────
    auto* rgbSection = new SliderSection(QStringLiteral("RGB"), this);
    mRgbSection = rgbSection;

    auto rRow = makeChannelRow(QStringLiteral("R"), GradientSlider::Channel::R, 0, 255, rgbSection->body());
    auto gRow = makeChannelRow(QStringLiteral("G"), GradientSlider::Channel::G, 0, 255, rgbSection->body());
    auto bRow = makeChannelRow(QStringLiteral("B"), GradientSlider::Channel::B, 0, 255, rgbSection->body());

    mRSlider = rRow.slider; mRInput = rRow.input;
    mGSlider = gRow.slider; mGInput = gRow.input;
    mBSlider = bRow.slider; mBInput = bRow.input;

    rgbSection->addRow(rRow.widget);
    rgbSection->addRow(gRow.widget);
    rgbSection->addRow(bRow.widget);

    mainLayout->addWidget(hslSection);
    mainLayout->addWidget(rgbSection);
    mainLayout->addStretch();

    // Restore per-section expanded state.
    // Keep these keys scoped to the Color Sliders pane under the same CosmicToolsPaneState namespace.
    // Default is expanded for both.
    {
        mRestoringState = true;
        QSettings s(PENCIL2D, PENCIL2D);
        const bool hslExpanded = s.value(QStringLiteral("CosmicToolsPaneState/Content/ColorSliders/HSLExpanded"), true).toBool();
        const bool rgbExpanded = s.value(QStringLiteral("CosmicToolsPaneState/Content/ColorSliders/RGBExpanded"), true).toBool();
        hslSection->setExpanded(hslExpanded);
        rgbSection->setExpanded(rgbExpanded);
        mRestoringState = false;
    }

    connect(hslSection, &SliderSection::expandedChanged, this, [this](bool on) {
        requestPaneHeightRefresh();
        if (mRestoringState) {
            return;
        }
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("CosmicToolsPaneState/Content/ColorSliders/HSLExpanded"), on);
        s.sync();
    });
    connect(rgbSection, &SliderSection::expandedChanged, this, [this](bool on) {
        requestPaneHeightRefresh();
        if (mRestoringState) {
            return;
        }
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("CosmicToolsPaneState/Content/ColorSliders/RGBExpanded"), on);
        s.sync();
    });

    // If the whole pane is collapsed/expanded (or disabled then re-enabled), the right tool panel
    // may restore a previously persisted height. On expand, re-apply the content-driven height
    // for the current HSL/RGB expanded state.
    auto hookPaneExpand = [this]() {
        auto* pane = qobject_cast<CosmicToolsPane*>(parentWidget());
        if (!pane) {
            pane = qobject_cast<CosmicToolsPane*>(parentWidget() ? parentWidget()->parentWidget() : nullptr);
        }
        if (!pane) {
            return;
        }
        connect(pane, &CosmicToolsPane::expandedChanged, this, [this](bool expanded) {
            if (expanded) {
                requestPaneHeightRefresh();
            }
        });
    };

    hookPaneExpand();
    QTimer::singleShot(0, this, [hookPaneExpand]() { hookPaneExpand(); });

    // ── Hold-to-repeat for +/- buttons ───────────────────────────────────────
    // Installs an event filter on a CosmicInputField that watches for left-button
    // press/release on the left (decrement) or right (increment) button regions.
    // On hold, fires a delay timer then a repeat timer, each tick calling
    // input->setValue(±step) which emits valueChanged/valueCommitted normally.
    struct RepeatFilter : public QObject {
        CosmicInputField* field;
        QTimer* delayTimer;
        QTimer* repeatTimer;
        int direction = 0; // -1 dec, +1 inc, 0 none

        RepeatFilter(CosmicInputField* f, QObject* parent)
            : QObject(parent), field(f)
        {
            delayTimer  = new QTimer(this);
            repeatTimer = new QTimer(this);
            delayTimer->setSingleShot(true);
            delayTimer->setInterval(400);
            repeatTimer->setSingleShot(false);
            repeatTimer->setInterval(60);
            connect(delayTimer,  &QTimer::timeout, repeatTimer, [this]() { repeatTimer->start(); });
            connect(repeatTimer, &QTimer::timeout, this, &RepeatFilter::tick);
        }

        void tick() {
            if (direction == 0) { stop(); return; }
            const double next = field->value() + direction * field->singleStep();
            field->setValue(qBound(field->minimum(), next, field->maximum()));
            // valueChanged already fired via setValue; also emit valueCommitted so
            // the color propagates through the same path as a single click.
            emit field->valueCommitted(field->value());
        }

        void stop() {
            direction = 0;
            delayTimer->stop();
            repeatTimer->stop();
        }

        bool eventFilter(QObject*, QEvent* e) override {
            if (e->type() == QEvent::MouseButtonPress) {
                auto* me = static_cast<QMouseEvent*>(e);
                if (me->button() == Qt::LeftButton) {
                    const int bw = field->buttonWidth();
                    if (me->pos().x() < bw)
                        direction = -1;
                    else if (me->pos().x() >= field->width() - bw)
                        direction = +1;
                    else
                        direction = 0;
                    if (direction != 0)
                        delayTimer->start();
                }
            } else if (e->type() == QEvent::MouseButtonRelease ||
                       e->type() == QEvent::Leave) {
                stop();
            }
            return false; // never consume — CosmicInputField handles normally
        }
    };

    auto installRepeat = [](CosmicInputField* input) {
        auto* rf = new RepeatFilter(input, input);
        input->installEventFilter(rf);
    };

    installRepeat(mHInput);
    installRepeat(mSInput);
    installRepeat(mLInput);
    installRepeat(mRInput);
    installRepeat(mGInput);
    installRepeat(mBInput);

    // Input connections: valueCommitted fires on every +/- click and Enter.
    // valueChanged fires on drag-scrub. Both paths go through the same onHsl/RgbChanged.
    auto connectHslInput = [this](CosmicInputField* input, GradientSlider* slider) {
        auto hslUpdate = [this, input, slider]() {
            if (mUpdating) return;
            slider->setValue(input->value());
            onHslChanged();
        };
        connect(input, &CosmicInputField::valueCommitted, this, [hslUpdate](double) { hslUpdate(); });
        connect(input, &CosmicInputField::valueChanged,   this, [hslUpdate](double) { hslUpdate(); });
        if (auto* le = input->findChild<QLineEdit*>()) {
            connect(le, &QLineEdit::textChanged, this, [this, input, slider](const QString& text) {
                if (mUpdating) return;
                bool ok = false;
                double v = text.trimmed().toDouble(&ok);
                if (ok) { slider->setValue(v); onHslChanged(); }
            });
        }
    };

    auto connectRgbInput = [this](CosmicInputField* input, GradientSlider* slider) {
        auto rgbUpdate = [this, input, slider]() {
            if (mUpdating) return;
            slider->setValue(input->value());
            onRgbChanged();
        };
        connect(input, &CosmicInputField::valueCommitted, this, [rgbUpdate](double) { rgbUpdate(); });
        connect(input, &CosmicInputField::valueChanged,   this, [rgbUpdate](double) { rgbUpdate(); });
        if (auto* le = input->findChild<QLineEdit*>()) {
            connect(le, &QLineEdit::textChanged, this, [this, input, slider](const QString& text) {
                if (mUpdating) return;
                bool ok = false;
                double v = text.trimmed().toDouble(&ok);
                if (ok) { slider->setValue(v); onRgbChanged(); }
            });
        }
    };

    // ── Connections: HSL sliders ──────────────────────────────────────────────
    connect(mHSlider, &GradientSlider::valueChanged,   this, &CosmicColorSliders::onHslChanged);
    connect(mSSlider, &GradientSlider::valueChanged,   this, &CosmicColorSliders::onHslChanged);
    connect(mLSlider, &GradientSlider::valueChanged,   this, &CosmicColorSliders::onHslChanged);

    connect(mHSlider, &GradientSlider::valueCommitted, this, [this](double) { emit colorCommitted(mCurrentColor); });
    connect(mSSlider, &GradientSlider::valueCommitted, this, [this](double) { emit colorCommitted(mCurrentColor); });
    connect(mLSlider, &GradientSlider::valueCommitted, this, [this](double) { emit colorCommitted(mCurrentColor); });

    connectHslInput(mHInput, mHSlider);
    connectHslInput(mSInput, mSSlider);
    connectHslInput(mLInput, mLSlider);

    // ── Connections: RGB sliders ──────────────────────────────────────────────
    connect(mRSlider, &GradientSlider::valueChanged,   this, &CosmicColorSliders::onRgbChanged);
    connect(mGSlider, &GradientSlider::valueChanged,   this, &CosmicColorSliders::onRgbChanged);
    connect(mBSlider, &GradientSlider::valueChanged,   this, &CosmicColorSliders::onRgbChanged);

    connect(mRSlider, &GradientSlider::valueCommitted, this, [this](double) { emit colorCommitted(mCurrentColor); });
    connect(mGSlider, &GradientSlider::valueCommitted, this, [this](double) { emit colorCommitted(mCurrentColor); });
    connect(mBSlider, &GradientSlider::valueCommitted, this, [this](double) { emit colorCommitted(mCurrentColor); });

    connectRgbInput(mRInput, mRSlider);
    connectRgbInput(mGInput, mGSlider);
    connectRgbInput(mBInput, mBSlider);

    // Initialise with default color
    updateAllFromColor(mCurrentColor);

    requestPaneHeightRefresh();
}

QColor CosmicColorSliders::color() const
{
    return mCurrentColor;
}

QSize CosmicColorSliders::sizeHint() const
{
    return QSize(width(), contentHeightForCurrentState());
}

int CosmicColorSliders::contentHeightForCurrentState() const
{
    const int rowH    = CosmicUiMetrics::controlThickness();
    const int headerH = CosmicUiMetrics::controlThickness();
    const int nRows   = 3;
    const int bodyH = 1 + nRows * rowH + (nRows - 1) * 1;
    const int sectionExpandedH = headerH + bodyH;

    int h = 0;
    h += headerH;
    if (mHslSection && mHslSection->isExpanded()) {
        h += bodyH;
    }
    h += headerH;
    if (mRgbSection && mRgbSection->isExpanded()) {
        h += bodyH;
    }

    // If both sections are expanded, this is exactly 2*sectionExpandedH.
    Q_UNUSED(sectionExpandedH);
    return h;
}

void CosmicColorSliders::requestPaneHeightRefresh()
{
    updateGeometry();

    auto* pane = qobject_cast<CosmicToolsPane*>(parentWidget());
    if (!pane) {
        // In some contexts the content can be wrapped; climb one more level.
        pane = qobject_cast<CosmicToolsPane*>(parentWidget() ? parentWidget()->parentWidget() : nullptr);
    }
    if (!pane || !pane->isExpanded()) {
        return;
    }

    // CosmicToolsPane body margins: (4,10,4,10)
    const int bodyTopBottom = 20;
    const int headerH = pane->headerWidget() ? pane->headerWidget()->height() : 0;

    const int desired = headerH + bodyTopBottom + contentHeightForCurrentState();

    // Minimum should be driven by real content, not a generic 140px clamp.
    // When both sections are collapsed, we still need:
    // - pane header
    // - pane body top+bottom margins
    // - 2 section headers inside the content widget
    const int minContent = headerH + bodyTopBottom + 2 * CosmicUiMetrics::controlThickness();

    // Respect the same clamp as CosmicRightToolPanel uses for pane resizing.
    const int clamped = qBound(minContent, desired, 900);
    pane->setFixedHeight(clamped);
    pane->setLastExpandedHeight(clamped);
}

void CosmicColorSliders::setColor(const QColor& color)
{
    if (!color.isValid())
        return;
    mCurrentColor = color;
    // Preserve HSL components independently so they survive round-trips through black/white
    mStoredH = (color.hslHue() < 0) ? mStoredH : static_cast<double>(color.hslHue());
    mStoredS = color.hslSaturationF() * 100.0;
    mStoredL = color.lightnessF() * 100.0;
    updateAllFromColor(color);
}

void CosmicColorSliders::updateAllFromColor(const QColor& c)
{
    mUpdating = true;

    // Use stored HSL so H and S survive when L=0 or L=100
    mHSlider->setValue(mStoredH);
    mSSlider->setValue(mStoredS);
    mLSlider->setValue(mStoredL);
    mHInput->setValue(mStoredH);
    mSInput->setValue(mStoredS);
    mLInput->setValue(mStoredL);

    // RGB values derived from actual color
    const double r = static_cast<double>(c.red());
    const double g = static_cast<double>(c.green());
    const double b = static_cast<double>(c.blue());

    mRSlider->setValue(r);
    mGSlider->setValue(g);
    mBSlider->setValue(b);
    mRInput->setValue(r);
    mGInput->setValue(g);
    mBInput->setValue(b);

    mHSlider->setContextColor(c);
    mSSlider->setContextColor(c);
    mLSlider->setContextColor(c);
    mRSlider->setContextColor(c);
    mGSlider->setContextColor(c);
    mBSlider->setContextColor(c);

    mUpdating = false;
}

void CosmicColorSliders::onHslChanged()
{
    if (mUpdating)
        return;

    // Store H and S independently so they survive L=0/100 round-trips
    mStoredH = mHSlider->value();
    mStoredS = mSSlider->value();
    mStoredL = mLSlider->value();

    mCurrentColor = QColor::fromHslF(mStoredH / 360.0,
                                     mStoredS / 100.0,
                                     mStoredL / 100.0);

    mUpdating = true;

    const double r = static_cast<double>(mCurrentColor.red());
    const double g = static_cast<double>(mCurrentColor.green());
    const double b = static_cast<double>(mCurrentColor.blue());

    mRSlider->setValue(r);
    mGSlider->setValue(g);
    mBSlider->setValue(b);
    mRInput->setValue(r);
    mGInput->setValue(g);
    mBInput->setValue(b);

    mHSlider->setContextColor(mCurrentColor);
    mSSlider->setContextColor(mCurrentColor);
    mLSlider->setContextColor(mCurrentColor);
    mRSlider->setContextColor(mCurrentColor);
    mGSlider->setContextColor(mCurrentColor);
    mBSlider->setContextColor(mCurrentColor);

    mUpdating = false;

    emit colorChanged(mCurrentColor);
}

void CosmicColorSliders::onRgbChanged()
{
    if (mUpdating)
        return;

    const int r = static_cast<int>(mRSlider->value());
    const int g = static_cast<int>(mGSlider->value());
    const int b = static_cast<int>(mBSlider->value());

    mCurrentColor = QColor(r, g, b);

    // Update stored HSL — but preserve H if color is achromatic (S=0 or L=0/100)
    // so that moving back to a chromatic state restores the last meaningful hue
    const double newH = mCurrentColor.hslHue();
    const double newS = mCurrentColor.hslSaturationF() * 100.0;
    const double newL = mCurrentColor.lightnessF() * 100.0;

    if (newH >= 0) mStoredH = newH;   // only update H if color is chromatic
    mStoredS = newS;
    mStoredL = newL;

    mUpdating = true;

    mHSlider->setValue(mStoredH);
    mSSlider->setValue(mStoredS);
    mLSlider->setValue(mStoredL);
    mHInput->setValue(mStoredH);
    mSInput->setValue(mStoredS);
    mLInput->setValue(mStoredL);

    mHSlider->setContextColor(mCurrentColor);
    mSSlider->setContextColor(mCurrentColor);
    mLSlider->setContextColor(mCurrentColor);
    mRSlider->setContextColor(mCurrentColor);
    mGSlider->setContextColor(mCurrentColor);
    mBSlider->setContextColor(mCurrentColor);

    mUpdating = false;

    emit colorChanged(mCurrentColor);
}
