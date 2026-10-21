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

#include "filespage.h"

#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QCheckBox>
#include <QGroupBox>

#include "errordialog.h"
#include "n2dprojectio.h"
#include "pencilerror.h"
#include "presetdialog.h"
#include "preferencemanager.h"
#include "preferencesdef.h"

#include "ui_filespage.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"

FilesPage::FilesPage()
    : ui(new Ui::FilesPage)
{
    ui->setupUi(this);

    // ── Scrollarea styling ────────────────────────────────────────────────
    if (ui->scrollArea) {
        ui->scrollArea->setFrameShape(QFrame::NoFrame);
        ui->scrollArea->setWidgetResizable(true);
        ui->scrollArea->viewport()->setAutoFillBackground(false);
        ui->scrollArea->setAutoFillBackground(false);
        ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        ui->scrollArea->verticalScrollBar()->setFixedWidth(12);
        ui->scrollArea->verticalScrollBar()->setStyleSheet(
            "QScrollBar:vertical {"
            "  background:#1f1f1f; border:none; border-radius:6px;"
            "  width:12px; margin:0px;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background:#505050; border-radius:6px; min-height:26px;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical"
            "  { background:transparent; height:0px; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical"
            "  { background:transparent; }"
        );
    }

    // ── Card + section-header stylesheet ─────────────────────────────────
    {
        const int cr = qMax(0, CosmicUiMetrics::cornerRadius());
        setStyleSheet(styleSheet() + QString(
            "QScrollArea{background:transparent;border:none;}"
            "QWidget#scrollAreaWidgetContents{background:transparent;}"
            "QLabel{background:transparent;}"
            "QCheckBox{background:transparent;}"
            "QGroupBox{"
            "  background:#2b2b2b; border:none;"
            "  border-radius:%1px; margin-top:10px; padding:12px;"
            "}"
            "QGroupBox::title{height:0px;padding:0px;margin:0px;border:none;color:transparent;}"
            "QLabel#cosmicPrefSectionHeader{"
            "  background:transparent; color:#b3b3b3;"
            "  font-weight:600; padding:0px; margin:0px;"
            "}"
        ).arg(cr));
    }

    // ── Helper: insert a section-header label at the top of a QGroupBox ──
    auto insertHeader = [](QGroupBox* gb, const QString& title) -> QLabel* {
        if (!gb) return nullptr;
        auto* vbox = qobject_cast<QVBoxLayout*>(gb->layout());
        if (!vbox) return nullptr;
        gb->setTitle(QString());
        gb->setFlat(true);
        auto* lbl = new QLabel(title, gb);
        lbl->setObjectName(QStringLiteral("cosmicPrefSectionHeader"));
        vbox->insertWidget(0, lbl);
        return lbl;
    };

    // ── Startup card (hidden — no startup UI yet) ─────────────────────────
    if (ui->startupBox) ui->startupBox->hide();
    mHeaderStartup = insertHeader(ui->startupBox, tr("Startup"));

    // ── Autosave card ─────────────────────────────────────────────────────
    mHeaderAutosave = insertHeader(ui->autosaveBox, tr("Autosave"));

    // Hide the legacy Qt widgets from the .ui — we replace them with CosmicUI
    if (ui->autosaveCheckBox)  ui->autosaveCheckBox->hide();
    if (ui->autosaveNumberBox) ui->autosaveNumberBox->hide();
    if (ui->autosaveNumberLabel) ui->autosaveNumberLabel->hide();

    // Remove the legacy interval row layout from the groupbox so we can
    // rebuild it cleanly (the .ui has an HBoxLayout named autosaveIntervalRow)
    auto* autosaveVBox = qobject_cast<QVBoxLayout*>(ui->autosaveBox->layout());

    // 1. Enable-autosave toggle (CosmicCheckboxToggle)
    mAutosaveToggle = new CosmicCheckboxToggle(ui->autosaveBox);
    mAutosaveToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
    mAutosaveToggle->setText(tr("Enable autosave"));
    mAutosaveToggle->setChecked(false);
    mAutosaveToggle->setFixedHeight(CosmicUiMetrics::controlThickness());
    autosaveVBox->addWidget(mAutosaveToggle);

    // 2. Interval row: label + CosmicInputField
    auto* intervalRow = new QHBoxLayout();
    intervalRow->setContentsMargins(0, 4, 0, 0);
    intervalRow->setSpacing(10);

    mAutosaveNumberLabel = new QLabel(tr("Interval (minutes):"), ui->autosaveBox);
    mAutosaveNumberLabel->setObjectName(QStringLiteral("autosaveIntervalLabel"));
    intervalRow->addWidget(mAutosaveNumberLabel, 1);

    mAutosaveNumberField = new CosmicInputField(ui->autosaveBox);
    mAutosaveNumberField->setRange(1, 120);
    mAutosaveNumberField->setDecimals(0);
    mAutosaveNumberField->setSingleStep(1.0);
    mAutosaveNumberField->setValue(15.0);
    mAutosaveNumberField->setFixedHeight(CosmicUiMetrics::controlThickness());
    mAutosaveNumberField->setFixedWidth(90);
    intervalRow->addWidget(mAutosaveNumberField, 0);

    autosaveVBox->addLayout(intervalRow);

    // Grey out the interval row when autosave is disabled
    auto syncIntervalEnabled = [this](bool enabled) {
        mAutosaveNumberField->setEnabled(enabled);
        mAutosaveNumberLabel->setEnabled(enabled);
    };
    syncIntervalEnabled(false); // disabled by default until prefs load

    connect(mAutosaveToggle, &CosmicCheckboxToggle::toggled, this, [this, syncIntervalEnabled](bool on) {
        syncIntervalEnabled(on);
        autoSaveChange(on ? Qt::Checked : Qt::Unchecked);
    });

    connect(mAutosaveNumberField, &CosmicInputField::valueChanged, this, [this](double v) {
        autoSaveNumberChange(static_cast<int>(v));
    });

    // ── Preset / startup wiring ───────────────────────────────────────────
    initPreset();

    connect(ui->addPreset,          &QPushButton::clicked,       this, &FilesPage::addPreset);
    connect(ui->removePreset,       &QPushButton::clicked,       this, &FilesPage::removePreset);
    connect(ui->setDefaultPreset,   &QPushButton::clicked,       this, &FilesPage::setDefaultPreset);
    connect(ui->askPresetRbtn,      &QRadioButton::toggled,      this, &FilesPage::askForPresetChange);
    connect(ui->loadLastActiveRbtn, &QRadioButton::toggled,      this, &FilesPage::loadMostRecentChange);
    connect(ui->loadDefaultPresetRbtn, &QRadioButton::toggled,   this, &FilesPage::loadDefaultPreset);
    connect(ui->presetListWidget,   &QListWidget::itemChanged,   this, &FilesPage::presetNameChanged);
}

FilesPage::~FilesPage()
{
    delete ui;
}

// ── Preset management ─────────────────────────────────────────────────────

void FilesPage::initPreset()
{
    mPresetDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    mPresetDir.mkpath("presets");
    mPresetDir.cd("presets");

    mPresetSettings = new QSettings(mPresetDir.filePath("presets.ini"), QSettings::IniFormat, this);

    QListWidgetItem* defaultItem = new QListWidgetItem("Blank");
    defaultItem->setData(Qt::UserRole, 0);
    ui->presetListWidget->addItem(defaultItem);

    bool ok = true;
    for (const QString& key : mPresetSettings->allKeys())
    {
        int index = key.toInt(&ok);
        if (!ok || index == 0 || !mPresetDir.exists(QString("%1.n2d").arg(index))) continue;

        mMaxPresetIndex = qMax(index, mMaxPresetIndex);

        QString name = mPresetSettings->value(key).toString();
        if (name.isEmpty()) continue;

        QListWidgetItem* item = new QListWidgetItem(name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setData(Qt::UserRole, index);
        ui->presetListWidget->addItem(item);
    }
}

void FilesPage::addPreset()
{
    int newPresetIndex = mMaxPresetIndex + 1;

    const N2DProjectIO::ProgressCallbacks cb;
    Status st = N2DProjectIO::saveSingleShotProject(PresetDialog::getPresetPath(newPresetIndex), mManager->object(), cb);
    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(),
                                st.description().append(tr("<br><br>Error: your preset may not have saved successfully.")), st.details().html());
        errorDialog.exec();
        return;
    }

    QString presetName = QString("Preset %1").arg(newPresetIndex);
    mPresetSettings->setValue(QString::number(newPresetIndex), presetName);
    mMaxPresetIndex = newPresetIndex;

    QListWidgetItem* newItem = new QListWidgetItem(presetName);
    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
    newItem->setData(Qt::UserRole, newPresetIndex);
    ui->presetListWidget->addItem(newItem);

    ui->presetListWidget->scrollToBottom();
    ui->presetListWidget->editItem(newItem);
}

void FilesPage::removePreset()
{
    if (ui->presetListWidget->count() <= 1) return;
    if (ui->presetListWidget->selectedItems().empty()) return;

    QList<QListWidgetItem*> itemsToRemove = ui->presetListWidget->selectedItems();
    for (QListWidgetItem* item : itemsToRemove)
        ui->presetListWidget->removeItemWidget(item);

    for (QListWidgetItem* item : itemsToRemove) {
        QFile(PresetDialog::getPresetPath(item->data(Qt::UserRole).toInt())).remove();
    }

    int prevDefaultIndex = mManager->getInt(SETTING::DEFAULT_PRESET);
    for (QListWidgetItem* item : itemsToRemove) {
        if (item->data(Qt::UserRole).toInt() == prevDefaultIndex)
            mManager->set(SETTING::DEFAULT_PRESET, 0);
        mPresetSettings->remove(QString::number(item->data(Qt::UserRole).toInt()));
    }

    qDeleteAll(itemsToRemove);
    updateValues();
}

void FilesPage::setDefaultPreset()
{
    QListWidgetItem* cur = ui->presetListWidget->currentItem();
    if (!cur) return;
    bool ok = true;
    const int idx = cur->data(Qt::UserRole).toInt(&ok);
    if (!ok) return;
    mManager->set(SETTING::DEFAULT_PRESET, idx);
    updateValues();
}

void FilesPage::presetNameChanged(QListWidgetItem* item)
{
    item->setText(item->text().remove(QChar('@')).remove(QChar('/')).remove(QChar('\\')));
    bool ok = true;
    const int index = item->data(Qt::UserRole).toInt(&ok);
    if (!ok) return;
    mPresetSettings->setValue(QString::number(index), item->text());
}

// ── Preference sync ───────────────────────────────────────────────────────

void FilesPage::updateValues()
{
    if (!mManager) return;

    // Preset list highlight
    const int defaultPresetIndex = mManager->getInt(SETTING::DEFAULT_PRESET);
    for (int i = 0; i < ui->presetListWidget->count(); i++) {
        QListWidgetItem* item = ui->presetListWidget->item(i);
        bool ok = true;
        const int presetIndex = item->data(Qt::UserRole).toInt(&ok);
        if (!ok) continue;
        const bool isDefault = (presetIndex == defaultPresetIndex);
        QFont f = item->font();
        f.setBold(isDefault);
        item->setFont(f);
        item->setBackground(isDefault ? palette().light() : palette().window());
    }

    // Autosave toggle
    const bool autoSaveOn = mManager->isOn(SETTING::AUTO_SAVE);
    if (mAutosaveToggle) {
        QSignalBlocker b(mAutosaveToggle);
        mAutosaveToggle->setChecked(autoSaveOn);
    }

    // Interval field — load from AUTO_SAVE_INTERVAL_MINUTES, clamp to valid range
    if (mAutosaveNumberField) {
        QSignalBlocker b(mAutosaveNumberField);
        int minutes = mManager->getInt(SETTING::AUTO_SAVE_INTERVAL_MINUTES);
        if (minutes < 1 || minutes > 120) minutes = 15;
        mAutosaveNumberField->setValue(static_cast<double>(minutes));
    }

    // Grey out interval row when autosave is off
    if (mAutosaveNumberField) mAutosaveNumberField->setEnabled(autoSaveOn);
    if (mAutosaveNumberLabel) mAutosaveNumberLabel->setEnabled(autoSaveOn);

    // Startup radio buttons
    ui->askPresetRbtn->setChecked(mManager->isOn(SETTING::ASK_FOR_PRESET));
    ui->loadDefaultPresetRbtn->setChecked(mManager->isOn(SETTING::LOAD_DEFAULT_PRESET));
    ui->loadLastActiveRbtn->setChecked(mManager->isOn(SETTING::LOAD_MOST_RECENT));
}

void FilesPage::askForPresetChange(int b)
{
    mManager->set(SETTING::ASK_FOR_PRESET, b != Qt::Unchecked);
}

void FilesPage::loadMostRecentChange(int b)
{
    mManager->set(SETTING::LOAD_MOST_RECENT, b != Qt::Unchecked);
}

void FilesPage::loadDefaultPreset(int b)
{
    mManager->set(SETTING::LOAD_DEFAULT_PRESET, b != Qt::Unchecked);
}

void FilesPage::autoSaveChange(int b)
{
    if (!mManager) return;
    mManager->set(SETTING::AUTO_SAVE, b != Qt::Unchecked);
}

void FilesPage::autoSaveNumberChange(int number)
{
    if (!mManager) return;
    mManager->set(SETTING::AUTO_SAVE_INTERVAL_MINUTES, number);
}

// ── Language change ───────────────────────────────────────────────────────

void FilesPage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslatePageStrings();
}

void FilesPage::retranslatePageStrings()
{
    ui->retranslateUi(this);

    if (mHeaderStartup)  mHeaderStartup->setText(tr("Startup"));
    if (mHeaderAutosave) mHeaderAutosave->setText(tr("Autosave"));

    if (mAutosaveToggle)      mAutosaveToggle->setText(tr("Enable autosave"));
    if (mAutosaveNumberLabel) mAutosaveNumberLabel->setText(tr("Interval (minutes):"));
}
