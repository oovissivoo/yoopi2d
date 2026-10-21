#ifndef CANVASTIMECODEOVERLAY_H
#define CANVASTIMECODEOVERLAY_H

#include <QWidget>
#include "pencildef.h"   // TimecodeTextLevel: NOTEXT, FRAMES, SMPTE, SFF

class Editor;

/**
 * @brief Pill-shaped timecode overlay at the top-left of the canvas.
 *
 * Mirrors the timecode label from the timeline bottom panel — same format
 * modes (NOTEXT / FRAMES / SMPTE / SFF), same FPS, same frame source.
 * Hides itself when the mode is NOTEXT.
 */
class CanvasTimecodeOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasTimecodeOverlay(Editor* editor, QWidget* parent = nullptr);

public slots:
    void onFrameChanged(int frame);
    void setFps(int fps);
    void setTimecodeMode(int mode);   // accepts TimecodeTextLevel int

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString formatFrame(int frame) const;
    void    syncSizeToMode();

    Editor* mEditor       = nullptr;
    int     mCurrentFrame = 1;
    int     mFps          = 12;
    int     mMode         = 3;   // default SFF (matches preferencemanager default)
};

#endif // CANVASTIMECODEOVERLAY_H
