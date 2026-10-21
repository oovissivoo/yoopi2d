// NewDocumentDialog header regenerated to support CosmicDropDown migration
#ifndef NEWDOCUMENTDIALOG_H
#define NEWDOCUMENTDIALOG_H

#include <QDialog>
#include <QColor>
#include <QSize>
#include <QVector>

class QTabWidget;
class QWidget;
class QPushButton;
class QLabel;
class QSpinBox;
class QGroupBox;
class QGridLayout;
class QHBoxLayout;
class CosmicDropDown;

class NewDocumentDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewDocumentDialog(QWidget *parent = nullptr);
    ~NewDocumentDialog();

    QSize getCanvasSize() const;
    QColor getBackgroundColor() const;
    bool isDrawMode() const;

private slots:
    void setPresetSize(int index);
    void openColorPicker();

private:
    void setupUI();
    void setupDrawTab();
    void setupAnimateTab();
    void updateBgColorLabel();

    // UI elements
    QTabWidget *tabWidget = nullptr;
    QWidget *drawTab = nullptr;
    QWidget *animateTab = nullptr;
    QPushButton *okButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QLabel *backgroundColorLabel = nullptr;
    QPushButton *colorPickerButton = nullptr;
    QSpinBox *widthSpinBox = nullptr;
    QSpinBox *heightSpinBox = nullptr;

    // Migrated dropdowns and shared preset data
    CosmicDropDown* mPresetSizeDropDown = nullptr;
    CosmicDropDown* mAnimPresetSizeDropDown = nullptr;
    QVector<QSize> mPresetSizeData;

    // State
    QColor backgroundColor;
    bool drawModeSelected;
};

#endif // NEWDOCUMENTDIALOG_H


