#ifndef BRUSHPRESETSTORE_H
#define BRUSHPRESETSTORE_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QTimer>

#include "tooloptionwidget.h"

class BrushPresetStore : public QObject
{
    Q_OBJECT
public:
    static BrushPresetStore* instance();

    QVector<ToolOptionWidget::BrushPreset> presets() const { return mPresets; }
    QString activePresetId() const { return mActivePresetId; }
    int activeIndex() const;

    void ensureLoaded();

    QString duplicatePreset(const QString& presetId);
    void renamePreset(const QString& presetId, const QString& newName);
    void deletePreset(const QString& presetId);
    void setActivePresetId(const QString& presetId);
    void reorderPresets(const QVector<QString>& idsInOrder);

    void updatePreset(const QString& presetId, const ToolOptionWidget::BrushPreset& preset);

    // Force an immediate save — call on app close to flush any pending deferred save.
    void saveToSettings();

signals:
    void presetsUpdated(const QVector<ToolOptionWidget::BrushPreset>& presets, const QString& activePresetId);

private:
    explicit BrushPresetStore(QObject* parent = nullptr);

    void loadFromSettings();
    void scheduleSave();
    void bootstrapDefaultsToSettings();

    int indexOfId(const QString& id) const;
    QString makeUniqueCopyName(const QString& base) const;

    QVector<ToolOptionWidget::BrushPreset> mPresets;
    QString mActivePresetId;
    bool mLoaded = false;

    QTimer* mDeferredSaveTimer = nullptr;
};

#endif // BRUSHPRESETSTORE_H
