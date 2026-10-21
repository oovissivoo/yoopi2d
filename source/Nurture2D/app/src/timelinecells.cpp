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

#include "timelinecells.h"

/* ===== UI SIZE ADJUSTMENT LANDMARKS ===== */
/*
 * TIMELINE FRAME RULER TEXT POSITIONING:
 * - Lines 1434-1435: Frame ruler label positioning adjusted for 9pt font using font ascent
 * - Lines 2407-2409: Current frame number positioning adjusted for 9pt font using font ascent
 * - Line 1385: Removed duplicate paintTicks call to prevent overlapping frame numbers
 * 
 * These fixes prevent overlapping text when using smaller font sizes.
 * Previously used hardcoded Y position of 15px, now uses dynamic font ascent + 2px padding.
 * Also eliminated double-drawing of frame ruler (was drawn twice per paint cycle).
 * 
 * RELATED SIZE ADJUSTMENTS:
 * - dark_theme.qss: Global font size controls (9pt)
 * - cosmictoolspane.cpp: Cosmic pane header layout and icon button sizes
 * - toolboxwidget.cpp: Tools pane drawing tool icon sizes
 */

#include <QApplication>
#include <QEventLoop>
#include <QResizeEvent>
#include <QInputDialog>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <QLineEdit>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSignalBlocker>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

#include <QSvgRenderer>

#include "camerapropertiesdialog.h"
#include "cosmiccamerapropertiespopup.h"
#include "editor.h"
#include "keyframe.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "timeline.h"
#include "scribblearea.h"

#include "scenesequencemanager.h"

#include "cameracontextmenu.h"
#include "../CUI/cosmicmenuwindow.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "pencilsettings.h"
#include "bitmapkeyframe.h"
#include "layerbitmap.h"
#include "cellbank.h"

#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmichslider.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicbutton.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include "structure/innerlayer.h"
#include "structure/celllayerstack.h"
#include "graphics/bitmap/bitmapimage.h"

static inline bool isCameraLayer(const Layer* layer)
{
    return layer && layer->type() == Layer::CAMERA;
}

void TimeLineCells::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        QWidget::keyPressEvent(event);
        return;
    }

    // Ignore pure modifier presses
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        QWidget::keyPressEvent(event);
        return;
    }

    const QKeySequence deselectSeq(pencilSettings().value(QStringLiteral("shortcuts/") + QStringLiteral(CMD_DESELECT_ALL)).toString());
    if (!deselectSeq.isEmpty()) {
        const QKeySequence pressed(event->modifiers() | event->key());
        if (deselectSeq.matches(pressed) == QKeySequence::ExactMatch) {
            // Use the same deselect logic as the rest of the app (tool-aware)
            if (mEditor) {
                mEditor->deselectAll();

                // Ensure any non-current-layer keyframe selections are also cleared
                Object* obj = mEditor->object();
                if (obj) {
                    for (int i = 0; i < obj->getLayerCount(); ++i) {
                        Layer* layer = obj->getLayer(i);
                        if (layer && layer->hasAnySelectedFrames()) {
                            layer->deselectAll();
                        }
                    }
                }
                emit mEditor->selectedFramesChanged();
                updateContent();
            }
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void TimeLineCells::wheelEvent(QWheelEvent* event)
{
    if (!event) return;

    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    const bool hasHorizontalDelta = pixelDelta.x() != 0 || angleDelta.x() != 0;
    const bool shiftToHorizontal = !hasHorizontalDelta &&
                                   event->modifiers().testFlag(Qt::ShiftModifier) &&
                                   (pixelDelta.y() != 0 || angleDelta.y() != 0);
    if (mTimeLine && (hasHorizontalDelta || shiftToHorizontal) &&
        mTimeLine->scrollTimelineHorizontally(event, shiftToHorizontal)) {
        return;
    }

    QWidget::wheelEvent(event);
}

namespace {

QIcon loadAndTintSvgIcon(const QString& path, const QSize& size, const QColor& color)
{
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
}

class MiniCircleToggle final : public QAbstractButton
{
public:
    explicit MiniCircleToggle(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setCheckable(false);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(8, 8);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
    }

protected:
    void enterEvent(QEvent* event) override
    {
        mHover = true;
        QAbstractButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        mHover = false;
        QAbstractButton::leaveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QVariant tagColorVar = property("tagColor");
        QColor fill = tagColorVar.canConvert<QColor>() ? tagColorVar.value<QColor>() : QColor(0x99, 0x99, 0x99);
        if (mHover) {
            fill = fill.lighter(110);
        }
        if (!isEnabled()) {
            fill.setAlphaF(0.4);
        }

        QPen pen(QColor(0x4d, 0x4d, 0x4d));
        pen.setWidthF(1.0);
        if (!isEnabled()) {
            QColor c = pen.color();
            c.setAlphaF(0.4);
            pen.setColor(c);
        }

        p.setPen(pen);
        p.setBrush(fill);

        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.drawEllipse(r);
    }

private:
    bool mHover = false;
};

struct LayerTagDef
{
    const char* name;
    QColor color;
};

static inline const QVector<LayerTagDef>& layerTagDefs()
{
    static const QVector<LayerTagDef> kDefs = {
        {"Red", QColor(0xf1, 0x46, 0x3f, 0xff)},
        {"Magenta", QColor(0xe6, 0x2a, 0x65, 0xff)},
        {"Purple", QColor(0x99, 0x36, 0xac, 0xff)},
        {"Indigo", QColor(0x63, 0x44, 0xb2, 0xff)},
        {"Blue", QColor(0x3c, 0x57, 0xb0, 0xff)},
        {"Sky", QColor(0x1f, 0x9a, 0xee, 0xff)},
        {"Green", QColor(0x54, 0xac, 0x58, 0xff)},
        {"Leaf", QColor(0x8f, 0xc0, 0x57, 0xff)},
        {"Lime", QColor(0xcf, 0xd9, 0x52, 0xff)},
        {"Yellow", QColor(0xff, 0xbf, 0x3b, 0xff)},
        {"Orange", QColor(0xfd, 0x97, 0x31, 0xff)},
        {"Orange Red", QColor(0xfc, 0x59, 0x34, 0xff)},
    };
    return kDefs;
}

} // namespace

QHash<int, int> TimeLineCells::sLayerTagByLayerId;
QVector<bool> TimeLineCells::sLayerTagFilterEnabled;
bool TimeLineCells::sDefaultLayerFilterEnabled = true;

QVector<bool> TimeLineCells::sLayerTagVisibilityEnabled;
bool TimeLineCells::sDefaultLayerVisibilityEnabled = true;
QHash<int, bool> TimeLineCells::sLayerVisibilityBackupByLayerId;

void TimeLineCells::ensureLayerTagFilterStateSize()
{
    const int n = layerTagDefs().size();
    if (n <= 0) {
        return;
    }

    if (sLayerTagFilterEnabled.size() == n) {
        return;
    }

    QVector<bool> next;
    next.resize(n);
    for (int i = 0; i < n; ++i) {
        next[i] = true;
    }

    const int oldN = sLayerTagFilterEnabled.size();
    const int copyN = qMin(oldN, n);
    for (int i = 0; i < copyN; ++i) {
        next[i] = sLayerTagFilterEnabled[i];
    }
    sLayerTagFilterEnabled = next;
}

void TimeLineCells::ensureLayerTagVisibilityStateSize()
{
    const int n = layerTagDefs().size();
    if (n <= 0) {
        return;
    }

    if (sLayerTagVisibilityEnabled.size() == n) {
        return;
    }

    QVector<bool> next;
    next.resize(n);
    for (int i = 0; i < n; ++i) {
        next[i] = true;
    }

    const int oldN = sLayerTagVisibilityEnabled.size();
    const int copyN = qMin(oldN, n);
    for (int i = 0; i < copyN; ++i) {
        next[i] = sLayerTagVisibilityEnabled[i];
    }
    sLayerTagVisibilityEnabled = next;
}

int TimeLineCells::layerTagDefCount()
{
    return layerTagDefs().size();
}

QString TimeLineCells::layerTagDefName(int index)
{
    const auto& defs = layerTagDefs();
    if (index < 0 || index >= defs.size()) {
        return QString();
    }
    return QString::fromUtf8(defs[index].name);
}

QColor TimeLineCells::layerTagDefColor(int index)
{
    const auto& defs = layerTagDefs();
    if (index < 0 || index >= defs.size()) {
        return QColor();
    }
    return defs[index].color;
}

bool TimeLineCells::isTagFilterEnabled(int tagIndex)
{
    ensureLayerTagFilterStateSize();
    if (tagIndex < 0 || tagIndex >= sLayerTagFilterEnabled.size()) {
        return true;
    }
    return sLayerTagFilterEnabled[tagIndex];
}

void TimeLineCells::setTagFilterEnabled(int tagIndex, bool enabled)
{
    ensureLayerTagFilterStateSize();
    if (tagIndex < 0 || tagIndex >= sLayerTagFilterEnabled.size()) {
        return;
    }
    sLayerTagFilterEnabled[tagIndex] = enabled;
}

bool TimeLineCells::isDefaultLayerFilterEnabled()
{
    return sDefaultLayerFilterEnabled;
}

void TimeLineCells::setDefaultLayerFilterEnabled(bool enabled)
{
    sDefaultLayerFilterEnabled = enabled;
}

void TimeLineCells::resetTagFilters()
{
    ensureLayerTagFilterStateSize();
    for (int i = 0; i < sLayerTagFilterEnabled.size(); ++i) {
        sLayerTagFilterEnabled[i] = true;
    }
    sDefaultLayerFilterEnabled = true;
}

bool TimeLineCells::isTagVisibilityEnabled(int tagIndex)
{
    ensureLayerTagVisibilityStateSize();
    if (tagIndex < 0 || tagIndex >= sLayerTagVisibilityEnabled.size()) {
        return true;
    }
    return sLayerTagVisibilityEnabled[tagIndex];
}

bool TimeLineCells::isDefaultLayerVisibilityEnabled()
{
    return sDefaultLayerVisibilityEnabled;
}

static inline bool layerHasTagIndex(const Layer* layer, int tagIndex)
{
    if (!layer || layer->type() != Layer::BITMAP) {
        return false;
    }
    const int layerId = layer->id();
    const QHash<int, int> map = TimeLineCells::layerTagByLayerId();
    auto it = map.constFind(layerId);
    if (it == map.constEnd()) {
        return false;
    }
    return it.value() == tagIndex;
}

static inline bool layerIsDefaultUntaggedBitmap(const Layer* layer)
{
    if (!layer || layer->type() != Layer::BITMAP) {
        return false;
    }
    const int layerId = layer->id();
    const QHash<int, int> map = TimeLineCells::layerTagByLayerId();
    return !map.contains(layerId);
}

static inline void applyLayerVisible(Editor* editor, int layerIndex, bool targetVisible)
{
    if (!editor || !editor->object()) {
        return;
    }
    Object* object = editor->object();
    if (layerIndex < 0 || layerIndex >= object->getLayerCount()) {
        return;
    }
    Layer* layer = object->getLayer(layerIndex);
    if (!layer) {
        return;
    }
    if (layer->visible() == targetVisible) {
        return;
    }
    editor->switchVisibilityOfLayer(layerIndex);
}

void TimeLineCells::setTagVisibilityEnabled(int tagIndex, bool enabled, Editor* editor)
{
    ensureLayerTagVisibilityStateSize();
    if (tagIndex < 0 || tagIndex >= sLayerTagVisibilityEnabled.size()) {
        return;
    }
    if (sLayerTagVisibilityEnabled[tagIndex] == enabled) {
        return;
    }
    sLayerTagVisibilityEnabled[tagIndex] = enabled;

    if (!editor || !editor->object()) {
        return;
    }

    Object* object = editor->object();
    const QHash<int, int> map = TimeLineCells::layerTagByLayerId();
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        Layer* layer = object->getLayer(i);
        if (!layer || layer->type() != Layer::BITMAP) {
            continue;
        }

        const int layerId = layer->id();
        auto it = map.constFind(layerId);
        if (it == map.constEnd() || it.value() != tagIndex) {
            continue;
        }
        if (!enabled)
        {
            if (!sLayerVisibilityBackupByLayerId.contains(layerId)) {
                sLayerVisibilityBackupByLayerId.insert(layerId, layer->visible());
            }
            applyLayerVisible(editor, i, false);
        }
        else
        {
            const bool restore = sLayerVisibilityBackupByLayerId.contains(layerId) ? sLayerVisibilityBackupByLayerId.value(layerId)
                                                                                   : true;
            applyLayerVisible(editor, i, restore);
            sLayerVisibilityBackupByLayerId.remove(layerId);
        }
    }
}

void TimeLineCells::setDefaultLayerVisibilityEnabled(bool enabled, Editor* editor)
{
    if (sDefaultLayerVisibilityEnabled == enabled) {
        return;
    }
    sDefaultLayerVisibilityEnabled = enabled;

    if (!editor || !editor->object()) {
        return;
    }

    Object* object = editor->object();
    const QHash<int, int> map = TimeLineCells::layerTagByLayerId();
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        Layer* layer = object->getLayer(i);
        if (!layer || layer->type() != Layer::BITMAP) {
            continue;
        }

        const int layerId = layer->id();
        if (map.contains(layerId)) {
            continue;
        }
        if (!enabled)
        {
            if (!sLayerVisibilityBackupByLayerId.contains(layerId)) {
                sLayerVisibilityBackupByLayerId.insert(layerId, layer->visible());
            }
            applyLayerVisible(editor, i, false);
        }
        else
        {
            const bool restore = sLayerVisibilityBackupByLayerId.contains(layerId) ? sLayerVisibilityBackupByLayerId.value(layerId)
                                                                                   : true;
            applyLayerVisible(editor, i, restore);
            sLayerVisibilityBackupByLayerId.remove(layerId);
        }
    }
}

void TimeLineCells::resetTagVisibility(Editor* editor)
{
    ensureLayerTagVisibilityStateSize();
    for (int i = 0; i < sLayerTagVisibilityEnabled.size(); ++i) {
        sLayerTagVisibilityEnabled[i] = true;
    }
    sDefaultLayerVisibilityEnabled = true;

    if (!editor || !editor->object()) {
        sLayerVisibilityBackupByLayerId.clear();
        return;
    }

    Object* object = editor->object();
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        Layer* layer = object->getLayer(i);
        if (!layer || layer->type() != Layer::BITMAP) {
            continue;
        }
        const int layerId = layer->id();
        if (!sLayerVisibilityBackupByLayerId.contains(layerId)) {
            continue;
        }
        applyLayerVisible(editor, i, sLayerVisibilityBackupByLayerId.value(layerId));
    }
    sLayerVisibilityBackupByLayerId.clear();
}

QHash<int, int> TimeLineCells::layerTagByLayerId()
{
    return sLayerTagByLayerId;
}

void TimeLineCells::setLayerTagByLayerId(const QHash<int, int>& map)
{
    sLayerTagByLayerId = map;
}

void TimeLineCells::clearLayerTags()
{
    sLayerTagByLayerId.clear();
}

QVector<int> TimeLineCells::getVisibleLayerIndices() const
{
    QVector<int> indices;
    const Object* object = mEditor->object();
    if (!object) return indices;
    indices.reserve(object->getLayerCount());
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        const Layer* layer = object->getLayer(i);
        if (!isCameraLayer(layer))
        {
            if (layer && layer->type() == Layer::BITMAP)
            {
                const int layerId = layer->id();
                auto it = sLayerTagByLayerId.constFind(layerId);
                if (it != sLayerTagByLayerId.constEnd())
                {
                    const int tagIndex = it.value();
                    if (!isTagFilterEnabled(tagIndex)) {
                        continue;
                    }
                }
                else
                {
                    if (!isDefaultLayerFilterEnabled()) {
                        continue;
                    }
                }
            }
            indices.push_back(i);
        }
    }
    return indices;
}

int TimeLineCells::getVisibleLayerCount() const
{
    return getVisibleLayerIndices().size();
}

int TimeLineCells::toVisibleIndex(int actualIndex) const
{
    // Layers are rendered bottom-up (0 is bottom), timeline inverts visually.
    // We map actual object indices to a compact sequence excluding camera layers,
    // preserving the relative order of non-camera layers.
    const QVector<int> visible = getVisibleLayerIndices();
    for (int v = 0; v < visible.size(); ++v)
    {
        if (visible[v] == actualIndex) return v;
    }
    return -1; // not visible (camera)
}

bool TimeLineCells::isLayerVisibleInTimeline(int actualIndex) const
{
    if (!mEditor || !mEditor->object()) {
        return false;
    }
    const Object* object = mEditor->object();
    if (actualIndex < 0 || actualIndex >= object->getLayerCount()) {
        return false;
    }

    const Layer* layer = object->getLayer(actualIndex);
    if (isCameraLayer(layer)) {
        return false;
    }

    if (layer && layer->type() == Layer::BITMAP)
    {
        const int layerId = layer->id();
        auto it = sLayerTagByLayerId.constFind(layerId);
        if (it != sLayerTagByLayerId.constEnd())
        {
            const int tagIndex = it.value();
            if (!isTagFilterEnabled(tagIndex)) {
                return false;
            }
        }
        else
        {
            if (!isDefaultLayerFilterEnabled()) {
                return false;
            }
        }
    }

    return true;
}

int TimeLineCells::findNearestVisibleLayerIndex(int preferredActualIndex) const
{
    if (!mEditor || !mEditor->object()) {
        return -1;
    }
    const Object* object = mEditor->object();
    const int n = object->getLayerCount();
    if (n <= 0) {
        return -1;
    }

    if (preferredActualIndex >= 0 && preferredActualIndex < n && isLayerVisibleInTimeline(preferredActualIndex)) {
        return preferredActualIndex;
    }

    for (int d = 1; d < n; ++d)
    {
        const int down = preferredActualIndex - d;
        if (down >= 0 && isLayerVisibleInTimeline(down)) {
            return down;
        }
        const int up = preferredActualIndex + d;
        if (up < n && isLayerVisibleInTimeline(up)) {
            return up;
        }
    }

    for (int i = 0; i < n; ++i) {
        if (isLayerVisibleInTimeline(i)) {
            return i;
        }
    }
    return -1;
}

TimeLineCells::TimeLineCells(TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE type) : QWidget(parent)
{
    mTimeLine = parent;
    mEditor = editor;
    mPrefs = editor->preference();
    mType = type;

    mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
    mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
    mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
    mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
    mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);

    setMinimumSize(500, 4 * mLayerHeight);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);

    connect(mPrefs, &PreferenceManager::optionChanged, this, &TimeLineCells::loadSetting);

    ensureLayerRowControls();

    mSelectionAnimClock.start();
    mSelectionAnimUpdateTimer = new QTimer(this);
    mSelectionAnimUpdateTimer->setInterval(33);
    connect(mSelectionAnimUpdateTimer, &QTimer::timeout, this, [this]() {
        if (!hasSelectedFramesForAnimation()) {
            mSelectionAnimUpdateTimer->stop();
            return;
        }

        QRect dirty = rect();
        if (mType == TIMELINE_CELL_TYPE::Tracks) {
            const int x = mOffsetX;
            const int y = mOffsetY;
            const int w = qMax(0, width() - x);
            const int h = qMax(0, height() - y);
            dirty = QRect(x, y, w, h);
        }
        if (!dirty.isNull()) {
            update(dirty);
        }
    });
}

TimeLineCells::~TimeLineCells()
{
    if (mSelectionAnimUpdateTimer) {
        mSelectionAnimUpdateTimer->stop();
    }
    delete mCache;
}

void TimeLineCells::loadSetting(SETTING setting)
{
    switch (setting)
    {
    case SETTING::TIMELINE_SIZE:
        mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
        mTimeLine->updateLength();
        break;
    case SETTING::LABEL_FONT_SIZE:
        mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
        break;
    case SETTING::FRAME_SIZE:
        mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
        mTimeLine->updateLength();
        break;
    case SETTING::SHORT_SCRUB:
        mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
        break;
    case SETTING::DRAW_LABEL:
        mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);
        break;
    default:
        break;
    }

    updateContent();
}

int TimeLineCells::getFrameNumber(int x) const
{
    // One negative visual frame (frame 0) is used as left padding so range markers at frame 1
    // are not clipped. This is visual-only padding; callers should treat frameNumber < 1 as
    // non-interactive space.
    const int negativePaddingFrames = 1;
    return mFrameOffset + 1 + (x - mOffsetX) / mFrameSize - negativePaddingFrames;
}

int TimeLineCells::getFrameX(int frameNumber) const
{
    const int negativePaddingFrames = 1;
    return mOffsetX + (frameNumber - mFrameOffset + negativePaddingFrames) * mFrameSize;
}

void TimeLineCells::setFrameSize(int size)
{
    mFrameSize = size;
    mPrefs->set(SETTING::FRAME_SIZE, mFrameSize);
    updateContent();
}

int TimeLineCells::getLayerNumber(int y) const
{
    // Compute the visible row under y, then map to the actual layer index.
    const QVector<int> visible = getVisibleLayerIndices();
    const int totalVisibleCount = visible.size();

    int visibleRow = mLayerOffset + (y - mOffsetY) / mLayerHeight;

    if (y < mOffsetY)
    {
        visibleRow = -1;
    }

    if (visibleRow < -1)
    {
        visibleRow = -1;
    }

    if (visibleRow >= totalVisibleCount)
    {
        visibleRow = totalVisibleCount;
    }

    if (visibleRow == -1)
    {
        return -1;
    }

    // Convert visible row to actual index accounting for inverted display order
    int actualIndex = -1;
    if (visibleRow < totalVisibleCount)
    {
        int invertedRow = (totalVisibleCount - 1) - visibleRow;
        actualIndex = visible[invertedRow];
    }

    return actualIndex;
}

int TimeLineCells::getLayerInsertSlot(int y) const
{
    // Returns an insertion slot index from the visual top (0..N).
    // Slot 0 means above the topmost visible row; slot N means below the bottommost.
    const QVector<int> visible = getVisibleLayerIndices();
    const int totalVisibleCount = visible.size();
    if (totalVisibleCount <= 0) {
        return 0;
    }

    if (y < mOffsetY) {
        return 0;
    }

    // If dragged beyond the widget, clamp to end.
    if (y >= mOffsetY + totalVisibleCount * mLayerHeight) {
        return totalVisibleCount;
    }

    const int localY = y - mOffsetY;
    // Use mid-point rounding so the slot switches when crossing a row's midpoint.
    int slot = (localY + (mLayerHeight / 2)) / mLayerHeight;
    if (slot < 0) slot = 0;
    if (slot > totalVisibleCount) slot = totalVisibleCount;
    return slot;
}

int TimeLineCells::getInbetweenLayerNumber(int y) const {
    int actualIndex = getLayerNumber(y);
    if (actualIndex == -1) return -1;

    // Map actual index to its visible row to do rounding against midpoints,
    // then remap back to actual index
    const QVector<int> visible = getVisibleLayerIndices();
    int totalVisible = visible.size();
    int visibleRow = -1;
    for (int v = 0; v < totalVisible; ++v) {
        if (visible[v] == actualIndex) { visibleRow = v; break; }
    }
    if (visibleRow == -1) return -1;

    int layerNumber = actualIndex;
    // Reuse previous logic by comparing with getLayerY using actual indices.
    if(layerNumber != mFromLayer) {
        if(mMouseMoveY > 0 && y < getLayerY(layerNumber) + mLayerHeight / 2) {
            // Move down visually -> next lower visible row means higher actual index above
            // Find the actual index corresponding to the next visible row upward
            int inverted = (totalVisible - 1) - visibleRow;
            int newInverted = inverted + 1;
            if (newInverted < totalVisible) {
                int newVisibleRow = (totalVisible - 1) - newInverted;
                layerNumber = visible[newVisibleRow];
            }
        }
        else if(mMouseMoveY < 0 && y > getLayerY(layerNumber) + mLayerHeight / 2) {
            int inverted = (totalVisible - 1) - visibleRow;
            int newInverted = inverted - 1;
            if (newInverted >= 0) {
                int newVisibleRow = (totalVisible - 1) - newInverted;
                layerNumber = visible[newVisibleRow];
            }
        }
    }
    return layerNumber;
}

int TimeLineCells::getLayerY(int layerNumber) const
{
    // Compute y based on the visible (non-camera) list
    const QVector<int> visible = getVisibleLayerIndices();
    const int totalVisibleCount = visible.size();

    // Find visible row of actual index
    int visibleRow = -1;
    for (int v = 0; v < totalVisibleCount; ++v) {
        if (visible[v] == layerNumber) { visibleRow = v; break; }
    }

    if (visibleRow == -1) {
        // Not visible (camera). Return a position off-screen to avoid drawing/hit-testing
        return -10000;
    }

    // Invert for display (0 at top)
    int displayRow = (totalVisibleCount - 1) - visibleRow - mLayerOffset;
    return mOffsetY + displayRow * mLayerHeight;
}

void TimeLineCells::updateFrame(int frameNumber)
{
    int x = getFrameX(frameNumber);
    update(x - mFrameSize, 0, mFrameSize + 1, height());
}

void TimeLineCells::updateContent()
{
    mRedrawContent = true;

    if (mContentUpdateQueued) {
        return;
    }

    mContentUpdateQueued = true;
    QTimer::singleShot(0, this, [this]() {
        mContentUpdateQueued = false;

        updateLayerRowControls();
        updateInlineRenameGeometry();

        if (mSelectionAnimUpdateTimer) {
            if (hasSelectedFramesForAnimation()) {
                if (!mSelectionAnimUpdateTimer->isActive()) {
                    mSelectionAnimUpdateTimer->start();
                }
            } else {
                if (mSelectionAnimUpdateTimer->isActive()) {
                    mSelectionAnimUpdateTimer->stop();
                }
            }
        }

        update();
    });
}

QRect TimeLineCells::inlineRenameRectForLayer(int layerIndex) const
{
    const int y = getLayerY(layerIndex);
    if (y < -5000) {
        return QRect();
    }

    // Must match paintLabel layout: strip(4) + visibility(22) + gap(4)
    const int kStripW      = 4;
    const int kVisibilityW = 22;
    const int kGap         = 4;
    const int kRightIconW  = 22;
    const int kRightPad    = 4;

    const int x = kStripW + kVisibilityW + kGap;
    const int w = qMax(1, width() - x - kRightIconW - kRightPad);

    const QFontMetrics fm(font());
    const int textH = fm.height();
    const int editH = textH + 4;
    const int rowH  = qMax(1, mLayerHeight);
    const int editY = y + (rowH - editH) / 2;

    return QRect(x, editY, w, editH);
}

void TimeLineCells::updateInlineRenameGeometry()
{
    if (!mIsInlineEditing || !mInlineEditor) {
        return;
    }
    if (mEditingLayerIndex < 0) {
        return;
    }

    const QRect r = inlineRenameRectForLayer(mEditingLayerIndex);
    if (!r.isValid() || r.isNull()) {
        return;
    }
    mInlineEditor->setGeometry(r);
}

void TimeLineCells::ensureLayerRowControls()
{
    if (mType != TIMELINE_CELL_TYPE::Layers) {
        return;
    }

    const QVector<int> visible = getVisibleLayerIndices();
    const int desired = visible.size();
    if (desired < 0) {
        return;
    }

    while (mLayerRowControls.size() > desired)
    {
        LayerRowControls row = mLayerRowControls.takeLast();
        if (row.visibilityToggle) {
            row.visibilityToggle->deleteLater();
        }
        if (row.stubToggle1) {
            row.stubToggle1->deleteLater();
        }
    }

    while (mLayerRowControls.size() < desired)
    {
        LayerRowControls row;
        row.visibilityToggle = new CosmicCheckboxToggle(this);
        row.visibilityToggle->setCheckable(false);
        row.visibilityToggle->setMode(CosmicCheckboxToggle::Custom);
        row.visibilityToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
        row.visibilityToggle->setBordered(false);
        row.visibilityToggle->setShowText(false);
        row.visibilityToggle->setShowIcon(true);
        row.visibilityToggle->setBackgroundEnabled(true);
        row.visibilityToggle->setHoverHighlightEnabled(true);
        row.visibilityToggle->setIconSize(QSize(16, 16));
        row.visibilityToggle->setFixedSize(22, 22);

        CosmicCheckboxToggle* vis = row.visibilityToggle;
        QObject::connect(vis, &QAbstractButton::clicked, this, [this, vis](bool) {
            if (!vis || !mEditor) {
                return;
            }
            const int layerIndex = vis->property("layerIndex").toInt();
            if (layerIndex < 0 || !mEditor->object() || layerIndex >= mEditor->object()->getLayerCount()) {
                return;
            }
            mEditor->switchVisibilityOfLayer(layerIndex);
            updateContent();
        });

        CosmicButton* tagButton = new CosmicButton(this);
        tagButton->setBordered(false);
        tagButton->setText("");
        tagButton->setIconSize(QSize(16, 16));
        tagButton->setFixedSize(22, 22);
        row.stubToggle1 = tagButton;

        QAbstractButton* stub = row.stubToggle1;
        QObject::connect(stub, &QAbstractButton::clicked, this, [this, stub](bool) {
            if (!stub || !mEditor) {
                return;
            }
            const int layerIndex = stub->property("layerIndex").toInt();
            if (layerIndex < 0 || !mEditor->object() || layerIndex >= mEditor->object()->getLayerCount()) {
                return;
            }
            showLayerTagPopup(layerIndex, stub->mapToGlobal(QPoint(0, stub->height())));
        });

        mLayerRowControls.push_back(row);
    }
}

void TimeLineCells::updateLayerRowControls()
{
    if (mType != TIMELINE_CELL_TYPE::Layers) {
        return;
    }

    ensureLayerRowControls();

    const Object* object = mEditor ? mEditor->object() : nullptr;
    const QVector<int> visible = getVisibleLayerIndices();
    const int count = qMin(visible.size(), mLayerRowControls.size());
    const int currentLayerIndex = (mEditor && mEditor->layers()) ? mEditor->layers()->currentLayerIndex() : -1;

    for (int i = 0; i < count; ++i)
    {
        const int layerIndex = visible[i];
        Layer* layer = (object && layerIndex >= 0 && layerIndex < object->getLayerCount()) ? object->getLayer(layerIndex) : nullptr;
        LayerRowControls& row = mLayerRowControls[i];
        row.layerIndex = layerIndex;

        const int y = getLayerY(layerIndex);
        if (!layer || y < -1000 || y > height())
        {
            if (row.visibilityToggle) row.visibilityToggle->hide();
            if (row.stubToggle1) row.stubToggle1->hide();
            continue;
        }

        const int visW = row.visibilityToggle ? row.visibilityToggle->width() : 0;
        const int visH = row.visibilityToggle ? row.visibilityToggle->height() : 0;
        const int visX = 4; // offset by left strip width
        const int visY = y + (mLayerHeight - visH) / 2 + 1;

        const int stubW = row.stubToggle1 ? row.stubToggle1->width() : 0;
        const int stubH = row.stubToggle1 ? row.stubToggle1->height() : 0;

        const int kRightPad = 4;
        const int stub1X = width() - stubW - kRightPad; // anchored to far right
        const int stubY = y + (mLayerHeight - stubH) / 2;

        if (row.visibilityToggle)
        {
            row.visibilityToggle->setProperty("layerIndex", layerIndex);
            const bool isVisible = layer->visible();
            const QString iconPath = isVisible ? ":/Interaction/Show.svg" : ":/Interaction/Hide.svg";
            if (layerIndex == currentLayerIndex)
            {
                const QColor fixedTint(0xcc, 0xcc, 0xcc, 0xff);
                row.visibilityToggle->setIcon(loadAndTintSvgIcon(iconPath, QSize(16, 16), fixedTint));
            }
            else
            {
                row.visibilityToggle->setIcon(QIcon(iconPath));
            }
            row.visibilityToggle->setToolTip(isVisible ? tr("Hide Layer") : tr("Show Layer"));
            row.visibilityToggle->setGeometry(visX, visY, visW, visH);
            row.visibilityToggle->show();
            row.visibilityToggle->raise();
        }

        if (row.stubToggle1)
        {
            row.stubToggle1->setProperty("layerIndex", layerIndex);
            if (layer->type() == Layer::BITMAP)
            {
                const int layerId = layer->id();
                const int tagIndex = sLayerTagByLayerId.contains(layerId) ? sLayerTagByLayerId.value(layerId) : -1;
                const bool isTagged = (tagIndex >= 0);

                // Always tint the icon with the tag color if tagged, otherwise neutral grey
                QColor iconTint(0x88, 0x88, 0x88, 0xff);
                if (isTagged) {
                    const auto& defs = layerTagDefs();
                    if (tagIndex < defs.size()) {
                        iconTint = defs[tagIndex].color;
                        if (!layer->visible()) iconTint.setAlpha(140);
                    }
                }
                row.stubToggle1->setIcon(loadAndTintSvgIcon(":/Interaction/Tag.svg", QSize(16, 16), iconTint));
                row.stubToggle1->setToolTip(isTagged ? tr("Change Tag") : tr("Set Tag"));
                row.stubToggle1->setGeometry(stub1X, stubY, stubW, stubH);
                row.stubToggle1->show();
                row.stubToggle1->raise();
            }
            else
            {
                row.stubToggle1->hide();
            }
        }
    }

    for (int i = count; i < mLayerRowControls.size(); ++i)
    {
        LayerRowControls& row = mLayerRowControls[i];
        if (row.visibilityToggle) row.visibilityToggle->hide();
        if (row.stubToggle1) row.stubToggle1->hide();
        row.layerIndex = -1;
    }
}

void TimeLineCells::showLayerTagPopup(int layerIndex, const QPoint& globalAnchorPos)
{
    if (!mEditor || !mEditor->object()) {
        return;
    }

    Object* object = mEditor->object();
    if (layerIndex < 0 || layerIndex >= object->getLayerCount()) {
        return;
    }

    Layer* layer = object->getLayer(layerIndex);
    if (!layer || layer->type() != Layer::BITMAP) {
        return;
    }

    QFrame* popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose, true);
    popup->setObjectName("LayerTagPopup");

    popup->setFixedSize(460, 286);

    QVBoxLayout* outer = new QVBoxLayout(popup);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QWidget* body = new QWidget(popup);
    QHBoxLayout* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    QWidget* leftCol = new QWidget(body);
    QWidget* rightCol = new QWidget(body);
    QVBoxLayout* leftLay = new QVBoxLayout(leftCol);
    QVBoxLayout* rightLay = new QVBoxLayout(rightCol);
    leftLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(0);
    rightLay->setSpacing(0);

    bodyLay->addWidget(leftCol, 1);
    bodyLay->addWidget(rightCol, 1);

    const auto& defs = layerTagDefs();
    const int layerId = layer->id();

    const QColor stripText(0x1f, 0x1f, 0x1f, 0xff);
    const QIcon stripIcon = loadAndTintSvgIcon(":/Interaction/Tag.svg", QSize(18, 18), stripText);
    const int stripH = 40;

    auto makeStrip = [&](int tagIndex) -> CosmicButton* {
        CosmicButton* b = new CosmicButton(body);
        b->setBordered(false);
        b->setFixedHeight(stripH);
        b->setIcon(stripIcon);
        b->setIconSize(QSize(18, 18));
        b->setText(tr(defs[tagIndex].name));
        b->setStyleSheet("QPushButton { border: none; padding-left: 14px; padding-right: 10px; background-color: transparent; text-align: left; }");

        QPalette pal = b->palette();
        pal.setColor(QPalette::ButtonText, stripText);
        b->setPalette(pal);

        const QColor bg = defs[tagIndex].color;
        b->setBackgroundColors(bg, bg.darker(104), bg.darker(112));

        QObject::connect(b, &QPushButton::clicked, popup, [this, popup, layerId, tagIndex]() {
            sLayerTagByLayerId.insert(layerId, tagIndex);
            popup->close();
            updateContent();
            emit mEditor->updateTimeLine();

            if (mTimeLine && mTimeLine->sceneSequenceManager()) {
                mTimeLine->sceneSequenceManager()->captureTagStateFromUIForActiveShot();
            }
            if (mEditor && mEditor->object()) {
                mEditor->object()->setModified(true);
            }
        });
        return b;
    };

    for (int i = 0; i < 6 && i < defs.size(); ++i) {
        leftLay->addWidget(makeStrip(i));
    }
    for (int i = 6; i < 12 && i < defs.size(); ++i) {
        rightLay->addWidget(makeStrip(i));
    }

    QWidget* bottom = new QWidget(popup);
    bottom->setFixedHeight(46);
    QVBoxLayout* bottomLay = new QVBoxLayout(bottom);
    bottomLay->setContentsMargins(0, 0, 0, 0);
    bottomLay->setSpacing(0);

    CosmicButton* removeBtn = new CosmicButton(bottom);
    removeBtn->setBordered(false);
    removeBtn->setFixedHeight(46);
    removeBtn->setText(tr("Remove Tag"));
    const QColor removeRed(0xff, 0x00, 0x00, 0xff);
    removeBtn->setIcon(loadAndTintSvgIcon(":/Interaction/Tag.svg", QSize(18, 18), removeRed));
    removeBtn->setIconSize(QSize(18, 18));
    {
        QPalette pal = removeBtn->palette();
        pal.setColor(QPalette::ButtonText, removeRed);
        removeBtn->setPalette(pal);
    }
    removeBtn->setBackgroundColors(QColor(0x7a, 0x7a, 0x7a, 0xff), QColor(0x82, 0x82, 0x82, 0xff), QColor(0x6f, 0x6f, 0x6f, 0xff));

    QObject::connect(removeBtn, &QPushButton::clicked, popup, [this, popup, layerId]() {
        sLayerTagByLayerId.remove(layerId);
        popup->close();
        updateContent();
        emit mEditor->updateTimeLine();

        if (mTimeLine && mTimeLine->sceneSequenceManager()) {
            mTimeLine->sceneSequenceManager()->captureTagStateFromUIForActiveShot();
        }
        if (mEditor && mEditor->object()) {
            mEditor->object()->setModified(true);
        }
    });
    bottomLay->addWidget(removeBtn);

    outer->addWidget(body, 1);
    outer->addWidget(bottom, 0);

    popup->setStyleSheet(
        "#LayerTagPopup{background-color:rgb(28,28,28);border:1px solid rgba(255,255,255,25);border-radius:0px;}"
    );

    QPoint pos = globalAnchorPos;
    QScreen* screen = QGuiApplication::screenAt(globalAnchorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // Prefer showing below the anchor. If it would go off-screen, flip above.
    if (pos.y() + popup->height() > avail.y() + avail.height()) {
        pos.setY(globalAnchorPos.y() - popup->height());
    }

    // Clamp horizontally
    const int maxX = (avail.x() + avail.width()) - popup->width();
    if (pos.x() > maxX) pos.setX(maxX);
    if (pos.x() < avail.x()) pos.setX(avail.x());

    // Clamp vertically
    const int maxY = (avail.y() + avail.height()) - popup->height();
    if (pos.y() > maxY) pos.setY(maxY);
    if (pos.y() < avail.y()) pos.setY(avail.y());

    popup->move(pos);
    popup->show();
}

bool TimeLineCells::didDetachLayer() const {
    return abs(mMouseMoveY) > mLayerDetachThreshold;
}

void TimeLineCells::showCameraMenu(QPoint pos)
{
    int frameNumber = getFrameNumber(pos.x());

    const Layer* curLayer = mEditor->layers()->currentLayer();
    Q_ASSERT(curLayer);

    // only show menu if on camera layer and key exists
    if (curLayer->type() != Layer::CAMERA || !curLayer->keyExists(frameNumber))
    {
        return;
    }

    mHighlightFrameEnabled = true;
    mHighlightedFrame = frameNumber;

    CameraContextMenu menu(frameNumber, static_cast<const LayerCamera*>(curLayer));

    menu.connect(&menu, &CameraContextMenu::aboutToHide, this, [=] {
        mHighlightFrameEnabled = false;
        mHighlightedFrame = -1;
        update();

        KeyFrame* key = curLayer->getKeyFrameAt(frameNumber);
        // Safeguard: key might have been deleted through the context menu action
        if (key && key->isModified()) {
            emit mEditor->frameModified(frameNumber);
        }
    });

    // Update needs to happen before executing menu, otherwise paint event might be postponed
    update();

    menu.exec(mapToGlobal(pos));
}

void TimeLineCells::showLayerMenu(QPoint pos)
{
    // Right-click on the layer labels area should open a context menu to copy timing
    const int clickedLayer = getLayerNumber(pos.y());
    if (clickedLayer < 0 || clickedLayer >= mEditor->object()->getLayerCount()) {
        return;
    }

    Layer* currentLayer = mEditor->object()->getLayer(clickedLayer);
    if (!currentLayer || currentLayer->type() == Layer::CAMERA) {
        return;
    }

    // Determine adjacent non-camera layers (above = smaller index visually below)
    Object* object = mEditor->object();
    const int total = object->getLayerCount();

    auto findUpper = [&]() -> Layer* {
        for (int i = clickedLayer + 1; i < total; ++i) {
            Layer* l = object->getLayer(i);
            if (l && l->type() != Layer::CAMERA) return l;
        }
        return nullptr;
    };
    auto findLower = [&]() -> Layer* {
        for (int i = clickedLayer - 1; i >= 0; --i) {
            Layer* l = object->getLayer(i);
            if (l && l->type() != Layer::CAMERA) return l;
        }
        return nullptr;
    };

    Layer* upperLayer = findUpper();
    Layer* lowerLayer = findLower();

    if (!upperLayer && !lowerLayer) {
        return; // nothing to do
    }

    // Build actions
    QAction actUpper(tr("Copy upper timing"));
    QAction actLower(tr("Copy bottom timing"));

    CosmicMenuWindow menu(this);
    if (upperLayer) menu.addAction(&actUpper);
    if (lowerLayer) menu.addAction(&actLower);

    // Track which was triggered
    int chosen = 0; // 0=none, 1=upper, 2=lower
    if (upperLayer) QObject::connect(&actUpper, &QAction::triggered, [&]{ chosen = 1; });
    if (lowerLayer) QObject::connect(&actLower, &QAction::triggered, [&]{ chosen = 2; });

    // Install app-level filter to close on outside click (CosmicMenuWindow is Qt::Tool, not Qt::Popup)
    struct Dismisser : public QObject {
        CosmicMenuWindow* w;
        bool eventFilter(QObject*, QEvent* e) override {
            if (e->type() == QEvent::MouseButtonPress) { w->closeAll(); }
            return false;
        }
    } dismisser;
    dismisser.w = &menu;
    qApp->installEventFilter(&dismisser);

    menu.popup(mapToGlobal(pos));

    // Spin a local event loop until the menu closes
    QEventLoop loop;
    QObject::connect(&actUpper, &QAction::triggered, &loop, &QEventLoop::quit);
    QObject::connect(&actLower, &QAction::triggered, &loop, &QEventLoop::quit);
    // Also quit when menu hides
    QObject::connect(&menu, &QWidget::destroyed, &loop, &QEventLoop::quit);
    // Use a hide-watcher via event filter on the menu
    struct HideWatcher : public QObject {
        QEventLoop* loop;
        bool eventFilter(QObject*, QEvent* e) override {
            if (e->type() == QEvent::Hide) { loop->quit(); return false; }
            return false;
        }
    } hideWatcher;
    hideWatcher.loop = &loop;
    menu.installEventFilter(&hideWatcher);

    loop.exec();

    qApp->removeEventFilter(&dismisser);

    auto copyTimingFrom = [&](Layer* sourceLayer) {
        if (!sourceLayer) return;

        if (sourceLayer->type() == Layer::BITMAP && currentLayer->type() == Layer::BITMAP) {
            LayerBitmap* srcBitmap = static_cast<LayerBitmap*>(sourceLayer);
            LayerBitmap* dstBitmap = static_cast<LayerBitmap*>(currentLayer);
            if (!srcBitmap || !dstBitmap || !dstBitmap->cellBank()) {
                return;
            }

            QMap<int, int> srcToDstCell;
            QList<KeyFrame*> srcKeys;
            srcBitmap->foreachKeyFrame([&](KeyFrame* key) {
                if (key) {
                    srcKeys.append(key);
                }
            });
            std::sort(srcKeys.begin(), srcKeys.end(), [](KeyFrame* a, KeyFrame* b) {
                return a->pos() < b->pos();
            });

            for (KeyFrame* key : srcKeys) {
                BitmapKeyFrame* srcBkf = dynamic_cast<BitmapKeyFrame*>(key);
                if (!srcBkf) {
                    continue;
                }
                const int framePos = key->pos();
                const int srcCellId = srcBkf->cellId();
                if (srcCellId <= 0) {
                    continue;
                }

                int dstCellId = -1;
                if (!srcToDstCell.contains(srcCellId)) {
                    if (dstBitmap->keyExists(framePos)) {
                        KeyFrame* existingDst = dstBitmap->getKeyFrameAt(framePos);
                        BitmapKeyFrame* existingBkf = dynamic_cast<BitmapKeyFrame*>(existingDst);
                        if (existingBkf) {
                            dstCellId = existingBkf->cellId();
                        }
                    } else {
                        if (dstBitmap->addNewKeyFrameAt(framePos)) {
                            KeyFrame* created = dstBitmap->getKeyFrameAt(framePos);
                            BitmapKeyFrame* createdBkf = dynamic_cast<BitmapKeyFrame*>(created);
                            if (createdBkf) {
                                dstCellId = createdBkf->cellId();
                            }
                        }
                    }
                    if (dstCellId <= 0) {
                        continue;
                    }
                    srcToDstCell.insert(srcCellId, dstCellId);
                } else {
                    dstCellId = srcToDstCell.value(srcCellId);
                }

                if (dstBitmap->keyExists(framePos)) {
                    continue;
                }

                BitmapKeyFrame* newKf = new BitmapKeyFrame(dstCellId);
                newKf->setPos(framePos);
                newKf->setCellBank(dstBitmap->cellBank());
                dstBitmap->addKeyFrame(framePos, newKf);
            }
        } else {
            sourceLayer->foreachKeyFrame([&](KeyFrame* key) {
                const int framePos = key->pos();
                if (currentLayer->keyExists(framePos)) {
                    return;
                }
                currentLayer->addNewKeyFrameAt(framePos);
            });
        }

        mEditor->layers()->notifyAnimationLengthChanged();
        emit mEditor->framesModified();
        emit mEditor->updateTimeLine();
    };

    if (chosen == 1) {
        copyTimingFrom(upperLayer);
    } else if (chosen == 2) {
        copyTimingFrom(lowerLayer);
    }
}

void TimeLineCells::drawContent()
{
    if (mCache == nullptr || mCache->size() != size())
    {
        delete mCache;
        mCache = new QPixmap(size());
        if (mCache->isNull())
        {
            // fail to create cache
            mCache = nullptr;
            return;
        }
    }

    QPainter painter(mCache);

    const QPalette palette = this->palette();
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Window));
    painter.drawRect(QRect(0, 0, width(), height()));

    // Get actual visible width - use parent's width if available (accounts for splitter clipping)
    // Otherwise fall back to our own width
    int widgetWidth = width();
    if (parentWidget() && parentWidget()->width() < widgetWidth) {
        widgetWidth = parentWidget()->width();
    }
    const int actualWidth = widgetWidth;

    // Draw non-current layers (exclude camera layers)
    const Object* object = mEditor->object();
    Q_ASSERT(object != nullptr);

    const QVector<int> visible = getVisibleLayerIndices();

    for (int idx = 0; idx < visible.size(); ++idx)
    {
        int i = visible[idx];
        if (i >= object->getLayerCount()) { continue; }
        if (i == mEditor->layers()->currentLayerIndex())
        {
            continue;
        }
        const Layer* layeri = object->getLayer(i);

        if (layeri != nullptr)
        {
            const int layerY = getLayerY(i);
            switch (mType)
            {
            case TIMELINE_CELL_TYPE::Tracks:
                paintTrack(painter, layeri, mOffsetX,
                           layerY, actualWidth - mOffsetX,
                           mLayerHeight, false, mFrameSize);
                break;

            case TIMELINE_CELL_TYPE::Layers:
                paintLabel(painter, layeri, 0,
                           layerY, actualWidth - 1,
                           mLayerHeight, false, mEditor->layerVisibility());
                break;
            }
        }
    }

    // Draw current layer if it is not a camera layer
    const Layer* currentLayer = mEditor->layers()->currentLayer();
    if (!isCameraLayer(currentLayer))
    {
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter,
                       currentLayer,
                       mOffsetX,
                       getLayerY(mEditor->layers()->currentLayerIndex()),
                       actualWidth - mOffsetX,
                       mLayerHeight,
                       true,
                       mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter,
                       currentLayer,
                       0,
                       getLayerY(mEditor->layers()->currentLayerIndex()),
                       actualWidth - 1,
                       mLayerHeight,
                       true,
                       mEditor->layerVisibility());
        }
    }

    // --- draw track bar background
    painter.setPen(Qt::NoPen);
    if (mType == TIMELINE_CELL_TYPE::Layers) {
        painter.setBrush(QColor(0x11, 0x11, 0x11, 0xff));
        painter.drawRect(QRect(0, 0, width() - 1, mOffsetY));
    } else {
        painter.setBrush(QColor(0x1f, 0x1f, 0x1f, 0xff));
        painter.drawRect(QRect(0, 0, width() - 1, mOffsetY));

        // --- draw bottom line splitter for track bar
        painter.setPen(palette.color(QPalette::Mid));
        painter.drawLine(0, mOffsetY - 1, width() - 1, mOffsetY - 1);
    }

    // paintTicks will be called later after range markers to ensure ticks stay on top
    mRedrawContent = false;
}

void TimeLineCells::paintTicks(QPainter& painter, const QPalette& palette) const
{
    painter.setPen(palette.color(QPalette::Text));
    painter.setBrush(palette.brush(QPalette::Text));
    int fps = mEditor->playback()->fps();

    const int negativePaddingFrames = 1;

    int labelEvery = 1;
    if (fps > 0 && mFrameSize > 0)
    {
        const int minLabelPx = 40;
        const int minFrames = std::max(1, (minLabelPx + mFrameSize - 1) / mFrameSize);

        QList<int> candidates;
        candidates << 1 << 2 << 3 << 4 << 5 << 6 << 8 << 10 << 12 << 15 << 20 << 24 << 30 << 40 << 60;
        candidates.removeAll(0);
        candidates.append(fps);
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

        labelEvery = fps;
        for (int step : candidates)
        {
            if (step <= 0 || step > fps) {
                continue;
            }
            if (fps % step != 0) {
                continue;
            }
            if (step >= minFrames)
            {
                labelEvery = step;
                break;
            }
        }
        labelEvery = std::max(1, std::min(labelEvery, fps));
    }

    for (int i = mFrameOffset - negativePaddingFrames;
         i < mFrameOffset + (width() - mOffsetX) / mFrameSize;
         i++)
    {
        // line x pos + some offset
        const int lineX = getFrameX(i) + 1;
        // Range highlight line removed for cleaner look
        
        // Draw large tick at fps mark
        if (i % fps == 0 || i % fps == fps / 2)
        {
            painter.drawLine(lineX, 1, lineX, 5);
        }
        else // draw small tick
        {
            painter.drawLine(lineX, 1, lineX, 3);
        }
        if (labelEvery > 0 && (i % labelEvery == 0))
        {
            int incr = (i < 9) ? 4 : 0; // poor man's text centering
            // Adjust Y position for 9pt font - use font ascent for proper alignment
            QFontMetrics fm(painter.font());
            int textY = fm.ascent() + 2; // 2px padding from top
            painter.drawText(QPoint(lineX + incr, textY), QString::number(i + 1));
        }
    }
}

void TimeLineCells::paintTrack(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, int frameSize) const
{
    if (!layer) return;

    const QPalette palette = QApplication::palette();

    QColor tagBg;
    if (layer->type() == Layer::BITMAP) {
        auto it = sLayerTagByLayerId.constFind(layer->id());
        if (it != sLayerTagByLayerId.constEnd()) {
            const int tagIndex = it.value();
            const auto& defs = layerTagDefs();
            if (tagIndex >= 0 && tagIndex < defs.size()) {
                tagBg = defs[tagIndex].color;
                if (!layer->visible()) {
                    tagBg.setAlpha(90);
                }
            }
        }
    }
    QColor col;
    // Color each track according to the layer type
    if (layer->type() == Layer::BITMAP) col = QColor(51, 155, 252);
    if (layer->type() == Layer::VECTOR) col = QColor(70, 205, 123);
    if (layer->type() == Layer::SOUND) col = QColor(255, 141, 112);
    if (layer->type() == Layer::CAMERA) col = QColor(253, 202, 92);
    // Dim invisible layers
    if (!layer->visible()) col.setAlpha(64);

    painter.save();
    // Background should match the general UI base color, not a solid layer-color strip
    painter.setPen(Qt::NoPen);
    QColor baseTagBg = tagBg;
    if (selected && baseTagBg.isValid()) {
        baseTagBg = baseTagBg.lighter(115);
    }

    if (baseTagBg.isValid())
    {
        painter.setBrush(baseTagBg);
        painter.drawRect(x, y, width, height);
    }
    else
    {
        Q_UNUSED(frameSize);
        painter.setBrush(palette.color(QPalette::Base));
        painter.drawRect(x, y, width, height);
    }

    // Removed early return for invisible layers so their keyframes are still visible in the timeline
    // if (!layer->visible())
    // {
    //     painter.restore();
    //     return;
    // }

    // Changes the appearance if selected
    if (selected && !baseTagBg.isValid())
    {
        paintSelection(painter, x, y, width, height);
    }
    else
    {
    }

    // Draw per-frame cells for blank (non-key) frames
    if (false) {
        const int recTop = y + 1;
        const int standardWidth = frameSize - 2;
        const int recHeight = height - 4;

        // Determine visible frame range for this row
        const int firstVisibleFrame = std::max(1, mFrameOffset + 1);
        const int framesVisible = (width - (mOffsetX)) / frameSize + 2; // small buffer
        const int lastVisibleFrame = std::min(mFrameLength, firstVisibleFrame + framesVisible);

        painter.save();

        if (layer->type() == Layer::BITMAP)
        {
            // Bitmap layers: dark grey rounded rectangles for normal frames
            QColor normalFill(48, 48, 48);          // dark grey fill
            QColor borderColor(220, 220, 220);      // soft light grey border (near white, but not pure)
            QPen normalPen(borderColor);
            normalPen.setWidth(1);

            if (tagBg.isValid())
            {
                // Tagged rows: keep the row background fully tag-colored.
                // Do not fill the blank cells with dark grey (it reads as #303030).
                // Instead draw only subtle slot outlines so the tag background shows through.
                normalFill = QColor(0, 0, 0, 0);
                QColor outline = tagBg.darker(170);
                outline.setAlpha(170);
                normalPen.setColor(outline);
            }

            // Dim invisible bitmap layers further
            if (!layer->visible()) {
                // For tagged rows, keep blank-cell fill transparent so the tag row background remains visible.
                if (!tagBg.isValid()) {
                    normalFill.setAlpha(80);
                }
                QColor dimPen = normalPen.color();
                dimPen.setAlpha(120);
                normalPen.setColor(dimPen);
            }

            painter.setBrush(normalFill);
            painter.setPen(normalPen);
            painter.setRenderHint(QPainter::Antialiasing, true);
            const qreal radius = 4.0;
            for (int f = firstVisibleFrame; f <= lastVisibleFrame; ++f)
            {
                const int recLeft = getFrameX(f) - standardWidth;
                painter.drawRoundedRect(recLeft, recTop, standardWidth, recHeight, radius, radius);
            }
            painter.setRenderHint(QPainter::Antialiasing, false);
        }
        else
        {
            // Non-bitmap layers: preserve existing light grey rectangular cells
            QColor normalFill(230, 230, 230); // light grey
            QPen normalPen(QColor(70, 70, 70)); // dark grey outline
            normalPen.setWidth(1);

            // If the layer is invisible, further dim the fill and outlines of the blank cells
            if (!layer->visible()) {
                normalFill.setAlpha(80);
                QColor dimPen = normalPen.color();
                dimPen.setAlpha(120);
                normalPen.setColor(dimPen);
            }

            painter.setBrush(normalFill);
            painter.setPen(normalPen);
            for (int f = firstVisibleFrame; f <= lastVisibleFrame; ++f)
            {
                const int recLeft = getFrameX(f) - standardWidth;
                painter.drawRect(recLeft, recTop, standardWidth, recHeight);
            }
        }

        painter.restore();
    }

    paintFrames(painter, col, layer, y, height, selected, frameSize);

    // Apply dark tint overlay to non-active layers
    if (!selected && !tagBg.isValid()) {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setBrush(QColor(0, 0, 0, 80)); // Semi-transparent black overlay
        painter.setPen(Qt::NoPen);
        painter.drawRect(x, y, width, height);
        painter.restore();
    }

    painter.restore();
}

bool TimeLineCells::hasSelectedFramesForAnimation() const
{
    if (mType != TIMELINE_CELL_TYPE::Tracks) {
        return false;
    }
    if (mMovingFrames) {
        return true;
    }
    const Object* object = (mEditor ? mEditor->object() : nullptr);
    if (!object) {
        return false;
    }
    const QVector<int> visible = getVisibleLayerIndices();
    for (int idx = 0; idx < visible.size(); ++idx)
    {
        int i = visible[idx];
        if (i < 0 || i >= object->getLayerCount()) {
            continue;
        }
        const Layer* layer = object->getLayer(i);
        if (!layer || isCameraLayer(layer)) {
            continue;
        }
        if (!layer->getSelectedFramesByPos().isEmpty()) {
            return true;
        }
    }
    return false;
}

void TimeLineCells::paintAnimatedSelectedFramesOverlay(QPainter& painter) const
{
    if (mType != TIMELINE_CELL_TYPE::Tracks) {
        return;
    }
    // Skip the animated overlay while scrubbing — it iterates all layers/frames
    // and is purely cosmetic; not worth the cost during interactive scrub.
    if (mScrubbing) {
        return;
    }
    const Object* object = (mEditor ? mEditor->object() : nullptr);
    if (!object) {
        return;
    }
    const QVector<int> visible = getVisibleLayerIndices();
    for (int idx = 0; idx < visible.size(); ++idx)
    {
        int i = visible[idx];
        if (i < 0 || i >= object->getLayerCount()) {
            continue;
        }
        const Layer* layer = object->getLayer(i);
        paintSelectedFrames(painter, layer, i);
    }
}

void TimeLineCells::paintBlankDragSelectOverlay(QPainter& painter) const
{
    if (mBlankDragLayer < 0 || mBlankDragLayer >= mEditor->object()->getLayerCount()) return;

    const QColor accent = BlenderStyleSlider::getAccentColor();
    const QColor outline(0, 0, 0);

    const int layerY   = getLayerY(mBlankDragLayer);
    const int centerY  = layerY + mLayerHeight / 2;

    // X positions: center of start frame cell and center of end frame cell
    const int startX = getFrameX(mBlankDragStartFrame - 1) + mFrameSize / 2;
    const int endX   = getFrameX(mBlankDragEndFrame   - 1) + mFrameSize / 2;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // --- thick connecting line ---
    // Black outline first
    QPen outlinePen(outline);
    outlinePen.setWidth(5);
    outlinePen.setCapStyle(Qt::RoundCap);
    painter.setPen(outlinePen);
    painter.drawLine(startX, centerY, endX, centerY);

    // Accent fill on top
    QPen accentPen(accent);
    accentPen.setWidth(3);
    accentPen.setCapStyle(Qt::RoundCap);
    painter.setPen(accentPen);
    painter.drawLine(startX, centerY, endX, centerY);

    // --- dots ---
    const int dotRadius = 5;

    // Outline dot (start)
    painter.setPen(Qt::NoPen);
    painter.setBrush(outline);
    painter.drawEllipse(QPoint(startX, centerY), dotRadius + 1, dotRadius + 1);
    painter.setBrush(accent);
    painter.drawEllipse(QPoint(startX, centerY), dotRadius, dotRadius);

    // Outline dot (end / cursor)
    painter.setBrush(outline);
    painter.drawEllipse(QPoint(endX, centerY), dotRadius + 1, dotRadius + 1);
    painter.setBrush(accent);
    painter.drawEllipse(QPoint(endX, centerY), dotRadius, dotRadius);

    painter.restore();
}

void TimeLineCells::paintFrames(QPainter& painter, QColor trackCol, const Layer* layer, int y, int height, bool selected, int frameSize) const
{
    Q_UNUSED(trackCol);
    Q_UNUSED(selected);
    painter.setPen(Qt::NoPen);

    const int recTop = y;
    const int standardWidth = frameSize;

    const int recHeight = height;

    const QColor accentColor = BlenderStyleSlider::getAccentColor();
    const bool isBitmapLayer = (layer && layer->type() == Layer::BITMAP);

    const QList<int> selectedFrames = layer->getSelectedFramesByPos();
    const int dragFrameOffset = (mMovingFrames ? (getFrameNumber(mMouseMoveX) - getFrameNumber(mMousePressX)) : 0);
    
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        int framePos = key->pos();
        int recWidth = standardWidth;
        int drawFramePos = framePos;
        if (mMovingFrames && selectedFrames.contains(framePos)) {
            drawFramePos = framePos + dragFrameOffset;
        }
        int recLeft = getFrameX(drawFramePos - 1);

        if (key->length() > 1)
        {
            // This is especially for sound clips.
            // Sound clips are the only type of KeyFrame with variable frame length.
            recWidth = frameSize * key->length();
        }

        painter.setPen(Qt::NoPen);
        if (isBitmapLayer)
        {
            QColor fillAccent = accentColor;
            painter.setBrush(fillAccent);
            painter.drawRect(recLeft, recTop, recWidth, recHeight);
        }
        else
        {
            QColor fillAccent = accentColor;
            painter.setBrush(fillAccent);
            painter.drawRect(recLeft, recTop, recWidth, recHeight);
        }

        if (layer->type() == Layer::BITMAP)
        {
             // Draw Cell ID (larger font, clamped to available rect height)
             BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(key);
             if (bkf) {
                 int valueToDraw = -1;
                 const LayerBitmap* bitmapLayer = dynamic_cast<const LayerBitmap*>(layer);
                 if (bitmapLayer && bitmapLayer->cellBank()) {
                     // Use the numeric suffix of the cell's own name (e.g. "layer_004" -> 4)
                     // so the label stays stable after delete/reorder.
                     const QString rawName = bitmapLayer->cellBank()->getCellName(bkf->cellId());
                     static const QRegularExpression trailNum(QStringLiteral("(\\d+)$"));
                     const QRegularExpressionMatch m = trailNum.match(rawName);
                     if (m.hasMatch())
                         valueToDraw = m.captured(1).toInt();
                     else
                         valueToDraw = bkf->cellId();
                 }

                 painter.setPen(QColor(178, 178, 178));
                 QFont font = painter.font();
                 int maxSize = recHeight - 2;
                 int fontSize = std::min(16, maxSize);
                 if (fontSize < 8) fontSize = 8;
                 font.setPixelSize(fontSize);
                 painter.setFont(font);
                 painter.drawText(QRect(recLeft, recTop, recWidth, recHeight), Qt::AlignCenter, QString::number(valueToDraw));
             }
        }
    });
}

void TimeLineCells::paintCurrentFrameBorder(QPainter &painter, int recLeft, int recTop, int recWidth, int recHeight) const
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(Qt::white);
    painter.drawRect(recLeft, recTop, recWidth, recHeight);
    painter.restore();
}

void TimeLineCells::paintFrameCursorOnCurrentLayer(QPainter &painter, int recTop, int recWidth, int recHeight) const
{
    int recLeft = getFrameX(mFramePosMoveX - 1);

    painter.save();
    const QPalette palette = QApplication::palette();
    // Don't fill
    painter.setBrush(Qt::NoBrush);
    // paint border
    QColor penColor = palette.color(QPalette::WindowText);
    penColor.setAlpha(127);
    painter.setPen(penColor);
    painter.drawRect(recLeft, recTop, recWidth, recHeight);
    painter.restore();
}

void TimeLineCells::paintHighlightedFrame(QPainter& painter, int framePos, int recTop, int recWidth, int recHeight) const
{
    int recLeft = getFrameX(framePos - 1);

    painter.save();
    // Use highlighted keyframe color (hex: 489fe3)
    QColor highlightBlue(72, 159, 227, 255);
    painter.setBrush(highlightBlue);
    painter.setPen(QPen(QColor(40, 40, 40), 0.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    painter.drawRect(recLeft, recTop, recWidth, recHeight);
    painter.restore();
}

void TimeLineCells::paintRangeDimming(QPainter& painter) const
{
    // Only draw dimming if range is active
    int rangeLower = mTimeLine->getRangeLower();
    int rangeUpper = mTimeLine->getRangeUpper();
    
    if (rangeLower < 0 || rangeUpper < 0) {
        return;
    }
    
    painter.save();
    
    // Semi-transparent black for dimming
    QColor dimmingColor(0xFF, 0x00, 0x00, 0x41);
    painter.setBrush(dimmingColor);
    painter.setPen(Qt::NoPen);
    
    // Dim area before range (from left edge to start marker)
    int startFrameX = getFrameX(rangeLower - 1);
    if (startFrameX > 0) {
        QRect leftDimArea(0, 0, startFrameX, height());
        painter.drawRect(leftDimArea);
    }
    
    // Dim area after range (from end marker to right edge)
    int endFrameX = getFrameX(rangeUpper);
    if (endFrameX < width()) {
        QRect rightDimArea(endFrameX, 0, width() - endFrameX, height());
        painter.drawRect(rightDimArea);
    }
    
    painter.restore();
}

void TimeLineCells::paintRangeMarkers(QPainter& painter) const
{
    // Only draw range markers if range is active
    int rangeLower = mTimeLine->getRangeLower();
    int rangeUpper = mTimeLine->getRangeUpper();
    
    if (rangeLower < 0 || rangeUpper < 0) {
        return;
    }
    
    painter.save();
    
    const int capHeight = mOffsetY;
    const QColor capColor(0xFF, 0x00, 0x00, 255);
    const QColor lineColor(0xFF, 0x00, 0x00, 255);
    const QColor capBorderColor(0xA0, 0x00, 0x00, 255);
    
    // Draw start marker inside the previous frame cell.
    // If rangeLower is N, marker lives in frame (N-1) and ends at the start boundary of frame N.
    const int startBoundaryX = getFrameX(rangeLower - 1);
    const int startX = startBoundaryX - mRangeMarkerWidth;
    painter.setBrush(capColor);
    painter.setPen(QPen(capBorderColor, 1));
    QRect startMarkerCap(startX, 0, mRangeMarkerWidth, capHeight);
    painter.drawRect(startMarkerCap);

    painter.setBrush(Qt::NoBrush);
    QPen startLinePen(lineColor);
    startLinePen.setWidth(1);
    startLinePen.setCosmetic(true);
    painter.setPen(startLinePen);
    painter.drawLine(startBoundaryX, capHeight, startBoundaryX, height());
    
    // Draw end marker inside the next frame cell.
    // If rangeUpper is U, marker lives in frame (U+1) and starts at the boundary after frame U.
    const int endBoundaryX = getFrameX(rangeUpper);
    const int endX = endBoundaryX;
    painter.setBrush(capColor);
    painter.setPen(QPen(capBorderColor, 1));
    QRect endMarkerCap(endX, 0, mRangeMarkerWidth, capHeight);
    painter.drawRect(endMarkerCap);

    painter.setBrush(Qt::NoBrush);
    QPen endLinePen(lineColor);
    endLinePen.setWidth(1);
    endLinePen.setCosmetic(true);
    painter.setPen(endLinePen);
    painter.drawLine(endBoundaryX, capHeight, endBoundaryX, height());
    
    painter.restore();
}

bool TimeLineCells::isOverRangeStartMarker(int x) const
{
    int rangeLower = mTimeLine->getRangeLower();
    if (rangeLower < 0) return false;
    
    const int startBoundaryX = getFrameX(rangeLower - 1);
    const int startX = startBoundaryX - mRangeMarkerWidth;
    return (x >= startX && x <= startX + mRangeMarkerWidth);
}

bool TimeLineCells::isOverRangeEndMarker(int x) const
{
    int rangeUpper = mTimeLine->getRangeUpper();
    if (rangeUpper < 0) return false;
    
    const int endBoundaryX = getFrameX(rangeUpper);
    const int endX = endBoundaryX;
    return (x >= endX && x <= endX + mRangeMarkerWidth);
}

void TimeLineCells::paintSelectedFrames(QPainter& painter, const Layer* layer, const int layerIndex) const
{
    // layerIndex here is actual index; ensure the layer is visible
    if (isCameraLayer(layer)) return;
    
    const QList<int> selectedFrames = layer->getSelectedFramesByPos();

    if (selectedFrames.isEmpty()) {
        return;
    }
    
    const int dragFrameOffset = getFrameNumber(mMouseMoveX) - getFrameNumber(mMousePressX);
    int standardWidth = mFrameSize;
    int recWidth = standardWidth;
    int recHeight = mLayerHeight;
    int recTop = getLayerY(layerIndex);

    const QColor selectedBorder(255, 255, 255, 255);

    painter.save();
    for (int framePos : selectedFrames) {

        KeyFrame* key = layer->getKeyFrameAt(framePos);
        // Safeguard: key can be null if selection list references a just-removed keyframe
        if (!key) {
            continue;
        }
        if (key->length() > 1)
        {
            // This is a special case for sound clip.
            // Sound clip is the only type of KeyFrame that has variable frame length.
            recWidth = mFrameSize * key->length();
        }

        const int drawFramePos = mMovingFrames ? (framePos + dragFrameOffset) : framePos;
        const int drawFrameX = getFrameX(drawFramePos - 1);
        const QRectF r(drawFrameX, recTop, recWidth, recHeight);

        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setRenderHint(QPainter::Antialiasing, false);

        QColor outerGlow = selectedBorder;
        outerGlow.setAlpha(90);
        QPen glowPen(outerGlow, 4.0);
        glowPen.setCapStyle(Qt::SquareCap);
        glowPen.setJoinStyle(Qt::MiterJoin);
        glowPen.setCosmetic(true);
        painter.setPen(glowPen);
        painter.drawRect(r);

        QPen borderPen(selectedBorder, 2.0);
        borderPen.setCapStyle(Qt::SquareCap);
        borderPen.setJoinStyle(Qt::MiterJoin);
        borderPen.setCosmetic(true);
        painter.setPen(borderPen);
        painter.drawRect(r);

        painter.restore();
    }
    painter.restore();
}

void TimeLineCells::paintLabel(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, LayerVisibility layerVisibility) const
{
    if (!layer) return;
    if (isCameraLayer(layer)) return;
    Q_UNUSED(layerVisibility);

    // --- colors ---
    const QColor rowBg(0x1c, 0x1d, 0x1c);
    const QColor activeBg(0x2a, 0x2b, 0x2a);

    // Tag color for the left edge strip
    QColor tagBg;
    if (layer->type() == Layer::BITMAP) {
        auto it = sLayerTagByLayerId.constFind(layer->id());
        if (it != sLayerTagByLayerId.constEnd()) {
            const int tagIndex = it.value();
            const auto& defs = layerTagDefs();
            if (tagIndex >= 0 && tagIndex < defs.size()) {
                tagBg = defs[tagIndex].color;
                if (!layer->visible()) tagBg.setAlpha(90);
            }
        }
    }
    // Fallback strip color: neutral grey when no tag assigned
    QColor stripColor = tagBg;
    if (!stripColor.isValid()) {
        stripColor = QColor(0x66, 0x66, 0x66, 0xff);
        if (!layer->visible()) stripColor.setAlpha(90);
    }

    // --- full row background: only for active layer ---
    painter.setPen(Qt::NoPen);
    if (selected) {
        painter.setBrush(activeBg);
        painter.drawRect(x, y, width, height);
    }
    // inactive rows: transparent — let the widget background (#111111) show through

    // --- thin left-edge color strip (4px): only for active layer ---
    const int kStripW = 4;
    if (selected && stripColor.isValid()) {
        painter.setBrush(stripColor);
        painter.drawRect(x, y, kStripW, height);
    }

    // --- row separator ---
    const QColor separatorColor(0x11, 0x11, 0x11);
    painter.setPen(QPen(separatorColor));
    painter.drawLine(x, y + height - 1, x + width, y + height - 1);

    // --- layer name text ---
    // Layout: strip(4) + visibility widget(22) + gap(4) + text ... right-icon(22) + pad(4)
    const int kVisibilityW = 22;
    const int kRightIconW  = 22;
    const int kRightPad    = 4;
    const int textStartX   = x + kStripW + kVisibilityW + 4;
    const int textEndX     = x + width - kRightIconW - kRightPad;

    const int labelFontPx = qMax(14, mFontSize);
    {
        QFont f = this->font();
        f.setPixelSize(labelFontPx);
        f.setBold(selected);
        painter.setFont(f);
    }

    const Layer* editingLayer = (mIsInlineEditing && mEditor && mEditor->object()
                                 && mEditingLayerIndex >= 0
                                 && mEditingLayerIndex < mEditor->object()->getLayerCount())
                                    ? mEditor->object()->getLayer(mEditingLayerIndex)
                                    : nullptr;

    if (layer != editingLayer) {
        painter.setPen(QColor(0xcc, 0xcc, 0xcc));
        const QRect textRect(textStartX, y, qMax(1, textEndX - textStartX), height);
        const QFontMetrics fm(painter.font());
        const QString displayName = fm.elidedText(layer->name(), Qt::ElideRight, textRect.width());
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, displayName);
    }
}

void TimeLineCells::paintSelection(QPainter& painter, int x, int y, int width, int height) const
{
    painter.save();
    painter.setBrush(QColor(255, 255, 255, 22));
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y, width, height);
    painter.restore();
}

void TimeLineCells::paintLayerGutter(QPainter& painter) const
{
    Q_UNUSED(painter);
}

void TimeLineCells::paintOnionSkin(QPainter& painter) const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    int frameNumber = mEditor->currentFrame();

    int prevOnionSkinCount = mEditor->preference()->getInt(SETTING::ONION_PREV_FRAMES_NUM);
    int nextOnionSkinCount = mEditor->preference()->getInt(SETTING::ONION_NEXT_FRAMES_NUM);

    bool isAbsolute = (mEditor->preference()->getString(SETTING::ONION_TYPE) == "absolute");

    if (mEditor->preference()->isOn(SETTING::PREV_ONION) && prevOnionSkinCount > 0)
    {
        int onionFrameNumber = frameNumber;
        if (isAbsolute)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber+1, true);
        }
        onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute);
        int onionPosition = 0;

        while (onionPosition < prevOnionSkinCount && onionFrameNumber > 0)
        {
            painter.setBrush(QColor(128, 128, 128, 128));
            painter.setPen(Qt::NoPen);
            QRect onionRect;
            onionRect.setTopLeft(QPoint(getFrameX(onionFrameNumber - 1), 0));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), height()));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), mOffsetY - 1));
            painter.drawRect(onionRect);

            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute);
            onionPosition++;
        }
    }

    if (mEditor->preference()->isOn(SETTING::NEXT_ONION) && nextOnionSkinCount > 0) {

        int onionFrameNumber = layer->getNextFrameNumber(frameNumber, isAbsolute);
        int onionPosition = 0;

        while (onionPosition < nextOnionSkinCount && onionFrameNumber > 0)
        {
            painter.setBrush(QColor(128, 128, 128, 128));
            painter.setPen(Qt::NoPen);
            QRect onionRect;
            onionRect.setTopLeft(QPoint(getFrameX(onionFrameNumber - 1), 0));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), height()));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), mOffsetY - 1));
            painter.drawRect(onionRect);

            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, isAbsolute);
            onionPosition++;
        }
    }
}

void TimeLineCells::paintEvent(QPaintEvent*)
{
    const QPalette palette = QApplication::palette();
    QPainter painter(this);

    bool isPlaying = mEditor->playback()->isPlaying();
    if (mCache == nullptr || mRedrawContent || trackScrubber())
    {
        drawContent();
    }
    if (mCache)
    {
        painter.drawPixmap(QPoint(0, 0), *mCache);
    }

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        // NOTE: updateLayerRowControls() is intentionally NOT called here.
        // Widget management must never happen inside paintEvent — it can cause
        // recursive repaints. It is called from updateContent() instead.

        if (primaryButton == Qt::LeftButton && mStartLayerNumber != -1 && didDetachLayer() && mLayerInsertSlot >= 0)
        {
            const QVector<int> visible = getVisibleLayerIndices();
            const int n = visible.size();
            if (n > 0)
            {
                int slot = mLayerInsertSlot;
                if (slot < 0) slot = 0;
                if (slot > n) slot = n;

                int y = mOffsetY + slot * mLayerHeight;
                if (y < mOffsetY) y = mOffsetY;
                if (y > mOffsetY + n * mLayerHeight) y = mOffsetY + n * mLayerHeight;

                QColor lineColor = BlenderStyleSlider::getAccentColor();
                lineColor.setAlpha(220);
                painter.save();
                painter.setPen(Qt::NoPen);
                painter.setBrush(lineColor);
                const int thickness = 6;
                painter.drawRect(2, y - (thickness / 2), width() - 4, thickness);
                painter.restore();
            }
        }

        // Image drag-and-drop indicator
        if (mImageDragActive && mImageDropSlot >= 0)
        {
            const QVector<int> visible = getVisibleLayerIndices();
            const int n = visible.size();
            int slot = qBound(0, mImageDropSlot, n);
            int y = mOffsetY + slot * mLayerHeight;

            const QColor accent = CosmicColor::accent();
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
            const int thickness = 3;
            painter.drawRoundedRect(4, y - (thickness / 2), width() - 8, thickness, 1, 1);
            // End dots
            painter.setBrush(accent);
            painter.drawEllipse(QPoint(4, y), 4, 4);
            painter.drawEllipse(QPoint(width() - 4, y), 4, 4);
            painter.restore();
        }
    }

    if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        {
            painter.save();
            QPen gridPen(QColor(0, 0, 0, 85));
            gridPen.setWidth(1);
            gridPen.setCosmetic(true);
            painter.setPen(gridPen);

            const int negativePaddingFrames = 1;
            const int firstIndex = mFrameOffset - negativePaddingFrames;
            const int lastIndex = mFrameOffset + (width() - mOffsetX) / mFrameSize + 2;

            // Batch all vertical grid lines into a single drawLines() call
            // instead of N individual drawLine() calls — one GPU state change.
            QVector<QLine> gridLines;
            const int h = height();
            gridLines.reserve((lastIndex - firstIndex + 1));
            for (int i = firstIndex; i <= lastIndex; ++i) {
                const int x = getFrameX(i);
                gridLines.append(QLine(x, 0, x, h));
            }

            // Horizontal grid lines per layer row (frames area only).
            // Draw only the bottom edge of each row — adjacent rows share borders,
            // and the header already provides the top boundary for the first row.
            const QVector<int> visible = getVisibleLayerIndices();
            const int xStart = getFrameX(mFrameOffset - negativePaddingFrames);
            const int xEnd = width();
            for (int idx : visible) {
                const int yTop = getLayerY(idx);
                const int yBottom = yTop + mLayerHeight;

                // Clamp to widget bounds.
                if (yBottom < mOffsetY || yTop > height()) {
                    continue;
                }

                gridLines.append(QLine(xStart, yBottom, xEnd, yBottom));
            }

            painter.drawLines(gridLines);
            painter.restore();
        }

        if (!isPlaying && !mScrubbing) {
            paintOnionSkin(painter);
        }

        int currentFrame = mEditor->currentFrame();
        Layer* currentLayer = mEditor->layers()->currentLayer();
        if (!isCameraLayer(currentLayer))
        {
            KeyFrame* keyFrame = currentLayer->getKeyFrameWhichCovers(currentFrame);
            if (keyFrame != nullptr)
            {
                const int recWidth = mFrameSize * keyFrame->length();
                const int recLeft = getFrameX(keyFrame->pos() - 1);
                paintCurrentFrameBorder(painter, recLeft, getLayerY(mEditor->currentLayerIndex()), recWidth, mLayerHeight);
            }
        }

        if (!mMovingFrames && mLayerPosMoveY != -1 && mLayerPosMoveY == mEditor->currentLayerIndex() && !isCameraLayer(currentLayer))
        {
            // This is terrible but well...
            int recTop = getLayerY(mLayerPosMoveY);
            int standardWidth = mFrameSize;
            int recHeight = mLayerHeight;

            if (mHighlightFrameEnabled)
            {
                paintHighlightedFrame(painter, mHighlightedFrame, recTop, standardWidth, recHeight);
            }
            if (currentLayer->visible())
            {
                paintFrameCursorOnCurrentLayer(painter, recTop, standardWidth, recHeight);
            }
        }

        // Draw range dimming/markers first, then re-paint header ticks so they stay on top.
        paintRangeDimming(painter);
        paintRangeMarkers(painter);
        paintTicks(painter, palette);

        paintAnimatedSelectedFramesOverlay(painter);
        // NOTE: animated overlay is skipped during scrubbing (mScrubbing guard is inside paintAnimatedSelectedFramesOverlay)

        // Blank-frame drag select overlay
        if (mBlankDragSelecting && mBlankDragDidMove)
        {
            paintBlankDragSelectOverlay(painter);
        }

        // --- draw the position of the current frame
        if (currentFrame > mFrameOffset)
        {
            QColor scrubColor(CosmicColor::accent());
            painter.setBrush(scrubColor);
            painter.setPen(Qt::NoPen);

            int currentFrameStartX = getFrameX(currentFrame - 1) + 1;
            int currentFrameEndX = getFrameX(currentFrame);
            QRect scrubRect;
            scrubRect.setTopLeft(QPoint(currentFrameStartX, 0));
            scrubRect.setBottomRight(QPoint(currentFrameEndX, height()));
            painter.save();

            // Always show the enhanced playhead look (previously only shown when mouse was over it)
            QRect smallScrub = QRect(QPoint(currentFrameStartX, 0), QPoint(currentFrameEndX, mOffsetY - 1));
            QPen pen = scrubColor;
            pen.setWidth(2);
            painter.setPen(pen);
            painter.drawRect(smallScrub);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(scrubRect);
            painter.restore();

            painter.setPen(palette.color(QPalette::HighlightedText));
            int incr = (currentFrame < 10) ? 4 : 0;
            // Adjust Y position for 9pt font - use font ascent for proper alignment
            QFontMetrics fm(painter.font());
            int textY = fm.ascent() + 2; // 2px padding from top
            painter.drawText(QPoint(currentFrameStartX + incr, textY),
                             QString::number(currentFrame));
        }
    }
}

void TimeLineCells::resizeEvent(QResizeEvent* event)
{
    clearCache();
    updateContent();
    event->accept();
    emit lengthChanged(getFrameLength());
}

bool TimeLineCells::event(QEvent* event)
{
    if (event->type() == QEvent::Leave) {
        onDidLeaveWidget();
    }

    return QWidget::event(event);
}

void TimeLineCells::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    setProperty("timelineTracks", mType == TIMELINE_CELL_TYPE::Tracks);

    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());
    mCurrentLayerNumber = layerNumber;

    mMousePressX = event->pos().x();
    mPressPos = event->pos(); // Store press position for drag detection
    mFromLayer = mToLayer = layerNumber;

    mStartY = event->pos().y();
    mStartLayerNumber = layerNumber;
    mEndY = event->pos().y();
    mLayerInsertSlot = -1;

    mStartFrameNumber = frameNumber;
    mLastFrameNumber = mStartFrameNumber;

    mCanMoveFrame = false;
    mMovingFrames = false;

    mCanBoxSelect = false;
    mBoxSelecting = false;

    mClickSelecting = false;
    
    // Reset blank-frame drag select state
    mBlankDragSelecting  = false;
    mBlankDragDidMove    = false;
    mBlankDragStartFrame = -1;
    mBlankDragEndFrame   = -1;
    mBlankDragLayer      = -1;

    // Reset new state variables
    mTempSelectMode = false;
    mShiftRangeMode = false;
    mClickDeselectPending = false;
    // mShiftOriginFrame is NOT reset here if Shift is held, but logic below handles it.
    // Actually, usually we reset state on press if not continuing an operation.
    // But Shift-click needs to know if it's a new chain or not. 
    // Standard Qt behavior: Shift+Click uses the "anchor" established by the last valid selection.
    // We'll let the logic below determine how to use/set mShiftOriginFrame.

    primaryButton = event->button();

    switch (mType)
    {
    case TIMELINE_CELL_TYPE::Layers:
        if (layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
        {
            if (event->button() == Qt::RightButton && event->pos().x() >= 15) {
                showLayerMenu(event->pos());
                break;
            }
            if (event->pos().x() < 15)
            {
                mEditor->switchVisibilityOfLayer(layerNumber);
            }
            else if (mEditor->currentLayerIndex() != layerNumber)
            {
                mEditor->layers()->setCurrentLayer(layerNumber);
                mEditor->layers()->currentLayer()->deselectAll();
            }
        }
        break;
    case TIMELINE_CELL_TYPE::Tracks:
        // Check for range marker clicks first
        if (event->button() == Qt::LeftButton && mTimeLine->getRangeLower() >= 0)
        {
            int mouseX = event->pos().x();
            if (isOverRangeStartMarker(mouseX))
            {
                mDraggingRangeStart = true;
                return;
            }
            else if (isOverRangeEndMarker(mouseX))
            {
                mDraggingRangeEnd = true;
                return;
            }
        }
        
        if (event->button() == Qt::MiddleButton)
        {
            mLastFrameNumber = getFrameNumber(event->pos().x());
        }
        else
        {
            if (frameNumber == mEditor->currentFrame() && mStartY < mOffsetY)
            {
                if (mEditor->playback()->isPlaying())
                {
                    mEditor->playback()->stop();
                }
                mTimeLine->scrubbing = true;
            }
            else
            {
                if ((layerNumber != -1) && layerNumber < mEditor->object()->getLayerCount())
                {
                    int previousLayerNumber = mEditor->layers()->currentLayerIndex();

                    if (previousLayerNumber != layerNumber)
                    {
                        Layer *previousLayer = mEditor->object()->getLayer(previousLayerNumber);
                        previousLayer->deselectAll();
                        emit mEditor->selectedFramesChanged();
                        mEditor->layers()->setCurrentLayer(layerNumber);
                    }

                    Layer *currentLayer = mEditor->object()->getLayer(layerNumber);

                    if (event->button() == Qt::LeftButton && frameNumber > 0) {
                        if (mEditor->playback()->isPlaying()) mEditor->playback()->stop();
                        mEditor->scrubTo(frameNumber);
                    }

                    // --- NEW SELECTION LOGIC ---

                    // Initialize Deep Interaction Mode (Ctrl held?)
                    mDeepInteract = isDeepSelectMode(event->modifiers());

                    // Alt+Click: Select After (Logic remains similar, but ensure Temp mode)
                    if (event->modifiers() & Qt::AltModifier)
                    {
                        mTempSelectMode = true; 
                        mMultiSelectMode = true; 
                        
                        if (mDeepInteract) {
                            deepSelectAllFramesAfter(frameNumber);
                        } else {
                            currentLayer->selectAllFramesAfter(frameNumber);
                        }
                        emit mEditor->selectedFramesChanged();

                        mCanMoveFrame = true;
                        mClickSelecting = true; 
                        updateContent();
                    }
                    else if (event->modifiers() & Qt::ShiftModifier)
                    {
                        // SHIFT + Click: "Paint Select" Start
                        // Requirement: "if drag begins on the first keyframe... all keyframes get the selected state of the initial keyframe"
                        // Logic: Toggle current frame, then capture its NEW state as the "Target State" for the drag.
                        // Also: Additive behavior (don't clear selection).
                        
                        // 1. Toggle the frame under cursor (Standard Shift-Click behavior)
                        // If Deep: Toggle deeply?
                        // Standard behavior: Toggle current, capture state, apply deep.
                        
                        currentLayer->toggleFrameSelected(frameNumber, true);
                        
                        // 2. Capture the state we just set (Is it now Selected or Deselected?)
                        bool isNowSelected = currentLayer->isFrameSelected(frameNumber);
                        mDragSelectTargetState = isNowSelected;
                        
                        // If Deep Mode, ensure all layers match this state for this frame immediately
                        if (mDeepInteract) {
                            deepSelectFrame(frameNumber, isNowSelected);
                        }
                        
                        // 3. Set Origin
                        mShiftOriginFrame = frameNumber;
                        
                        // 4. Setup Drag
                        mMultiSelectMode = true;
                        mCanBoxSelect = true; 
                        
                        emit mEditor->selectedFramesChanged();
                        updateContent();
                    }
                    else 
                    {
                        // NO MODIFIERS
                        
                        // Case A: Clicking an ALREADY SELECTED frame
                        if (currentLayer->isFrameSelected(frameNumber))
                        {
                             // Drag → move frames. Simple click (no drag) → deselect all.
                             mCanMoveFrame = true;
                             mTempSelectMode = false;
                             mClickDeselectPending = false;
                        }
                        else if (currentLayer->keyExistsWhichCovers(frameNumber))
                        {
                            // Case B: Clicking an UNSELECTED KEYFRAME
                            // Deferred selection: only select if we drag. Release without drag = deselect all.
                            mTempSelectMode = true; 
                            mCanMoveFrame = false; 
                            mMultiSelectMode = false;
                        }
                        else
                        {
                            // Case D: Clicking BLANK FRAME
                            // Start blank-frame drag select. Deselect existing selection immediately.
                            deepDeselectAll();
                            emit mEditor->selectedFramesChanged();

                            // Arm the blank drag select — visual + selection fires on drag/release
                            mBlankDragSelecting = true;
                            mBlankDragDidMove   = false;
                            mBlankDragStartFrame = frameNumber;
                            mBlankDragEndFrame   = frameNumber;
                            mBlankDragLayer      = layerNumber;
                        }
                    }

                    if (event->button() == Qt::RightButton)
                    {
                        showCameraMenu(event->pos());
                    }

                    if (currentLayer->hasAnySelectedFrames()) {
                        emit selectionChanged();
                    }

                    mTimeLine->updateContent();
                }
                else
                {
                    if (frameNumber > 0)
                    {
                        if (mEditor->playback()->isPlaying())
                        {
                            mEditor->playback()->stop();
                        }
                        if (mEditor->playback()->getSoundScrubActive() && mLastScrubFrame != frameNumber)
                        {
                            mEditor->playback()->playScrub(frameNumber);
                            mLastScrubFrame = frameNumber;
                        }

                        // Clamp scrubbing to active playback range (if enabled)
                        const int rangeLower = mTimeLine->getRangeLower();
                        const int rangeUpper = mTimeLine->getRangeUpper();
                        if (rangeLower > 0 && rangeUpper >= rangeLower) {
                            frameNumber = qBound(rangeLower, frameNumber, rangeUpper);
                        }

                        mEditor->scrubTo(frameNumber);

                        mTimeLine->scrubbing = true;
                        mEditor->setTimelineScrubbing(true);
                        // Reset throttle so the first move after press is never skipped.
                        mScrubThrottle.invalidate();
                        mLastScrubFrame = frameNumber;
                    }
                }
            }
        }
        break;
    }
}

void TimeLineCells::mouseMoveEvent(QMouseEvent* event)
{
    mMouseMoveX = event->pos().x();
    mFramePosMoveX = getFrameNumber(mMouseMoveX);
    mLayerPosMoveY = getLayerNumber(event->pos().y());

    // Update cursor when over range markers (if not dragging)
    if (mType == TIMELINE_CELL_TYPE::Tracks && !mDraggingRangeStart && !mDraggingRangeEnd)
    {
        if (isOverRangeStartMarker(mMouseMoveX) || isOverRangeEndMarker(mMouseMoveX))
        {
            setCursor(Qt::SizeHorCursor);
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }
    }

    // Handle range marker dragging
    if (mDraggingRangeStart || mDraggingRangeEnd)
    {
        // Range markers represent *boundaries*.
        // Start marker ends on its boundary (sample right edge). End marker starts on its boundary (sample left edge).
        const int boundaryFrame = mDraggingRangeEnd
                                     ? getFrameNumber(event->pos().x())
                                     : getFrameNumber(event->pos().x() + mRangeMarkerWidth);
        int frameNumber = boundaryFrame;
        if (frameNumber < 1) frameNumber = 1;
        
        if (mDraggingRangeStart)
        {
            // Left marker: constrain to left side of frames (can't exceed right marker)
            int rangeUpper = mTimeLine->getRangeUpper();
            qDebug() << "[RangeMarker] Dragging START to frame" << frameNumber << "(upper=" << rangeUpper << ")";
            if (frameNumber < rangeUpper)
            {
                mTimeLine->setRangeLower(frameNumber);
                update();
            }
        }
        else if (mDraggingRangeEnd)
        {
            // Right marker: constrain to right side of frames (can't go before left marker)
            int rangeLower = mTimeLine->getRangeLower();
            const int endFrame = frameNumber - 1;
            qDebug() << "[RangeMarker] Dragging END to frame" << endFrame << "(lower=" << rangeLower << ")";
            if (endFrame > rangeLower)
            {
                mTimeLine->setRangeUpper(endFrame);
                update();
            }
        }
        return;
    }

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        if (event->buttons() & Qt::LeftButton ) {
            mEndY = event->pos().y();
            emit mouseMovedY(mEndY - mStartY);

            if (mStartLayerNumber != -1 && didDetachLayer())
            {
                mLayerInsertSlot = getLayerInsertSlot(mEndY);
                update();
            }
        }
    }
    else if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        if (primaryButton == Qt::MiddleButton)
        {
            mFrameOffset = qMin(qMax(0, mFrameLength - width() / getFrameSize()), qMax(0, mFrameOffset + mLastFrameNumber - mFramePosMoveX));
            update();
            emit offsetChanged(mFrameOffset);
        }
        else
        {
            if (mTimeLine->scrubbing)
            {
                int scrubFrame = mFramePosMoveX;
                // Clamp scrubbing to active playback range (if enabled)
                const int rangeLower = mTimeLine->getRangeLower();
                const int rangeUpper = mTimeLine->getRangeUpper();
                if (rangeLower > 0 && rangeUpper >= rangeLower) {
                    scrubFrame = qBound(rangeLower, scrubFrame, rangeUpper);
                    mFramePosMoveX = scrubFrame;
                }

                // Skip if we haven't moved to a new frame — avoids redundant scrubTo
                // calls (and the full canvas repaint they trigger) when the mouse moves
                // within the same frame cell.
                if (scrubFrame == mLastScrubFrame) {
                    return;
                }

                // Rate-limit to ~60 fps so a very fast mouse can't flood the pipeline.
                // 16 ms ≈ one 60 Hz frame; still feels instant to the user.
                if (mScrubThrottle.isValid() && mScrubThrottle.elapsed() < 16) {
                    return;
                }
                mScrubThrottle.restart();

                if (mEditor->playback()->getSoundScrubActive())
                {
                    mEditor->playback()->playScrub(scrubFrame);
                }
                mLastScrubFrame = scrubFrame;
                mEditor->scrubTo(scrubFrame);
            }
            else
            {

                if (event->buttons() & Qt::LeftButton) {
                    // Handle blank-frame drag select
                    if (mBlankDragSelecting)
                    {
                        QPoint diff = event->pos() - mPressPos;
                        if (diff.manhattanLength() > QApplication::startDragDistance())
                        {
                            mBlankDragDidMove = true;
                            // End frame is determined by cursor X mapped to the original layer
                            int cursorFrame = getFrameNumber(event->pos().x());
                            if (cursorFrame < 1) cursorFrame = 1;
                            mBlankDragEndFrame = cursorFrame;
                        }
                        update();
                        return;
                    }

                    if (mStartLayerNumber != -1 && mStartLayerNumber < mEditor->object()->getLayerCount())
                    {
                        Layer *currentLayer = mEditor->object()->getLayer(mStartLayerNumber);

                        QPoint diff = event->pos() - mPressPos;
                        bool isDragging = (diff.manhattanLength() > QApplication::startDragDistance());

                        if (isDragging)
                        {
                            // Any drag cancels the pending simple-click deselect
                            mClickDeselectPending = false;

                            if (event->modifiers() & Qt::ShiftModifier)
                            {
                                // SHIFT Drag: Paint Selection (Additive/Subtractive based on start)
                                mShiftRangeMode = true; 
                                mMovingFrames = false; 
                                
                                // Perform Range Selection from Origin to Current
                                // Logic: Apply mDragSelectTargetState to all frames in range [Origin, Current]
                                
                                int start = qMin(mShiftOriginFrame, mFramePosMoveX);
                                int end = qMax(mShiftOriginFrame, mFramePosMoveX);
                                
                                bool isDeep = isDeepSelectMode(event->modifiers());
                                if (isDeep) mDeepInteract = true;

                                for (int i = start; i <= end; ++i) {
                                    if (isDeep) {
                                        deepSelectFrame(i, mDragSelectTargetState);
                                    } else {
                                        currentLayer->setFrameSelected(i, mDragSelectTargetState);
                                    }
                                }
                                
                                emit mEditor->selectedFramesChanged();
                            }
                            else if (mCanMoveFrame)
                            {
                                // Normal or Alt Drag (Existing Selection): Move Frames
                                mMovingFrames = true;
                               // mTempSelectMode state is already correct from Press
                            }
                            else if (mTempSelectMode)
                            {
                                // Deferred Selection Drag!
                                // We clicked an unselected frame but didn't select it yet.
                                // Now we are dragging, so we Select, Move, and set Temp Mode.
                                // Check if Ctrl is held for Deep Select
                                
                                bool isDeep = isDeepSelectMode(event->modifiers());
                                if (isDeep) mDeepInteract = true;
                                
                                if (isDeep) {
                                    deepDeselectAll();
                                    deepSelectFrame(mStartFrameNumber, true);
                                } else {
                                    currentLayer->deselectAll();
                                    currentLayer->setFrameSelected(mStartFrameNumber, true); 
                                }
                                emit mEditor->selectedFramesChanged();
                                
                                mMovingFrames = true;
                                mCanMoveFrame = true; // Now we can move
                                
                                // mTempSelectMode remains true (Auto-deselect on release)
                            }
                        }

                        // If moving, we update the UI to show the ghost frames (handled in paintEvent via mMovingFrames)
                        // If range selecting, we updated the selection above.
                        
                        mLastFrameNumber = mFramePosMoveX;
                        updateContent();
                    }
                }
                update();
            }
        }
    }
}

void TimeLineCells::mouseReleaseEvent(QMouseEvent* event)
{
    // Reset range marker dragging state
    if (mDraggingRangeStart || mDraggingRangeEnd)
    {
        mDraggingRangeStart = false;
        mDraggingRangeEnd = false;
        update();
        return;
    }

    if (event->button() != primaryButton) return;

    int frameNumber = getFrameNumber(event->pos().x());
    if (frameNumber < 1) frameNumber = 1;
    int layerNumber = getLayerNumber(event->pos().y());
    Q_UNUSED(layerNumber);

    if (mType == TIMELINE_CELL_TYPE::Tracks && mCurrentLayerNumber != -1 && primaryButton != Qt::MiddleButton)
    {
        // Finalize blank-frame drag select
        if (mBlankDragSelecting)
        {
            if (mBlankDragDidMove && mBlankDragLayer >= 0 && mBlankDragLayer < mEditor->object()->getLayerCount())
            {
                Layer* dragLayer = mEditor->object()->getLayer(mBlankDragLayer);
                int startF = qMin(mBlankDragStartFrame, mBlankDragEndFrame);
                int endF   = qMax(mBlankDragStartFrame, mBlankDragEndFrame);
                for (int f = startF; f <= endF; ++f)
                {
                    if (dragLayer->keyExistsWhichCovers(f))
                        dragLayer->setFrameSelected(f, true);
                }
                emit mEditor->selectedFramesChanged();
                emit selectionChanged();
            }
            // simple click on blank → already deselected in press, nothing more to do
            mBlankDragSelecting = false;
            mBlankDragDidMove   = false;
            mBlankDragStartFrame = -1;
            mBlankDragEndFrame   = -1;
            mBlankDragLayer      = -1;
            updateContent();
            return;
        }

        // We should affect the current layer based on what's selected, not where the mouse currently is.
        Layer* currentLayer = mEditor->layers()->getLayer(mCurrentLayerNumber);
        
        if (mMovingFrames)
        {
            int posUnderCursor = getFrameNumber(mMousePressX);
            int offset = frameNumber - posUnderCursor;

            // FIX: Always use deep move. 
            // If background layers have selection, they move. 
            // If only current layer has selection, only it moves.
            deepMoveSelectedFrames(offset);
            
            mEditor->updateUndoContext();

            // Auto-Deselect Logic for Temp Mode (Single Drag) or Alt Mode
            if (mTempSelectMode) 
            {
                if (mDeepInteract) {
                    deepDeselectAll();
                } else {
                    currentLayer->deselectAll();
                }
                emit mEditor->selectedFramesChanged();
            }
            // Standard behavior: Keep selection if not temp mode (e.g. if we add other modes later)

            mEditor->layers()->notifyAnimationLengthChanged();
            emit mEditor->framesModified();
            updateContent();
        }
        else if (!mTimeLine->scrubbing && !mBoxSelecting && !mClickSelecting)
        {
            // A simple click selects the numbered drawing for Copy; dragging
            // still uses the existing deferred selection/move path.
            if (mTempSelectMode && !mMovingFrames)
            {
                deepDeselectAll();
                if (currentLayer->keyExists(mStartFrameNumber))
                    currentLayer->setFrameSelected(mStartFrameNumber, true);
                emit mEditor->selectedFramesChanged();
                emit selectionChanged();
            }
            // Already-selected frame simple click (no drag) -> Deselect All
            else if (mClickDeselectPending)
            {
                deepDeselectAll();
                emit mEditor->selectedFramesChanged();
            }
            // Alt Click Release -> Deselect All (if not dragging, which sets mMovingFrames)
            else if (event->modifiers() & Qt::AltModifier)
            {
                 if (mDeepInteract) {
                     deepDeselectAll();
                 } else {
                     currentLayer->deselectAll();
                 }
                 emit mEditor->selectedFramesChanged();
            }
            
            updateContent();
        }
        else if ((event->modifiers() & Qt::AltModifier) && !mMovingFrames)
        {
            // Fallback for Alt if mClickSelecting prevented entry above
            if (!(event->modifiers() & Qt::ShiftModifier) && !(event->modifiers() & Qt::AltModifier))
            {
                deepDeselectAll(); // Require Deep Deselect for consistency
                emit mEditor->selectedFramesChanged();
            }
            updateContent();
        }
    }
    if (mType == TIMELINE_CELL_TYPE::Layers && !mScrollingVertically && primaryButton == Qt::LeftButton && mStartLayerNumber != -1 && didDetachLayer())
    {
        const QVector<int> visible = getVisibleLayerIndices();
        const int n = visible.size();
        if (n > 0)
        {
            const int fromActual = mStartLayerNumber;
            int fromPos = -1;
            for (int i = 0; i < n; ++i) {
                if (visible[i] == fromActual) { fromPos = i; break; }
            }

            if (fromPos != -1)
            {
                int slot = (mLayerInsertSlot >= 0) ? mLayerInsertSlot : getLayerInsertSlot(event->pos().y());
                if (slot < 0) slot = 0;
                if (slot > n) slot = n;

                // Convert visual-top slot to insertion position in bottom->top visible array.
                int insertPos = n - slot; // 0..n
                if (insertPos < 0) insertPos = 0;
                if (insertPos > n) insertPos = n;

                int newPos = insertPos;
                if (newPos > fromPos) {
                    newPos -= 1;
                }
                if (newPos < 0) newPos = 0;
                if (newPos > n - 1) newPos = n - 1;

                if (newPos != fromPos)
                {
                    const int toActual = visible[newPos];
                    if (toActual < fromActual)
                    {
                        for (int i = fromActual - 1; i >= toActual; --i) {
                            mEditor->swapLayers(i, i + 1);
                        }
                    }
                    else
                    {
                        for (int i = fromActual + 1; i <= toActual; ++i) {
                            mEditor->swapLayers(i, i - 1);
                        }
                    }
                }
            }
        }
    }

    if (mType == TIMELINE_CELL_TYPE::Layers && event->button() == Qt::LeftButton)
    {
        emit mouseMovedY(0);
    }

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        mLayerInsertSlot = -1;
    }

    primaryButton = Qt::NoButton;
    mEndY = mStartY;
    mTimeLine->scrubbing = false;
    mEditor->setTimelineScrubbing(false); // also flushes deferred updateActiveFrames/updateUndoContext
    mScrubThrottle.invalidate(); // allow immediate response on next scrub
    mMovingFrames = false;
    mClickDeselectPending = false;

    // Force a clean canvas repaint at the final scrub position.
    // During scrubbing we skipped the layer cache reset for speed; now that
    // the user has released, do a proper invalidation so the canvas is correct.
    if (mType == TIMELINE_CELL_TYPE::Tracks) {
        mEditor->getScribbleArea()->onFrameModified(mEditor->currentFrame());
    }
}

void TimeLineCells::mouseDoubleClickEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());

    // -- layer --
    Layer* layer = nullptr;
    if (layerNumber >= 0 && layerNumber < mEditor->object()->getLayerCount())
    {
        layer = mEditor->object()->getLayer(layerNumber);
    }

    if (layer && (event->buttons() & Qt::LeftButton))
    {
        if (mType == TIMELINE_CELL_TYPE::Tracks && layerNumber != -1 && frameNumber > 0)
        {
            // Bitmap layers: start inline numeric editor to assign/swap cell exposure by ID
            if (layer->type() == Layer::BITMAP && event->pos().y() >= mOffsetY)
            {
                // Ensure the clicked layer is current and scrub to the frame for consistency
                if (mEditor->layers()->currentLayerIndex() != layerNumber)
                {
                    mEditor->layers()->setCurrentLayer(layerNumber);
                }
                mEditor->scrubTo(frameNumber);

                startInlineCellEdit(layerNumber, frameNumber);
                event->accept();
                return;
            }
            else
            {
                // Non-bitmap or header area: preserve existing behavior
                if (!layer->keyExistsWhichCovers(frameNumber))
                {
                    mEditor->scrubTo(frameNumber);
                }

                // The release event will toggle the frame on again, so we make sure it gets
                // deselected now instead.
                layer->setFrameSelected(frameNumber, true);
            }
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers && event->pos().x() >= 15)
        {
            startInlineRename(layerNumber);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimeLineCells::editLayerProperties(Layer *layer) const
{
    if (layer->type() != Layer::CAMERA)
    {
        editLayerName(layer);
        return;
    }

    auto cameraLayer = dynamic_cast<LayerCamera*>(layer);
    Q_ASSERT(cameraLayer);
    editLayerProperties(cameraLayer);
}

void TimeLineCells::editLayerProperties(LayerCamera* cameraLayer) const
{
    QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");

    CosmicCameraPropertiesPopup dialog(cameraLayer->name(), cameraLayer->getViewRect().width(),
                                      cameraLayer->getViewRect().height());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    QString name = dialog.cameraName().replace(regex, "");

    if (!name.isEmpty())
    {
        mEditor->layers()->renameLayer(cameraLayer, name);
    }
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(SETTING_FIELD_W, dialog.cameraWidth());
    settings.setValue(SETTING_FIELD_H, dialog.cameraHeight());
    cameraLayer->setViewRect(QRect(-dialog.cameraWidth() / 2, -dialog.cameraHeight() / 2, dialog.cameraWidth(), dialog.cameraHeight()));
    mEditor->view()->forceUpdateViewTransform();
}

void TimeLineCells::editLayerName(Layer* layer) const
{
    QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");

    bool ok;
    QString name = QInputDialog::getText(const_cast<TimeLineCells*>(this), tr("Layer Properties"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         layer->name(), &ok);
    name.replace(regex, "");
    if (!ok || name.isEmpty())
    {
        return;
    }

    mEditor->layers()->renameLayer(layer, name);
}

void TimeLineCells::startInlineRename(int layerIndex)
{
    if (mIsInlineEditing) return;
    if (layerIndex < 0 || layerIndex >= mEditor->object()->getLayerCount()) return;
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    if (isCameraLayer(layer)) return;

    mEditingLayerIndex = layerIndex;
    mIsInlineEditing = true;

    if (!mInlineEditor)
    {
        mInlineEditor = new QLineEdit(this);
        mInlineEditor->installEventFilter(this);
        mInlineEditor->setFrame(false);
        mInlineEditor->setTextMargins(0, 0, 0, 0);
        mInlineEditor->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mInlineEditor->setStyleSheet(CosmicInlineEdit::stylesheet());
    }

    QFont f = this->font();
    f.setPixelSize(qMax(14, mFontSize));
    mInlineEditor->setFont(f);
    mInlineEditor->setText(layer->name());

    mInlineEditor->setGeometry(inlineRenameRectForLayer(layerIndex));
    mInlineEditor->show();
    mInlineEditor->raise();
    mInlineEditor->setFocus(Qt::OtherFocusReason);
    mInlineEditor->grabKeyboard();
    mInlineEditor->selectAll();

    // Install app-level event filter to auto-confirm on outside interaction
    if (!mHasAppEventFilter)
    {
        qApp->installEventFilter(this);
        mHasAppEventFilter = true;
    }
}

void TimeLineCells::finishInlineRename(bool accept)
{
    if (!mIsInlineEditing) return;
    Layer* layer = mEditor->object()->getLayer(mEditingLayerIndex);
    if (accept)
    {
        QString name = mInlineEditor->text().trimmed();
        if (!name.isEmpty())
        {
            mEditor->layers()->renameLayer(layer, name);
        }
    }
    if (mInlineEditor) {
        if (QWidget::keyboardGrabber() == mInlineEditor)
            mInlineEditor->releaseKeyboard();
        mInlineEditor->hide();
    }
    mIsInlineEditing = false;
    mEditingLayerIndex = -1;
    if (mHasAppEventFilter && !mIsCellInlineEditing)
    {
        qApp->removeEventFilter(this);
        mHasAppEventFilter = false;
    }
    updateContent();
}

void TimeLineCells::startInlineCellEdit(int layerIndex, int frameNumber)
{
    if (mIsCellInlineEditing) return;
    if (mType != TIMELINE_CELL_TYPE::Tracks) return;
    if (layerIndex < 0 || layerIndex >= mEditor->object()->getLayerCount()) return;
    if (frameNumber < 1) return;

    Layer* baseLayer = mEditor->object()->getLayer(layerIndex);
    if (!baseLayer || baseLayer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(baseLayer);

    // Lazily create inline editor
    if (!mCellInlineEditor)
    {
        mCellInlineEditor = new QLineEdit(this);
        mCellInlineEditor->installEventFilter(this);
        mCellInlineEditor->setFrame(false);
        mCellInlineEditor->setAlignment(Qt::AlignCenter);
    }

    QString initialText;
    KeyFrame* kf = bitmapLayer->getKeyFrameAt(frameNumber);
    if (kf)
    {
        if (BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf))
        {
            if (bitmapLayer->cellBank()) {
                const QString rawName = bitmapLayer->cellBank()->getCellName(bkf->cellId());
                static const QRegularExpression trailNum(QStringLiteral("(\\d+)$"));
                const QRegularExpressionMatch m = trailNum.match(rawName);
                if (m.hasMatch())
                    initialText = QString::number(m.captured(1).toInt());
                else
                    initialText = QString::number(bkf->cellId());
            }
        }
    }
    mCellInlineEditor->setText(initialText);

    // Position editor over the clicked frame rectangle
    int y = getLayerY(layerIndex);
    int recHeight = mLayerHeight;
    int recWidth = mFrameSize;
    int recLeft = getFrameX(frameNumber - 1);
    QRect r(recLeft, y, recWidth, recHeight);
    mCellInlineEditor->setGeometry(r);
    mCellInlineEditor->show();
    mCellInlineEditor->raise();
    mCellInlineEditor->setFocus(Qt::OtherFocusReason);
    mCellInlineEditor->selectAll();
    QTimer::singleShot(0, mCellInlineEditor, [this]() {
        if (!mCellInlineEditor || !mIsCellInlineEditing) {
            return;
        }
        mCellInlineEditor->setFocus(Qt::OtherFocusReason);
        mCellInlineEditor->selectAll();
    });

    mCellEditingLayerIndex = layerIndex;
    mCellEditingFrame = frameNumber;
    mIsCellInlineEditing = true;

    // Install app-level event filter to auto-confirm on outside interaction
    if (!mHasAppEventFilter)
    {
        qApp->installEventFilter(this);
        mHasAppEventFilter = true;
    }
}

void TimeLineCells::finishInlineCellEdit(bool accept)
{
    if (!mIsCellInlineEditing) return;

    const int layerIndex = mCellEditingLayerIndex;
    const int frameNumber = mCellEditingFrame;

    if (accept && mCellInlineEditor)
    {
        bool ok = false;
        int typedIndex = mCellInlineEditor->text().trimmed().toInt(&ok);
        if (ok && typedIndex > 0 &&
            layerIndex >= 0 && layerIndex < mEditor->object()->getLayerCount())
        {
            Layer* baseLayer = mEditor->object()->getLayer(layerIndex);
            if (baseLayer && baseLayer->type() == Layer::BITMAP)
            {
                LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(baseLayer);
                CellBank* bank = bitmapLayer->cellBank();
                int typedId = bank ? bank->cellIdForIndex(typedIndex) : -1;
                if (bank && typedId > 0 && bank->getCell(typedId))
                {
                    // Determine existing keyframe at this exact frame
                    KeyFrame* existingKf = bitmapLayer->getKeyFrameAt(frameNumber);
                    BitmapKeyFrame* existingBkf = dynamic_cast<BitmapKeyFrame*>(existingKf);

                    // No-op if exposure already uses this cell
                    if (!(existingBkf && existingBkf->cellId() == typedId))
                    {
                        BitmapKeyFrame* newKf = new BitmapKeyFrame(typedId);
                        newKf->setPos(frameNumber);
                        newKf->setCellBank(bank);

                        if (existingKf)
                        {
                            // Replace exposure: remove old keyframe, then add new one
                            bitmapLayer->removeKeyFrame(frameNumber);
                            bitmapLayer->addKeyFrame(frameNumber, newKf);
                        }
                        else
                        {
                            // Empty frame: add new exposure
                            bitmapLayer->addKeyFrame(frameNumber, newKf);
                        }

                        // Mark document modified and refresh UI around this frame
                        mEditor->setModified(layerIndex, frameNumber);
                        mEditor->scrubTo(frameNumber);
                        mEditor->getScribbleArea()->update();
                    }
                }
            }
        }
    }

    if (mCellInlineEditor) mCellInlineEditor->hide();
    mIsCellInlineEditing = false;
    mCellEditingLayerIndex = -1;
    mCellEditingFrame = -1;

    if (mHasAppEventFilter && !mIsInlineEditing)
    {
        qApp->removeEventFilter(this);
        mHasAppEventFilter = false;
    }

    updateContent();
}

bool TimeLineCells::eventFilter(QObject* watched, QEvent* event)
{
    // 1) Handle keys and focus for inline editors themselves
    const bool renameActive = (watched == mInlineEditor && mIsInlineEditing);
    const bool cellEditActive = (watched == mCellInlineEditor && mIsCellInlineEditing);

    if (renameActive || cellEditActive)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            {
                if (renameActive) finishInlineRename(true);
                if (cellEditActive) finishInlineCellEdit(true);
                return true;
            }
            if (ke->key() == Qt::Key_Escape)
            {
                if (renameActive) finishInlineRename(false);
                if (cellEditActive) finishInlineCellEdit(false);
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut)
        {
            return false;
        }
    }
    // 2) While editing, confirm automatically when the user clicks elsewhere
    if ((mIsInlineEditing || mIsCellInlineEditing) &&
        watched != mInlineEditor && watched != mCellInlineEditor)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            const bool insideRename = (mInlineEditor && mInlineEditor->isVisible() &&
                                       mInlineEditor->rect().contains(mInlineEditor->mapFromGlobal(me->globalPos())));
            const bool insideCellEdit = (mCellInlineEditor && mCellInlineEditor->isVisible() &&
                                         mCellInlineEditor->rect().contains(mCellInlineEditor->mapFromGlobal(me->globalPos())));

            if (mIsInlineEditing && mType == TIMELINE_CELL_TYPE::Layers)
            {
                const QPoint local = mapFromGlobal(me->globalPos());
                const bool insideThis = rect().contains(local);
                if (insideThis)
                {
                    const int clickedLayer = getLayerNumber(local.y());
                    if (clickedLayer == mEditingLayerIndex)
                    {
                        if (!insideRename)
                        {
                            QTimer::singleShot(0, this, [this]() {
                                if (mInlineEditor && mIsInlineEditing) {
                                    mInlineEditor->setFocus(Qt::OtherFocusReason);
                                    mInlineEditor->setCursorPosition(mInlineEditor->text().size());
                                }
                            });
                        }
                        return false;
                    }

                    if (clickedLayer >= 0 && clickedLayer != mEditingLayerIndex)
                    {
                        finishInlineRename(true);
                        return false;
                    }
                }
                else
                {
                    finishInlineRename(true);
                    return false;
                }
            }

            // If click is not inside any inline editor, accept current edits
            if (!insideRename && !insideCellEdit)
            {
                if (mIsInlineEditing) finishInlineRename(true);
                if (mIsCellInlineEditing) finishInlineCellEdit(true);
                return false;
            }
        }
        if (event->type() == QEvent::TouchBegin)
        {
            if (mIsInlineEditing) finishInlineRename(true);
            if (mIsCellInlineEditing) finishInlineCellEdit(true);
            return true;
        }
        if (event->type() == QEvent::WindowDeactivate)
        {
            // Window lost activation and user likely switched widgets/apps: accept
            if (mIsInlineEditing) finishInlineRename(true);
            if (mIsCellInlineEditing) finishInlineCellEdit(true);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TimeLineCells::hScrollChange(int x)
{
    mFrameOffset = x;
    updateContent();
}

void TimeLineCells::vScrollChange(int x)
{
    mLayerOffset = x;
    mScrollingVertically = true;
    updateContent();
}

void TimeLineCells::onScrollingVerticallyStopped()
{
    mScrollingVertically = false;
}

void TimeLineCells::setMouseMoveY(int x)
{
    mMouseMoveY = x;
    updateContent();
}

bool TimeLineCells::trackScrubber()
{
    if (mType != TIMELINE_CELL_TYPE::Tracks ||
        (mPrevFrame == mEditor->currentFrame() && !mEditor->playback()->isPlaying()))
    {
        return false;
    }
    mPrevFrame = mEditor->currentFrame();

    if (mEditor->currentFrame() <= mFrameOffset)
    {
        // Move the timeline back if the scrubber is offscreen to the left
        mFrameOffset = mEditor->currentFrame() - 1;
        emit offsetChanged(mFrameOffset);
        return true;
    }
    else if (width() < (mEditor->currentFrame() - mFrameOffset + 1) * mFrameSize)
    {
        // Move timeline forward if the scrubber is offscreen to the right
        if (mEditor->playback()->isPlaying())
            mFrameOffset = mFrameOffset + ((mEditor->currentFrame() - mFrameOffset) / 2);
        else
            mFrameOffset = mEditor->currentFrame() - width() / mFrameSize;
        emit offsetChanged(mFrameOffset);
        return true;
    }
    return false;
}

void TimeLineCells::onDidLeaveWidget()
{
    // Reset last known frame pos to avoid wrong UI states when leaving the widget
    mFramePosMoveX = 0;
    update();
}

// Multi-Layer Selection Helpers
bool TimeLineCells::isDeepSelectMode(Qt::KeyboardModifiers modifiers) const
{
    return (modifiers & Qt::ControlModifier);
}

void TimeLineCells::deepSelectFrame(int frame, bool selected)
{
    Object* obj = mEditor->object();
    for (int i = 0; i < obj->getLayerCount(); ++i) {
        Layer* layer = obj->getLayer(i);
        if (layer && layer->keyExistsWhichCovers(frame)) {
            layer->setFrameSelected(frame, selected);
        }
    }
}

void TimeLineCells::deepSelectAllFramesAfter(int frame)
{
    Object* obj = mEditor->object();
    for (int i = 0; i < obj->getLayerCount(); ++i) {
        Layer* layer = obj->getLayer(i);
        if (layer) {
            layer->selectAllFramesAfter(frame);
        }
    }
}

void TimeLineCells::deepMoveSelectedFrames(int offset)
{
    Object* obj = mEditor->object();
    for (int i = 0; i < obj->getLayerCount(); ++i) {
        Layer* layer = obj->getLayer(i);
        if (layer) {
             layer->moveSelectedFrames(offset);
        }
    }
}

void TimeLineCells::deepDeselectAll()
{
    Object* obj = mEditor->object();
    for (int i = 0; i < obj->getLayerCount(); ++i) {
        Layer* layer = obj->getLayer(i);
        if (layer) {
            layer->deselectAll();
        }
    }
}


// ── Image drag-and-drop into timeline layer list ──────────────────────────────

static bool timelineMimeHasImages(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls()) return false;
    for (const QUrl& url : mime->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        QFileInfo fi(path);
        if (fi.isDir()) {
            for (const QFileInfo& f : QDir(path).entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot))
                if (QImageReader(f.absoluteFilePath()).canRead()) return true;
        } else if (QImageReader(path).canRead()) {
            return true;
        }
    }
    return false;
}

static QList<QString> timelineCollectImagePaths(const QList<QUrl>& urls)
{
    QList<QString> paths;
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        QFileInfo fi(path);
        if (fi.isDir()) {
            for (const QFileInfo& f : QDir(path).entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot))
                if (QImageReader(f.absoluteFilePath()).canRead()) paths << f.absoluteFilePath();
        } else if (QImageReader(path).canRead()) {
            paths << path;
        }
    }
    return paths;
}

int TimeLineCells::getImageDropSlot(int y) const
{
    const QVector<int> visible = getVisibleLayerIndices();
    const int n = visible.size();
    if (n <= 0) return 0;
    if (y < mOffsetY) return 0;
    if (y >= mOffsetY + n * mLayerHeight) return n;
    const int localY = y - mOffsetY;
    return qBound(0, (localY + mLayerHeight / 2) / mLayerHeight, n);
}

void TimeLineCells::handleImageDragEnter(QDragEnterEvent* e)
{
    if (mType != TIMELINE_CELL_TYPE::Layers) { e->ignore(); return; }
    if (!timelineMimeHasImages(e->mimeData())) { e->ignore(); return; }
    mImageDragActive = true;
    mImageDropSlot = getImageDropSlot(e->pos().y());
    update();
    e->acceptProposedAction();
}

void TimeLineCells::handleImageDragMove(QDragMoveEvent* e)
{
    if (!mImageDragActive) { e->ignore(); return; }
    const int newSlot = getImageDropSlot(e->pos().y());
    if (newSlot != mImageDropSlot) {
        mImageDropSlot = newSlot;
        update();
    }
    e->acceptProposedAction();
}

void TimeLineCells::importImagesAsLayers(const QList<QUrl>& urls, int insertSlot)
{
    if (!mEditor) return;
    const QList<QString> paths = timelineCollectImagePaths(urls);
    if (paths.isEmpty()) return;

    const QVector<int> visible = getVisibleLayerIndices();
    const int n = visible.size();
    const int slot = qBound(0, insertSlot, n);

    // Determine the actual layer index to insert above.
    // visible[0] = topmost (highest actual index), visible[n-1] = bottommost.
    // slot 0 = above topmost, slot n = below bottommost.
    int insertAboveActual;
    if (n == 0) {
        insertAboveActual = -1;
    } else if (slot == 0) {
        insertAboveActual = visible[0];
    } else {
        insertAboveActual = (slot < n) ? visible[slot] : -1;
    }

    const QSize canvasSize = mEditor->view()->canvasSize();
    const int frame = mEditor->currentFrame();

    for (int i = 0; i < paths.size(); ++i) {
        QImageReader reader(paths[i]);
        QImage img = reader.read();
        if (img.isNull()) continue;

        if (canvasSize.width() > 0 && canvasSize.height() > 0)
            img = img.scaled(canvasSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const QString layerName = QFileInfo(paths[i]).baseName();
        const QString uniqueName = mEditor->layers()->nameSuggestLayer(layerName);

        // createBitmapLayer creates the layer and adds a keyframe at position 1
        LayerBitmap* newLayer = mEditor->layers()->createBitmapLayer(uniqueName);
        if (!newLayer || !newLayer->cellBank()) continue;

        // Get the keyframe at position 1 (auto-created)
        auto* bkf = dynamic_cast<BitmapKeyFrame*>(newLayer->getKeyFrameAt(1));
        if (!bkf) continue;

        const int cid = bkf->cellId();
        CellLayerStack* st = newLayer->cellBank()->getCellLayerStack(cid);
        if (!st) continue;

        InnerLayer* il = st->activeLayer();
        if (!il || !il->image) continue;

        const QPoint topLeft(-img.width() / 2, -img.height() / 2);
        BitmapImage importedBmp(topLeft, img);
        il->image->paste(&importedBmp);
        st->invalidateComposite();

        // Move the new layer to the desired position
        // createBitmapLayer places it at the top (highest index)
        const int newLayerActual = mEditor->layers()->getIndex(newLayer);
        if (newLayerActual >= 0) {
            if (insertAboveActual >= 0) {
                // Move down to just above insertAboveActual
                int target = insertAboveActual + 1;
                if (target > newLayerActual) target = newLayerActual;
                for (int j = newLayerActual; j > target; --j) {
                    mEditor->swapLayers(j, j - 1);
                }
                insertAboveActual++;
            } else if (slot == n && n > 0) {
                // Bottom slot: move all the way down to index 0
                for (int j = newLayerActual; j > 0; --j) {
                    mEditor->swapLayers(j, j - 1);
                }
                // insertAboveActual stays -1 but next image should go just above index 0
                insertAboveActual = 1; // next image lands above the one we just placed at 0
            }
        }

        mEditor->setModified(mEditor->layers()->getIndex(newLayer), frame);
    }

    mEditor->updateUndoContext();
    updateContent();
}

void TimeLineCells::handleImageDrop(QDropEvent* e)
{
    mImageDragActive = false;
    mImageDropSlot = -1;
    update();

    if (mType != TIMELINE_CELL_TYPE::Layers) { e->ignore(); return; }
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }
    if (!timelineMimeHasImages(e->mimeData())) { e->ignore(); return; }

    const int slot = getImageDropSlot(e->pos().y());
    importImagesAsLayers(e->mimeData()->urls(), slot);
    e->acceptProposedAction();
}

void TimeLineCells::dragEnterEvent(QDragEnterEvent* e)
{
    handleImageDragEnter(e);
}

void TimeLineCells::dragMoveEvent(QDragMoveEvent* e)
{
    handleImageDragMove(e);
}

void TimeLineCells::dragLeaveEvent(QDragLeaveEvent*)
{
    mImageDragActive = false;
    mImageDropSlot = -1;
    update();
}

void TimeLineCells::dropEvent(QDropEvent* e)
{
    handleImageDrop(e);
}
