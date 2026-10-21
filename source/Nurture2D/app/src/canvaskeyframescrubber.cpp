#include "canvaskeyframescrubber.h"

#include "pencildef.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QMouseEvent>
#include <QSettings>
#include <QList>
#include <QCursor>
#include <QGuiApplication>
#include <algorithm>

#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "layer.h"
#include "keyframe.h"

CanvasKeyframeScrubber::CanvasKeyframeScrubber(Editor* editor, QWidget* parent)
    : QWidget(parent)
    , mEditor(editor)
{
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::SizeHorCursor);
    setFixedSize(260, 32);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QList<int> CanvasKeyframeScrubber::keyframePositions() const
{
    if (!mEditor) return {};
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer) return {};

    QList<int> positions;
    layer->foreachKeyFrame([&](KeyFrame* kf) {
        positions << kf->pos();
    });
    std::sort(positions.begin(), positions.end());
    return positions;
}

int CanvasKeyframeScrubber::resolveKeyframe(int frame) const
{
    if (!mEditor) return -1;
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer) return -1;

    KeyFrame* kf = layer->getLastKeyFrameAtPosition(frame);
    if (!kf) {
        if (layer->keyFrameCount() == 0) return -1;
        return layer->firstKeyFramePosition();
    }
    return kf->pos();
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void CanvasKeyframeScrubber::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || !mEditor) {
        QWidget::mousePressEvent(e);
        return;
    }

    if (mEditor->playback()->isPlaying()) {
        QWidget::mousePressEvent(e);
        return;
    }

    const QList<int> keys = keyframePositions();
    if (keys.isEmpty()) {
        QWidget::mousePressEvent(e);
        return;
    }

    mPreScrubFrame = mEditor->currentFrame();
    mDragging      = true;
    mDragStartX    = e->globalPos().x();

    // Record which keyframe index we start on
    int snapped = resolveKeyframe(mPreScrubFrame);
    if (snapped < 1) snapped = keys.first();
    mScrubFrame  = snapped;
    mStartIndex  = keys.indexOf(snapped);
    if (mStartIndex < 0) mStartIndex = 0;

    if (mScrubFrame != mPreScrubFrame)
        mEditor->scrubTo(mScrubFrame);

    mEditor->setTimelineScrubbing(true);
    grabMouse();

    update();
    e->accept();
}

void CanvasKeyframeScrubber::mouseMoveEvent(QMouseEvent* e)
{
    if (!mDragging || !mEditor) {
        QWidget::mouseMoveEvent(e);
        return;
    }

    const QList<int> keys = keyframePositions();
    if (keys.size() < 2) {
        e->accept();
        return;
    }

    // Total displacement from the press point — absolute, stable, no warp needed.
    // Dividing by kStepPx gives exactly how many keyframes away from the start
    // index we should be right now. This is a direct 1:1 mapping, like a jog dial.
    const int totalDelta = e->globalPos().x() - mDragStartX;
    const int offset     = totalDelta / kStepPx;   // truncates toward zero — correct for both signs
    const int newIdx     = qBound(0, mStartIndex + offset, keys.size() - 1);

    if (keys[newIdx] != mScrubFrame) {
        mScrubFrame = keys[newIdx];
        mEditor->scrubTo(mScrubFrame);
        update();
    }

    e->accept();
}

void CanvasKeyframeScrubber::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || !mDragging) {
        QWidget::mouseReleaseEvent(e);
        return;
    }

    mDragging = false;
    releaseMouse();

    if (mEditor) {
        mEditor->setTimelineScrubbing(false);
        if (mPreScrubFrame >= 1)
            mEditor->scrubTo(mPreScrubFrame);
    }

    mPreScrubFrame = -1;
    mScrubFrame    = -1;
    update();
    e->accept();
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void CanvasKeyframeScrubber::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r      = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal  radius = r.height() / 2.0;

    // ── Pill background ─────────────────────────────────────────────────────
    const QColor bgColor    (0x1f, 0x1f, 0x1f, 0xff);
    const QColor borderColor(0x55, 0x55, 0x55, 0xff);

    QPainterPath pill;
    pill.addRoundedRect(r, radius, radius);

    p.setClipPath(pill);
    p.fillPath(pill, bgColor);

    // ── Accent colour ────────────────────────────────────────────────────────
    QSettings settings(PENCIL2D, PENCIL2D);
    const QString accentHex = settings.value(
        QStringLiteral("CosmicAccentColor"), QStringLiteral("#6a4cff")).toString();
    QColor accent(accentHex);
    if (!accent.isValid()) accent = QColor(0x6a, 0x4c, 0xff);

    // ── Grid lines ───────────────────────────────────────────────────────────
    const int vSpacing = 16;
    const int hSpacing = 10;
    const int x0 = static_cast<int>(r.left());
    const int y0 = static_cast<int>(r.top());
    const int iW = static_cast<int>(r.width());
    const int iH = static_cast<int>(r.height());

    QColor gridColor = accent;
    gridColor.setAlpha(90);
    p.setPen(QPen(gridColor, 1.0));

    for (int x = x0 + vSpacing - (x0 % vSpacing); x < x0 + iW; x += vSpacing)
        p.drawLine(x, y0, x, y0 + iH);
    for (int y = y0 + hSpacing - (y0 % hSpacing); y < y0 + iH; y += hSpacing)
        p.drawLine(x0, y, x0 + iW, y);

    // ── Edge fades ───────────────────────────────────────────────────────────
    const int hFadeW = static_cast<int>(radius) + 24;
    const int vFadeH = 10;

    auto fillGrad = [&](const QRectF& rect, const QLinearGradient& g) {
        p.fillRect(rect, g);
    };

    // Left
    { QLinearGradient g(r.left(), 0, r.left() + hFadeW, 0);
      g.setColorAt(0.0, bgColor); g.setColorAt(1.0, Qt::transparent);
      fillGrad(QRectF(r.left(), r.top(), hFadeW, r.height()), g); }
    // Right
    { QLinearGradient g(r.right(), 0, r.right() - hFadeW, 0);
      g.setColorAt(0.0, bgColor); g.setColorAt(1.0, Qt::transparent);
      fillGrad(QRectF(r.right() - hFadeW, r.top(), hFadeW, r.height()), g); }
    // Top
    { QLinearGradient g(0, r.top(), 0, r.top() + vFadeH);
      g.setColorAt(0.0, bgColor); g.setColorAt(1.0, Qt::transparent);
      fillGrad(QRectF(r.left(), r.top(), r.width(), vFadeH), g); }
    // Bottom
    { QLinearGradient g(0, r.bottom(), 0, r.bottom() - vFadeH);
      g.setColorAt(0.0, bgColor); g.setColorAt(1.0, Qt::transparent);
      fillGrad(QRectF(r.left(), r.bottom() - vFadeH, r.width(), vFadeH), g); }

    // ── Centre glow bloom ────────────────────────────────────────────────────
    { QRadialGradient glow(r.center(), r.width() * 0.45);
      QColor gi = accent; gi.setAlpha(38);
      QColor go = accent; go.setAlpha(0);
      glow.setColorAt(0.0, gi); glow.setColorAt(1.0, go);
      p.fillRect(r, glow); }

    // ── Active-drag indicator: centre tick brightens during scrub ────────────
    if (mDragging) {
        const qreal cx = r.center().x();
        const qreal ty = r.top()    + vFadeH;
        const qreal by = r.bottom() - vFadeH;
        QColor tickColor = accent;
        tickColor.setAlpha(200);
        p.setPen(QPen(tickColor, 2.0));
        p.drawLine(QPointF(cx, ty), QPointF(cx, by));
    }

    // ── Border ───────────────────────────────────────────────────────────────
    p.setClipping(false);
    p.setPen(QPen(borderColor, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(pill);
}
