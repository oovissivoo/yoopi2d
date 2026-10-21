#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QAction>
#include <QKeySequence>
#include <QMap>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QLabel>
#include <QTabWidget>
#include <QSlider>
#include <QCheckBox>

class Canvas;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog();
    
    void setActions(const QList<QAction*> &actions);
    void setCanvas(Canvas *canvas);

private slots:
    void captureKeySequence(QTreeWidgetItem *item, int column);
    void clearShortcut();
    void resetShortcuts();
    void saveShortcuts();
    void filterShortcuts(const QString &text);

private:
    void setupUI();
    void setupShortcutsTab();
    void populateShortcutsTree();
    
    QTabWidget *tabWidget;
    QTreeWidget *shortcutsTree;
    QPushButton *saveButton;
    QPushButton *resetButton;
    QPushButton *clearButton;
    QKeySequenceEdit *keySequenceEdit;
    QLabel *currentShortcutLabel;
    QLineEdit *searchEdit;
    
    QTreeWidgetItem *currentEditItem;
    
    QList<QAction*> actionsList;
    QMap<QString, QKeySequence> originalShortcuts;
    QMap<QString, QKeySequence> customShortcuts;
    
    Canvas *m_canvas;
};

#endif // PREFERENCESDIALOG_H 