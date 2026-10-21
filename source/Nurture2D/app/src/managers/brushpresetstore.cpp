#include "brushassetstore.h"
#include "brushpresetstore.h"

#include <QSettings>
#include <QUuid>
#include <QFile>

#include "pencildef.h"

static QString normalizeBuiltinId(const QString& name)
{
    QString s = name.trimmed().toLower();
    return QStringLiteral("builtin:%1").arg(s);
}

BrushPresetStore* BrushPresetStore::instance()
{
    static BrushPresetStore* sInstance = new BrushPresetStore();
    return sInstance;
}

BrushPresetStore::BrushPresetStore(QObject* parent)
    : QObject(parent)
{
    mDeferredSaveTimer = new QTimer(this);
    mDeferredSaveTimer->setSingleShot(true);
    mDeferredSaveTimer->setInterval(150);
    connect(mDeferredSaveTimer, &QTimer::timeout, this, [this]() {
        saveToSettings();
    });
}

void BrushPresetStore::scheduleSave()
{
    if (!mDeferredSaveTimer) {
        saveToSettings();
        return;
    }
    mDeferredSaveTimer->start();
}

int BrushPresetStore::indexOfId(const QString& id) const
{
    for (int i = 0; i < mPresets.size(); ++i) {
        if (mPresets[i].id == id) {
            return i;
        }
    }
    return -1;
}

int BrushPresetStore::activeIndex() const
{
    const int idx = indexOfId(mActivePresetId);
    return idx >= 0 ? idx : 0;
}

QString BrushPresetStore::makeUniqueCopyName(const QString& base) const
{
    auto exists = [this](const QString& n) -> bool {
        for (const auto& p : mPresets) {
            if (p.name.compare(n, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    const QString copyBase = QStringLiteral("%1 copy").arg(base);
    if (!exists(copyBase)) {
        return copyBase;
    }
    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("%1 copy %2").arg(base).arg(i);
        if (!exists(candidate)) {
            return candidate;
        }
    }
    return copyBase;
}

void BrushPresetStore::ensureLoaded()
{
    if (mLoaded) {
        return;
    }

    loadFromSettings();
    mLoaded = true;
    emit presetsUpdated(mPresets, mActivePresetId);
}

void BrushPresetStore::bootstrapDefaultsToSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup("PencilBrushFactory");

    using BrushPreset = ToolOptionWidget::BrushPreset;

    QVector<QPointF> defaultCurve;
    defaultCurve.append(QPointF(0.5, 0.5));
    const bool thicknessEnabled = true;
    const bool opacityEnabled = true;

    const qreal defaultSizeJitter = 0.0;
    const qreal defaultColorJitter = 0.0;
    const qreal kCanonicalScatter = 0.0;

    QVector<BrushPreset> corePresets;

    BrushPreset defaultPreset;
    defaultPreset.id = normalizeBuiltinId(QStringLiteral("default"));
    defaultPreset.name = QStringLiteral("default");
    defaultPreset.spacingMult = 0.25;
    defaultPreset.scatter = kCanonicalScatter;
    defaultPreset.rotationDeg = 0.0;
    defaultPreset.sizeJitter = defaultSizeJitter;
    defaultPreset.colorJitter = defaultColorJitter;
    defaultPreset.builtIn = true;
    defaultPreset.textureTileScale = 16.0;
    defaultPreset.textureContrastPercent = 10;
    defaultPreset.textureAssetId.clear(); // plain white — no texture
    defaultPreset.thicknessCurvePoints = defaultCurve;
    defaultPreset.opacityCurvePoints = defaultCurve;
    defaultPreset.thicknessCurveEnabled = thicknessEnabled;
    defaultPreset.opacityCurveEnabled = opacityEnabled;
    corePresets.append(defaultPreset);

    struct CoreTexturePreset {
        const char* name;
        const char* resourcePath;
    };

    static const CoreTexturePreset kCoreTextures[] = {
        {"ChalkFlow",      ":/BrushTextures/ChalkFlow.jpeg"},
        {"ClawingCore",    ":/BrushTextures/ClawingCore.jpeg"},
        {"CrackedMason",   ":/BrushTextures/CrackedMason.jpeg"},
        {"NightNudge",     ":/BrushTextures/NightNudge.jpeg"}
    };

    for (const CoreTexturePreset& core : kCoreTextures) {
        const QString name = QString::fromLatin1(core.name);

        BrushPreset preset;
        preset.id = normalizeBuiltinId(name);
        preset.name = name;
        preset.scatter = kCanonicalScatter;
        preset.sizeJitter = defaultSizeJitter;
        preset.colorJitter = defaultColorJitter;
        preset.rotationDeg = 0.0;
        preset.spacingMult = 0.25;
        preset.textureTileScale = 16.0;
        preset.textureContrastPercent = 10;

        if (name.compare(QStringLiteral("ChalkFlow"), Qt::CaseInsensitive) == 0) {
            preset.textureContrastPercent = 5;
        } else if (name.compare(QStringLiteral("ClawingCore"), Qt::CaseInsensitive) == 0) {
            preset.colorJitter = 0.50;
            preset.textureContrastPercent = 15;
        } else if (name.compare(QStringLiteral("CrackedMason"), Qt::CaseInsensitive) == 0) {
            preset.textureContrastPercent = 30;
        } else if (name.compare(QStringLiteral("NightNudge"), Qt::CaseInsensitive) == 0) {
            preset.spacingMult = 1.60;
            preset.rotationDeg = 4.0; // 4 active angles (0°, 30°, 60°, 90°)
            preset.colorJitter = 0.25;
            preset.textureContrastPercent = 50;
        }

        preset.builtIn = true;
        preset.thicknessCurvePoints = defaultCurve;
        preset.opacityCurvePoints = defaultCurve;
        preset.thicknessCurveEnabled = thicknessEnabled;
        preset.opacityCurveEnabled = opacityEnabled;

        // Built-in textures are referenced by asset ID — no blob storage
        preset.textureAssetId = BrushAssetStore::builtinTextureId(name);

        corePresets.append(preset);
    }

    settings.remove("");
    settings.setValue("Count", corePresets.size());
    settings.setValue("ActiveIndex", 0);
    settings.setValue("ActivePresetId", corePresets[0].id);

    for (int i = 0; i < corePresets.size(); ++i) {
        const BrushPreset& p = corePresets.at(i);
        settings.beginGroup(QStringLiteral("Preset_%1").arg(i));
        settings.setValue("Id", p.id);
        settings.setValue("Name", p.name);
        settings.setValue("SpacingMult", p.spacingMult);
        settings.setValue("Scatter", p.scatter);
        settings.setValue("RotationDeg", p.rotationDeg);
        settings.setValue("SizeJitter", p.sizeJitter);
        settings.setValue("ColorJitter", p.colorJitter);
        settings.setValue("BuiltIn", p.builtIn);
        settings.setValue("TipMode", static_cast<int>(p.tipMode));
        settings.setValue("TipAssetId", p.tipAssetId);
        settings.setValue("TipContrastPercent", p.tipContrastPercent);
        settings.setValue("TextureTileScale", p.textureTileScale);
        settings.setValue("TextureContrastPercent", p.textureContrastPercent);
        settings.setValue("TextureAssetId", p.textureAssetId);

        QVariantList flatThickness;
        for (const QPointF& pt : p.thicknessCurvePoints) {
            flatThickness.append(pt.x());
            flatThickness.append(pt.y());
        }
        settings.setValue("ThicknessCurvePoints", flatThickness);

        QVariantList flatOpacity;
        for (const QPointF& pt : p.opacityCurvePoints) {
            flatOpacity.append(pt.x());
            flatOpacity.append(pt.y());
        }
        settings.setValue("OpacityCurvePoints", flatOpacity);

        settings.setValue("ThicknessCurveEnabled", p.thicknessCurveEnabled);
        settings.setValue("OpacityCurveEnabled", p.opacityCurveEnabled);
        settings.endGroup();
    }

    settings.endGroup();
    settings.sync();
}

void BrushPresetStore::loadFromSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup("PencilBrushFactory");

    int count = settings.value("Count", 0).toInt();
    int activeIndex = settings.value("ActiveIndex", 0).toInt();

    if (count == 0) {
        // Truly empty — first ever launch. Bootstrap defaults.
        settings.endGroup();
        bootstrapDefaultsToSettings();
        settings.beginGroup("PencilBrushFactory");
        count = settings.value("Count", 0).toInt();
        activeIndex = settings.value("ActiveIndex", 0).toInt();
    }

    mPresets.clear();
    mPresets.reserve(qMax(0, count));

    bool needsSave = false;

    for (int i = 0; i < count; ++i) {
        settings.beginGroup(QStringLiteral("Preset_%1").arg(i));

        const bool isDefaultPreset = (i == 0);

        ToolOptionWidget::BrushPreset preset;
        preset.id = settings.value("Id").toString();
        preset.name = settings.value("Name", isDefaultPreset ? QStringLiteral("default") : tr("Preset %1").arg(i)).toString();

        preset.spacingMult = settings.value("SpacingMult", isDefaultPreset ? 0.25 : 0.25).toDouble();
        preset.spacingMult = qBound<qreal>(0.05, preset.spacingMult, 10.0);

        preset.scatter = settings.value("Scatter", isDefaultPreset ? 0.0 : 0.0).toDouble();
        preset.scatter = qBound<qreal>(0.0, preset.scatter, 3.0);

        preset.rotationDeg = settings.value("RotationDeg", isDefaultPreset ? 0.0 : 0.0).toDouble();
        preset.rotationDeg = qBound<qreal>(0.0, preset.rotationDeg, 12.0);

        preset.sizeJitter = settings.value("SizeJitter", isDefaultPreset ? 0.0 : 0.0).toDouble();
        preset.sizeJitter = qBound<qreal>(0.0, preset.sizeJitter, 0.5);

        preset.colorJitter = settings.value("ColorJitter", isDefaultPreset ? 0.0 : 0.0).toDouble();
        preset.colorJitter = qBound<qreal>(0.0, preset.colorJitter, 1.0);

        preset.builtIn = settings.value("BuiltIn", true).toBool();

        const int tipModeInt = settings.value("TipMode", 0).toInt();
        preset.tipMode = (tipModeInt == 1) ? ToolOptionWidget::BrushPreset::TipMode::Soft
                       : (tipModeInt == 2) ? ToolOptionWidget::BrushPreset::TipMode::Custom
                       : ToolOptionWidget::BrushPreset::TipMode::Hard;

        // Load tip asset ID — migrate legacy blob if present
        preset.tipAssetId = settings.value("TipAssetId").toString();
        if (preset.tipAssetId.isEmpty()) {
            const QByteArray legacyBlob = settings.value("TipData").toByteArray();
            if (!legacyBlob.isEmpty()) {
                preset.tipAssetId = BrushAssetStore::instance()->importTip(legacyBlob);
                needsSave = true;
            }
        }
        // Validate: if the asset file no longer exists on disk, clear the stale ID.
        if (!preset.tipAssetId.isEmpty() && !BrushAssetStore::isBuiltinId(preset.tipAssetId)) {
            if (BrushAssetStore::instance()->loadTip(preset.tipAssetId).isEmpty()) {
                preset.tipAssetId.clear();
                preset.tipMode = ToolOptionWidget::BrushPreset::TipMode::Hard;
                needsSave = true;
            }
        }
        preset.tipContrastPercent = settings.value("TipContrastPercent", 50).toInt();
        preset.tipContrastPercent = qBound(0, preset.tipContrastPercent, 100);

        preset.textureTileScale = settings.value("TextureTileScale", 16.0).toDouble();
        preset.textureTileScale = qBound<qreal>(0.25, preset.textureTileScale, 16.0);

        preset.textureContrastPercent = settings.value("TextureContrastPercent", 10).toInt();
        preset.textureContrastPercent = qBound(0, preset.textureContrastPercent, 100);

        // Load texture asset ID — migrate legacy blob if present
        preset.textureAssetId = settings.value("TextureAssetId").toString();
        if (preset.textureAssetId.isEmpty()) {
            const QByteArray legacyBlob = settings.value("TextureData").toByteArray();
            if (!legacyBlob.isEmpty()) {
                preset.textureAssetId = BrushAssetStore::instance()->importTexture(legacyBlob);
                needsSave = true;
            }
            // Built-in presets get their canonical asset ID
            if (preset.textureAssetId.isEmpty() && preset.builtIn) {
                const QString key = preset.name.trimmed().toLower();
                const QString bid = BrushAssetStore::builtinTextureId(preset.name);
                // Only assign if this name has a known built-in texture
                if (!BrushAssetStore::instance()->loadTexture(bid).isEmpty()) {
                    preset.textureAssetId = bid;
                    needsSave = true;
                }
            }
        }
        // Validate: if the user asset file no longer exists on disk, clear the stale ID.
        if (!preset.textureAssetId.isEmpty() && !BrushAssetStore::isBuiltinId(preset.textureAssetId)) {
            if (BrushAssetStore::instance()->loadTexture(preset.textureAssetId).isEmpty()) {
                preset.textureAssetId.clear();
                needsSave = true;
            }
        }

        auto readCurve = [&settings](const QString& key) -> QVector<QPointF> {
            QVector<QPointF> pts;
            const QVariant v = settings.value(key);
            if (v.type() == QVariant::List) {
                const QVariantList list = v.toList();
                // Flat list of doubles.
                if (!list.isEmpty() && list.first().type() != QVariant::PointF) {
                    for (int k = 0; k + 1 < list.size(); k += 2) {
                        pts.append(QPointF(list[k].toDouble(), list[k + 1].toDouble()));
                    }
                    return pts;
                }
                // Legacy list of QPointF.
                for (const QVariant& item : list) {
                    pts.append(item.toPointF());
                }
            }
            return pts;
        };

        preset.thicknessCurvePoints = readCurve(QStringLiteral("ThicknessCurvePoints"));
        preset.opacityCurvePoints = readCurve(QStringLiteral("OpacityCurvePoints"));
        preset.thicknessCurveEnabled = settings.value("ThicknessCurveEnabled", true).toBool();
        preset.opacityCurveEnabled = settings.value("OpacityCurveEnabled", true).toBool();

        settings.endGroup();

        if (preset.id.isEmpty()) {
            if (preset.builtIn) {
                preset.id = normalizeBuiltinId(preset.name);
            } else {
                preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }
            needsSave = true;
        }

        mPresets.append(preset);
    }

    mActivePresetId = settings.value("ActivePresetId").toString();
    if (mActivePresetId.isEmpty()) {
        if (activeIndex < 0 || activeIndex >= mPresets.size()) {
            activeIndex = 0;
        }
        mActivePresetId = mPresets.isEmpty() ? QString() : mPresets[activeIndex].id;
        needsSave = true;
    }
    if (indexOfId(mActivePresetId) < 0 && !mPresets.isEmpty()) {
        mActivePresetId = mPresets[0].id;
        needsSave = true;
    }

    settings.endGroup();

    if (needsSave) {
        saveToSettings();
    }
}

void BrushPresetStore::saveToSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup("PencilBrushFactory");

    settings.setValue("Count", mPresets.size());
    settings.setValue("ActivePresetId", mActivePresetId);
    settings.setValue("ActiveIndex", activeIndex());

    for (int i = 0; i < mPresets.size(); ++i) {
        const auto& p = mPresets[i];
        settings.beginGroup(QStringLiteral("Preset_%1").arg(i));
        settings.setValue("Id", p.id);
        settings.setValue("Name", p.name);
        settings.setValue("SpacingMult", p.spacingMult);
        settings.setValue("Scatter", p.scatter);
        settings.setValue("RotationDeg", p.rotationDeg);
        settings.setValue("SizeJitter", p.sizeJitter);
        settings.setValue("ColorJitter", p.colorJitter);
        settings.setValue("BuiltIn", p.builtIn);
        settings.setValue("TipMode", static_cast<int>(p.tipMode));
        settings.setValue("TipAssetId", p.tipAssetId);
        settings.setValue("TipContrastPercent", p.tipContrastPercent);
        settings.setValue("TextureTileScale", p.textureTileScale);
        settings.setValue("TextureContrastPercent", p.textureContrastPercent);
        settings.setValue("TextureAssetId", p.textureAssetId);

        QVariantList flatThickness;
        for (const QPointF& pt : p.thicknessCurvePoints) {
            flatThickness.append(pt.x());
            flatThickness.append(pt.y());
        }
        settings.setValue("ThicknessCurvePoints", flatThickness);

        QVariantList flatOpacity;
        for (const QPointF& pt : p.opacityCurvePoints) {
            flatOpacity.append(pt.x());
            flatOpacity.append(pt.y());
        }
        settings.setValue("OpacityCurvePoints", flatOpacity);

        settings.setValue("ThicknessCurveEnabled", p.thicknessCurveEnabled);
        settings.setValue("OpacityCurveEnabled", p.opacityCurveEnabled);
        settings.endGroup();
    }

    settings.endGroup();
    settings.sync();
}

void BrushPresetStore::setActivePresetId(const QString& presetId)
{
    ensureLoaded();
    if (presetId.isEmpty() || indexOfId(presetId) < 0) {
        return;
    }
    if (mActivePresetId == presetId) {
        return;
    }
    mActivePresetId = presetId;
    scheduleSave();
    emit presetsUpdated(mPresets, mActivePresetId);
}

void BrushPresetStore::updatePreset(const QString& presetId, const ToolOptionWidget::BrushPreset& preset)
{
    ensureLoaded();
    const int idx = indexOfId(presetId);
    if (idx < 0) {
        return;
    }

    ToolOptionWidget::BrushPreset updated = mPresets[idx];
    updated.spacingMult = preset.spacingMult;
    updated.scatter = preset.scatter;
    updated.rotationDeg = preset.rotationDeg;
    updated.sizeJitter = preset.sizeJitter;
    updated.colorJitter = preset.colorJitter;
    updated.tipMode = preset.tipMode;
    updated.tipAssetId = preset.tipAssetId;
    updated.tipContrastPercent = preset.tipContrastPercent;
    updated.textureTileScale = preset.textureTileScale;
    updated.textureContrastPercent = preset.textureContrastPercent;
    updated.textureAssetId = preset.textureAssetId;
    updated.thicknessCurvePoints = preset.thicknessCurvePoints;
    updated.opacityCurvePoints = preset.opacityCurvePoints;
    updated.thicknessCurveEnabled = preset.thicknessCurveEnabled;
    updated.opacityCurveEnabled = preset.opacityCurveEnabled;

    if (!mPresets[idx].builtIn) {
        updated.name = preset.name;
    }

    mPresets[idx] = updated;
    scheduleSave();
    emit presetsUpdated(mPresets, mActivePresetId);
}

QString BrushPresetStore::duplicatePreset(const QString& presetId)
{
    ensureLoaded();
    const int srcIndex = indexOfId(presetId);
    if (srcIndex < 0) {
        return QString();
    }

    const auto& src = mPresets[srcIndex];
    ToolOptionWidget::BrushPreset dup = src;
    dup.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dup.name = makeUniqueCopyName(src.name);
    dup.builtIn = false;

    const int insertIndex = srcIndex + 1;
    mPresets.insert(insertIndex, dup);
    mActivePresetId = dup.id;

    saveToSettings(); // structural change — save immediately, don't defer
    emit presetsUpdated(mPresets, mActivePresetId);
    return dup.id;
}

void BrushPresetStore::renamePreset(const QString& presetId, const QString& newName)
{
    ensureLoaded();
    if (newName.isEmpty()) {
        return;
    }
    const int idx = indexOfId(presetId);
    if (idx < 0) {
        return;
    }
    if (mPresets[idx].builtIn) {
        return;
    }

    mPresets[idx].name = newName;
    saveToSettings(); // structural change — save immediately
    emit presetsUpdated(mPresets, mActivePresetId);
}

void BrushPresetStore::deletePreset(const QString& presetId)
{
    ensureLoaded();
    const int idx = indexOfId(presetId);
    if (idx < 0) {
        return;
    }
    if (mPresets[idx].builtIn) {
        return;
    }

    const bool deletingActive = (mActivePresetId == presetId);
    mPresets.removeAt(idx);

    if (mPresets.isEmpty()) {
        mActivePresetId.clear();
    } else if (deletingActive) {
        const int newIndex = qBound(0, idx, mPresets.size() - 1);
        mActivePresetId = mPresets[newIndex].id;
    }

    saveToSettings(); // structural change — save immediately
    emit presetsUpdated(mPresets, mActivePresetId);
}

void BrushPresetStore::reorderPresets(const QVector<QString>& idsInOrder)
{
    ensureLoaded();
    if (idsInOrder.size() != mPresets.size()) {
        return;
    }

    QVector<ToolOptionWidget::BrushPreset> newPresets;
    newPresets.reserve(mPresets.size());

    for (const QString& id : idsInOrder) {
        const int idx = indexOfId(id);
        if (idx < 0) {
            return;
        }
        newPresets.append(mPresets[idx]);
    }

    mPresets = newPresets;
    if (!mActivePresetId.isEmpty() && indexOfId(mActivePresetId) < 0 && !mPresets.isEmpty()) {
        mActivePresetId = mPresets[0].id;
    }

    saveToSettings();
    emit presetsUpdated(mPresets, mActivePresetId);
}
