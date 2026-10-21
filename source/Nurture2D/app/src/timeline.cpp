/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "timeline.h"

#include <QWidget>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QMessageBox>
#include <QLabel>
#include <QWheelEvent>
#include <QGridLayout>
#include <QTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QSvgRenderer>
#include <QPainter>
#include <QDebug>
#include <QApplication>
#include <QScreen>
#include <QFrame>
#include <QTabBar>
#include <QStackedWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QFontMetrics>
#include <QImage>
#include <QFile>
#include <QPixmapCache>
#include <QHash>
#include <QVector>
#include <QFrame>
#include <QSignalBlocker>

#include "editor.h"
#include "object.h"

#include "scenesequencemanager.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "preferencesdef.h"
#include "cellbank.h"
#include "layer.h"
#include "keyframe.h"
#include "layercamera.h"
#include "../CUI/cosmicuimetrics.h"
#include "cosmicwarningpopup.h"
#include "layermanager.h"
#include "managers/shifttracemanager.h"
#include "timecontrols.h"
#include "timelinecells.h"
#include "pencilsettings.h"
#include "pencildef.h"
#include "timelinesettingspopup.h"
#include "windowsscrollbarspage.h"
#include "scenesequencemanager.h"
#include "scribblearea.h"
#include "flowlayout.h"
#include "cellstackwidget.h"
#include "../CUI/cosmichslider.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/blenderstyletoggle.h"
#include "../CUI/blenderstylebutton.h"


TimeLine::TimeLine(QWidget* parent) : BaseDockWidget(parent)
{
}

TimeLine::~TimeLine()
{
    // mSettingsDialog has nullptr parent so Qt won't auto-delete it
    delete mSettingsDialog;
    mSettingsDialog = nullptr;
}

void TimeLine::applyScrollbarSettings(int hMode, int vMode)
{
    // hMode: 0=Bottom only, 1=Top only, 2=Both
    if (mHScrollbar)    mHScrollbar->setVisible(hMode == 0 || hMode == 2);
    if (mHScrollbarTop) mHScrollbarTop->setVisible(hMode == 1 || hMode == 2);

    // Keep the left-panel spacer in sync with the top scrollbar so that
    // mLayerList and mTracks always start at the same vertical position.
    if (mLeftTopSpacer) mLeftTopSpacer->setVisible(hMode == 1 || hMode == 2);

    // vMode: 0=Left only, 1=Right only, 2=Both
    if (mVScrollbar)      mVScrollbar->setVisible(vMode == 0 || vMode == 2);
    if (mVScrollbarRight) mVScrollbarRight->setVisible(vMode == 1 || vMode == 2);
}

bool TimeLine::isScenesAndShotsTabActive() const
{
    return mActiveTabIndex == 1;
}

static QPixmap renderSceneSvgWithColors(const QSize& size, const QColor& fill, const QColor& stroke, double strokeWidthPx)
{
    const QString key = QStringLiteral("SceneSvg_%1x%2_%3_%4_%5")
                            .arg(size.width())
                            .arg(size.height())
                            .arg(fill.name(QColor::HexRgb))
                            .arg(stroke.name(QColor::HexRgb))
                            .arg(strokeWidthPx, 0, 'f', 2);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) {
        return cached;
    }

    QFile f(QStringLiteral(":/Interaction/Scene.svg"));
    if (!f.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QByteArray data = f.readAll();
    const QByteArray fillHex = fill.name(QColor::HexRgb).toUtf8();
    const QByteArray strokeHex = stroke.name(QColor::HexRgb).toUtf8();
    data.replace("fill:#1f1f1f", QByteArray("fill:") + fillHex);
    data.replace("stroke:#505050", QByteArray("stroke:") + strokeHex);

    if (strokeWidthPx > 0.0)
    {
        constexpr double kSceneSvgViewBox = 25.4;
        const double refPx = qMax(1, qMin(size.width(), size.height()));
        const double strokeUnits = (strokeWidthPx * kSceneSvgViewBox) / refPx;
        const QByteArray newStrokeStr = QByteArray("stroke-width:") + QByteArray::number(strokeUnits, 'f', 5);
        data.replace("stroke-width:1.89159", newStrokeStr);
    }

    QSvgRenderer renderer;
    if (!renderer.load(data) || !renderer.isValid()) {
        return QPixmap();
    }

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p, QRectF(0, 0, size.width(), size.height()));
    }
    const QPixmap pm = QPixmap::fromImage(img);
    QPixmapCache::insert(key, pm);
    return pm;
}

static QPixmap renderSequenceSvgWithColors(const QSize& size, const QColor& fill, const QColor& stroke, double strokeWidthPx)
{
    const QString key = QStringLiteral("SequenceSvg_%1x%2_%3_%4_%5")
                            .arg(size.width())
                            .arg(size.height())
                            .arg(fill.name(QColor::HexRgb))
                            .arg(stroke.name(QColor::HexRgb))
                            .arg(strokeWidthPx, 0, 'f', 2);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) {
        return cached;
    }

    QFile f(QStringLiteral(":/Interaction/Sequence.svg"));
    if (!f.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QByteArray data = f.readAll();
    const QByteArray fillHex = fill.name(QColor::HexRgb).toUtf8();
    const QByteArray strokeHex = stroke.name(QColor::HexRgb).toUtf8();
    data.replace("fill:#1f1f1f", QByteArray("fill:") + fillHex);
    data.replace("stroke:#505050", QByteArray("stroke:") + strokeHex);

    if (strokeWidthPx > 0.0)
    {
        constexpr double kSequenceSvgViewBox = 25.4;
        const double refPx = qMax(1, qMin(size.width(), size.height()));
        const double strokeUnits = (strokeWidthPx * kSequenceSvgViewBox) / refPx;
        const QByteArray newStrokeStr = QByteArray("stroke-width:") + QByteArray::number(strokeUnits, 'f', 5);
        data.replace("stroke-width:1.27003", newStrokeStr);
    }

    QSvgRenderer renderer;
    if (!renderer.load(data) || !renderer.isValid()) {
        return QPixmap();
    }

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p, QRectF(0, 0, size.width(), size.height()));
    }

    const QPixmap pm = QPixmap::fromImage(img);
    QPixmapCache::insert(key, pm);
    return pm;
}

static int maxFontPixelSizeToFit(const QString& text, const QFont& baseFont, const QSize& box)
{
    if (text.isEmpty() || box.width() <= 0 || box.height() <= 0) {
        return 1;
    }

    QFont f = baseFont;

    int lo = 1;
    int hi = 300;
    int best = 1;
    while (lo <= hi)
    {
        const int mid = (lo + hi) / 2;
        f.setPixelSize(mid);
        QFontMetrics fm(f);
        const QRect br = fm.boundingRect(text);
        if (br.width() <= box.width() && br.height() <= box.height())
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return best;
}

class ScenePillWidget final : public QWidget
{
public:
    ScenePillWidget(const QString& sceneText, bool selected, QWidget* parent)
        : QWidget(parent)
        , mSceneText(sceneText)
        , mSelected(selected)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        return minimumSize();
    }

    QSize minimumSizeHint() const override
    {
        return minimumSize();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        const QColor border = mSelected ? QColor(QStringLiteral("#999bff")) : QColor(QStringLiteral("#505050"));
        const QColor fill = mSelected ? QColor(QStringLiteral("#275fb6")) : QColor(QStringLiteral("#1f1f1f"));

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF iconRect = rect().adjusted(2, 2, -2, -2);
        const qreal side = qMin(iconRect.width(), iconRect.height());
        iconRect.setLeft((width() - side) * 0.5);
        iconRect.setTop((height() - side) * 0.5);
        iconRect.setWidth(side);
        iconRect.setHeight(side);

        const double strokePx = mSelected ? 2.8 : 2.4;
        const QPixmap icon = renderSceneSvgWithColors(iconRect.size().toSize(), fill, border, strokePx);
        if (!icon.isNull()) {
            p.drawPixmap(iconRect.topLeft(), icon);
        }

        QRectF numberRect(
            iconRect.left() + iconRect.width() * 0.10,
            iconRect.top() + iconRect.height() * 0.42,
            iconRect.width() * 0.80,
            iconRect.height() * 0.48
        );

        QFont f = font();
        f.setBold(true);
        f.setPixelSize(maxFontPixelSizeToFit(mSceneText, f, numberRect.size().toSize()));
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(numberRect, Qt::AlignCenter, mSceneText);
    }

private:
    QString mSceneText;
    bool mSelected = false;
};

class SequencePillWidget final : public QWidget
{
public:
    SequencePillWidget(const QPixmap& thumbnail, const QString& infoText, bool selected, QWidget* parent)
        : QWidget(parent)
        , mThumb(thumbnail)
        , mInfoText(infoText)
        , mSelected(selected)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setStillThumbnail(const QPixmap& pix)
    {
        mStillThumb = pix;
        if (mThumb.isNull()) {
            mThumb = pix;
        }
        update();
    }

    void setPreviewThumbnail(const QPixmap& pix)
    {
        if (pix.isNull()) {
            return;
        }
        mThumb = pix;
        update();
    }

    void showStillThumbnail()
    {
        if (mStillThumb.isNull()) {
            return;
        }
        mThumb = mStillThumb;
        update();
    }

    QSize sizeHint() const override
    {
        return minimumSize();
    }

    QSize minimumSizeHint() const override
    {
        return minimumSize();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        const QColor border = mSelected ? QColor(QStringLiteral("#999bff")) : QColor(QStringLiteral("#505050"));
        const QColor fill = mSelected ? QColor(QStringLiteral("#275fb6")) : QColor(QStringLiteral("#1f1f1f"));

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF iconRect = rect().adjusted(2, 2, -2, -2);
        const qreal side = qMin(iconRect.width(), iconRect.height());
        iconRect.setLeft((width() - side) * 0.5);
        iconRect.setTop((height() - side) * 0.5);
        iconRect.setWidth(side);
        iconRect.setHeight(side);

        const double strokePx = mSelected ? 2.8 : 2.4;
        const QPixmap icon = renderSequenceSvgWithColors(iconRect.size().toSize(), fill, border, strokePx);
        if (!icon.isNull()) {
            p.drawPixmap(iconRect.topLeft(), icon);
        }

        QRectF contentRect(
            iconRect.left() + iconRect.width() * 0.10,
            iconRect.top() + iconRect.height() * 0.22,
            iconRect.width() * 0.80,
            iconRect.height() * 0.60
        );

        const qreal textH = contentRect.height() * 0.18;
        QRectF thumbRect = contentRect;
        thumbRect.setHeight(contentRect.height() - textH);

        QRectF textRect = contentRect;
        textRect.setTop(thumbRect.bottom());
        textRect.setHeight(textH);

        if (!mThumb.isNull())
        {
            const QPixmap scaled = mThumb.scaled(thumbRect.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const QPointF topLeft(thumbRect.left() + (thumbRect.width() - scaled.width()) * 0.5,
                                  thumbRect.top() + (thumbRect.height() - scaled.height()) * 0.5);
            p.save();
            p.setClipRect(thumbRect);
            p.drawPixmap(topLeft, scaled);
            p.restore();
        }
        else
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(QStringLiteral("#111111")));
            p.drawRect(thumbRect);
        }

        p.setPen(Qt::white);
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(maxFontPixelSizeToFit(mInfoText, f, textRect.size().toSize()));
        p.setFont(f);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, mInfoText);
    }

private:
    QPixmap mThumb;
    QPixmap mStillThumb;
    QString mInfoText;
    bool mSelected = false;
};

static LayerCamera* findCameraLayer(const Object* obj)
{
    if (!obj) {
        return nullptr;
    }
    const int layerCount = obj->getLayerCount();
    for (int i = 0; i < layerCount; ++i) {
        Layer* layer = obj->getLayer(i);
        if (layer && layer->type() == Layer::CAMERA) {
            return static_cast<LayerCamera*>(layer);
        }
    }
    return nullptr;
}

static int previewMaxFrameForObject(const Object* obj)
{
    if (!obj) {
        return 1;
    }
    int maxFrame = 1;
    const int layerCount = obj->getLayerCount();
    for (int i = 0; i < layerCount; ++i) {
        Layer* layer = obj->getLayer(i);
        if (!layer) {
            continue;
        }
        maxFrame = qMax(maxFrame, layer->getMaxKeyFramePosition());
    }
    return qMax(1, qMin(maxFrame, 48));
}

static QPixmap renderObjectFrameThumbnail(Object* obj, int frameNumber, const QSize& size, bool antialiasing)
{
    if (!obj || size.isEmpty()) {
        return QPixmap();
    }

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (LayerCamera* cam = findCameraLayer(obj)) {
        QTransform view = cam->getViewAtFrame(frameNumber);
        QSize camSize = cam->getViewSize();
        QTransform centralizeCamera;
        centralizeCamera.translate(camSize.width() / 2.0, camSize.height() / 2.0);
        painter.setWorldTransform(view * centralizeCamera);
        painter.setWindow(QRect(0, 0, camSize.width(), camSize.height()));
    }

    obj->paintImage(painter, frameNumber, false, antialiasing);
    painter.end();
    return QPixmap::fromImage(img);
}

bool TimeLine::eventFilter(QObject* watched, QEvent* event)
{
    if (watched && event && mScenesScrollArea && watched == mScenesScrollArea->viewport())
    {
        if (event->type() == QEvent::Resize)
        {
            if (!mScenesRebuildQueued)
            {
                mScenesRebuildQueued = true;
                QTimer::singleShot(0, this, [this]() {
                    mScenesRebuildQueued = false;
                    rebuildScenesAndShotsUI();
                });
            }
        }
    }

    auto cancelReorder = [this]() {
        if (!mReorderArmed && !mReorderDragging) {
            return;
        }
        if (QWidget* grabber = QWidget::mouseGrabber()) {
            grabber->releaseMouse();
        }
        mReorderArmed = false;
        mReorderDragging = false;
        if (mReorderDropIndicator) {
            mReorderDropIndicator->hide();
        }

        if (mScenesAndShotsRebuildDeferred) {
            mScenesAndShotsRebuildDeferred = false;
            rebuildScenesAndShotsUI();
        }
    };

    if (event && (event->type() == QEvent::FocusOut || event->type() == QEvent::WindowDeactivate))
    {
        cancelReorder();
    }

    if (event && event->type() == QEvent::KeyRelease)
    {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        if (ke && ke->key() == Qt::Key_Control) {
            cancelReorder();
        }
    }

    if (event && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (!me || me->button() != Qt::LeftButton) {
            return BaseDockWidget::eventFilter(watched, event);
        }

        const Qt::KeyboardModifiers mods = me->modifiers();
        const bool ctrlDown = (mods & Qt::ControlModifier);

        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w)
        {
            QWidget* pill = w;
            while (pill && pill != mScenesGridContainer)
            {
                const QVariant k = pill->property("_pill_kind");
                if (k.isValid() && !k.toString().isEmpty()) {
                    break;
                }
                pill = pill->parentWidget();
            }

            if (!pill || pill == mScenesGridContainer) {
                return BaseDockWidget::eventFilter(watched, event);
            }

            const QString kind = pill->property("_pill_kind").toString();

            if (editor() && editor()->playback() && editor()->playback()->isPlaying()) {
                if (kind == QStringLiteral("scene") || kind == QStringLiteral("shot")) {
                    return true;
                }
            }
            if (kind == QStringLiteral("scene"))
            {
                const int clickedSceneIndex = pill->property("_scene_index").toInt();

                if (ctrlDown)
                {
                    // Ctrl+click arms reorder; if user doesn't drag, we treat release as "activate only".
                    mReorderArmed = true;
                    mReorderDragging = false;
                    mReorderKind = QStringLiteral("scene");
                    mReorderSceneIndex = 0;
                    mReorderPressedIndex = clickedSceneIndex;
                    mReorderInsertPos = clickedSceneIndex;
                    mReorderPressGlobalPos = me->globalPos();
                    pill->grabMouse();
                    return true;
                }

                // Normal click: enter the scene (browse its shots) and make it active.
                mSelectedSceneIndex = clickedSceneIndex;
                mSelectedSequenceIndex = 0;
                if (mSceneSequenceManager) {
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                    mSceneSequenceManager->browseSequences(mSelectedSceneIndex);
                    return true;
                }
                rebuildScenesAndShotsUI();
                return true;
            }
            else if (kind == QStringLiteral("shot"))
            {
                const int clickedSceneIndex = pill->property("_scene_index").toInt();
                const int clickedShotIndex = pill->property("_sequence_index").toInt();

                if (ctrlDown)
                {
                    // Ctrl+click arms reorder; if user doesn't drag, we treat release as "activate shot".
                    mReorderArmed = true;
                    mReorderDragging = false;
                    mReorderKind = QStringLiteral("shot");
                    mReorderSceneIndex = clickedSceneIndex;
                    mReorderPressedIndex = clickedShotIndex;
                    mReorderInsertPos = clickedShotIndex;
                    mReorderPressGlobalPos = me->globalPos();
                    pill->grabMouse();
                    return true;
                }

                mSelectedSceneIndex = clickedSceneIndex;
                mSelectedSequenceIndex = clickedShotIndex;

                if (mSceneSequenceManager) {
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                    return true;
                }

                rebuildScenesAndShotsUI();
                return true;
            }
        }
    }

    if (event && event->type() == QEvent::MouseMove)
    {
        if ((mReorderArmed || mReorderDragging) && mScenesScrollArea && mScenesGridContainer && mReorderDropIndicator)
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (!me) {
                return BaseDockWidget::eventFilter(watched, event);
            }

            // Only start reorder after a small drag threshold to avoid "stuck" reorder from clicks.
            if (mReorderArmed && !mReorderDragging)
            {
                constexpr int kDragThreshold = 6;
                if ((me->globalPos() - mReorderPressGlobalPos).manhattanLength() < kDragThreshold) {
                    return true;
                }
                mReorderDragging = true;
            }

            // If ctrl is not held anymore, cancel immediately.
            if (!(me->modifiers() & Qt::ControlModifier)) {
                cancelReorder();
                return true;
            }

            auto findPillAtGlobal = [this](const QPoint& globalPos) -> QWidget* {
                if (!mScenesGridContainer) return nullptr;
                QPoint p = mScenesGridContainer->mapFromGlobal(globalPos);
                QWidget* child = mScenesGridContainer->childAt(p);
                QWidget* w = child;
                while (w && w != mScenesGridContainer) {
                    const QVariant k = w->property("_pill_kind");
                    if (k.isValid() && !k.toString().isEmpty()) {
                        return w;
                    }
                    w = w->parentWidget();
                }
                return nullptr;
            };

            QWidget* target = findPillAtGlobal(me->globalPos());
            int targetIndex = -1;
            bool insertAfter = true;

            if (target && target->property("_pill_kind").toString() == mReorderKind)
            {
                if (mReorderKind == QStringLiteral("scene")) {
                    targetIndex = target->property("_scene_index").toInt();
                } else {
                    targetIndex = target->property("_sequence_index").toInt();
                }

                const QRect r = target->geometry();
                const QPoint localInTarget = target->mapFromGlobal(me->globalPos());
                insertAfter = (localInTarget.x() >= (r.width() / 2));
            }
            else
            {
                const int count = mScenesGridLayout ? mScenesGridLayout->count() : 0;
                if (count > 0) {
                    if (QLayoutItem* it = mScenesGridLayout->itemAt(count - 1)) {
                        if (QWidget* last = it->widget()) {
                            target = last;
                            if (mReorderKind == QStringLiteral("scene")) {
                                targetIndex = last->property("_scene_index").toInt();
                            } else {
                                targetIndex = last->property("_sequence_index").toInt();
                            }
                            insertAfter = true;
                        }
                    }
                }
            }

            if (target && targetIndex >= 0)
            {
                mReorderInsertPos = insertAfter ? (targetIndex + 1) : targetIndex;

                const QRect r = target->geometry();
                const QPoint tl = mScenesGridContainer->mapTo(mScenesScrollArea->viewport(), r.topLeft());
                const int barW = 4;
                QRect ind;
                if (insertAfter) {
                    ind = QRect(tl.x() + r.width() - barW, tl.y(), barW, r.height());
                } else {
                    ind = QRect(tl.x(), tl.y(), barW, r.height());
                }

                mReorderDropIndicator->setGeometry(ind);
                if (!mReorderDropIndicator->isVisible()) {
                    mReorderDropIndicator->show();
                }
            }

            return true;
        }
    }

    if (event && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me && me->button() == Qt::LeftButton && (mReorderArmed || mReorderDragging))
        {
            if (editor() && editor()->playback() && editor()->playback()->isPlaying()) {
                if (QWidget* grabber = QWidget::mouseGrabber()) {
                    grabber->releaseMouse();
                }
                mReorderArmed = false;
                mReorderDragging = false;
                if (mReorderDropIndicator) {
                    mReorderDropIndicator->hide();
                }
                return true;
            }

            if (QWidget* grabber = QWidget::mouseGrabber()) {
                grabber->releaseMouse();
            }

            const bool didDrag = mReorderDragging;
            const QString kind = mReorderKind;
            const int pressedIndex = mReorderPressedIndex;
            const int insertPos = mReorderInsertPos;
            const int sceneIndex = mReorderSceneIndex;

            // We will rebuild explicitly at the end; avoid an intermediate rebuild from cancelReorder().
            mScenesAndShotsRebuildDeferred = false;

            quint64 movedShotUid = 0;
            if (mSceneSequenceManager && kind == QStringLiteral("shot")) {
                movedShotUid = mSceneSequenceManager->sequenceUid(sceneIndex, pressedIndex);
            }

            cancelReorder();

            if (!mSceneSequenceManager) {
                rebuildScenesAndShotsUI();
                return true;
            }

            if (!didDrag)
            {
                // Ctrl+click behavior (no drag):
                // - Scene overview: activate only, do NOT enter scene.
                // - Shot view: activate only.
                if (kind == QStringLiteral("scene")) {
                    mSelectedSceneIndex = pressedIndex;
                    mSelectedSequenceIndex = 0;
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                    mSceneSequenceManager->browseScenes();
                } else if (kind == QStringLiteral("shot")) {
                    mSelectedSceneIndex = sceneIndex;
                    mSelectedSequenceIndex = pressedIndex;
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                }
                rebuildScenesAndShotsUI();
                return true;
            }

            // Ctrl+drag behavior: reorder single item.
            if (kind == QStringLiteral("scene")) {
                mSceneSequenceManager->moveScenes(QVector<int>({ pressedIndex }), insertPos);
                mSceneSequenceManager->browseScenes();
            } else if (kind == QStringLiteral("shot")) {
                mSceneSequenceManager->moveSequences(sceneIndex, QVector<int>({ pressedIndex }), insertPos);

                // Match scene behavior by activating on release (after the reorder).
                if (movedShotUid != 0)
                {
                    const auto& scenes = mSceneSequenceManager->scenes();
                    if (sceneIndex >= 0 && sceneIndex < static_cast<int>(scenes.size())) {
                        const auto& seqs = scenes[sceneIndex].sequences;
                        for (int i = 0; i < static_cast<int>(seqs.size()); ++i) {
                            if (seqs[i].uid == movedShotUid) {
                                mSelectedSceneIndex = sceneIndex;
                                mSelectedSequenceIndex = i;
                                mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                                break;
                            }
                        }
                    }
                }
            }

            rebuildScenesAndShotsUI();
            return true;
        }
    }

    if (event && (event->type() == QEvent::Enter || event->type() == QEvent::Leave))
    {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->property("_pill_kind").toString() == QStringLiteral("shot"))
        {
            if (event->type() == QEvent::Enter)
            {
                mHoveredShotPill = w;
                mHoverFrame = 1;
                if (mSceneSequenceManager) {
                    const int sceneIdx = w->property("_scene_index").toInt();
                    const int shotIdx = w->property("_sequence_index").toInt();
                    if (Object* obj = mSceneSequenceManager->objectForSequence(sceneIdx, shotIdx)) {
                        mHoverMaxFrame = previewMaxFrameForObject(obj);
                    } else {
                        mHoverMaxFrame = 1;
                    }
                }
                if (mShotHoverTimer && !mShotHoverTimer->isActive()) {
                    mShotHoverTimer->start();
                }
                return false;
            }
            else
            {
                if (mHoveredShotPill == w) {
                    mHoveredShotPill = nullptr;
                    if (mShotHoverTimer) {
                        mShotHoverTimer->stop();
                    }
                    if (auto pill = dynamic_cast<SequencePillWidget*>(w)) {
                        pill->showStillThumbnail();
                    }
                }
                return false;
            }
        }
    }

    return BaseDockWidget::eventFilter(watched, event);
}

QString TimeLine::projectDisplayName() const
{
    if (!editor() || !editor()->object()) {
        return QStringLiteral("Untitled");
    }

    const QString filePath = editor()->object()->filePath();
    if (filePath.isEmpty()) {
        return QStringLiteral("Untitled");
    }

    const QFileInfo fi(filePath);
    const QString base = fi.completeBaseName();
    return base.isEmpty() ? QStringLiteral("Untitled") : base;
}

void TimeLine::rebuildScenesAndShotsUI()
{
    if (mReorderArmed || mReorderDragging) {
        mScenesAndShotsRebuildDeferred = true;
        return;
    }
    if (!mSceneSequenceManager || !mScenesAndShotsTab || !mScenesGridContainer || !mScenesGridLayout || !mScenesFlowWrap) {
        return;
    }
    if (mShotHoverTimer) {
        mShotHoverTimer->stop();
    }
    mHoveredShotPill = nullptr;

    mSceneSequenceManager->ensureInitialized();

    if (mShotPlaylistActive) {
        mSelectedSceneIndex = mShotPlaylistOriginalSceneIndex;
        mSelectedSequenceIndex = mShotPlaylistOriginalSequenceIndex;
    } else {
        mSelectedSceneIndex = mSceneSequenceManager->activeSceneIndex();
        mSelectedSequenceIndex = mSceneSequenceManager->activeSequenceIndex();
    }

    // Clear existing pills
    while (mScenesGridLayout->count() > 0)
    {
        QLayoutItem* it = mScenesGridLayout->takeAt(0);
        if (!it) {
            break;
        }
        QWidget* w = it->widget();
        delete it;
        if (w) {
            w->deleteLater();
        }
    }

    const bool atSceneLevel = mSceneSequenceManager->isAtSceneLevel();

    auto configurePillHitTarget = [this](QWidget* pill) {
        if (!pill) return;
        pill->installEventFilter(this);
        const auto children = pill->findChildren<QWidget*>();
        for (QWidget* c : children) {
            if (!c) continue;
            c->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
    };

    // Breadcrumb
    if (mBreadcrumbSceneButton && mBreadcrumbSequenceLabel) {
        const int browsingScene = mSceneSequenceManager->browsingSceneIndex();

        if (atSceneLevel) {
            mBreadcrumbSceneButton->setText(QStringLiteral("Scenes"));
        } else {
            mBreadcrumbSceneButton->setText(QString("Scene %1").arg(browsingScene + 1));
        }

        if (atSceneLevel) {
            mBreadcrumbSequenceLabel->setText(QStringLiteral(" >> Shots"));
        } else {
            int displayShot = 0;
            if (mSelectedSceneIndex == browsingScene) {
                displayShot = mSelectedSequenceIndex;
            } else {
                displayShot = mSceneSequenceManager->activeSequenceIndex();
            }
            mBreadcrumbSequenceLabel->setText(QString(" >> Shot %1").arg(displayShot + 1));
        }

        const QString activeStyle = QStringLiteral(
            "color: #ffffff; background: transparent; border: none; font-size: 12px;"
        );
        const QString inactiveStyle = QStringLiteral(
            "color: #808080; background: transparent; border: none; font-size: 12px;"
        );

        mBreadcrumbSceneButton->setStyleSheet(
            QString("QPushButton { %1 } QPushButton:hover { text-decoration: underline; }")
                .arg(atSceneLevel ? activeStyle : inactiveStyle)
        );
        mBreadcrumbSequenceLabel->setStyleSheet(
            QString("QLabel { %1 }")
                .arg(atSceneLevel ? inactiveStyle : activeStyle)
        );
        mBreadcrumbSequenceLabel->setVisible(true);
    }

    constexpr int kScenePillW = 180;
    constexpr int kScenePillH = 120;
    constexpr int kSequencePillSize = 234;
    const QString gridBg = QStringLiteral("#353535");

    const int viewportW = (mScenesScrollArea && mScenesScrollArea->viewport())
                               ? mScenesScrollArea->viewport()->width()
                               : mScenesGridContainer->width();

    const int itemW = atSceneLevel ? kScenePillW : kSequencePillSize;
    const int itemH = atSceneLevel ? kScenePillH : kSequencePillSize;

    auto computeCols = [viewportW, itemW](int totalItems) -> int {
        const int fit = qMax(1, viewportW / qMax(1, itemW));
        return qMax(1, qMin(fit, qMax(1, totalItems)));
    };

    int cols = 1;
    int totalItems = 0;

    if (atSceneLevel)
    {
        const int sceneCount = mSceneSequenceManager->sceneCount();
        totalItems = sceneCount;
        cols = computeCols(totalItems);
        for (int i = 0; i < sceneCount; ++i)
        {
            const bool selected = (i == mSelectedSceneIndex);
            ScenePillWidget* pill = new ScenePillWidget(QString::number(i + 1), selected, mScenesGridContainer);
            pill->setFixedSize(kScenePillW, kScenePillH);

            pill->setCursor(Qt::PointingHandCursor);
            pill->setProperty("_pill_kind", QStringLiteral("scene"));
            pill->setProperty("_scene_index", i);
            configurePillHitTarget(pill);

            const int row = (cols > 0) ? (i / cols) : 0;
            const int col = (cols > 0) ? (i % cols) : 0;
            mScenesGridLayout->addWidget(pill, row, col, Qt::AlignLeft | Qt::AlignTop);
        }
    }
    else
    {
        const int sceneIndex = mSceneSequenceManager->browsingSceneIndex();
        const auto& scenes = mSceneSequenceManager->scenes();
        if (sceneIndex < 0 || sceneIndex >= static_cast<int>(scenes.size())) {
            return;
        }

        const auto& seqs = scenes[sceneIndex].sequences;
        totalItems = static_cast<int>(seqs.size());
        cols = computeCols(totalItems);
        for (int si = 0; si < static_cast<int>(seqs.size()); ++si)
        {
            const bool selected = (sceneIndex == mSelectedSceneIndex && si == mSelectedSequenceIndex);
            const int targetScene = sceneIndex;
            const int targetSeq = si;

            const QString infoText = QString("Shot %1")
                                         .arg(si + 1);

            const QPixmap thumb;
            SequencePillWidget* pill = new SequencePillWidget(thumb, infoText, selected, mScenesGridContainer);
            pill->setFixedSize(kSequencePillSize, kSequencePillSize);

            if (mSceneSequenceManager) {
                Object* obj = mSceneSequenceManager->objectForSequence(targetScene, targetSeq);
                if (obj) {
                    quint32 rev = 0;
                    if (mShotThumbRevisions) {
                        rev = mShotThumbRevisions->value(obj, 0);
                    }
                    const quint64 uid = mSceneSequenceManager->sequenceUid(targetScene, targetSeq);
                    const QSize thumbSize(qMax(1, qRound(kSequencePillSize * 0.64)), qMax(1, qRound(kSequencePillSize * 0.48)));
                    const QString key = QString("ShotThumb_uid%1_%2x%3_r%4")
                                            .arg(uid)
                                            .arg(thumbSize.width())
                                            .arg(thumbSize.height())
                                            .arg(rev);
                    QPixmap cached;
                    if (!QPixmapCache::find(key, &cached)) {
                        cached = renderObjectFrameThumbnail(obj, 1, thumbSize, true);
                        if (!cached.isNull()) {
                            QPixmapCache::insert(key, cached);
                        }
                    }
                    pill->setStillThumbnail(cached);
                }
            }

            pill->setCursor(Qt::PointingHandCursor);
            pill->setMouseTracking(true);
            configurePillHitTarget(pill);

            pill->setProperty("_scene_index", targetScene);
            pill->setProperty("_sequence_index", targetSeq);
            pill->setProperty("_pill_kind", QStringLiteral("shot"));

            const int row = (cols > 0) ? (si / cols) : 0;
            const int col = (cols > 0) ? (si % cols) : 0;
            mScenesGridLayout->addWidget(pill, row, col, Qt::AlignLeft | Qt::AlignTop);
        }
    }

    const int rows = (totalItems > 0) ? ((totalItems + cols - 1) / cols) : 0;
    const int contentW = cols * itemW;
    const int contentH = rows * itemH;

    // Prevent layouts from distributing extra space as gaps: we pin the inner wrap
    // to the exact grid extent and let the scroll area show extra empty space.
    mScenesFlowWrap->setFixedWidth(qMax(1, contentW));
    mScenesFlowWrap->setMinimumHeight(qMax(1, contentH));
    mScenesFlowWrap->setMaximumHeight(qMax(1, contentH));

    mScenesGridContainer->setMinimumSize(qMax(1, contentW), qMax(1, contentH));

    if (mScenesGridContainer) {
        mScenesGridContainer->setStyleSheet(QString("QWidget { background-color: %1; }").arg(gridBg));
    }
}

void TimeLine::onPlayToggleClicked()
{
    if (!editor() || !editor()->playback() || !mSceneSequenceManager) {
        emit playButtonTriggered();
        return;
    }

    PlaybackManager* pb = editor()->playback();

    // Shot = legacy behavior (no scene/shot traversal).
    if (mShotPlaylistMode == 0)
    {
        if (mShotPlaylistActive) {
            mShotPlaylistActive = false;
        }
        emit playButtonTriggered();
        return;
    }

    // If currently playing, stop and restore.
    if (pb->isPlaying())
    {
        mShotPlaylistStopRequested = true;
        pb->stop();
        // Restoration happens in onPlaybackManagerPlayStateChanged(false)
        return;
    }

    // Start playlist.
    mShotPlaylistActive = true;
    mShotPlaylistAdvancing = true;
    mShotPlaylistStopRequested = false;
    mShotPlaylistQueuedPlay = false;

    mShotPlaylistOriginalSceneIndex = mSceneSequenceManager->activeSceneIndex();
    mShotPlaylistOriginalSequenceIndex = mSceneSequenceManager->activeSequenceIndex();
    mShotPlaylistOriginalAtSceneLevel = mSceneSequenceManager->isAtSceneLevel();
    mShotPlaylistOriginalBrowsingSceneIndex = mSceneSequenceManager->browsingSceneIndex();
    mShotPlaylistOriginalLooping = pb->isLooping();

    // In playlist modes, loop means "loop the traversal".
    // UI is source-of-truth.
    mShotPlaylistLoopingEnabled = (mLoopToggle) ? mLoopToggle->isChecked() : false;

    // For playlist modes we need playback to naturally stop at the end of each shot.
    mShotPlaylistForcingPlaybackLoopOff = true;
    pb->setLooping(false);
    mShotPlaylistForcingPlaybackLoopOff = false;

    if (mShotPlaylistMode == 2) {
        mShotPlaylistSceneIndex = 0;
        mShotPlaylistSequenceIndex = 0;
    } else {
        mShotPlaylistSceneIndex = mShotPlaylistOriginalSceneIndex;
        mShotPlaylistSequenceIndex = mShotPlaylistOriginalSequenceIndex;
    }

    mShotPlaylistLoopStartSceneIndex = mShotPlaylistSceneIndex;
    mShotPlaylistLoopStartSequenceIndex = mShotPlaylistSequenceIndex;

    const int sceneCount = mSceneSequenceManager->sceneCount();
    if (sceneCount <= 0) {
        mShotPlaylistActive = false;
        mShotPlaylistAdvancing = false;
        emit playButtonTriggered();
        return;
    }

    // Clamp start indices.
    mShotPlaylistSceneIndex = qBound(0, mShotPlaylistSceneIndex, sceneCount - 1);
    const auto& scenes = mSceneSequenceManager->scenes();
    const int seqCount = static_cast<int>(scenes[mShotPlaylistSceneIndex].sequences.size());
    mShotPlaylistSequenceIndex = qBound(0, mShotPlaylistSequenceIndex, qMax(0, seqCount - 1));

    // Temporarily activate the shot to play it.
    mSceneSequenceManager->activateSequence(mShotPlaylistSceneIndex, mShotPlaylistSequenceIndex);

    // After object switch, PlaybackManager::load() may restore looping from object data.
    // Force it back off (traversal requires stop at end-of-shot).
    mShotPlaylistForcingPlaybackLoopOff = true;
    pb->setLooping(false);
    mShotPlaylistForcingPlaybackLoopOff = false;

    // Keep the UI view stable (do not jump browsing level/scene).
    if (mShotPlaylistOriginalAtSceneLevel) {
        mSceneSequenceManager->browseScenes();
    } else {
        mSceneSequenceManager->browseSequences(mShotPlaylistOriginalBrowsingSceneIndex);
    }
    rebuildScenesAndShotsUI();

    // In Scenes/Shots tab, playback always starts from the beginning of the shot.
    if (isScenesAndShotsTabActive()) {
        const int start = pb->startFrame();
        editor()->scrubTo(start);
        pb->setCheckForSoundsHalfway(true);
    }

    // Queue play to avoid reentrancy/races during object switch.
    mShotPlaylistQueuedPlay = true;
    QTimer::singleShot(0, this, [this]() {
        if (!mShotPlaylistActive || mShotPlaylistMode == 0 || !editor() || !editor()->playback()) {
            mShotPlaylistQueuedPlay = false;
            return;
        }
        PlaybackManager* pb2 = editor()->playback();

        // Ensure traversal constraints.
        mShotPlaylistForcingPlaybackLoopOff = true;
        pb2->setLooping(false);
        mShotPlaylistForcingPlaybackLoopOff = false;

        pb2->play();
        setPlaying(true);
        mShotPlaylistQueuedPlay = false;

        // Allow end-of-shot events now.
        mShotPlaylistAdvancing = false;
    });
}

void TimeLine::onPlaybackManagerPlayStateChanged(bool isPlaying)
{
    if (!mShotPlaylistActive || !editor() || !editor()->playback() || !mSceneSequenceManager) {
        return;
    }

    PlaybackManager* pb = editor()->playback();

    if (isPlaying) {
        return;
    }

    // If we stopped/started during an intentional transition, ignore.
    if (mShotPlaylistAdvancing) {
        return;
    }

    // Decide if this was a natural end-of-shot stop.
    const int frame = editor()->currentFrame();
    // In traversal modes, PlaybackManager looping is forced off; natural end means reaching endFrame.
    const bool naturalEnd = (frame >= pb->endFrame());

    auto restoreOriginal = [this, pb]() {
        mShotPlaylistAdvancing = true;

        // Leave playlist mode BEFORE restoring PlaybackManager loop state,
        // so UI loop reflects the restored PlaybackManager state.
        mShotPlaylistActive = false;
        mShotPlaylistQueuedPlay = false;
        mShotPlaylistStopRequested = false;

        // UI is the source-of-truth.
        // If Playback Type is Shot, looping controls PlaybackManager looping.
        // If Playback Type is From Current/All, looping controls traversal looping (PlaybackManager looping must stay off).
        const bool uiLoop = (mLoopToggle) ? mLoopToggle->isChecked() : false;
        const bool shouldPlaybackLoop = (mShotPlaylistMode == 0) ? uiLoop : false;
        mShotPlaylistForcingPlaybackLoopOff = true;
        pb->setLooping(shouldPlaybackLoop);
        mShotPlaylistForcingPlaybackLoopOff = false;

        // Restore original active shot.
        if (mSceneSequenceManager) {
            mSceneSequenceManager->activateSequence(mShotPlaylistOriginalSceneIndex, mShotPlaylistOriginalSequenceIndex);
            if (mShotPlaylistOriginalAtSceneLevel) {
                mSceneSequenceManager->browseScenes();
            } else {
                mSceneSequenceManager->browseSequences(mShotPlaylistOriginalBrowsingSceneIndex);
            }
        }

        mShotPlaylistAdvancing = false;
        rebuildScenesAndShotsUI();

        // Ensure transport reflects the actual (stopped) state after playlist ends.
        setPlaying(false);
    };

    if (mShotPlaylistStopRequested)
    {
        restoreOriginal();
        return;
    }

    // If we stopped without reaching the end frame, treat as a manual stop (e.g. via menu shortcut).
    if (!naturalEnd)
    {
        restoreOriginal();
        return;
    }

    // Natural end -> advance to next shot/scene.
    const auto& scenes = mSceneSequenceManager->scenes();
    const int sceneCount = static_cast<int>(scenes.size());
    if (sceneCount <= 0) {
        restoreOriginal();
        return;
    }

    int nextScene = mShotPlaylistSceneIndex;
    int nextSeq = mShotPlaylistSequenceIndex + 1;
    if (nextScene < 0 || nextScene >= sceneCount) {
        restoreOriginal();
        return;
    }

    const int seqCount = static_cast<int>(scenes[nextScene].sequences.size());
    if (nextSeq >= seqCount)
    {
        nextScene++;
        nextSeq = 0;
        if (nextScene >= sceneCount) {
            if (!mShotPlaylistLoopingEnabled) {
                restoreOriginal();
                return;
            }

            // Loop traversal back to the start of the playback type.
            nextScene = mShotPlaylistLoopStartSceneIndex;
            nextSeq = mShotPlaylistLoopStartSequenceIndex;
        }
        const int nextSeqCount = static_cast<int>(scenes[nextScene].sequences.size());
        if (nextSeqCount <= 0) {
            restoreOriginal();
            return;
        }
    }

    mShotPlaylistSceneIndex = nextScene;
    mShotPlaylistSequenceIndex = nextSeq;

    mSceneSequenceManager->activateSequence(mShotPlaylistSceneIndex, mShotPlaylistSequenceIndex);

    // Force traversal constraint after object switch.
    mShotPlaylistForcingPlaybackLoopOff = true;
    pb->setLooping(false);
    mShotPlaylistForcingPlaybackLoopOff = false;

    // For traversal playback, always start each shot from its beginning.
    // (This prevents immediate stop if the current frame is already past the new end frame.)
    editor()->scrubTo(pb->startFrame());
    pb->setCheckForSoundsHalfway(true);

    // Keep UI view stable.
    if (mShotPlaylistOriginalAtSceneLevel) {
        mSceneSequenceManager->browseScenes();
    } else {
        mSceneSequenceManager->browseSequences(mShotPlaylistOriginalBrowsingSceneIndex);
    }
    rebuildScenesAndShotsUI();

    // Start playback of the next shot (queued to avoid races).
    if (!mShotPlaylistQueuedPlay)
    {
        mShotPlaylistQueuedPlay = true;
        QTimer::singleShot(0, this, [this]() {
            if (!mShotPlaylistActive || !editor() || !editor()->playback()) {
                mShotPlaylistQueuedPlay = false;
                return;
            }
            PlaybackManager* pb2 = editor()->playback();

            // Enforce traversal constraints every segment.
            mShotPlaylistForcingPlaybackLoopOff = true;
            pb2->setLooping(false);
            mShotPlaylistForcingPlaybackLoopOff = false;

            pb2->play();
            setPlaying(true);
            mShotPlaylistQueuedPlay = false;

            // Allow end-of-shot events now.
            mShotPlaylistAdvancing = false;
        });
    }
}

void TimeLine::initUI()
{
    Q_ASSERT(editor() != nullptr);

    setWindowTitle(tr("Timeline", "Subpanel title"));

    QWidget* dockContent = new QWidget(this);
    QVBoxLayout* dockLayout = new QVBoxLayout(dockContent);
    dockLayout->setContentsMargins(0, 0, 0, 0);
    dockLayout->setSpacing(0);

    QWidget* tabRow = new QWidget(dockContent);
    tabRow->setFixedHeight(30);
    tabRow->setStyleSheet("QWidget { background-color: #1f1f1f; }");

    QTabBar* tabBar = new QTabBar(tabRow);
    tabBar->setExpanding(false);
    tabBar->addTab(tr("Timeline"));
    tabBar->addTab(tr("Scenes and Shots"));
    // TODO: Scenes and Shots not ready for production - hide tab until a future release
    tabBar->setTabVisible(1, false);

    QStackedWidget* tabStack = new QStackedWidget(dockContent);
    QWidget* timelineTab = new QWidget(tabStack);
    QWidget* scenesAndShotsTab = new QWidget(tabStack);
    tabStack->addWidget(timelineTab);
    tabStack->addWidget(scenesAndShotsTab);
    connect(tabBar, &QTabBar::currentChanged, tabStack, &QStackedWidget::setCurrentIndex);

    connect(tabBar, &QTabBar::currentChanged, this, [this](int idx) {
        if (!editor()) {
            return;
        }

        mActiveTabIndex = idx;

        // Avoid mixing playback state with presentation mode when user switches tabs.
        if (editor()->playback() && editor()->playback()->isPlaying()) {
            editor()->playback()->stop();
        }

        ScribbleArea* canvas = editor()->getScribbleArea();
        if (!canvas) {
            return;
        }
        canvas->setPlaybackPresentationModeEnabled(idx == 1);
        canvas->setCanvasInteractionLocked(idx == 1);
    });

    mScenesAndShotsTab = scenesAndShotsTab;

    // Scenes/Sequences manager (in-memory for now)
    if (!mSceneSequenceManager) {
        mSceneSequenceManager = new SceneSequenceManager(editor(), this);
        connect(editor(), &Editor::objectLoaded, this, [this]() {
            if (mSceneSequenceManager) mSceneSequenceManager->onExternalObjectLoaded();
            rebuildScenesAndShotsUI();
        });
        connect(mSceneSequenceManager, &SceneSequenceManager::browsingLevelChanged, this, &TimeLine::rebuildScenesAndShotsUI);
        connect(mSceneSequenceManager, &SceneSequenceManager::activeSequenceChanged, this, &TimeLine::rebuildScenesAndShotsUI);
    }

    if (editor() && editor()->playback()) {
        connect(editor()->playback(), &PlaybackManager::playStateChanged, this, &TimeLine::onPlaybackManagerPlayStateChanged);
    }

    // --- Scenes and Shots tab UI ---
    {
        QHBoxLayout* root = new QHBoxLayout(scenesAndShotsTab);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        mScenesLeftPanel = new QWidget(scenesAndShotsTab);
        mScenesLeftPanel->setFixedWidth(0);
        mScenesLeftPanel->setMinimumWidth(0);
        mScenesLeftPanel->setMaximumWidth(0);
        mScenesLeftPanel->setVisible(false);
        mScenesLeftPanel->setStyleSheet("QWidget { background-color: #1f1f1f; }");

        mScenesRightPanel = new QWidget(scenesAndShotsTab);
        mScenesRightPanel->setStyleSheet("QWidget { background-color: #353535; }");

        // Right panel: breadcrumb header + scroll grid
        {
            QVBoxLayout* right = new QVBoxLayout(mScenesRightPanel);
            right->setContentsMargins(0, 0, 0, 0);
            right->setSpacing(0);

            QWidget* header = new QWidget(mScenesRightPanel);
            header->setFixedHeight(34);
            header->setStyleSheet("QWidget { background-color: #2f2f2f; }");

            QHBoxLayout* headerLay = new QHBoxLayout(header);
            headerLay->setContentsMargins(10, 0, 10, 0);
            headerLay->setSpacing(0);

            mBreadcrumbSceneButton = new QPushButton(header);
            mBreadcrumbSceneButton->setFlat(true);
            mBreadcrumbSceneButton->setCursor(Qt::PointingHandCursor);
            mBreadcrumbSceneButton->setStyleSheet(
                "QPushButton { color: #ffffff; background: transparent; border: none; font-size: 12px; }"
                "QPushButton:hover { text-decoration: underline; }"
            );
            connect(mBreadcrumbSceneButton, &QPushButton::clicked, this, [this]() {
                if (mSceneSequenceManager) {
                    mSelectedSceneIndex = mSceneSequenceManager->activeSceneIndex();
                    mSelectedSequenceIndex = mSceneSequenceManager->activeSequenceIndex();
                    mSceneSequenceManager->browseScenes();
                }
            });

            mBreadcrumbSequenceLabel = new QLabel(header);
            mBreadcrumbSequenceLabel->setStyleSheet("QLabel { color: #b2b2b2; font-size: 12px; }");

            auto loadSvgIcon = [](const QString& path, const QSize& size) -> QIcon {
                QIcon icon;
                QSvgRenderer renderer(path);
                if (renderer.isValid()) {
                    QPixmap pixmap(size);
                    pixmap.fill(Qt::transparent);
                    QPainter painter(&pixmap);
                    renderer.render(&painter);
                    painter.end();
                    icon.addPixmap(pixmap);
                } else {
                    icon = QIcon(path);
                }
                return icon;
            };

            auto configureCosmicToolbarButton = [](CosmicButton* b) {
                b->setBordered(false);
                b->setFixedSize(28, 28);
                b->setIconSize(QSize(18, 18));
            };

            headerLay->addWidget(mBreadcrumbSceneButton);
            headerLay->addWidget(mBreadcrumbSequenceLabel);

            QWidget* headerActions = new QWidget(header);
            QHBoxLayout* headerActionsLay = new QHBoxLayout(headerActions);
            headerActionsLay->setContentsMargins(0, 0, 0, 0);
            headerActionsLay->setSpacing(6);

            mScenesAddButton = new CosmicButton(headerActions);
            configureCosmicToolbarButton(mScenesAddButton);
            mScenesAddButton->setIcon(loadSvgIcon(":/Interaction/Add.svg", QSize(18, 18)));
            mScenesAddButton->setToolTip(tr("Add"));

            mScenesRemoveButton = new CosmicButton(headerActions);
            configureCosmicToolbarButton(mScenesRemoveButton);
            mScenesRemoveButton->setIcon(loadSvgIcon(":/Interaction/Remove.svg", QSize(18, 18)));
            mScenesRemoveButton->setToolTip(tr("Remove"));

            headerActionsLay->addWidget(mScenesAddButton);
            headerActionsLay->addWidget(mScenesRemoveButton);

            QWidget* playbackWrap = new QWidget(header);
            QHBoxLayout* playbackLay = new QHBoxLayout(playbackWrap);
            playbackLay->setContentsMargins(0, 0, 0, 0);
            playbackLay->setSpacing(6);

            mPlaybackTypeLabel = new QLabel(tr("Playback Type"), playbackWrap);
            mPlaybackTypeLabel->setStyleSheet("QLabel { color: #b2b2b2; font-size: 12px; }");
            mPlaybackTypeLabel->setToolTip(tr("Controls how Play traverses Scenes and Shots."));

            mPlaybackTypeDropDown = new CosmicDropDown(playbackWrap);
            mPlaybackTypeDropDown->setOptions(QStringList() << tr("Shot") << tr("From Current") << tr("All"));
            mPlaybackTypeDropDown->setCurrentIndex(0);
            mPlaybackTypeDropDown->setFixedHeight(24);

            mPlaybackPerfLabel = new QLabel(tr("Playback"), playbackWrap);
            mPlaybackPerfLabel->setStyleSheet("QLabel { color: #b2b2b2; font-size: 12px; }");

            mPlaybackPerfDropDown = new CosmicDropDown(playbackWrap);
            mPlaybackPerfDropDown->setOptions(QStringList() << tr("Prefer Quality") << tr("Prefer Speed"));
            mPlaybackPerfDropDown->setFixedHeight(24);
            if (editor() && editor()->preference()) {
                int mode = editor()->preference()->getInt(SETTING::PLAYBACK_PERFORMANCE_MODE);
                if (mode < 0 || mode > 1) {
                    mode = 0;
                }
                mPlaybackPerfDropDown->setCurrentIndex(mode);
            }

            auto updatePlaybackTypeToolTip = [this](int idx) {
                if (!mPlaybackTypeDropDown) return;
                if (idx == 0) {
                    mPlaybackTypeDropDown->setToolTip(tr("Shot: play the currently active shot."));
                } else if (idx == 1) {
                    mPlaybackTypeDropDown->setToolTip(tr("From Current: play from the current scene/shot, then continue to next shots/scenes."));
                } else {
                    mPlaybackTypeDropDown->setToolTip(tr("All: play from the first scene/shot, then continue through all shots/scenes."));
                }
            };
            updatePlaybackTypeToolTip(0);

            connect(mPlaybackTypeDropDown, &CosmicDropDown::currentIndexChanged, this, [this, updatePlaybackTypeToolTip](int idx) {
                mShotPlaylistMode = (idx == 1) ? 1 : (idx == 2) ? 2 : 0;
                updatePlaybackTypeToolTip(mShotPlaylistMode);

                if (mShotPlaylistActive && editor() && editor()->playback() && editor()->playback()->isPlaying()) {
                    editor()->playback()->stop();
                }
            });

            playbackLay->addWidget(mPlaybackTypeLabel);
            playbackLay->addWidget(mPlaybackTypeDropDown);

            playbackLay->addWidget(mPlaybackPerfLabel);
            playbackLay->addWidget(mPlaybackPerfDropDown);

            connect(mScenesAddButton, &QPushButton::clicked, this, [this]() {
                if (!mSceneSequenceManager) return;

                if (mSceneSequenceManager->isAtSceneLevel()) {
                    const int newIdx = mSceneSequenceManager->insertSceneAfter(mSelectedSceneIndex);
                    mSelectedSceneIndex = newIdx;
                    mSelectedSequenceIndex = 0;
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                    mSceneSequenceManager->browseScenes();
                } else {
                    const int sceneIdx = mSceneSequenceManager->browsingSceneIndex();
                    const int newSeq = mSceneSequenceManager->insertSequenceAfter(sceneIdx, mSelectedSequenceIndex);
                    mSelectedSceneIndex = sceneIdx;
                    mSelectedSequenceIndex = newSeq;
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                }
            });

            connect(mScenesRemoveButton, &QPushButton::clicked, this, [this]() {
                if (!mSceneSequenceManager) return;

                // Stop playlist playback before destructive operations.
                if (mShotPlaylistActive && editor() && editor()->playback() && editor()->playback()->isPlaying()) {
                    editor()->playback()->stop();
                }

                if (mSceneSequenceManager->isAtSceneLevel()) {
                    const auto& scenes = mSceneSequenceManager->scenes();
                    const int sceneCount = static_cast<int>(scenes.size());
                    if (sceneCount <= 1) {
                        return;
                    }

                    const int delSceneIndex = qBound(0, mSelectedSceneIndex, sceneCount - 1);

                    // Prefer the previous scene; if deleting the first, fall back to the next.
                    const int targetSceneIndex = (delSceneIndex > 0) ? (delSceneIndex - 1) : 1;
                    const quint64 targetSceneUid = scenes[targetSceneIndex].uid;
                    const int targetSeqCount = static_cast<int>(scenes[targetSceneIndex].sequences.size());
                    const quint64 targetSeqUid = (targetSeqCount > 0) ? scenes[targetSceneIndex].sequences[targetSeqCount - 1].uid : 0;

                    // Make sure the deleted scene is NOT active so the manager doesn't auto-switch for us.
                    if (mSceneSequenceManager->activeSceneIndex() == delSceneIndex) {
                        const int preActSeq = qMax(0, targetSeqCount - 1);
                        mSceneSequenceManager->activateSequence(targetSceneIndex, preActSeq);
                    }

                    const bool ok = mSceneSequenceManager->removeScene(delSceneIndex);
                    if (!ok) {
                        return;
                    }

                    // Re-find target by UID after reindex.
                    int foundScene = -1;
                    int foundSeq = 0;
                    const auto& scenesAfter = mSceneSequenceManager->scenes();
                    for (int si = 0; si < static_cast<int>(scenesAfter.size()); ++si) {
                        if (scenesAfter[si].uid == targetSceneUid) {
                            foundScene = si;
                            const auto& seqs = scenesAfter[si].sequences;
                            if (targetSeqUid != 0) {
                                for (int qi = 0; qi < static_cast<int>(seqs.size()); ++qi) {
                                    if (seqs[qi].uid == targetSeqUid) {
                                        foundSeq = qi;
                                        break;
                                    }
                                }
                            } else {
                                foundSeq = qMax(0, static_cast<int>(seqs.size()) - 1);
                            }
                            break;
                        }
                    }
                    if (foundScene < 0 && !scenesAfter.empty()) {
                        foundScene = qMax(0, static_cast<int>(scenesAfter.size()) - 1);
                        foundSeq = qMax(0, static_cast<int>(scenesAfter[foundScene].sequences.size()) - 1);
                    }

                    mSelectedSceneIndex = qMax(0, foundScene);
                    mSelectedSequenceIndex = qMax(0, foundSeq);
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                    mSceneSequenceManager->browseScenes();
                } else {
                    const int sceneIdx = mSceneSequenceManager->browsingSceneIndex();
                    const auto& scenes = mSceneSequenceManager->scenes();
                    if (sceneIdx < 0 || sceneIdx >= static_cast<int>(scenes.size())) {
                        return;
                    }
                    const auto& seqs = scenes[sceneIdx].sequences;
                    const int seqCount = static_cast<int>(seqs.size());
                    if (seqCount <= 0) {
                        return;
                    }

                    const int delSeqIndex = qBound(0, mSelectedSequenceIndex, seqCount - 1);

                    // If this is the last remaining shot in the scene, delete the scene and
                    // move to the last shot of the previous scene (or next if deleting the first).
                    if (seqCount == 1)
                    {
                        const int sceneCount = static_cast<int>(scenes.size());
                        if (sceneCount <= 1) {
                            return;
                        }

                        const int targetSceneIndex = (sceneIdx > 0) ? (sceneIdx - 1) : 1;
                        const quint64 targetSceneUid = scenes[targetSceneIndex].uid;
                        const int targetSeqCount = static_cast<int>(scenes[targetSceneIndex].sequences.size());
                        const quint64 targetSeqUid = (targetSeqCount > 0) ? scenes[targetSceneIndex].sequences[targetSeqCount - 1].uid : 0;

                        // Make sure we're not deleting the active scene to avoid manager auto-switch.
                        if (mSceneSequenceManager->activeSceneIndex() == sceneIdx) {
                            const int preActSeq = qMax(0, targetSeqCount - 1);
                            mSceneSequenceManager->activateSequence(targetSceneIndex, preActSeq);
                        }

                        const bool ok = mSceneSequenceManager->removeScene(sceneIdx);
                        if (!ok) {
                            return;
                        }

                        int foundScene = -1;
                        int foundSeq = 0;
                        const auto& scenesAfter = mSceneSequenceManager->scenes();
                        for (int si = 0; si < static_cast<int>(scenesAfter.size()); ++si) {
                            if (scenesAfter[si].uid == targetSceneUid) {
                                foundScene = si;
                                const auto& seqsAfter = scenesAfter[si].sequences;
                                if (targetSeqUid != 0) {
                                    for (int qi = 0; qi < static_cast<int>(seqsAfter.size()); ++qi) {
                                        if (seqsAfter[qi].uid == targetSeqUid) {
                                            foundSeq = qi;
                                            break;
                                        }
                                    }
                                } else {
                                    foundSeq = qMax(0, static_cast<int>(seqsAfter.size()) - 1);
                                }
                                break;
                            }
                        }
                        if (foundScene < 0 && !scenesAfter.empty()) {
                            foundScene = qMax(0, static_cast<int>(scenesAfter.size()) - 1);
                            foundSeq = qMax(0, static_cast<int>(scenesAfter[foundScene].sequences.size()) - 1);
                        }

                        mSelectedSceneIndex = qMax(0, foundScene);
                        mSelectedSequenceIndex = qMax(0, foundSeq);
                        mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                        mSceneSequenceManager->browseSequences(mSelectedSceneIndex);
                        rebuildScenesAndShotsUI();
                        return;
                    }

                    // Normal case: delete a shot inside the scene.
                    quint64 targetSeqUid = 0;
                    if (delSeqIndex > 0) {
                        targetSeqUid = seqs[delSeqIndex - 1].uid;
                    } else {
                        // No previous; fall back to the new first shot (old index 1).
                        targetSeqUid = seqs[1].uid;
                    }

                    // Ensure we don't delete the active shot (manager would auto-switch to previous/next).
                    if (mSceneSequenceManager->activeSceneIndex() == sceneIdx && mSceneSequenceManager->activeSequenceIndex() == delSeqIndex) {
                        const int preActIndex = (delSeqIndex > 0) ? (delSeqIndex - 1) : 1;
                        mSceneSequenceManager->activateSequence(sceneIdx, preActIndex);
                    }

                    const bool ok = mSceneSequenceManager->removeSequence(sceneIdx, delSeqIndex);
                    if (!ok) {
                        return;
                    }

                    int foundSeq = 0;
                    const auto& seqsAfter = mSceneSequenceManager->scenes()[sceneIdx].sequences;
                    for (int qi = 0; qi < static_cast<int>(seqsAfter.size()); ++qi) {
                        if (seqsAfter[qi].uid == targetSeqUid) {
                            foundSeq = qi;
                            break;
                        }
                    }

                    mSelectedSceneIndex = sceneIdx;
                    mSelectedSequenceIndex = foundSeq;
                    mSceneSequenceManager->activateSequence(mSelectedSceneIndex, mSelectedSequenceIndex);
                }
            });

            headerLay->addStretch(1);
            headerLay->addWidget(headerActions);
            headerLay->addStretch(1);
            headerLay->addWidget(playbackWrap);

            mScenesScrollArea = new QScrollArea(mScenesRightPanel);
            mScenesScrollArea->setWidgetResizable(true);
            mScenesScrollArea->setFrameShape(QFrame::NoFrame);
            mScenesScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            mScenesScrollArea->setStyleSheet(
                "QScrollArea { background-color: #353535; }"
                "QScrollBar:vertical { background-color: #1f1f1f; width: 10px; }"
                "QScrollBar::handle:vertical { background-color: #3a3a3a; border-radius: 4px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
            );

            mScenesGridContainer = new QWidget(mScenesScrollArea);
            QVBoxLayout* gridRoot = new QVBoxLayout(mScenesGridContainer);
            gridRoot->setContentsMargins(0, 0, 0, 0);
            gridRoot->setSpacing(0);

            mScenesFlowWrap = new QWidget(mScenesGridContainer);
            mScenesFlowWrap->setContentsMargins(0, 0, 0, 0);
            mScenesFlowWrap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

            mScenesGridLayout = new QGridLayout(mScenesFlowWrap);
            mScenesGridLayout->setContentsMargins(0, 0, 0, 0);
            mScenesGridLayout->setHorizontalSpacing(0);
            mScenesGridLayout->setVerticalSpacing(0);

            gridRoot->addWidget(mScenesFlowWrap, 0, Qt::AlignLeft | Qt::AlignTop);
            gridRoot->addStretch(1);

            mScenesScrollArea->setWidget(mScenesGridContainer);

            if (mScenesScrollArea->viewport()) {
                mScenesScrollArea->viewport()->installEventFilter(this);
            }

            if (!mReorderDropIndicator && mScenesScrollArea->viewport()) {
                mReorderDropIndicator = new QFrame(mScenesScrollArea->viewport());
                mReorderDropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                mReorderDropIndicator->setStyleSheet("QFrame { background-color: #ffffff; }");
                mReorderDropIndicator->hide();
            }

            right->addWidget(header);

            QWidget* scrollRow = new QWidget(mScenesRightPanel);
            QHBoxLayout* scrollRowLay = new QHBoxLayout(scrollRow);
            scrollRowLay->setContentsMargins(0, 0, 0, 0);
            scrollRowLay->setSpacing(0);

            mScenesVScrollbar = new QScrollBar(Qt::Vertical, scrollRow);
            mScenesVScrollbar->setRange(0, 0);
            mScenesVScrollbar->setPageStep(1);
            mScenesVScrollbar->setSingleStep(20);
            mScenesVScrollbar->setStyleSheet(
                "QScrollBar:vertical { background-color: #1f1f1f; width: 14px; }"
                "QScrollBar::handle:vertical { background-color: #4a4a4a; border-radius: 6px; min-height: 28px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background-color: #1f1f1f; }"
            );

            QScrollBar* sceneSb = mScenesScrollArea->verticalScrollBar();
            connect(sceneSb, &QScrollBar::rangeChanged, this, [this, sceneSb](int min, int max) {
                if (!mScenesVScrollbar) return;
                QSignalBlocker b(mScenesVScrollbar);
                mScenesVScrollbar->setRange(min, max);
                mScenesVScrollbar->setPageStep(sceneSb->pageStep());
                mScenesVScrollbar->setSingleStep(sceneSb->singleStep());
            });
            connect(sceneSb, &QScrollBar::valueChanged, this, [this](int v) {
                if (!mScenesVScrollbar) return;
                QSignalBlocker b(mScenesVScrollbar);
                mScenesVScrollbar->setValue(v);
            });
            connect(mScenesVScrollbar, &QScrollBar::valueChanged, this, [this](int v) {
                if (!mScenesScrollArea) return;
                mScenesScrollArea->verticalScrollBar()->setValue(v);
            });

            scrollRowLay->addWidget(mScenesScrollArea, 1);
            scrollRowLay->addWidget(mScenesVScrollbar);

            right->addWidget(scrollRow);
        }

        root->addWidget(mScenesLeftPanel);
        root->addWidget(mScenesRightPanel, 1);

        rebuildScenesAndShotsUI();
    }

    if (!mShotThumbRevisions) {
        mShotThumbRevisions = new QHash<const Object*, quint32>();
    }

    if (!mShotThumbInvalidateTimer) {
        mShotThumbInvalidateTimer = new QTimer(this);
        mShotThumbInvalidateTimer->setSingleShot(true);
        connect(mShotThumbInvalidateTimer, &QTimer::timeout, this, [this]() {
            if (!editor() || !mShotThumbRevisions) {
                return;
            }
            Object* obj = editor()->object();
            if (!obj) {
                return;
            }
            (*mShotThumbRevisions)[obj] = mShotThumbRevisions->value(obj, 0) + 1;
            rebuildScenesAndShotsUI();
        });
        connect(editor(), &Editor::framesModified, this, [this]() {
            if (mShotThumbInvalidateTimer) {
                mShotThumbInvalidateTimer->start(200);
            }
        });
    }

    if (!mShotHoverTimer) {
        mShotHoverTimer = new QTimer(this);
        mShotHoverTimer->setInterval(80);
        connect(mShotHoverTimer, &QTimer::timeout, this, [this]() {
            if (!mHoveredShotPill || !mSceneSequenceManager) {
                if (mShotHoverTimer) {
                    mShotHoverTimer->stop();
                }
                return;
            }
            auto pill = dynamic_cast<SequencePillWidget*>(mHoveredShotPill);
            if (!pill) {
                if (mShotHoverTimer) {
                    mShotHoverTimer->stop();
                }
                mHoveredShotPill = nullptr;
                return;
            }
            const int sceneIdx = mHoveredShotPill->property("_scene_index").toInt();
            const int shotIdx = mHoveredShotPill->property("_sequence_index").toInt();
            Object* obj = mSceneSequenceManager->objectForSequence(sceneIdx, shotIdx);
            if (!obj) {
                return;
            }
            const QSize thumbSize(qMax(1, qRound(234 * 0.64)), qMax(1, qRound(234 * 0.48)));
            const QPixmap framePix = renderObjectFrameThumbnail(obj, qMax(1, mHoverFrame), thumbSize, false);
            pill->setPreviewThumbnail(framePix);

            mHoverFrame += 1;
            if (mHoverFrame > qMax(1, mHoverMaxFrame)) {
                mHoverFrame = 1;
            }
        });
    }

    QWidget* transportWidget = new QWidget(tabRow);
    transportWidget->setStyleSheet("QWidget { background-color: #1f1f1f; }");
    QHBoxLayout* transportLayout = new QHBoxLayout(transportWidget);
    transportLayout->setContentsMargins(0, 0, 0, 0);
    transportLayout->setSpacing(0);

    QWidget* playbackButtonsWrap = new QWidget(transportWidget);
    QHBoxLayout* playbackButtonsLayout = new QHBoxLayout(playbackButtonsWrap);
    playbackButtonsLayout->setContentsMargins(0, 0, 0, 0);
    playbackButtonsLayout->setSpacing(0);

    mLayerList = new TimeLineCells(this, editor(), TIMELINE_CELL_TYPE::Layers);
    // Match dock master container background (2f2f2f) for a seamless panel.
    // Use both stylesheet (for any style-based paints) and palette (for
    // custom drawing in TimeLineCells::drawContent which uses QPalette::Window).
    mLayerList->setStyleSheet("TimeLineCells { background-color: #111111; }");
    QPalette layersPal = mLayerList->palette();
    layersPal.setColor(QPalette::Window, QColor(17, 17, 17)); // 111111ff
    mLayerList->setPalette(layersPal);

    mTracks = new TimeLineCells(this, editor(), TIMELINE_CELL_TYPE::Tracks);
    QPalette tracksPal = mTracks->palette();
    tracksPal.setColor(QPalette::Window, QColor(47, 47, 47)); // 2f2f2fff
    mTracks->setPalette(tracksPal);

    mHScrollbar = new QScrollBar(Qt::Horizontal, timelineTab);
    mHScrollbar->setFixedHeight(12);
    mHScrollbar->setStyleSheet(
        "QScrollBar:horizontal {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  height: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-width: 26px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: transparent; width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
    );

    mHScrollbarTop = new QScrollBar(Qt::Horizontal, timelineTab);
    mHScrollbarTop->setFixedHeight(12);
    mHScrollbarTop->setStyleSheet(mHScrollbar->styleSheet());
    mVScrollbar = new QScrollBar(Qt::Vertical, timelineTab);
    mVScrollbar->setFixedWidth(12);
    mVScrollbar->setStyleSheet(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 26px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );
    mVScrollbar->setMinimum(0);
    mVScrollbar->setMaximum(1);
    mVScrollbar->setPageStep(1);

    QWidget* leftWidget = new QWidget(timelineTab);
    leftWidget->setMinimumWidth(120);
    leftWidget->setStyleSheet("QWidget { background-color: #111111; }");
    QWidget* rightWidget = new QWidget(timelineTab);

    // Primary vertical scrollbar belongs to the frames area (rightWidget), left edge
    mVScrollbar->setParent(rightWidget);

    // Second vertical scrollbar on the frames area (rightWidget)
    mVScrollbarRight = new QScrollBar(Qt::Vertical, rightWidget);
    mVScrollbarRight->setMinimum(mVScrollbar->minimum());
    mVScrollbarRight->setMaximum(mVScrollbar->maximum());
    mVScrollbarRight->setPageStep(mVScrollbar->pageStep());
    mVScrollbarRight->setFixedWidth(12);
    mVScrollbarRight->setStyleSheet(mVScrollbar->styleSheet());

    QWidget* leftToolBar = new QWidget(timelineTab);
    leftToolBar->setFixedHeight(30);
    // Match timeline ruler background color
    leftToolBar->setStyleSheet("QWidget { background-color: #111111; border: none; }");
    QWidget* rightToolBar = new QWidget(timelineTab);
    rightToolBar->setFixedHeight(30);
    rightToolBar->setStyleSheet("QWidget { background-color: #1f1f1f; }");

    // --- left widget ---
    // --------- layer buttons ---------
    QToolBar* layerButtons = new QToolBar(timelineTab);
    layerButtons->setStyleSheet("QToolBar { background: transparent; border: none; } QToolBar::separator { background: transparent; }");
    layerButtons->setIconSize(QSize(22,22));

    // Helper lambda to load SVG icons properly (avoiding symbolic icon rendering)
    auto loadSvgIcon = [](const QString& path, const QSize& size) -> QIcon {
        QIcon icon;
        QSvgRenderer renderer(path);
        qDebug() << "[Timeline] loadSvgIcon" << path << "valid=" << renderer.isValid();
        if (renderer.isValid()) {
            QPixmap pixmap(size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            icon.addPixmap(pixmap);
        } else {
            icon = QIcon(path); // Fallback
        }
        return icon;
    };

    auto loadAndTintSvgIcon = [](const QString& path, const QSize& size, const QColor& color) -> QIcon {
        QSvgRenderer renderer(path);
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        if (renderer.isValid()) {
            renderer.render(&p);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pixmap.rect(), color);
        }
        p.end();
        QIcon icon;
        icon.addPixmap(pixmap);
        return icon;
    };

    auto configureCosmicToolbarButton = [](CosmicButton* b) {
        b->setBordered(false);
        b->setFixedSize(28, 28);
        b->setIconSize(QSize(18, 18));
    };

    auto configureCosmicToolbarToggle = [](CosmicCheckboxToggle* t) {
        t->setMode(CosmicCheckboxToggle::Custom);
        t->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
        t->setBordered(false);
        t->setShowText(false);
        t->setShowIcon(true);
        t->setBackgroundEnabled(true);
        t->setHoverHighlightEnabled(true);
        t->setFixedSize(28, 28);
        t->setIconSize(QSize(18, 18));
    };

    CosmicButton* addLayerButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(addLayerButton);
    addLayerButton->setIcon(loadSvgIcon(":/Interaction/TimelineLayerAdd.svg", QSize(18, 18)));
    addLayerButton->setToolTip(tr("Add Layer"));

    mLayerDeleteButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(mLayerDeleteButton);
    mLayerDeleteButton->setIcon(loadSvgIcon(":/Interaction/TimelineLayerRemove.svg", QSize(18, 18)));
    mLayerDeleteButton->setToolTip(tr("Delete Layer"));

    // Single active-layer opacity slider (Clip Studio style)
    mLayersHeaderOpacitySlider = new CosmicHSlider(timelineTab);
    mLayersHeaderOpacitySlider->setRange(0, 100);
    mLayersHeaderOpacitySlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mLayersHeaderOpacitySlider->setLabelMode(CosmicHSlider::WithLabel);
    mLayersHeaderOpacitySlider->setLabel(tr("100%"));
    mLayersHeaderOpacitySlider->setToolTip(tr("Active layer opacity"));
    mLayersHeaderOpacitySlider->setFocusPolicy(Qt::TabFocus);

    auto syncOpacityFromActiveLayer = [this]() {
        if (!editor() || !editor()->layers() || !editor()->object() || !mLayersHeaderOpacitySlider) {
            return;
        }
        const int idx = editor()->currentLayerIndex();
        if (idx < 0 || idx >= editor()->object()->getLayerCount()) {
            mLayersHeaderOpacitySlider->setEnabled(false);
            return;
        }
        Layer* layer = editor()->object()->getLayer(idx);
        if (!layer || layer->type() == Layer::CAMERA) {
            mLayersHeaderOpacitySlider->setEnabled(false);
            return;
        }
        mLayersHeaderOpacitySlider->setEnabled(true);
        QSignalBlocker b(mLayersHeaderOpacitySlider);
        const int value = qRound(layer->opacity() * 100.0);
        mLayersHeaderOpacitySlider->setValue(value);
        mLayersHeaderOpacitySlider->setLabel(QString::number(value) + "%");
    };

    connect(mLayersHeaderOpacitySlider, &QSlider::valueChanged, this, [this](int v) {
        if (!editor() || !editor()->object()) {
            return;
        }

        if (mLayersHeaderOpacitySlider) {
            mLayersHeaderOpacitySlider->setLabel(QString::number(v) + "%");
        }

        const int idx = editor()->currentLayerIndex();
        if (idx < 0 || idx >= editor()->object()->getLayerCount()) {
            return;
        }
        Layer* layer = editor()->object()->getLayer(idx);
        if (!layer || layer->type() == Layer::CAMERA) {
            return;
        }
        layer->setOpacity(qBound(0.0, v / 100.0, 1.0));
        if (editor()->getScribbleArea()) {
            editor()->getScribbleArea()->onViewChanged();
        }
        updateContent();
    });

    // Top header is now empty — controls moved to the second header below
    layerButtons->setFixedHeight(30);
    layerButtons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout* leftToolBarLayout = new QHBoxLayout();
    leftToolBarLayout->setContentsMargins(0, 0, 0, 0);
    leftToolBarLayout->addWidget(layerButtons, 1);
    leftToolBar->setLayout(leftToolBarLayout);
    // Keep leftToolBar visible as a spacer row to stay aligned with the right panel's first toolbar row

    QWidget* leftTopSpacer = new QWidget(leftWidget);
    mLeftTopSpacer = leftTopSpacer;
    leftTopSpacer->setFixedHeight(12);
    leftTopSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    leftTopSpacer->setStyleSheet("QWidget { background-color: #111111; }");
    leftTopSpacer->hide(); // hidden by default; shown only when top scrollbar is visible
    QAction* newBitmapLayerAct = new QAction(QIcon(":icons/themes/playful/timeline/cell-bitmap.svg"), tr("New Bitmap Layer"), this);
    // Simplify: Add Layer button creates a bitmap layer directly
    connect(addLayerButton, &QPushButton::clicked, this, &TimeLine::newBitmapLayer);

    // Keep header opacity slider synced with active layer.
    syncOpacityFromActiveLayer();

    QGridLayout* leftLayout = new QGridLayout();
    // Row 0 = leftSecondHeader (added below after mCellBankToggle/mDimInactiveLayersCheckBox are created)
    leftLayout->addWidget(leftTopSpacer, 1, 0);
    // Row 2 = leftBottomBar (keyframe spacing buttons — added below)
    leftLayout->addWidget(mLayerList, 3, 0);
    leftLayout->setRowStretch(3, 1);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftWidget->setLayout(leftLayout);

    int minLeftPanelW = 280;
    {
        const int minControlsW = leftToolBar ? leftToolBar->sizeHint().width() : 0;
        minLeftPanelW = qMax(minLeftPanelW, minControlsW);
        leftWidget->setMinimumWidth(minLeftPanelW);
        leftToolBar->setMinimumWidth(minLeftPanelW);
        if (mLayerList) {
            mLayerList->setMinimumWidth(minLeftPanelW);
        }
    }

    // --- right widget ---

    CosmicButton* makeUniqueButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(makeUniqueButton);
    makeUniqueButton->setIcon(loadSvgIcon(":/Interaction/MakeUnique.svg", QSize(18, 18)));
    makeUniqueButton->setToolTip(tr("Make Drawing Unique"));

    CosmicButton* addKeyButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(addKeyButton);
    addKeyButton->setIcon(loadSvgIcon(":/Interaction/KeyframeAdd.svg", QSize(18, 18)));
    addKeyButton->setToolTip(tr("Add Frame"));

    CosmicButton* removeKeyButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(removeKeyButton);
    removeKeyButton->setIcon(loadSvgIcon(":/Interaction/KeyframeRemove.svg", QSize(18, 18)));
    removeKeyButton->setToolTip(tr("Remove Frame"));

    // Tag filter toggles (UI-only) moved into key header area
    CosmicCheckboxToggle* defaultTagFilterToggle = nullptr;
    QVector<CosmicCheckboxToggle*> tagFilterToggles;
    const int tagCount = TimeLineCells::layerTagDefCount();
    tagFilterToggles.reserve(tagCount);

    auto refreshTagToggleIcon = [this, loadAndTintSvgIcon](CosmicCheckboxToggle* t, int tagIndex) {
        if (!t)
            return;
        const QColor c = TimeLineCells::layerTagDefColor(tagIndex);
        const QString iconPath = t->isChecked() ? QStringLiteral(":/Interaction/LayerTag.svg")
                                                : QStringLiteral(":/Interaction/LayerUntagged.svg");
        QColor tint = c;
        if (!t->isChecked()) {
            tint.setAlphaF(0.55);
        }
        t->setIcon(loadAndTintSvgIcon(iconPath, QSize(18, 18), tint));
    };

    auto refreshDefaultTagToggleIcon = [this, loadAndTintSvgIcon](CosmicCheckboxToggle* t) {
        if (!t)
            return;
        QColor tint(0xb3, 0xb3, 0xb3, 0xff);
        if (!t->isChecked()) {
            tint.setAlphaF(0.40);
        }
        const QString iconPath = t->isChecked() ? QStringLiteral(":/Interaction/LayerTag.svg")
                                                : QStringLiteral(":/Interaction/LayerUntagged.svg");
        t->setIcon(loadAndTintSvgIcon(iconPath, QSize(18, 18), tint));
    };

    // Default tag filter (untagged layers)
    defaultTagFilterToggle = new CosmicCheckboxToggle(timelineTab);
    configureCosmicToolbarToggle(defaultTagFilterToggle);
    defaultTagFilterToggle->setToolTip(tr("Default tag (untagged layers)"));
    defaultTagFilterToggle->setBackgroundEnabled(false);
    defaultTagFilterToggle->setHoverHighlightEnabled(false);
    defaultTagFilterToggle->setChecked(TimeLineCells::isDefaultLayerFilterEnabled());
    refreshDefaultTagToggleIcon(defaultTagFilterToggle);
    connect(defaultTagFilterToggle, &CosmicCheckboxToggle::toggled, this, [this, defaultTagFilterToggle, refreshDefaultTagToggleIcon](bool on) {
        TimeLineCells::setDefaultLayerFilterEnabled(on);
        refreshDefaultTagToggleIcon(defaultTagFilterToggle);
        if (mLayerList) {
            mLayerList->updateContent();
        }
        if (mTracks) {
            mTracks->updateContent();
        }
    });

    // Tag visibility toggles (toggle actual layer visibility by tag)
    CosmicCheckboxToggle* defaultTagVisibilityToggle = nullptr;
    QVector<CosmicCheckboxToggle*> tagVisibilityToggles;
    tagVisibilityToggles.reserve(tagCount);

    auto refreshTagVisibilityToggleIcon = [this, loadAndTintSvgIcon](CosmicCheckboxToggle* t, int tagIndex) {
        if (!t)
            return;
        const QColor c = TimeLineCells::layerTagDefColor(tagIndex);
        const QString iconPath = t->isChecked() ? QStringLiteral(":/Interaction/Show.svg")
                                                : QStringLiteral(":/Interaction/Hide.svg");
        QColor tint = c;
        if (!t->isChecked()) {
            tint.setAlphaF(0.55);
        }
        t->setIcon(loadAndTintSvgIcon(iconPath, QSize(18, 18), tint));
    };

    auto refreshDefaultTagVisibilityToggleIcon = [this, loadAndTintSvgIcon](CosmicCheckboxToggle* t) {
        if (!t)
            return;
        QColor tint(0xb3, 0xb3, 0xb3, 0xff);
        if (!t->isChecked()) {
            tint.setAlphaF(0.40);
        }
        const QString iconPath = t->isChecked() ? QStringLiteral(":/Interaction/Show.svg")
                                                : QStringLiteral(":/Interaction/Hide.svg");
        t->setIcon(loadAndTintSvgIcon(iconPath, QSize(18, 18), tint));
    };

    defaultTagVisibilityToggle = new CosmicCheckboxToggle(timelineTab);
    configureCosmicToolbarToggle(defaultTagVisibilityToggle);
    defaultTagVisibilityToggle->setToolTip(tr("Default visibility (untagged layers)"));
    defaultTagVisibilityToggle->setBackgroundEnabled(false);
    defaultTagVisibilityToggle->setHoverHighlightEnabled(false);
    defaultTagVisibilityToggle->setChecked(TimeLineCells::isDefaultLayerVisibilityEnabled());
    refreshDefaultTagVisibilityToggleIcon(defaultTagVisibilityToggle);
    connect(defaultTagVisibilityToggle, &CosmicCheckboxToggle::toggled, this, [this, defaultTagVisibilityToggle, refreshDefaultTagVisibilityToggleIcon](bool on) {
        TimeLineCells::setDefaultLayerVisibilityEnabled(on, editor());
        refreshDefaultTagVisibilityToggleIcon(defaultTagVisibilityToggle);
        if (mLayerList) {
            mLayerList->updateContent();
        }
        if (mTracks) {
            mTracks->updateContent();
        }
    });

    for (int i = 0; i < tagCount; ++i) {
        CosmicCheckboxToggle* t = new CosmicCheckboxToggle(timelineTab);
        configureCosmicToolbarToggle(t);
        t->setBackgroundEnabled(false);
        t->setHoverHighlightEnabled(false);
        t->setToolTip(TimeLineCells::layerTagDefName(i));
        t->setChecked(TimeLineCells::isTagVisibilityEnabled(i));
        refreshTagVisibilityToggleIcon(t, i);
        connect(t, &CosmicCheckboxToggle::toggled, this, [this, t, i, refreshTagVisibilityToggleIcon](bool on) {
            TimeLineCells::setTagVisibilityEnabled(i, on, editor());
            refreshTagVisibilityToggleIcon(t, i);
            if (mLayerList) {
                mLayerList->updateContent();
            }
            if (mTracks) {
                mTracks->updateContent();
            }
        });
        tagVisibilityToggles.push_back(t);
    }

    CosmicButton* resetAllTagVisibilityButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(resetAllTagVisibilityButton);
    resetAllTagVisibilityButton->setToolTip(tr("Reset tag visibility"));
    resetAllTagVisibilityButton->setIcon(loadSvgIcon(":/Interaction/ResetShift.svg", QSize(18, 18)));
    connect(resetAllTagVisibilityButton, &QPushButton::clicked, this, [this, defaultTagVisibilityToggle, tagVisibilityToggles, refreshTagVisibilityToggleIcon, refreshDefaultTagVisibilityToggleIcon]() {
        TimeLineCells::resetTagVisibility(editor());

        if (defaultTagVisibilityToggle) {
            const bool enabled = TimeLineCells::isDefaultLayerVisibilityEnabled();
            {
                QSignalBlocker b(defaultTagVisibilityToggle);
                defaultTagVisibilityToggle->setChecked(enabled);
            }
            refreshDefaultTagVisibilityToggleIcon(defaultTagVisibilityToggle);
        }

        for (int i = 0; i < tagVisibilityToggles.size(); ++i) {
            CosmicCheckboxToggle* t = tagVisibilityToggles[i];
            if (!t)
                continue;
            const bool enabled = TimeLineCells::isTagVisibilityEnabled(i);
            {
                QSignalBlocker b(t);
                t->setChecked(enabled);
            }
            refreshTagVisibilityToggleIcon(t, i);
        }

        if (mLayerList) {
            mLayerList->updateContent();
        }
        if (mTracks) {
            mTracks->updateContent();
        }
    });

    for (int i = 0; i < tagCount; ++i) {
        CosmicCheckboxToggle* t = new CosmicCheckboxToggle(timelineTab);
        configureCosmicToolbarToggle(t);
        t->setBackgroundEnabled(false);
        t->setHoverHighlightEnabled(false);
        t->setToolTip(TimeLineCells::layerTagDefName(i));
        t->setChecked(TimeLineCells::isTagFilterEnabled(i));
        refreshTagToggleIcon(t, i);
        connect(t, &CosmicCheckboxToggle::toggled, this, [this, t, i, refreshTagToggleIcon](bool on) {
            TimeLineCells::setTagFilterEnabled(i, on);
            refreshTagToggleIcon(t, i);
            if (mLayerList) {
                mLayerList->updateContent();
            }
            if (mTracks) {
                mTracks->updateContent();
            }
        });
        tagFilterToggles.push_back(t);
    }

    CosmicButton* resetAllTagsButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(resetAllTagsButton);
    resetAllTagsButton->setToolTip(tr("Reset tag filters"));
    resetAllTagsButton->setIcon(loadSvgIcon(":/Interaction/ResetShift.svg", QSize(18, 18)));
    connect(resetAllTagsButton, &QPushButton::clicked, this, [this, defaultTagFilterToggle, tagFilterToggles, refreshTagToggleIcon, refreshDefaultTagToggleIcon]() {
        TimeLineCells::resetTagFilters();

        if (defaultTagFilterToggle) {
            const bool enabled = TimeLineCells::isDefaultLayerFilterEnabled();
            {
                QSignalBlocker b(defaultTagFilterToggle);
                defaultTagFilterToggle->setChecked(enabled);
            }
            refreshDefaultTagToggleIcon(defaultTagFilterToggle);
        }

        for (int i = 0; i < tagFilterToggles.size(); ++i) {
            CosmicCheckboxToggle* t = tagFilterToggles[i];
            if (!t)
                continue;
            const bool enabled = TimeLineCells::isTagFilterEnabled(i);
            {
                QSignalBlocker b(t);
                t->setChecked(enabled);
            }
            refreshTagToggleIcon(t, i);
        }
        if (mLayerList) {
            mLayerList->updateContent();
        }
        if (mTracks) {
            mTracks->updateContent();
        }
    });

    // Timeline zoom controls with auto-repeat
    CosmicButton* zoomInButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(zoomInButton);
    zoomInButton->setIcon(loadSvgIcon(":/Interaction/ZoomIn.svg", QSize(18, 18)));
    zoomInButton->setToolTip(tr("Zoom In"));
    zoomInButton->setAutoRepeat(true);
    zoomInButton->setAutoRepeatDelay(500);
    zoomInButton->setAutoRepeatInterval(100);

    CosmicButton* zoomOutButton = new CosmicButton(timelineTab);
    configureCosmicToolbarButton(zoomOutButton);
    zoomOutButton->setIcon(loadSvgIcon(":/Interaction/ZoomOut.svg", QSize(18, 18)));
    zoomOutButton->setToolTip(tr("Zoom Out"));
    zoomOutButton->setAutoRepeat(true);
    zoomOutButton->setAutoRepeatDelay(500);
    zoomOutButton->setAutoRepeatInterval(100);
    
    // Dim inactive layers toggle - CosmicCheckboxToggle (text + background always visible)
    mDimInactiveLayersCheckBox = new CosmicCheckboxToggle(timelineTab);
    mDimInactiveLayersCheckBox->setText(tr("Dim inactive"));
    mDimInactiveLayersCheckBox->setToolTip(tr("Dim non-active layers on canvas"));
    mDimInactiveLayersCheckBox->setFixedHeight(28);
    mDimInactiveLayersCheckBox->setMode(CosmicCheckboxToggle::Custom);
    mDimInactiveLayersCheckBox->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
    mDimInactiveLayersCheckBox->setShowText(false);
    mDimInactiveLayersCheckBox->setShowIcon(true);
    mDimInactiveLayersCheckBox->setIcon(QIcon(QStringLiteral(":/Interaction/DimInactive.svg")));
    mDimInactiveLayersCheckBox->setIconSize(QSize(16, 16));
    mDimInactiveLayersCheckBox->setBordered(false);
    mDimInactiveLayersCheckBox->setBackgroundEnabled(true);
    mDimInactiveLayersCheckBox->setHoverHighlightEnabled(true);
    mDimInactiveLayersCheckBox->setIconDimWhenUnchecked(true);

    mDimInactiveLayersCheckBox->setChecked(editor()->preference()->isOn(SETTING::DIM_INACTIVE_LAYERS));

    // Second header row — Add/Remove layer, Opacity slider, Dim Inactive toggle
    {
        QWidget* leftSecondHeader = new QWidget(leftWidget);
        leftSecondHeader->setFixedHeight(30);
        leftSecondHeader->setStyleSheet("QWidget { background-color: #111111; border: none; }");
        QHBoxLayout* lsh = new QHBoxLayout(leftSecondHeader);
        lsh->setContentsMargins(4, 1, 4, 1);
        lsh->setSpacing(4);
        lsh->addWidget(addLayerButton);
        lsh->addWidget(mLayerDeleteButton);
        mLayersHeaderOpacitySlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        lsh->addWidget(mLayersHeaderOpacitySlider, 1);
        mDimInactiveLayersCheckBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        lsh->addWidget(mDimInactiveLayersCheckBox);
        leftLayout->addWidget(leftSecondHeader, 0, 0);
    }

    // Bottom bar — keyframe spacing buttons (1s / 2s / 3s)
    {
        QWidget* leftBottomBar = new QWidget(leftWidget);
        leftBottomBar->setFixedHeight(30);
        leftBottomBar->setStyleSheet("QWidget { background-color: #111111; border: none; }");
        QHBoxLayout* lbb = new QHBoxLayout(leftBottomBar);
        lbb->setContentsMargins(4, 1, 4, 1);
        lbb->setSpacing(4);

        // Warning popup for retime guard failures
        auto showSpacingWarning = [this](const QString& msg) {
            CosmicWarningPopup* popup = new CosmicWarningPopup(this);
            popup->setAttribute(Qt::WA_DeleteOnClose);
            popup->setBodyText(msg);
            popup->setConfirmText(tr("OK"));
            popup->exec();
        };

        // Shared guard + retime logic.
        // ripple=true: keyframes after the last selected one shift by the same delta.
        auto makeSpacingButton = [&](const QString& iconName, int spacing, bool ripple,
                                     const QColor& tint, const QString& tip) -> CosmicButton* {
            CosmicButton* btn = new CosmicButton(leftWidget);
            btn->setBordered(false);
            btn->setFixedHeight(28);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setIconSize(QSize(18, 18));
            btn->setIcon(loadAndTintSvgIcon(
                QStringLiteral(":/Interaction/") + iconName,
                QSize(18, 18), tint));
            btn->setToolTip(tip);
            connect(btn, &CosmicButton::clicked, this, [this, spacing, ripple, showSpacingWarning]() {
                if (!editor() || !editor()->object() || !editor()->layers()) return;
                const int layerIdx = editor()->currentLayerIndex();
                if (layerIdx < 0 || layerIdx >= editor()->object()->getLayerCount()) return;
                Layer* layer = editor()->object()->getLayer(layerIdx);
                if (!layer) return;

                // Snapshot selection sorted ascending (lowest frame = first)
                QList<int> selected = layer->selectedKeyFramesPositions();
                if (selected.isEmpty()) {
                    KeyFrame* kf = layer->getKeyFrameWhichCovers(editor()->currentFrame());
                    if (kf) selected << kf->pos();
                }
                if (selected.size() < 2) return;
                std::sort(selected.begin(), selected.end());

                const int first = selected.first();
                const int last  = selected.last();

                // Guard: no unselected keyframe may exist between first and last
                {
                    bool hasGap = false;
                    layer->foreachKeyFrame([&](KeyFrame* kf) {
                        const int p = kf->pos();
                        if (p > first && p < last && !selected.contains(p))
                            hasGap = true;
                    });
                    if (hasGap) {
                        showSpacingWarning(tr(
                            "Can't retime — there's an unselected keyframe inside your selection.\n"
                            "Fix: select all keyframes between your first and last selected frame, then try again."));
                        return;
                    }
                }

                // Build targets anchored at first
                QList<int> targets;
                for (int i = 0; i < selected.size(); ++i)
                    targets << first + i * spacing;

                // Guard: no target lands on an unselected keyframe
                // For ripple mode, trailing frames will be moved out of the way,
                // so only check against frames that are outside both selected and trailing ranges.
                for (int t : targets) {
                    if (layer->keyExists(t) && !selected.contains(t)) {
                        // In ripple mode, a trailing frame at this position is fine — it will move
                        bool isTrailingConflict = ripple && (t > last);
                        if (!isTrailingConflict) {
                            showSpacingWarning(tr(
                                "Can't retime — the new positions would overlap existing keyframes.\n"
                                "Fix: clear the frames in the way, or choose a different spacing."));
                            return;
                        }
                    }
                }

                const int rippleDelta = targets.last() - last;

                // Collect trailing frames for ripple (snapshot before any moves)
                QList<int> trailing;
                if (ripple) {
                    layer->foreachKeyFrame([&](KeyFrame* kf) {
                        if (kf->pos() > last) trailing << kf->pos();
                    });
                    std::sort(trailing.begin(), trailing.end());
                    if (!trailing.isEmpty() && rippleDelta != 0) {
                        for (int t : trailing) {
                            if (t + rippleDelta < 1) {
                                showSpacingWarning(tr(
                                    "Can't ripple — shifting the following keyframes would push them before frame 1."));
                                return;
                            }
                        }
                    }
                }

                // --- Move selected frames to targets ---
                // Strategy: find a scratch zone that is guaranteed empty right now,
                // then do pass1 (selected → scratch) and pass2 (scratch → targets).
                // We verify the scratch zone is clean before using it.
                auto findCleanScratch = [&](int count) -> int {
                    // Collect all occupied positions
                    QList<int> occupied;
                    layer->foreachKeyFrame([&](KeyFrame* kf) { occupied << kf->pos(); });
                    // Find a contiguous block of `count` free slots starting at candidate
                    int candidate = 100000;
                    while (true) {
                        bool clean = true;
                        for (int i = 0; i < count; ++i) {
                            if (occupied.contains(candidate + i)) { clean = false; break; }
                        }
                        if (clean) return candidate;
                        candidate += count + 1;
                        if (candidate > 10000000) return -1; // safety
                    }
                };

                const int scratch = findCleanScratch(selected.size() + trailing.size());
                if (scratch < 0) return;

                // Park ALL frames that need to move (selected + trailing) into scratch first.
                // This fully decouples the two groups and prevents any mid-move collision.
                const int trailScratchBase = scratch + selected.size();

                // Park selected → scratch[0..n-1]
                for (int i = 0; i < selected.size(); ++i) {
                    layer->deselectAll();
                    layer->setFrameSelected(selected[i], true);
                    layer->moveSelectedFrames((scratch + i) - selected[i]);
                }

                // Park trailing → scratch[n..n+t-1]
                if (ripple && !trailing.isEmpty()) {
                    for (int i = 0; i < trailing.size(); ++i) {
                        layer->deselectAll();
                        layer->setFrameSelected(trailing[i], true);
                        layer->moveSelectedFrames((trailScratchBase + i) - trailing[i]);
                    }
                }

                // Place selected frames at targets (right-to-left)
                for (int i = selected.size() - 1; i >= 0; --i) {
                    layer->deselectAll();
                    layer->setFrameSelected(scratch + i, true);
                    layer->moveSelectedFrames(targets[i] - (scratch + i));
                }

                // Place trailing frames at their final positions (preserving original distances)
                // rippleDelta = targets.last() - last, so each trailing frame shifts by that amount.
                if (ripple && rippleDelta != 0 && !trailing.isEmpty()) {
                    // Build final trailing targets
                    QList<int> trailTargets;
                    for (int t : trailing) trailTargets << t + rippleDelta;

                    if (rippleDelta < 0) {
                        // Moving left — place lowest first
                        for (int i = 0; i < trailing.size(); ++i) {
                            layer->deselectAll();
                            layer->setFrameSelected(trailScratchBase + i, true);
                            layer->moveSelectedFrames(trailTargets[i] - (trailScratchBase + i));
                        }
                    } else {
                        // Moving right — place highest first
                        for (int i = trailing.size() - 1; i >= 0; --i) {
                            layer->deselectAll();
                            layer->setFrameSelected(trailScratchBase + i, true);
                            layer->moveSelectedFrames(trailTargets[i] - (trailScratchBase + i));
                        }
                    }
                } else if (ripple && rippleDelta == 0 && !trailing.isEmpty()) {
                    // No delta — put trailing back where they were
                    for (int i = trailing.size() - 1; i >= 0; --i) {
                        layer->deselectAll();
                        layer->setFrameSelected(trailScratchBase + i, true);
                        layer->moveSelectedFrames(trailing[i] - (trailScratchBase + i));
                    }
                }

                // Restore selection at final positions
                layer->deselectAll();
                for (int t : targets) layer->setFrameSelected(t, true);

                if (editor()->getScribbleArea()) editor()->getScribbleArea()->onViewChanged();
                updateContent();
            });
            return btn;
        };

        const QColor yellow = CosmicColor::yellow();
        const QColor green  = CosmicColor::green();

        // Yellow row — retime only selected frames
        lbb->addWidget(makeSpacingButton("1s.svg", 1, false, yellow, tr("1s — keyframes directly after each other")));
        lbb->addWidget(makeSpacingButton("2s.svg", 2, false, yellow, tr("2s — 1 blank frame between each keyframe")));
        lbb->addWidget(makeSpacingButton("3s.svg", 3, false, yellow, tr("3s — 2 blank frames between each keyframe")));

        // Small separator
        QFrame* sep = new QFrame(leftBottomBar);
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setStyleSheet("QFrame { background: #333333; }");
        lbb->addWidget(sep);

        // Green row — retime selected + ripple trailing frames
        lbb->addWidget(makeSpacingButton("1s.svg", 1, true, green, tr("1s ripple — retime selected and shift all following keyframes")));
        lbb->addWidget(makeSpacingButton("2s.svg", 2, true, green, tr("2s ripple — retime selected and shift all following keyframes")));
        lbb->addWidget(makeSpacingButton("3s.svg", 3, true, green, tr("3s ripple — retime selected and shift all following keyframes")));

        leftLayout->addWidget(leftBottomBar, 2, 0);
    }

    // Shift and Trace - two joined CosmicButtons:
    //   Left  = on/off toggle (checkable, ShiftAndTrace icon, rounded left corners only)
    //   Right = settings popup (settings icon, rounded right corners only)
    const QColor stBaseBg(0x50, 0x50, 0x50, 0xff);
    const QColor stAccent(0x6B, 0x46, 0xC1, 0xff);
    const QColor stActiveIcon(255, 223, 0, 255);   // #FFDF00 yellow when on
    const QColor stNormalIcon(0xb3, 0xb3, 0xb3, 0xff);

    mShiftAndTraceToggleBtn = new CosmicButton(timelineTab);
    mShiftAndTraceToggleBtn->setCheckable(true);
    mShiftAndTraceToggleBtn->setFixedSize(28, 28);
    mShiftAndTraceToggleBtn->setBordered(true);
    mShiftAndTraceToggleBtn->setAccentColor(stAccent);
    mShiftAndTraceToggleBtn->setCornerRadii(true, false, true, false); // round left, flat right
    mShiftAndTraceToggleBtn->setToolTip(tr("Toggle Shift and Trace"));

    mShiftAndTraceSettingsBtn = new CosmicButton(timelineTab);
    mShiftAndTraceSettingsBtn->setFixedSize(28, 28);
    mShiftAndTraceSettingsBtn->setBordered(true);
    mShiftAndTraceSettingsBtn->setAccentColor(stAccent);
    mShiftAndTraceSettingsBtn->setCornerRadii(false, true, false, true); // flat left, round right
    mShiftAndTraceSettingsBtn->setIcon(loadAndTintSvgIcon(":/Interaction/settings.svg", QSize(16, 16), stNormalIcon));
    mShiftAndTraceSettingsBtn->setIconSize(QSize(16, 16));
    mShiftAndTraceSettingsBtn->setToolTip(tr("Shift and Trace Settings"));

    // Helper to refresh the toggle button icon based on checked state
    auto refreshShiftAndTraceIcon = [this, loadAndTintSvgIcon, stActiveIcon, stNormalIcon]() {
        if (!mShiftAndTraceToggleBtn) return;
        const QColor col = mShiftAndTraceToggleBtn->isChecked() ? stActiveIcon : stNormalIcon;
        mShiftAndTraceToggleBtn->setIcon(loadAndTintSvgIcon(":/Interaction/ShiftAndTrace.svg", QSize(16, 16), col));
        mShiftAndTraceToggleBtn->setIconSize(QSize(16, 16));
    };

    // Initial state: enabled (not hidden) by default; sync from manager if available
    const bool initiallyEnabled = editor()->shiftTrace() ? !editor()->shiftTrace()->hideAll() : true;
    mShiftAndTraceToggleBtn->setChecked(initiallyEnabled);
    refreshShiftAndTraceIcon();

    // Keep toggle synced when hideAll changes from the popup's own controls
    if (editor() && editor()->preference()) {
        // (CELL_BANK_VISIBLE is now managed by the canvas-level toggle in mainwindow2)
    }

    auto buildTagHeaderGroup = [this](QWidget* parent,
                                     CosmicCheckboxToggle* defaultToggle,
                                     const QVector<CosmicCheckboxToggle*>& toggles,
                                     CosmicButton* resetButton) -> QWidget* {
        QWidget* container = new QWidget(parent);
        QHBoxLayout* lay = new QHBoxLayout(container);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);
        container->setFixedHeight(30);
        container->setLayoutDirection(Qt::LeftToRight);
        container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        container->setStyleSheet("QWidget { background: transparent; border: none; }");

        if (defaultToggle) {
            lay->addWidget(defaultToggle);
        }
        for (CosmicCheckboxToggle* t : toggles) {
            lay->addWidget(t);
        }
        if (resetButton) {
            lay->addWidget(resetButton);
        }
        return container;
    };

    QWidget* tagFilterGroup = buildTagHeaderGroup(timelineTab, defaultTagFilterToggle, tagFilterToggles, resetAllTagsButton);
    QWidget* tagVisibilityGroup = buildTagHeaderGroup(timelineTab, defaultTagVisibilityToggle, tagVisibilityToggles, resetAllTagVisibilityButton);

    QStackedWidget* tagModeStack = new QStackedWidget(timelineTab);
    tagModeStack->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    tagModeStack->setFixedHeight(30);
    tagModeStack->addWidget(tagFilterGroup);
    tagModeStack->addWidget(tagVisibilityGroup);

    CosmicDropDown* tagModeDropDown = new CosmicDropDown(timelineTab);
    tagModeDropDown->setOptions(QStringList() << tr("Layer UI") << tr("Canvas"));
    tagModeDropDown->setCurrentIndex(0);
    tagModeDropDown->setFixedHeight(24);
    tagModeDropDown->setFixedWidth(100);
    tagModeDropDown->setToolTip(tr("Switch tag mode"));
    connect(tagModeDropDown, &CosmicDropDown::currentIndexChanged, tagModeStack, &QStackedWidget::setCurrentIndex);
    mTagModeDropDown = tagModeDropDown;

    QWidget* tagControlsContainer = new QWidget(timelineTab);
    QHBoxLayout* tagControlsLayout = new QHBoxLayout(tagControlsContainer);
    tagControlsLayout->setContentsMargins(0, 0, 0, 0);
    tagControlsLayout->setSpacing(6);
    tagControlsContainer->setFixedHeight(30);
    tagControlsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tagControlsContainer->setStyleSheet("QWidget { background: transparent; border: none; }");

    tagControlsLayout->addWidget(makeUniqueButton, 0);
    tagControlsLayout->addWidget(addKeyButton, 0);
    tagControlsLayout->addWidget(removeKeyButton, 0);
    tagControlsLayout->addWidget(tagModeDropDown, 0);
    tagControlsLayout->addWidget(tagModeStack, 0);

    // Timeline settings button (will be added to right side)
    mTimelineSettingsButton = new BlenderStyleButton(timelineTab);
    mTimelineSettingsButton->setBorderless(true);
    mTimelineSettingsButton->setIcon(loadSvgIcon(":/Interaction/settings.svg", QSize(22, 22)), QSize(22, 22));
    mTimelineSettingsButton->setToolTip(tr("Timeline Settings"));
    mTimelineSettingsButton->setFixedSize(28, 28);

    // --------- Time controls ---------
    mTimeControls = new TimeControls(this);
    mTimeControls->setIconSize(QSize(22,22));
    mTimeControls->setEditor(editor());
    mTimeControls->initUI();
    mTimeControls->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    mTimeControls->hide();
    updateLength();

    mPlayIcon = loadSvgIcon(":/Interaction/PlaybackPlay.svg", QSize(18, 18));
    mStopIcon = loadSvgIcon(":/Interaction/PlaybackStop.svg", QSize(18, 18));

    auto configureCosmicButton = [](CosmicButton* b) {
        b->setBordered(false);
        b->setFixedSize(28, 28);
        b->setIconSize(QSize(18, 18));
    };
    auto configureCosmicToggle = [](CosmicCheckboxToggle* t) {
        t->setMode(CosmicCheckboxToggle::Custom);
        t->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
        t->setBordered(false);
        t->setShowText(false);
        t->setShowIcon(true);
        t->setBackgroundEnabled(true);
        t->setHoverHighlightEnabled(true);
        t->setFixedSize(28, 28);
        t->setIconSize(QSize(18, 18));
    };

    mFirstFrameButton = new CosmicButton(transportWidget);
    configureCosmicButton(mFirstFrameButton);
    mFirstFrameButton->setIcon(loadSvgIcon(":/Interaction/FrameFirst.svg", QSize(18, 18)));
    mFirstFrameButton->setToolTip(tr("Jump to first frame"));

    mPrevKeyframeButton = new CosmicButton(transportWidget);
    configureCosmicButton(mPrevKeyframeButton);
    mPrevKeyframeButton->setIcon(loadSvgIcon(":/Interaction/FrameBackward.svg", QSize(18, 18)));
    mPrevKeyframeButton->setToolTip(tr("Previous keyframe"));

    mPlayToggle = new CosmicCheckboxToggle(transportWidget);
    configureCosmicToggle(mPlayToggle);
    mPlayToggle->setIcon(mPlayIcon);
    mPlayToggle->setToolTip(tr("Play"));

    mNextKeyframeButton = new CosmicButton(transportWidget);
    configureCosmicButton(mNextKeyframeButton);
    mNextKeyframeButton->setIcon(loadSvgIcon(":/Interaction/FrameForward.svg", QSize(18, 18)));
    mNextKeyframeButton->setToolTip(tr("Next keyframe"));

    mLastFrameButton = new CosmicButton(transportWidget);
    configureCosmicButton(mLastFrameButton);
    mLastFrameButton->setIcon(loadSvgIcon(":/Interaction/FrameLast.svg", QSize(18, 18)));
    mLastFrameButton->setToolTip(tr("Jump to last frame"));

    mLoopToggle = new CosmicCheckboxToggle(transportWidget);
    configureCosmicToggle(mLoopToggle);
    mLoopToggle->setIcon(loadSvgIcon(":/Interaction/PlaybackLoop.svg", QSize(18, 18)));
    mLoopToggle->setToolTip(tr("Loop"));

    playbackButtonsLayout->addWidget(mFirstFrameButton);
    playbackButtonsLayout->addWidget(mPrevKeyframeButton);
    playbackButtonsLayout->addWidget(mPlayToggle);
    playbackButtonsLayout->addWidget(mNextKeyframeButton);
    playbackButtonsLayout->addWidget(mLastFrameButton);
    playbackButtonsLayout->addWidget(mLoopToggle);

    transportLayout->addWidget(mShiftAndTraceToggleBtn);
    transportLayout->addWidget(mShiftAndTraceSettingsBtn);
    transportLayout->addStretch(1);
    transportLayout->addWidget(playbackButtonsWrap);
    transportLayout->addStretch(1);

    connect(mFirstFrameButton, &QPushButton::clicked, this, [this]() {
        editor()->scrubTo(1);
        editor()->playback()->stopSounds();
    });
    connect(mPrevKeyframeButton, &QPushButton::clicked, editor(), &Editor::scrubPreviousKeyFrame);
    connect(mNextKeyframeButton, &QPushButton::clicked, editor(), &Editor::scrubNextKeyFrame);
    connect(mLastFrameButton, &QPushButton::clicked, this, [this]() {
        const int end = qMax(1, editor()->layers()->animationLength());
        editor()->scrubTo(end);
    });
    connect(mPlayToggle, &QAbstractButton::clicked, this, [this](bool) {
        onPlayToggleClicked();
    });
    connect(mLoopToggle, &QAbstractButton::toggled, this, [this](bool checked) {
        if (!editor() || !editor()->playback()) {
            return;
        }

        // UI is source-of-truth:
        // - Shot: loop means PlaybackManager looping
        // - From Current / All: loop means traversal looping (PlaybackManager looping must stay off)
        if (mShotPlaylistMode != 0)
        {
            mShotPlaylistLoopingEnabled = checked;
            if (editor()->playback()->isLooping()) {
                mShotPlaylistForcingPlaybackLoopOff = true;
                editor()->playback()->setLooping(false);
                mShotPlaylistForcingPlaybackLoopOff = false;
            }
            return;
        }

        editor()->playback()->setLooping(checked);
    });

    // Header row layout: tab bar (left), transport (fills remaining space), right controls (far right)
    QWidget* tabBarWrap = new QWidget(tabRow);
    QHBoxLayout* tabBarWrapLayout = new QHBoxLayout(tabBarWrap);
    tabBarWrapLayout->setContentsMargins(0, 0, 0, 0);
    tabBarWrapLayout->setSpacing(0);
    tabBarWrapLayout->addWidget(tabBar);
    tabBarWrap->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    tabBarWrap->setFixedWidth(tabBarWrap->sizeHint().width());
    tabBarWrap->hide(); // tab label hidden until Scenes and Shots is ready

    QHBoxLayout* tabRowLayout = new QHBoxLayout(tabRow);
    tabRowLayout->setContentsMargins(0, 0, 0, 0);
    tabRowLayout->setSpacing(0);
    tabRowLayout->addWidget(tabBarWrap);
    QWidget* tabRowSplitSpacer = new QWidget(tabRow);
    tabRowSplitSpacer->setFixedWidth(0);
    tabRowLayout->addWidget(tabRowSplitSpacer);
    // transportWidget moved to rightLayout row 0 — not added here
    
    // Add right-side controls (zoom, FPS, timestamp, settings) into transportWidget
    QWidget* headerRightContainer = new QWidget(transportWidget);
    QHBoxLayout* headerRightLayout = new QHBoxLayout(headerRightContainer);
    headerRightLayout->setContentsMargins(6, 0, 6, 0);
    headerRightLayout->setSpacing(6);
    headerRightContainer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    
    // Add zoom controls to header
    headerRightLayout->addWidget(zoomInButton);
    headerRightLayout->addWidget(zoomOutButton);
    
    // Add trailing controls (FPS, timestamp) from time controls to header
    if (QWidget* trailing = mTimeControls->trailingWidget()) {
        trailing->setParent(headerRightContainer);
        headerRightLayout->addWidget(trailing);
    }
    
    // Add separator before settings button
    QFrame* headerSeparator = new QFrame(headerRightContainer);
    headerSeparator->setFrameShape(QFrame::VLine);
    headerSeparator->setFrameShadow(QFrame::Sunken);
    headerSeparator->setStyleSheet("QFrame { background-color: #444444; }");
    headerRightLayout->addWidget(headerSeparator);
    
    // Add settings button at the far right of header
    headerRightLayout->addWidget(mTimelineSettingsButton);
    
    transportLayout->addWidget(headerRightContainer);

    // tabRow now only holds the (hidden) tab bar wrap

    QHBoxLayout* rightToolBarLayout = new QHBoxLayout();
    rightToolBarLayout->addWidget(tagControlsContainer, 1);
    rightToolBarLayout->setContentsMargins(0, 0, 0, 0);
    rightToolBarLayout->setSpacing(0);
    rightToolBar->setLayout(rightToolBarLayout);

    // Move transportWidget into the right panel so it sits in row 0 (top strip)
    // and rightToolBar (tags/Layer UI) drops to row 1 (the second strip).
    transportWidget->setParent(rightWidget);

    QGridLayout* rightLayout = new QGridLayout();
    rightLayout->addWidget(transportWidget, 0, 0, 1, 3);   // row 0: playback/transport
    rightLayout->addWidget(rightToolBar, 1, 0, 1, 3);       // row 1: tags, Layer UI dropdown

    rightLayout->addWidget(mHScrollbarTop, 2, 0, 1, 3);
    rightLayout->addWidget(mVScrollbar, 3, 0);
    rightLayout->addWidget(mTracks, 3, 1);
    rightLayout->addWidget(mVScrollbarRight, 3, 2);
    rightLayout->addWidget(mHScrollbar, 4, 0, 1, 3);
    rightLayout->setColumnStretch(1, 1);
    rightLayout->setColumnMinimumWidth(0, mVScrollbar->sizeHint().width());
    rightLayout->setColumnMinimumWidth(2, mVScrollbarRight->width());
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    // Cell Bank panel is now a standalone floating panel managed by mainwindow2.
    rightWidget->setLayout(rightLayout);

    if (editor() && editor()->preference())
    {
        auto applyMode = [this]() {
            if (!editor() || !editor()->preference()) {
                return;
            }
            int mode = editor()->preference()->getInt(SETTING::PLAYBACK_PERFORMANCE_MODE);
            if (mode < 0 || mode > 1) {
                mode = 0;
            }

            if (mPlaybackPerfDropDown) {
                QSignalBlocker b(mPlaybackPerfDropDown);
                mPlaybackPerfDropDown->setCurrentIndex(mode);
            }

            const quint64 cacheBytes = (mode == 1)
                                          ? (1024ull * 1024ull * 1024ull)
                                          : (512ull * 1024ull * 1024ull);
            CellBank::setBitmapCacheLimitBytes(cacheBytes);
        };

        if (mPlaybackPerfDropDown) {
            connect(mPlaybackPerfDropDown, &CosmicDropDown::currentIndexChanged, this, [this, applyMode](int idx) {
                if (!editor() || !editor()->preference()) {
                    return;
                }
                editor()->preference()->set(SETTING::PLAYBACK_PERFORMANCE_MODE, idx);
                applyMode();
            });
        }

        connect(editor()->preference(), &PreferenceManager::optionChanged, this, [applyMode](SETTING e) {
            if (e == SETTING::PLAYBACK_PERFORMANCE_MODE) {
                applyMode();
            }
        });

        applyMode();
    }

    // --- Splitter ---
    QSplitter* splitter = new QSplitter(timelineTab);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setSizes(QList<int>() << minLeftPanelW << 600);
    splitter->setCollapsible(0, false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStyleSheet("QSplitter::handle { background-color: #2b2b2b; }");


    QGridLayout* lay = new QGridLayout();
    lay->addWidget(splitter, 0, 0);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    timelineTab->setLayout(lay);

    // Transport anchor no longer needed — transportWidget is in rightLayout
    auto updateTransportAnchor = [tabRowSplitSpacer]() {
        Q_UNUSED(tabRowSplitSpacer);
    };
    QTimer::singleShot(0, this, [updateTransportAnchor]() {
        updateTransportAnchor();
    });
    connect(splitter, &QSplitter::splitterMoved, this, [updateTransportAnchor](int, int) { updateTransportAnchor(); });
    dockLayout->addWidget(tabRow);
    dockLayout->addWidget(tabStack);
    setDockContentWidget(dockContent);

    mScrollingStoppedTimer = new QTimer();
    mScrollingStoppedTimer->setSingleShot(true);

    // Horizontal scrollbars share a single source of truth (TimeLineCells offset)
    connect(mHScrollbar, &QScrollBar::valueChanged, mTracks, &TimeLineCells::hScrollChange);
    connect(mHScrollbarTop, &QScrollBar::valueChanged, mTracks, &TimeLineCells::hScrollChange);
    connect(mTracks, &TimeLineCells::offsetChanged, mHScrollbar, &QScrollBar::setValue);
    connect(mTracks, &TimeLineCells::offsetChanged, mHScrollbarTop, &QScrollBar::setValue);

    // Keep bottom/top scrollbars mirrored without feedback loops.
    connect(mHScrollbar, &QScrollBar::rangeChanged, this, [this](int min, int max) {
        if (!mHScrollbarTop) {
            return;
        }
        QSignalBlocker b(mHScrollbarTop);
        mHScrollbarTop->setRange(min, max);
        mHScrollbarTop->setPageStep(mHScrollbar ? mHScrollbar->pageStep() : mHScrollbarTop->pageStep());
    });
    connect(mHScrollbar, &QScrollBar::valueChanged, this, [this](int v) {
        if (!mHScrollbarTop || !mHScrollbar) {
            return;
        }
        QSignalBlocker b(mHScrollbarTop);
        mHScrollbarTop->setValue(v);
    });
    connect(mHScrollbarTop, &QScrollBar::valueChanged, this, [this](int v) {
        if (!mHScrollbarTop || !mHScrollbar) {
            return;
        }
        QSignalBlocker b(mHScrollbar);
        mHScrollbar->setValue(v);
    });

    connect(mVScrollbar, &QScrollBar::valueChanged, mTracks, &TimeLineCells::vScrollChange);
    connect(mVScrollbar, &QScrollBar::valueChanged, mLayerList, &TimeLineCells::vScrollChange);
    connect(mVScrollbar, &QScrollBar::valueChanged, this, &TimeLine::onScrollbarValueChanged);
    // Mirror primary → right so they never desync
    connect(mVScrollbar, &QScrollBar::valueChanged, this, [this](int v) {
        if (!mVScrollbarRight) return;
        QSignalBlocker b(mVScrollbarRight);
        mVScrollbarRight->setValue(v);
    });
    connect(mVScrollbar, &QScrollBar::rangeChanged, this, [this](int min, int max) {
        if (!mVScrollbarRight || !mVScrollbar) return;
        QSignalBlocker b(mVScrollbarRight);
        mVScrollbarRight->setRange(min, max);
        mVScrollbarRight->setPageStep(mVScrollbar->pageStep());
        mVScrollbarRight->setValue(mVScrollbar->value());
    });
    // Right side drives primary — primary's valueChanged handles everything else
    connect(mVScrollbarRight, &QScrollBar::valueChanged, this, [this](int v) {
        if (!mVScrollbar || !mVScrollbarRight) return;
        mVScrollbar->setValue(v); // fires mVScrollbar::valueChanged → cells + mirror back (blocked)
    });
    connect(mScrollingStoppedTimer, &QTimer::timeout, mLayerList, &TimeLineCells::onScrollingVerticallyStopped);

    connect(splitter, &QSplitter::splitterMoved, this, &TimeLine::updateLength);

    connect(addKeyButton, &QPushButton::clicked, this, &TimeLine::insertKeyClick);
    connect(removeKeyButton, &QPushButton::clicked, this, &TimeLine::removeKeyClick);
    connect(makeUniqueButton, &QPushButton::clicked, this, &TimeLine::makeUniqueDrawingClick);
    // Zoom button click handlers with auto-repeat
    connect(zoomInButton, &QPushButton::clicked, this, [this]() {
        int currentFrameSize = mTracks->getFrameSize();
        int newFrameSize = qMin(55, currentFrameSize + 2);
        mTracks->setFrameSize(newFrameSize);
        // Save frame size to preferences
        editor()->preference()->set(SETTING::FRAME_SIZE, newFrameSize);
    });
    connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
        int currentFrameSize = mTracks->getFrameSize();
        int newFrameSize = qMax(25, currentFrameSize - 2);
        mTracks->setFrameSize(newFrameSize);
        // Save frame size to preferences
        editor()->preference()->set(SETTING::FRAME_SIZE, newFrameSize);
    });
    connect(mDimInactiveLayersCheckBox, &CosmicCheckboxToggle::toggled, this, &TimeLine::onDimInactiveLayersToggled);
    connect(mShiftAndTraceToggleBtn, &CosmicButton::toggled, this, [this, refreshShiftAndTraceIcon](bool checked) {
        refreshShiftAndTraceIcon();
        onShiftAndTraceToggled(checked);
    });
    connect(mShiftAndTraceSettingsBtn, &CosmicButton::clicked, this, [this]() {
        // Toggle the shift and trace popup open/closed
        if (editor() && editor()->preference()) {
            const bool wasVisible = editor()->preference()->isOn(SETTING::SHIFT_TRACE_WINDOW_VISIBLE);
            editor()->preference()->set(SETTING::SHIFT_TRACE_WINDOW_VISIBLE, !wasVisible);
        }
    });
    // Keep toggle button in sync when the popup's own toggle changes hideAll
    if (editor() && editor()->shiftTrace()) {
        connect(editor()->shiftTrace(), &ShiftTraceManager::hideAllChanged, this,
                [this, refreshShiftAndTraceIcon](bool hideAll) {
                    if (!mShiftAndTraceToggleBtn) return;
                    QSignalBlocker b(mShiftAndTraceToggleBtn);
                    mShiftAndTraceToggleBtn->setChecked(!hideAll);
                    refreshShiftAndTraceIcon();
                });
    }
    connect(mTimelineSettingsButton, &BlenderStyleButton::clicked, this, &TimeLine::onTimelineSettingsClicked);

    connect(mTimeControls, &TimeControls::soundToggled, this, &TimeLine::soundClick);
    connect(mTimeControls, &TimeControls::fpsChanged, this, &TimeLine::fpsChanged);
    connect(mTimeControls, &TimeControls::fpsChanged, this, &TimeLine::updateLength);
    connect(editor(), &Editor::scrubbed, mTimeControls, &TimeControls::updateTimecodeLabel);
    connect(mTimeControls, &TimeControls::fpsChanged, mTimeControls, &TimeControls::setFps);
    connect(this, &TimeLine::fpsChanged, mTimeControls, &TimeControls::setFps);

    connect(newBitmapLayerAct, &QAction::triggered, this, &TimeLine::newBitmapLayer);
    connect(mLayerDeleteButton, &QPushButton::clicked, this, &TimeLine::deleteCurrentLayerClick);

    connect(mLayerList, &TimeLineCells::mouseMovedY, mLayerList, &TimeLineCells::setMouseMoveY);
    connect(mLayerList, &TimeLineCells::mouseMovedY, mTracks, &TimeLineCells::setMouseMoveY);
    connect(mTracks, &TimeLineCells::lengthChanged, this, &TimeLine::updateLength);
    connect(mTracks, &TimeLineCells::selectionChanged, this, &TimeLine::selectionChanged);
    connect(mTracks, &TimeLineCells::insertNewKeyFrame, this, &TimeLine::insertKeyClick);

    connect(editor(), &Editor::scrubbed, this, &TimeLine::updateFrame);
    connect(editor(), &Editor::frameModified, this, &TimeLine::updateContent);
    connect(editor(), &Editor::framesModified, this, &TimeLine::updateContent);

    LayerManager* layer = editor()->layers();
    connect(layer, &LayerManager::layerCountChanged, this, &TimeLine::updateLayerNumber);
    connect(layer, &LayerManager::currentLayerChanged, this, &TimeLine::onCurrentLayerChanged);
    mNumLayers = layer->count();

    // Initialize vertical scrollbar range/pageStep now that the UI is fully constructed.
    // Without this, the default max=1 makes the thumb look like there is off-screen content.
    updateLayerNumber(mNumLayers);
    updateVerticalScrollbarPosition();

    scrubbing = false;
}

void TimeLine::updateUI()
{
    updateContent();

    // Apply saved scrollbar layout on first updateUI (after editor is ready)
    if (editor() && editor()->preference()) {
        const int hMode = editor()->preference()->getInt(SETTING::TIMELINE_H_SCROLLBAR_MODE);
        const int vMode = editor()->preference()->getInt(SETTING::TIMELINE_V_SCROLLBAR_MODE);
        applyScrollbarSettings(hMode, vMode);
    }
}

void TimeLine::updateUICached()
{
    // During scrubbing only the playhead position changes — use the cheaper
    // partial-region update (two frame-width strips) instead of a full repaint.
    const int frame = editor()->currentFrame();
    if (mTracks && mLastUpdatedFrame != frame) {
        mTracks->updateFrame(mLastUpdatedFrame);
        mTracks->updateFrame(frame);
        mLastUpdatedFrame = frame;
    } else {
        mLayerList->update();
        mTracks->update();
    }
}

/** Extends the timeline frame length if necessary
 *
 *  @param[in] frame The new animation length
 */
void TimeLine::extendLength(int frame)
{
    mContentLength = frame;
    checkTimelineExpansion(mLastUpdatedFrame);
}

void TimeLine::checkTimelineExpansion(int currentFrame)
{
    // maxEffectiveFrame is either the playhead position, the last piece of content (keyframe), 
    // or the active playback range end.
    int rangeEnd = getRangeUpper();
    int maxEffectiveFrame = std::max(currentFrame, mContentLength);
    
    if (rangeEnd > 0) {
        maxEffectiveFrame = std::max(maxEffectiveFrame, rangeEnd);
    }

    // Calculate target length based on 250-frame chunks
    // logic: if maxEffectiveFrame is within last 70 frames of a chunk, add another chunk.
    
    // We want the total length to be a multiple of 250.
    // The "trigger" for expansion is when we are within 70 frames of the END of the current chunk layout.
    // Actually, the user requirement is: 
    // "Set a detector range from the end frame.detector range of 70 frames.if the playhead or any keyframe/cell is in this range.then add 250 more frames to the timtline."
    
    // Let's reverse that logic to find the required length.
    // We need a length L such that maxEffectiveFrame < L - 70.
    // So maxEffectiveFrame + 70 < L.
    // And L must be a multiple of 250.
    
    // So L = ceil((maxEffectiveFrame + 70) / 250.0) * 250.
    
    int targetLength = static_cast<int>(std::ceil((maxEffectiveFrame + 70) / 250.0) * 250);
    
    // Ensure minimum of 250 frames
    if (targetLength < 250) targetLength = 250;
    
    int currentLength = mTracks->getFrameLength();
    
    if (currentLength != targetLength)
    {
        if (targetLength > 9999)
            targetLength = 9999;
            
        mTracks->setFrameLength(targetLength);
        updateLength();
    }
}

void TimeLine::resizeEvent(QResizeEvent*)
{
    updateLayerView();
}

void TimeLine::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange) return;

    if (mTagModeDropDown) {
        const int idx = mTagModeDropDown->currentIndex();
        QSignalBlocker b(mTagModeDropDown);
        auto safetr = [this](const char* src) -> QString {
            const QString t = tr(src);
            return t.isEmpty() ? QString::fromUtf8(src) : t;
        };
        mTagModeDropDown->setOptions(QStringList() << safetr("Layer UI") << safetr("Canvas"));
        mTagModeDropDown->setToolTip(safetr("Switch tag mode"));
        mTagModeDropDown->setCurrentIndex(idx);
    }
}

void TimeLine::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        mHScrollbar->event(event);
    }
    else
    {
        mVScrollbar->event(event);
    }
}

void TimeLine::onScrollbarValueChanged()
{
    // After the scrollbar has been updated, prepare to trigger stopped event
    mScrollingStoppedTimer->start(150);
}

void TimeLine::onDimInactiveLayersToggled(bool checked)
{
    // Persist preference
    editor()->preference()->set(SETTING::DIM_INACTIVE_LAYERS, checked);
    // Defer canvas refresh to avoid lag during toggle animation
    QTimer::singleShot(0, this, [this]() {
        editor()->updateFrame();
        updateContent();
    });
}

void TimeLine::onShiftAndTraceToggled(bool checked)
{
    // Left button: toggle the global Shift & Trace feature on/off
    if (editor() && editor()->shiftTrace()) {
        editor()->shiftTrace()->setHideAll(!checked);
    }
}

void TimeLine::onCellBankToggled(bool checked)
{
    // Cell Bank panel is now managed by mainwindow2 as a standalone floating panel.
    // This slot is kept for ABI compatibility but does nothing.
    Q_UNUSED(checked)
}

void TimeLine::updateFrame(int frameNumber)
{
    Q_ASSERT(mTracks);


    mTracks->updateFrame(mLastUpdatedFrame);
    mTracks->updateFrame(frameNumber);

    mLastUpdatedFrame = frameNumber;
    checkTimelineExpansion(frameNumber);
}

void TimeLine::updateLayerView()
{
    // Use visible (non-camera) layers for UI pagination
    int pageDisplay = (mTracks->height() - mTracks->getOffsetY()) / mTracks->getLayerHeight();

    if (pageDisplay < 1) {
        pageDisplay = 1;
    }

    int visibleCount = mLayerList->getVisibleLayerCount();

    mVScrollbar->setMinimum(0);
    mVScrollbar->setPageStep(pageDisplay);
    mVScrollbar->setSingleStep(1);
    mVScrollbar->setMaximum(qMax(0, visibleCount - pageDisplay));

    const bool needsScroll = (mVScrollbar->maximum() > 0);
    const QString pillVStyle = QStringLiteral(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 26px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );
    const QString pillVStyleNoScroll = QStringLiteral(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 9999px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );

    // If there's no off-screen content, keep the bar visually "full" (thumb fills the track)
    // by leaving it enabled with range 0..0 and a large pageStep.
    if (!needsScroll) {
        mVScrollbar->setPageStep(qMax(1, visibleCount));
        mVScrollbar->setValue(0);
        mVScrollbar->setStyleSheet(pillVStyleNoScroll);
    } else {
        mVScrollbar->setStyleSheet(pillVStyle);
    }

    if (mVScrollbarRight) {
        QSignalBlocker b(mVScrollbarRight);
        mVScrollbarRight->setRange(mVScrollbar->minimum(), mVScrollbar->maximum());
        mVScrollbarRight->setPageStep(mVScrollbar->pageStep());
        mVScrollbarRight->setSingleStep(mVScrollbar->singleStep());
        mVScrollbarRight->setValue(mVScrollbar->value());
        if (!needsScroll) {
            mVScrollbarRight->setStyleSheet(pillVStyleNoScroll);
        } else {
            mVScrollbarRight->setStyleSheet(pillVStyle);
        }
    }
    updateContent();
}

void TimeLine::updateLayerNumber(int numberOfLayers)
{
    Q_UNUSED(numberOfLayers);
    // Recompute using visible count to reflect camera-hidden UI
    mNumLayers = mLayerList->getVisibleLayerCount();
    updateLayerView();
}

void TimeLine::updateLength()
{
    checkTimelineExpansion(mLastUpdatedFrame);
    int frameLength = mTracks->getFrameLength();
    const int maxOffset = qMax(0, frameLength - mTracks->width() / mTracks->getFrameSize());
    mHScrollbar->setRange(0, maxOffset);
    if (mHScrollbarTop) {
        QSignalBlocker b(mHScrollbarTop);
        mHScrollbarTop->setRange(0, maxOffset);
        mHScrollbarTop->setPageStep(mHScrollbar->pageStep());
        mHScrollbarTop->setSingleStep(mHScrollbar->singleStep());
    }
    mTimeControls->updateLength(frameLength);
    updateContent();
}

void TimeLine::updateContent()
{
    mLayerList->updateContent();
    mTracks->updateContent();
    update();
}

void TimeLine::setLoop(bool loop)
{
    // In playlist traversal modes, "loop" is interpreted as looping the traversal,
    // not PlaybackManager's internal looping.
    if (mShotPlaylistActive && mShotPlaylistMode != 0)
    {
        // If this call originates from us forcing PlaybackManager looping OFF,
        // do not overwrite the user's traversal-loop intent.
        if (!mShotPlaylistForcingPlaybackLoopOff) {
            mShotPlaylistLoopingEnabled = loop;
        }

        // Keep PlaybackManager looping OFF so end-of-shot produces a stop event,
        // which we use to advance to the next shot.
        if (editor() && editor()->playback() && editor()->playback()->isLooping()) {
            mShotPlaylistForcingPlaybackLoopOff = true;
            editor()->playback()->setLooping(false);
            mShotPlaylistForcingPlaybackLoopOff = false;
        }

        if (mTimeControls) {
            mTimeControls->setLoop(mShotPlaylistLoopingEnabled);
        }
        if (mLoopToggle) {
            QSignalBlocker b(mLoopToggle);
            mLoopToggle->setChecked(mShotPlaylistLoopingEnabled);
        }
        return;
    }

    if (mTimeControls) {
        mTimeControls->setLoop(loop);
    }

    if (mLoopToggle) {
        QSignalBlocker b(mLoopToggle);
        mLoopToggle->setChecked(loop);
    }
}

void TimeLine::setPlaying(bool isPlaying)
{
    // During playlist traversal (From Current / All), PlaybackManager will emit
    // playStateChanged(false) at the end of each shot. We must not flip the
    // transport UI to "stopped" while we are auto-advancing.
    if (!isPlaying && mShotPlaylistActive && mShotPlaylistMode != 0) {
        return;
    }

    if (mTimeControls) {
        mTimeControls->updatePlayState();
    }

    if (mPlayToggle) {
        QSignalBlocker b(mPlayToggle);
        mPlayToggle->setChecked(isPlaying);
        mPlayToggle->setIcon(isPlaying ? mStopIcon : mPlayIcon);
        mPlayToggle->setToolTip(isPlaying ? tr("Stop") : tr("Play"));
    }

    const bool lockScenesShots = isPlaying;
    if (mScenesGridContainer) {
        mScenesGridContainer->setEnabled(!lockScenesShots);
    }
    if (mScenesScrollArea) {
        mScenesScrollArea->setEnabled(!lockScenesShots);
    }
    if (mScenesAddButton) {
        mScenesAddButton->setEnabled(!lockScenesShots);
    }
    if (mScenesRemoveButton) {
        mScenesRemoveButton->setEnabled(!lockScenesShots);
    }
    if (mBreadcrumbSceneButton) {
        mBreadcrumbSceneButton->setEnabled(!lockScenesShots);
    }
    if (mPlaybackTypeDropDown) {
        mPlaybackTypeDropDown->setEnabled(!lockScenesShots);
    }
}

void TimeLine::setRangeState(bool range)
{
    mTimeControls->setRangeState(range);
}

int TimeLine::getRangeLower()
{
    return mTimeControls->getRangeLower();
}

int TimeLine::getRangeUpper()
{
    return mTimeControls->getRangeUpper();
}

void TimeLine::setRangeLower(int frame)
{
    mTimeControls->setRangeLower(frame);
}

void TimeLine::setRangeUpper(int frame)
{
    mTimeControls->setRangeUpper(frame);
}

void TimeLine::onObjectLoaded()
{
    mTimeControls->updateUI();
    updateLayerNumber(editor()->layers()->count());
}

void TimeLine::onCurrentLayerChanged()
{
    updateVerticalScrollbarPosition();
    mLayerDeleteButton->setEnabled(editor()->layers()->canDeleteLayer(editor()->currentLayerIndex()));

    if (mLayersHeaderOpacitySlider && editor() && editor()->object())
    {
        const int idx = editor()->currentLayerIndex();
        if (idx >= 0 && idx < editor()->object()->getLayerCount())
        {
            Layer* layer = editor()->object()->getLayer(idx);
            if (layer && layer->type() != Layer::CAMERA)
            {
                mLayersHeaderOpacitySlider->setEnabled(true);
                QSignalBlocker b(mLayersHeaderOpacitySlider);
                const int value = qRound(layer->opacity() * 100.0);
                mLayersHeaderOpacitySlider->setValue(value);
                mLayersHeaderOpacitySlider->setLabel(QString::number(value) + "%");
            }
            else
            {
                mLayersHeaderOpacitySlider->setEnabled(false);
            }
        }
        else
        {
            mLayersHeaderOpacitySlider->setEnabled(false);
        }
    }
}

void TimeLine::updateVerticalScrollbarPosition()
{
    // invert index so 0 is at the top (based on visible layers only)
    int currentActualIndex = editor()->currentLayerIndex();
    int visibleIdx = mLayerList->toVisibleIndex(currentActualIndex);
    if (visibleIdx < 0) {
        // Current layer is a camera layer; keep scrollbar unchanged
        return;
    }

    int idx = mNumLayers - visibleIdx - 1;
    // number of visible layers
    int height = mNumLayers - mVScrollbar->maximum();
    // scroll bar position/offset
    int pos = mVScrollbar->value();

    if (idx < pos) // above visible area
    {
        mVScrollbar->setValue(idx);
    }
    else if (idx >= pos + height) // below visible area
    {
        mVScrollbar->setValue(idx - height + 1);
    }
}

void TimeLine::onTimelineSettingsClicked()
{
    if (!mSettingsDialog)
    {
        mSettingsDialog = new TimelineSettingsPopup(nullptr);

        // ── Playback range ────────────────────────────────────────────────
        const int maxFrame = qMax(240, mContentLength);
        mSettingsDialog->loopStartField()->setRange(1, maxFrame);
        mSettingsDialog->loopEndField()->setRange(2, maxFrame);

        connect(mSettingsDialog->loopStartField(), &CosmicInputField::valueCommitted,
                mTimeControls, [this](double v) {
                    mTimeControls->setRangeLower(static_cast<int>(v));
                });
        connect(mSettingsDialog->loopEndField(), &CosmicInputField::valueCommitted,
                mTimeControls, [this](double v) {
                    mTimeControls->setRangeUpper(static_cast<int>(v));
                });

        // ── Timecode display ──────────────────────────────────────────────
        {
            connect(mSettingsDialog->timestampDropDown(), &CosmicDropDown::currentIndexChanged,
                    this, [this](int index) {
                        if (mTimeControls) mTimeControls->setTimecodeMode(index);
                    });
        }

        // ── Scrollbar tiles ───────────────────────────────────────────────
        {
            auto syncH = [this](int mode) {
                mSettingsDialog->hScrollBottom()->setActive(mode == 0);
                mSettingsDialog->hScrollTop()->setActive(mode == 1);
                mSettingsDialog->hScrollBoth()->setActive(mode == 2);
                editor()->preference()->set(SETTING::TIMELINE_H_SCROLLBAR_MODE, mode);
                applyScrollbarSettings(mode,
                    editor()->preference()->getInt(SETTING::TIMELINE_V_SCROLLBAR_MODE));
            };
            auto syncV = [this](int mode) {
                mSettingsDialog->vScrollLeft()->setActive(mode == 0);
                mSettingsDialog->vScrollRight()->setActive(mode == 1);
                mSettingsDialog->vScrollBoth()->setActive(mode == 2);
                editor()->preference()->set(SETTING::TIMELINE_V_SCROLLBAR_MODE, mode);
                applyScrollbarSettings(
                    editor()->preference()->getInt(SETTING::TIMELINE_H_SCROLLBAR_MODE), mode);
            };

            connect(mSettingsDialog->hScrollBottom(), &WindowOptionTile::clicked, this, syncH);
            connect(mSettingsDialog->hScrollTop(),    &WindowOptionTile::clicked, this, syncH);
            connect(mSettingsDialog->hScrollBoth(),   &WindowOptionTile::clicked, this, syncH);
            connect(mSettingsDialog->vScrollLeft(),   &WindowOptionTile::clicked, this, syncV);
            connect(mSettingsDialog->vScrollRight(),  &WindowOptionTile::clicked, this, syncV);
            connect(mSettingsDialog->vScrollBoth(),   &WindowOptionTile::clicked, this, syncV);
        }
    }

    // ── Update values from current state every time dialog is shown ──────
    mSettingsDialog->loopStartField()->setValue(
        mTimeControls->getRangeLower() > 0 ? mTimeControls->getRangeLower() : 1);
    mSettingsDialog->loopEndField()->setValue(
        mTimeControls->getRangeUpper() > 0 ? mTimeControls->getRangeUpper() : 2);

    // Update timecode dropdown from preferences
    {
        int mode = editor()->preference()->getInt(SETTING::TIMECODE_TEXT);
        if (mode < 0 || mode > 3) mode = 3;
        // Block signals to prevent triggering setTimecodeMode during initialization
        QSignalBlocker blocker(mSettingsDialog->timestampDropDown());
        mSettingsDialog->timestampDropDown()->setCurrentIndex(mode);
    }

    // Update scrollbar tiles from preferences
    {
        const int hMode = editor()->preference()->getInt(SETTING::TIMELINE_H_SCROLLBAR_MODE);
        const int vMode = editor()->preference()->getInt(SETTING::TIMELINE_V_SCROLLBAR_MODE);
        mSettingsDialog->hScrollBottom()->setActive(hMode == 0, false);
        mSettingsDialog->hScrollTop()->setActive(hMode == 1, false);
        mSettingsDialog->hScrollBoth()->setActive(hMode == 2, false);
        mSettingsDialog->vScrollLeft()->setActive(vMode == 0, false);
        mSettingsDialog->vScrollRight()->setActive(vMode == 1, false);
        mSettingsDialog->vScrollBoth()->setActive(vMode == 2, false);
    }

    mSettingsDialog->show();
    mSettingsDialog->raise();
    mSettingsDialog->activateWindow();
}

// ── Shortcut-triggered retime ─────────────────────────────────────────────────
// Mirrors the bottom-bar spacing button logic exactly.
// spacing = target frames between keyframes (1 = back-to-back, 2 = 1 blank, 3 = 2 blanks).
// ripple  = also shift all keyframes that follow the last selected one.
void TimeLine::retimeSelectedFrames(int spacing, bool ripple)
{
    if (!editor() || !editor()->object() || !editor()->layers()) return;
    const int layerIdx = editor()->currentLayerIndex();
    if (layerIdx < 0 || layerIdx >= editor()->object()->getLayerCount()) return;
    Layer* layer = editor()->object()->getLayer(layerIdx);
    if (!layer) return;

    // Build sorted selection (fall back to the keyframe under the scrub head).
    QList<int> selected = layer->selectedKeyFramesPositions();
    if (selected.isEmpty()) {
        KeyFrame* kf = layer->getKeyFrameWhichCovers(editor()->currentFrame());
        if (kf) selected << kf->pos();
    }
    if (selected.size() < 2) return;
    std::sort(selected.begin(), selected.end());

    const int first = selected.first();
    const int last  = selected.last();

    // Guard: no unselected keyframe between first and last.
    {
        bool hasGap = false;
        layer->foreachKeyFrame([&](KeyFrame* kf) {
            const int p = kf->pos();
            if (p > first && p < last && !selected.contains(p))
                hasGap = true;
        });
        if (hasGap) return; // silently ignore — user can see the warning via the button
    }

    // Build target positions anchored at first.
    QList<int> targets;
    for (int i = 0; i < selected.size(); ++i)
        targets << first + i * spacing;

    // Guard: targets must not land on unselected, non-trailing keyframes.
    for (int t : targets) {
        if (layer->keyExists(t) && !selected.contains(t)) {
            bool isTrailingConflict = ripple && (t > last);
            if (!isTrailingConflict) return;
        }
    }

    const int rippleDelta = targets.last() - last;

    // Collect trailing frames for ripple.
    QList<int> trailing;
    if (ripple) {
        layer->foreachKeyFrame([&](KeyFrame* kf) {
            if (kf->pos() > last) trailing << kf->pos();
        });
        std::sort(trailing.begin(), trailing.end());
        if (!trailing.isEmpty() && rippleDelta != 0) {
            for (int t : trailing) {
                if (t + rippleDelta < 1) return;
            }
        }
    }

    // Find a clean scratch zone.
    auto findCleanScratch = [&](int count) -> int {
        QList<int> occupied;
        layer->foreachKeyFrame([&](KeyFrame* kf) { occupied << kf->pos(); });
        int candidate = 100000;
        while (true) {
            bool clean = true;
            for (int i = 0; i < count; ++i) {
                if (occupied.contains(candidate + i)) { clean = false; break; }
            }
            if (clean) return candidate;
            candidate += count + 1;
            if (candidate > 10000000) return -1;
        }
    };

    const int scratch = findCleanScratch(selected.size() + trailing.size());
    if (scratch < 0) return;

    const int trailScratchBase = scratch + selected.size();

    // Park selected → scratch.
    for (int i = 0; i < selected.size(); ++i) {
        layer->deselectAll();
        layer->setFrameSelected(selected[i], true);
        layer->moveSelectedFrames((scratch + i) - selected[i]);
    }

    // Park trailing → scratch.
    if (ripple && !trailing.isEmpty()) {
        for (int i = 0; i < trailing.size(); ++i) {
            layer->deselectAll();
            layer->setFrameSelected(trailing[i], true);
            layer->moveSelectedFrames((trailScratchBase + i) - trailing[i]);
        }
    }

    // Place selected at targets (right-to-left).
    for (int i = selected.size() - 1; i >= 0; --i) {
        layer->deselectAll();
        layer->setFrameSelected(scratch + i, true);
        layer->moveSelectedFrames(targets[i] - (scratch + i));
    }

    // Place trailing at final positions.
    if (ripple && rippleDelta != 0 && !trailing.isEmpty()) {
        QList<int> trailTargets;
        for (int t : trailing) trailTargets << t + rippleDelta;

        if (rippleDelta < 0) {
            for (int i = 0; i < trailing.size(); ++i) {
                layer->deselectAll();
                layer->setFrameSelected(trailScratchBase + i, true);
                layer->moveSelectedFrames(trailTargets[i] - (trailScratchBase + i));
            }
        } else {
            for (int i = trailing.size() - 1; i >= 0; --i) {
                layer->deselectAll();
                layer->setFrameSelected(trailScratchBase + i, true);
                layer->moveSelectedFrames(trailTargets[i] - (trailScratchBase + i));
            }
        }
    } else if (ripple && rippleDelta == 0 && !trailing.isEmpty()) {
        for (int i = trailing.size() - 1; i >= 0; --i) {
            layer->deselectAll();
            layer->setFrameSelected(trailScratchBase + i, true);
            layer->moveSelectedFrames(trailing[i] - (trailScratchBase + i));
        }
    }

    // Restore selection at final positions.
    layer->deselectAll();
    for (int t : targets) layer->setFrameSelected(t, true);

    if (editor()->getScribbleArea()) editor()->getScribbleArea()->onViewChanged();
    updateContent();
}

// ── Shortcut-triggered layer navigation ──────────────────────────────────────
// Navigates only through layers that are visible in the timeline (non-camera,
// not filtered out by tag). This matches what the user sees and expects.
void TimeLine::selectLayerAbove()
{
    if (!editor() || !editor()->layers() || !mLayerList) return;
    const int current = editor()->currentLayerIndex();
    const QVector<int> visible = mLayerList->getVisibleLayerIndices();

    if (visible.size() < 2) {
        // Only one (or zero) visible layers — nothing to navigate, but ensure
        // the highlight is still painted correctly in case of a stale repaint.
        updateContent();
        return;
    }

    const int visPos = visible.indexOf(current);
    if (visPos < 0) {
        updateContent(); // current layer not in visible list — repaint and stop
        return;
    }
    if (visPos >= visible.size() - 1) {
        updateContent(); // already at the topmost visible layer
        return;
    }
    editor()->layers()->setCurrentLayer(visible[visPos + 1]);
}

void TimeLine::selectLayerBelow()
{
    if (!editor() || !editor()->layers() || !mLayerList) return;
    const int current = editor()->currentLayerIndex();
    const QVector<int> visible = mLayerList->getVisibleLayerIndices();

    if (visible.size() < 2) {
        // Only one (or zero) visible layers — nothing to navigate, but ensure
        // the highlight is still painted correctly in case of a stale repaint.
        updateContent();
        return;
    }

    const int visPos = visible.indexOf(current);
    if (visPos < 0) {
        updateContent(); // current layer not in visible list — repaint and stop
        return;
    }
    if (visPos <= 0) {
        updateContent(); // already at the bottommost visible layer
        return;
    }
    editor()->layers()->setCurrentLayer(visible[visPos - 1]);
}
