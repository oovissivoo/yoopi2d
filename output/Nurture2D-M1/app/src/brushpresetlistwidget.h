#ifndef BRUSHPRESETLISTWIDGET_H
#define BRUSHPRESETLISTWIDGET_H

#include <QWidget>
#include <QVector>
#include "tooloptionwidget.h"

class QScrollArea;
class QVBoxLayout;
class QButtonGroup;
class QPainter;
class QMouseEvent;
class QParallelAnimationGroup;
class QScrollBar;
class QLineEdit;
class CosmicBrushSettings;

class BrushPresetItem : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal slideOffset READ slideOffset WRITE setSlideOffset)

public:
    explicit BrushPresetItem(const ToolOptionWidget::BrushPreset& preset, int index, QWidget* parent = nullptr);

    void setFinalLineMode(bool v) { mFinalLineMode = v; update(); }
    bool isFinalLineMode() const { return mFinalLineMode; }

    void setName(const QString& name);
    QString name() const { return mName; }

    void setPreset(const ToolOptionWidget::BrushPreset& preset);
    const ToolOptionWidget::BrushPreset& preset() const { return mPreset; }
    
    void setActive(bool active);
    bool isActive() const { return mIsActive; }

    int index() const { return mIndex; }
    void setIndex(int index) { mIndex = index; }

    qreal slideOffset() const { return mSlideOffset; }
    void setSlideOffset(qreal v);

    int actionsWidth() const;
    bool isBuiltIn() const { return mBuiltIn; }
    void startInlineRename();
    void cancelInlineRename();
    bool isRenaming() const;

signals:
    void selected(int index);
    void settingsRequested(int index);
    void dragStarted(int index);
    void deleteRequested(int index);
    void duplicateRequested(int index);
    void slideClosed(int index);
    void slideStateRequested(int index, bool open);
    void renameRequested(int index, const QString& newName);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateLayout();
    QRect settingsButtonRect() const;
    QRect previewRect() const;
    QRect actionsRect() const;
    QRect deleteRect() const;
    QRect duplicateRect() const;
    QRect closeRect() const;
    void setSlideOpen(bool open, bool animated);

    QString mName;
    int mIndex;
    bool mBuiltIn = false;
    bool mIsActive = false;
    bool mFinalLineMode = false;
    ToolOptionWidget::BrushPreset mPreset;
    QPoint mDragStartPos;
    QPoint mPressPos;
    bool mSwipeTracking = false;
    bool mSwipeDecided = false;
    bool mSwipeIsHorizontal = false;
    qreal mSlideOffset = 0.0;
    qreal mSlideOffsetAtPress = 0.0;
    bool mPressConsumedByAction = false;
    bool mDragStarted = false;
    QLineEdit* mRenameEdit = nullptr;
    bool mRenameMode = false;

    QRect mIconRect;
    QRect mTextRect;
    QRect mPreviewRect;
    QRect mSettingsRect;
};

class BrushPresetListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BrushPresetListWidget(QWidget* parent = nullptr);
    ~BrushPresetListWidget() override;

    void setPresets(const QVector<ToolOptionWidget::BrushPreset>& presets);
    void setActivePreset(int index);
    int activePreset() const { return mActivePresetIndex; }

    void setFinalLineActive(bool active);
    bool isFinalLineActive() const { return mFinalLineActive; }

    CosmicBrushSettings* settingsPopup() const { return mSettingsPopup; }

signals:
    void presetSelected(int index);
    void presetSettingsRequested(int index);
    void presetOrderChanged(const QVector<int>& newOrder);
    void presetsChanged(const QVector<ToolOptionWidget::BrushPreset>& presets, int activeIndex);
    // Emitted when the Final Line settings button is tapped, so the caller can seed live pen tool state.
    void finalLineSettingsRequested();
    // Emitted whenever Final Line pressure toggle or curve changes (forwarded from popup).
    void finalLinePressureChanged(bool enabled, const QVector<QPointF>& curvePoints);

private slots:
    void onPresetSelected(int index);
    void onPresetSettingsRequested(int index);
    void onPresetDeleteRequested(int index);
    void onPresetDuplicateRequested(int index);
    void onPresetSlideClosed(int index);
    void onPresetSlideStateRequested(int index, bool open);
    void onPresetRenameRequested(int index, const QString& newName);
    void startDrag(int index);

private:
    void updateUI();
    void loadPresets();
    void ensureSettingsPopup();
    void updateSettingsPopupForActivePreset();
    void updateDrag(const QPoint& globalPos);
    void endDrag();
    int getDropIndex(const QPoint& globalPos) const;
    void applyActiveSlideState();
    QVector<int> generateCurrentOrder() const;

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    QScrollArea* mScrollArea = nullptr;
    QScrollBar* mScrollSlider = nullptr;
    BrushPresetItem* mFinalLineItem = nullptr;
    QWidget* mContainerWidget = nullptr;
    QVBoxLayout* mContainerLayout = nullptr;
    QButtonGroup* mButtonGroup = nullptr;

    QVector<ToolOptionWidget::BrushPreset> mPresets;
    QVector<BrushPresetItem*> mPresetItems;
    int mActivePresetIndex = 0;
    bool mFinalLineActive = false;
    bool mShowActionsForActive = false;

    // Drag reorder state
    bool mIsDragging = false;
    int mDragFromIndex = -1;  // which preset is being dragged
    int mDropIndex = -1;      // insertion slot (0..n), shown as accent line between rows

    // Transparent overlay widget that draws the drop indicator line on top of the viewport.
    QWidget* mDropIndicator = nullptr;

    // Settings popup
    CosmicBrushSettings* mSettingsPopup = nullptr;
};

#endif // BRUSHPRESETLISTWIDGET_H
