#include "brushfactorydock.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QImage>
#include <QPixmap>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QRandomGenerator>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QAbstractButton>
#include <QTimer>

#include "editor.h"
#include "toolmanager.h"
#include "penciltool.h"
#include "scribblearea.h"
#include "beziercurveeditor.h"
#include "pentool.h"
#include "penciltool.h"
#include "pencildef.h"
#include "managers/settingsmanager.h"
#include "util.h"
#include "layer.h"
#include "layermanager.h"
#include "stroketool.h"
#include "titlebarwidget.h"
#include "cosmicbrushsettings.h"
#include "managers/brushpresetstore.h"
#include "managers/brushassetstore.h"
#include "filedialog.h"

#include "../CUI/cosmichslider.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/blenderstylemodalpopup.h"
#include "cosmicsplittoollayout.h"

#include <QUuid>

class BrushFactoryPresetDelegate : public QStyledItemDelegate
{
public:
    explicit BrushFactoryPresetDelegate(QVector<ToolOptionWidget::BrushPreset>* presets, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , mPresets(presets)
    {
    }

    void clearTextureCaches() const
    {
        mCustomTextures.clear();
        mHasDefaultTexture = false;
        mDefaultTexture = QImage();
        mStrokeCache.clear();
        mDirtyRows.clear();
    }

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
        return QSize(220, 72);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!mPresets || !index.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        int row = index.row();
        if (row < 0 || row >= mPresets->size()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const ToolOptionWidget::BrushPreset& preset = mPresets->at(row);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QRect outerRect = option.rect;
        const bool selected = (option.state & QStyle::State_Selected);
        QColor fillColor = selected ? QColor(0x60, 0x60, 0x60) : option.palette.color(QPalette::Base).darker(115);
        painter->fillRect(outerRect, fillColor);

        // Thick black separator line
        QPen sepPen(QColor(0, 0, 0));
        sepPen.setWidth(3);
        painter->setPen(sepPen);
        painter->drawLine(outerRect.bottomLeft(), outerRect.bottomRight());

        QRect strokeRect = outerRect.adjusted(10, 8, -10, -24);

        if (strokeRect.width() > 4 && strokeRect.height() > 4) {
            int w = strokeRect.width();
            int h = strokeRect.height();
            QSize desiredSize(w, h);

            auto it = mStrokeCache.constFind(row);
            bool hasCache = (it != mStrokeCache.constEnd());
            if (hasCache && it.value().size() == desiredSize && !mDirtyRows.contains(row)) {
                painter->drawImage(strokeRect.topLeft(), it.value());
            } else {
                QImage strokeImg(w, h, QImage::Format_ARGB32_Premultiplied);
                strokeImg.fill(Qt::transparent);

                auto blurStrokeImage = [](QImage& img) {
                    if (img.isNull()) {
                        return;
                    }
                    int iw = img.width();
                    int ih = img.height();
                    if (iw < 3 || ih < 3) {
                        return;
                    }

                    int radius = 2;
                    QImage tmp(img.size(), img.format());

                    for (int y = 0; y < ih; ++y) {
                        QRgb* dstLine = reinterpret_cast<QRgb*>(tmp.scanLine(y));
                        for (int x = 0; x < iw; ++x) {
                            int rSum = 0;
                            int gSum = 0;
                            int bSum = 0;
                            int aSum = 0;
                            int count = 0;
                            for (int k = -radius; k <= radius; ++k) {
                                int sx = x + k;
                                if (sx < 0 || sx >= iw) {
                                    continue;
                                }
                                QRgb px = img.pixel(sx, y);
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

                    for (int y = 0; y < ih; ++y) {
                        QRgb* dstLine = reinterpret_cast<QRgb*>(img.scanLine(y));
                        for (int x = 0; x < iw; ++x) {
                            int rSum = 0;
                            int gSum = 0;
                            int bSum = 0;
                            int aSum = 0;
                            int count = 0;
                            for (int k = -radius; k <= radius; ++k) {
                                int sy = y + k;
                                if (sy < 0 || sy >= ih) {
                                    continue;
                                }
                                QRgb px = tmp.pixel(x, sy);
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

                int hVal = strokeRect.height();
                qreal midY = hVal * 0.5;

                qreal baseRadius = qMax<qreal>(1.0, hVal * 0.26);

                qreal scatterNorm = qBound<qreal>(0.0, preset.scatter / 3.0, 1.0);
                qreal maxJitterY = hVal * 0.35 * scatterNorm;

                qreal spacingMult = qBound<qreal>(0.25, preset.spacingMult, 10.0);
                qreal densityFactor = 1.5 - (spacingMult - 0.25) * 0.7 + scatterNorm * 0.8;
                densityFactor = qBound<qreal>(0.6, densityFactor, 2.4);
                int baseDabs = 72;
                int dabCount = qBound(40, static_cast<int>(baseDabs * densityFactor), 160);

                const QImage* dabImage = nullptr;

                auto makeBlackFromTexture = [](const QImage& src) {
                    if (src.isNull()) {
                        return QImage();
                    }

                    QImage working = src;
                    int maxDim = 1024;
                    if (working.width() > maxDim || working.height() > maxDim) {
                        working = working.scaled(QSize(maxDim, maxDim), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }

                    QImage texture = working.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                    int texWidth = texture.width();
                    int texHeight = texture.height();
                    if (texWidth <= 0 || texHeight <= 0) {
                        return QImage();
                    }

                    for (int ty = 0; ty < texHeight; ++ty) {
                        QRgb* line = reinterpret_cast<QRgb*>(texture.scanLine(ty));
                        for (int tx = 0; tx < texWidth; ++tx) {
                            int gray = qGray(line[tx]);
                            line[tx] = qRgba(255, 255, 255, gray);
                        }
                    }

                    int dabSize = 64;
                    qreal radius = dabSize / 2.0;
                    qreal radiusMinusOne = radius - 1.0;

                    QImage dab(dabSize, dabSize, QImage::Format_ARGB32_Premultiplied);
                    dab.fill(Qt::transparent);

                    int texCenterX = texWidth / 2;
                    int texCenterY = texHeight / 2;
                    int texSampleSize = qMin(texWidth, texHeight) / 2;
                    qreal texScale = texSampleSize > 0 ? (texSampleSize / radius) : 1.0;

                    const uchar* texBits = texture.constBits();
                    int texBytesPerLine = texture.bytesPerLine();

                    for (int y = 0; y < dabSize; ++y) {
                        QRgb* line = reinterpret_cast<QRgb*>(dab.scanLine(y));
                        qreal dy = y - radius + 0.5;
                        qreal dy2 = dy * dy;

                        for (int x = 0; x < dabSize; ++x) {
                            qreal dx = x - radius + 0.5;
                            qreal distSq = dx * dx + dy2;
                            qreal dist = std::sqrt(distSq);

                            if (dist >= radius) {
                                continue;
                            }

                            qreal circleMask = (dist > radiusMinusOne) ? (radius - dist) : 1.0;

                            int texX = texCenterX + qRound((x - radius) * texScale);
                            int texY = texCenterY + qRound((y - radius) * texScale);

                            if (texWidth > 0 && texHeight > 0) {
                                texX = (texX % texWidth + texWidth) % texWidth;
                                texY = (texY % texHeight + texHeight) % texHeight;
                            }

                            const uchar* texPixel = texBits + texY * texBytesPerLine + texX * 4;
                            qreal textureOpacity = texPixel[3] / 255.0;

                            qreal finalOpacity = circleMask * textureOpacity;
                            int alpha = qRound(finalOpacity * 255.0);
                            if (alpha <= 0) {
                                continue;
                            }
                            int c = alpha;
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

                int dabBase = qMax(1, qMin(dabImage->width(), dabImage->height()));

                qreal rotStrength = qBound<qreal>(0.0, preset.rotationDeg / 40.0, 1.0);

                QPainter imgPainter(&strokeImg);
                imgPainter.setRenderHint(QPainter::Antialiasing, true);

                QRandomGenerator gen(static_cast<quint32>(qHash(QStringLiteral("brush_preview_tile_dabs_%1").arg(row))));

                for (int i = 0; i < dabCount; ++i) {
                    qreal t = (static_cast<qreal>(i) + 0.5) / static_cast<qreal>(dabCount);

                    qreal tri = (t < 0.5) ? (t * 2.0) : (2.0 - 2.0 * t);
                    qreal radius = baseRadius * (0.30 + 0.70 * tri);

                    if (preset.sizeJitter > 0.0) {
                        qreal amount = 0.6 * preset.sizeJitter;
                        qreal sizeJit = (gen.generateDouble() * 2.0 - 1.0) * amount;
                        radius *= (1.0 + sizeJit);
                        radius = qMax<qreal>(1.0, radius);
                    }

                    qreal x = strokeRect.left() + t * (strokeRect.width());
                    qreal curveAmp = hVal * 0.18;
                    qreal baseYVal = midY + curveAmp * qSin(t * 3.14159265 * 2.0);

                    qreal jitterY = 0.0;
                    if (maxJitterY > 0.0) {
                        jitterY = (gen.generateDouble() * 2.0 - 1.0) * maxJitterY;
                    }
                    qreal yVal = baseYVal + jitterY;

                    if (yVal < 0.0 || yVal >= hVal) {
                        continue;
                    }

                    qreal baseAngle = 0.0;
                    qreal jitterAngle = (gen.generateDouble() * 2.0 - 1.0) * 45.0 * rotStrength;
                    qreal angleDeg = baseAngle + jitterAngle;

                    qreal alphaFactor = 0.35 + 0.65 * tri;

                    if (alphaFactor <= 0.01) {
                        continue;
                    }

                    qreal scale = (radius * 2.0) / static_cast<qreal>(dabBase);
                    imgPainter.save();
                    imgPainter.translate(x, yVal);
                    imgPainter.rotate(angleDeg);
                    imgPainter.scale(scale, scale);

                    imgPainter.setOpacity(alphaFactor);
                    imgPainter.drawImage(QPointF(-dabImage->width() / 2.0, -dabImage->height() / 2.0), *dabImage);
                    imgPainter.restore();
                }

                blurStrokeImage(strokeImg);

                mStrokeCache.insert(row, strokeImg);
                mDirtyRows.remove(row);

                painter->drawImage(strokeRect.topLeft(), strokeImg);
            }
        }

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

BrushFactoryDock::BrushFactoryDock(QWidget* parent)
    : BaseDockWidget(parent)
{
    setWindowTitle(QStringLiteral("Brush Presets"));
    setObjectName(QStringLiteral("BrushFactoryDock"));

    // For Brush Factory we only want the close button in the title bar;
    // hide the dock/undock and floating-lock controls.
    if (auto* tb = qobject_cast<TitleBarWidget*>(titleBarWidget())) {
        tb->setDockControlsVisible(false, false);
    }
}

BrushFactoryDock::~BrushFactoryDock()
{
}

void BrushFactoryDock::initUI()
{
    loadPresets();

    QWidget* content = new QWidget(this);
    mContentWidget = content;

    int borderMargin = 16;
    QVBoxLayout* outerLayout = new QVBoxLayout(content);
    outerLayout->setContentsMargins(borderMargin, borderMargin, borderMargin, borderMargin);
    outerLayout->setSpacing(8);

    QWidget* headerWidget = new QWidget(content);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    mAddPresetButton = new CosmicButton(headerWidget);
    mAddPresetButton->setText(tr("Add"));
    mAddPresetButton->setBordered(true);
    mAddPresetButton->setAccentColor(BlenderStyleModalPopup::getAccentColor());

    mRemovePresetButton = new CosmicButton(headerWidget);
    mRemovePresetButton->setText(tr("Remove"));
    mRemovePresetButton->setBordered(true);
    mRemovePresetButton->setAccentColor(BlenderStyleModalPopup::getAccentColor());

    CosmicButton* importPresetButton = new CosmicButton(headerWidget);
    importPresetButton->setText(tr("Import"));
    importPresetButton->setBordered(true);
    importPresetButton->setAccentColor(BlenderStyleModalPopup::getAccentColor());

    CosmicButton* exportPresetButton = new CosmicButton(headerWidget);
    exportPresetButton->setText(tr("Export"));
    exportPresetButton->setBordered(true);
    exportPresetButton->setAccentColor(BlenderStyleModalPopup::getAccentColor());
    importPresetButton->setEnabled(false);
    exportPresetButton->setEnabled(false);

    headerLayout->addWidget(mAddPresetButton);
    headerLayout->addWidget(mRemovePresetButton);
    headerLayout->addWidget(importPresetButton);
    headerLayout->addWidget(exportPresetButton);
    headerLayout->addStretch(1);
    outerLayout->addWidget(headerWidget);

    headerWidget->setVisible(false);

    QWidget* bodyWidget = new QWidget(content);
    bodyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QHBoxLayout* contentLayout = new QHBoxLayout(bodyWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);
    outerLayout->addWidget(bodyWidget, 1);

    QWidget* presetsWidget = new QWidget(bodyWidget);
    QVBoxLayout* presetsLayout = new QVBoxLayout(presetsWidget);
    presetsLayout->setContentsMargins(0, 0, 0, 0);
    presetsLayout->setSpacing(8);
    mPresetListWidget = new QListWidget(presetsWidget);
    mPresetListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mPresetListWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    presetsLayout->addWidget(mPresetListWidget);
    contentLayout->addWidget(presetsWidget, 1);

    using BrushPreset = ToolOptionWidget::BrushPreset;
    QVector<BrushPreset>* presetsData = &mPresets;
    int* activePresetIndex = &mActivePresetIndex;

    BrushFactoryPresetDelegate* presetDelegate = new BrushFactoryPresetDelegate(presetsData, mPresetListWidget);
    mPresetListWidget->setItemDelegate(presetDelegate);

    QWidget* settingsWidget = new QWidget(bodyWidget);
    QHBoxLayout* settingsLayout = new QHBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(12);

    mCategoryContainer = new QWidget(settingsWidget);
    mCategoryContainer->setObjectName("brushFactoryCategoryContainer");
    mCategoryContainer->setStyleSheet("#brushFactoryCategoryContainer { background-color: #1f1f1f; border-radius: 10px; }");
    QVBoxLayout* categoryLayout = new QVBoxLayout(mCategoryContainer);
    categoryLayout->setContentsMargins(10, 10, 10, 10);
    categoryLayout->setSpacing(6);

    mCategoryGroup = new QButtonGroup(this);
    mCategoryGroup->setExclusive(true);

    auto makeCategoryToggle = [this](const QString& text) {
        auto* t = new CosmicCheckboxToggle(mCategoryContainer);
        t->setMode(CosmicCheckboxToggle::Custom);
        t->setScaleMode(CosmicCheckboxToggle::ScaleBig);
        t->setShowIcon(false);
        t->setShowText(true);
        t->setBordered(false);
        t->setBackgroundEnabled(true);
        t->setHoverHighlightEnabled(false);
        t->setText(text);
        t->setAccentColor(BlenderStyleModalPopup::getAccentColor());
        return t;
    };

    CosmicCheckboxToggle* strokeCat = makeCategoryToggle(tr("Stroke"));
    CosmicCheckboxToggle* pressureCat = makeCategoryToggle(tr("Pressure"));
    CosmicCheckboxToggle* textureCat = makeCategoryToggle(tr("Texture"));
    categoryLayout->addWidget(strokeCat);
    categoryLayout->addWidget(pressureCat);
    categoryLayout->addWidget(textureCat);
    categoryLayout->addStretch(1);

    mCategoryGroup->addButton(strokeCat, 0);
    mCategoryGroup->addButton(pressureCat, 1);
    mCategoryGroup->addButton(textureCat, 2);

    settingsLayout->addWidget(mCategoryContainer, 0);

    mCategoryStack = new QStackedWidget(settingsWidget);
    settingsLayout->addWidget(mCategoryStack, 1);

    auto wireCategory = [this](CosmicCheckboxToggle* btn, int idx) {
        if (!btn) return;

        // Tabs must behave like radio buttons: clicking the active tab should not uncheck it.
        QObject::connect(btn, &CosmicCheckboxToggle::toggled, this, [this, btn, idx](bool on) {
            if (!mCategoryStack) {
                return;
            }

            if (on) {
                mCategoryStack->setCurrentIndex(idx);
                return;
            }

            // Only prevent unchecking if it would leave *no* category selected.
            // This avoids fighting legitimate tab switches where another tab is being turned on.
            bool anyChecked = false;
            if (mCategoryGroup) {
                const auto buttons = mCategoryGroup->buttons();
                for (QAbstractButton* b : buttons) {
                    if (b && b->isChecked()) {
                        anyChecked = true;
                        break;
                    }
                }
            }

            if (!anyChecked) {
                QSignalBlocker b(btn);
                btn->setChecked(true);
                mCategoryStack->setCurrentIndex(idx);
            }
        });
    };
    wireCategory(strokeCat, 0);
    wireCategory(pressureCat, 1);
    wireCategory(textureCat, 2);

    strokeCat->setChecked(true);

    auto makeSplitPage = [this](QWidget* parent) {
        QWidget* page = new QWidget(parent);
        page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        QHBoxLayout* h = new QHBoxLayout(page);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(12);

        QWidget* leftArea = new QWidget(page);
        leftArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        auto* leftLayout = new QVBoxLayout(leftArea);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        QWidget* rightArea = new QWidget(page);
        rightArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto* rightLayout = new QVBoxLayout(rightArea);
        rightLayout->setContentsMargins(0, 0, 0, 0);

        h->addWidget(leftArea, 0);
        h->addWidget(rightArea, 1);

        return std::tuple<QWidget*, QWidget*, QVBoxLayout*, QVBoxLayout*>(page, leftArea, leftLayout, rightLayout);
    };

    QWidget* strokePage = nullptr;
    QWidget* strokeLeftArea = nullptr;
    QVBoxLayout* strokeLeft = nullptr;
    QVBoxLayout* strokeRight = nullptr;
    std::tie(strokePage, strokeLeftArea, strokeLeft, strokeRight) = makeSplitPage(mCategoryStack);
    CosmicSplitToolLayout strokeSplit(strokeLeft, strokeRight, strokeLeftArea);

    auto makeIntSlider = [](QWidget* parent, int min, int max) {
        auto* s = new CosmicHSlider(parent);
        s->setRange(min, max);
        s->setSingleStep(1);
        s->setPageStep(1);
        s->setTracking(true);
        s->setLabelMode(CosmicHSlider::WithLabel);
        return s;
    };

    mSpacingField = makeIntSlider(strokePage, 5, 1000);
    mScatterField = makeIntSlider(strokePage, 0, 300);
    mRotationField = makeIntSlider(strokePage, 0, 40);
    mColorJitterField = makeIntSlider(strokePage, 0, 100);

    auto syncStrokeLabels = [this]() {
        if (mSpacingField) mSpacingField->setLabel(QString::number(mSpacingField->value()));
        if (mScatterField) mScatterField->setLabel(QString::number(mScatterField->value()));
        if (mRotationField) mRotationField->setLabel(QString::number(mRotationField->value()));
        if (mColorJitterField) mColorJitterField->setLabel(QString::number(mColorJitterField->value()));
    };
    syncStrokeLabels();

    strokeSplit.createEntry(tr("Spacing"), mSpacingField);
    strokeSplit.createEntry(tr("Scatter"), mScatterField);
    strokeSplit.createEntry(tr("Rotation"), mRotationField);
    strokeSplit.createEntry(tr("Color Jitter"), mColorJitterField);

    {
        auto* scroll = new QScrollArea(mCategoryStack);
        scroll->setWidget(strokePage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mCategoryStack->addWidget(scroll);
    }

    QWidget* pressurePage = nullptr;
    QWidget* pressureLeftArea = nullptr;
    QVBoxLayout* pressureLeft = nullptr;
    QVBoxLayout* pressureRight = nullptr;
    std::tie(pressurePage, pressureLeftArea, pressureLeft, pressureRight) = makeSplitPage(mCategoryStack);
    CosmicSplitToolLayout pressureSplit(pressureLeft, pressureRight, pressureLeftArea);

    auto makePressureToggle = [](QWidget* parent) {
        auto* t = new CosmicCheckboxToggle(parent);
        t->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        t->setScaleMode(CosmicCheckboxToggle::ScaleBigBox);
        t->setShowText(false);
        t->setShowIcon(false);
        t->setAccentColor(BlenderStyleModalPopup::getAccentColor());
        return t;
    };

    mSizePressureToggle = makePressureToggle(pressurePage);
    mFlowPressureToggle = makePressureToggle(pressurePage);

    pressureSplit.createEntry(tr("Size Pressure"), mSizePressureToggle);
    pressureSplit.createEntry(tr("Flow Pressure"), mFlowPressureToggle);

    // Inline curve editors (live apply + per-preset persistence)
    mSizeCurveEditor = new BezierCurveEditor(pressurePage);
    mSizeCurveEditor->setMinimumSize(240, 180);
    mFlowCurveEditor = new BezierCurveEditor(pressurePage);
    mFlowCurveEditor->setMinimumSize(240, 180);

    // Live apply + persistence (per preset)
    QObject::connect(mSizeCurveEditor, &BezierCurveEditor::curveChanged, this,
                     [this, presetsData, activePresetIndex, presetDelegate](const QVector<QPointF>& pts) {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->setThicknessPressureCurve(pts);
            }
        }
    });

    QObject::connect(mSizeCurveEditor, &BezierCurveEditor::curveCommitted, this,
                     [this, presetsData, activePresetIndex, presetDelegate](const QVector<QPointF>& pts) {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.thicknessCurvePoints = pts;
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    QObject::connect(mFlowCurveEditor, &BezierCurveEditor::curveChanged, this,
                     [this, presetsData, activePresetIndex, presetDelegate](const QVector<QPointF>& pts) {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->setOpacityPressureCurve(pts);
            }
        }
    });

    QObject::connect(mFlowCurveEditor, &BezierCurveEditor::curveCommitted, this,
                     [this, presetsData, activePresetIndex, presetDelegate](const QVector<QPointF>& pts) {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.opacityCurvePoints = pts;
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    // Centered labels above each curve, with curve directly beneath.
    auto* sizeCurveLabel = new QLabel(tr("Size Curve"), pressurePage);
    sizeCurveLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    auto* flowCurveLabel = new QLabel(tr("Flow Curve"), pressurePage);
    flowCurveLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    pressureRight->addSpacing(6);
    pressureRight->addWidget(sizeCurveLabel);
    pressureRight->addWidget(mSizeCurveEditor);

    pressureRight->addSpacing(10);
    pressureRight->addWidget(flowCurveLabel);
    pressureRight->addWidget(mFlowCurveEditor);

    {
        auto* scroll = new QScrollArea(mCategoryStack);
        scroll->setWidget(pressurePage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mCategoryStack->addWidget(scroll);
    }

    QWidget* texturePage = nullptr;
    QWidget* textureLeftArea = nullptr;
    QVBoxLayout* textureLeft = nullptr;
    QVBoxLayout* textureRight = nullptr;
    std::tie(texturePage, textureLeftArea, textureLeft, textureRight) = makeSplitPage(mCategoryStack);
    CosmicSplitToolLayout textureSplit(textureLeft, textureRight, textureLeftArea);

    mTextureScaleField = makeIntSlider(texturePage, 25, 1600);
    mTextureContrastField = makeIntSlider(texturePage, 0, 100);

    auto syncTextureLabels = [this]() {
        if (mTextureScaleField) mTextureScaleField->setLabel(QString::number(mTextureScaleField->value()));
        if (mTextureContrastField) mTextureContrastField->setLabel(QString::number(mTextureContrastField->value()));
    };
    syncTextureLabels();

    QWidget* textureRow = new QWidget(texturePage);
    QHBoxLayout* textureRowLayout = new QHBoxLayout(textureRow);
    textureRowLayout->setContentsMargins(0, 0, 0, 0);
    textureRowLayout->setSpacing(8);

    mTexturePreview = new QLabel(textureRow);
    mTexturePreview->setFixedSize(64, 64);
    mTexturePreview->setFrameShape(QFrame::Box);
    mTexturePreview->setAlignment(Qt::AlignCenter);

    mAddTextureButton = new CosmicButton(textureRow);
    mAddTextureButton->setText(tr("Add Texture"));
    mAddTextureButton->setBordered(true);
    mAddTextureButton->setAccentColor(BlenderStyleModalPopup::getAccentColor());

    mRemoveTextureButton = new CosmicButton(textureRow);
    mRemoveTextureButton->setIcon(QIcon(":/Interaction/CloseWindow.svg"));
    mRemoveTextureButton->setIconSize(QSize(16, 16));
    mRemoveTextureButton->setToolTip(tr("Remove texture (use default)"));
    mRemoveTextureButton->setFixedSize(24, 24);

    textureRowLayout->addWidget(mTexturePreview);
    textureRowLayout->addWidget(mAddTextureButton);
    textureRowLayout->addWidget(mRemoveTextureButton);
    textureRowLayout->addStretch(1);

    textureSplit.createEntry(tr("Texture"), textureRow);
    textureSplit.createEntry(tr("Scale"), mTextureScaleField);
    textureSplit.createEntry(tr("Contrast"), mTextureContrastField);

    {
        auto* scroll = new QScrollArea(mCategoryStack);
        scroll->setWidget(texturePage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mCategoryStack->addWidget(scroll);
    }

    mResetDefaultButton = new CosmicButton(settingsWidget);
    mResetDefaultButton->setText(tr("Reset Default"));
    mResetDefaultButton->setBordered(true);
    mResetDefaultButton->setAccentColor(BlenderStyleModalPopup::getAccentColor());

    QVBoxLayout* rightOuter = new QVBoxLayout;
    rightOuter->setContentsMargins(0, 0, 0, 0);
    rightOuter->setSpacing(8);
    rightOuter->addWidget(settingsWidget, 1);
    rightOuter->addWidget(mResetDefaultButton, 0, Qt::AlignLeft);

    QWidget* settingsOuterWidget = new QWidget(bodyWidget);
    settingsOuterWidget->setLayout(rightOuter);
    contentLayout->addWidget(settingsOuterWidget, 1);

    Editor* ed = editor();
    if (ed && ed->tools()) {
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            if (presetsData->isEmpty()) {
                BrushPreset defaultPreset;
                defaultPreset.name = QStringLiteral("default");
                defaultPreset.spacingMult = 0.25;
                defaultPreset.scatter = 0.0;
                defaultPreset.rotationDeg = 0.0;
                defaultPreset.sizeJitter = 0.0;
                defaultPreset.colorJitter = 0.0;
                defaultPreset.builtIn = true;
                defaultPreset.textureTileScale = 16.0;
                defaultPreset.textureContrastPercent = 10;
                defaultPreset.textureAssetId.clear();
                QVector<QPointF> defaultCurve;
                defaultCurve.append(QPointF(0.5, 0.5));
                defaultPreset.thicknessCurvePoints = defaultCurve;
                defaultPreset.opacityCurvePoints = defaultCurve;
                defaultPreset.thicknessCurveEnabled = true;
                defaultPreset.opacityCurveEnabled = true;
                presetsData->append(defaultPreset);
                *activePresetIndex = 0;
            }

            mPresetListWidget->clear();
            for (int i = 0; i < presetsData->size(); ++i) {
                const BrushPreset& p = presetsData->at(i);
                QListWidgetItem* item = new QListWidgetItem(p.name);
                if (p.builtIn) {
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                } else {
                    item->setFlags(item->flags() | Qt::ItemIsEditable);
                }
                mPresetListWidget->addItem(item);
            }

            if (*activePresetIndex < 0 || *activePresetIndex >= presetsData->size()) {
                *activePresetIndex = 0;
            }
            mPresetListWidget->setCurrentRow(*activePresetIndex);

            const BrushPreset& active = presetsData->at(*activePresetIndex);
            mSpacingField->setValue(qRound(active.spacingMult * 100.0));
            mScatterField->setValue(qRound(active.scatter * 100.0));
            mRotationField->setValue(qRound(active.rotationDeg));
            mColorJitterField->setValue(qRound(active.colorJitter * 100.0));
            mTextureScaleField->setValue(qRound(qBound<qreal>(0.25, active.textureTileScale, 16.0) * 100.0));
            mTextureContrastField->setValue(qBound(0.0, double(active.textureContrastPercent), 100.0));
            syncStrokeLabels();
            syncTextureLabels();
            mSizePressureToggle->setChecked(active.thicknessCurveEnabled);
            mFlowPressureToggle->setChecked(active.opacityCurveEnabled);

            if (mSizeCurveEditor) {
                QSignalBlocker b(mSizeCurveEditor);
                mSizeCurveEditor->setControlPoints(active.thicknessCurvePoints);
            }
            if (mFlowCurveEditor) {
                QSignalBlocker b(mFlowCurveEditor);
                mFlowCurveEditor->setControlPoints(active.opacityCurvePoints);
            }

            pencil->setUserSpacingMult(active.spacingMult);
            pencil->setScatterAmount(active.scatter);
            pencil->setRotationJitterRangeDegrees(active.rotationDeg);
            pencil->setSizeJitterAmount(active.sizeJitter);
            pencil->setColorJitterAmount(active.colorJitter);
            pencil->setTextureTileScale(active.textureTileScale);
            pencil->setTextureContrastPercent(active.textureContrastPercent);
            // Apply per-preset curves when available; otherwise keep existing global ones
            if (!active.thicknessCurvePoints.isEmpty()) {
                pencil->setThicknessPressureCurve(active.thicknessCurvePoints);
            }
            if (!active.opacityCurvePoints.isEmpty()) {
                pencil->setOpacityPressureCurve(active.opacityCurvePoints);
            }
            pencil->setThicknessCurveEnabled(active.thicknessCurveEnabled);
            pencil->setOpacityCurveEnabled(active.opacityCurveEnabled);
            if (!active.textureAssetId.isEmpty()) {
                pencil->setCustomTextureFromData(BrushAssetStore::instance()->loadTexture(active.textureAssetId));
            } else {
                active.builtIn ? pencil->clearCustomTexture() : pencil->setPlainWhiteTexture();
            }
            pencil->setTipShape(active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Soft
                                ? PencilTool::CosmicTipShape::Soft
                                : active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom
                                ? PencilTool::CosmicTipShape::Custom
                                : PencilTool::CosmicTipShape::Hard);
            if (active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom && !active.tipAssetId.isEmpty())
                pencil->setCustomTipFromData(BrushAssetStore::instance()->loadTip(active.tipAssetId));
            pencil->setTipContrastPercent(active.tipContrastPercent);
            pencil->saveSettings();

            QImage tex = pencil->currentTextureForPreview();
            if (!tex.isNull()) {
                mTexturePreview->setPixmap(QPixmap::fromImage(tex).scaled(mTexturePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                mTexturePreview->setPixmap(QPixmap());
            }
        }
    }

    auto isPresetCanonical = [presetsData](int index) -> bool {
        if (!presetsData || presetsData->isEmpty() || index < 0 || index >= presetsData->size()) {
            return true;
        }

        const BrushPreset& d = presetsData->at(index);

        auto normalizeName = [](const QString& in) {
            QString s = in;
            s.remove(' ');
            return s.trimmed().toLower();
        };

        const QString key = normalizeName(d.name);

        qreal kCanonicalSpacingMult = 0.25;
        qreal kCanonicalScatter = 0.0;
        qreal kCanonicalRotationDeg = 0.0;
        qreal kCanonicalSizeJitter = 0.0;
        qreal kCanonicalColorJitter = 0.0;
        qreal kCanonicalTextureScale = 16.0;
        int kCanonicalTextureContrast = 10;
        bool canonicalWantsTexture = false;

        if (key == QStringLiteral("chalkflow")) {
            canonicalWantsTexture = true;
            kCanonicalTextureContrast = 5;
        } else if (key == QStringLiteral("clawingcore")) {
            canonicalWantsTexture = true;
            kCanonicalColorJitter = 0.50;
            kCanonicalTextureContrast = 15;
        } else if (key == QStringLiteral("crackedmason")) {
            canonicalWantsTexture = true;
            kCanonicalTextureContrast = 30;
        } else if (key == QStringLiteral("nightnudge")) {
            canonicalWantsTexture = true;
            kCanonicalSpacingMult = 1.60;
            kCanonicalRotationDeg = 40.0;
            kCanonicalColorJitter = 0.25;
            kCanonicalTextureContrast = 50;
        }

        bool spacingOk = qFuzzyCompare(d.spacingMult, kCanonicalSpacingMult);
        bool scatterOk = qFuzzyCompare(d.scatter, kCanonicalScatter);
        bool rotationOk = qFuzzyCompare(d.rotationDeg, kCanonicalRotationDeg);
        bool sizeOk = qFuzzyCompare(d.sizeJitter, kCanonicalSizeJitter);
        bool colorOk = qFuzzyCompare(d.colorJitter, kCanonicalColorJitter);
        bool texScaleOk = qFuzzyCompare(d.textureTileScale, kCanonicalTextureScale);
        bool texContrastOk = (d.textureContrastPercent == kCanonicalTextureContrast);

        auto isDefaultCurve = [](const QVector<QPointF>& pts) {
            if (pts.size() != 1) return false;
            const QPointF& p = pts.front();
            return qFuzzyCompare(p.x(), 0.5) && qFuzzyCompare(p.y(), 0.5);
        };

        bool thicknessCurveOk = isDefaultCurve(d.thicknessCurvePoints);
        bool opacityCurveOk = isDefaultCurve(d.opacityCurvePoints);
        bool thicknessEnabledOk = (d.thicknessCurveEnabled == true);
        bool opacityEnabledOk = (d.opacityCurveEnabled == true);

        bool textureOk = canonicalWantsTexture ? (!d.textureAssetId.isEmpty()) : (d.textureAssetId.isEmpty());

        return spacingOk && scatterOk && rotationOk && sizeOk && colorOk &&
               texScaleOk && texContrastOk && textureOk &&
               thicknessCurveOk && opacityCurveOk && thicknessEnabledOk && opacityEnabledOk;
    };

    const bool presetModified = !isPresetCanonical(*activePresetIndex);
    mResetDefaultButton->setVisible(presetModified);
    mResetDefaultButton->setEnabled(presetModified);

    bool canRemoveInitial = false;
    bool canChangeTextureInitial = false;
    if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
        const BrushPreset& p = presetsData->at(*activePresetIndex);
        canRemoveInitial = (!p.builtIn && presetsData->size() > 1);
        canChangeTextureInitial = (!p.builtIn);
    }
    mRemovePresetButton->setEnabled(canRemoveInitial);
    mAddTextureButton->setVisible(canChangeTextureInitial);
    mRemoveTextureButton->setVisible(canChangeTextureInitial);

    QObject::connect(mPresetListWidget, &QListWidget::currentRowChanged, this,
                     [this, presetsData, activePresetIndex, presetDelegate, isPresetCanonical](int row) {
        if (!presetsData || presetsData->isEmpty()) return;
        if (row < 0 || row >= presetsData->size()) return;

        int oldIndex = *activePresetIndex;
        if (oldIndex != row) {
            emit activePresetAboutToChange(oldIndex, row);
        }
        *activePresetIndex = row;

        const BrushPreset& p = presetsData->at(row);
        {
            QSignalBlocker b1(mSpacingField);
            QSignalBlocker b2(mScatterField);
            QSignalBlocker b3(mRotationField);
            QSignalBlocker b4(mColorJitterField);
            QSignalBlocker b5(mTextureScaleField);
            QSignalBlocker b6(mTextureContrastField);
            QSignalBlocker b7(mSizePressureToggle);
            QSignalBlocker b8(mFlowPressureToggle);

            mSpacingField->setValue(qRound(p.spacingMult * 100.0));
            mScatterField->setValue(qRound(p.scatter * 100.0));
            mRotationField->setValue(qRound(p.rotationDeg));
            mColorJitterField->setValue(qRound(p.colorJitter * 100.0));
            mTextureScaleField->setValue(qRound(qBound<qreal>(0.25, p.textureTileScale, 16.0) * 100.0));
            mTextureContrastField->setValue(qBound(0.0, double(p.textureContrastPercent), 100.0));
            mSizePressureToggle->setChecked(p.thicknessCurveEnabled);
            mFlowPressureToggle->setChecked(p.opacityCurveEnabled);
        }

        if (mSizeCurveEditor) {
            QSignalBlocker b(mSizeCurveEditor);
            mSizeCurveEditor->setControlPoints(p.thicknessCurvePoints);
        }
        if (mFlowCurveEditor) {
            QSignalBlocker b(mFlowCurveEditor);
            mFlowCurveEditor->setControlPoints(p.opacityCurvePoints);
        }

        if (Editor* edLocal = editor()) {
            if (edLocal->tools()) {
                if (auto* pencilLocal = qobject_cast<PencilTool*>(edLocal->tools()->getTool(PENCIL))) {
                    pencilLocal->setUserSpacingMult(p.spacingMult);
                    pencilLocal->setScatterAmount(p.scatter);
                    pencilLocal->setRotationJitterRangeDegrees(p.rotationDeg);
                    pencilLocal->setSizeJitterAmount(p.sizeJitter);
                    pencilLocal->setColorJitterAmount(p.colorJitter);
                    pencilLocal->setTextureTileScale(p.textureTileScale);
                    pencilLocal->setTextureContrastPercent(p.textureContrastPercent);
                    if (!p.thicknessCurvePoints.isEmpty()) {
                        pencilLocal->setThicknessPressureCurve(p.thicknessCurvePoints);
                    }
                    if (!p.opacityCurvePoints.isEmpty()) {
                        pencilLocal->setOpacityPressureCurve(p.opacityCurvePoints);
                    }
                    pencilLocal->setThicknessCurveEnabled(p.thicknessCurveEnabled);
                    pencilLocal->setOpacityCurveEnabled(p.opacityCurveEnabled);
                    if (!p.textureAssetId.isEmpty()) {
                        pencilLocal->setCustomTextureFromData(BrushAssetStore::instance()->loadTexture(p.textureAssetId));
                    } else {
                        p.builtIn ? pencilLocal->clearCustomTexture() : pencilLocal->setPlainWhiteTexture();
                    }
                    pencilLocal->setTipShape(p.tipMode == ToolOptionWidget::BrushPreset::TipMode::Soft
                                            ? PencilTool::CosmicTipShape::Soft
                                            : p.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom
                                            ? PencilTool::CosmicTipShape::Custom
                                            : PencilTool::CosmicTipShape::Hard);
                    if (p.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom && !p.tipAssetId.isEmpty())
                        pencilLocal->setCustomTipFromData(BrushAssetStore::instance()->loadTip(p.tipAssetId));
                    pencilLocal->setTipContrastPercent(p.tipContrastPercent);
                    pencilLocal->saveSettings();
                }
            }
        }

        // Update texture preview from the current pencil texture
        if (Editor* edLocal2 = editor()) {
            if (edLocal2->tools()) {
                if (auto* pencilTex = qobject_cast<PencilTool*>(edLocal2->tools()->getTool(PENCIL))) {
                    QImage tex = pencilTex->currentTextureForPreview();
                    if (!tex.isNull()) {
                        mTexturePreview->setPixmap(QPixmap::fromImage(tex).scaled(mTexturePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    } else {
                        mTexturePreview->setPixmap(QPixmap());
                    }
                }
            }
        }

        const bool presetModifiedLocal = !isPresetCanonical(row);
        mResetDefaultButton->setVisible(presetModifiedLocal);
        mResetDefaultButton->setEnabled(presetModifiedLocal);

        const BrushPreset& activePreset = presetsData->at(row);
        const bool canRemove = (!activePreset.builtIn && presetsData->size() > 1);
        const bool canChangeTexture = (!activePreset.builtIn);
        mRemovePresetButton->setEnabled(canRemove);
        mAddTextureButton->setVisible(canChangeTexture);
        mRemoveTextureButton->setVisible(canChangeTexture);

        if (presetDelegate) {
            presetDelegate->markRowDirty(row);
            mPresetListWidget->viewport()->update();
        }
    });

    mPresetListWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    QObject::connect(mPresetListWidget, &QListWidget::itemChanged, this,
                     [this, presetsData](QListWidgetItem* item) {
        if (!presetsData || presetsData->isEmpty() || !item) return;

        int row = mPresetListWidget->row(item);
        if (row < 0 || row >= presetsData->size()) {
            return;
        }

        BrushPreset& preset = (*presetsData)[row];
        if (preset.builtIn) {
            item->setText(preset.name);
            return;
        }

        QString newName = item->text().trimmed();
        if (newName.isEmpty()) {
            item->setText(preset.name);
            return;
        }
        if (preset.name == newName) {
            return;
        }

        preset.name = newName;
        mPresetListWidget->viewport()->update();
        savePresets();
    });

    // Re-evaluate canonical state after curve commits and refresh Reset button visibility.
    QObject::connect(mSizeCurveEditor, &BezierCurveEditor::curveCommitted, this,
                     [this, activePresetIndex, isPresetCanonical](const QVector<QPointF>&) {
        const bool modified = !isPresetCanonical(*activePresetIndex);
        mResetDefaultButton->setVisible(modified);
        mResetDefaultButton->setEnabled(modified);
    });
    QObject::connect(mFlowCurveEditor, &BezierCurveEditor::curveCommitted, this,
                     [this, activePresetIndex, isPresetCanonical](const QVector<QPointF>&) {
        const bool modified = !isPresetCanonical(*activePresetIndex);
        mResetDefaultButton->setVisible(modified);
        mResetDefaultButton->setEnabled(modified);
    });

    QObject::connect(mAddPresetButton, &CosmicButton::clicked, this,
                     [this, presetsData, activePresetIndex, isPresetCanonical, presetDelegate]() {
        if (!presetsData) {
            return;
        }

        BrushPreset newPreset;
        if (!presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            newPreset = presetsData->at(*activePresetIndex);
        } else {
            newPreset.name = tr("Preset %1").arg(presetsData->size());
            newPreset.spacingMult = 0.25;
            newPreset.scatter = 0.0;
            newPreset.rotationDeg = 0.0;
            newPreset.sizeJitter = 0.0;
            newPreset.colorJitter = 0.0;
            newPreset.textureTileScale = 12.0;
            newPreset.textureContrastPercent = 50;
            newPreset.textureAssetId.clear();
            QVector<QPointF> defaultCurve;
            defaultCurve.append(QPointF(0.5, 0.5));
            newPreset.thicknessCurvePoints = defaultCurve;
            newPreset.opacityCurvePoints = defaultCurve;
            newPreset.thicknessCurveEnabled = true;
            newPreset.opacityCurveEnabled = true;
        }

        // New presets are always user presets.
        newPreset.builtIn = false;

        newPreset.name = tr("Preset %1").arg(presetsData->size());

        presetsData->append(newPreset);
        QListWidgetItem* item = new QListWidgetItem(newPreset.name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        mPresetListWidget->addItem(item);

        int row = presetsData->size() - 1;
        mPresetListWidget->setCurrentRow(row);
        *activePresetIndex = row;

        mRemovePresetButton->setEnabled(presetsData->size() > 1 && !newPreset.builtIn);

        const bool presetModifiedAfterAdd = !isPresetCanonical(*activePresetIndex);
        mResetDefaultButton->setVisible(presetModifiedAfterAdd);
        mResetDefaultButton->setEnabled(presetModifiedAfterAdd);

        const bool canChangeTexture = (!newPreset.builtIn);
        mAddTextureButton->setVisible(canChangeTexture);
        mRemoveTextureButton->setVisible(canChangeTexture);

        if (presetDelegate) {
            presetDelegate->clearTextureCaches();
            mPresetListWidget->viewport()->update();
        }

        savePresets();
    });

    QObject::connect(mRemovePresetButton, &CosmicButton::clicked, this,
                     [this, presetsData, activePresetIndex, isPresetCanonical, presetDelegate]() {
        if (!presetsData || presetsData->size() <= 1) return;
        int row = mPresetListWidget->currentRow();
        if (row < 0 || row >= presetsData->size()) return;
        if (presetsData->at(row).builtIn) return;
        delete mPresetListWidget->takeItem(row);
        presetsData->remove(row);
        if (row >= presetsData->size()) {
            row = presetsData->size() - 1;
        }
        if (row < 0) {
            row = 0;
        }
        mPresetListWidget->setCurrentRow(row);
        *activePresetIndex = row;

        const bool presetModifiedAfterRemove = !isPresetCanonical(*activePresetIndex);
        mResetDefaultButton->setVisible(presetModifiedAfterRemove);
        mResetDefaultButton->setEnabled(presetModifiedAfterRemove);

        bool canRemove = false;
        bool canChangeTexture = false;
        if (*activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            const BrushPreset& p = presetsData->at(*activePresetIndex);
            canRemove = (!p.builtIn && presetsData->size() > 1);
            canChangeTexture = (!p.builtIn);
        }
        mRemovePresetButton->setEnabled(canRemove);
        mAddTextureButton->setVisible(canChangeTexture);
        mRemoveTextureButton->setVisible(canChangeTexture);

        if (presetDelegate) {
            presetDelegate->clearTextureCaches();
            mPresetListWidget->viewport()->update();
        }

        savePresets();
    });

    connect(mResetDefaultButton, &CosmicButton::clicked, this,
            [this, presetsData, activePresetIndex, presetDelegate, isPresetCanonical]() {
        if (!presetsData || presetsData->isEmpty()) return;
        if (*activePresetIndex < 0 || *activePresetIndex >= presetsData->size()) return;

        auto normalizeName = [](const QString& in) {
            QString s = in;
            s.remove(' ');
            return s.trimmed().toLower();
        };

        BrushPreset& p = (*presetsData)[*activePresetIndex];
        const QString key = normalizeName(p.name);

        qreal kCanonicalSpacingMult = 0.25;
        qreal kCanonicalScatter = 0.0;
        qreal kCanonicalRotationDeg = 0.0;
        qreal kCanonicalSizeJitter = 0.0;
        qreal kCanonicalColorJitter = 0.0;
        qreal kCanonicalTextureScale = 16.0;
        int kCanonicalTextureContrast = 10;
        QString kCanonicalTextureAssetId;

        if (key == QStringLiteral("chalkflow")) {
            kCanonicalTextureContrast = 5;
            kCanonicalTextureAssetId = BrushAssetStore::builtinTextureId(QStringLiteral("ChalkFlow"));
        } else if (key == QStringLiteral("clawingcore")) {
            kCanonicalColorJitter = 0.50;
            kCanonicalTextureContrast = 15;
            kCanonicalTextureAssetId = BrushAssetStore::builtinTextureId(QStringLiteral("ClawingCore"));
        } else if (key == QStringLiteral("crackedmason")) {
            kCanonicalTextureContrast = 30;
            kCanonicalTextureAssetId = BrushAssetStore::builtinTextureId(QStringLiteral("CrackedMason"));
        } else if (key == QStringLiteral("nightnudge")) {
            kCanonicalSpacingMult = 1.60;
            kCanonicalRotationDeg = 40.0;
            kCanonicalColorJitter = 0.25;
            kCanonicalTextureContrast = 50;
            kCanonicalTextureAssetId = BrushAssetStore::builtinTextureId(QStringLiteral("NightNudge"));
        }

        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->setUserSpacingMult(kCanonicalSpacingMult);
                pencil->setScatterAmount(kCanonicalScatter);
                pencil->setRotationJitterRangeDegrees(kCanonicalRotationDeg);
                pencil->setSizeJitterAmount(kCanonicalSizeJitter);
                pencil->setColorJitterAmount(kCanonicalColorJitter);
                pencil->setTextureTileScale(kCanonicalTextureScale);
                pencil->setTextureContrastPercent(kCanonicalTextureContrast);

                QVector<QPointF> defaultCurve;
                defaultCurve.append(QPointF(0.5, 0.5));
                pencil->setThicknessPressureCurve(defaultCurve);
                pencil->setOpacityPressureCurve(defaultCurve);
                pencil->setThicknessCurveEnabled(true);
                pencil->setOpacityCurveEnabled(true);

                if (!kCanonicalTextureAssetId.isEmpty()) {
                    pencil->setCustomTextureFromData(BrushAssetStore::instance()->loadTexture(kCanonicalTextureAssetId));
                } else {
                    pencil->clearCustomTexture();
                }
                pencil->saveSettings();

                {
                    QSignalBlocker b1(mSpacingField);
                    QSignalBlocker b2(mScatterField);
                    QSignalBlocker b3(mRotationField);
                    QSignalBlocker b4(mColorJitterField);
                    QSignalBlocker b5(mTextureScaleField);
                    QSignalBlocker b6(mTextureContrastField);
                    QSignalBlocker b7(mSizePressureToggle);
                    QSignalBlocker b8(mFlowPressureToggle);
                    mSpacingField->setValue(qRound(kCanonicalSpacingMult * 100.0));
                    mScatterField->setValue(qRound(kCanonicalScatter * 100.0));
                    mRotationField->setValue(qRound(kCanonicalRotationDeg));
                    mColorJitterField->setValue(qRound(kCanonicalColorJitter * 100.0));
                    mTextureScaleField->setValue(qRound(kCanonicalTextureScale * 100.0));
                    mTextureContrastField->setValue(kCanonicalTextureContrast);
                    mSizePressureToggle->setChecked(true);
                    mFlowPressureToggle->setChecked(true);
                }

                if (mSizeCurveEditor) {
                    QSignalBlocker b(mSizeCurveEditor);
                    mSizeCurveEditor->setControlPoints(defaultCurve);
                }
                if (mFlowCurveEditor) {
                    QSignalBlocker b(mFlowCurveEditor);
                    mFlowCurveEditor->setControlPoints(defaultCurve);
                }

                // Reset the currently active preset's sliders/curves to baseline
                p.spacingMult = kCanonicalSpacingMult;
                p.scatter = kCanonicalScatter;
                p.rotationDeg = kCanonicalRotationDeg;
                p.sizeJitter = kCanonicalSizeJitter;
                p.colorJitter = kCanonicalColorJitter;
                p.textureTileScale = kCanonicalTextureScale;
                p.textureContrastPercent = kCanonicalTextureContrast;
                p.textureAssetId = kCanonicalTextureAssetId;
                p.thicknessCurvePoints = defaultCurve;
                p.opacityCurvePoints = defaultCurve;
                p.thicknessCurveEnabled = true;
                p.opacityCurveEnabled = true;

                QImage tex = pencil->currentTextureForPreview();
                if (!tex.isNull()) {
                    mTexturePreview->setPixmap(QPixmap::fromImage(tex).scaled(mTexturePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    mTexturePreview->setPixmap(QPixmap());
                }

                if (presetDelegate) {
                    presetDelegate->markRowDirty(*activePresetIndex);
                    mPresetListWidget->viewport()->update();
                }
            }
        }

        const bool presetModifiedAfterReset = !isPresetCanonical(*activePresetIndex);
        mResetDefaultButton->setVisible(presetModifiedAfterReset);
        mResetDefaultButton->setEnabled(presetModifiedAfterReset);

        savePresets();
    });

    connect(mAddTextureButton, &CosmicButton::clicked, this,
            [this, presetsData, activePresetIndex, presetDelegate]() {
        if (!presetsData || presetsData->isEmpty()) return;
        if (!activePresetIndex) return;
        int index = *activePresetIndex;
        if (index <= 0 || index >= presetsData->size()) return;

        if (presetsData->at(index).builtIn) {
            return;
        }

        QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (startDir.isEmpty()) {
            startDir = QDir::homePath();
        }

        const QString filePath = FileDialog::getOpenFileName(
            this,
            FileType::IMAGE,
            tr("Select Brush Texture"));
        if (filePath.isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        QByteArray data = file.readAll();
        file.close();
        if (data.isEmpty()) {
            return;
        }

        BrushPreset& preset = (*presetsData)[index];
        preset.textureAssetId = BrushAssetStore::instance()->importTexture(data);

        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            if (!pencil->setCustomTextureFromData(BrushAssetStore::instance()->loadTexture(preset.textureAssetId))) {
                return;
            }
            pencil->saveSettings();

            QImage tex = pencil->currentTextureForPreview();
            if (!tex.isNull()) {
                mTexturePreview->setPixmap(QPixmap::fromImage(tex).scaled(mTexturePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                mTexturePreview->setPixmap(QPixmap());
            }
        }

        savePresets();
        if (presetDelegate) {
            presetDelegate->clearTextureCaches();
            mPresetListWidget->viewport()->update();
        }
    });

    connect(mRemoveTextureButton, &CosmicButton::clicked, this,
            [this, presetsData, activePresetIndex, presetDelegate]() {
        if (!presetsData || presetsData->isEmpty()) return;
        if (!activePresetIndex) return;
        int index = *activePresetIndex;
        if (index <= 0 || index >= presetsData->size()) return;

        if (presetsData->at(index).builtIn) {
            return;
        }

        BrushPreset& preset = (*presetsData)[index];
        preset.textureAssetId.clear();

        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            pencil->clearCustomTexture();
            pencil->saveSettings();

            QImage tex = pencil->currentTextureForPreview();
            if (!tex.isNull()) {
                mTexturePreview->setPixmap(QPixmap::fromImage(tex).scaled(mTexturePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                mTexturePreview->setPixmap(QPixmap());
            }
        }

        if (presetDelegate) {
            presetDelegate->clearTextureCaches();
            mPresetListWidget->viewport()->update();
        }

        savePresets();
    });

    connect(mSpacingField, &QSlider::valueChanged, this, [this](int value) {
        if (mSpacingField) mSpacingField->setLabel(QString::number(value));
        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            const qreal spacing = qBound<qreal>(0.05, qreal(value) / 100.0, 10.0);
            pencil->setUserSpacingMult(spacing);
        }
    });

    connect(mSpacingField, &QSlider::sliderReleased, this, [this, presetsData, activePresetIndex, presetDelegate](void) {
        const int value = mSpacingField ? mSpacingField->value() : 0;
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.spacingMult = qBound<qreal>(0.05, qreal(value) / 100.0, 10.0);
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    connect(mScatterField, &QSlider::valueChanged, this, [this](int value) {
        if (mScatterField) mScatterField->setLabel(QString::number(value));
        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            const qreal scatter = qBound<qreal>(0.0, qreal(value) / 100.0, 3.0);
            pencil->setScatterAmount(scatter);
        }
    });

    connect(mScatterField, &QSlider::sliderReleased, this, [this, presetsData, activePresetIndex, presetDelegate](void) {
        const int value = mScatterField ? mScatterField->value() : 0;
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.scatter = qBound<qreal>(0.0, qreal(value) / 100.0, 3.0);
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    connect(mRotationField, &QSlider::valueChanged, this, [this](int value) {
        if (mRotationField) mRotationField->setLabel(QString::number(value));
        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            const qreal rot = qBound<qreal>(0.0, qreal(value), 40.0);
            pencil->setRotationJitterRangeDegrees(rot);
        }
    });

    connect(mRotationField, &QSlider::sliderReleased, this, [this, presetsData, activePresetIndex, presetDelegate](void) {
        const int value = mRotationField ? mRotationField->value() : 0;
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.rotationDeg = qBound<qreal>(0.0, qreal(value), 40.0);
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    connect(mColorJitterField, &QSlider::valueChanged, this, [this](int value) {
        if (mColorJitterField) mColorJitterField->setLabel(QString::number(value));
        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            const qreal jitter = qBound<qreal>(0.0, qreal(value) / 100.0, 1.0);
            pencil->setColorJitterAmount(jitter);
        }
    });

    connect(mColorJitterField, &QSlider::sliderReleased, this, [this, presetsData, activePresetIndex, presetDelegate](void) {
        const int value = mColorJitterField ? mColorJitterField->value() : 0;
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.colorJitter = qBound<qreal>(0.0, qreal(value) / 100.0, 1.0);
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    connect(mTextureScaleField, &QSlider::valueChanged, this, [this](int value) {
        if (mTextureScaleField) mTextureScaleField->setLabel(QString::number(value));
        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            const qreal scale = qBound<qreal>(0.25, qreal(value) / 100.0, 16.0);
            pencil->setTextureTileScale(scale);
        }
    });

    connect(mTextureScaleField, &QSlider::sliderReleased, this, [this, presetsData, activePresetIndex, presetDelegate](void) {
        const int value = mTextureScaleField ? mTextureScaleField->value() : 0;
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.textureTileScale = qBound<qreal>(0.25, qreal(value) / 100.0, 16.0);
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    connect(mTextureContrastField, &QSlider::valueChanged, this, [this](int value) {
        if (mTextureContrastField) mTextureContrastField->setLabel(QString::number(value));
        Editor* ed = editor();
        if (!ed || !ed->tools()) return;
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            pencil->setTextureContrastPercent(qBound(0, value, 100));
        }
    });

    connect(mTextureContrastField, &QSlider::sliderReleased, this, [this, presetsData, activePresetIndex, presetDelegate](void) {
        const int value = mTextureContrastField ? mTextureContrastField->value() : 0;
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.textureContrastPercent = qBound(0, value, 100);
            savePresets();
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
        }
    });

    auto* pressureCommitTimer = new QTimer(this);
    pressureCommitTimer->setSingleShot(true);
    pressureCommitTimer->setInterval(25);
    QObject::connect(pressureCommitTimer, &QTimer::timeout, this, [this]() {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->saveSettings();
            }
        }
        savePresets();
    });

    connect(mSizePressureToggle, &CosmicCheckboxToggle::toggled, this,
            [this, presetsData, activePresetIndex, presetDelegate, isPresetCanonical, pressureCommitTimer](bool on) {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->setThicknessCurveEnabled(on);
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.thicknessCurveEnabled = on;
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
            const bool presetModifiedLocal = !isPresetCanonical(*activePresetIndex);
            mResetDefaultButton->setVisible(presetModifiedLocal);
            mResetDefaultButton->setEnabled(presetModifiedLocal);
        }

        pressureCommitTimer->start();
    });

    connect(mFlowPressureToggle, &CosmicCheckboxToggle::toggled, this,
            [this, presetsData, activePresetIndex, presetDelegate, isPresetCanonical, pressureCommitTimer](bool on) {
        Editor* ed = editor();
        if (ed && ed->tools()) {
            if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
                pencil->setOpacityCurveEnabled(on);
            }
        }

        if (presetsData && !presetsData->isEmpty() && *activePresetIndex >= 0 && *activePresetIndex < presetsData->size()) {
            BrushPreset& p = (*presetsData)[*activePresetIndex];
            p.opacityCurveEnabled = on;
            if (presetDelegate) {
                presetDelegate->markRowDirty(*activePresetIndex);
                mPresetListWidget->viewport()->update();
            }
            const bool presetModifiedLocal = !isPresetCanonical(*activePresetIndex);
            mResetDefaultButton->setVisible(presetModifiedLocal);
            mResetDefaultButton->setEnabled(presetModifiedLocal);
        }

        pressureCommitTimer->start();
    });

    QVBoxLayout* panelLayout = new QVBoxLayout;
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);
    panelLayout->addWidget(content);
    content->setParent(this);
    setDockContentWidget(content);
}

void BrushFactoryDock::updateUI()
{
}

void BrushFactoryDock::loadPresets()
{
    // BrushPresetStore is the single source of truth for preset persistence.
    // Delegate entirely — do not read QSettings directly or bootstrap here.
    auto* store = BrushPresetStore::instance();
    store->ensureLoaded();

    const QVector<ToolOptionWidget::BrushPreset> presets = store->presets();
    mPresets = presets;

    const QString activeId = store->activePresetId();
    mActivePresetIndex = 0;
    for (int i = 0; i < mPresets.size(); ++i) {
        if (mPresets[i].id == activeId) {
            mActivePresetIndex = i;
            break;
        }
    }
}

void BrushFactoryDock::savePresets()
{
    // BrushPresetStore is the single source of truth.
    // All mutations go through the store which handles its own persistence.
    // This function is intentionally a no-op.
}

void BrushFactoryDock::onPresetCurvesCommitted(int index,
                                               const QVector<QPointF>& thickness,
                                               const QVector<QPointF>& opacity,
                                               bool thicknessEnabled,
                                               bool opacityEnabled)
{
    if (index < 0 || index >= mPresets.size()) {
        return;
    }

    ToolOptionWidget::BrushPreset& preset = mPresets[index];
    preset.thicknessCurvePoints = thickness;
    preset.opacityCurvePoints = opacity;
    preset.thicknessCurveEnabled = thicknessEnabled;
    preset.opacityCurveEnabled = opacityEnabled;

    savePresets();
}

void BrushFactoryDock::setActivePreset(int index)
{
    if (index < 0 || index >= mPresets.size()) {
        return;
    }

    qDebug() << "[BrushFactoryDock] setActivePreset index=" << index
             << "count=" << mPresets.size()
             << "name=" << mPresets.at(index).name;
    
    int oldIndex = mActivePresetIndex;
    if (oldIndex != index) {
        emit activePresetAboutToChange(oldIndex, index);
    }
    mActivePresetIndex = index;
    
    // Update the UI to reflect the new selection
    if (mPresetListWidget) {
        mPresetListWidget->setCurrentRow(index);
    }
    
    // Persist through the store (single source of truth) — this also emits
    // presetsUpdated so BrushPresetListWidget stays in sync.
    BrushPresetStore::instance()->setActivePresetId(mPresets.at(index).id);

    // Normal presets imply brush-mode (not Final Line).
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.beginGroup("PencilBrushFactory");
        settings.setValue("FinalLineActive", false);
        settings.endGroup();
        settings.sync();
    }

    // UI is source of truth: if a normal brush preset is selected while the user is
    // still on Pen (eg. after using Final Line), switch back to Pencil so the stroke
    // pipeline matches the preset UI. Do not force-switch other tools.
    if (Editor* ed = editor()) {
        if (ed->tools()) {
            if (BaseTool* cur = ed->tools()->currentTool()) {
                if (cur->type() == PEN) {
                    ed->tools()->setCurrentTool(PENCIL);
                }
            }
        }
    }

    // Ensure the active preset parameters are applied immediately.
    Editor* ed = editor();
    if (ed && ed->tools()) {
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            const ToolOptionWidget::BrushPreset& active = mPresets.at(mActivePresetIndex);

            // Per-preset opacity (stored outside preset groups so it survives reorder/save).
            {
                int percent = 100;
                if (!active.id.isEmpty()) {
                    QSettings s(PENCIL2D, PENCIL2D);
                    s.beginGroup("PencilBrushFactoryOpacityById");
                    s.beginGroup(active.id);
                    percent = qBound(0, s.value("OpacityPercent", 100).toInt(), 100);
                    s.endGroup();
                    s.endGroup();
                }
                const qreal op = qBound<qreal>(0.0, qreal(percent) / 100.0, 1.0);
                pencil->setPencilOpacity(op);
            }

            pencil->setUserSpacingMult(active.spacingMult);
            pencil->setScatterAmount(active.scatter);
            pencil->setRotationJitterRangeDegrees(active.rotationDeg);
            pencil->setSizeJitterAmount(active.sizeJitter);
            pencil->setColorJitterAmount(active.colorJitter);
            pencil->setTextureTileScale(active.textureTileScale);
            pencil->setTextureContrastPercent(active.textureContrastPercent);
            if (!active.thicknessCurvePoints.isEmpty()) {
                pencil->setThicknessPressureCurve(active.thicknessCurvePoints);
            }
            if (!active.opacityCurvePoints.isEmpty()) {
                pencil->setOpacityPressureCurve(active.opacityCurvePoints);
            }
            pencil->setThicknessCurveEnabled(active.thicknessCurveEnabled);
            pencil->setOpacityCurveEnabled(active.opacityCurveEnabled);
            if (!active.textureAssetId.isEmpty()) {
                pencil->setCustomTextureFromData(BrushAssetStore::instance()->loadTexture(active.textureAssetId));
            } else {
                active.builtIn ? pencil->clearCustomTexture() : pencil->setPlainWhiteTexture();
            }
            pencil->setTipShape(active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Soft
                                ? PencilTool::CosmicTipShape::Soft
                                : active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom
                                ? PencilTool::CosmicTipShape::Custom
                                : PencilTool::CosmicTipShape::Hard);
            if (active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom && !active.tipAssetId.isEmpty())
                pencil->setCustomTipFromData(BrushAssetStore::instance()->loadTip(active.tipAssetId));
            pencil->setTipContrastPercent(active.tipContrastPercent);
            pencil->saveSettings();
        }

        // Also apply the compatible subset to PenTool, since the user may be drawing
        // with Pen while changing presets.
        if (auto* pen = qobject_cast<PenTool*>(ed->tools()->getTool(PEN))) {
            const ToolOptionWidget::BrushPreset& active = mPresets.at(mActivePresetIndex);
            pen->setPressureCurve(active.thicknessCurvePoints);
            pen->setPressureCurveEnabled(active.thicknessCurveEnabled);
            pen->saveSettings();
        }
    }
}

void BrushFactoryDock::applyExternalPresets(const QVector<ToolOptionWidget::BrushPreset>& presets, int activeIndex)
{
    if (presets.isEmpty()) {
        return;
    }

    qDebug() << "[BrushFactoryDock] applyExternalPresets count=" << presets.size()
             << "activeIndex=" << activeIndex
             << "activeName=" << presets.at(qBound(0, activeIndex, presets.size() - 1)).name;

    mPresets = presets;
    mActivePresetIndex = qBound(0, activeIndex, mPresets.size() - 1);
    savePresets();

    // Rebuild the dock list so the new presets exist as real selectable rows.
    if (mPresetListWidget) {
        QSignalBlocker b(mPresetListWidget);
        mPresetListWidget->clear();
        for (int i = 0; i < mPresets.size(); ++i) {
            const ToolOptionWidget::BrushPreset& p = mPresets.at(i);
            QListWidgetItem* item = new QListWidgetItem(p.name);
            if (p.builtIn) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            } else {
                item->setFlags(item->flags() | Qt::ItemIsEditable);
            }
            mPresetListWidget->addItem(item);
        }
    }

    // Activate (this also applies to the pencil tool)
    setActivePreset(mActivePresetIndex);
}

void BrushFactoryDock::applyStorePresets(const QVector<ToolOptionWidget::BrushPreset>& presets, int activeIndex)
{
    if (presets.isEmpty()) {
        return;
    }

    // Store is source of truth. Do NOT call savePresets() here.
    mPresets = presets;
    const int newActive = qBound(0, activeIndex, mPresets.size() - 1);

    const int oldIndex = mActivePresetIndex;
    if (oldIndex != newActive) {
        emit activePresetAboutToChange(oldIndex, newActive);
    }
    mActivePresetIndex = newActive;

    // Sync dock list selection without rebuilding items.
    if (mPresetListWidget) {
        QSignalBlocker b(mPresetListWidget);
        if (mPresetListWidget->count() == mPresets.size()) {
            // Update item text if names changed.
            for (int i = 0; i < mPresets.size(); ++i) {
                if (QListWidgetItem* it = mPresetListWidget->item(i)) {
                    const QString desired = mPresets[i].name;
                    if (it->text() != desired) {
                        it->setText(desired);
                    }
                    if (mPresets[i].builtIn) {
                        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
                    } else {
                        it->setFlags(it->flags() | Qt::ItemIsEditable);
                    }
                }
            }
        }
        mPresetListWidget->setCurrentRow(mActivePresetIndex);
    }

    // Sync sliders/toggles to active preset (no persistence).
    if (mActivePresetIndex >= 0 && mActivePresetIndex < mPresets.size()) {
        const ToolOptionWidget::BrushPreset& p = mPresets.at(mActivePresetIndex);

        if (mSpacingField) {
            QSignalBlocker b(mSpacingField);
            mSpacingField->setValue(qRound(p.spacingMult * 100.0));
            mSpacingField->setLabel(QString::number(mSpacingField->value()));
        }
        if (mScatterField) {
            QSignalBlocker b(mScatterField);
            mScatterField->setValue(qRound(p.scatter * 100.0));
            mScatterField->setLabel(QString::number(mScatterField->value()));
        }
        if (mRotationField) {
            QSignalBlocker b(mRotationField);
            mRotationField->setValue(qRound(p.rotationDeg));
            mRotationField->setLabel(QString::number(mRotationField->value()));
        }
        if (mColorJitterField) {
            QSignalBlocker b(mColorJitterField);
            mColorJitterField->setValue(qRound(p.colorJitter * 100.0));
            mColorJitterField->setLabel(QString::number(mColorJitterField->value()));
        }
        if (mTextureScaleField) {
            QSignalBlocker b(mTextureScaleField);
            mTextureScaleField->setValue(qRound(qBound<qreal>(0.25, p.textureTileScale, 16.0) * 100.0));
            mTextureScaleField->setLabel(QString::number(mTextureScaleField->value()));
        }
        if (mTextureContrastField) {
            QSignalBlocker b(mTextureContrastField);
            mTextureContrastField->setValue(qBound(0, p.textureContrastPercent, 100));
            mTextureContrastField->setLabel(QString::number(mTextureContrastField->value()));
        }
        if (mSizePressureToggle) {
            QSignalBlocker b(mSizePressureToggle);
            mSizePressureToggle->setChecked(p.thicknessCurveEnabled);
        }
        if (mFlowPressureToggle) {
            QSignalBlocker b(mFlowPressureToggle);
            mFlowPressureToggle->setChecked(p.opacityCurveEnabled);
        }
    }

    // Apply active preset parameters to tools immediately (no settings save).
    Editor* ed = editor();
    if (ed && ed->tools() && mActivePresetIndex >= 0 && mActivePresetIndex < mPresets.size()) {
        const ToolOptionWidget::BrushPreset& active = mPresets.at(mActivePresetIndex);

        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            pencil->setUserSpacingMult(active.spacingMult);
            pencil->setScatterAmount(active.scatter);
            pencil->setRotationJitterRangeDegrees(active.rotationDeg);
            pencil->setSizeJitterAmount(active.sizeJitter);
            pencil->setColorJitterAmount(active.colorJitter);
            pencil->setTextureTileScale(active.textureTileScale);
            pencil->setTextureContrastPercent(active.textureContrastPercent);
            if (!active.thicknessCurvePoints.isEmpty()) {
                pencil->setThicknessPressureCurve(active.thicknessCurvePoints);
            }
            if (!active.opacityCurvePoints.isEmpty()) {
                pencil->setOpacityPressureCurve(active.opacityCurvePoints);
            }
            pencil->setThicknessCurveEnabled(active.thicknessCurveEnabled);
            pencil->setOpacityCurveEnabled(active.opacityCurveEnabled);
            if (!active.textureAssetId.isEmpty()) {
                pencil->setCustomTextureFromData(BrushAssetStore::instance()->loadTexture(active.textureAssetId));
            } else {
                active.builtIn ? pencil->clearCustomTexture() : pencil->setPlainWhiteTexture();
            }
            pencil->setTipShape(active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Soft
                                ? PencilTool::CosmicTipShape::Soft
                                : active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom
                                ? PencilTool::CosmicTipShape::Custom
                                : PencilTool::CosmicTipShape::Hard);
            if (active.tipMode == ToolOptionWidget::BrushPreset::TipMode::Custom && !active.tipAssetId.isEmpty())
                pencil->setCustomTipFromData(BrushAssetStore::instance()->loadTip(active.tipAssetId));
            pencil->setTipContrastPercent(active.tipContrastPercent);
        }

        if (auto* pen = qobject_cast<PenTool*>(ed->tools()->getTool(PEN))) {
            pen->setPressureCurve(active.thicknessCurvePoints);
            pen->setPressureCurveEnabled(active.thicknessCurveEnabled);
            pen->saveSettings();
        }

        // Persist pencil tool's own settings keys (curve enabled flags etc.)
        if (auto* pencil = qobject_cast<PencilTool*>(ed->tools()->getTool(PENCIL))) {
            pencil->saveSettings();
        }
    }
}
