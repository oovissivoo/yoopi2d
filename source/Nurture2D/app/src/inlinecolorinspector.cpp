#include "inlinecolorinspector.h"
#include "ui_colorinspector.h"
#include "colorslider.h"
#include "../../CUI/blenderstylebutton.h"
#include "../../CUI/blenderstyletoggle.h"
#include "../../CUI/cosmicbutton.h"

#include <QTabWidget>
#include <QTabBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QPalette>
#include <QBrush>
#include <QImage>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QApplication>
#include <QStyle>
#include <QTimer>
#include <QMouseEvent>
#include <QLineEdit>
#include <QIntValidator>
#include <QLabel>
#include <QToolButton>
#include <QPainter>
#include <QtMath>

// Helper class to add auto-repeat functionality to buttons
class AutoRepeatButtonHelper : public QObject
{
    Q_OBJECT
public:
    AutoRepeatButtonHelper(BlenderStyleButton* button, QSpinBox* spinBox, int delta, int minVal, int maxVal, QObject* parent = nullptr)
        : QObject(parent), mButton(button), mSpinBox(spinBox), mDelta(delta), mMinVal(minVal), mMaxVal(maxVal)
    {
        mRepeatTimer = new QTimer(this);
        mRepeatTimer->setInterval(50); // Repeat every 50ms while held
        
        mInitialDelayTimer = new QTimer(this);
        mInitialDelayTimer->setSingleShot(true);
        mInitialDelayTimer->setInterval(300); // Wait 300ms before starting repeat
        
        connect(mRepeatTimer, &QTimer::timeout, this, &AutoRepeatButtonHelper::onRepeat);
        connect(mInitialDelayTimer, &QTimer::timeout, this, &AutoRepeatButtonHelper::startRepeat);
        
        // Install event filter to detect press and release
        mButton->installEventFilter(this);
    }
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (obj == mButton) {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    // Immediate first action
                    adjustValue();
                    // Start delay timer before repeating
                    mInitialDelayTimer->start();
                }
            }
            else if (event->type() == QEvent::MouseButtonRelease) {
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    stopRepeat();
                }
            }
            else if (event->type() == QEvent::Leave) {
                // Stop if mouse leaves button while pressed
                stopRepeat();
            }
        }
        return QObject::eventFilter(obj, event);
    }
    
private slots:
    void startRepeat()
    {
        mRepeatTimer->start();
    }
    
    void onRepeat()
    {
        adjustValue();
    }
    
private:
    void adjustValue()
    {
        int newValue = qBound(mMinVal, mSpinBox->value() + mDelta, mMaxVal);
        mSpinBox->setValue(newValue);
    }
    
    void stopRepeat()
    {
        mInitialDelayTimer->stop();
        mRepeatTimer->stop();
    }
    
    BlenderStyleButton* mButton;
    QSpinBox* mSpinBox;
    int mDelta;
    int mMinVal;
    int mMaxVal;
    QTimer* mRepeatTimer;
    QTimer* mInitialDelayTimer;
};

// Compact value editor with label + inline edit + vertical increment/decrement buttons
class SliderValueControl : public QWidget
{
public:
    explicit SliderValueControl(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        mMin = 0;
        mMax = 100;
        mStep = 1;
        mValue = 0;

        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(1);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        setFixedWidth(50);

        mLabel = new QLabel(this);
        mLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mLabel->setMinimumWidth(24);
        mLabel->setMaximumWidth(36);
        mLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        h->addWidget(mLabel, 1);

        QWidget* buttons = new QWidget(this);
        auto* v = new QVBoxLayout(buttons);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);

        auto makeGlowIcon = [this](QStyle::StandardPixmap sp) {
            QIcon baseIcon = QApplication::style()->standardIcon(sp, nullptr, this);
            QPixmap normal = baseIcon.pixmap(QSize(10, 10));
            QPixmap bright = normal;
            {
                QPainter p(&bright);
                p.setCompositionMode(QPainter::CompositionMode_Screen);
                p.fillRect(bright.rect(), QColor(255, 255, 255, 160));
            }
            QIcon icon;
            icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
            icon.addPixmap(bright, QIcon::Active, QIcon::Off);
            icon.addPixmap(bright, QIcon::Selected, QIcon::Off);
            icon.addPixmap(bright, QIcon::Disabled, QIcon::Off);
            return icon;
        };

        mUpButton = new QToolButton(buttons);
        mUpButton->setAutoRaise(false);
        mUpButton->setCursor(Qt::PointingHandCursor);
        mUpButton->setIcon(makeGlowIcon(QStyle::SP_ArrowUp));
        mUpButton->setIconSize(QSize(12, 12));
        mUpButton->setFixedSize(18, 14);
        mUpButton->setAutoRepeat(true);
        mUpButton->setAutoRepeatDelay(300);
        mUpButton->setAutoRepeatInterval(50);
        mUpButton->setFocusPolicy(Qt::NoFocus);
        mUpButton->setStyleSheet(
            "QToolButton { background: transparent; border: none; padding: 0; }"
            "QToolButton::hover { background: transparent; }"
            "QToolButton::pressed { background: transparent; }"
        );

        mDownButton = new QToolButton(buttons);
        mDownButton->setAutoRaise(false);
        mDownButton->setCursor(Qt::PointingHandCursor);
        mDownButton->setIcon(makeGlowIcon(QStyle::SP_ArrowDown));
        mDownButton->setIconSize(QSize(12, 12));
        mDownButton->setFixedSize(18, 14);
        mDownButton->setAutoRepeat(true);
        mDownButton->setAutoRepeatDelay(300);
        mDownButton->setAutoRepeatInterval(50);
        mDownButton->setFocusPolicy(Qt::NoFocus);
        mDownButton->setStyleSheet(
            "QToolButton { background: transparent; border: none; padding: 0; }"
            "QToolButton::hover { background: transparent; }"
            "QToolButton::pressed { background: transparent; }"
        );

        v->addWidget(mUpButton);
        v->addWidget(mDownButton);
        h->addWidget(buttons, 0);

        mEditor = new QLineEdit(this);
        mEditor->hide();
        mEditor->setAlignment(Qt::AlignCenter);
        mEditor->setFrame(false);

        mLabel->installEventFilter(this);

        connect(mUpButton, &QToolButton::clicked, this, [this]() {
            applyDelta(mStep);
        });
        connect(mDownButton, &QToolButton::clicked, this, [this]() {
            applyDelta(-mStep);
        });
        connect(mEditor, &QLineEdit::editingFinished, this, &SliderValueControl::onEditFinished);

        updateLabel();
    }

    void setRange(int minVal, int maxVal)
    {
        mMin = minVal;
        mMax = maxVal;
        setValue(mValue);
    }

    void setStep(int step)
    {
        mStep = qMax(1, step);
    }

    void setSuffix(const QString& suffix)
    {
        mSuffix = suffix;
        updateLabel();
    }

    void setSpinBox(QSpinBox* sb)
    {
        mSpinBox = sb;
        if (mSpinBox) {
            mMin = mSpinBox->minimum();
            mMax = mSpinBox->maximum();
            mValue = mSpinBox->value();
            updateLabel();
        }
    }

    void setValue(int v)
    {
        int clamped = qBound(mMin, v, mMax);
        if (clamped == mValue)
            return;
        mValue = clamped;
        updateLabel();
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (obj == mLabel && event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                beginEdit();
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        if (mEditor->isVisible()) {
            mEditor->setGeometry(mLabel->geometry());
        }
    }

private:
    void onEditFinished()
    {
        bool ok = false;
        int v = mEditor->text().toInt(&ok);
        mEditor->hide();
        if (!ok) {
            updateLabel();
            return;
        }
        v = qBound(mMin, v, mMax);
        if (v != mValue) {
            mValue = v;
            updateLabel();
            if (mSpinBox) {
                mSpinBox->setValue(mValue);
            }
        }
    }

private:
    void beginEdit()
    {
        mEditor->setText(QString::number(mValue));
        mEditor->setGeometry(mLabel->geometry());
        mEditor->show();
        mEditor->setFocus();
        mEditor->selectAll();
    }

    void applyDelta(int delta)
    {
        int v = qBound(mMin, mValue + delta, mMax);
        if (v != mValue) {
            mValue = v;
            updateLabel();
            if (mSpinBox) {
                mSpinBox->setValue(mValue);
            }
        }
    }

    void updateLabel()
    {
        QString text = QString::number(mValue);
        if (!mSuffix.isEmpty()) {
            text += mSuffix;
        }
        mLabel->setText(text);
        if (!mEditor->isVisible()) {
            mEditor->setText(text);
        }
    }

    int mMin;
    int mMax;
    int mStep;
    int mValue;
    QString mSuffix;
    QLabel* mLabel = nullptr;
    QLineEdit* mEditor = nullptr;
    QToolButton* mUpButton = nullptr;
    QToolButton* mDownButton = nullptr;
    QSpinBox* mSpinBox = nullptr;
};

InlineColorInspector::InlineColorInspector(QWidget *parent)
    : QWidget(parent)
{
    mInner = new QWidget(this);
    auto* uiRaw = new Ui::ColorInspector; // generated from .ui
    uiRaw->setupUi(mInner);

    // Map generated UI to our proxy for easy use
    ui.colorSpecTabWidget = mInner->findChild<QTabWidget*>("colorSpecTabWidget");
    ui.rgbTab = mInner->findChild<QWidget*>("rgbTab");
    ui.hsvTab = mInner->findChild<QWidget*>("hsvTab");
    ui.redSlider = mInner->findChild<ColorSlider*>("redSlider");
    ui.greenSlider = mInner->findChild<ColorSlider*>("greenSlider");
    ui.blueSlider = mInner->findChild<ColorSlider*>("blueSlider");
    ui.rgbAlphaSlider = mInner->findChild<ColorSlider*>("rgbAlphaSlider");
    ui.hueSlider = mInner->findChild<ColorSlider*>("hueSlider");
    ui.saturationSlider = mInner->findChild<ColorSlider*>("saturationSlider");
    ui.valueSlider = mInner->findChild<ColorSlider*>("valueSlider");
    ui.hsvAlphaSlider = mInner->findChild<ColorSlider*>("hsvAlphaSlider");
    ui.colorWrapper = mInner->findChild<QWidget*>("colorWrapper");
    ui.color = mInner->findChild<QWidget*>("color");
    QLabel* hsvAlphaLabel = mInner->findChild<QLabel*>("hsvAlphaLabel");
    QLabel* rgbAlphaLabel = mInner->findChild<QLabel*>("rgbAlphaLabel");
    QLabel* hueLabel = mInner->findChild<QLabel*>("hueLabel");
    QLabel* saturationLabel = mInner->findChild<QLabel*>("saturationLabel");
    QLabel* valueLabel = mInner->findChild<QLabel*>("valueLabel");
    QLabel* redLabel = mInner->findChild<QLabel*>("redLabel");
    QLabel* greenLabel = mInner->findChild<QLabel*>("greenLabel");
    QLabel* blueLabel = mInner->findChild<QLabel*>("blueLabel");
    ui.redSpinBox = mInner->findChild<QSpinBox*>("redSpinBox");
    ui.greenSpinBox = mInner->findChild<QSpinBox*>("greenSpinBox");
    ui.blueSpinBox = mInner->findChild<QSpinBox*>("blueSpinBox");
    ui.rgbAlphaSpinBox = mInner->findChild<QSpinBox*>("rgbAlphaSpinBox");
    ui.hueSpinBox = mInner->findChild<QSpinBox*>("hueSpinBox");
    ui.saturationSpinBox = mInner->findChild<QSpinBox*>("saturationSpinBox");
    ui.valueSpinBox = mInner->findChild<QSpinBox*>("valueSpinBox");
    ui.hsvAlphaSpinBox = mInner->findChild<QSpinBox*>("hsvAlphaSpinBox");

    QGridLayout* hsvLayout = mInner->findChild<QGridLayout*>("hsvLayout");
    QGridLayout* rgbLayout = mInner->findChild<QGridLayout*>("rgbLayout");
    if (hsvLayout) {
        hsvLayout->setHorizontalSpacing(2);
        hsvLayout->setColumnStretch(0, 0); // H/S/V labels
        hsvLayout->setColumnStretch(1, 1); // sliders
        hsvLayout->setColumnStretch(2, 0); // value controls
    }
    if (rgbLayout) {
        rgbLayout->setHorizontalSpacing(2);
        rgbLayout->setColumnStretch(0, 0); // R/G/B labels
        rgbLayout->setColumnStretch(1, 1); // sliders
        rgbLayout->setColumnStretch(2, 0); // value controls
    }

    auto tightenLabel = [](QLabel* lbl) {
        if (!lbl) return;
        lbl->setMinimumWidth(10);
        lbl->setMaximumWidth(14);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    };
    tightenLabel(hueLabel);
    tightenLabel(saturationLabel);
    tightenLabel(valueLabel);
    tightenLabel(redLabel);
    tightenLabel(greenLabel);
    tightenLabel(blueLabel);

    // Replace spinboxes in the layouts with simple typed QLineEdit inputs.
    // Keep spinboxes hidden as canonical storage for existing logic.
    auto installTypedEdit = [this](QGridLayout* grid, QSpinBox* spin, QLineEdit*& edit) {
        if (!grid || !spin)
            return;

        int row = -1, col = -1, rowSpan = 1, colSpan = 1;
        const int index = grid->indexOf(spin);
        if (index < 0)
            return;
        grid->getItemPosition(index, &row, &col, &rowSpan, &colSpan);
        if (row < 0 || col < 0)
            return;

        edit = new QLineEdit(spin->parentWidget());
        edit->setAlignment(Qt::AlignCenter);
        edit->setFixedWidth(44);
        edit->setMinimumHeight(18);
        edit->setMaximumHeight(18);
        edit->setText(QString::number(spin->value()));
        edit->setValidator(new QIntValidator(spin->minimum(), spin->maximum(), edit));

        grid->removeWidget(spin);
        spin->hide();
        grid->addWidget(edit, row, col, rowSpan, colSpan);

        auto commitToSpin = [spin, edit]() {
            bool ok = false;
            int v = edit->text().trimmed().toInt(&ok);
            if (!ok) {
                QSignalBlocker b(edit);
                edit->setText(QString::number(spin->value()));
                return;
            }
            v = qBound(spin->minimum(), v, spin->maximum());
            if (spin->value() != v) {
                spin->setValue(v);
            }
        };

        connect(edit, &QLineEdit::editingFinished, this, [commitToSpin]() { commitToSpin(); });

        connect(spin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
                [edit](int v) {
                    if (!edit)
                        return;
                    QSignalBlocker b(edit);
                    edit->setText(QString::number(v));
                });
    };

    auto wrapSliderWithArrows = [this](QGridLayout* grid, ColorSlider* slider, QSpinBox* spin) {
        if (!grid || !slider || !spin)
            return;

        int row = -1, col = -1, rowSpan = 1, colSpan = 1;
        const int index = grid->indexOf(slider);
        if (index < 0)
            return;
        grid->getItemPosition(index, &row, &col, &rowSpan, &colSpan);
        if (row < 0 || col < 0)
            return;

        QWidget* wrapper = new QWidget(slider->parentWidget());
        QHBoxLayout* h = new QHBoxLayout(wrapper);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);

        auto makeArrow = [wrapper](const QString& iconPath) {
            auto* b = new CosmicButton(wrapper);
            b->setBordered(false);
            b->setText("");
            b->setIcon(QIcon(iconPath));
            b->setIconSize(QSize(14, 14));
            b->setFixedSize(18, 18);
            b->setAutoRepeat(true);
            b->setAutoRepeatDelay(300);
            b->setAutoRepeatInterval(50);
            return b;
        };

        CosmicButton* left = makeArrow(":/Interaction/ArrowLeft.svg");
        CosmicButton* right = makeArrow(":/Interaction/ArrowRight.svg");

        grid->removeWidget(slider);
        slider->setParent(wrapper);
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        slider->setFixedHeight(18);

        h->addWidget(left);
        h->addWidget(slider, 1);
        h->addWidget(right);
        grid->addWidget(wrapper, row, col, rowSpan, colSpan);

        connect(left, &QPushButton::clicked, this, [spin]() {
            spin->setValue(qBound(spin->minimum(), spin->value() - 1, spin->maximum()));
        });
        connect(right, &QPushButton::clicked, this, [spin]() {
            spin->setValue(qBound(spin->minimum(), spin->value() + 1, spin->maximum()));
        });
    };

    // HSV manual input should display pure numbers, without degree/percent symbols
    installTypedEdit(hsvLayout, ui.hueSpinBox, ui.hueEdit);
    installTypedEdit(hsvLayout, ui.saturationSpinBox, ui.saturationEdit);
    installTypedEdit(hsvLayout, ui.valueSpinBox, ui.valueEdit);

    installTypedEdit(rgbLayout, ui.redSpinBox, ui.redEdit);
    installTypedEdit(rgbLayout, ui.greenSpinBox, ui.greenEdit);
    installTypedEdit(rgbLayout, ui.blueSpinBox, ui.blueEdit);

    wrapSliderWithArrows(hsvLayout, ui.hueSlider, ui.hueSpinBox);
    wrapSliderWithArrows(hsvLayout, ui.saturationSlider, ui.saturationSpinBox);
    wrapSliderWithArrows(hsvLayout, ui.valueSlider, ui.valueSpinBox);

    wrapSliderWithArrows(rgbLayout, ui.redSlider, ui.redSpinBox);
    wrapSliderWithArrows(rgbLayout, ui.greenSlider, ui.greenSpinBox);
    wrapSliderWithArrows(rgbLayout, ui.blueSlider, ui.blueSpinBox);

    if (ui.colorSpecTabWidget) {
        if (QTabBar* bar = ui.colorSpecTabWidget->findChild<QTabBar*>()) {
            bar->hide();
        }
    }

    if (QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(mInner->layout())) {
        if (ui.colorSpecTabWidget) {
            mainLayout->removeWidget(ui.colorSpecTabWidget);

            QWidget* container = new QWidget(mInner);
            QHBoxLayout* hLayout = new QHBoxLayout(container);
            hLayout->setContentsMargins(0, 0, 0, 0);
            hLayout->setSpacing(4);

            QWidget* modeColumn = new QWidget(container);
            modeColumn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            modeColumn->setMinimumWidth(26);
            modeColumn->setMaximumWidth(30);

            QVBoxLayout* modeLayout = new QVBoxLayout(modeColumn);
            modeLayout->setContentsMargins(0, 0, 0, 0);
            modeLayout->setSpacing(0);

            ui.hsvModeToggle = new BlenderStyleToggle(modeColumn);
            ui.hsvModeToggle->setText(tr("H\nS\nV"));
            ui.hsvModeToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            ui.rgbModeToggle = new BlenderStyleToggle(modeColumn);
            ui.rgbModeToggle->setText(tr("R\nG\nB"));
            ui.rgbModeToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            modeLayout->addWidget(ui.hsvModeToggle);
            modeLayout->addWidget(ui.rgbModeToggle);
            modeLayout->setStretch(0, 1);
            modeLayout->setStretch(1, 1);

            hLayout->addWidget(modeColumn, 0);
            hLayout->addWidget(ui.colorSpecTabWidget, 1);

            mainLayout->insertWidget(0, container);
        }
    }

    if (ui.hsvModeToggle && ui.rgbModeToggle && ui.colorSpecTabWidget) {
        ui.hsvModeToggle->setChecked(ui.colorSpecTabWidget->currentWidget() == ui.hsvTab);
        ui.rgbModeToggle->setChecked(ui.colorSpecTabWidget->currentWidget() == ui.rgbTab);

        connect(ui.hsvModeToggle, &BlenderStyleToggle::clicked, this, [this]() {
            if (ui.colorSpecTabWidget && ui.hsvTab) {
                ui.colorSpecTabWidget->setCurrentWidget(ui.hsvTab);
            }
        });
        connect(ui.rgbModeToggle, &BlenderStyleToggle::clicked, this, [this]() {
            if (ui.colorSpecTabWidget && ui.rgbTab) {
                ui.colorSpecTabWidget->setCurrentWidget(ui.rgbTab);
            }
        });
    }

    // Layout
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0,0,0,0);
    l->addWidget(mInner);

    // Defaults
    isRgbColors = true;
    mCurrentColor = mCurrentColor.toHsv();

    // Initialize sliders
    ui.redSlider->init(ColorSlider::ColorSpecType::RGB, ColorSlider::ColorType::RED, mCurrentColor, 0.0, 255.0);
    ui.greenSlider->init(ColorSlider::ColorSpecType::RGB, ColorSlider::ColorType::GREEN, mCurrentColor, 0.0, 255.0);
    ui.blueSlider->init(ColorSlider::ColorSpecType::RGB, ColorSlider::ColorType::BLUE, mCurrentColor, 0.0, 255.0);
    if (ui.rgbAlphaSlider) ui.rgbAlphaSlider->hide();

    ui.hueSlider->init(ColorSlider::ColorSpecType::HSV, ColorSlider::ColorType::HUE, mCurrentColor, 0.0, 359.0);
    ui.saturationSlider->init(ColorSlider::ColorSpecType::HSV, ColorSlider::ColorType::SAT, mCurrentColor, 0.0, 255.0);
    ui.valueSlider->init(ColorSlider::ColorSpecType::HSV, ColorSlider::ColorType::VAL, mCurrentColor, 0.0, 255.0);
    if (ui.hsvAlphaSlider) ui.hsvAlphaSlider->hide();

    // Remove color preview area
    if (ui.colorWrapper) { ui.colorWrapper->setVisible(false); ui.colorWrapper->setMaximumHeight(0); }
    if (ui.color) { ui.color->setVisible(false); ui.color->setMaximumHeight(0); }
    if (hsvAlphaLabel) hsvAlphaLabel->setVisible(false);
    if (rgbAlphaLabel) rgbAlphaLabel->setVisible(false);

    // Make sliders thicker for better visibility and usability
    auto setThin = [](ColorSlider* s){ if (s) { s->setFixedHeight(18); s->update(); } };
    setThin(ui.redSlider);
    setThin(ui.greenSlider);
    setThin(ui.blueSlider);
    setThin(ui.hueSlider);
    setThin(ui.saturationSlider);
    setThin(ui.valueSlider);

    // Signals
    connect(ui.colorSpecTabWidget, &QTabWidget::currentChanged, this, &InlineColorInspector::onColorSpecChanged);

    auto onColorChangedSlider = static_cast<void(InlineColorInspector::*)(const QColor&)>(&InlineColorInspector::onColorChanged);
    connect(ui.redSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui.greenSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui.blueSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    if (ui.rgbAlphaSlider) ui.rgbAlphaSlider->hide();
    connect(ui.hueSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui.saturationSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui.valueSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    if (ui.hsvAlphaSlider) ui.hsvAlphaSlider->hide();

    auto spinBoxChanged = static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged);
    auto onSpin = static_cast<void(InlineColorInspector::*)()>(&InlineColorInspector::onColorChanged);
    connect(ui.redSpinBox, spinBoxChanged, this, onSpin);
    connect(ui.greenSpinBox, spinBoxChanged, this, onSpin);
    connect(ui.blueSpinBox, spinBoxChanged, this, onSpin);
    if (ui.rgbAlphaSpinBox) ui.rgbAlphaSpinBox->hide();
    connect(ui.hueSpinBox, spinBoxChanged, this, onSpin);
    connect(ui.saturationSpinBox, spinBoxChanged, this, onSpin);
    connect(ui.valueSpinBox, spinBoxChanged, this, onSpin);
    if (ui.hsvAlphaSpinBox) ui.hsvAlphaSpinBox->hide();

    onColorSpecChanged();
    updateControls();
}

InlineColorInspector::~InlineColorInspector() = default;

void InlineColorInspector::setColor(QColor newColor)
{
    newColor = (isRgbColors) ? newColor.toRgb() : newColor.toHsv();
    if (newColor == mCurrentColor) return;
    mCurrentColor = newColor;
    updateControls();
    // Emit so external listeners (e.g., hex, other views) can sync
    emit colorChanged(mCurrentColor);
}

void InlineColorInspector::updateControls()
{
    QSignalBlocker b1(ui.redSlider);
    QSignalBlocker b2(ui.greenSlider);
    QSignalBlocker b3(ui.blueSlider);
    QSignalBlocker b4(ui.rgbAlphaSlider);
    ui.redSlider->setRgb(mCurrentColor);
    ui.greenSlider->setRgb(mCurrentColor);
    ui.blueSlider->setRgb(mCurrentColor);
    if (ui.rgbAlphaSlider) ui.rgbAlphaSlider->hide();

    QSignalBlocker b5(ui.redSpinBox);
    QSignalBlocker b6(ui.greenSpinBox);
    QSignalBlocker b7(ui.blueSpinBox);
    QSignalBlocker b8(ui.rgbAlphaSpinBox);
    ui.redSpinBox->setValue(mCurrentColor.red());
    ui.greenSpinBox->setValue(mCurrentColor.green());
    ui.blueSpinBox->setValue(mCurrentColor.blue());
    if (ui.rgbAlphaSpinBox) ui.rgbAlphaSpinBox->hide();

    if (ui.redEdit) {
        QSignalBlocker b(ui.redEdit);
        ui.redEdit->setText(QString::number(ui.redSpinBox->value()));
    }
    if (ui.greenEdit) {
        QSignalBlocker b(ui.greenEdit);
        ui.greenEdit->setText(QString::number(ui.greenSpinBox->value()));
    }
    if (ui.blueEdit) {
        QSignalBlocker b(ui.blueEdit);
        ui.blueEdit->setText(QString::number(ui.blueSpinBox->value()));
    }

    QSignalBlocker b9(ui.hueSlider);
    QSignalBlocker b10(ui.saturationSlider);
    QSignalBlocker b11(ui.valueSlider);
    QSignalBlocker b12(ui.hsvAlphaSlider);
    ui.hueSlider->setHsv(mCurrentColor);
    ui.saturationSlider->setHsv(mCurrentColor);
    ui.valueSlider->setHsv(mCurrentColor);
    if (ui.hsvAlphaSlider) ui.hsvAlphaSlider->hide();

    QSignalBlocker b13(ui.hueSpinBox);
    QSignalBlocker b14(ui.saturationSpinBox);
    QSignalBlocker b15(ui.valueSpinBox);
    QSignalBlocker b16(ui.hsvAlphaSpinBox);
    ui.hueSpinBox->setValue(mCurrentColor.hue());
    ui.saturationSpinBox->setValue(qRound(mCurrentColor.saturation() / 2.55));
    ui.valueSpinBox->setValue(qRound(mCurrentColor.value() / 2.55));
    if (ui.hsvAlphaSpinBox) ui.hsvAlphaSpinBox->hide();

    if (ui.hueEdit) {
        QSignalBlocker b(ui.hueEdit);
        ui.hueEdit->setText(QString::number(ui.hueSpinBox->value()));
    }
    if (ui.saturationEdit) {
        QSignalBlocker b(ui.saturationEdit);
        ui.saturationEdit->setText(QString::number(ui.saturationSpinBox->value()));
    }
    if (ui.valueEdit) {
        QSignalBlocker b(ui.valueEdit);
        ui.valueEdit->setText(QString::number(ui.valueSpinBox->value()));
    }

    QPalette p = ui.color->palette();
    p.setColor(QPalette::Window, mCurrentColor);
    ui.color->setPalette(p);
    ui.color->update();
    mInner->update();
    update();
}

void InlineColorInspector::onColorSpecChanged()
{
    bool rgb = ui.colorSpecTabWidget->currentWidget() == ui.rgbTab;
    isRgbColors = rgb;
    mCurrentColor = rgb ? mCurrentColor.toRgb() : mCurrentColor.toHsv();
    if (ui.hsvModeToggle && ui.rgbModeToggle) {
        QSignalBlocker b1(ui.hsvModeToggle);
        QSignalBlocker b2(ui.rgbModeToggle);
        ui.hsvModeToggle->setChecked(!rgb);
        ui.rgbModeToggle->setChecked(rgb);
    }
    updateControls();
}

void InlineColorInspector::onColorChanged()
{
    if (isRgbColors) {
        mCurrentColor.setRgb(
            ui.redSpinBox->value(),
            ui.greenSpinBox->value(),
            ui.blueSpinBox->value(),
            255);
        
        // Update only RGB sliders (not spinboxes) to avoid resetting user input
        QSignalBlocker b1(ui.redSlider);
        QSignalBlocker b2(ui.greenSlider);
        QSignalBlocker b3(ui.blueSlider);
        ui.redSlider->setRgb(mCurrentColor);
        ui.greenSlider->setRgb(mCurrentColor);
        ui.blueSlider->setRgb(mCurrentColor);
        ui.redSlider->update();
        ui.greenSlider->update();
        ui.blueSlider->update();
    } else {
        mCurrentColor.setHsv(
            ui.hueSpinBox->value(),
            static_cast<int>(ui.saturationSpinBox->value() * 2.55),
            static_cast<int>(ui.valueSpinBox->value() * 2.55),
            255);
        
        // Update only HSV sliders (not spinboxes) to avoid resetting user input
        QSignalBlocker b1(ui.hueSlider);
        QSignalBlocker b2(ui.saturationSlider);
        QSignalBlocker b3(ui.valueSlider);
        ui.hueSlider->setHsv(mCurrentColor);
        ui.saturationSlider->setHsv(mCurrentColor);
        ui.valueSlider->setHsv(mCurrentColor);
        ui.hueSlider->update();
        ui.saturationSlider->update();
        ui.valueSlider->update();
    }
    
    emit colorChanged(mCurrentColor);
}

void InlineColorInspector::onColorChanged(const QColor &color)
{
    mCurrentColor = color;
    updateControls();
    emit colorChanged(color);
}

#include "inlinecolorinspector.moc"
