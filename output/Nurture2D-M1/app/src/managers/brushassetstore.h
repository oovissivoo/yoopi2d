#ifndef BRUSHASSETSTORE_H
#define BRUSHASSETSTORE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QVector>

// BrushAssetStore — manages brush texture and tip mask files on disk.
//
// Assets are stored in:
//   QStandardPaths::AppDataLocation / "brushes" / "textures" / <id>.dat
//   QStandardPaths::AppDataLocation / "brushes" / "tips"     / <id>.dat
//
// Built-in assets use QRC paths prefixed with "qrc:" — they are never
// written to disk. User assets get a UUID as their ID.
//
// BrushPreset stores only the asset ID string, not the raw bytes.
// This keeps QSettings small and fast.
class BrushAssetStore : public QObject
{
    Q_OBJECT
public:
    static BrushAssetStore* instance();

    // A lightweight descriptor returned by listTextures() / listTips().
    // Contains only the ID and a human-readable display name — no image data.
    struct AssetEntry
    {
        QString id;          // asset ID passed to loadTexture/loadTip
        QString displayName; // original filename or built-in label
        bool    builtIn = false;
    };

    // --- Textures ---

    // Import raw image bytes as a user texture. Returns the asset ID.
    // The bytes are written to disk immediately.
    QString importTexture(const QByteArray& data, const QString& suggestedName = QString());

    // Load texture bytes by ID. Returns empty if not found.
    // Built-in IDs (prefixed "builtin:") are loaded from QRC.
    QByteArray loadTexture(const QString& id) const;

    // Remove a user texture from disk. No-op for built-in IDs.
    void removeTexture(const QString& id);

    // Returns true if the ID refers to a built-in (QRC) texture.
    static bool isBuiltinId(const QString& id) { return id.startsWith(QStringLiteral("builtin:")); }

    // Built-in texture IDs for the core presets.
    static QString builtinTextureId(const QString& name);

    // Enumerate all available textures (built-ins first, then user-imported).
    // Cheap: reads only sidecar .name files, not image data.
    QVector<AssetEntry> listTextures() const;

    // --- Tips ---

    QString importTip(const QByteArray& data, const QString& suggestedName = QString());
    QByteArray loadTip(const QString& id) const;
    void removeTip(const QString& id);

    // Enumerate all user-imported tips.
    QVector<AssetEntry> listTips() const;

    // --- Paths ---
    QString texturesDir() const;
    QString tipsDir()     const;

private:
    explicit BrushAssetStore(QObject* parent = nullptr);

    void ensureDirs() const;
    QString assetPath(const QString& dir, const QString& id) const;
    QString writeAsset(const QString& dir, const QByteArray& data, const QString& hint) const;
    QByteArray readAsset(const QString& dir, const QString& id) const;

    // Sidecar helpers — one tiny .name file per asset stores the display name.
    void        writeSidecarName(const QString& dir, const QString& id, const QString& name) const;
    QString     readSidecarName(const QString& dir, const QString& id) const;
    QVector<AssetEntry> listDir(const QString& dir) const;
};

#endif // BRUSHASSETSTORE_H
