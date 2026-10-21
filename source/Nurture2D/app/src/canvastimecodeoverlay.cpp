#include "canvastimecodeoverlay.h"

#include <QPainter>
#include <QPainterPath>

#include "editor.h"
#include "preferencemanager.h"
#include "preferencesdef.h"
#include "pencildef.h"

CanvasTimecodeOverlay::CanvasTimecodeOverlay(Editor* editor, QWidget* parent)
    : QWidget(parent)
    , mEditor(editor)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);

    if (mEditor) {
        // Sync initial state from preferences
        mMode         = mEditor->preference()->getInt(SETTING::TIMECODE_TEXT);
        mFps          = mEditor->fps();
        mCurrentFrame = mEditor->currentFrame();

        connect(mEditor, &Editor::scrubbed,
                this, &CanvasTimecodeOverlay::onFrameChanged);

        connect(mEditor->preference(), &PreferenceManager::optionChanged,
                this, [this](SETTING s) {
                    if (s == SETTING::TIMECODE_TEXT) {
                        setTimecodeMode(mEditor->preference()->getInt(SETTING::TIMECODE_TEXT));
                    }
                });

        connect(mEditor, &Editor::fpsChanged,
                this, &CanvasTimecodeOverlay::setFps);
    }

    syncSizeToMode();
}

// ---------------------------------------------------------------------------

void CanvasTimecodeOverlay::onFrameChanged(int frame)
{
    mCurrentFrame = frame;
    update();
}

void CanvasTimecodeOverlay::setFps(int fps)
{
    mFps = qMax(1, fps);
    update();
}

void CanvasTimecodeOverlay::setTimecodeMode(int mode)
{
    mMode = mode;
    syncSizeToMode();
    update();
}

// ---------------------------------------------------------------------------

void CanvasTimecodeOverlay::syncSizeToMode()
{
    if (mMode == NOTEXT) {
        hide();
        return;
    }
    show();

    // Size the pill to fit the widest possible text for each mode
    // SMPTE "MM:SS:FF" needs more room than FRAMES "0000"
    int w = 72;
    switch (mMode) {
    case FRAMES: w = 60;  break;
    case SFF:    w = 72;  break;
    case SMPTE:  w = 96;  break;
    default:     w = 72;  break;
    }
    setFixedSize(w, 28);
}

// ---------------------------------------------------------------------------

QString CanvasTimecodeOverlay::formatFrame(int frame) const
{
    const int fps = qMax(1, mFps);
    switch (mMode) {
    case SMPTE:
        return QString("%1:%2:%3")
            .arg(frame / (60 * fps) % 60, 2, 10, QLatin1Char('0'))
            .arg(frame / fps % 60,        2, 10, QLatin1Char('0'))
            .arg(frame % fps,             2, 10, QLatin1Char('0'));
    case SFF:
        return QString("%1:%2")
            .arg(frame / fps)
            .arg(frame % fps, 2, 10, QLatin1Char('0'));
    case FRAMES:
        return QString::number(frame).rightJustified(4, QLatin1Char('0'));
    default:
        return QString();
    }
}

// ---------------------------------------------------------------------------

void CanvasTimecodeOverlay::paintEvent(QPaintEvent*)
{
    if (mMode == NOTEXT) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r      = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal  radius = r.height() / 2.0;

    // Pill background #1f1f1fff
    const QColor bgColor    (0x1f, 0x1f, 0x1f, 0xff);
    // Lighter border
    const QColor borderColor(0x55, 0x55, 0x55, 0xff);
    // CosmicYellow #ffb21aff
    const QColor yellow     (0xff, 0xb2, 0x1a, 0xff);

    QPainterPath pill;
    pill.addRoundedRect(r, radius, radius);

    p.fillPath(pill, bgColor);
    p.setPen(QPen(borderColor, 1.0));
    p.drawPath(pill);

    const QString text = formatFrame(mCurrentFrame);

    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(12);
    p.setFont(font);
    p.setPen(yellow);
    p.drawText(r, Qt::AlignCenter, text);
}
