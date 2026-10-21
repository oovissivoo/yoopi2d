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
#include "presetdialog.h"
#include "ui_presetdialog.h"
#include "app_util.h"
#include "../CUI/cosmicdropdown.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

#include <QStandardPaths>
#include <QDir>
#include <QSettings>


PresetDialog::PresetDialog(PreferenceManager* preferences, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PresetDialog),
    mPrefs(preferences)
{
    ui->setupUi(this);

    initPresets();
    hideQuestionMark(*this);
}

PresetDialog::~PresetDialog()
{
    delete ui;
}

int PresetDialog::getPresetIndex()
{
    if (mPresetDropDown && !mPresetIds.isEmpty()) {
        const int i = mPresetDropDown->currentIndex();
        if (i >= 0 && i < mPresetIds.size()) return mPresetIds[i];
    }
    return 0;
}

bool PresetDialog::shouldAlwaysUse()
{
    return ui->alwaysUse->isChecked();
}

QString PresetDialog::getPresetPath(int index)
{
    if (index == 0)
    {
        return QString();
    }

    const QString filename = QString("%1.n2d").arg(index);
    QDir dataDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (dataDir.cd("presets"))
    {
        return dataDir.filePath(filename);
    }
    return QString();
}

void PresetDialog::initPresets()
{
    // Make sure the presets directory in the data directory exists and navigate to it
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(dataPath);
    dataDir.mkdir("presets");
    if (dataDir.cd("presets") == false)
    {
        reject(); // the presets folder doesn't exist and cannot be created
        return;
    }

    // Find all presets in the preferences and add them to the dropdown with explicit ID mapping
    int defaultIndex = mPrefs->getInt(SETTING::DEFAULT_PRESET);
    // Insert cosmic dropdown and mirror data from UI combo as hidden source
    if (!mPresetDropDown && ui->presetComboBox) {
        mPresetDropDown = new CosmicDropDown(this);
        // Build options and IDs; hide legacy combo box
        QStringList opts;
        mPresetIds.clear();
        opts << tr("Default");
        mPresetIds << 0;
        if (ui->presetComboBox) ui->presetComboBox->setCurrentIndex(0);
        if (auto layoutHost = ui->presetComboBox->parentWidget()->layout()) {
            layoutHost->addWidget(mPresetDropDown);
            ui->presetComboBox->hide();
        }
        mPresetDropDown->setOptions(opts);
        mPresetDropDown->setCurrentIndex(0);
    }

    if (!dataDir.exists("presets.ini"))
    {
        reject();
        return;
    }
    QSettings presets(dataDir.filePath("presets.ini"), QSettings::IniFormat, this);

    bool ok = true;
    for (const QString& key : presets.allKeys())
    {
        int index = key.toInt(&ok);
        if (!ok || index == 0 || !dataDir.exists(QString("%1.n2d").arg(index))) continue;

        QString name = presets.value(key, QString()).toString();
        if (name.isEmpty()) continue;
        if (mPresetDropDown) {
            QStringList opts = mPresetDropDown->options();
            opts << name;
            mPresetDropDown->setOptions(opts);
            mPresetIds << index;
        }
        if (index == defaultIndex && mPresetDropDown)
        {
            mPresetDropDown->setCurrentIndex((int)mPresetDropDown->options().size()-1);
        }
    }

    // Keep visual dropdown options order; underlying ui combo holds sorted model if needed
}

// Preferences change: update accent color on dropdown
// This dialog is transient; when open, rely on PreferencesDialog to re-init accent styling.
