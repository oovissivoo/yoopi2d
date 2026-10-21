#ifndef COSMICLAYEROPACITYPOPUP_H
#define COSMICLAYEROPACITYPOPUP_H

#include "cosmicpopupwindow.h"
#include "editor.h"
#include "layeropacitydialog.h"
#include <QPointer>

class CosmicWindowHeader;
class CosmicButton;
class CosmicInputField;
class CosmicCheckboxToggle;
class QLabel;
class QSlider;
class Layer;
class KeyFrame;
class LayerManager;
class PlaybackManager;

// Redesigned Layer Opacity popup.
// Cards: Target | Opacity | Fade
class CosmicLayerOpacityPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicLayerOpacityPopup(QWidget* parent = nullptr);
    ~CosmicLayerOpacityPopup() override;

    void setCore(Editor* editor);
    void initUI();
    void updateUI();

protected:
    void showEvent(QShowEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onSliderChanged(int value);
    void onFieldChanged(double value);
    void onFadeIn();
    void onFadeOut();
    void onObjectLoaded();
    void onCurrentLayerChanged(int index);
    void onCurrentFrameChanged(int frame);
    void onSelectedFramesChanged();
    void onPlayStateChanged(bool isPlaying);

private:
    void buildUI();
    void setCanAdjust(bool opacity, bool fade);
    void fade(OpacityFadeType type);
    qreal getOpacityForKeyFrame(Layer* layer, const KeyFrame* kf) const;
    void  setOpacityForKeyFrame(Layer* layer, KeyFrame* kf, qreal opacity);
    void  updateValues(qreal opacity);
    void  opacityValueChanged();
    void  setOpacityForCurrentKeyframe();
    void  setOpacityForSelectedKeyframes();
    void  setOpacityForLayer();
    void  updateSelectedFramesUI();

    Editor*          mEditor = nullptr;
    LayerManager*    mLayerManager = nullptr;
    PlaybackManager* mPlayBack = nullptr;
    bool             mPlayerIsPlaying = false;

    const qreal mMultiplier = 500.0;
    const qreal mSpinBoxMultiplier = 0.2;
    const int   mMinSelectedFrames = 3;

    QPointer<CosmicWindowHeader>   mHeader;
    QPointer<QLabel>               mLayerInfoLabel;
    QPointer<QSlider>              mSlider;
    QPointer<CosmicInputField>     mOpacityField;
    // Target scope toggles (radio-like)
    QPointer<CosmicCheckboxToggle> mActiveKeyframeToggle;
    QPointer<CosmicCheckboxToggle> mSelectedKeyframesToggle;
    QPointer<CosmicCheckboxToggle> mActiveLayerToggle;
    // Fade buttons
    QPointer<CosmicButton>         mFadeInBtn;
    QPointer<CosmicButton>         mFadeOutBtn;
    QPointer<QWidget>              mFadeCard;
};

#endif // COSMICLAYEROPACITYPOPUP_H
