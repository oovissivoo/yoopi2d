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

#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QVariant>
#include <QMutex>
#include <QTimer>

#include "pencildef.h"

class QSettings;

/**
 * SettingsManager provides an in-memory cache over QSettings and defers
 * disk writes using a short single-shot timer to batch updates.
 */
class SettingsManager : public QObject
{
    Q_OBJECT
public:
    static SettingsManager* instance();

    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant());
    void deferredSetValue(const QString& key, const QVariant& value);
    void forceSyncNow();

signals:
    void willSync();
    void didSync();

private slots:
    void performSync();

private:
    explicit SettingsManager(QObject* parent = nullptr);
    ~SettingsManager() override;

    void scheduleSync();

    QSettings* mSettings;
    QHash<QString, QVariant> mCache;
    QSet<QString> mDirtyKeys;
    QMutex mMutex;
    QTimer mTimer;
};

#endif // SETTINGS_MANAGER_H


