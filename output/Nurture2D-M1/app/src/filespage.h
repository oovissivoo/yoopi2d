#ifndef FILESPAGE_H
#define FILESPAGE_H

#include <QDir>
#include <QWidget>

class PreferenceManager;
class QListWidgetItem;
class QSettings;
class QLabel;
class QEvent;
class CosmicInputField;
class CosmicCheckboxToggle;

namespace Ui {
class FilesPage;
}

class FilesPage : public QWidget
{
    Q_OBJECT

public:
    FilesPage();
    ~FilesPage() override;

    void setManager(PreferenceManager* p) { mManager = p; }

public slots:
    void initPreset();
    void addPreset();
    void removePreset();
    void setDefaultPreset();
    void presetNameChanged(QListWidgetItem* item);

    void updateValues();
    void askForPresetChange(int b);
    void loadMostRecentChange(int b);
    void loadDefaultPreset(int b);
    void autoSaveChange(int b);
    void autoSaveNumberChange(int number);

signals:
    void clearRecentList();

protected:
    void changeEvent(QEvent* e) override;

private:
    void retranslatePageStrings();

    Ui::FilesPage*     ui            = nullptr;
    PreferenceManager* mManager      = nullptr;
    QSettings*         mPresetSettings = nullptr;
    QDir               mPresetDir;
    int                mMaxPresetIndex = 0;

    // CosmicUI autosave controls
    CosmicCheckboxToggle* mAutosaveToggle      = nullptr;
    CosmicInputField*     mAutosaveNumberField = nullptr;

    // Labels needing retranslation
    QLabel* mHeaderStartup       = nullptr;
    QLabel* mHeaderAutosave      = nullptr;
    QLabel* mAutosaveNumberLabel = nullptr;
};

#endif // FILESPAGE_H
