#ifndef CANVASKEYFRAMESCRUBBER_H
#define CANVASKEYFRAMESCRUBBER_H

#include <QWidget>
#include <QList>
#include <QPoint>

class Editor;

/**
 * @brief Top-center canvas overlay pill for keyframe scrubbing.
 *
 * Click: snaps to the keyframe that covers the current frame (hold-frame → keyframe).
 * Drag left/right: flips through keyframes on the active layer.
 *   - Each flip requires kStepPx pixels of horizontal movement (pixel-based, device-agnostic).
 *   - Dragging past the first or last keyframe stops — no looping.
 * Release: restores the frame that was active before the scrub began.
 *
 * The pill renders a glowing accent-coloured grid with edge fades.
 * A small centre tick gives visual feedback during the drag.
 */
class CanvasKeyframeScrubber : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasKeyframeScrubber(Editor* editor, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    // Snap to the keyframe that covers the given frame (resolves hold frames).
    // Returns the keyframe position, or -1 if the layer has no keyframes.
    int resolveKeyframe(int frame) const;

    // Collect all keyframe positions on the active layer, sorted ascending.
    QList<int> keyframePositions() const;

    Editor* mEditor       = nullptr;

    // Drag state
    bool   mDragging      = false;
    int    mDragStartX    = 0;       // global X at press; never changes during the drag
    int    mStartIndex    = 0;       // keyframe index at press; never changes during the drag
    QPoint mCursorLockPos;           // global pos where drag started; cursor warps back here
    int    mPreScrubFrame = -1;      // frame to restore on release
    int    mScrubFrame    = -1;      // current keyframe shown during scrub

    // Pixels of horizontal drag per keyframe step.
    // Total displacement from press divided by this gives the target index offset.
    // No accumulation — the mapping is always absolute from the drag origin.
    static constexpr int kStepPx = 50;
};

#endif // CANVASKEYFRAMESCRUBBER_H
