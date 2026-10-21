#include "brushassetstore.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QCryptographicHash>
#include <QDebug>

static const QString kTexturesSubdir = QStringLiteral("brushes/textures");
static const QString kTipsSubdir     = QStringLiteral("brushes/tips");

// Built-in texture resource paths keyed by normalised name
static const QHash<QString, QString> kBuiltinTexturePaths = {
    { QStringLiteral("blotchermixed"),  QStringLiteral(":/BrushTextures/BlotcherMixed.jpeg")  },
    { QStringLiteral("blotchersolo"),   QStringLiteral(":/BrushTextures/BlotcherSolo.jpeg")   },
    { QStringLiteral("breakance"),      QStringLiteral(":/BrushTextures/Breakance.png")        },
    { QStringLiteral("chalkflow"),      QStringLiteral(":/BrushTextures/ChalkFlow.jpeg")       },
    { QStringLiteral("chalkflowpng"),   QStringLiteral(":/BrushTextures/ChalkFlow.png")        },
    { QStringLiteral("clawingcore"),    QStringLiteral(":/BrushTextures/ClawingCore.jpeg")     },
    { QStringLiteral("crackedmason"),   QStringLiteral(":/BrushTextures/CrackedMason.jpeg")    },
    { QStringLiteral("galvinsteps"),    QStringLiteral(":/BrushTextures/GalvinSteps.jpeg")     },
    { QStringLiteral("graysole"),       QStringLiteral(":/BrushTextures/Graysole.jpeg")        },
    { QStringLiteral("graysolefury"),   QStringLiteral(":/BrushTextures/GraysoleFury.jpeg")    },
    { QStringLiteral("moldycloud"),     QStringLiteral(":/BrushTextures/MoldyCloud.jpeg")      },
    { QStringLiteral("nightnudge"),     QStringLiteral(":/BrushTextures/NightNudge.jpeg")      },
    { QStringLiteral("paper1"),         QStringLiteral(":/BrushTextures/Paper1.png")           },
    { QStringLiteral("paper3"),         QStringLiteral(":/BrushTextures/Paper3.png")           },
    { QStringLiteral("pencilnoise1"),   QStringLiteral(":/BrushTextures/PencilNoise1.png")     },
    { QStringLiteral("sandey"),         QStringLiteral(":/BrushTextures/Sandey.png")           },
    { QStringLiteral("stormystream"),   QStringLiteral(":/BrushTextures/StormyStream.jpeg")    },
    { QStringLiteral("tearingflow"),    QStringLiteral(":/BrushTextures/TearingFlow.jpeg")     },
    { QStringLiteral("wayweave"),       QStringLiteral(":/BrushTextures/WayWeave.png")         },
    { QStringLiteral("weepingboy"),     QStringLiteral(":/BrushTextures/WeepingBoy.jpeg")      },
    { QStringLiteral("weepingfather"),  QStringLiteral(":/BrushTextures/WeepingFather.jpeg")   },
    { QStringLiteral("weepinggirl"),    QStringLiteral(":/BrushTextures/WeepingGirl.jpeg")     },
    { QStringLiteral("weepingmother"),  QStringLiteral(":/BrushTextures/WeepingMother.jpeg")   },
    { QStringLiteral("zichs"),          QStringLiteral(":/BrushTextures/Zichs.png")            },
};

// Built-in tip resource paths keyed by normalised name
static const QHash<QString, QString> kBuiltinTipPaths = {
    { QStringLiteral("hard"),    QStringLiteral(":/BrushTips/CosmicDefaultTip_Hard.png")    },
    { QStringLiteral("hardest"), QStringLiteral(":/BrushTips/CosmicDefaultTip_Hardest.png") },
    { QStringLiteral("medium"),  QStringLiteral(":/BrushTips/CosmicDefaultTip_Medium.png")  },
    { QStringLiteral("soft"),    QStringLiteral(":/BrushTips/CosmicDefaultTip_Soft.png")    },
};

// ── Singleton ────────────────────────────────────────────────────────────────

BrushAssetStore* BrushAssetStore::instance()
{
    static BrushAssetStore* s = new BrushAssetStore();
    return s;
}

BrushAssetStore::BrushAssetStore(QObject* parent) : QObject(parent)
{
    ensureDirs();
}

// ── Directory helpers ────────────────────────────────────────────────────────

void BrushAssetStore::ensureDirs() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(base).mkpath(kTexturesSubdir);
    QDir(base).mkpath(kTipsSubdir);
}

QString BrushAssetStore::texturesDir() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
               .filePath(kTexturesSubdir);
}

QString BrushAssetStore::tipsDir() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
               .filePath(kTipsSubdir);
}

QString BrushAssetStore::assetPath(const QString& dir, const QString& id) const
{
    return QDir(dir).filePath(id);
}

// ── Low-level read/write ─────────────────────────────────────────────────────

// Content-hash filename deduplicates identical assets automatically.
QString BrushAssetStore::writeAsset(const QString& dir, const QByteArray& data, const QString& hint) const
{
    const QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
    const QString ext = hint.contains('.') ? hint.mid(hint.lastIndexOf('.')) : QStringLiteral(".dat");
    const QString id  = hash + ext;
    const QString path = QDir(dir).filePath(id);

    if (!QFile::exists(path)) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning() << "[BrushAssetStore] Failed to write asset:" << path;
            return QString();
        }
        f.write(data);
    }
    return id;
}

QByteArray BrushAssetStore::readAsset(const QString& dir, const QString& id) const
{
    const QString path = QDir(dir).filePath(id);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        // Silently return empty — callers treat empty as "not found" and fall back gracefully.
        return QByteArray();
    }
    return f.readAll();
}

// ── Sidecar name helpers ─────────────────────────────────────────────────────
// One tiny UTF-8 text file per asset stores the human-readable display name.
// Written once at import; read only when listing assets for the gallery.

void BrushAssetStore::writeSidecarName(const QString& dir, const QString& id, const QString& name) const
{
    if (id.isEmpty() || name.isEmpty()) return;
    QFile f(QDir(dir).filePath(id + QStringLiteral(".name")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(name.toUtf8());
}

QString BrushAssetStore::readSidecarName(const QString& dir, const QString& id) const
{
    QFile f(QDir(dir).filePath(id + QStringLiteral(".name")));
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

// Enumerate all user assets in a directory. Skips .name sidecars.
QVector<BrushAssetStore::AssetEntry> BrushAssetStore::listDir(const QString& dir) const
{
    QVector<AssetEntry> out;
    const QDir d(dir);
    if (!d.exists()) return out;

    const QStringList files = d.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& file : files) {
        if (file.endsWith(QStringLiteral(".name"), Qt::CaseInsensitive)) continue;
        AssetEntry e;
        e.id = file;
        e.builtIn = false;
        e.displayName = readSidecarName(dir, file);
        if (e.displayName.isEmpty()) {
            // Fallback for assets imported before sidecar support: show truncated hash + ext.
            const QFileInfo fi(file);
            e.displayName = fi.completeBaseName().left(8) + QStringLiteral("…") + fi.suffix();
        }
        out.push_back(e);
    }
    return out;
}

// ── Public API: Textures ─────────────────────────────────────────────────────

QString BrushAssetStore::importTexture(const QByteArray& data, const QString& suggestedName)
{
    ensureDirs();
    const QString id = writeAsset(texturesDir(), data, suggestedName);
    if (!id.isEmpty() && !suggestedName.isEmpty())
        writeSidecarName(texturesDir(), id, QFileInfo(suggestedName).fileName());
    return id;
}

QByteArray BrushAssetStore::loadTexture(const QString& id) const
{
    if (id.isEmpty()) return QByteArray();

    if (isBuiltinId(id)) {
        const QString key = id.mid(8).toLower();
        const QString resPath = kBuiltinTexturePaths.value(key);
        if (resPath.isEmpty()) return QByteArray();
        QFile f(resPath);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        return f.readAll();
    }

    return readAsset(texturesDir(), id);
}

void BrushAssetStore::removeTexture(const QString& id)
{
    if (id.isEmpty() || isBuiltinId(id)) return;
    const QString dir = texturesDir();
    QFile::remove(QDir(dir).filePath(id));
    QFile::remove(QDir(dir).filePath(id + QStringLiteral(".name")));
}

QString BrushAssetStore::builtinTextureId(const QString& name)
{
    return QStringLiteral("builtin:") + name.trimmed().toLower();
}

QVector<BrushAssetStore::AssetEntry> BrushAssetStore::listTextures() const
{
    QVector<AssetEntry> out;

    // Built-ins first — stable order, always present.
    const QList<QString> builtinKeys = kBuiltinTexturePaths.keys();
    QStringList sorted = QStringList(builtinKeys.begin(), builtinKeys.end());
    sorted.sort(Qt::CaseInsensitive);
    for (const QString& key : sorted) {
        AssetEntry e;
        e.id = builtinTextureId(key);
        e.builtIn = true;
        // Capitalise first letter for display.
        e.displayName = key.left(1).toUpper() + key.mid(1);
        out.push_back(e);
    }

    // User-imported textures.
    out += listDir(texturesDir());
    return out;
}

// ── Public API: Tips ─────────────────────────────────────────────────────────

QString BrushAssetStore::importTip(const QByteArray& data, const QString& suggestedName)
{
    ensureDirs();
    const QString id = writeAsset(tipsDir(), data, suggestedName);
    if (!id.isEmpty() && !suggestedName.isEmpty())
        writeSidecarName(tipsDir(), id, QFileInfo(suggestedName).fileName());
    return id;
}

QByteArray BrushAssetStore::loadTip(const QString& id) const
{
    if (id.isEmpty()) return QByteArray();
    return readAsset(tipsDir(), id);
}

void BrushAssetStore::removeTip(const QString& id)
{
    if (id.isEmpty()) return;
    const QString dir = tipsDir();
    QFile::remove(QDir(dir).filePath(id));
    QFile::remove(QDir(dir).filePath(id + QStringLiteral(".name")));
}

QVector<BrushAssetStore::AssetEntry> BrushAssetStore::listTips() const
{
    // No built-in tips in the gallery — built-ins (Hard/Soft) are selected via
    // the mode selector, not the gallery. Gallery shows only user-imported tips.
    return listDir(tipsDir());
}
