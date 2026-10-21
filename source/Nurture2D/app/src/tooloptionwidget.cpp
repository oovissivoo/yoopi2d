#include "canvaspaletteoverlay.h"
#include "canvasgalleryoverlay.h"
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
#include "tooloptionwidget.h"
#include "ui_tooloptions.h"
#include "preferencemanager.h"
#include "managers/brushassetstore.h"
#include "../../paintlyFillAlgorithm/blenderstyleslider.h"
#include "../../CUI/blenderstylecombobox.h"
#include "../../CUI/blenderstyletoggle.h"
#include "../../CUI/blenderstylemodalpopup.h"
#include "../../CUI/blenderstylevaluefield.h"

#include <QSettings>
#include "managers/settingsmanager.h"
#include <QDebug>

#include "cameraoptionswidget.h"
#include "pencolorpalette.h"
#include "bucketoptionswidget.h"
#include "spinslider.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "editor.h"
#include "util.h"
#include "layer.h"
#include "layermanager.h"
#include "stroketool.h"
#include "toolmanager.h"
#include "colormanager.h"
#include "eyedroppertool.h"
#include "lassogapfilltool.h"
#include "rectgapfilltool.h"
#include "scribblearea.h"
#include "dplatetool.h"
#include "dplatecolorpalette.h"
#include "ignorefillsplittoggle.h"
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QPushButton>
#include <QCheckBox>
#include "beziercurveeditor.h"
#include "tool/pentool.h"
#include <QApplication>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QByteArray>
#include <QBuffer>
#include <QLineEdit>
#include <QButtonGroup>
#include <QIcon>
#include <QListWidget>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QRandomGenerator>
#include <QHash>
#include <QSet>
#include "beziercurveeditor.h"
#include "pentool.h"
#include "penciltool.h"
#include "eyedroppertool.h"
#include "../CUI/blenderstylebutton.h"
#include "../CUI/cosmictappersliders.h"
#include "../CUI/cosmicintensitybox.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmichslider.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicignorecolor.h"
#include "cosmicsplittoollayout.h"
#include "managers/brushpresetstore.h"

namespace {
static QString activeBrushFactoryPresetId()
{
    // Prefer store-driven active id (authoritative when presets are managed via BrushPresetStore)
    if (BrushPresetStore::instance()) {
        BrushPresetStore::instance()->ensureLoaded();
        const QString storeId = BrushPresetStore::instance()->activePresetId();
        if (!storeId.isEmpty()) {
            return storeId;
        }
    }

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup("PencilBrushFactory");

    const QString directId = settings.value("ActivePresetId", QString()).toString();
    const int count = settings.value("Count", 0).toInt();
    const int activeIndex = qBound(0, settings.value("ActiveIndex", 0).toInt(), qMax(0, count - 1));

    QString id = directId;
    if (id.isEmpty() && count > 0) {
        settings.beginGroup(QStringLiteral("Preset_%1").arg(activeIndex));
        id = settings.value("Id", QString()).toString();
        if (id.isEmpty()) {
            id = QStringLiteral("builtin:%1").arg(settings.value("Name", QStringLiteral("default")).toString().trimmed().toLower());
        }
        settings.endGroup();
    }

    settings.endGroup();
    return id;
}

} // namespace

class BrushPresetDelegate : public QStyledItemDelegate
{
public:
    explicit BrushPresetDelegate(QVector<ToolOptionWidget::BrushPreset>* presets, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , mPresets(presets)
    {
    }

    // Clear cached textures when preset set changes structure (add/remove or
    // texture replacement) to avoid using stale images for the wrong rows.
    void clearTextureCaches() const
    {
        mCustomTextures.clear();
        mHasDefaultTexture = false;
        mDefaultTexture = QImage();
        clearStrokeCache();
    }

    // Clear cached stroke images and dirty flags (e.g. when presets are
    // reloaded from disk or removed).
    void clearStrokeCache() const
    {
        mStrokeCache.clear();
        mDirtyRows.clear();
    }

    // Mark a given preset row as dirty so the next paint will recompute the
    // stroke preview and refresh its cached image.
    void markRowDirty(int row) const
    {
        if (row >= 0) {
            mDirtyRows.insert(row);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        // Fixed tile size: wide stroke preview with label at bottom
        return QSize(220, 72);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!mPresets || !index.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const int row = index.row();
        if (row < 0 || row >= mPresets->size()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const ToolOptionWidget::BrushPreset& preset = mPresets->at(row);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QRect outerRect = option.rect.adjusted(4, 4, -4, -4);

        QPainterPath backgroundPath;
        backgroundPath.addRoundedRect(outerRect, 6, 6);

        QColor baseColor = option.palette.color(QPalette::Base).darker(115);
        QColor borderColor = baseColor.darker(130);
        if (option.state & QStyle::State_Selected) {
            // Keep fill consistent; represent selection as a bright border
            borderColor = option.palette.highlight().color();
        }

        painter->fillPath(backgroundPath, baseColor);
        painter->setPen(QPen(borderColor, 1.0));
        painter->drawPath(backgroundPath);

        // Stroke preview area
        QRect strokeRect = outerRect.adjusted(8, 8, -8, -22);

        // Render a tapered brush stroke composed of reusable white dabs that
        // respect the brush texture. Rotation controls per-dab orientation
        // jitter; spacing/scatter/size jitter affect dab density, offset and
        // scale. This approximates how the raster brush behaves when drawing.
        if (strokeRect.width() > 4 && strokeRect.height() > 4) {
            const int w = strokeRect.width();
            const int h = strokeRect.height();
            const QSize desiredSize(w, h);

            // Fast path: reuse cached stroke image when size matches and row
            // is not marked dirty.
            auto it = mStrokeCache.constFind(row);
            const bool hasCache = (it != mStrokeCache.constEnd());
            if (hasCache && it.value().size() == desiredSize && !mDirtyRows.contains(row)) {
                painter->drawImage(strokeRect.topLeft(), it.value());
            } else {
                QImage strokeImg(w, h, QImage::Format_ARGB32_Premultiplied);
                strokeImg.fill(Qt::transparent);

                // Lightweight blur kernel to gently weld neighbouring dabs
                auto blurStrokeImage = [](QImage& img) {
                    if (img.isNull()) {
                        return;
                    }
                    const int iw = img.width();
                    const int ih = img.height();
                    if (iw < 3 || ih < 3) {
                        return;
                    }

                    const int radius = 2;
                    const int kernelSize = radius * 2 + 1;
                    Q_UNUSED(kernelSize);

                    QImage tmp(img.size(), img.format());

                    // Horizontal pass
                    for (int y = 0; y < ih; ++y) {
                        QRgb* dstLine = reinterpret_cast<QRgb*>(tmp.scanLine(y));
                        for (int x = 0; x < iw; ++x) {
                            int rSum = 0;
                            int gSum = 0;
                            int bSum = 0;
                            int aSum = 0;
                            int count = 0;
                            for (int k = -radius; k <= radius; ++k) {
                                const int sx = x + k;
                                if (sx < 0 || sx >= iw) {
                                    continue;
                                }
                                const QRgb px = img.pixel(sx, y);
                                rSum += qRed(px);
                                gSum += qGreen(px);
                                bSum += qBlue(px);
                                aSum += qAlpha(px);
                                ++count;
                            }
                            if (count > 0) {
                                dstLine[x] = qRgba(rSum / count, gSum / count, bSum / count, aSum / count);
                            } else {
                                dstLine[x] = img.pixel(x, y);
                            }
                        }
                    }

                    // Vertical pass
                    for (int y = 0; y < ih; ++y) {
                        QRgb* dstLine = reinterpret_cast<QRgb*>(img.scanLine(y));
                        for (int x = 0; x < iw; ++x) {
                            int rSum = 0;
                            int gSum = 0;
                            int bSum = 0;
                            int aSum = 0;
                            int count = 0;
                            for (int k = -radius; k <= radius; ++k) {
                                const int sy = y + k;
                                if (sy < 0 || sy >= ih) {
                                    continue;
                                }
                                const QRgb px = tmp.pixel(x, sy);
                                rSum += qRed(px);
                                gSum += qGreen(px);
                                bSum += qBlue(px);
                                aSum += qAlpha(px);
                                ++count;
                            }
                            if (count > 0) {
                                dstLine[x] = qRgba(rSum / count, gSum / count, bSum / count, aSum / count);
                            } else {
                                dstLine[x] = tmp.pixel(x, y);
                            }
                        }
                    }
                };

                const qreal midY = h * 0.5;

                // Base thickness is independent of size jitter so the preview
                // always shows a full, strong stroke.
                const qreal baseRadius = qMax<qreal>(1.0, h * 0.26);

                // Scatter controls vertical noise amplitude
                const qreal scatterNorm = qBound<qreal>(0.0, preset.scatter / 3.0, 1.0);
                const qreal maxJitterY = h * 0.35 * scatterNorm;

                // Spacing + scatter influence dab density
                const qreal spacingMult = qBound<qreal>(0.25, preset.spacingMult, 2.0);
                qreal densityFactor = 1.5 - (spacingMult - 0.25) * 0.7 + scatterNorm * 0.8;
                densityFactor = qBound<qreal>(0.6, densityFactor, 2.4);
                const int baseDabs = 72;
                const int dabCount = qBound(40, static_cast<int>(baseDabs * densityFactor), 160);

                // Resolve texture -> white dab image (greyscale as alpha)
                const QImage* dabImage = nullptr;
                auto makeBlackFromTexture = [](const QImage& src) {
                    if (src.isNull()) {
                        return QImage();
                    }

                    QImage working = src;
                    const int maxDim = 1024;
                    if (working.width() > maxDim || working.height() > maxDim) {
                        working = working.scaled(QSize(maxDim, maxDim), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }

                    QImage texture = working.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                    const int texWidth = texture.width();
                    const int texHeight = texture.height();
                    if (texWidth <= 0 || texHeight <= 0) {
                        return QImage();
                    }

                    // Greyscale -> alpha (black transparent, white opaque)
                    for (int ty = 0; ty < texHeight; ++ty) {
                        QRgb* line = reinterpret_cast<QRgb*>(texture.scanLine(ty));
                        for (int tx = 0; tx < texWidth; ++tx) {
                            int gray = qGray(line[tx]);
                            line[tx] = qRgba(255, 255, 255, gray);
                        }
                    }

                    // Build circular dab sampling the texture alpha
                    const int dabSize = 64;
                    const qreal radius = dabSize / 2.0;
                    const qreal radiusMinusOne = radius - 1.0;

                    QImage dab(dabSize, dabSize, QImage::Format_ARGB32_Premultiplied);
                    dab.fill(Qt::transparent);

                    const int texCenterX = texWidth / 2;
                    const int texCenterY = texHeight / 2;
                    const int texSampleSize = qMin(texWidth, texHeight) / 2;
                    const qreal texScale = texSampleSize > 0 ? (texSampleSize / radius) : 1.0;

                    const uchar* texBits = texture.constBits();
                    const int texBytesPerLine = texture.bytesPerLine();

                    for (int y = 0; y < dabSize; ++y) {
                        QRgb* line = reinterpret_cast<QRgb*>(dab.scanLine(y));
                        const qreal dy = y - radius + 0.5;
                        const qreal dy2 = dy * dy;

                        for (int x = 0; x < dabSize; ++x) {
                            const qreal dx = x - radius + 0.5;
                            const qreal distSq = dx * dx + dy2;
                            const qreal dist = std::sqrt(distSq);

                            if (dist >= radius) {
                                continue;
                            }

                            const qreal circleMask = (dist > radiusMinusOne) ? (radius - dist) : 1.0;

                            int texX = texCenterX + qRound((x - radius) * texScale);
                            int texY = texCenterY + qRound((y - radius) * texScale);

                            if (texWidth > 0 && texHeight > 0) {
                                texX = (texX % texWidth + texWidth) % texWidth;
                                texY = (texY % texHeight + texHeight) % texHeight;
                            }

                            const uchar* texPixel = texBits + texY * texBytesPerLine + texX * 4;
                            const qreal textureOpacity = texPixel[3] / 255.0;

                            const qreal finalOpacity = circleMask * textureOpacity;
                            const int alpha = qRound(finalOpacity * 255.0);
                            if (alpha <= 0) {
                                continue;
                            }
                            const int c = alpha;
                            line[x] = qRgba(c, c, c, alpha);
                        }
                    }

                    return dab;
                };

                if (!preset.textureAssetId.isEmpty()) {
                    if (!mCustomTextures.contains(row)) {
                        QImage src;
                        if (src.loadFromData(BrushAssetStore::instance()->loadTexture(preset.textureAssetId))) {
                            mCustomTextures.insert(row, makeBlackFromTexture(src));
                        } else {
                            mCustomTextures.insert(row, QImage());
                        }
                    }
                    const QImage& tex = mCustomTextures.value(row);
                    if (!tex.isNull()) {
                        dabImage = &tex;
                    }
                } else {
                    if (!mHasDefaultTexture) {
                        QImage src;
                        if (src.load(QLatin1String(":/BrushFiles/P1.png"))) {
                            mDefaultTexture = makeBlackFromTexture(src);
                        }
                        mHasDefaultTexture = true;
                    }
                    if (!mDefaultTexture.isNull()) {
                        dabImage = &mDefaultTexture;
                    }
                }

                // Fallback: simple circular dab if texture is missing
                static const int fallbackSize = 32;
                QImage fallbackDab;
                if (!dabImage || dabImage->isNull()) {
                    fallbackDab = QImage(fallbackSize, fallbackSize, QImage::Format_ARGB32_Premultiplied);
                    fallbackDab.fill(Qt::transparent);
                    QPainter fp(&fallbackDab);
                    fp.setRenderHint(QPainter::Antialiasing, true);
                    QRadialGradient g(QPointF(fallbackSize / 2.0, fallbackSize / 2.0), fallbackSize / 2.0);
                    g.setColorAt(0.0, QColor(255, 255, 255, 255));
                    g.setColorAt(1.0, QColor(255, 255, 255, 0));
                    fp.setBrush(g);
                    fp.setPen(Qt::NoPen);
                    fp.drawEllipse(QRectF(0, 0, fallbackSize, fallbackSize));
                    fp.end();
                    dabImage = &fallbackDab;
                }

                const int dabBase = qMax(1, qMin(dabImage->width(), dabImage->height()));

                // Rotation jitter strength from slider (0..40). Use a more
                // conservative range in the preview so individual dabs don't
                // stick out with harsh angles.
                const qreal rotStrength = qBound<qreal>(0.0, preset.rotationDeg / 40.0, 1.0);

                QPainter imgPainter(&strokeImg);
                imgPainter.setRenderHint(QPainter::Antialiasing, true);

                // Deterministic random source per row for stable previews
                QRandomGenerator gen(static_cast<quint32>(qHash(QStringLiteral("brush_preview_tile_dabs_%1").arg(row))));

                for (int i = 0; i < dabCount; ++i) {
                    const qreal t = (static_cast<qreal>(i) + 0.5) / static_cast<qreal>(dabCount); // 0..1

                    // Taper profile: 0 at ends, 1 in center
                    const qreal tri = (t < 0.5) ? (t * 2.0) : (2.0 - 2.0 * t);
                    qreal radius = baseRadius * (0.30 + 0.70 * tri);

                    // Size jitter per dab only adds variation around the base
                    if (preset.sizeJitter > 0.0) {
                        const qreal amount = 0.6 * preset.sizeJitter; // up to ±60% at max
                        const qreal sizeJit = (gen.generateDouble() * 2.0 - 1.0) * amount;
                        radius *= (1.0 + sizeJit);
                        radius = qMax<qreal>(1.0, radius);
                    }

                    // Center along fixed S-curve (independent of rotation)
                    const qreal x = strokeRect.left() + t * (strokeRect.width());
                    const qreal curveAmp = h * 0.18;
                    const qreal baseY = midY + curveAmp * qSin(t * 3.14159265 * 2.0);

                    qreal jitterY = 0.0;
                    if (maxJitterY > 0.0) {
                        jitterY = (gen.generateDouble() * 2.0 - 1.0) * maxJitterY;
                    }
                    const qreal y = baseY + jitterY;

                    // Clamp to stroke rect
                    if (y < 0.0 || y >= h) {
                        continue;
                    }

                    // Rotation: base direction ~horizontal + modest random jitter
                    const qreal baseAngle = 0.0; // along X
                    const qreal jitterAngle = (gen.generateDouble() * 2.0 - 1.0) * 45.0 * rotStrength; // up to +-45 deg
                    const qreal angleDeg = baseAngle + jitterAngle;

                    // Alpha taper: tips softer, body fully opaque.
                    const qreal alphaFactor = 0.35 + 0.65 * tri; // 0.35 at tips, 1.0 center

                    if (alphaFactor <= 0.01) {
                        continue;
                    }

                    // Draw dab with appropriate scale and rotation
                    const qreal scale = (radius * 2.0) / static_cast<qreal>(dabBase);
                    imgPainter.save();
                    imgPainter.translate(x, y);
                    imgPainter.rotate(angleDeg);
                    imgPainter.scale(scale, scale);

                    // Global alpha for this dab
                    imgPainter.setOpacity(alphaFactor);
                    imgPainter.drawImage(QPointF(-dabImage->width() / 2.0, -dabImage->height() / 2.0), *dabImage);
                    imgPainter.restore();
                }

                blurStrokeImage(strokeImg);

                // Store snapshot for future paints and clear dirty flag
                mStrokeCache.insert(row, strokeImg);
                mDirtyRows.remove(row);

                painter->drawImage(strokeRect.topLeft(), strokeImg);
            }
        }

        // Preset name in bottom-left
        QRect textRect = outerRect.adjusted(10, outerRect.height() - 22, -8, -4);
        painter->setPen(option.palette.color(QPalette::Text));
        QFont f = painter->font();
        f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
        painter->setFont(f);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, preset.name);

        painter->restore();
    }

private:
    QVector<ToolOptionWidget::BrushPreset>* mPresets;
    mutable bool mHasDefaultTexture = false;
    mutable QImage mDefaultTexture;
    mutable QHash<int, QImage> mCustomTextures;
    mutable QHash<int, QImage> mStrokeCache;
    mutable QSet<int> mDirtyRows;
};

ToolOptionWidget::ToolOptionWidget(QWidget* parent) : BaseDockWidget(parent)
{
    setWindowTitle(tr("Options", "Window title of tool option panel like pen width, feather etc.."));

    // Wrap entire options content in a scroll area to avoid resizing surrounding widgets
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* innerWidget = new QWidget(scrollArea);
    ui = new Ui::ToolOptions;
    ui->setupUi(innerWidget);

    // Hugging: we want the options content to hug vertically, but still be free
    // to shrink/expand horizontally with the pane. Avoid layout size constraints
    // here because they impose a minimum width based on sizeHint() and can cause
    // right-side clipping inside narrow panes.
    if (innerWidget->layout()) {
        innerWidget->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        // Reduce layout margins to fit comfortably at the new 280px pane width
        innerWidget->layout()->setContentsMargins(4, 4, 4, 4);
    }
    innerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    scrollArea->setWidget(innerWidget);
    mDockContentWidget = scrollArea;
    mInnerOptionsWidget = innerWidget;
    setDockContentWidget(scrollArea);
}

ToolOptionWidget::~ToolOptionWidget()
{
    delete ui;
}

bool ToolOptionWidget::hasVisibleOptions() const
{
    if (!ui) {
        return false;
    }

    // Simple rule: if any of the known option blocks is NOT hidden, then we have options.
    // NOTE: We intentionally use isHidden() rather than isVisible(), because the tool options
    // may be embedded in other containers and the original dock widget may be hidden.
    auto has = [](QWidget* w) -> bool {
        return w && !w->isHidden();
    };

    if (has(mBucketOptionsWidget)) return true;
    if (has(mCameraOptionsWidget)) return true;
    // mDplateColorPalette removed; Dplate shows Draw Behind toggle via mDrawBehindToggle

    if (ui->cosmicPenOptionsWidget && !ui->cosmicPenOptionsWidget->isHidden()) return true;
    if (ui->cosmicPencilOptionsWidget && !ui->cosmicPencilOptionsWidget->isHidden()) return true;
    if (ui->cosmicEraserOptionsWidget && !ui->cosmicEraserOptionsWidget->isHidden()) return true;
    if (ui->cosmicEyedropperOptionsWidget && !ui->cosmicEyedropperOptionsWidget->isHidden()) return true;
    if (ui->cosmicCurveOptionsWidget && !ui->cosmicCurveOptionsWidget->isHidden()) return true;
    if (ui->cosmicGapFillOptionsWidget && !ui->cosmicGapFillOptionsWidget->isHidden()) return true;
    if (ui->cosmicDplateOptionsWidget && !ui->cosmicDplateOptionsWidget->isHidden()) return true;
    if (has(mDrawBehindToggle)) return true;

    return false;
}

void ToolOptionWidget::refreshOptionsPaneState()
{
    const bool hasOpts = hasVisibleOptions();
    // Always emit so the pane body forced-hidden state stays in sync with the
    // active tool's options visibility.  The receiver (CosmicToolsPane::setBodyForcedHidden)
    // has its own deduplication guard, so redundant calls are cheap.
    mLastHasVisibleOptions = hasOpts;
    mHasVisibleOptionsInitialized = true;
    emit visibleOptionsChanged(hasOpts);

    // Hugging: clamp the *inner* options widget height to its active layout.
    // This works whether the widget is hosted inside the legacy dock scroll area
    // or reparented directly into a Cosmic pane (no nested scrollbars).
    if (mInnerOptionsWidget) {
        // Force the layout to recompute before reading sizeHint so that
        // recently shown/hidden widgets (e.g. mBucketOptionsWidget) are
        // accounted for correctly.
        if (mInnerOptionsWidget->layout()) {
            mInnerOptionsWidget->layout()->activate();
        }
        const int h = (mInnerOptionsWidget->layout())
                          ? qMax(0, mInnerOptionsWidget->layout()->sizeHint().height())
                          : qMax(0, mInnerOptionsWidget->sizeHint().height());
        if (h > 0) {
            mInnerOptionsWidget->setMaximumHeight(h);
            mInnerOptionsWidget->setMinimumHeight(0);
            mInnerOptionsWidget->updateGeometry();
            mInnerOptionsWidget->adjustSize();
        }
    }
}

void ToolOptionWidget::initUI()
{
    mBucketOptionsWidget = new BucketOptionsWidget(editor(), this);
    mCameraOptionsWidget = new CameraOptionsWidget(editor(), this);
    // PenColorPalette moved to canvas overlay; do not create here
    mPenColorPalette = nullptr;
    mDplateColorPalette = new DplateColorPalette(this);
    // No pen color palette in tool options
    ui->horizontalLayout_2->addWidget(mBucketOptionsWidget);
    ui->horizontalLayout_2->addWidget(mCameraOptionsWidget);
    ui->penColorPaletteLayout->addWidget(mDplateColorPalette);

    mBucketOptionsWidget->setHidden(true);
    mCameraOptionsWidget->setHidden(true);
    // mPenColorPalette is not present
    mDplateColorPalette->setHidden(true);

    QSettings settings(PENCIL2D, PENCIL2D);

    // Initialize from the active tool's current width to avoid mismatched keys
    qreal initialWidth = 3.0;
    if (editor() && editor()->tools() && editor()->tools()->currentTool()) {
        initialWidth = editor()->tools()->currentTool()->properties.width;
    }

    // Cosmic input field for Pen width (Pen-only visual; shares the same WIDTH property)
    if (!mPenWidthField) {
        mPenWidthField = new CosmicInputField(this);
        mPenWidthField->setObjectName("penWidthField");
        mPenWidthField->setVariant(CosmicInputField::Minimal);
        mPenWidthField->setRange(StrokeTool::WIDTH_MIN, StrokeTool::WIDTH_MAX);
        mPenWidthField->setDecimals(0);
        mPenWidthField->setSingleStep(1.0);
        mPenWidthField->setAccentColor(BlenderStyleSlider::getAccentColor());
        mPenWidthField->setValue(initialWidth);
        if (ui->horizontalLayout_2) {
            // Place the cosmic field at the start of the Width row.
            ui->horizontalLayout_2->insertWidget(0, mPenWidthField);
        }
        // Hidden by default; shown only for Pen tools in setVisibility().
        mPenWidthField->hide();
    }

    // NOTE to future devs: when introducing new UI that controls the same logic as legacy UI,
    // remove/hide the legacy widgets to avoid duplicate controls and conflicting settings.

    // No Pencil width/opacity pressure icon: pressure is handled via cosmic/other dedicated UI.

    // CosmicSplitLayout: Eyedropper reference dropdown (From View / Current Layer)
    if (ui->cosmicEyedropperRowsArea && ui->cosmicEyedropperRowsLayout) {
        CosmicSplitToolLayout eyedropperLayout(ui->cosmicEyedropperRowsLayout,
                                               ui->cosmicEyedropperRowsArea);

        if (!mCosmicEyedropperReferenceDropDown) {
            mCosmicEyedropperReferenceDropDown = new CosmicDropDown(this);
            mCosmicEyedropperReferenceDropDown->setObjectName("cosmicEyedropperReferenceDropDown");
            QStringList opts;
            opts << tr("From View") << tr("Current Layer");
            mCosmicEyedropperReferenceDropDown->setOptions(opts);
            mCosmicEyedropperReferenceDropDown->setCurrentIndex(0);
            mLabelEyedropperRef = eyedropperLayout.createEntry(tr("Reference"), mCosmicEyedropperReferenceDropDown);

            if (auto* tm = editor()->tools()) {
                connect(mCosmicEyedropperReferenceDropDown, &CosmicDropDown::currentIndexChanged,
                        this, [tm](int idx){
                    if (auto* dropper = qobject_cast<EyedropperTool*>(tm->getTool(EYEDROPPER))) {
                        dropper->setReferenceMode(idx == 0 ? EyedropperReferenceMode::FROM_VIEW
                                                           : EyedropperReferenceMode::CURRENT_LAYER);
                        dropper->saveSettings();
                    }
                });
            }
        }
    }

    ui->featherSlider->init(tr("Feather"), SpinSlider::LOG, SpinSlider::INTEGER, StrokeTool::FEATHER_MIN, StrokeTool::FEATHER_MAX);
    ui->featherSlider->setValue(settings.value("brushFeather", "5").toDouble());
    ui->featherSlider->setSliderColor(BlenderStyleSlider::getAccentColor());

    // Legacy Krita-style Pen smoothing UI (combo + sliders) has been
    // removed in favor of the Cosmic Pen controls. Internal properties
    // (penSmoothingMode, weightedDistance, stab2Delay) still exist and
    // are driven by the Cosmic widgets.
    
    // Prepare Draw Behind toggle (replace legacy checkbox in layout for non-Cosmic split tools)
    if (!mDrawBehindToggle) {
        mDrawBehindToggle = new CosmicCheckboxToggle(this);
        mDrawBehindToggle->setObjectName("drawBehindToggle");

        // Standard checkbox visuals, with a larger box and icon in Big scale mode.
        mDrawBehindToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        mDrawBehindToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
        mDrawBehindToggle->setText(tr("Draw Behind"));
        mDrawBehindToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        // Initially insert at the legacy checkbox position
        if (ui->drawBehindBox) {
            int idx = ui->verticalLayout_2->indexOf(ui->drawBehindBox);
            if (idx < 0) idx = ui->verticalLayout_2->count();
            ui->verticalLayout_2->insertWidget(idx, mDrawBehindToggle);
            ui->drawBehindBox->setVisible(false);
        } else {
            ui->verticalLayout_2->addWidget(mDrawBehindToggle);
        }
    }
    
    // Prepare Anti-Aliasing toggle (replace legacy checkbox in layout)
    if (!mAntiAliasingToggle) {
        mAntiAliasingToggle = new BlenderStyleToggle(this);
        mAntiAliasingToggle->setObjectName("antiAliasingToggle");
        mAntiAliasingToggle->setText(tr("Anti-Aliasing"));
        mAntiAliasingToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        // Insert at position of legacy checkbox
        if (ui->useAABox) {
            int idx = ui->verticalLayout_2->indexOf(ui->useAABox);
            if (idx < 0) idx = ui->verticalLayout_2->count();
            ui->verticalLayout_2->insertWidget(idx, mAntiAliasingToggle);
            ui->useAABox->setVisible(false);
        } else {
            ui->verticalLayout_2->addWidget(mAntiAliasingToggle);
        }
    }

    // Prepare toggles to replace all legacy QCheckBox options in verticalLayout_2
    auto insertToggleAt = [this](QVBoxLayout* layout, QWidget* oldWidget, QWidget* toggle){
        if (!layout || !oldWidget || !toggle) return;
        int idx = layout->indexOf(oldWidget);
        if (idx < 0) idx = layout->count();
        layout->insertWidget(idx, toggle);
        oldWidget->setVisible(false);
    };

    // Use Feather (disabled for Eraser tool, but kept for other tools)
    if (ui->useFeatherBox && !mUseFeatherToggle) {
        mUseFeatherToggle = new BlenderStyleToggle(this);
        mUseFeatherToggle->setObjectName("useFeatherToggle");
        mUseFeatherToggle->setText(tr("Use Feather"));
        mUseFeatherToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->useFeatherBox, mUseFeatherToggle);
    }
    // Show Size and Diff.
    if (ui->showInfoBox && !mShowInfoToggle) {
        mShowInfoToggle = new BlenderStyleToggle(this);
        mShowInfoToggle->setObjectName("showInfoToggle");
        mShowInfoToggle->setText(tr("Show Size and Diff."));
        mShowInfoToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->showInfoBox, mShowInfoToggle);
    }
    // Fill Contour
    if (ui->fillContourBox && !mFillContourToggle) {
        mFillContourToggle = new BlenderStyleToggle(this);
        mFillContourToggle->setObjectName("fillContourToggle");
        mFillContourToggle->setText(tr("Fill Contour"));
        mFillContourToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->fillContourBox, mFillContourToggle);
    }
    // Closed Path
    if (ui->useClosedPathBox && !mClosedPathToggle) {
        mClosedPathToggle = new BlenderStyleToggle(this);
        mClosedPathToggle->setObjectName("closedPathToggle");
        mClosedPathToggle->setText(tr("Closed Path"));
        mClosedPathToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->useClosedPathBox, mClosedPathToggle);
    }
    // Bézier
    if (ui->useBezierBox && !mBezierToggle) {
        mBezierToggle = new BlenderStyleToggle(this);
        mBezierToggle->setObjectName("bezierToggle");
        mBezierToggle->setText(tr("Bézier"));
        mBezierToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->useBezierBox, mBezierToggle);
    }
    // Invisible
    if (ui->makeInvisibleBox && !mInvisibleToggle) {
        mInvisibleToggle = new BlenderStyleToggle(this);
        mInvisibleToggle->setObjectName("invisibleToggle");
        mInvisibleToggle->setText(tr("Invisible"));
        mInvisibleToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->makeInvisibleBox, mInvisibleToggle);
    }
    // Preserve Alpha
    if (ui->preserveAlphaBox && !mPreserveAlphaToggle) {
        mPreserveAlphaToggle = new BlenderStyleToggle(this);
        mPreserveAlphaToggle->setObjectName("preserveAlphaToggle");
        mPreserveAlphaToggle->setText(tr("Alpha"));
        mPreserveAlphaToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->preserveAlphaBox, mPreserveAlphaToggle);
    }
    // Vector Merge
    if (ui->vectorMergeBox && !mVectorMergeToggle) {
        mVectorMergeToggle = new BlenderStyleToggle(this);
        mVectorMergeToggle->setObjectName("vectorMergeToggle");
        mVectorMergeToggle->setText(tr("Merge"));
        mVectorMergeToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        insertToggleAt(ui->verticalLayout_2, ui->vectorMergeBox, mVectorMergeToggle);
    }

    // Add Post-Stroke Correction intensity box (Off/Low/Med/High) AFTER the smoothing sliders
    if (!mPostCorrBox) {
        mPostCorrBox = new CosmicIntensityBox(this);
        mPostCorrBox->setObjectName("postCorrBox");

        // Initially Off
        mPostCorrBox->setValue(0);

        connect(mPostCorrBox, &CosmicIntensityBox::valueChanged, this, [this](int level){
            editor()->tools()->setPostCorrectionLevel(level);

            // Mutual exclusion with Pull mode (index 1 in mPenSmoothingComboBox)
            if (mPenSmoothingComboBox) {
                bool pullEnabled = (level == 0);
                mPenSmoothingComboBox->setOptionEnabled(1, pullEnabled);
                // If Pull was selected and now disabled, switch to Weighted
                if (!pullEnabled && mPenSmoothingComboBox->currentIndex() == 1) {
                    mPenSmoothingComboBox->setCurrentIndex(0); // Switch to Weighted
                }
            }

            // Force refresh so the fallback to Weighted (when level>0) is reflected right away
            updateUI();
        });
    }

    // CosmicSplitLayout: construct Pen split layout entries (layout-only; logic will be wired later).
    if (ui->cosmicPenRowsArea && ui->cosmicPenRowsLayout) {
        CosmicSplitToolLayout penLayout(ui->cosmicPenRowsLayout,
                                        ui->cosmicPenRowsArea);

        if (!mCosmicPenSizeField) {
            mCosmicPenSizeField = new CosmicInputField(this);
            mCosmicPenSizeField->setObjectName("cosmicPenSizeField");
            mCosmicPenSizeField->setVariant(CosmicInputField::Minimal);
            mCosmicPenSizeField->setDecimals(1);
            mCosmicPenSizeField->setRange(StrokeTool::WIDTH_MIN, StrokeTool::WIDTH_MAX);
            mCosmicPenSizeField->setSingleStep(0.5);
            mCosmicPenSizeField->setAccentColor(BlenderStyleSlider::getAccentColor());
        }

        if (mCosmicPenSizeField) {
            // Single row: Size field with a compact pressure icon toggle on the right.
            mLabelPenSize = penLayout.createEntry(tr("Size"), mCosmicPenSizeField);
        }

        if (!mCosmicPenModeDropDown) {
            mCosmicPenModeDropDown = new CosmicDropDown(this);
            mCosmicPenModeDropDown->setObjectName("cosmicPenModeDropDown");
            QStringList modeOptions;
            modeOptions << tr("Delayed") << tr("String");
            mCosmicPenModeDropDown->setOptions(modeOptions);
            mCosmicPenModeDropDown->setCurrentIndex(0);
            mLabelPenMode = penLayout.createEntry(tr("Mode"), mCosmicPenModeDropDown);
        }

        if (!mCosmicPenSmoothField) {
            mCosmicPenSmoothField = new CosmicInputField(this);
            mCosmicPenSmoothField->setObjectName("cosmicPenSmoothField");
            mCosmicPenSmoothField->setVariant(CosmicInputField::Minimal);
            mCosmicPenSmoothField->setDecimals(1);
            mCosmicPenSmoothField->setRange(0.0, 100.0);
            mCosmicPenSmoothField->setSingleStep(0.5);
            mCosmicPenSmoothField->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelPenSmooth = penLayout.createEntry(tr("Smooth"), mCosmicPenSmoothField);
        }

        if (!mCosmicPenDrawBehindToggle) {
            mCosmicPenDrawBehindToggle = new CosmicCheckboxToggle(this);
            mCosmicPenDrawBehindToggle->setObjectName("cosmicPenDrawBehindToggle");
            mCosmicPenDrawBehindToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            mCosmicPenDrawBehindToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
            // No text here; label lives in the left column.
            mCosmicPenDrawBehindToggle->setShowText(false);
            mCosmicPenDrawBehindToggle->setText(QString());
            mCosmicPenDrawBehindToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelPenDrawBehind = penLayout.createEntry(tr("Draw Behind"), mCosmicPenDrawBehindToggle);
        }

        if (!mCosmicPenPostIntensityBox) {
            mCosmicPenPostIntensityBox = new CosmicIntensityBox(this);
            mCosmicPenPostIntensityBox->setObjectName("cosmicPenPostIntensityBox");
            mCosmicPenPostIntensityBox->setValue(0);
            mLabelPenPostCorr = penLayout.createEntry(tr("Post Correction"), mCosmicPenPostIntensityBox);
        }
    }

    // CosmicSplitLayout: construct Pencil split layout entries (bitmap Pencil tool).
    if (ui->cosmicPencilRowsArea && ui->cosmicPencilRowsLayout) {
        CosmicSplitToolLayout pencilLayout(ui->cosmicPencilRowsLayout,
                                           ui->cosmicPencilRowsArea);

        if (!mCosmicPencilSizeField) {
            mCosmicPencilSizeField = new CosmicInputField(this);
            mCosmicPencilSizeField->setObjectName("cosmicPencilSizeField");
            mCosmicPencilSizeField->setVariant(CosmicInputField::Minimal);
            mCosmicPencilSizeField->setDecimals(1);
            mCosmicPencilSizeField->setRange(StrokeTool::WIDTH_MIN, StrokeTool::WIDTH_MAX);
            mCosmicPencilSizeField->setSingleStep(0.5);
            mCosmicPencilSizeField->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelPencilSize = pencilLayout.createEntry(tr("Size"), mCosmicPencilSizeField);
        }

        // Post Correction should be the 2nd entry (right after Size)
        if (!mCosmicPencilPostIntensityBox) {
            mCosmicPencilPostIntensityBox = new CosmicIntensityBox(this);
            mCosmicPencilPostIntensityBox->setObjectName("cosmicPencilPostIntensityBox");
            mCosmicPencilPostIntensityBox->setValue(0);
            mCosmicPencilPostCorrRow = pencilLayout.createEntryRow(tr("Post Correction"), mCosmicPencilPostIntensityBox);
            if (mCosmicPencilPostCorrRow) {
                mLabelPencilPostCorr = mCosmicPencilPostCorrRow->findChild<QLabel*>();
            }
        }

        if (!mCosmicPencilOpacityField) {
            mCosmicPencilOpacityField = new CosmicInputField(this);
            mCosmicPencilOpacityField->setObjectName("cosmicPencilOpacityField");
            mCosmicPencilOpacityField->setVariant(CosmicInputField::Minimal);
            // Opacity is expressed as a percentage [0,100] to mirror the legacy slider.
            mCosmicPencilOpacityField->setDecimals(0);
            mCosmicPencilOpacityField->setRange(0.0, 100.0);
            mCosmicPencilOpacityField->setSingleStep(1.0);
            mCosmicPencilOpacityField->setAccentColor(BlenderStyleSlider::getAccentColor());
            mCosmicPencilOpacityRow = pencilLayout.createEntryRow(tr("Opacity"), mCosmicPencilOpacityField);
            // Also store the label pointer by finding it in the row
            if (mCosmicPencilOpacityRow) {
                mLabelPencilOpacity = mCosmicPencilOpacityRow->findChild<QLabel*>();
            }
        }

        if (!mCosmicPencilModeDropDown) {
            mCosmicPencilModeDropDown = new CosmicDropDown(this);
            mCosmicPencilModeDropDown->setObjectName("cosmicPencilModeDropDown");
            QStringList modeOptions;
            modeOptions << tr("Delayed") << tr("String");
            mCosmicPencilModeDropDown->setOptions(modeOptions);
            mCosmicPencilModeDropDown->setCurrentIndex(0);
            mLabelPencilMode = pencilLayout.createEntry(tr("Mode"), mCosmicPencilModeDropDown);
        }

        if (!mCosmicPencilSmoothField) {
            mCosmicPencilSmoothField = new CosmicInputField(this);
            mCosmicPencilSmoothField->setObjectName("cosmicPencilSmoothField");
            mCosmicPencilSmoothField->setVariant(CosmicInputField::Minimal);
            mCosmicPencilSmoothField->setDecimals(1);
            mCosmicPencilSmoothField->setRange(0.0, 100.0);
            mCosmicPencilSmoothField->setSingleStep(0.5);
            mCosmicPencilSmoothField->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelPencilSmooth = pencilLayout.createEntry(tr("Smooth"), mCosmicPencilSmoothField);
        }

        if (!mCosmicPencilDrawBehindToggle) {
            mCosmicPencilDrawBehindToggle = new CosmicCheckboxToggle(this);
            mCosmicPencilDrawBehindToggle->setObjectName("cosmicPencilDrawBehindToggle");
            mCosmicPencilDrawBehindToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            mCosmicPencilDrawBehindToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
            mCosmicPencilDrawBehindToggle->setShowText(false);
            mCosmicPencilDrawBehindToggle->setText(QString());
            mCosmicPencilDrawBehindToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelPencilDrawBehind = pencilLayout.createEntry(tr("Draw Behind"), mCosmicPencilDrawBehindToggle);
        }

        // Ensure Post Correction row stays second even if layout was rebuilt/reordered.
        if (mCosmicPencilPostCorrRow && ui->cosmicPencilRowsLayout) {
            ui->cosmicPencilRowsLayout->removeWidget(mCosmicPencilPostCorrRow);
            ui->cosmicPencilRowsLayout->insertWidget(1, mCosmicPencilPostCorrRow);
        }
    }

    // CosmicSplitLayout: construct Eraser split layout entries (size + pressure + lasso mode).
    if (ui->cosmicEraserRowsArea && ui->cosmicEraserRowsLayout) {
        CosmicSplitToolLayout eraserLayout(ui->cosmicEraserRowsLayout,
                                           ui->cosmicEraserRowsArea);

        if (!mCosmicEraserSizeField) {
            mCosmicEraserSizeField = new CosmicInputField(this);
            mCosmicEraserSizeField->setObjectName("cosmicEraserSizeField");
            mCosmicEraserSizeField->setVariant(CosmicInputField::Minimal);
            mCosmicEraserSizeField->setDecimals(1);
            mCosmicEraserSizeField->setRange(StrokeTool::WIDTH_MIN, StrokeTool::WIDTH_MAX);
            mCosmicEraserSizeField->setSingleStep(0.5);
            mCosmicEraserSizeField->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelEraserSize = eraserLayout.createEntry(tr("Size"), mCosmicEraserSizeField);
        }

        if (!mCosmicEraserPressureToggle) {
            mCosmicEraserPressureToggle = new CosmicCheckboxToggle(this);
            mCosmicEraserPressureToggle->setObjectName("cosmicEraserPressureToggle");
            mCosmicEraserPressureToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            mCosmicEraserPressureToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
            mCosmicEraserPressureToggle->setShowText(false);
            mCosmicEraserPressureToggle->setText(QString());
            mCosmicEraserPressureToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelEraserPressure = eraserLayout.createEntry(tr("Pressure"), mCosmicEraserPressureToggle);
        }

        if (!mEraserLassoToggle) {
            mEraserLassoToggle = new CosmicCheckboxToggle(this);
            mEraserLassoToggle->setObjectName("cosmicEraserLassoToggle");
            mEraserLassoToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            mEraserLassoToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
            // Label lives in the left column; keep checkbox icon-only.
            mEraserLassoToggle->setShowText(false);
            mEraserLassoToggle->setText(QString());
            mEraserLassoToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelEraserLasso = eraserLayout.createEntry(tr("Lasso Mode"), mEraserLassoToggle);
        }
    }

    // CosmicSplitLayout: construct Curve-family split layout entries for stroke tools
    // (Curve, BSpline, and Polyline all share this exact UI and settings pipeline).
    if (ui->cosmicCurveRowsArea && ui->cosmicCurveRowsLayout) {
        CosmicSplitToolLayout curveLayout(ui->cosmicCurveRowsLayout,
                                          ui->cosmicCurveRowsArea);

        // Size control (shares WIDTH property with other width controls; bitmap Curve only).
        if (!mCosmicCurveWidthField) {
            mCosmicCurveWidthField = new CosmicInputField(this);
            mCosmicCurveWidthField->setObjectName("cosmicCurveWidthField");
            mCosmicCurveWidthField->setVariant(CosmicInputField::Minimal);
            mCosmicCurveWidthField->setDecimals(1);
            mCosmicCurveWidthField->setRange(StrokeTool::WIDTH_MIN, StrokeTool::WIDTH_MAX);
            mCosmicCurveWidthField->setSingleStep(0.5);
            mCosmicCurveWidthField->setAccentColor(BlenderStyleSlider::getAccentColor());
            mCosmicCurveWidthField->setValue(initialWidth);
            mLabelCurveSize = curveLayout.createEntry(tr("Size"), mCosmicCurveWidthField);
        }

        // Hybrid snap strength (replaces legacy combo; Off/Low/Med/High mapped to 0..3).
        if (!mCosmicHybridSnapBox) {
            mCosmicHybridSnapBox = new CosmicIntensityBox(this);
            mCosmicHybridSnapBox->setObjectName("cosmicHybridSnapBox");
            // Initial value mirrors hybrid snap level from settings (clamped 0..3).
            SettingsManager* sm = SettingsManager::instance();
            int snapLevel = sm->getValue("Tool/Curve/Shared/HybridSnapLevel", 0).toInt();
            if (snapLevel < 0) snapLevel = 0;
            if (snapLevel > 3) snapLevel = 3;
            mCosmicHybridSnapBox->setValue(snapLevel);
            mLabelCurveSnap = curveLayout.createEntry(tr("Snap"), mCosmicHybridSnapBox);
        }

        // Stroke taper control (Fixed Pixels only, 0-200px; mirrors mCurveTapperSliders).
        if (!mCosmicCurveTapperSliders) {
            const qreal cosmicStartTaper = settings.value("Tool/Curve/Shared/TaperStart", 0.0).toDouble();
            const qreal cosmicEndTaper   = settings.value("Tool/Curve/Shared/TaperEnd", 0.0).toDouble();

            mCosmicCurveTapperSliders = new CosmicTapperSliders(this);
            mCosmicCurveTapperSliders->setObjectName("cosmicCurveTapperSliders");
            mCosmicCurveTapperSliders->setStartValue(cosmicStartTaper);
            mCosmicCurveTapperSliders->setEndValue(cosmicEndTaper);
            mCosmicCurveTapperSliders->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelCurveTaper = curveLayout.createEntry(tr("Stroke Taper"), mCosmicCurveTapperSliders);
        }

        // Draw Behind control (cosmic default checkbox variant, label lives in left column).
        if (!mCosmicCurveDrawBehindToggle) {
            mCosmicCurveDrawBehindToggle = new CosmicCheckboxToggle(this);
            mCosmicCurveDrawBehindToggle->setObjectName("cosmicCurveDrawBehindToggle");
            mCosmicCurveDrawBehindToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            mCosmicCurveDrawBehindToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
            mCosmicCurveDrawBehindToggle->setShowText(false);
            mCosmicCurveDrawBehindToggle->setText(QString());
            mCosmicCurveDrawBehindToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelCurveDrawBehind = curveLayout.createEntry(tr("Draw Behind"), mCosmicCurveDrawBehindToggle);
        }
    }

    // CosmicSplitLayout: Dplate tool — Draw Behind + box count controls
    if (ui->cosmicDplateRowsArea && ui->cosmicDplateRowsLayout) {
        CosmicSplitToolLayout dplateLayout(ui->cosmicDplateRowsLayout,
                                           ui->cosmicDplateRowsArea);

        if (!mCosmicDplateDrawBehindToggle) {
            mCosmicDplateDrawBehindToggle = new CosmicCheckboxToggle(this);
            mCosmicDplateDrawBehindToggle->setObjectName("cosmicDplateDrawBehindToggle");
            mCosmicDplateDrawBehindToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            mCosmicDplateDrawBehindToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
            mCosmicDplateDrawBehindToggle->setShowText(false);
            mCosmicDplateDrawBehindToggle->setText(QString());
            mCosmicDplateDrawBehindToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
            mLabelDplateDrawBehind = dplateLayout.createEntry(tr("Draw Behind"), mCosmicDplateDrawBehindToggle);
        }

        // Big boxes count: CosmicIntensityBox, 4 levels (0-3)
        if (!mCosmicDplateBigBoxField) {
            mCosmicDplateBigBoxField = new CosmicIntensityBox(this);
            mCosmicDplateBigBoxField->setObjectName("cosmicDplateBigBoxField");
            mCosmicDplateBigBoxField->setValue(3);
            mLabelDplateBigBoxes = dplateLayout.createEntry(tr("Big Boxes"), mCosmicDplateBigBoxField);
        }

        // Small boxes count: CosmicIntensityBox, 4 levels (0-3)
        if (!mCosmicDplateSmallBoxBox) {
            mCosmicDplateSmallBoxBox = new CosmicIntensityBox(this);
            mCosmicDplateSmallBoxBox->setObjectName("cosmicDplateSmallBoxBox");
            mCosmicDplateSmallBoxBox->setValue(0);
            mLabelDplateSmallBoxes = dplateLayout.createEntry(tr("Small Boxes"), mCosmicDplateSmallBoxBox);
        }
    }

    if (ui->cosmicPenBottomLayout) {
        if (!mCosmicPenSettingsToggle) {
            mCosmicPenSettingsToggle = new CosmicCheckboxToggle(this);
            mCosmicPenSettingsToggle->setObjectName("cosmicPenSettingsToggle");
            mCosmicPenSettingsToggle->setMode(CosmicCheckboxToggle::Custom);
            mCosmicPenSettingsToggle->setShowText(false);
            mCosmicPenSettingsToggle->setShowIcon(true);
            mCosmicPenSettingsToggle->setBordered(false);
            mCosmicPenSettingsToggle->setScaleMode(CosmicCheckboxToggle::ScaleNormal);
            mCosmicPenSettingsToggle->setIcon(QIcon(":/Interaction/settings.svg"));
            mCosmicPenSettingsToggle->setToolTip(tr("Pen settings"));
            mCosmicPenSettingsToggle->setAccentColor(BlenderStyleSlider::getAccentColor());

            ui->cosmicPenBottomLayout->addStretch();
            ui->cosmicPenBottomLayout->addSpacing(4);
            ui->cosmicPenBottomLayout->addWidget(mCosmicPenSettingsToggle);
        }
    }

    if (ui->cosmicPencilBottomLayout) {
        ui->cosmicPencilBottomLayout->addStretch();
    }
    // Legacy Weighted Distance / Pull delay sliders removed; values are
    // now controlled by the Cosmic Pen "Smooth" field based on mode.

    // Curve taper controls init - Fixed Pixels only (0-200px)
    // Shared between Curve and BSpline tools
    const qreal initialStartTaper = settings.value("Tool/Curve/Shared/TaperStart", 0.0).toDouble();
    const qreal initialEndTaper = settings.value("Tool/Curve/Shared/TaperEnd", 0.0).toDouble();

    // Keep legacy sliders hidden for backward compatibility, but drive UI via CurveTaperControl
    if (ui->curveTaperStartSlider) {
        ui->curveTaperStartSlider->init(tr("Start Taper (px)"), SpinSlider::LINEAR, SpinSlider::FLOAT, 0.0, 200.0);
        ui->curveTaperStartSlider->setValue(initialStartTaper);
        ui->curveTaperStartSlider->hide();
    }
    if (ui->curveTaperEndSlider) {
        ui->curveTaperEndSlider->init(tr("End Taper (px)"), SpinSlider::LINEAR, SpinSlider::FLOAT, 0.0, 200.0);
        ui->curveTaperEndSlider->setValue(initialEndTaper);
        ui->curveTaperEndSlider->hide();
    }

    if (!mCurveTapperSliders && ui->curveTaperGroupLayout) {
        mCurveTapperSliders = new CosmicTapperSliders(ui->curveTaperGroup);
        mCurveTapperSliders->setStartValue(initialStartTaper);
        mCurveTapperSliders->setEndValue(initialEndTaper);
        mCurveTapperSliders->setAccentColor(BlenderStyleSlider::getAccentColor());
        ui->curveTaperGroupLayout->addWidget(mCurveTapperSliders);
    }

    if (!mHybridSnapComboBox) {
        mHybridSnapComboBox = new BlenderStyleComboBox(this);
        mHybridSnapComboBox->setObjectName("hybridSnapComboBox");
        mHybridSnapComboBox->setOptions({tr("Snap:Off"), tr("Snap:Low"), tr("Snap:Med"), tr("Snap:High")});
        SettingsManager* sm = SettingsManager::instance();
        int snapLevel = sm->getValue("Tool/Curve/Shared/HybridSnapLevel", 0).toInt();
        if (snapLevel < 0) snapLevel = 0;
        if (snapLevel > 3) snapLevel = 3;
        mHybridSnapComboBox->setCurrentIndex(snapLevel);
        mHybridSnapComboBox->setAccentColor(BlenderStyleSlider::getAccentColor());
        mHybridSnapComboBox->hide();
        connect(mHybridSnapComboBox, &BlenderStyleComboBox::currentIndexChanged, this, [this](int idx) {
            if (editor() && editor()->tools()) {
                editor()->tools()->setHybridSnapLevel(idx);
            }
        });
    }


    // Hide legacy stabilizer combo box (Base Stabiliser is now always "None")
    if (ui->stabilizerLabel) ui->stabilizerLabel->setVisible(false);
    if (ui->inpolLevelsCombo) ui->inpolLevelsCombo->setVisible(false);

    // Reorganize layout order beneath Width/Weighted Distance for stroke tools.
    // Desired order of stacked rows:
    //   Width
    //   Weighted Distance / Pull Distance
    //   Draw Behind
    //   Pen Smoothing Mode
    //   Post-Correction
    //   Hybrid Snap
    //   Adjust (pressure)
    // Find the position immediately after the weightedDistanceLayout row.
    QLayoutItem* weightedItem = nullptr;
    for (int i = 0; i < ui->verticalLayout_3->count(); ++i) {
        QLayoutItem* item = ui->verticalLayout_3->itemAt(i);
        if (item && item->layout() && item->layout()->objectName() == "weightedDistanceLayout") {
            weightedItem = item;
            break;
        }
    }

    int insertPos = weightedItem ? ui->verticalLayout_3->indexOf(weightedItem) + 1 : ui->verticalLayout_3->count();

    // Insert in order: Snap -> Draw Behind -> (optional Anti-Aliasing)
    // Note: Hybrid snap is only visible for the curve family tools; for Pen it stays hidden,
    // so Draw Behind remains the first visible row under Width/Weighted Distance there.
    if (mHybridSnapComboBox) {
        ui->verticalLayout_3->insertWidget(insertPos++, mHybridSnapComboBox);
    }
    if (mDrawBehindToggle) {
        ui->verticalLayout_3->insertWidget(insertPos++, mDrawBehindToggle);
    }
    if (mAntiAliasingToggle) {
        ui->verticalLayout_3->insertWidget(insertPos++, mAntiAliasingToggle);
    }

    // Also hide the legacy bottom stabiliser label and combo if present
    if (ui->stabilizerLabel) ui->stabilizerLabel->setVisible(false);
    if (ui->inpolLevelsCombo) ui->inpolLevelsCombo->setVisible(false);

    // Ensure initial visibility of groups based on current mode
    updateUI();
}





void ToolOptionWidget::updatePencilOpacityField()
{
    if (!mCosmicPencilOpacityField) {
        return;
    }

    BaseTool* currentTool = editor()->tools()->currentTool();
    if (!currentTool || currentTool->type() != PENCIL) {
        return;
    }

    const QString presetId = activeBrushFactoryPresetId();
    int percent = 100;
    if (!presetId.isEmpty()) {
        QSettings s(PENCIL2D, PENCIL2D);
        s.beginGroup("PencilBrushFactoryOpacityById");
        s.beginGroup(presetId);
        percent = qBound(0, s.value("OpacityPercent", 100).toInt(), 100);
        s.endGroup();
        s.endGroup();
    } else if (auto* pencil = qobject_cast<PencilTool*>(editor()->tools()->getTool(PENCIL))) {
        percent = qBound(0, int(qRound(pencil->getPencilOpacity() * 100.0)), 100);
    }

    QSignalBlocker b(mCosmicPencilOpacityField);
    mCosmicPencilOpacityField->setValue(percent);
}

void ToolOptionWidget::stepSmoothValue(int delta)
{
    // The unified Pencil smooth field is the active one for both Pen and Pencil
    // (Pen uses the unified Pencil UI). Just call increment/decrement on it —
    // the field's existing valueChanged connection handles everything else.
    if (!mCosmicPencilSmoothField) return;
    if (delta > 0)
        mCosmicPencilSmoothField->increment();
    else if (delta < 0)
        mCosmicPencilSmoothField->decrement();
}

void ToolOptionWidget::updateUI()
{
    BaseTool* currentTool = editor()->tools()->currentTool();
    Q_ASSERT(currentTool);

    setVisibility(currentTool);

    const Properties& p = currentTool->properties;
    const bool isBitmapLayer            = (editor()->layers()->currentLayer()->type() == Layer::BITMAP);
    const bool isCosmicEraserLayout     = (ui->cosmicEraserOptionsWidget && currentTool->type() == ERASER);
    const bool isCosmicEyedropperLayout = (ui->cosmicEyedropperOptionsWidget && currentTool->type() == EYEDROPPER);
    // NOTE: Curve, BSpline, and Polyline intentionally share the same Cosmic UI and
    // persisted settings (width, taper, hybrid snap, draw behind). Keeping them on
    // a single UI/pipeline avoids the desync issues we previously had when each tool
    // owned its own controls. If you add new curve-family options, wire them here
    // once and use shared SettingsManager keys.
    const bool isCosmicCurveLayout     = (ui->cosmicCurveOptionsWidget &&
                                          (currentTool->type() == CURVE ||
                                           currentTool->type() == BSPLINE ||
                                           currentTool->type() == POLYLINE));

    if (isCosmicCurveLayout) {
        // Curve-family tools always use the Pencil preset pipeline now.
    }

    if (currentTool->isPropertyEnabled(WIDTH))
    {
        setPenWidth(p.width);
    }
    if (currentTool->isPropertyEnabled(FEATHER))
    {
        setPenFeather(p.feather);
    }
    setUseFeather(p.useFeather);

    // Sync pressure state
    const bool supportsPressure = currentTool->isPropertyEnabled(PRESSURE);

    // Legacy Pressure checkbox is hidden; state is driven by the toggles beside Width/Opacity
    setPenInvisibility(p.invisibility);
    setPreserveAlpha(p.preserveAlpha);
    setVectorMergeEnabled(p.vectorMergeEnabled);
    setAA(p.useAA);
    // Base Stabiliser removed - always None (0)
    setFillContour(p.useFillContour);
    setShowSelectionInfo(p.showSelectionInfo);
    setClosedPath(p.closedPolylinePath);
    setDrawBehind(p.drawBehind);

    // Legacy Krita-style Pen smoothing UI has been removed from the stacked
    // Blender controls; smoothing and post-correction are represented only
    // via the Cosmic Pen controls for the Pen tool.

    // Legacy Weighted Distance / Pull delay sliders were removed from UI;
    // their values remain in Properties and are driven by the Cosmic Pen UI.

    if (mCurveTapperSliders) {
        mCurveTapperSliders->setStartValue(p.curveTaperStart);
        mCurveTapperSliders->setEndValue(p.curveTaperEnd);
    }
    if (mHybridSnapComboBox) {
        QSignalBlocker b(mHybridSnapComboBox);
        mHybridSnapComboBox->setCurrentIndex(qBound(0, p.hybridSnapLevel, 3));
    }



    // Conditional visibility for legacy smoothing UI removed; Cosmic Pen
    // controls are the only UI for Pen smoothing.

    // CosmicSplitLayout (Unified): Pencil UI is the only UI for both Pencil and Pen.
    // We sync it from whichever stroke tool is active.
    const bool isUnifiedPencilUi = (ui->cosmicPencilOptionsWidget &&
                                   (currentTool->type() == PENCIL || currentTool->type() == PEN) &&
                                   (isBitmapLayer || currentTool->type() == PEN));
    if (isUnifiedPencilUi) {
        const bool isPenTool = (currentTool->type() == PEN);

        if (mCosmicPencilSizeField) {
            QSignalBlocker b(mCosmicPencilSizeField);
            mCosmicPencilSizeField->setValue(p.width);
        }

        if (mCosmicPencilOpacityField) {
            if (currentTool->type() == PENCIL) {
                const QString presetId = activeBrushFactoryPresetId();
                int percent = 100;
                if (!presetId.isEmpty()) {
                    QSettings s(PENCIL2D, PENCIL2D);
                    s.beginGroup("PencilBrushFactoryOpacityById");
                    s.beginGroup(presetId);
                    percent = qBound(0, s.value("OpacityPercent", 100).toInt(), 100);
                    s.endGroup();
                    s.endGroup();
                } else if (auto* pencil = qobject_cast<PencilTool*>(editor()->tools()->getTool(PENCIL))) {
                    percent = qBound(0, int(qRound(pencil->getPencilOpacity() * 100.0)), 100);
                }
                QSignalBlocker b(mCosmicPencilOpacityField);
                mCosmicPencilOpacityField->setValue(percent);
            }
            // Pencil presets: show Opacity. Final Line: hide Opacity.
            if (mCosmicPencilOpacityRow) {
                mCosmicPencilOpacityRow->setVisible(!isPenTool);
            } else {
                mCosmicPencilOpacityField->setVisible(!isPenTool);
            }
            mCosmicPencilOpacityField->setEnabled(!isPenTool);
        }

        int modeId = static_cast<int>(p.penSmoothingMode);
        if (modeId == static_cast<int>(PenSmoothingMode::LEGACY)) {
            modeId = static_cast<int>(PenSmoothingMode::WEIGHTED);
        }

        if (mCosmicPencilModeDropDown) {
            QSignalBlocker b(mCosmicPencilModeDropDown);
            // Delayed(0)=WEIGHTED, String(1)=PULL
            int modeIndex = 0;
            if (modeId == static_cast<int>(PenSmoothingMode::PULL)) {
                modeIndex = 1;
            }
            mCosmicPencilModeDropDown->setCurrentIndex(modeIndex);
        }

        if (mCosmicPencilSmoothField) {
            QSignalBlocker b(mCosmicPencilSmoothField);
            // Unified Smooth UI: always [1..10] integer and persistent.
            mCosmicPencilSmoothField->setDecimals(0);
            mCosmicPencilSmoothField->setRange(1.0, 10.0);
            mCosmicPencilSmoothField->setSingleStep(1.0);

            int initUi = 5;
            {
                const double legacy = (modeId == static_cast<int>(PenSmoothingMode::PULL))
                                          ? qMax(0.0, static_cast<double>(p.stab2Delay))
                                          : qMax(0.0, static_cast<double>(p.weightedDistance));
                if (legacy <= 0.0) {
                    initUi = 1;
                } else {
                    const double t = qBound(0.0, qSqrt(qBound(0.0, legacy, 300.0) / 300.0), 1.0);
                    initUi = qBound(2, 2 + int(qRound(8.0 * t)), 10);
                }
            }
            mUnifiedSmoothUiValue = initUi;
            mUnifiedSmoothUiInitialized = true;
            mCosmicPencilSmoothField->setValue(double(qBound(1, mUnifiedSmoothUiValue, 10)));
        }

        if (mCosmicPencilDrawBehindToggle) {
            QSignalBlocker b(mCosmicPencilDrawBehindToggle);
            mCosmicPencilDrawBehindToggle->setChecked(p.drawBehind);
        }

        if (mCosmicPencilPostIntensityBox) {
            QSignalBlocker b(mCosmicPencilPostIntensityBox);
            int clamped = qBound(0, p.postCorrectionLevel, 3);
            mCosmicPencilPostIntensityBox->setValue(clamped);

            // Final Line: show Post Correction. Pencil presets: hide it.
            if (mCosmicPencilPostCorrRow) {
                mCosmicPencilPostCorrRow->setVisible(isPenTool);
            } else {
                mCosmicPencilPostIntensityBox->setVisible(isPenTool);
            }

            mCosmicPencilPostIntensityBox->setFillTintEnabled(false);
            mCosmicPencilPostIntensityBox->setEnabled(isPenTool);
            mCosmicPencilPostIntensityBox->setToolTip(QString());
            if (QWidget* pw = mCosmicPencilPostIntensityBox->parentWidget()) {
                pw->setToolTip(QString());
            }

            mCosmicPencilPostIntensityBox->update();
        }
        // Do NOT return here; allow generic UI (feather, alpha, etc.) to sync as well.
    }

    // CosmicSplitLayout: keep Eyedropper-specific Cosmic controls in sync with tool properties.
    if (isCosmicEyedropperLayout) {
        if (mCosmicEyedropperReferenceDropDown) {
            QSignalBlocker b(mCosmicEyedropperReferenceDropDown);
            int mode = p.eyedropperReferenceMode;
            if (mode < 0 || mode > 1) mode = 0;
            mCosmicEyedropperReferenceDropDown->setCurrentIndex(mode);
        }

        // Eyedropper has no other legacy options in this dock; keep generic UI hidden.
        refreshOptionsPaneState();
        return;
    }

    // CosmicSplitLayout: keep Eraser-specific Cosmic controls in sync with tool properties
    // and hide legacy UI to prevent duplicate controls.
    if (isCosmicEraserLayout) {
        if (mCosmicEraserSizeField) {
            QSignalBlocker b(mCosmicEraserSizeField);
            mCosmicEraserSizeField->setValue(p.width);
        }

        if (mCosmicEraserPressureToggle) {
            QSignalBlocker b(mCosmicEraserPressureToggle);
            mCosmicEraserPressureToggle->setChecked(p.pressure > 0);
            mCosmicEraserPressureToggle->setEnabled(supportsPressure);
        }

        if (mEraserLassoToggle) {
            QSignalBlocker b(mEraserLassoToggle);
            mEraserLassoToggle->setChecked(p.eraserLassoMode);
        }

        // Hide legacy controls when using the cosmic eraser layout
        if (ui->featherSlider)   ui->featherSlider->setVisible(false);
        if (mPenWidthField)      mPenWidthField->setVisible(false);
        if (mUseFeatherToggle)   mUseFeatherToggle->setVisible(false);
        if (ui->useFeatherBox)   ui->useFeatherBox->setVisible(false);

        if (ui->verticalLayout_2) {
            for (int i = 0; i < ui->verticalLayout_2->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_2->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        w->setVisible(false);
                    }
                }
            }
        }

        if (ui->verticalLayout_3) {
            for (int i = 0; i < ui->verticalLayout_3->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_3->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        if (w != ui->cosmicEraserOptionsWidget) {
                            w->setVisible(false);
                        }
                    }
                }
            }
        }

        refreshOptionsPaneState();
        return;
    }

    // CosmicSplitLayout: keep Curve-specific Cosmic controls in sync with tool properties
    // and hide legacy curve rows to prevent duplicate controls when the cosmic layout is active.
    if (isCosmicCurveLayout) {
        if (mCosmicCurveWidthField) {
            QSignalBlocker b(mCosmicCurveWidthField);
            mCosmicCurveWidthField->setValue(p.width);
        }
        if (mCosmicCurveTapperSliders) {
            QSignalBlocker b(mCosmicCurveTapperSliders);
            mCosmicCurveTapperSliders->setStartValue(p.curveTaperStart);
            mCosmicCurveTapperSliders->setEndValue(p.curveTaperEnd);
        }
        if (mCosmicHybridSnapBox) {
            QSignalBlocker b(mCosmicHybridSnapBox);
            int clamped = qBound(0, p.hybridSnapLevel, 3);
            mCosmicHybridSnapBox->setValue(clamped);
        }
        if (mCosmicCurveDrawBehindToggle) {
            QSignalBlocker b(mCosmicCurveDrawBehindToggle);
            mCosmicCurveDrawBehindToggle->setChecked(p.drawBehind);
        }

        // Hide legacy stacked controls while the dedicated Cosmic Curve layout is visible.
        if (ui->curveTaperGroup)   ui->curveTaperGroup->setVisible(false);
        if (mHybridSnapComboBox)   mHybridSnapComboBox->setVisible(false);
        if (mDrawBehindToggle)     mDrawBehindToggle->setVisible(false);

        refreshOptionsPaneState();
        return;
    }

    // CosmicSplitLayout: sync Dplate Draw Behind toggle
    const bool isCosmicDplateLayout = (ui->cosmicDplateOptionsWidget && currentTool->type() == DPLATE);
    if (isCosmicDplateLayout) {
        if (mCosmicDplateDrawBehindToggle) {
            QSignalBlocker b(mCosmicDplateDrawBehindToggle);
            mCosmicDplateDrawBehindToggle->setChecked(p.drawBehind);
        }
        if (mDrawBehindToggle) mDrawBehindToggle->setVisible(false);

        // Sync box count controls
        auto* dplateTool = static_cast<DplateTool*>(currentTool);
        if (mCosmicDplateBigBoxField) {
            QSignalBlocker b(mCosmicDplateBigBoxField);
            mCosmicDplateBigBoxField->setValue(dplateTool->bigBoxCount());
        }
        if (mCosmicDplateSmallBoxBox) {
            QSignalBlocker b(mCosmicDplateSmallBoxBox);
            mCosmicDplateSmallBoxBox->setValue(dplateTool->smallBoxCount());
        }

        refreshOptionsPaneState();
        return;
    }
    const bool isGapFill = currentTool->type() == LASSO_GAP_FILL || currentTool->type() == RECT_GAP_FILL;

    if (isGapFill) {
        // Lazily build the cosmic gap-fill panel
        if (!mCosmicGapUseActiveToggle || !mCosmicGapIgnoreColor) {
            if (ui->cosmicGapFillRowsArea && ui->cosmicGapFillRowsLayout) {
                CosmicSplitToolLayout split(ui->cosmicGapFillRowsLayout, ui->cosmicGapFillRowsArea);

                    if (!mCosmicGapUseActiveToggle) {
                        mCosmicGapUseActiveToggle = new CosmicCheckboxToggle(this);
                        mCosmicGapUseActiveToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
                        mCosmicGapUseActiveToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
                        mCosmicGapUseActiveToggle->setShowText(false);
                        mCosmicGapUseActiveToggle->setShowIcon(true);
                        mCosmicGapUseActiveToggle->setToolTip(tr("Use the currently active color as the gap fill color"));

                        QSettings settings(PENCIL2D, PENCIL2D);
                        bool useActive = settings.value("GapFill/UseActiveColor", false).toBool();
                        mCosmicGapUseActiveToggle->setChecked(useActive);

                        connect(mCosmicGapUseActiveToggle, &QAbstractButton::toggled, this, [this](bool on){
                            QSettings s(PENCIL2D, PENCIL2D);
                            s.setValue("GapFill/UseActiveColor", on);

                            if (auto* lt = qobject_cast<LassoGapFillTool*>(editor()->tools()->getTool(LASSO_GAP_FILL))) {
                                lt->setUseActiveColor(on); lt->saveSettings();
                            }
                            if (auto* rt = qobject_cast<RectGapFillTool*>(editor()->tools()->getTool(RECT_GAP_FILL))) {
                                rt->setUseActiveColor(on); rt->saveSettings();
                            }
                        });
                    }

                    if (!mCosmicGapIgnoreColor) {
                        mCosmicGapIgnoreColor = new CosmicIgnoreColor(this);
                        mCosmicGapIgnoreColor->setObjectName("cosmicGapIgnoreColor");

                        // Initialize from editor
                        QColor init = QColor::fromRgba(qUnpremultiply(editor()->ignoreFillColor()));
                        if (!init.isValid()) init = Qt::black;
                        mCosmicGapIgnoreColor->setIgnoreColor(init);
                        mCosmicGapIgnoreColor->setIgnoreEnabled(editor()->ignoreFillEnabled());
                        mCosmicGapIgnoreColor->setColorPickerActive(false);

                        connect(mCosmicGapIgnoreColor, &CosmicIgnoreColor::ignoreEnabledChanged,
                                this, [this](bool on){ editor()->setIgnoreFillEnabled(on); });

                        connect(mCosmicGapIgnoreColor, &CosmicIgnoreColor::colorPickToggled,
                                this, [this](bool on){
                            if (on) {
                                // Reuse bucket ignore-color eyedropper logic via Editor/ToolManager
                                editor()->tools()->setEyedropperSampleOnly(true);
                                editor()->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_unknown);
                            } else {
                                editor()->tools()->endTemporaryTool();
                                editor()->tools()->setEyedropperSampleOnly(false);
                            }
                        });

                        // Sync with editor-wide updates so the swatch and toggle stay in sync
                        connect(editor(), &Editor::ignoreFillColorChanged, this, [this](QRgb color){
                            if (!mCosmicGapIgnoreColor) return;
                            QColor c = QColor::fromRgba(qUnpremultiply(color));
                            if (!c.isValid()) c = Qt::black;
                            mCosmicGapIgnoreColor->setIgnoreColor(c);
                        });
                        connect(editor(), &Editor::ignoreFillEnabledChanged, this, [this](bool on){
                            if (!mCosmicGapIgnoreColor) return;
                            mCosmicGapIgnoreColor->setIgnoreEnabled(on);
                        });
                    }

                    mLabelGapUseActive   = split.createEntry(tr("Use Active Color"), mCosmicGapUseActiveToggle);
                    mLabelGapIgnoreColor = split.createEntry(tr("Ignore color"), mCosmicGapIgnoreColor);
            }
        }

        // Keep Use Active Color toggle synced with current tool
        bool useActive = false;
        if (currentTool->type() == LASSO_GAP_FILL) useActive = static_cast<LassoGapFillTool*>(currentTool)->useActiveColor();
        if (currentTool->type() == RECT_GAP_FILL)  useActive = static_cast<RectGapFillTool*>(currentTool)->useActiveColor();
        if (mCosmicGapUseActiveToggle) {
            QSignalBlocker b(mCosmicGapUseActiveToggle);
            mCosmicGapUseActiveToggle->setChecked(useActive);
        }
        if (ui->cosmicGapFillOptionsWidget) ui->cosmicGapFillOptionsWidget->show();
    } else {
        if (ui->cosmicGapFillOptionsWidget) ui->cosmicGapFillOptionsWidget->hide();
    }

    refreshOptionsPaneState();
}

void ToolOptionWidget::createUI()
{
    // No-op: PenColorPalette is no longer created here and the layout row is unused
}

void ToolOptionWidget::changeEvent(QEvent* e)
{
    BaseDockWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslateUI();
    }
}

void ToolOptionWidget::retranslateUI()
{
    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    // Window title
    setWindowTitle(safetr("Options"));

    // ui->retranslateUi covers .ui file strings (featherSlider, checkboxes, etc.)
    ui->retranslateUi(mInnerOptionsWidget);

    // BlenderStyleToggle / CosmicCheckboxToggle texts (set programmatically in initUI)
    if (mDrawBehindToggle)      mDrawBehindToggle->setText(safetr("Draw Behind"));
    if (mAntiAliasingToggle)    mAntiAliasingToggle->setText(safetr("Anti-Aliasing"));
    if (mUseFeatherToggle)      mUseFeatherToggle->setText(safetr("Use Feather"));
    if (mShowInfoToggle)        mShowInfoToggle->setText(safetr("Show Size and Diff."));
    if (mFillContourToggle)     mFillContourToggle->setText(safetr("Fill Contour"));
    if (mClosedPathToggle)      mClosedPathToggle->setText(safetr("Closed Path"));
    if (mBezierToggle)          mBezierToggle->setText(safetr("Bézier"));
    if (mInvisibleToggle)       mInvisibleToggle->setText(safetr("Invisible"));
    if (mPreserveAlphaToggle)   mPreserveAlphaToggle->setText(safetr("Alpha"));
    if (mVectorMergeToggle)     mVectorMergeToggle->setText(safetr("Merge"));

    // Tooltips
    if (mCosmicPenSettingsToggle)
        mCosmicPenSettingsToggle->setToolTip(safetr("Pen settings"));
    if (mCosmicGapUseActiveToggle)
        mCosmicGapUseActiveToggle->setToolTip(safetr("Use the currently active color as the gap fill color"));

    // CosmicDropDown options
    if (mCosmicPenModeDropDown) {
        const int idx = mCosmicPenModeDropDown->currentIndex();
        QSignalBlocker b(mCosmicPenModeDropDown);
        mCosmicPenModeDropDown->setOptions({ safetr("Delayed"), safetr("String") });
        mCosmicPenModeDropDown->setCurrentIndex(idx);
    }
    if (mCosmicPencilModeDropDown) {
        const int idx = mCosmicPencilModeDropDown->currentIndex();
        QSignalBlocker b(mCosmicPencilModeDropDown);
        mCosmicPencilModeDropDown->setOptions({ safetr("Delayed"), safetr("String") });
        mCosmicPencilModeDropDown->setCurrentIndex(idx);
    }
    if (mCosmicEyedropperReferenceDropDown) {
        const int idx = mCosmicEyedropperReferenceDropDown->currentIndex();
        QSignalBlocker b(mCosmicEyedropperReferenceDropDown);
        mCosmicEyedropperReferenceDropDown->setOptions({ safetr("From View"), safetr("Current Layer") });
        mCosmicEyedropperReferenceDropDown->setCurrentIndex(idx);
    }
    if (mHybridSnapComboBox) {
        const int idx = mHybridSnapComboBox->currentIndex();
        mHybridSnapComboBox->setOptions({ safetr("Snap:Off"), safetr("Snap:Low"), safetr("Snap:Med"), safetr("Snap:High") });
        mHybridSnapComboBox->setCurrentIndex(idx);
    }

    // CosmicSplitToolLayout row labels (created in initUI, stored as members)
    auto setLbl = [](QLabel* lbl, const char* src) {
        if (lbl) lbl->setText(QString::fromUtf8(src));
    };
    setLbl(mLabelPenSize,          "Size");
    setLbl(mLabelPenMode,          "Mode");
    setLbl(mLabelPenSmooth,        "Smooth");
    setLbl(mLabelPenDrawBehind,    "Draw Behind");
    setLbl(mLabelPenPostCorr,      "Post Correction");
    setLbl(mLabelPencilSize,       "Size");
    setLbl(mLabelPencilPostCorr,   "Post Correction");
    setLbl(mLabelPencilOpacity,    "Opacity");
    setLbl(mLabelPencilMode,       "Mode");
    setLbl(mLabelPencilSmooth,     "Smooth");
    setLbl(mLabelPencilDrawBehind, "Draw Behind");
    setLbl(mLabelEraserSize,       "Size");
    setLbl(mLabelEraserPressure,   "Pressure");
    setLbl(mLabelEraserLasso,      "Lasso Mode");
    setLbl(mLabelCurveSize,        "Size");
    setLbl(mLabelCurveSnap,        "Snap");
    setLbl(mLabelCurveTaper,       "Stroke Taper");
    setLbl(mLabelCurveDrawBehind,  "Draw Behind");
    setLbl(mLabelDplateDrawBehind, "Draw Behind");
    setLbl(mLabelDplateBigBoxes,   "Big Boxes");
    setLbl(mLabelDplateSmallBoxes, "Small Boxes");
    setLbl(mLabelEyedropperRef,    "Reference");
    setLbl(mLabelGapUseActive,     "Use Active Color");
    setLbl(mLabelGapIgnoreColor,   "Ignore color");

    // Apply translated text to the row labels using tr() for proper translation lookup
    // (the setLbl above uses English fallback; now override with actual translations)
    auto trLbl = [this](QLabel* lbl, const char* src) {
        if (!lbl) return;
        const QString t = tr(src);
        if (!t.isEmpty()) lbl->setText(t);
    };
    trLbl(mLabelPenSize,          "Size");
    trLbl(mLabelPenMode,          "Mode");
    trLbl(mLabelPenSmooth,        "Smooth");
    trLbl(mLabelPenDrawBehind,    "Draw Behind");
    trLbl(mLabelPenPostCorr,      "Post Correction");
    trLbl(mLabelPencilSize,       "Size");
    trLbl(mLabelPencilPostCorr,   "Post Correction");
    trLbl(mLabelPencilOpacity,    "Opacity");
    trLbl(mLabelPencilMode,       "Mode");
    trLbl(mLabelPencilSmooth,     "Smooth");
    trLbl(mLabelPencilDrawBehind, "Draw Behind");
    trLbl(mLabelEraserSize,       "Size");
    trLbl(mLabelEraserPressure,   "Pressure");
    trLbl(mLabelEraserLasso,      "Lasso Mode");
    trLbl(mLabelCurveSize,        "Size");
    trLbl(mLabelCurveSnap,        "Snap");
    trLbl(mLabelCurveTaper,       "Stroke Taper");
    trLbl(mLabelCurveDrawBehind,  "Draw Behind");
    trLbl(mLabelDplateDrawBehind, "Draw Behind");
    trLbl(mLabelDplateBigBoxes,   "Big Boxes");
    trLbl(mLabelDplateSmallBoxes, "Small Boxes");
    trLbl(mLabelEyedropperRef,    "Reference");
    trLbl(mLabelGapUseActive,     "Use Active Color");
    trLbl(mLabelGapIgnoreColor,   "Ignore color");
}

void ToolOptionWidget::makeConnectionToEditor(Editor* editor)
{
    auto toolManager = editor->tools();

    connect(ui->useBezierBox, &QCheckBox::clicked, toolManager, &ToolManager::setBezier);
    connect(ui->useClosedPathBox, &QCheckBox::clicked, toolManager, &ToolManager::setClosedPath);
    // New Blender-style toggles
    if (mBezierToggle)          connect(mBezierToggle, &BlenderStyleToggle::toggled,      toolManager, &ToolManager::setBezier);
    if (mClosedPathToggle)      connect(mClosedPathToggle, &BlenderStyleToggle::toggled,  toolManager, &ToolManager::setClosedPath);
    // Pressure checkbox is deprecated in favor of curve enable flags; disconnect/remove
    if (ui->usePressureBox) {
        connect(ui->usePressureBox, &QCheckBox::clicked, toolManager, &ToolManager::setPressure);
    }
    connect(ui->makeInvisibleBox, &QCheckBox::clicked, toolManager, &ToolManager::setInvisibility);
    connect(ui->preserveAlphaBox, &QCheckBox::clicked, toolManager, &ToolManager::setPreserveAlpha);
    if (mInvisibleToggle)       connect(mInvisibleToggle, &BlenderStyleToggle::toggled,   toolManager, &ToolManager::setInvisibility);
    if (mPreserveAlphaToggle)   connect(mPreserveAlphaToggle, &BlenderStyleToggle::toggled, toolManager, &ToolManager::setPreserveAlpha);

    // Pen cosmic width field: mirror behavior by directly driving WIDTH.
    if (mPenWidthField) {
        connect(mPenWidthField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
                toolManager->setWidth(v);
            }
        });

        connect(mPenWidthField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->endEditingProperties();
            }
        });
    }
    
    // Legacy Pencil opacity / stabiliser sliders are deprecated in favor of the
    // Cosmic Pencil split layout; keep their logic but do not surface the old UI.
    if (mPencilOpacitySlider) {
        connect(mPencilOpacitySlider, &BlenderStyleSlider::valueChanged, this, [editor](double value) {
            if (editor && editor->tools()) {
                if (auto* pencil = qobject_cast<PencilTool*>(editor->tools()->getTool(PENCIL))) {
                    pencil->setPencilOpacity(value / 100.0);  // Convert 0-100 to 0-1
                }
            }
        });
    }

    if (mPencilStabilizerSlider) {
        connect(mPencilStabilizerSlider, &BlenderStyleSlider::valueChanged, this, [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
            }
            toolManager->setWeightedDistance(v);
        });
    }

    connect(ui->featherSlider, &SpinSlider::sliderPressed, this, [this, toolManager, editor]() {
        if (editor && editor->tools() && editor->tools()->currentTool()) {
            toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
        }
    });
    // Emit continuously but persistence is deferred until sliderReleased
    connect(ui->featherSlider, &SpinSlider::valueChanged, toolManager, &ToolManager::setFeather);
    connect(ui->featherSlider, &SpinSlider::sliderReleased, this, [toolManager]() {
        toolManager->endEditingProperties();
    });

    // Removed numeric spin boxes; Blender-style sliders support inline numeric entry

    connect(ui->useFeatherBox, &QCheckBox::clicked, toolManager, &ToolManager::setUseFeather);
    if (mUseFeatherToggle)      connect(mUseFeatherToggle, &BlenderStyleToggle::toggled,  toolManager, &ToolManager::setUseFeather);

    connect(ui->vectorMergeBox, &QCheckBox::clicked, toolManager, &ToolManager::setVectorMergeEnabled);
    if (mVectorMergeToggle)     connect(mVectorMergeToggle, &BlenderStyleToggle::toggled, toolManager, &ToolManager::setVectorMergeEnabled);
    
    // Anti-aliasing toggle (new BlenderStyleToggle)
    if (mAntiAliasingToggle) {
        connect(mAntiAliasingToggle, &BlenderStyleToggle::toggled, this, [toolManager](bool on){
            toolManager->setAA(on ? 1 : 0);
        });
    }
    // Keep old checkbox connection as fallback (hidden but functional)
    connect(ui->useAABox, &QCheckBox::clicked, toolManager, &ToolManager::setAA);

    // New: Eraser lasso mode toggle
    if (auto lassoBox = this->findChild<QCheckBox*>("eraserLassoModeBox")) {
        connect(lassoBox, &QCheckBox::clicked, toolManager, &ToolManager::setEraserLassoMode);
    }
    if (mEraserLassoToggle) {
        connect(mEraserLassoToggle, &CosmicCheckboxToggle::toggled, toolManager, &ToolManager::setEraserLassoMode);
    }

    // Base Stabiliser removed - always set to None (0)

    connect(ui->fillContourBox, &QCheckBox::clicked, toolManager, &ToolManager::setUseFillContour);
    if (mFillContourToggle)     connect(mFillContourToggle, &BlenderStyleToggle::toggled, toolManager, &ToolManager::setUseFillContour);

    // Draw behind toggle (global across tools)
    if (mDrawBehindToggle) {
        connect(mDrawBehindToggle, &CosmicCheckboxToggle::toggled, toolManager, &ToolManager::setDrawBehind);
    }

    connect(ui->showInfoBox, &QCheckBox::clicked, toolManager, &ToolManager::setShowSelectionInfo);
    if (mShowInfoToggle)        connect(mShowInfoToggle, &BlenderStyleToggle::toggled,    toolManager, &ToolManager::setShowSelectionInfo);

    // Commit-on-demand helper removed: most ToolManager setters persist immediately
    // (commit via ToolManager) so explicit commitCurrentTool calls are unnecessary.

    // Krita-like modes connections
    if (mPenSmoothingComboBox) {
        connect(mPenSmoothingComboBox, &BlenderStyleComboBox::currentIndexChanged, this, [toolManager](int index) {
            // Convert combo box index to mode: 0=WEIGHTED, 1=PULL
            int mode = (index == 1) ? static_cast<int>(PenSmoothingMode::PULL) : static_cast<int>(PenSmoothingMode::WEIGHTED);
            toolManager->setPenSmoothingMode(mode);
        });
    }
    
    // Legacy Weighted Distance / Pull delay controls have been removed;
    // these properties are now driven by the Cosmic Pen "Smooth" field.
    // Removed finish line / sensors / scalable

    // Curve taper connections (Fixed Pixels only, 0-200px, shared between Curve and BSpline)
    if (mCurveTapperSliders) {
        connect(mCurveTapperSliders, &CosmicTapperSliders::interactionBegun, this, [this, toolManager, editor]() {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
            }
        });
        connect(mCurveTapperSliders, &CosmicTapperSliders::interactionEnded, this, [toolManager]() {
            toolManager->endEditingProperties();
        });
        connect(mCurveTapperSliders, &CosmicTapperSliders::startTaperChanged, toolManager, &ToolManager::setCurveTaperStart);
        connect(mCurveTapperSliders, &CosmicTapperSliders::endTaperChanged, toolManager, &ToolManager::setCurveTaperEnd);
    }

    // CosmicSplitLayout: Curve cosmic taper connections (bitmap Curve tool only).
    if (mCosmicCurveTapperSliders) {
        connect(mCosmicCurveTapperSliders, &CosmicTapperSliders::interactionBegun, this, [this, toolManager, editor]() {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
            }
        });
        connect(mCosmicCurveTapperSliders, &CosmicTapperSliders::interactionEnded, this, [toolManager]() {
            toolManager->endEditingProperties();
        });
        connect(mCosmicCurveTapperSliders, &CosmicTapperSliders::startTaperChanged, toolManager, &ToolManager::setCurveTaperStart);
        connect(mCosmicCurveTapperSliders, &CosmicTapperSliders::endTaperChanged, toolManager, &ToolManager::setCurveTaperEnd);
    }


    // Conditional visibility on mode change
    if (mPenSmoothingComboBox) {
        connect(mPenSmoothingComboBox, &BlenderStyleComboBox::currentIndexChanged, this, [this, editor](int index) {
            // Convert combo box index to mode: 0=WEIGHTED, 1=PULL
            int mode = (index == 1) ? static_cast<int>(PenSmoothingMode::PULL) : static_cast<int>(PenSmoothingMode::WEIGHTED);
            
            // Mutual exclusion: disable Post-Correction when Pull is active
            if (mPostCorrBox) {
                bool isPull = (mode == static_cast<int>(PenSmoothingMode::PULL));
                mPostCorrBox->setEnabled(!isPull);
                if (isPull) {
                    // Force Post-Correction to Off when switching to Pull
                    QSignalBlocker b(mPostCorrBox);
                    mPostCorrBox->setValue(0);
                    editor->tools()->setPostCorrectionLevel(0);
                }
            }
        });
    }

    // CosmicSplitLayout: wire Pen Cosmic controls to existing tool properties.
    if (mCosmicPenSizeField) {
        connect(mCosmicPenSizeField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
                toolManager->setWidth(v);
            }
        });

        connect(mCosmicPenSizeField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->endEditingProperties();
            }
        });
    }

    if (mCosmicPenModeDropDown) {
        connect(mCosmicPenModeDropDown, &CosmicDropDown::currentIndexChanged, this,
                [this, toolManager](int index) {
            // 0 = Delayed (Weighted), 1 = String (Pull)
            int mode = static_cast<int>(PenSmoothingMode::WEIGHTED);
            if (index == 1) {
                mode = static_cast<int>(PenSmoothingMode::PULL);
            }
            toolManager->setPenSmoothingMode(mode);

            // Smooth UI is the single source of truth; re-apply current UI value to the tool.
            const int ui = qBound(1, mUnifiedSmoothUiValue, 10);
            const double legacy = (ui <= 1) ? 0.0 : 300.0 * qPow((double(ui - 2) / 8.0), 2.0);
            toolManager->setWeightedDistance(legacy);
            toolManager->setStab2Delay(legacy);

            // Force visuals to refresh immediately.
            updateUI();

            Q_UNUSED(mode);
        });
    }

    if (mCosmicPenSmoothField) {
        connect(mCosmicPenSmoothField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (!editor || !editor->tools()) {
                return;
            }

            const int ui = qBound(1, int(qRound(v)), 10);
            mUnifiedSmoothUiValue = ui;
            mUnifiedSmoothUiInitialized = true;

            const double legacy = (ui <= 1) ? 0.0 : 300.0 * qPow((double(ui - 2) / 8.0), 2.0);
            toolManager->setWeightedDistance(legacy);
            toolManager->setStab2Delay(legacy);
        });

        connect(mCosmicPenSmoothField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double) {
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            toolManager->endEditingProperties();
        });
    }

    if (mCosmicPenDrawBehindToggle) {
        connect(mCosmicPenDrawBehindToggle, &CosmicCheckboxToggle::toggled, this,
                [toolManager](bool on) {
            toolManager->setDrawBehind(on);
        });
    }

    if (mCosmicPenPostIntensityBox) {
        connect(mCosmicPenPostIntensityBox, &CosmicIntensityBox::valueChanged, this,
                [editor](int level){
            if (!editor || !editor->tools()) {
                return;
            }

            int clamped = qBound(0, level, 3);
            editor->tools()->setPostCorrectionLevel(clamped);
        });
    }

    if (mCosmicPenSettingsToggle) {
        if (!mCosmicPenSettingsPopup) {
            mCosmicPenSettingsPopup = new BlenderStyleModalPopup(this);
            mCosmicPenSettingsPopup->setAccentColor(BlenderStyleSlider::getAccentColor());
            mCosmicPenSettingsPopup->setBorderWidth(4);
            mCosmicPenSettingsPopup->setGlowIntensity(0.9);
            mCosmicPenSettingsPopup->setControllingToggle(mCosmicPenSettingsToggle);
            mCosmicPenSettingsPopup->hide();
        }

        connect(mCosmicPenSettingsToggle, &CosmicCheckboxToggle::toggled, this,
                [this, editor, toolManager](bool checked) {
            if (!editor || !editor->tools()) {
                return;
            }

            if (checked) {
                if (editor->tools()->currentTool()->type() != PEN) {
                    QSignalBlocker b(mCosmicPenSettingsToggle);
                    mCosmicPenSettingsToggle->setChecked(false);
                    return;
                }

                if (!mCosmicPenSettingsPopup) {
                    QSignalBlocker b(mCosmicPenSettingsToggle);
                    mCosmicPenSettingsToggle->setChecked(false);
                    return;
                }

                mCosmicPenSettingsPopup->disconnect();

                QLayout* existingLayout = mCosmicPenSettingsPopup->layout();
                if (existingLayout) {
                    while (QLayoutItem* item = existingLayout->takeAt(0)) {
                        if (QWidget* widget = item->widget()) {
                            widget->deleteLater();
                        }
                        delete item;
                    }
                    delete existingLayout;
                }

                QWidget* contentWidget = new QWidget(mCosmicPenSettingsPopup);
                contentWidget->setObjectName("cosmicPenSettingsPopupContent");
                auto* contentLayout = new QVBoxLayout(contentWidget);
                const int borderMargin = 16;
                contentLayout->setContentsMargins(borderMargin, borderMargin, borderMargin, borderMargin);
                contentLayout->setSpacing(8);

                auto* pressureToggle = new CosmicCheckboxToggle(contentWidget);
                pressureToggle->setObjectName("cosmicPenPressureEnableToggle");
                pressureToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
                pressureToggle->setScaleMode(CosmicCheckboxToggle::ScaleBigBox);
                pressureToggle->setShowText(true);
                pressureToggle->setText(tr("Pressure"));
                pressureToggle->setAccentColor(BlenderStyleSlider::getAccentColor());

                bool pressureOn = false;
                bool supportsPressure = false;
                if (BaseTool* t = editor->tools()->currentTool()) {
                    pressureOn = (t->properties.pressure > 0);
                    supportsPressure = t->isPropertyEnabled(PRESSURE);
                }
                {
                    QSignalBlocker b(pressureToggle);
                    pressureToggle->setChecked(pressureOn);
                    pressureToggle->setEnabled(supportsPressure);
                }
                contentLayout->addWidget(pressureToggle);

                auto* label = new QLabel(tr("Pressure curve"), contentWidget);
                contentLayout->addWidget(label);

                auto* curveEditor = new BezierCurveEditor(contentWidget);
                curveEditor->setObjectName("cosmicPenPressureCurveEditor");
                curveEditor->setMinimumSize(220, 220);
                contentLayout->addWidget(curveEditor);

                QVector<QPointF> points;
                if (auto* pen = qobject_cast<PenTool*>(editor->tools()->getTool(PEN))) {
                    points = pen->getPressureCurve();
                }
                curveEditor->setControlPoints(points);

                curveEditor->setEnabled(pressureOn);
                label->setEnabled(pressureOn);

                connect(pressureToggle, &CosmicCheckboxToggle::toggled, this,
                        [toolManager, curveEditor, label](bool on) {
                    toolManager->setPressure(on);
                    if (curveEditor) curveEditor->setEnabled(on);
                    if (label) label->setEnabled(on);
                });

                connect(curveEditor, &BezierCurveEditor::curveChanged, this,
                        [toolManager](const QVector<QPointF>& newPoints) {
                    toolManager->setPenPressureCurve(newPoints);
                });

                connect(mCosmicPenSettingsPopup, &QDialog::finished, this, [this]() {
                    if (mCosmicPenSettingsToggle && mCosmicPenSettingsToggle->isChecked()) {
                        QSignalBlocker b(mCosmicPenSettingsToggle);
                        mCosmicPenSettingsToggle->setChecked(false);
                    }
                });

                QVBoxLayout* popupLayout = new QVBoxLayout();
                popupLayout->setContentsMargins(0, 0, 0, 0);
                popupLayout->setSpacing(0);
                popupLayout->addWidget(contentWidget);
                mCosmicPenSettingsPopup->setLayout(popupLayout);

                mCosmicPenSettingsPopup->adjustSize();
                QPoint gpos = mCosmicPenSettingsToggle->mapToGlobal(QPoint(0, mCosmicPenSettingsToggle->height()));
                mCosmicPenSettingsPopup->move(gpos);
                mCosmicPenSettingsPopup->show();
                mCosmicPenSettingsPopup->raise();
                mCosmicPenSettingsPopup->activateWindow();
            } else {
                if (mCosmicPenSettingsPopup && mCosmicPenSettingsPopup->isVisible()) {
                    mCosmicPenSettingsPopup->accept();
                }
            }
        });
    }

    // CosmicSplitLayout: wire Pencil Cosmic controls to existing tool properties.
    if (mCosmicPencilSizeField) {
        connect(mCosmicPencilSizeField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            const ToolType tt = editor->tools()->currentTool()->type();
            if (tt != PENCIL && tt != PEN) {
                return;
            }
            toolManager->beginEditingPropertiesForTool(tt);
            toolManager->setWidth(v);
        });

        connect(mCosmicPencilSizeField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double v) {
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            BaseTool* ct = editor->tools()->currentTool();
            const ToolType tt = ct ? ct->type() : PENCIL;
            if (tt != PENCIL && tt != PEN) {
                return;
            }
            toolManager->endEditingProperties();

            const ToolType other = (tt == PEN) ? PENCIL : PEN;
            mirrorUnifiedStrokeSettings(other, tt);
        });
    }

    if (mCosmicPencilOpacityField) {
        connect(mCosmicPencilOpacityField, &CosmicInputField::valueChanged, this,
                [editor](double v) {
            if (!editor || !editor->tools()) {
                return;
            }
            // Opacity is per brush preset (Pencil only)
            const QString presetId = activeBrushFactoryPresetId();
            const int percent = qBound(0, int(qRound(v)), 100);
            if (auto* pencil = qobject_cast<PencilTool*>(editor->tools()->getTool(PENCIL))) {
                // Convert percent [0,100] -> [0,1]
                const double op = qBound(0.0, percent / 100.0, 1.0);
                pencil->setPencilOpacity(op);
            }
        });

        connect(mCosmicPencilOpacityField, &CosmicInputField::valueCommitted, this,
                [editor](double v) {
            if (!editor || !editor->tools()) {
                return;
            }
            const QString presetId = activeBrushFactoryPresetId();
            if (presetId.isEmpty()) {
                return;
            }
            const int percent = qBound(0, int(qRound(v)), 100);
            QSettings s(PENCIL2D, PENCIL2D);
            s.beginGroup("PencilBrushFactoryOpacityById");
            s.beginGroup(presetId);
            s.setValue("OpacityPercent", percent);
            s.endGroup();
            s.endGroup();
            s.sync();
        });
    }

    if (mCosmicPencilModeDropDown) {
        connect(mCosmicPencilModeDropDown, &CosmicDropDown::currentIndexChanged, this,
                [this, toolManager, editor](int index) {
            if (!editor || !editor->tools()) {
                return;
            }

            BaseTool* ct = editor->tools()->currentTool();
            if (!ct) {
                return;
            }
            const ToolType tt = ct->type();
            if (tt != PENCIL && tt != PEN) {
                return;
            }

            toolManager->beginEditingPropertiesForTool(tt);

            // 0 = Delayed (Weighted), 1 = String (Pull)
            int mode = static_cast<int>(PenSmoothingMode::WEIGHTED);
            if (index == 1) {
                mode = static_cast<int>(PenSmoothingMode::PULL);
            }
            toolManager->setPenSmoothingMode(mode);
            toolManager->endEditingProperties();

            // Smooth UI is the single source of truth; re-apply current UI value to the tool.
            const int ui = qBound(1, mUnifiedSmoothUiValue, 10);
            const double legacy = (ui <= 1) ? 0.0 : 300.0 * qPow((double(ui - 2) / 8.0), 2.0);
            toolManager->setWeightedDistance(legacy);
            toolManager->setStab2Delay(legacy);

            updateUI();

            const ToolType other = (tt == PEN) ? PENCIL : PEN;
            mirrorUnifiedStrokeSettings(other, tt);
        });
    }

    if (mCosmicPencilSmoothField) {
        connect(mCosmicPencilSmoothField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (!editor || !editor->tools()) {
                return;
            }

            BaseTool* ct = editor->tools()->currentTool();
            if (!ct) {
                return;
            }
            const ToolType tt = ct->type();
            if (tt != PENCIL && tt != PEN) {
                return;
            }

            const int ui = qBound(1, int(qRound(v)), 10);
            mUnifiedSmoothUiValue = ui;
            mUnifiedSmoothUiInitialized = true;

            const double legacy = (ui <= 1) ? 0.0 : 300.0 * qPow((double(ui - 2) / 8.0), 2.0);
            toolManager->beginEditingPropertiesForTool(tt);
            toolManager->setWeightedDistance(legacy);
            toolManager->setStab2Delay(legacy);
            toolManager->beginEditingPropertiesForTool(tt);
            toolManager->setWeightedDistance(legacy);
            toolManager->setStab2Delay(legacy);
        });

        connect(mCosmicPencilSmoothField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double) {
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            BaseTool* ct = editor->tools()->currentTool();
            const ToolType tt = ct ? ct->type() : PENCIL;
            if (tt != PENCIL && tt != PEN) {
                return;
            }
            toolManager->endEditingProperties();

            const ToolType other = (tt == PEN) ? PENCIL : PEN;
            mirrorUnifiedStrokeSettings(other, tt);
        });
    }

    if (mCosmicPencilPostIntensityBox) {
        connect(mCosmicPencilPostIntensityBox, &CosmicIntensityBox::valueChanged, this,
                [this, editor](int level){
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }

            BaseTool* ct = editor->tools()->currentTool();
            const ToolType tt = ct->type();
            if (tt != PEN) {
                return;
            }

            // Post Correction is Pen-only.
            if (tt != PEN) {
                return;
            }

            const int clamped = qBound(0, level, 3);

            // Commit through the tool property pipeline to ensure the value is applied
            // consistently (and persists across tool switches).
            ToolManager* tm = editor->tools();
            tm->beginEditingPropertiesForTool(PEN);
            tm->setPostCorrectionLevel(clamped);
            tm->endEditingProperties();

            // Mirror to Pencil (will clamp to 0 by design).
            mirrorUnifiedStrokeSettings(PENCIL, PEN);
        });
    }


    if (mCosmicPencilDrawBehindToggle) {
        connect(mCosmicPencilDrawBehindToggle, &CosmicCheckboxToggle::toggled, this,
                [this, toolManager, editor](bool on) {
            toolManager->setDrawBehind(on);

            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            const ToolType tt = editor->tools()->currentTool()->type();
            if (tt == PEN || tt == PENCIL) {
                const ToolType other = (tt == PEN) ? PENCIL : PEN;
                mirrorUnifiedStrokeSettings(other, tt);
            }
        });
    }

    // CosmicSplitLayout: wire Eraser cosmic controls to existing tool properties.
    if (mCosmicEraserSizeField) {
        connect(mCosmicEraserSizeField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
                toolManager->setWidth(v);
            }
        });

        connect(mCosmicEraserSizeField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double v) {
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            toolManager->endEditingProperties();
        });
    }
    if (mCosmicEraserPressureToggle) {
        connect(mCosmicEraserPressureToggle, &CosmicCheckboxToggle::toggled, this,
                [toolManager](bool on) {
            toolManager->setPressure(on ? 1 : 0);
        });
    }

    // CosmicSplitLayout: wire Curve cosmic controls to existing tool properties.
    if (mCosmicCurveWidthField) {
        connect(mCosmicCurveWidthField, &CosmicInputField::valueChanged, this,
                [this, toolManager, editor](double v) {
            if (editor && editor->tools() && editor->tools()->currentTool()) {
                toolManager->beginEditingPropertiesForTool(editor->tools()->currentTool()->type());
                toolManager->setWidth(v);
            }
        });

        connect(mCosmicCurveWidthField, &CosmicInputField::valueCommitted, this,
                [this, toolManager, editor](double v) {
            if (!editor || !editor->tools() || !editor->tools()->currentTool()) {
                return;
            }
            toolManager->endEditingProperties();
        });
    }

    if (mCosmicHybridSnapBox) {
        connect(mCosmicHybridSnapBox, &CosmicIntensityBox::valueChanged, this,
                [toolManager](int level) {
            int clamped = qBound(0, level, 3);
            toolManager->setHybridSnapLevel(clamped);
        });
    }

    if (mCosmicCurveDrawBehindToggle) {
        connect(mCosmicCurveDrawBehindToggle, &CosmicCheckboxToggle::toggled, this,
                [toolManager](bool on) {
            toolManager->setDrawBehind(on);
        });
    }

    if (mCosmicDplateDrawBehindToggle) {
        connect(mCosmicDplateDrawBehindToggle, &CosmicCheckboxToggle::toggled, this,
                [toolManager](bool on) {
            toolManager->setDrawBehind(on);
        });
    }

    // Dplate box count controls
    if (mCosmicDplateBigBoxField) {
        connect(mCosmicDplateBigBoxField, &CosmicIntensityBox::valueChanged, this,
                [editor](int v) {
            if (!editor || !editor->tools()) return;
            auto* tool = editor->tools()->currentTool();
            if (!tool || tool->type() != DPLATE) return;
            auto* dplate = static_cast<DplateTool*>(tool);
            dplate->setBigBoxCount(v);
            if (dplate->isActive()) {
                dplate->showCenterPreview(tool->properties.width);
            }
        });
    }

    if (mCosmicDplateSmallBoxBox) {
        connect(mCosmicDplateSmallBoxBox, &CosmicIntensityBox::valueChanged, this,
                [editor](int v) {
            if (!editor || !editor->tools()) return;
            auto* tool = editor->tools()->currentTool();
            if (!tool || tool->type() != DPLATE) return;
            auto* dplate = static_cast<DplateTool*>(tool);
            dplate->setSmallBoxCount(v);
            if (dplate->isActive()) {
                dplate->showCenterPreview(tool->properties.width);
            }
        });
    }

    // PenColorPalette connections removed; handled by canvas overlay

    // Dplate tool uses the active color directly; no palette connection needed

    // Keep size slider available except where explicitly hidden per tool in setVisibility()

    connect(toolManager, &ToolManager::toolChanged, this, &ToolOptionWidget::onToolChanged);
    connect(toolManager, &ToolManager::toolPropertyChanged, this, &ToolOptionWidget::onToolPropertyChanged);


    // Safeguard: ensure any in-progress property edits are ended when the active tool changes.
    connect(toolManager, &ToolManager::toolChanged, this, [toolManager](ToolType){ toolManager->endEditingProperties(); });

    // Create Adjust Pressure toggle and persistent popup
    if (!mAdjustPressureToggle) {
        mAdjustPressureToggle = new BlenderStyleToggle(this);
        mAdjustPressureToggle->setObjectName("adjustPressureToggle");
        mAdjustPressureToggle->setText(tr("Adjust"));
        mAdjustPressureToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        
        // Add to layout (will manage visibility per tool)
        ui->verticalLayout_2->addWidget(mAdjustPressureToggle);
        
        // Create persistent popup (but don't show yet)
        mAdjustPressurePopup = new BlenderStyleModalPopup(this);
        mAdjustPressurePopup->setAccentColor(BlenderStyleSlider::getAccentColor());
        mAdjustPressurePopup->setBorderWidth(4);
        mAdjustPressurePopup->setGlowIntensity(0.9);
        mAdjustPressurePopup->setControllingToggle(mAdjustPressureToggle); // Allow toggle interaction!
        mAdjustPressurePopup->hide();
        
        connect(mAdjustPressureToggle, &BlenderStyleToggle::toggled, this, [this, editor](bool checked) {
            if (checked) {
                // Show popup when toggle is ON
                const ToolType currentType = editor->tools()->currentTool()->type();
                if (currentType != PEN) {
                    qWarning() << "Adjust pressure toggle activated but not on PEN tool";
                    QSignalBlocker b(mAdjustPressureToggle);
                    mAdjustPressureToggle->setChecked(false);
                    return;
                }
                
                // GUARD: Ensure popup exists
                if (!mAdjustPressurePopup) {
                    qCritical() << "Adjust pressure popup is null!";
                    return;
                }
                
                // Disconnect all previous connections to avoid duplicates
                mAdjustPressurePopup->disconnect();
                
                // Properly clear existing layout and content
                QLayout* existingLayout = mAdjustPressurePopup->layout();
                if (existingLayout) {
                    // Delete all widgets in the layout
                    while (QLayoutItem* item = existingLayout->takeAt(0)) {
                        if (QWidget* widget = item->widget()) {
                            widget->deleteLater();
                        }
                        delete item;
                    }
                    // Delete the layout itself
                    delete existingLayout;
                }
                
                // GUARD: Double-check layout is cleared
                if (mAdjustPressurePopup->layout()) {
                    qCritical() << "Failed to clear popup layout!";
                    return;
                }
                
                // Build popup content with proper margins for the glowing border
                QWidget* contentWidget = new QWidget(mAdjustPressurePopup);
                contentWidget->setObjectName("popupContent");
                auto* contentLayout = new QVBoxLayout(contentWidget);
                // Add extra margin to account for glow effect
                int borderMargin = 16; // Enough space for 4px border + glow
                contentLayout->setContentsMargins(borderMargin, borderMargin, borderMargin, borderMargin);
                contentLayout->setSpacing(8);
                
                auto* label = new QLabel(tr("Pressure curve"), contentWidget);
                label->setObjectName("pressureLabel");
                contentLayout->addWidget(label);
                
                auto* curveEditor = new BezierCurveEditor(contentWidget);
                curveEditor->setObjectName("curvEditor");
                curveEditor->setMinimumSize(200, 200);
                contentLayout->addWidget(curveEditor);
                
                // GUARD: Verify widgets were created
                if (!label || !curveEditor) {
                    qCritical() << "Failed to create popup widgets!";
                    delete contentWidget;
                    return;
                }
                
                // Initialize from PenTool stored curve
                QVector<QPointF> points;
                if (auto* pen = qobject_cast<PenTool*>(editor->tools()->getTool(PEN))) {
                    points = pen->getPressureCurve();
                    curveEditor->setControlPoints(points);
                } else {
                    qWarning() << "Could not get PenTool for pressure curve initialization";
                    points.append(QPointF(0.5, 0.5)); // default single point
                }
                
                // State management
                struct PenCurveState { QVector<QPointF> points; };
                auto state = std::make_shared<PenCurveState>(PenCurveState{points});
                
                connect(curveEditor, &BezierCurveEditor::curveChanged, this, [this, editor, state](const QVector<QPointF>& newPoints){ 
                    state->points = newPoints; 
                    // LIVE-APPLY to PenTool and persist for consistency
                    if (editor && editor->tools()) {
                        editor->tools()->setPenPressureCurve(newPoints);
                    }
                });
                
                // Save when popup is hidden
                connect(mAdjustPressurePopup, &QDialog::finished, this, [this, editor, state]() {
                    editor->tools()->setPenPressureCurve(state->points);
                    if (auto* pen = qobject_cast<PenTool*>(editor->tools()->getTool(PEN))) {
                        // Always keep the pressure curve enabled; the Width-side
                        // pressure toggle now controls whether pressure affects width.
                        pen->setPressureCurveEnabled(true);
                        pen->saveSettings();
                    } else {
                        QSettings settings(PENCIL2D, PENCIL2D);
                        settings.setValue("penPressureCurveEnabled", true);
                        settings.sync();
                    }
                    // Turn off toggle when popup closes
                    if (mAdjustPressureToggle && mAdjustPressureToggle->isChecked()) {
                        QSignalBlocker b(mAdjustPressureToggle);
                        mAdjustPressureToggle->setChecked(false);
                    }
                });
                
                // Create main layout for the popup (no margins on popup itself)
                QVBoxLayout* popupLayout = new QVBoxLayout();
                popupLayout->setContentsMargins(0, 0, 0, 0);
                popupLayout->setSpacing(0);
                popupLayout->addWidget(contentWidget);
                
                // Set layout and verify
                mAdjustPressurePopup->setLayout(popupLayout);
                
                // GUARD: Verify layout was set
                if (!mAdjustPressurePopup->layout()) {
                    qCritical() << "Failed to set popup layout!";
                    delete popupLayout;
                    delete contentWidget;
                    return;
                }
                
                // Make sure content widget is visible
                contentWidget->show();
                
                // Adjust size to fit content
                mAdjustPressurePopup->adjustSize();
                
                // GUARD: Verify popup has reasonable size
                QSize popupSize = mAdjustPressurePopup->size();
                if (popupSize.width() < 50 || popupSize.height() < 50) {
                    qWarning() << "Popup size suspiciously small:" << popupSize;
                    mAdjustPressurePopup->resize(250, 300);
                }
                
                // Position near toggle
                QPoint gpos = mAdjustPressureToggle->mapToGlobal(QPoint(0, mAdjustPressureToggle->height()));
                mAdjustPressurePopup->move(gpos);
                
                // Show and verify
                mAdjustPressurePopup->show();
                mAdjustPressurePopup->raise();
                mAdjustPressurePopup->activateWindow();
                
                qDebug() << "Adjust pressure popup shown successfully at" << gpos << "size:" << mAdjustPressurePopup->size();
                
            } else {
                // Hide popup when toggle is OFF
                if (mAdjustPressurePopup && mAdjustPressurePopup->isVisible()) {
                    qDebug() << "Hiding adjust pressure popup";
                    // Use accept() so QDialog emits finished() and connected save logic runs
                    mAdjustPressurePopup->accept();
                }
            }
        });
    }
    
    
    // Legacy Adjust Pressure button has been removed; pressure curve
    // editing is now handled via the new Adjust/Cosmic controls.
    
    // Connect to preference changes for real-time updates
    if (editor && editor->preference()) {
        connect(editor->preference(), &PreferenceManager::optionChanged, this, &ToolOptionWidget::onColorPreferenceChanged);
    }
    
    // Initialize accent colors
    updateAccentColors();
}

void ToolOptionWidget::onToolPropertyChanged(ToolType sourceToolType, ToolPropertyType ePropertyType)
{
    // Strict guard: only respond to property change notifications that are
    // emitted for the currently active tool
    BaseTool* active = editor()->tools()->currentTool();
    if (!active) return;

    if (sourceToolType != active->type()) {
            return;
    }

    const Properties& p = active->properties;

    switch (ePropertyType)
    {
    case WIDTH: 
    {
        setPenWidth(p.width);
        break;
    }
    case FEATHER: setPenFeather(p.feather); break;
    case USEFEATHER: setUseFeather(p.useFeather); break;
    case INVISIBILITY: setPenInvisibility(p.invisibility); break;
    case PRESERVEALPHA: setPreserveAlpha(p.preserveAlpha); break;
    case PRESSURE: setPressure(p.pressure); break;
    case VECTORMERGE: setVectorMergeEnabled(p.vectorMergeEnabled); break;
    case ANTI_ALIASING: setAA(p.useAA); break;
    case STABILIZATION: setStabilizerLevel(p.stabilizerLevel); break;
    case FILLCONTOUR: setFillContour(p.useFillContour); break;
    case SHOWSELECTIONINFO: setShowSelectionInfo(p.showSelectionInfo); break;
    case BEZIER: setBezier(p.bezier_state); break;
    case CLOSEDPATH: setClosedPath(p.closedPolylinePath); break;
    case DRAW_BEHIND: setDrawBehind(p.drawBehind); break;
    case PEN_SMOOTHING_MODE: {
        // Mode changes are reflected via the Cosmic Pen UI only.
        break;
    }
    case WEIGHTED_DISTANCE: {
        // Weighted distance is represented in the Cosmic Pen "Smooth" UI.
        break;
    }
    case STAB2_DELAY: {
        // Pull delay is represented in the Cosmic Pen "Smooth" UI.
        break;
    }
    case STAB2_FINISH_LINE: {
        break;
    }
    case STAB2_STABILIZE_SENSORS: {
        break;
    }
    case STAB2_SCALABLE_DISTANCE: {
        break;
    }
    case POST_CORRECTION_LEVEL: {
        updateUI();
        break;
    }
    case WEIGHTED_STROKE_ENDING: {
        updateUI();
        break;
    }
    case WEIGHTED_SMOOTH_PRESSURE: {
        updateUI();
        break;
    }
    case WEIGHTED_SCALABLE_DISTANCE: {
        updateUI();
        break;
    }
    case STAB2_DISTANCE: {
        updateUI();
        break;
    }
    case PRESSURE_SMOOTHING_AMOUNT: {
        updateUI();
        break;
    }
    case PRESSURE_TAPERING_START: {
        updateUI();
        break;
    }
    case PRESSURE_TAPERING_END: {
        updateUI();
        break;
    }
    case BUCKET_GAP_FILL_MODE: {
        updateUI();
        break;
    }
    case EYEDROPPER_REFERENCE_MODE: {
        updateUI();
        break;
    }
    case HYBRID_SNAP_LEVEL: {
        if (mHybridSnapComboBox) {
            QSignalBlocker b(mHybridSnapComboBox);
            mHybridSnapComboBox->setCurrentIndex(qBound(0, p.hybridSnapLevel, 3));
        }
        if (mCosmicHybridSnapBox) {
            QSignalBlocker b(mCosmicHybridSnapBox);
            int clamped = qBound(0, p.hybridSnapLevel, 3);
            mCosmicHybridSnapBox->setValue(clamped);
        }
        break;
    }
    case CURVE_TAPER_START: {
        if (mCurveTapperSliders) {
            mCurveTapperSliders->setStartValue(p.curveTaperStart);
        }
        if (mCosmicCurveTapperSliders) {
            mCosmicCurveTapperSliders->setStartValue(p.curveTaperStart);
        }
        break;
    }
    case CURVE_TAPER_END: {
        if (mCurveTapperSliders) {
            mCurveTapperSliders->setEndValue(p.curveTaperEnd);
        }
        if (mCosmicCurveTapperSliders) {
            mCosmicCurveTapperSliders->setEndValue(p.curveTaperEnd);
        }
        break;
    }
    case CAMERAPATH: { break; }
    case TOLERANCE: break;
    case USETOLERANCE: break;
    case BUCKETFILLEXPAND: break;
    case USEBUCKETFILLEXPAND: break;
    case BUCKETFILLLAYERREFERENCEMODE: break;
    case FILL_MODE: break;
    default:
        qWarning() << "ToolOptionWidget::onToolPropertyChanged: unhandled ToolPropertyType" << int(ePropertyType);
        break;
    }
}

void ToolOptionWidget::setVisibility(BaseTool* tool)
{
    Q_ASSERT(mBucketOptionsWidget);
    Q_ASSERT(mCameraOptionsWidget);

    disableAllOptions();

    // Hard reset: hide every top-level widget in the main options layout so we
    // always start from a clean slate before enabling the active tool's UI.
    if (ui->verticalLayout_3) {
        for (int i = 0; i < ui->verticalLayout_3->count(); ++i) {
            if (QLayoutItem* item = ui->verticalLayout_3->itemAt(i)) {
                if (QWidget* w = item->widget()) {
                    w->setVisible(false);
                }
            }
        }
    }

    // Default-hide all top-level tool option blocks so only the active tool enables its own UI.
    if (ui->cosmicPenOptionsWidget)        ui->cosmicPenOptionsWidget->setVisible(false);
    if (ui->cosmicEraserOptionsWidget)     ui->cosmicEraserOptionsWidget->setVisible(false);
    if (ui->cosmicEyedropperOptionsWidget) ui->cosmicEyedropperOptionsWidget->setVisible(false);
    if (ui->cosmicCurveOptionsWidget)      ui->cosmicCurveOptionsWidget->setVisible(false);

    if (tool->type() == BUCKET)
    {
        // Show bucket-specific options but continue to allow the rest of
        // the visibility logic to run so shared UI (palettes, sliders)
        // are properly updated and not left showing previous tool state.
        mBucketOptionsWidget->setHidden(false);
        mBucketOptionsWidget->updateUI();
        // Do NOT return here; fall-through to update shared controls below.
    }
    else if (tool->type() == CAMERA)
    {
        mCameraOptionsWidget->setHidden(false);
    }
    else
    {
        mCameraOptionsWidget->setHidden(true);
        mBucketOptionsWidget->setHidden(true);
    }

    // Dplate tool uses the active color; palette is not shown
    mDplateColorPalette->setHidden(true);

    // CosmicSplitLayout: show dedicated layouts only for their tools.
    // NOTE: The "curve family" (Curve, BSpline, Polyline) all reuse the same
    // cosmicCurveOptionsWidget so they stay in lockstep for width / taper /
    // snap / draw-behind behavior and persistence.
    const bool isBitmapLayer           = (editor()->layers()->currentLayer()->type() == Layer::BITMAP);
    const bool isCosmicPencilTool      = (tool->type() == PENCIL && isBitmapLayer);
    const bool isUnifiedPencilToolUi   = (tool->type() == PEN || isCosmicPencilTool);
    const bool isCosmicEraserTool      = (tool->type() == ERASER);
    const bool isCosmicEyedropperTool  = (tool->type() == EYEDROPPER);
    const bool isCosmicCurveTool       = (tool->type() == CURVE ||
                                          tool->type() == BSPLINE ||
                                          tool->type() == POLYLINE);
    if (ui->cosmicPenOptionsWidget) {
        ui->cosmicPenOptionsWidget->setVisible(false);
    }
    if (ui->cosmicPencilOptionsWidget) {
        ui->cosmicPencilOptionsWidget->setVisible(isUnifiedPencilToolUi);
    }
    if (ui->cosmicEraserOptionsWidget) {
        ui->cosmicEraserOptionsWidget->setVisible(isCosmicEraserTool);
    }
    if (ui->cosmicEyedropperOptionsWidget) {
        ui->cosmicEyedropperOptionsWidget->setVisible(isCosmicEyedropperTool);
    }
    if (ui->cosmicCurveOptionsWidget) {
        ui->cosmicCurveOptionsWidget->setVisible(isCosmicCurveTool);
    }
    const bool isCosmicDplateTool = (tool->type() == DPLATE);
    if (ui->cosmicDplateOptionsWidget) {
        ui->cosmicDplateOptionsWidget->setVisible(isCosmicDplateTool);
        if (isCosmicDplateTool && mCosmicDplateDrawBehindToggle) {
            QSignalBlocker b(mCosmicDplateDrawBehindToggle);
            mCosmicDplateDrawBehindToggle->setChecked(tool->properties.drawBehind);
        }
    }

    // When using the unified Pencil UI for Pen, hide legacy pen-option UI and
    // ensure only the Pencil cosmic layout shows.
    if (tool->type() == PEN) {
        if (mPenWidthField)           mPenWidthField->setVisible(false);
        if (ui->featherSlider)        ui->featherSlider->setVisible(false);

        if (mUseFeatherToggle)        mUseFeatherToggle->setVisible(false);
        if (mShowInfoToggle)          mShowInfoToggle->setVisible(false);
        if (mFillContourToggle)       mFillContourToggle->setVisible(false);
        if (mClosedPathToggle)        mClosedPathToggle->setVisible(false);
        if (mBezierToggle)            mBezierToggle->setVisible(false);
        if (mInvisibleToggle)         mInvisibleToggle->setVisible(false);
        if (mPreserveAlphaToggle)     mPreserveAlphaToggle->setVisible(false);
        if (mVectorMergeToggle)       mVectorMergeToggle->setVisible(false);

        if (mAntiAliasingToggle)      mAntiAliasingToggle->setVisible(false);
        if (mDrawBehindToggle)        mDrawBehindToggle->setVisible(false);
        if (mPenSmoothingComboBox)    mPenSmoothingComboBox->setVisible(false);
        if (mPostCorrBox)             mPostCorrBox->setVisible(false);
        if (mHybridSnapComboBox)      mHybridSnapComboBox->setVisible(false);
        if (mAdjustPressureToggle)    mAdjustPressureToggle->setVisible(false);

        if (ui->curveTaperGroup)       ui->curveTaperGroup->setVisible(false);

        if (mPencilOpacitySlider)       mPencilOpacitySlider->setVisible(false);
        if (mPencilStabilizerSlider)    mPencilStabilizerSlider->setVisible(false);

        if (ui->cosmicEraserOptionsWidget) ui->cosmicEraserOptionsWidget->setVisible(false);
        if (mEraserLassoToggle)          mEraserLassoToggle->setVisible(false);
        if (auto lassoBox = this->findChild<QCheckBox*>("eraserLassoModeBox")) {
            lassoBox->setVisible(false);
        }

        if (ui->verticalLayout_3) {
            for (int i = 0; i < ui->verticalLayout_3->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_3->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        if (w != ui->cosmicPencilOptionsWidget) {
                            w->setVisible(false);
                        }
                    }
                }
            }
        }

        return;
    }

    // Dplate: hide all legacy controls, show only the cosmic Dplate layout.
    if (isCosmicDplateTool) {
        if (ui->featherSlider)        ui->featherSlider->setVisible(false);
        if (mPenWidthField)           mPenWidthField->setVisible(false);
        if (mDrawBehindToggle)        mDrawBehindToggle->setVisible(false);
        if (mAntiAliasingToggle)      mAntiAliasingToggle->setVisible(false);
        if (mEraserLassoToggle)       mEraserLassoToggle->setVisible(false);
        if (mAdjustPressureToggle)    mAdjustPressureToggle->setVisible(false);
        if (ui->curveTaperGroup)      ui->curveTaperGroup->setVisible(false);
        if (mHybridSnapComboBox)      mHybridSnapComboBox->setVisible(false);

        if (ui->verticalLayout_2) {
            for (int i = 0; i < ui->verticalLayout_2->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_2->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        w->setVisible(false);
                    }
                }
            }
        }

        if (ui->verticalLayout_3) {
            for (int i = 0; i < ui->verticalLayout_3->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_3->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        if (w != ui->cosmicDplateOptionsWidget) {
                            w->setVisible(false);
                        }
                    }
                }
            }
        }

        return;
    }

    // When using CosmicSplitLayout for Eraser, hide all legacy eraser UI and
    // show only the cosmic eraser layout.
    if (isCosmicEraserTool) {
        if (ui->featherSlider)        ui->featherSlider->setVisible(false);
        if (ui->useFeatherBox)        ui->useFeatherBox->setVisible(false);
        if (mUseFeatherToggle)        mUseFeatherToggle->setVisible(false);
        if (mPenWidthField)           mPenWidthField->setVisible(false);
        if (mDrawBehindToggle)        mDrawBehindToggle->setVisible(false);
        if (mAntiAliasingToggle)      mAntiAliasingToggle->setVisible(false);
        if (ui->useAABox)             ui->useAABox->setVisible(false);

        // Explicitly show cosmic eraser lasso toggle when in eraser mode
        if (mEraserLassoToggle)       mEraserLassoToggle->setVisible(true);
        if (auto lassoBox = this->findChild<QCheckBox*>("eraserLassoModeBox")) {
            lassoBox->setVisible(false);
        }

        // Hide legacy checkbox/toggle stack to avoid duplicate rows
        if (ui->verticalLayout_2) {
            for (int i = 0; i < ui->verticalLayout_2->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_2->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        w->setVisible(false);
                    }
                }
            }
        }

        if (ui->verticalLayout_3) {
            for (int i = 0; i < ui->verticalLayout_3->count(); ++i) {
                if (QLayoutItem* item = ui->verticalLayout_3->itemAt(i)) {
                    if (QWidget* w = item->widget()) {
                        if (w != ui->cosmicEraserOptionsWidget) {
                            w->setVisible(false);
                        }
                    }
                }
            }
        }

        return;
    }

    // Show Adjust Pressure toggle only for Pen tool
    if (mAdjustPressureToggle) {
        const bool showAdjust = (tool->type() == PEN);
        mAdjustPressureToggle->setVisible(showAdjust);
        
        // Hide popup if switching away from Pen tool
        if (!showAdjust && mAdjustPressurePopup && mAdjustPressurePopup->isVisible()) {
            QSignalBlocker b(mAdjustPressureToggle);
            mAdjustPressureToggle->setChecked(false);
            mAdjustPressurePopup->hide();
        }
    }

    // Cosmic Pen settings popup: ensure it is closed when switching away from Pen.
    if (tool->type() != PEN) {
        if (mCosmicPenSettingsPopup && mCosmicPenSettingsPopup->isVisible()) {
            mCosmicPenSettingsPopup->hide();
        }
        if (mCosmicPenSettingsToggle && mCosmicPenSettingsToggle->isChecked()) {
            QSignalBlocker b(mCosmicPenSettingsToggle);
            mCosmicPenSettingsToggle->setChecked(false);
        }
    }

    const bool isPencilTool = (tool->type() == PENCIL);

    if (mPencilOpacitySlider) {
        mPencilOpacitySlider->setVisible(isPencilTool);
        if (isPencilTool) {
            if (auto* pencil = qobject_cast<PencilTool*>(editor()->tools()->getTool(PENCIL))) {
                const int percent = qBound(0, static_cast<int>(pencil->getPencilOpacity() * 100.0 + 0.5), 100);
                QSignalBlocker b(mPencilOpacitySlider);
                mPencilOpacitySlider->setValue(percent);
            }
        }
    }

    if (mPencilStabilizerSlider) {
        mPencilStabilizerSlider->setVisible(isPencilTool);
    }

    // Show opacity slider only for Pencil tool and sync its value
    if (mPencilOpacitySlider) {
        mPencilOpacitySlider->setVisible(isPencilTool);
        if (isPencilTool) {
            if (auto* pencil = qobject_cast<PencilTool*>(editor()->tools()->getTool(PENCIL))) {
                const int percent = qBound(0, static_cast<int>(pencil->getPencilOpacity() * 100.0 + 0.5), 100);
                QSignalBlocker b(mPencilOpacitySlider);
                mPencilOpacitySlider->setValue(percent);
            }
        }
    }
    
    // Width controls: Pen uses CosmicInputField; Curve can use the dedicated
    // cosmic curve width field, and Pencil/Eraser use their dedicated cosmic fields.
    const bool widthEnabled = tool->isPropertyEnabled(WIDTH);
    const bool isPenToolForWidth   = (tool->type() == PEN);

    if (mPenWidthField) {
        // Pen-only cosmic width field.
        mPenWidthField->setVisible(isPenToolForWidth && widthEnabled);
    }
    ui->featherSlider->setVisible(tool->isPropertyEnabled(FEATHER));
    // Use Feather
    if (mUseFeatherToggle) mUseFeatherToggle->setVisible(tool->isPropertyEnabled(USEFEATHER));
    if (ui->useFeatherBox) ui->useFeatherBox->setVisible(false);
    // Bézier
    if (mBezierToggle) mBezierToggle->setVisible(tool->isPropertyEnabled(BEZIER));
    if (ui->useBezierBox) ui->useBezierBox->setVisible(false);
    // Closed Path
    if (mClosedPathToggle) mClosedPathToggle->setVisible(tool->isPropertyEnabled(CLOSEDPATH));
    if (ui->useClosedPathBox) ui->useClosedPathBox->setVisible(false);
    // Deprecated: hide Pressure checkbox completely from UI
    if (ui->usePressureBox) ui->usePressureBox->setVisible(false);
    // Invisible
    if (mInvisibleToggle) mInvisibleToggle->setVisible(tool->isPropertyEnabled(INVISIBILITY));
    if (ui->makeInvisibleBox) ui->makeInvisibleBox->setVisible(false);
    // Preserve Alpha
    if (mPreserveAlphaToggle) mPreserveAlphaToggle->setVisible(tool->isPropertyEnabled(PRESERVEALPHA));
    if (ui->preserveAlphaBox) ui->preserveAlphaBox->setVisible(false);
    
    // Anti-aliasing: new toggle instead of old checkbox
    if (mAntiAliasingToggle) mAntiAliasingToggle->setVisible(tool->isPropertyEnabled(ANTI_ALIASING));
    if (ui->useAABox) ui->useAABox->setVisible(false); // Keep old checkbox hidden

    // Show Lasso Erase toggle only for Eraser tool
    if (mEraserLassoToggle) {
        bool showLasso = (tool->type() == ERASER);
        mEraserLassoToggle->setVisible(showLasso);
        if (showLasso) {
            QSignalBlocker b(mEraserLassoToggle);
            mEraserLassoToggle->setChecked(tool->properties.eraserLassoMode);
        }
    }
    if (auto lassoBox = this->findChild<QCheckBox*>("eraserLassoModeBox")) {
        lassoBox->setVisible(false);
        if (tool->type() == ERASER) {
            QSignalBlocker b(lassoBox);
            lassoBox->setChecked(tool->properties.eraserLassoMode);
        }
    }
    // Base Stabiliser removed - always hidden
    ui->stabilizerLabel->setVisible(false);
    ui->inpolLevelsCombo->setVisible(false);
    if (mFillContourToggle) mFillContourToggle->setVisible(tool->isPropertyEnabled(FILLCONTOUR));
    if (ui->fillContourBox) ui->fillContourBox->setVisible(false);
    if (mShowInfoToggle) mShowInfoToggle->setVisible(tool->isPropertyEnabled(SHOWSELECTIONINFO));
    if (ui->showInfoBox) ui->showInfoBox->setVisible(false);
    
    // Show Draw Behind toggle (new widget) and hide old checkbox. When using the
    // dedicated Cosmic Curve layout, Draw Behind is controlled by the per-tool
    // curve checkbox instead of the global stacked toggle. For the Pencil tool,
    // Draw Behind lives in the Cosmic Pencil split layout, so hide the global
    // stacked toggle there as well.
    if (mDrawBehindToggle) mDrawBehindToggle->setVisible(tool->isPropertyEnabled(DRAW_BEHIND)
                                                         && !isCosmicCurveTool
                                                         && tool->type() != PENCIL);
    if (ui->drawBehindBox) ui->drawBehindBox->setVisible(false); // Always hide old checkbox


    // Cosmic Pen layout: legacy Krita-style smoothing widgets have been
    // removed; only the Cosmic Pen controls represent these properties.

    const bool showCurveTaper = (!isCosmicCurveTool && (tool->type() == CURVE || tool->type() == BSPLINE || tool->type() == POLYLINE));
    if (ui->curveTaperGroup) ui->curveTaperGroup->setVisible(showCurveTaper);
    if (mHybridSnapComboBox) mHybridSnapComboBox->setVisible(showCurveTaper);

    auto currentLayerType = editor()->layers()->currentLayer()->type();
    auto propertyType = editor()->tools()->currentTool()->type();

    if (currentLayerType == Layer::VECTOR)
    {
        switch (propertyType)
        {

        default:
            if (mAntiAliasingToggle) mAntiAliasingToggle->setVisible(false);
            if (ui->useAABox) ui->useAABox->setVisible(false);
            break;
        }
    }
    else
    {
        switch (propertyType)
        {
        case BUCKET:
            break;
        case SELECT:
        case MOVE:
            ui->usePressureBox->setVisible(false);
            ui->featherSlider->setVisible(false);
            ui->useFeatherBox->setVisible(false);
            break;
        default:
            ui->makeInvisibleBox->setVisible(false);
            break;
        }
    }
}

void ToolOptionWidget::onToolChanged(ToolType toolType)
{
    // Pen color palette removed from tool options
    if (mDplateColorPalette) {
        mDplateColorPalette->setHidden(true);
    }

    // Unified Pencil UI: keep Pen and Pencil stroke settings in lockstep.
    // Whichever tool becomes active should inherit the shared settings from
    // the other tool so switching presets (Final Line <-> normal brushes)
    // does not feel like switching to a separate tool.
    if (toolType == PEN) {
        mirrorUnifiedStrokeSettings(PEN, PENCIL);
    } else if (toolType == PENCIL) {
        mirrorUnifiedStrokeSettings(PENCIL, PEN);
    }

    updateUI();

    // Draw Behind is global; UI reflects the global state via current tool properties.
    // The UI is already synced from the tool's saved properties in updateUI().
}

void ToolOptionWidget::mirrorUnifiedStrokeSettings(ToolType dstTool, ToolType srcTool)
{
    if (!editor() || !editor()->tools()) {
        return;
    }
    if ((dstTool != PEN && dstTool != PENCIL) || (srcTool != PEN && srcTool != PENCIL)) {
        return;
    }

    ToolManager* tm = editor()->tools();
    BaseTool* src = tm->getTool(srcTool);
    BaseTool* dst = tm->getTool(dstTool);
    if (!src || !dst) {
        return;
    }

    const Properties& p = src->properties;

    // Pencil presets must not receive unsupported settings.
    const bool dstIsPencil = (dstTool == PENCIL);
    const bool dstIsPen = (dstTool == PEN);
    if (!dstIsPencil && !dstIsPen) {
        return;
    }

    PenSmoothingMode dstMode = p.penSmoothingMode;
    int dstPostCorr = 0;

    if (dstIsPencil) {
        dstPostCorr = 0;
    } else {
        // Pen (Final Line): Post Correction is supported, but do NOT let Pencil->Pen
        // mirroring wipe it back to 0.
        if (srcTool == PEN) {
            dstPostCorr = p.postCorrectionLevel;
        } else {
            BaseTool* currentDst = tm->getTool(dstTool);
            dstPostCorr = currentDst ? currentDst->properties.postCorrectionLevel : 0;
        }
    }

    tm->beginEditingPropertiesForTool(dstTool);
    tm->setWidth(p.width);
    tm->setDrawBehind(p.drawBehind);
    tm->setPenSmoothingMode(static_cast<int>(dstMode));

    // IMPORTANT: ToolManager::setPostCorrectionLevel() always applies to the Pen tool.
    // Do not call it when syncing Pencil, otherwise switching/adjusting Pencil presets
    // will wipe Pen's post-correction value.
    if (dstTool == PEN) {
        tm->setPostCorrectionLevel(dstPostCorr);
    } else if (dstTool == PENCIL) {
        if (BaseTool* pencil = tm->getTool(PENCIL)) {
            pencil->properties.postCorrectionLevel = 0;
        }
    }

    tm->setWeightedDistance(p.weightedDistance);
    tm->setStab2Delay(p.stab2Delay);
    tm->endEditingProperties();
}

void ToolOptionWidget::applyUnifiedSmoothToTool(ToolType toolType)
{
    if (mApplyingUnifiedSmoothToTool) {
        return;
    }
    if (!editor() || !editor()->tools()) {
        return;
    }

    // If UI hasn't been initialized yet, try to seed it from the currently visible smooth field.
    if (!mUnifiedSmoothUiInitialized) {
        const CosmicInputField* field = nullptr;
        if (toolType == PEN) {
            field = mCosmicPenSmoothField;
        } else if (toolType == PENCIL) {
            field = mCosmicPencilSmoothField;
        }
        if (field) {
            mUnifiedSmoothUiValue = qBound(1, int(qRound(field->value())), 10);
            mUnifiedSmoothUiInitialized = true;
        }
    }
    if (!mUnifiedSmoothUiInitialized) {
        return;
    }

    mApplyingUnifiedSmoothToTool = true;
    ToolManager* toolManager = editor()->tools();

    const int ui = qBound(1, mUnifiedSmoothUiValue, 10);

    if (toolType == PEN) {
        toolManager->beginEditingPropertiesForTool(PEN);
        const double legacy = (ui <= 1) ? 0.0 : 300.0 * qPow((double(ui - 2) / 8.0), 2.0);
        toolManager->setWeightedDistance(legacy);
        toolManager->setStab2Delay(legacy);
        toolManager->endEditingProperties();
    }
    else if (toolType == PENCIL) {
        // Pencil only supports Delayed/String.
        toolManager->beginEditingPropertiesForTool(PENCIL);
        const double legacy = (ui <= 1) ? 0.0 : 300.0 * qPow((double(ui - 2) / 8.0), 2.0);
        toolManager->setWeightedDistance(legacy);
        toolManager->setStab2Delay(legacy);
        toolManager->endEditingProperties();
    }

    mApplyingUnifiedSmoothToTool = false;
}

void ToolOptionWidget::setPenWidth(qreal width)
{
    if (mPenWidthField) {
        QSignalBlocker b2(mPenWidthField);
        mPenWidthField->setValue(width);
    }
}

void ToolOptionWidget::setPenFeather(qreal featherValue)
{
    QSignalBlocker b(ui->featherSlider);
    ui->featherSlider->setEnabled(true);
    ui->featherSlider->setValue(featherValue);
}

void ToolOptionWidget::setUseFeather(bool useFeather)
{
    if (mUseFeatherToggle) {
        QSignalBlocker b(mUseFeatherToggle);
        mUseFeatherToggle->setEnabled(true);
        mUseFeatherToggle->setChecked(useFeather);
    }
    QSignalBlocker b(ui->useFeatherBox);
    ui->useFeatherBox->setEnabled(true);
    ui->useFeatherBox->setChecked(useFeather);
}

void ToolOptionWidget::setPenInvisibility(int x)
{
    if (mInvisibleToggle) {
        QSignalBlocker b(mInvisibleToggle);
        mInvisibleToggle->setEnabled(true);
        mInvisibleToggle->setChecked(x > 0);
    }
    QSignalBlocker b(ui->makeInvisibleBox);
    ui->makeInvisibleBox->setEnabled(true);
    ui->makeInvisibleBox->setChecked(x > 0);
}

void ToolOptionWidget::setPressure(int x)
{
    QSignalBlocker b(ui->usePressureBox);
    ui->usePressureBox->setEnabled(true);
    ui->usePressureBox->setChecked(x > 0);
}

void ToolOptionWidget::setPreserveAlpha(int x)
{
    if (mPreserveAlphaToggle) {
        QSignalBlocker b(mPreserveAlphaToggle);
        mPreserveAlphaToggle->setEnabled(true);
        mPreserveAlphaToggle->setChecked(x > 0);
    }
    QSignalBlocker b(ui->preserveAlphaBox);
    ui->preserveAlphaBox->setEnabled(true);
    ui->preserveAlphaBox->setChecked(x > 0);
}

void ToolOptionWidget::setVectorMergeEnabled(int x)
{
    if (mVectorMergeToggle) {
        QSignalBlocker b(mVectorMergeToggle);
        mVectorMergeToggle->setEnabled(true);
        mVectorMergeToggle->setChecked(x > 0);
    }
    QSignalBlocker b(ui->vectorMergeBox);
    ui->vectorMergeBox->setEnabled(true);
    ui->vectorMergeBox->setChecked(x > 0);
}

void ToolOptionWidget::setAA(int x)
{
    // Update new toggle
    if (mAntiAliasingToggle) {
        QSignalBlocker b(mAntiAliasingToggle);
        mAntiAliasingToggle->setEnabled(true);
        mAntiAliasingToggle->setVisible(false);
    }
    
    // Keep old checkbox updated but hidden
    QSignalBlocker b(ui->useAABox);
    ui->useAABox->setEnabled(true);
    ui->useAABox->setVisible(false);

    auto layerType = editor()->layers()->currentLayer()->type();

    if (layerType == Layer::BITMAP)
    {
        // Show AA for Pen, Curve, BSpline, Polyline, and Lasso Select tools
        const ToolType t = editor()->tools()->currentTool()->type();
        const bool hideAA = !(t == PEN || t == CURVE || t == BSPLINE || t == POLYLINE || t == LASSO_SELECT);
        if (hideAA) {
            if (mAntiAliasingToggle) {
                mAntiAliasingToggle->setEnabled(false);
                mAntiAliasingToggle->setVisible(false);
            }
            ui->useAABox->setEnabled(false);
            ui->useAABox->setVisible(false);
        } else {
            if (x == -1) {
                if (mAntiAliasingToggle) {
                    mAntiAliasingToggle->setEnabled(false);
                    mAntiAliasingToggle->setVisible(false);
                }
                ui->useAABox->setEnabled(false);
                ui->useAABox->setVisible(false);
            } else {
                if (mAntiAliasingToggle) {
                    mAntiAliasingToggle->setVisible(false); // AA hardcoded off, keep hidden
                    mAntiAliasingToggle->setChecked(x > 0);
                }
                ui->useAABox->setVisible(false); // Keep old checkbox hidden
            }
            ui->useAABox->setChecked(x > 0);
        }
    }
}

void ToolOptionWidget::setStabilizerLevel(int x)
{
    // Base Stabiliser removed - always None (0)
    Q_UNUSED(x);
}

void ToolOptionWidget::setFillContour(int useFill)
{
    if (mFillContourToggle) {
        QSignalBlocker b(mFillContourToggle);
        mFillContourToggle->setEnabled(true);
        mFillContourToggle->setChecked(useFill > 0);
    }
    QSignalBlocker b(ui->fillContourBox);
    ui->fillContourBox->setEnabled(true);
    ui->fillContourBox->setChecked(useFill > 0);
}

void ToolOptionWidget::setBezier(bool useBezier)
{
    if (mBezierToggle) {
        QSignalBlocker b(mBezierToggle);
        mBezierToggle->setChecked(useBezier);
    }
    QSignalBlocker b(ui->useBezierBox);
    ui->useBezierBox->setChecked(useBezier);
}

void ToolOptionWidget::setClosedPath(bool useClosedPath)
{
    if (mClosedPathToggle) {
        QSignalBlocker b(mClosedPathToggle);
        mClosedPathToggle->setChecked(useClosedPath);
    }
    QSignalBlocker b(ui->useClosedPathBox);
    ui->useClosedPathBox->setChecked(useClosedPath);
}

void ToolOptionWidget::setDrawBehind(bool drawBehind)
{
    if (mDrawBehindToggle) {
        QSignalBlocker b(mDrawBehindToggle);
        mDrawBehindToggle->setEnabled(true);
        mDrawBehindToggle->setChecked(drawBehind);
    }
    if (mCosmicCurveDrawBehindToggle) {
        QSignalBlocker b(mCosmicCurveDrawBehindToggle);
        mCosmicCurveDrawBehindToggle->setChecked(drawBehind);
    }
    if (mCosmicDplateDrawBehindToggle) {
        QSignalBlocker b(mCosmicDplateDrawBehindToggle);
        mCosmicDplateDrawBehindToggle->setChecked(drawBehind);
    }
    // Keep old checkbox for backward compatibility but hidden
    if (ui->drawBehindBox) {
        QSignalBlocker b(ui->drawBehindBox);
        ui->drawBehindBox->setChecked(drawBehind);
    }
}

void ToolOptionWidget::setShowSelectionInfo(bool showSelectionInfo)
{
    if (mShowInfoToggle) {
        QSignalBlocker b(mShowInfoToggle);
        mShowInfoToggle->setChecked(showSelectionInfo);
    }
    QSignalBlocker b(ui->showInfoBox);
    ui->showInfoBox->setChecked(showSelectionInfo);
}

void ToolOptionWidget::onPresetDuplicateRequested(int index)
{
    // This method was used for pencil preset duplication
    // Since pencil tool is removed, this is now a no-op
    Q_UNUSED(index)
}

void ToolOptionWidget::onColorPreferenceChanged(SETTING setting)
{
    if (setting == SETTING::ACCENT_COLOR) {
        updateAccentColors();
    }
}

void ToolOptionWidget::updateAccentColors()
{
    QColor accentColor = BlenderStyleSlider::getAccentColor();
    const QString accentHex = accentColor.name(QColor::HexRgb);
    const QString borderHex = accentColor.darker(150).name(QColor::HexRgb);
    
    if (mPenSmoothingComboBox) {
        mPenSmoothingComboBox->setAccentColor(accentColor);
    }
    if (mHybridSnapComboBox) {
        mHybridSnapComboBox->setAccentColor(accentColor);
    }
    if (mDrawBehindToggle) {
        mDrawBehindToggle->setAccentColor(accentColor);
    }
    if (mAntiAliasingToggle) {
        mAntiAliasingToggle->setAccentColor(accentColor);
    }
    if (mCurveTapperSliders) {
        mCurveTapperSliders->setAccentColor(accentColor);
    }
    if (mAdjustPressureToggle) {
        mAdjustPressureToggle->setAccentColor(accentColor);
    }
    if (mAdjustPressurePopup) {
        mAdjustPressurePopup->setAccentColor(accentColor);
    }
    if (mPencilOpacitySlider) {
        mPencilOpacitySlider->setSliderColor(accentColor);
    }
    // Cosmic Pencil split layout controls
    if (mCosmicPencilSizeField) {
        mCosmicPencilSizeField->setAccentColor(accentColor);
    }
    if (mCosmicPencilOpacityField) {
        mCosmicPencilOpacityField->setAccentColor(accentColor);
    }
    if (mCosmicPencilDrawBehindToggle) {
        mCosmicPencilDrawBehindToggle->setAccentColor(accentColor);
    }
    if (mCosmicPenSettingsToggle) {
        mCosmicPenSettingsToggle->setAccentColor(accentColor);
    }
    if (mCosmicPenSettingsPopup) {
        mCosmicPenSettingsPopup->setAccentColor(accentColor);
    }
    if (mUseFeatherToggle) mUseFeatherToggle->setAccentColor(accentColor);
    if (mShowInfoToggle) mShowInfoToggle->setAccentColor(accentColor);
    if (mFillContourToggle) mFillContourToggle->setAccentColor(accentColor);
    if (mClosedPathToggle) mClosedPathToggle->setAccentColor(accentColor);
    if (mBezierToggle) mBezierToggle->setAccentColor(accentColor);
    if (mInvisibleToggle) mInvisibleToggle->setAccentColor(accentColor);
    if (mPreserveAlphaToggle) mPreserveAlphaToggle->setAccentColor(accentColor);
    if (mVectorMergeToggle) mVectorMergeToggle->setAccentColor(accentColor);
    if (mEraserLassoToggle) mEraserLassoToggle->setAccentColor(accentColor);
}

void ToolOptionWidget::disableAllOptions()
{
    ui->featherSlider->hide();
    ui->useFeatherBox->hide();
    ui->useBezierBox->hide();
    ui->useClosedPathBox->hide();
    ui->usePressureBox->hide();
    ui->makeInvisibleBox->hide();
    ui->preserveAlphaBox->hide();
    ui->vectorMergeBox->hide();
    ui->useAABox->hide();
    if (mAntiAliasingToggle) mAntiAliasingToggle->hide();
    // Base Stabiliser removed
    ui->fillContourBox->hide();
    ui->showInfoBox->hide();
    ui->stabilizerLabel->hide();
    ui->drawBehindBox->hide();

    if (mPenSmoothingComboBox) mPenSmoothingComboBox->hide();
    if (mPostCorrBox) mPostCorrBox->hide();
    if (mHybridSnapComboBox) mHybridSnapComboBox->hide();
    if (mDrawBehindToggle) mDrawBehindToggle->hide();
    if (mAdjustPressureToggle) mAdjustPressureToggle->hide();
    if (mAdjustPressurePopup) mAdjustPressurePopup->hide();
    if (mCosmicPenSettingsPopup) mCosmicPenSettingsPopup->hide();
    if (mCosmicPenSettingsToggle) {
        QSignalBlocker b(mCosmicPenSettingsToggle);
        mCosmicPenSettingsToggle->setChecked(false);
    }
    if (mUseFeatherToggle) mUseFeatherToggle->hide();
    if (mShowInfoToggle) mShowInfoToggle->hide();
    if (mFillContourToggle) mFillContourToggle->hide();
    if (mClosedPathToggle) mClosedPathToggle->hide();
    if (mBezierToggle) mBezierToggle->hide();
    if (mInvisibleToggle) mInvisibleToggle->hide();
    if (mPreserveAlphaToggle) mPreserveAlphaToggle->hide();
    if (mVectorMergeToggle) mVectorMergeToggle->hide();
    if (mEraserLassoToggle) mEraserLassoToggle->hide();
    
    if (ui->curveTaperGroup) ui->curveTaperGroup->hide();

    // Also hide any top-level grouped option widgets so nothing leaks before
    // setVisibility() re-enables only the active tool's UI.
    if (ui->cosmicPenOptionsWidget)     ui->cosmicPenOptionsWidget->hide();
    if (ui->cosmicPencilOptionsWidget)  ui->cosmicPencilOptionsWidget->hide();
    if (ui->cosmicEraserOptionsWidget)  ui->cosmicEraserOptionsWidget->hide();
    if (mBucketOptionsWidget)          mBucketOptionsWidget->hide();
    if (mCameraOptionsWidget)          mCameraOptionsWidget->hide();
}
