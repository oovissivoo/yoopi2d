#include "brushpresetlistwidget.h"
#include "cosmicbrushsettings.h"

#include "managers/brushpresetstore.h"
#include "managers/brushassetstore.h"

#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QSettings>
#include <QIcon>
#include <QCursor>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QHash>
#include <QScrollBar>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>
#include <QDebug>

#include "pencildef.h"
#include "pencilsettings.h"
#include "penciltool.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "../CUI/cosmicuimetrics.h"

// BrushPresetItem implementation
namespace {
static constexpr int kBrushPresetRowHeightPx = 56;
}

static quint64 brushPresetPreviewKey(const ToolOptionWidget::BrushPreset& p)
{
    QByteArray b;
    b.reserve(256);
    b.append(p.id.toUtf8());
    b.append('|');
    b.append(QByteArray::number(p.spacingMult, 'f', 6));
    b.append('|');
    b.append(QByteArray::number(p.scatter, 'f', 6));
    b.append('|');
    b.append(QByteArray::number(p.rotationDeg, 'f', 6));
    b.append('|');
    b.append(QByteArray::number(p.sizeJitter, 'f', 6));
    b.append('|');
    b.append(QByteArray::number(p.colorJitter, 'f', 6));
    b.append('|');
    b.append(QByteArray::number(p.textureTileScale, 'f', 6));
    b.append('|');
    b.append(QByteArray::number(p.textureContrastPercent));
    b.append('|');
    b.append(p.thicknessCurveEnabled ? '1' : '0');
    b.append('|');
    b.append(p.opacityCurveEnabled ? '1' : '0');
    b.append('|');
    for (const QPointF& pt : p.thicknessCurvePoints) {
        b.append(QByteArray::number(pt.x(), 'f', 6));
        b.append(',');
        b.append(QByteArray::number(pt.y(), 'f', 6));
        b.append(';');
    }
    b.append('|');
    for (const QPointF& pt : p.opacityCurvePoints) {
        b.append(QByteArray::number(pt.x(), 'f', 6));
        b.append(',');
        b.append(QByteArray::number(pt.y(), 'f', 6));
        b.append(';');
    }
    b.append('|');
    b.append(p.textureAssetId.toUtf8());
    b.append('|');
    b.append(QByteArray::number(static_cast<int>(p.tipMode)));
    b.append('|');
    b.append(p.tipAssetId.toUtf8());
    b.append('|');
    b.append(QByteArray::number(p.tipContrastPercent));
    return quint64(qHash(b));
}

BrushPresetItem::BrushPresetItem(const ToolOptionWidget::BrushPreset& preset, int index, QWidget* parent)
    : QWidget(parent)
    , mName(preset.name)
    , mIndex(index)
    , mBuiltIn(preset.builtIn)
    , mPreset(preset)
{
    setFixedHeight(kBrushPresetRowHeightPx);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    updateLayout();
}

void BrushPresetItem::setSlideOffset(qreal v)
{
    if (mFinalLineMode) {
        if (!qFuzzyCompare(mSlideOffset + 1.0, 1.0)) {
            mSlideOffset = 0.0;
            update();
        }
        return;
    }
    const qreal actionsW = actionsWidth();
    const qreal clamped = qBound(-actionsW, v, 0.0);
    if (!qFuzzyCompare(mSlideOffset + 1.0, clamped + 1.0)) {
        mSlideOffset = clamped;
        update();
    }
}

int BrushPresetItem::actionsWidth() const
{
    // Keep in sync with actionsRect()
    const int btnW = 34;
    return btnW * 3;
}

void BrushPresetItem::startInlineRename()
{
    if (mBuiltIn || mRenameMode) {
        return;
    }
    if (!mRenameEdit) {
        mRenameEdit = new QLineEdit(this);
        mRenameEdit->setFrame(false);
        mRenameEdit->installEventFilter(this);
        mRenameEdit->setStyleSheet(CosmicInlineEdit::stylesheet());
        QFont f = mRenameEdit->font();
        f.setPixelSize(12);
        mRenameEdit->setFont(f);
        mRenameEdit->setTextMargins(0, 0, 0, 0);
    }
    mRenameEdit->setText(mName);
    mRenameEdit->selectAll();
    mRenameEdit->setGeometry(mTextRect);
    mRenameEdit->show();
    mRenameEdit->setFocus();
    mRenameMode = true;
    update();
}

void BrushPresetItem::cancelInlineRename()
{
    if (!mRenameEdit || !mRenameMode) {
        return;
    }
    mRenameEdit->hide();
    mRenameMode = false;
    update();
}

bool BrushPresetItem::isRenaming() const
{
    return mRenameEdit && mRenameEdit->isVisible();
}

void BrushPresetItem::setName(const QString& name)
{
    if (mName != name) {
        mName = name;
        update();
    }
}

void BrushPresetItem::setPreset(const ToolOptionWidget::BrushPreset& preset)
{
    mPreset = preset;
    mBuiltIn = preset.builtIn;
    setName(preset.name);
    update();
}

void BrushPresetItem::setActive(bool active)
{
    if (mIsActive != active) {
        mIsActive = active;
        update();
    }
}

void BrushPresetItem::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect rect = this->rect();

    // Actions area (drawn behind; revealed by reducing visible content width)
    if (!mFinalLineMode) {
        const QRect aRect = actionsRect();
        const QRect delRect = deleteRect();
        const QRect dupRect = duplicateRect();
        const QRect clsRect = closeRect();

        const bool deleteEnabled = !mBuiltIn;
        p.fillRect(delRect, deleteEnabled ? QColor(0xd6, 0x2f, 0x2f) : QColor(0x4a, 0x4a, 0x4a));
        p.fillRect(dupRect, QColor(0x2b, 0x2b, 0x2b));
        p.fillRect(clsRect, QColor(0x2b, 0x2b, 0x2b));

        const auto drawActionIcon = [&p](const QRect& r, const QString& path, qreal alpha) {
            const QSize s(qRound(r.width() * 0.55), qRound(r.height() * 0.55));
            QPixmap pm = QIcon(path).pixmap(s);
            if (pm.isNull()) {
                return;
            }
            QPainter ip(&pm);
            ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
            QColor c = Qt::white;
            c.setAlphaF(alpha);
            ip.fillRect(pm.rect(), c);
            ip.end();
            p.drawPixmap(r.center() - pm.rect().center(), pm);
        };

        drawActionIcon(delRect, QStringLiteral(":/Interaction/trash.svg"), deleteEnabled ? 1.0 : 0.35);
        drawActionIcon(dupRect, QStringLiteral(":/Interaction/Duplicate.svg"), 1.0);
        drawActionIcon(clsRect, QStringLiteral(":/Interaction/CloseWindow.svg"), 1.0);

        // Subtle separator at left edge of action area
        p.setPen(QColor(0x1a, 0x1a, 0x1a));
        p.drawLine(aRect.left(), rect.top(), aRect.left(), rect.bottom());
    }

    // Keep icon + name fixed; reveal actions by shrinking/clipping from the right.
    // `mSlideOffset` is in [-actionsWidth, 0].
    const int revealW = mFinalLineMode ? 0 : qRound(-mSlideOffset);
    const int contentW = qMax(0, rect.width() - revealW);
    p.save();
    p.setClipRect(QRect(rect.left(), rect.top(), contentW, rect.height()));

    // Background
    if (mIsActive) {
        // Use accent color for active preset
        QColor accentColor = BlenderStyleSlider::getAccentColor();
        p.fillRect(rect, accentColor);
        p.setPen(Qt::white);
    } else {
        // Always paint an opaque base background so the actions area isn't visible
        // unless the row is actually slid open.
        p.fillRect(rect, QColor(0x2b, 0x2b, 0x2b));

        // Hover effect for non-active items
        if (underMouse()) {
            p.fillRect(rect, QColor(0x35, 0x35, 0x35));
        }
        p.setPen(QColor(0xb2, 0xb2, 0xb2));
    }

    // Icon area (left)
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    {
        const QRect iconRect = mIconRect;
        const QSize iconSize = (iconRect.size() * 0.75);
        QPixmap scaled = QIcon(QStringLiteral(":/Interaction/TabBrushPresets.svg")).pixmap(iconSize);
        if (!scaled.isNull()) {
            QPainter iconPainter(&scaled);
            iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            iconPainter.fillRect(scaled.rect(), mIsActive ? Qt::white : QColor(0xb2, 0xb2, 0xb2));
            iconPainter.end();
            p.drawPixmap(iconRect.center() - scaled.rect().center(), scaled);
        }
    }

    // Preset name (skip if in rename mode)
    if (!mRenameMode) {
        p.setPen(mIsActive ? Qt::white : QColor(0xb2, 0xb2, 0xb2));
        QFont font = p.font();
        font.setPixelSize(12);
        p.setFont(font);
        QFontMetrics fm(font);
        const QRect textRect = mTextRect;
        QString elidedName = fm.elidedText(mName, Qt::ElideRight, textRect.width());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
    }

    // Preview stroke
    {
        const QRect pr = mPreviewRect;
        if (pr.width() > 4 && pr.height() > 4) {
            const qreal dpr = devicePixelRatioF();
            const QSize pixSz(qMax(1, qRound(pr.width() * dpr)), qMax(1, qRound(pr.height() * dpr)));
            static constexpr quint64 kFinalLineKeySalt = 0xF1A11A11EULL;
            const quint64 presetKey = brushPresetPreviewKey(mPreset) ^ (mFinalLineMode ? kFinalLineKeySalt : 0ULL);
            const quint64 sizeKey = (quint64(pixSz.width()) << 32) ^ quint64(pixSz.height());
            // Renderer version salt: bump this when preview rendering logic changes so
            // cached pixmaps are invalidated deterministically.
            static constexpr quint64 kPreviewRendererVersionSalt = 0x6D5BDBDE4D9E2C32ULL;
            const quint64 key = presetKey ^ sizeKey ^ kPreviewRendererVersionSalt;

            static QHash<quint64, QPixmap> cache;
            auto it = cache.constFind(key);
            QPixmap pm;
            if (it != cache.constEnd()) {
                pm = it.value();
            } else {
                QImage strokeImg;
                if (mFinalLineMode) {
                    // Pixelated (non-antialiased) preview for Final Line.
                    strokeImg = QImage(pixSz, QImage::Format_ARGB32_Premultiplied);
                    strokeImg.fill(Qt::transparent);
                    QPainter ip(&strokeImg);
                    ip.setRenderHint(QPainter::Antialiasing, false);
                    ip.setRenderHint(QPainter::SmoothPixmapTransform, false);
                    ip.setCompositionMode(QPainter::CompositionMode_Source);

                    const int w = strokeImg.width();
                    const int h = strokeImg.height();
                    const int pad = qMax(1, qRound(2 * dpr));

                    // Match the normal preset preview principles:
                    // - Same overall S-curve flow
                    // - Symmetric tip taper
                    // - Deterministic spacing/segment count
                    // But draw with AA disabled and quantize points to integer pixels.
                    const qreal padX = qMax<qreal>(2.0, w * 0.06);
                    const qreal x0 = padX;
                    const qreal x1 = qMax<qreal>(x0 + 1.0, w - padX);
                    const qreal midY = h * 0.52;
                    const qreal curveAmp = h * 0.16;

                    const qreal maxW = qMax<qreal>(2.0 * dpr, h * 0.22);
                    const qreal minW = qMax<qreal>(1.0, maxW * 0.12);
                    const qreal approxSpacing = qMax<qreal>(0.8, maxW * 0.45);
                    const int steps = qBound(40, int(qCeil((x1 - x0) / approxSpacing)), 260);

                    QPoint prevPt;
                    bool hasPrev = false;
                    for (int i = 0; i < steps; ++i) {
                        const qreal t = qreal(i) / qMax(1, steps - 1);
                        const qreal tri = (t < 0.5) ? (t * 2.0) : (2.0 - 2.0 * t);
                        const qreal tipTaper = qBound<qreal>(0.0, tri, 1.0);

                        const qreal x = x0 + t * (x1 - x0);
                        const qreal y = midY + curveAmp * qSin(t * 3.14159265 * 2.0);
                        const QPoint curPt(int(qRound(x)), int(qRound(y)));
                        if (!hasPrev) {
                            prevPt = curPt;
                            hasPrev = true;
                            continue;
                        }

                        qreal width = maxW;
                        width *= (0.10 + 0.90 * tipTaper);
                        width = qBound<qreal>(minW, width, maxW);
                        const int iw = qMax(1, int(qRound(width)));

                        QPen pen(QColor(0xff, 0xff, 0xff));
                        pen.setWidth(iw);
                        pen.setCapStyle(Qt::SquareCap);
                        pen.setJoinStyle(Qt::MiterJoin);
                        ip.setPen(pen);

                        if (prevPt != curPt) {
                            ip.drawLine(prevPt, curPt);
                            prevPt = curPt;
                        }
                    }

                    // Fill color (solid black). We draw black directly for maximum
                    // pixel crispness.
                    ip.end();
                } else {
                    PencilTool::PreviewPreset pp;
                    pp.spacingMult = mPreset.spacingMult;
                    pp.scatter = mPreset.scatter;
                    pp.rotationDeg = mPreset.rotationDeg;
                    pp.sizeJitter = mPreset.sizeJitter;
                    pp.colorJitter = mPreset.colorJitter;
                    pp.textureTileScale = mPreset.textureTileScale;
                    pp.textureContrastPercent = mPreset.textureContrastPercent;
                    pp.textureAssetId = mPreset.textureAssetId;
                    pp.textureBytes   = BrushAssetStore::instance()->loadTexture(mPreset.textureAssetId);
                    // Tip
                    using BPTipMode = ToolOptionWidget::BrushPreset::TipMode;
                    using PPTipMode = PencilTool::PreviewPreset::TipMode;
                    pp.tipMode = (mPreset.tipMode == BPTipMode::Soft)   ? PPTipMode::Soft
                               : (mPreset.tipMode == BPTipMode::Custom) ? PPTipMode::Custom
                                                                         : PPTipMode::Hard;
                    if (mPreset.tipMode == BPTipMode::Custom && !mPreset.tipAssetId.isEmpty())
                        pp.tipBytes = BrushAssetStore::instance()->loadTip(mPreset.tipAssetId);
                    pp.tipContrastPercent = mPreset.tipContrastPercent;
                    pp.thicknessCurvePoints = mPreset.thicknessCurvePoints;
                    pp.opacityCurvePoints = mPreset.opacityCurvePoints;
                    pp.thicknessCurveEnabled = mPreset.thicknessCurveEnabled;
                    pp.opacityCurveEnabled = mPreset.opacityCurveEnabled;

                    strokeImg = PencilTool::renderPresetPreviewStroke(pp, pixSz, quint32(key));
                }

                pm = QPixmap::fromImage(strokeImg);
                pm.setDevicePixelRatio(dpr);
                if (cache.size() > 256) {
                    cache.clear();
                }
                cache.insert(key, pm);
            }

            if (!pm.isNull()) {
                p.drawPixmap(pr.topLeft(), pm, QRect(QPoint(0, 0), pm.size() / pm.devicePixelRatio()));
            }
        }
    }

    // Settings button (right) — shown for all presets including Final Line
    {
        const QRect settingsRect = mSettingsRect;
        if (!settingsRect.isNull()) {
            const QSize settingsSize = (settingsRect.size() * 1.0);
            QPixmap scaled = QIcon(QStringLiteral(":/Interaction/settings.svg")).pixmap(settingsSize);
            if (!scaled.isNull()) {
                QPainter settingsPainter(&scaled);
                settingsPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                settingsPainter.fillRect(scaled.rect(), mIsActive ? Qt::white : QColor(0xb2, 0xb2, 0xb2));
                settingsPainter.end();
                p.drawPixmap(settingsRect.center() - scaled.rect().center(), scaled);
            }
        }
    }

    // Separator line at bottom (drawn below the background fill, not overlapping)
    p.setPen(QColor(0x1a, 0x1a, 0x1a)); // Very dark grey
    p.drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());

    p.restore();
}

void BrushPresetItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mPressConsumedByAction = false;
        mPressPos = event->pos();
        mDragStartPos = event->pos();

        if (mFinalLineMode) {
            // Final Line is a pinned tool-switch row: no swipe actions.
            mSwipeTracking = true;
            mSwipeDecided = false;
            mSwipeIsHorizontal = false;
            mDragStarted = false;
            mSlideOffsetAtPress = 0.0;
            QWidget::mousePressEvent(event);
            return;
        }

        mSwipeTracking = true;
        mSwipeDecided = false;
        mSwipeIsHorizontal = false;
        mDragStarted = false;
        mSlideOffsetAtPress = mSlideOffset;

        // If actions are visible, allow clicking them.
        if (mSlideOffset < 0.0) {
            if (deleteRect().contains(event->pos())) {
                if (!mBuiltIn) {
                    mPressConsumedByAction = true;
                    emit deleteRequested(mIndex);
                    event->accept();
                    return;
                }
            }
            if (duplicateRect().contains(event->pos())) {
                mPressConsumedByAction = true;
                emit duplicateRequested(mIndex);
                event->accept();
                return;
            }
            if (closeRect().contains(event->pos())) {
                mPressConsumedByAction = true;
                emit slideClosed(mIndex);
                setSlideOpen(false, true);
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void BrushPresetItem::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        if (mFinalLineMode) {
            return;
        }
        const QPoint delta = event->pos() - mPressPos;
        const int dx = delta.x();
        const int dy = delta.y();

        // Use a generous threshold to avoid triggering on pen/tablet micro-jitter.
        // QApplication::startDragDistance() is typically 10px on desktop but we
        // clamp to at least 8px so even low-DPI setups feel intentional.
        const int dragThresh = qMax(QApplication::startDragDistance(), 8);

        if (!mSwipeDecided) {
            if (qAbs(dx) > dragThresh || qAbs(dy) > dragThresh) {
                mSwipeDecided = true;
                mSwipeIsHorizontal = (qAbs(dx) > qAbs(dy) * 2);
            }
        }

        if (mSwipeDecided && mSwipeIsHorizontal) {
            // Swipe left reveals actions (negative offset)
            setSlideOffset(mSlideOffsetAtPress + dx);
            event->accept();
            return;
        }

        // Only start a vertical reorder drag once the threshold is clearly exceeded
        // and the gesture is confirmed as non-horizontal. This prevents accidental
        // reorders from pen lift/tap micro-movements.
        if (mSwipeDecided && !mSwipeIsHorizontal && qAbs(dy) > dragThresh && !mDragStarted) {
            mDragStarted = true;
            emit dragStarted(mIndex);
        }
    }
}

void BrushPresetItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (mRenameMode) {
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && mSwipeTracking) {
        mSwipeTracking = false;

        if (mFinalLineMode) {
            QRect settingsRect = settingsButtonRect().translated(qRound(mSlideOffset), 0);
            if (settingsRect.contains(event->pos())) {
                emit settingsRequested(mIndex);
            } else {
                emit selected(mIndex);
            }
            event->accept();
            return;
        }

        // If the press already triggered one of the swipe action buttons, don't allow
        // the release to also count as a tap on the (now moved) settings button.
        if (mPressConsumedByAction) {
            mPressConsumedByAction = false;
            event->accept();
            return;
        }

        if (mSwipeDecided && mSwipeIsHorizontal) {
            const qreal openThreshold = -actionsRect().width() * 0.35;
            const bool shouldOpen = (mSlideOffset <= openThreshold);
            setSlideOpen(shouldOpen, true);
            emit slideStateRequested(mIndex, shouldOpen);
            event->accept();
            return;
        }

        // Plain tap — no gesture was decided. Select and optionally open settings.
        if (!mSwipeDecided || !mSwipeIsHorizontal) {
            emit selected(mIndex);
            QRect settingsRect = settingsButtonRect().translated(qRound(mSlideOffset), 0);
            if (settingsRect.contains(event->pos())) {
                emit settingsRequested(mIndex);
            }
            event->accept();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void BrushPresetItem::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    updateLayout();
    if (mRenameEdit && mRenameMode) {
        mRenameEdit->setGeometry(mTextRect);
    }
}

void BrushPresetItem::updateLayout()
{
    const QRect rect = this->rect();
    const int padding = 8;
    const int iconSize = 20;
    const int settingsSize = 15;
    const int headerH = 18;

    mIconRect = QRect(padding, (rect.height() - iconSize) / 2, iconSize, iconSize);

    const int settingsTop = (rect.height() - settingsSize) / 2;
    mSettingsRect = QRect(rect.right() - padding - settingsSize,
                          settingsTop,
                          settingsSize, settingsSize);

    const int contentLeft = mIconRect.right() + padding;
    const int contentRight = mSettingsRect.left() - padding;

    mTextRect = QRect(contentLeft, 0, qMax(0, contentRight - contentLeft), headerH);

    const int previewTop = headerH;
    const int previewBottom = rect.bottom();
    const int previewH = qMax(0, previewBottom - previewTop);
    mPreviewRect = QRect(contentLeft, previewTop, qMax(0, contentRight - contentLeft), previewH);
}

QRect BrushPresetItem::settingsButtonRect() const
{
    return mSettingsRect;
}

QRect BrushPresetItem::previewRect() const
{
    return mPreviewRect;
}

QRect BrushPresetItem::actionsRect() const
{
    const int btnW = 34;
    return QRect(rect().right() - (btnW * 3) + 1, rect().top(), btnW * 3, rect().height());
}

QRect BrushPresetItem::deleteRect() const
{
    const QRect a = actionsRect();
    const int btnW = a.width() / 3;
    return QRect(a.left(), a.top(), btnW, a.height());
}

QRect BrushPresetItem::duplicateRect() const
{
    const QRect a = actionsRect();
    const int btnW = a.width() / 3;
    return QRect(a.left() + btnW, a.top(), btnW, a.height());
}

QRect BrushPresetItem::closeRect() const
{
    const QRect a = actionsRect();
    const int btnW = a.width() / 3;
    return QRect(a.left() + (2 * btnW), a.top(), btnW, a.height());
}

void BrushPresetItem::setSlideOpen(bool open, bool animated)
{
    const qreal target = open ? -actionsWidth() : 0.0;
    if (!animated) {
        setSlideOffset(target);
        return;
    }

    auto* anim = new QPropertyAnimation(this, "slideOffset", this);
    anim->setDuration(140);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(mSlideOffset);
    anim->setEndValue(target);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// BrushPresetListWidget implementation
BrushPresetListWidget::BrushPresetListWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // External slider (always visible) matching the panes stack slider style.
    mScrollSlider = new QScrollBar(Qt::Vertical, this);
    mScrollSlider->setObjectName(QStringLiteral("BrushPresetScrollSlider"));
    mScrollSlider->setFixedWidth(12);
    mScrollSlider->setStyleSheet(QStringLiteral(
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
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"));
    mScrollSlider->setRange(0, 0);
    mScrollSlider->setValue(0);
    mScrollSlider->setVisible(false);
    layout->addWidget(mScrollSlider, 0);

    mScrollArea = new QScrollArea(this);
    mScrollArea->setFrameShape(QFrame::NoFrame);
    mScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mScrollArea->setWidgetResizable(true);
    mScrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
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

    mContainerWidget = new QWidget();
    mContainerLayout = new QVBoxLayout(mContainerWidget);
    mContainerLayout->setContentsMargins(0, 0, 0, 0);
    mContainerLayout->setSpacing(0);

    // Keep the container transparent so the empty area below presets inherits
    // the parent panel background instead of showing a mismatched color.
    mContainerWidget->setAutoFillBackground(false);
    mContainerWidget->setAttribute(Qt::WA_NoSystemBackground, true);
    mContainerWidget->setStyleSheet(QStringLiteral("background: transparent;"));

    // Viewport must also be transparent for the same reason.
    // Also install an event filter so we can paint the drag drop-indicator on top.
    if (QWidget* vp = mScrollArea->viewport()) {
        vp->setAutoFillBackground(false);
        vp->setAttribute(Qt::WA_NoSystemBackground, true);
        vp->setStyleSheet(QStringLiteral("background: transparent;"));
    }

    mScrollArea->setWidget(mContainerWidget);

    // Transparent overlay that draws the accent drop-indicator line during drag.
    // It sits as a child of the viewport so it's always on top of the list items.
    if (QWidget* vp = mScrollArea->viewport()) {
        mDropIndicator = new QWidget(vp);
        mDropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
        mDropIndicator->setAttribute(Qt::WA_NoSystemBackground);
        mDropIndicator->setAutoFillBackground(false);
        mDropIndicator->hide();
        // Will be resized/repositioned in updateDrag.
    }

    layout->addWidget(mScrollArea, 1);

    if (auto* sb = mScrollArea->verticalScrollBar()) {
        connect(sb, &QScrollBar::rangeChanged, this, [this](int minV, int maxV) {
            if (mScrollSlider) {
                mScrollSlider->setRange(minV, maxV);
                mScrollSlider->setVisible(maxV > minV);
            }
        });
        connect(sb, &QScrollBar::valueChanged, this, [this](int v) {
            if (mScrollSlider) {
                QSignalBlocker b(mScrollSlider);
                mScrollSlider->setValue(v);
            }
        });
    }
    connect(mScrollSlider, &QScrollBar::valueChanged, this, [this](int v) {
        if (mScrollArea && mScrollArea->verticalScrollBar()) {
            mScrollArea->verticalScrollBar()->setValue(v);
        }
    });

    mButtonGroup = new QButtonGroup(this);
    mButtonGroup->setExclusive(true);

    auto* store = BrushPresetStore::instance();
    store->ensureLoaded();

    auto applyFromStore = [this, store]() {
        const QVector<ToolOptionWidget::BrushPreset> presets = store->presets();
        const QString activeId = store->activePresetId();
        mPresets = presets;

        // Resolve active index BEFORE updateUI() so the correct item is
        // highlighted on the first build (updateUI reads mActivePresetIndex).
        int activeIndex = 0;
        for (int i = 0; i < mPresets.size(); ++i) {
            if (mPresets[i].id == activeId) {
                activeIndex = i;
                break;
            }
        }
        mActivePresetIndex = activeIndex;

        updateUI();
        setActivePreset(activeIndex);

        // Store-driven active preset implies pencil-mode (not Final Line).
        setFinalLineActive(false);

        mShowActionsForActive = false;
        applyActiveSlideState();
    };

    connect(store, &BrushPresetStore::presetsUpdated, this,
            [this, applyFromStore](const QVector<ToolOptionWidget::BrushPreset>&, const QString&) {
                applyFromStore();
            });

    applyFromStore();

    // Restore Final Line active state from the previous session.
    // This must happen AFTER applyFromStore() so it overrides the default setFinalLineActive(false).
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.beginGroup("PencilBrushFactory");
        const bool wasActive = settings.value("FinalLineActive", false).toBool();
        settings.endGroup();
        if (wasActive) {
            setFinalLineActive(true);
        }
    }

    // Settings popup is created lazily on first use (see ensureSettingsPopup())
    // so it can be parented to the top-level window rather than this embedded widget.

    // Never show swipe actions on launch; user must explicitly swipe.
    mShowActionsForActive = false;
    applyActiveSlideState();
}

BrushPresetListWidget::~BrushPresetListWidget()
{
    // Popup is parented to the top-level window, so we must delete it explicitly
    // here to avoid it outliving this widget and referencing our destroyed children.
    delete mSettingsPopup;
    mSettingsPopup = nullptr;
}

void BrushPresetListWidget::setFinalLineActive(bool active)
{
    mFinalLineActive = active;
    if (mFinalLineItem) {
        mFinalLineItem->setActive(active);
        mFinalLineItem->setSlideOffset(0.0);
    }

    if (active) {
        // Deactivate all scrollable presets.
        for (BrushPresetItem* it : mPresetItems) {
            if (it) {
                it->setActive(false);
                it->setSlideOffset(0.0);
            }
        }
        mShowActionsForActive = false;
    } else {
        // Restore highlight to active preset.
        for (int i = 0; i < mPresetItems.size(); ++i) {
            if (mPresetItems[i]) {
                mPresetItems[i]->setActive(i == mActivePresetIndex);
            }
        }
    }

    // Keep the settings popup in sync if it's open.
    updateSettingsPopupForActivePreset();
}

void BrushPresetListWidget::ensureSettingsPopup()
{
    if (mSettingsPopup) {
        return;
    }
    // Parent to the top-level window so the popup is not clipped or constrained
    // by this embedded panel widget. Using window() here (not 'this') is critical:
    // a QDialog parented to an embedded widget gets confined to that widget's geometry,
    // which breaks close button clicks, resize hit-testing, and all non-drag interaction.
    QWidget* topLevel = window();
    mSettingsPopup = new CosmicBrushSettings(-1, topLevel ? topLevel : nullptr);
    mSettingsPopup->hide();

    // Forward Final Line pressure changes so mainwindow2 can connect to the widget
    // signal rather than the lazily-created popup signal.
    connect(mSettingsPopup, &CosmicBrushSettings::finalLinePressureChanged,
            this, &BrushPresetListWidget::finalLinePressureChanged);
}

void BrushPresetListWidget::updateSettingsPopupForActivePreset()
{
    if (!mSettingsPopup || !mSettingsPopup->isVisible()) {
        return;
    }

    if (mFinalLineActive) {
        mSettingsPopup->setFinalLineMode(true);
        mSettingsPopup->setPresetId(QStringLiteral("__final_line__"));
        mSettingsPopup->setPresetName(tr("Pen"));
        return;
    }

    if (mActivePresetIndex < 0 || mActivePresetIndex >= mPresets.size()) {
        mSettingsPopup->hide();
        return;
    }

    const auto& preset = mPresets[mActivePresetIndex];
    mSettingsPopup->setFinalLineMode(false);
    mSettingsPopup->setPresetId(preset.id);
    mSettingsPopup->setPresetName(preset.name);
}

void BrushPresetListWidget::setPresets(const QVector<ToolOptionWidget::BrushPreset>& presets)
{
    mPresets = presets;
    updateUI();
}

void BrushPresetListWidget::setActivePreset(int index)
{
    if (mActivePresetIndex != index && index >= 0 && index < mPresetItems.size()) {
        mActivePresetIndex = index;
        
        // Update active state for all items
        for (int i = 0; i < mPresetItems.size(); ++i) {
            if (mPresetItems[i]) {
                mPresetItems[i]->setActive(i == index);
            }
        }
        
        // Scroll to active item
        if (index < mPresetItems.size() && mPresetItems[index]) {
            mScrollArea->ensureWidgetVisible(mPresetItems[index]);
        }

        // If the settings popup is open, keep it synced to the active preset.
        updateSettingsPopupForActivePreset();
    }
}

void BrushPresetListWidget::onPresetSelected(int index)
{
    // Selecting a normal preset disables Final Line.
    setFinalLineActive(false);
    setActivePreset(index);
    if (index >= 0 && index < mPresets.size()) {
        BrushPresetStore::instance()->setActivePresetId(mPresets[index].id);
    }
    applyActiveSlideState();

    emit presetSelected(index);
}

void BrushPresetListWidget::onPresetSettingsRequested(int index)
{
    if (index < 0 || index >= mPresets.size()) {
        return;
    }

    // Full selection — UI, store, and upstream.
    setFinalLineActive(false);
    setActivePreset(index);
    BrushPresetStore::instance()->setActivePresetId(mPresets[index].id);
    emit presetSelected(index);
    
    // Configure and show the settings popup
    ensureSettingsPopup();
    mSettingsPopup->setFinalLineMode(false);
    mSettingsPopup->setPresetId(mPresets[index].id);
    mSettingsPopup->setPresetName(mPresets[index].name);
    
    // Position the popup near the settings button
    if (index < mPresetItems.size() && mPresetItems[index]) {
        QWidget* presetItem = mPresetItems[index];
        QPoint itemGlobalPos = presetItem->mapToGlobal(presetItem->rect().topRight());
        
        // Position popup to the right of the preset item
        mSettingsPopup->move(itemGlobalPos + QPoint(10, 0));
    }
    
    mSettingsPopup->show();
    mSettingsPopup->raise();
    mSettingsPopup->activateWindow();

    // Ensure content reflects the current active preset.
    updateSettingsPopupForActivePreset();
    
    applyActiveSlideState();
}

void BrushPresetListWidget::onPresetDeleteRequested(int index)
{
    if (index < 0 || index >= mPresets.size()) {
        return;
    }

    if (mPresets[index].builtIn) {
        return;
    }

    BrushPresetStore::instance()->deletePreset(mPresets[index].id);
}

void BrushPresetListWidget::onPresetDuplicateRequested(int index)
{
    if (index < 0 || index >= mPresets.size()) {
        return;
    }
    BrushPresetStore::instance()->duplicatePreset(mPresets[index].id);
}

void BrushPresetListWidget::onPresetRenameRequested(int index, const QString& newName)
{
    if (index < 0 || index >= mPresets.size()) {
        return;
    }
    if (mPresets[index].builtIn) {
        return;
    }
    if (newName.isEmpty()) {
        return;
    }

    BrushPresetStore::instance()->renamePreset(mPresets[index].id, newName);
}

void BrushPresetListWidget::onPresetSlideClosed(int index)
{
    if (index < 0 || index >= mPresetItems.size()) {
        return;
    }
    if (mPresetItems[index]) {
        mPresetItems[index]->setSlideOffset(0.0);
    }

    if (index == mActivePresetIndex) {
        mShowActionsForActive = false;
    }

    applyActiveSlideState();
}

void BrushPresetListWidget::onPresetSlideStateRequested(int index, bool open)
{
    // Opening the swipe actions on a preset makes it the active one —
    // both visually and in the store so the brush engine actually switches.
    if (open) {
        setFinalLineActive(false);
        setActivePreset(index);
        if (index >= 0 && index < mPresets.size()) {
            BrushPresetStore::instance()->setActivePresetId(mPresets[index].id);
        }
        emit presetSelected(index);
    }
    mShowActionsForActive = open;
    applyActiveSlideState();
}

void BrushPresetListWidget::applyActiveSlideState()
{
    // Clamp state to current model bounds.
    if (mPresets.isEmpty()) {
        mShowActionsForActive = false;
        return;
    }
    mActivePresetIndex = qBound(0, mActivePresetIndex, mPresets.size() - 1);

    for (int i = 0; i < mPresetItems.size(); ++i) {
        BrushPresetItem* it = mPresetItems[i];
        if (!it) {
            continue;
        }
        const bool shouldShow = (mShowActionsForActive && i == mActivePresetIndex);
        it->setSlideOffset(shouldShow ? -it->actionsWidth() : 0.0);
    }
}

QVector<int> BrushPresetListWidget::generateCurrentOrder() const
{
    QVector<int> order;
    order.reserve(mPresets.size());
    for (int i = 0; i < mPresets.size(); ++i) {
        order.append(i);
    }
    return order;
}

void BrushPresetListWidget::updateUI()
{
    // Clear existing layout items (including spacers) to avoid accumulating gaps.
    while (QLayoutItem* li = mContainerLayout->takeAt(0)) {
        if (QWidget* w = li->widget()) {
            w->deleteLater();
        }
        delete li;
    }
    mPresetItems.clear();
    mFinalLineItem = nullptr;

    // Create the Pen (Final Line) item as the first scrollable row.
    {
        ToolOptionWidget::BrushPreset finalPreset;
        finalPreset.id = QStringLiteral("__final_line__");
        finalPreset.name = QStringLiteral("Pen");
        finalPreset.builtIn = true;
        auto* item = new BrushPresetItem(finalPreset, -1, mContainerWidget);
        item->setFinalLineMode(true);
        connect(item, &BrushPresetItem::selected, this, [this](int) {
            setFinalLineActive(true);
            emit presetSelected(-1);
        });
        connect(item, &BrushPresetItem::settingsRequested, this, [this](int) {
            // Create the popup first so mainwindow2's finalLineSettingsRequested handler
            // can call popup->setFinalLineLiveState() on an already-existing popup.
            ensureSettingsPopup();
            emit finalLineSettingsRequested();
            mSettingsPopup->setFinalLineMode(true);
            mSettingsPopup->setPresetId(QStringLiteral("__final_line__"));
            mSettingsPopup->setPresetName(tr("Pen"));
            QPoint itemGlobalPos = mFinalLineItem->mapToGlobal(mFinalLineItem->rect().topRight());
            mSettingsPopup->move(itemGlobalPos + QPoint(10, 0));
            mSettingsPopup->show();
            mSettingsPopup->raise();
            mSettingsPopup->activateWindow();
        });
        connect(item, &BrushPresetItem::dragStarted, this, &BrushPresetListWidget::startDrag);
        mContainerLayout->addWidget(item);
        mFinalLineItem = item;
    }

    // Create normal preset items
    for (int i = 0; i < mPresets.size(); ++i) {
        const auto& preset = mPresets[i];
        auto* item = new BrushPresetItem(preset, i, mContainerWidget);
        
        connect(item, &BrushPresetItem::selected, this, &BrushPresetListWidget::onPresetSelected);
        connect(item, &BrushPresetItem::settingsRequested, this, &BrushPresetListWidget::onPresetSettingsRequested);
        connect(item, &BrushPresetItem::dragStarted, this, &BrushPresetListWidget::startDrag);
        connect(item, &BrushPresetItem::deleteRequested, this, &BrushPresetListWidget::onPresetDeleteRequested);
        connect(item, &BrushPresetItem::duplicateRequested, this, &BrushPresetListWidget::onPresetDuplicateRequested);
        connect(item, &BrushPresetItem::slideClosed, this, &BrushPresetListWidget::onPresetSlideClosed);
        connect(item, &BrushPresetItem::slideStateRequested, this, &BrushPresetListWidget::onPresetSlideStateRequested);
        connect(item, &BrushPresetItem::renameRequested, this, &BrushPresetListWidget::onPresetRenameRequested);
        
        mContainerLayout->addWidget(item);
        mPresetItems.append(item);
        
        if (i == mActivePresetIndex) {
            item->setActive(true);
        }
    }

    // Single global spacer so the list always packs to the top.
    mContainerLayout->addStretch(1);

    // +1 for the Pen item
    mContainerWidget->setMinimumHeight((mPresets.size() + 1) * kBrushPresetRowHeightPx);

    applyActiveSlideState();
}

void BrushPresetListWidget::loadPresets()
{
    // BrushPresetStore is the single source of truth.
    auto* store = BrushPresetStore::instance();
    store->ensureLoaded();
    mPresets = store->presets();
    const QString activeId = store->activePresetId();
    mActivePresetIndex = 0;
    for (int i = 0; i < mPresets.size(); ++i) {
        if (mPresets[i].id == activeId) { mActivePresetIndex = i; break; }
    }
    updateUI();
}

void BrushPresetListWidget::startDrag(int index)
{
    if (mIsDragging || index < 0 || index >= mPresetItems.size()) {
        return;
    }

    mIsDragging = true;
    mDragFromIndex = index;
    mDropIndex = index;

    // Install event filter on all items so we receive their mouse move/release
    // events while dragging — grabMouse on a parent doesn't intercept child events.
    if (mFinalLineItem) {
        mFinalLineItem->installEventFilter(this);
    }
    for (BrushPresetItem* it : mPresetItems) {
        if (it) {
            it->installEventFilter(this);
        }
    }

    // Position and show the drop indicator overlay.
    if (mDropIndicator && mScrollArea && mScrollArea->viewport()) {
        QWidget* vp = mScrollArea->viewport();
        mDropIndicator->installEventFilter(this);
        const int scrollOffset = mScrollArea->verticalScrollBar() ? mScrollArea->verticalScrollBar()->value() : 0;
        // +1 to skip the Pen item which occupies row 0 in the container.
        const int lineY = (index + 1) * kBrushPresetRowHeightPx - scrollOffset;
        mDropIndicator->setGeometry(0, lineY - 3, vp->width(), 6);
        mDropIndicator->raise();
        mDropIndicator->show();
    }

    if (mScrollArea && mScrollArea->viewport()) {
        mScrollArea->viewport()->update();
    }
}

void BrushPresetListWidget::updateDrag(const QPoint& globalPos)
{
    if (!mIsDragging) {
        return;
    }

    const int newDrop = getDropIndex(globalPos);
    if (newDrop != mDropIndex) {
        mDropIndex = newDrop;
        // Reposition the overlay indicator.
        if (mDropIndicator && mScrollArea && mScrollArea->viewport()) {
            QWidget* vp = mScrollArea->viewport();
            const int scrollOffset = mScrollArea->verticalScrollBar() ? mScrollArea->verticalScrollBar()->value() : 0;
            // +1 to skip the Pen item which occupies row 0 in the container.
            const int lineY = (mDropIndex + 1) * kBrushPresetRowHeightPx - scrollOffset;
            mDropIndicator->setGeometry(0, lineY - 3, vp->width(), 6);
            mDropIndicator->update();
        }
    }
}

void BrushPresetListWidget::endDrag()
{
    if (!mIsDragging) {
        return;
    }

    mIsDragging = false;

    // Remove event filters installed during drag.
    if (mFinalLineItem) {
        mFinalLineItem->removeEventFilter(this);
    }
    for (BrushPresetItem* it : mPresetItems) {
        if (it) {
            it->removeEventFilter(this);
        }
    }

    if (mDropIndicator) {
        mDropIndicator->hide();
    }

    const int from = mDragFromIndex;
    // mDropIndex is a gap slot (0..n where n=count). Convert to QVector::move target.
    // When dropping below the source, subtract 1 to account for the source being removed first.
    int to = mDropIndex;
    if (to > from) {
        to -= 1;
    }
    to = qBound(0, to, mPresets.size() - 1);

    if (from != to && from >= 0 && from < mPresets.size()) {
        // Reorder backing data.
        mPresets.move(from, to);
        mPresetItems.move(from, to);

        // Update active index to follow the moved item.
        if (mActivePresetIndex == from) {
            mActivePresetIndex = to;
        } else if (from < mActivePresetIndex && to >= mActivePresetIndex) {
            mActivePresetIndex -= 1;
        } else if (from > mActivePresetIndex && to <= mActivePresetIndex) {
            mActivePresetIndex += 1;
        }

        // Rebuild layout order to match new mPresetItems order.
        while (QLayoutItem* li = mContainerLayout->takeAt(0)) {
            delete li;
        }
        for (int i = 0; i < mPresetItems.size(); ++i) {
            BrushPresetItem* it = mPresetItems[i];
            it->setIndex(i);
            it->setPreset(mPresets[i]);
            it->setActive(i == mActivePresetIndex);
            mContainerLayout->addWidget(it);
        }
        mContainerLayout->addStretch(1);
        mContainerWidget->setMinimumHeight(mPresets.size() * kBrushPresetRowHeightPx);

        // Persist new order.
        QVector<QString> ids;
        ids.reserve(mPresets.size());
        for (const auto& p : mPresets) {
            ids.append(p.id);
        }
        BrushPresetStore::instance()->reorderPresets(ids);

        QVector<int> newOrder;
        newOrder.reserve(mPresets.size());
        for (int i = 0; i < mPresets.size(); ++i) {
            newOrder.append(i);
        }
        emit presetOrderChanged(newOrder);

        updateSettingsPopupForActivePreset();
        mShowActionsForActive = false;
        applyActiveSlideState();

        // Select the moved preset now that reorder is committed.
        if (to >= 0 && to < mPresets.size()) {
            BrushPresetStore::instance()->setActivePresetId(mPresets[to].id);
            emit presetSelected(to);
        }
    } else {
        // No reorder — still select the dragged item (user intended to interact with it).
        if (from >= 0 && from < mPresets.size()) {
            onPresetSelected(from);
        }
    }

    mDragFromIndex = -1;
    mDropIndex = -1;

    if (mScrollArea && mScrollArea->viewport()) {
        mScrollArea->viewport()->update();
    }
}

int BrushPresetListWidget::getDropIndex(const QPoint& globalPos) const
{
    if (!mContainerWidget) {
        return 0;
    }
    const QPoint local = mContainerWidget->mapFromGlobal(globalPos);
    const int rowH = kBrushPresetRowHeightPx;
    // The Pen item occupies row 0; brush presets start at row 1.
    // Subtract 1 so drop index 0 = before first brush preset.
    const int rawIdx = (local.y() + rowH / 2) / rowH - 1;
    // Drop index is the gap between rows: 0 = before first, n = after last.
    const int idx = qBound(0, rawIdx, mPresetItems.size());
    return idx;
}

void BrushPresetListWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (mIsDragging) {
        updateDrag(QCursor::pos());
    }
    QWidget::mouseMoveEvent(event);
}

void BrushPresetListWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (mIsDragging) {
        endDrag();
    }
    QWidget::mouseReleaseEvent(event);
}

void BrushPresetListWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}

bool BrushPresetListWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Paint the drop indicator line when the overlay widget needs painting.
    if (watched == mDropIndicator && event->type() == QEvent::Paint) {
        QPainter p(mDropIndicator);
        const QColor accent = CosmicColor::accent();
        const int y = mDropIndicator->height() / 2;
        const int x1 = 4;
        const int x2 = mDropIndicator->width() - 4;
        p.setPen(QPen(accent, 2, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(x1, y, x2, y);
        p.setPen(QPen(accent, 6, Qt::SolidLine, Qt::RoundCap));
        p.drawPoint(x1, y);
        p.drawPoint(x2, y);
        return true;
    }

    // During drag, intercept mouse move/release from child BrushPresetItems.
    if (mIsDragging) {
        if (event->type() == QEvent::MouseMove) {
            updateDrag(QCursor::pos());
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            endDrag();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BrushPresetItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (mBuiltIn || mRenameMode) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    // Start inline rename on double-click anywhere on the row content,
    // except the settings button and any revealed swipe actions.
    if (mSettingsRect.contains(event->pos())) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    if (mSlideOffset < 0.0 && actionsRect().contains(event->pos())) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    {
        // Keep UI uncluttered: close swipe actions before entering rename.
        if (mSlideOffset < 0.0) {
            setSlideOpen(false, false);
            emit slideStateRequested(mIndex, false);
        }
        startInlineRename();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

bool BrushPresetItem::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mRenameEdit) {
        switch (event->type()) {
        case QEvent::KeyPress: {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                cancelInlineRename();
                return true;
            } else if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                const QString newName = mRenameEdit->text().trimmed();
                if (!newName.isEmpty() && newName != mName) {
                    emit renameRequested(mIndex, newName);
                } else {
                    cancelInlineRename();
                }
                return true;
            }
            break;
        }
        case QEvent::FocusOut: {
            const QString newName = mRenameEdit->text().trimmed();
            if (!newName.isEmpty() && newName != mName) {
                emit renameRequested(mIndex, newName);
            } else {
                cancelInlineRename();
            }
            return true;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
