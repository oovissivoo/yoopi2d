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

#include "settingsmanager.h"

#include <QSettings>
#include <QCoreApplication>
#include <QMutexLocker>

static SettingsManager* s_instance = nullptr;

SettingsManager* SettingsManager::instance()
{
    if (!s_instance) {
        s_instance = new SettingsManager(QCoreApplication::instance());
    }
    return s_instance;
}

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent)
    , mSettings(new QSettings(PENCIL2D, PENCIL2D, this))
{
    // Preload existing keys into cache for fast reads
    const QStringList keys = mSettings->allKeys();
    for (const QString& k : keys) {
        mCache.insert(k, mSettings->value(k));
    }

    mTimer.setSingleShot(true);
    mTimer.setInterval(500);
    connect(&mTimer, &QTimer::timeout, this, &SettingsManager::performSync);

    // Ensure pending changes are flushed on shutdown
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &SettingsManager::performSync);
    }
}

SettingsManager::~SettingsManager()
{
    forceSyncNow();
}

QVariant SettingsManager::getValue(const QString& key, const QVariant& defaultValue)
{
    QMutexLocker locker(&mMutex);
    auto it = mCache.constFind(key);
    if (it != mCache.constEnd()) {
        return it.value();
    }
    // Fallback to underlying settings (also seed cache)
    const QVariant v = mSettings->value(key, defaultValue);
    mCache.insert(key, v);
    return v;
}

void SettingsManager::deferredSetValue(const QString& key, const QVariant& value)
{
    {
        QMutexLocker locker(&mMutex);
        mCache.insert(key, value);
        mDirtyKeys.insert(key);
    }
    scheduleSync();
}

void SettingsManager::scheduleSync()
{
    // Restart the single-shot timer to coalesce bursts of changes
    mTimer.start();
}

void SettingsManager::performSync()
{
    QSet<QString> dirty;
    {
        QMutexLocker locker(&mMutex);
        dirty = mDirtyKeys;
        mDirtyKeys.clear();
    }
    if (dirty.isEmpty()) { return; }
    emit willSync();
    for (const QString& k : dirty) {
        mSettings->setValue(k, mCache.value(k));
    }
    mSettings->sync();
    emit didSync();
}

void SettingsManager::forceSyncNow()
{
    if (mTimer.isActive()) {
        mTimer.stop();
    }
    performSync();
}


