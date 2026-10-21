#include "cosmiclayeropacitypopup.h"

#include "cosmicwindowheader.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "layervector.h"
#include "vectorimage.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmictext.h"
#include "../CUI/cosmicuimetrics.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QWidget* makeCard(QWidget* parent, QVBoxLayout*& outLay)
{
    const int r = CosmicUiMetrics::windowCornerRadius();
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString(
        "QWidget{ background: #1e1e1e; border-radius: %1px; }").arg(r));
    outLay = new QVBoxLayout(card);
    outLay->setContentsMargins(14, 12, 14, 12);
    outLay->setSpacing(10);
    return card;
}

QLabel* makeLabel(const QString& text, QWidget* parent,
                  int fontSize = 11, bool muted = false)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "QLabel{ background: transparent; color: %1; font-size: %2px; }")
        .arg(muted ? QStringLiteral("#777777") : QStringLiteral("#b3b3b3"))
        .arg(fontSize));
    lbl->setWordWrap(true);
    return lbl;
}

QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-size: 11px; font-weight: 600; }"));
    return lbl;
}

QFrame* makeSeparator(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("QFrame{ background: #2a2a2a; border: none; }"));
    return sep;
}

} // namespace

// ── constructor ───────────────────────────────────────────────────────────────

CosmicLayerOpacityPopup::CosmicLayerOpacityPopup(QWidget* parent)
    : CosmicPopupWindow(parent)
{
    setModalMode(ModalMode::NonModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);
    setDraggable(true);
    setDragRegionHeight(40);
    setFixedWindowSize(QSize(340, 1));
    setShadowEnabled(false);
    setShadowPadding(0);
    setPanelColor(QColor(0x2f, 0x2f, 0x2f));
    setBorderColor(CosmicColor::popupBorder());
    setBorderWidth(2);

    buildUI();
}

CosmicLayerOpacityPopup::~CosmicLayerOpacityPopup() = default;

void CosmicLayerOpacityPopup::buildUI()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("LayerOpacityRoot"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#LayerOpacityRoot{ background: transparent; border: none; }"));

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(16, 10, 16, 14);
    lay->setSpacing(10);

    mHeader = new CosmicWindowHeader(root);
    lay->addWidget(mHeader);

    // ── title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel(tr("Layer Opacity"), root);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QStringLiteral(
        "QLabel{ background: transparent; color: #ffffff; font-weight: 700; font-size: 14px; }"));
    lay->addWidget(titleLbl);

    // ── target card ───────────────────────────────────────────────────────────
    QVBoxLayout* targetLay = nullptr;
    auto* targetCard = makeCard(root, targetLay);
    lay->addWidget(targetCard);
    targetLay->addWidget(makeSectionTitle(tr("Apply To"), targetCard));
    targetLay->addWidget(makeLabel(tr("Choose which frames the opacity change affects."),
                                   targetCard, 10, true));
    targetLay->addWidget(makeSeparator(targetCard));

    mLayerInfoLabel = makeLabel(tr("Layer: —"), targetCard, 11, true);
    targetLay->addWidget(mLayerInfoLabel);

    // scope toggles (radio-like: only one active at a time)
    auto* scopeRow = new QHBoxLayout();
    scopeRow->setSpacing(4);

    mActiveKeyframeToggle = new CosmicCheckboxToggle(targetCard);
    mActiveKeyframeToggle->setMode(CosmicCheckboxToggle::Custom);
    mActiveKeyframeToggle->setBordered(true);
    mActiveKeyframeToggle->setShowIcon(false);
    mActiveKeyframeToggle->setText(tr("Current Frame"));
    mActiveKeyframeToggle->setChecked(true);
    mActiveKeyframeToggle->setFixedHeight(CosmicUiMetrics::controlThickness());
    scopeRow->addWidget(mActiveKeyframeToggle, 1);

    mSelectedKeyframesToggle = new CosmicCheckboxToggle(targetCard);
    mSelectedKeyframesToggle->setMode(CosmicCheckboxToggle::Custom);
    mSelectedKeyframesToggle->setBordered(true);
    mSelectedKeyframesToggle->setShowIcon(false);
    mSelectedKeyframesToggle->setText(tr("Selected"));
    mSelectedKeyframesToggle->setChecked(false);
    mSelectedKeyframesToggle->setFixedHeight(CosmicUiMetrics::controlThickness());
    scopeRow->addWidget(mSelectedKeyframesToggle, 1);

    mActiveLayerToggle = new CosmicCheckboxToggle(targetCard);
    mActiveLayerToggle->setMode(CosmicCheckboxToggle::Custom);
    mActiveLayerToggle->setBordered(true);
    mActiveLayerToggle->setShowIcon(false);
    mActiveLayerToggle->setText(tr("Whole Layer"));
    mActiveLayerToggle->setChecked(false);
    mActiveLayerToggle->setFixedHeight(CosmicUiMetrics::controlThickness());
    scopeRow->addWidget(mActiveLayerToggle, 1);

    targetLay->addLayout(scopeRow);

    // radio-like mutual exclusion
    auto makeExclusive = [this](CosmicCheckboxToggle* toggled) {
        connect(toggled, &CosmicCheckboxToggle::toggled, this, [this, toggled](bool on) {
            if (!on) { toggled->setChecked(true); return; } // prevent deselect
            if (toggled != mActiveKeyframeToggle)    mActiveKeyframeToggle->setChecked(false);
            if (toggled != mSelectedKeyframesToggle) mSelectedKeyframesToggle->setChecked(false);
            if (toggled != mActiveLayerToggle)       mActiveLayerToggle->setChecked(false);
            updateUI();
        });
    };
    makeExclusive(mActiveKeyframeToggle);
    makeExclusive(mSelectedKeyframesToggle);
    makeExclusive(mActiveLayerToggle);

    // ── opacity card ──────────────────────────────────────────────────────────
    QVBoxLayout* opLay = nullptr;
    auto* opCard = makeCard(root, opLay);
    lay->addWidget(opCard);
    opLay->addWidget(makeSectionTitle(tr("Opacity"), opCard));
    opLay->addWidget(makeSeparator(opCard));

    // slider + field row
    auto* opRow = new QHBoxLayout();
    opRow->setSpacing(8);

    mSlider = new QSlider(Qt::Horizontal, opCard);
    mSlider->setRange(0, 500);
    mSlider->setValue(500);
    mSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal{ background: #2a2a2a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal{ background: #b3b3b3; width: 12px; height: 12px;"
        " margin: -4px 0; border-radius: 6px; }"
        "QSlider::sub-page:horizontal{ background: #6a4cff; border-radius: 2px; }"));
    opRow->addWidget(mSlider, 1);

    mOpacityField = new CosmicInputField(opCard);
    mOpacityField->setVariant(CosmicInputField::Minimal);
    mOpacityField->setRange(0.0, 100.0);
    mOpacityField->setDecimals(1);
    mOpacityField->setSingleStep(1.0);
    mOpacityField->setSuffix(QStringLiteral("%"));
    mOpacityField->setValue(100.0);
    mOpacityField->setFixedWidth(80);
    mOpacityField->setFixedHeight(CosmicUiMetrics::controlThickness());
    opRow->addWidget(mOpacityField);

    opLay->addLayout(opRow);

    // ── fade card ─────────────────────────────────────────────────────────────
    QVBoxLayout* fadeLay = nullptr;
    mFadeCard = makeCard(root, fadeLay);
    lay->addWidget(mFadeCard);
    fadeLay->addWidget(makeSectionTitle(tr("Fade"), mFadeCard));
    fadeLay->addWidget(makeLabel(
        tr("Fade opacity across selected frames. Requires 3 or more selected frames."),
        mFadeCard, 10, true));
    fadeLay->addWidget(makeSeparator(mFadeCard));

    auto* fadeRow = new QHBoxLayout();
    fadeRow->setSpacing(8);

    mFadeInBtn = new CosmicButton(tr("Fade In"), mFadeCard);
    mFadeInBtn->setBordered(true);
    mFadeInBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    fadeRow->addWidget(mFadeInBtn, 1);

    mFadeOutBtn = new CosmicButton(tr("Fade Out"), mFadeCard);
    mFadeOutBtn->setBordered(true);
    mFadeOutBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    fadeRow->addWidget(mFadeOutBtn, 1);

    fadeLay->addLayout(fadeRow);

    // ── close row ─────────────────────────────────────────────────────────────
    auto* closeRow = new QHBoxLayout();
    closeRow->addStretch(1);
    auto* closeBtn = new CosmicButton(tr("Close"), root);
    closeBtn->setBordered(true);
    closeBtn->setFixedHeight(CosmicUiMetrics::controlThickness());
    closeRow->addWidget(closeBtn);
    lay->addLayout(closeRow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(root);

    connect(mHeader,   &CosmicWindowHeader::closeRequested, this, &QDialog::close);
    connect(closeBtn,  &QPushButton::clicked,               this, &QDialog::close);
    connect(mFadeInBtn,  &QPushButton::clicked, this, &CosmicLayerOpacityPopup::onFadeIn);
    connect(mFadeOutBtn, &QPushButton::clicked, this, &CosmicLayerOpacityPopup::onFadeOut);
}

// ── public ────────────────────────────────────────────────────────────────────

void CosmicLayerOpacityPopup::setCore(Editor* editor)
{
    mEditor = editor;
    mLayerManager = editor->layers();
    mPlayBack = editor->playback();
}

void CosmicLayerOpacityPopup::initUI()
{
    connect(mSlider, &QSlider::valueChanged, this, &CosmicLayerOpacityPopup::onSliderChanged);
    connect(mOpacityField, &CosmicInputField::valueChanged,
            this, &CosmicLayerOpacityPopup::onFieldChanged);
    connect(mFadeInBtn,  &QPushButton::clicked, this, &CosmicLayerOpacityPopup::onFadeIn);
    connect(mFadeOutBtn, &QPushButton::clicked, this, &CosmicLayerOpacityPopup::onFadeOut);

    connect(mEditor,       &Editor::objectLoaded,          this, &CosmicLayerOpacityPopup::onObjectLoaded);
    connect(mEditor,       &Editor::scrubbed,              this, &CosmicLayerOpacityPopup::onCurrentFrameChanged);
    connect(mEditor,       &Editor::selectedFramesChanged, this, &CosmicLayerOpacityPopup::onSelectedFramesChanged);
    connect(mPlayBack,     &PlaybackManager::playStateChanged, this, &CosmicLayerOpacityPopup::onPlayStateChanged);
    connect(mLayerManager, &LayerManager::currentLayerChanged, this, &CosmicLayerOpacityPopup::onCurrentLayerChanged);

    onObjectLoaded();
}

void CosmicLayerOpacityPopup::updateUI()
{
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;

    if (mLayerInfoLabel)
        mLayerInfoLabel->setText(tr("Layer: %1").arg(layer->name()));

    if (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR) {
        setCanAdjust(false, false);
        return;
    }

    bool canAdjust = false;
    if (mActiveKeyframeToggle && mActiveKeyframeToggle->isChecked()) {
        canAdjust = layer->getLastKeyFrameAtPosition(mEditor->currentFrame()) != nullptr;
    } else if (mSelectedKeyframesToggle && mSelectedKeyframesToggle->isChecked()) {
        canAdjust = !layer->getSelectedFramesByPos().isEmpty();
    } else if (mActiveLayerToggle && mActiveLayerToggle->isChecked()) {
        canAdjust = true;
    }

    updateSelectedFramesUI();
    setCanAdjust(canAdjust, false);
}

// ── slots ─────────────────────────────────────────────────────────────────────

void CosmicLayerOpacityPopup::onSliderChanged(int value)
{
    const double v = value * mSpinBoxMultiplier;
    if (mOpacityField) {
        QSignalBlocker b(mOpacityField);
        mOpacityField->setValue(v);
    }
    opacityValueChanged();
}

void CosmicLayerOpacityPopup::onFieldChanged(double value)
{
    if (mSlider) {
        QSignalBlocker b(mSlider);
        mSlider->setValue(static_cast<int>(value * 5.0));
    }
    opacityValueChanged();
}

void CosmicLayerOpacityPopup::onFadeIn()  { fade(OpacityFadeType::IN); }
void CosmicLayerOpacityPopup::onFadeOut() { fade(OpacityFadeType::OUT); }

void CosmicLayerOpacityPopup::onObjectLoaded()
{
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    if (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR) return;
    KeyFrame* kf = layer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    updateValues(kf ? getOpacityForKeyFrame(layer, kf) : 1.0);
    updateUI();
}

void CosmicLayerOpacityPopup::onCurrentLayerChanged(int)
{
    onCurrentFrameChanged(mEditor->currentFrame());
    updateUI();
}

void CosmicLayerOpacityPopup::onCurrentFrameChanged(int frame)
{
    if (mPlayerIsPlaying) return;
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    if (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR) {
        setCanAdjust(false, false); return;
    }
    KeyFrame* kf = layer->getLastKeyFrameAtPosition(frame);
    if (kf) updateValues(getOpacityForKeyFrame(layer, kf));
    updateUI();
}

void CosmicLayerOpacityPopup::onSelectedFramesChanged() { updateUI(); }

void CosmicLayerOpacityPopup::onPlayStateChanged(bool isPlaying)
{
    mPlayerIsPlaying = isPlaying;
    if (!mPlayerIsPlaying) onCurrentFrameChanged(mEditor->currentFrame());
    updateUI();
}

// ── private helpers ───────────────────────────────────────────────────────────

void CosmicLayerOpacityPopup::setCanAdjust(bool opacity, bool fade)
{
    if (mFadeCard) mFadeCard->setEnabled(fade);
    if (mSlider)   mSlider->setEnabled(opacity);
    if (mOpacityField) mOpacityField->setEnabled(opacity);
}

void CosmicLayerOpacityPopup::updateSelectedFramesUI()
{
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    const int count = layer->getSelectedFramesByPos().count();
    const bool enough = count >= mMinSelectedFrames;
    if (mFadeCard) mFadeCard->setEnabled(enough);
    if (mSelectedKeyframesToggle) mSelectedKeyframesToggle->setEnabled(enough);
}

void CosmicLayerOpacityPopup::updateValues(qreal opacity)
{
    const int sliderVal = static_cast<int>(opacity * mMultiplier);
    if (mSlider) { QSignalBlocker b(mSlider); mSlider->setValue(sliderVal); }
    if (mOpacityField) {
        QSignalBlocker b(mOpacityField);
        mOpacityField->setValue(sliderVal * mSpinBoxMultiplier);
    }
}

void CosmicLayerOpacityPopup::opacityValueChanged()
{
    if (mActiveKeyframeToggle && mActiveKeyframeToggle->isChecked())
        setOpacityForCurrentKeyframe();
    else if (mActiveLayerToggle && mActiveLayerToggle->isChecked())
        setOpacityForLayer();
    else if (mSelectedKeyframesToggle && mSelectedKeyframesToggle->isChecked())
        setOpacityForSelectedKeyframes();
}

qreal CosmicLayerOpacityPopup::getOpacityForKeyFrame(Layer* layer, const KeyFrame* kf) const
{
    if (layer->type() == Layer::BITMAP)
        return static_cast<const BitmapImage*>(kf)->getOpacity();
    if (layer->type() == Layer::VECTOR)
        return static_cast<const VectorImage*>(kf)->getOpacity();
    return -1.0;
}

void CosmicLayerOpacityPopup::setOpacityForKeyFrame(Layer* layer, KeyFrame* kf, qreal opacity)
{
    if (layer->type() == Layer::BITMAP) {
        auto* bmp = static_cast<BitmapImage*>(kf);
        bmp->setOpacity(opacity);
        layer->markFrameAsDirty(bmp->pos());
    } else if (layer->type() == Layer::VECTOR) {
        auto* vec = static_cast<VectorImage*>(kf);
        vec->setOpacity(opacity);
        layer->markFrameAsDirty(vec->pos());
    }
}

void CosmicLayerOpacityPopup::setOpacityForCurrentKeyframe()
{
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    if (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR) return;
    KeyFrame* kf = layer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    if (!kf) return;
    const qreal opacity = mSlider ? mSlider->value() / mMultiplier : 1.0;
    setOpacityForKeyFrame(layer, kf, opacity);
    emit mEditor->framesModified();
}

void CosmicLayerOpacityPopup::setOpacityForSelectedKeyframes()
{
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    const qreal opacity = mSlider ? mSlider->value() / mMultiplier : 1.0;
    for (int pos : layer->getSelectedFramesByPos()) {
        KeyFrame* kf = layer->getKeyFrameAt(pos);
        if (kf) setOpacityForKeyFrame(layer, kf, opacity);
    }
    emit mEditor->framesModified();
}

void CosmicLayerOpacityPopup::setOpacityForLayer()
{
    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    const qreal opacity = mSlider ? mSlider->value() / mMultiplier : 1.0;
    layer->foreachKeyFrame([this, layer, opacity](KeyFrame* kf) {
        setOpacityForKeyFrame(layer, kf, opacity);
    });
    emit mEditor->framesModified();
}

void CosmicLayerOpacityPopup::fade(OpacityFadeType type)
{
    QSignalBlocker b1(mSlider);
    QSignalBlocker b2(mOpacityField);

    Layer* layer = mLayerManager->currentLayer();
    if (!layer) return;
    if (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR) return;

    QList<int> keys = layer->getSelectedFramesByPos();
    if (keys.count() < mMinSelectedFrames) return;

    KeyFrame* first = layer->getLastKeyFrameAtPosition(keys.first());
    if (!first) return;

    qreal initial = getOpacityForKeyFrame(layer, first);
    const qreal count = static_cast<qreal>(keys.count() - 1);
    qreal step = 0.0;

    if (type == OpacityFadeType::IN) {
        if (initial >= 1.0) initial = 0.0;
        step = (1.0 - initial) / count;
    } else {
        if (initial <= 0.0) initial = 1.0;
        step = initial / count;
    }

    for (int i = 0; i < keys.count(); i++) {
        KeyFrame* kf = layer->getLastKeyFrameAtPosition(keys.at(i));
        if (!kf) continue;
        const qreal newOp = (type == OpacityFadeType::IN)
            ? initial + i * step
            : initial - i * step;
        setOpacityForKeyFrame(layer, kf, newOp);
    }

    KeyFrame* cur = layer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    if (cur) updateValues(getOpacityForKeyFrame(layer, cur));
    emit mEditor->framesModified();
}

// ── showEvent / closeEvent ────────────────────────────────────────────────────

void CosmicLayerOpacityPopup::showEvent(QShowEvent* e)
{
    CosmicPopupWindow::showEvent(e);

    clearFixedWindowSize();
    setMinimumWidth(340);
    setMaximumWidth(340);
    adjustSize();

    setWindowOpacity(0.0);
    const QRect g0 = geometry();
    const QPoint c = g0.center();
    const QSize  s = g0.size();
    const QSize  s0(qMax(1, int(s.width() * 0.93)), qMax(1, int(s.height() * 0.93)));
    const QRect  g1(QPoint(c.x() - s0.width()/2, c.y() - s0.height()/2), s0);
    setGeometry(g1);

    auto* op = new QPropertyAnimation(this, "windowOpacity", this);
    op->setDuration(80); op->setStartValue(0.0); op->setEndValue(1.0);
    auto* gm = new QPropertyAnimation(this, "geometry", this);
    gm->setDuration(95); gm->setStartValue(g1); gm->setEndValue(g0);
    auto* grp = new QParallelAnimationGroup(this);
    grp->addAnimation(op); grp->addAnimation(gm);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

void CosmicLayerOpacityPopup::closeEvent(QCloseEvent* e)
{
    CosmicPopupWindow::closeEvent(e);
}
