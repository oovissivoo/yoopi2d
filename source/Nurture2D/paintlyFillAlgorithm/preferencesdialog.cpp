#include "preferencesdialog.h"
#include "canvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QSettings>
#include <QMessageBox>
#include <QMenu>
#include <QGroupBox>
#include <QFormLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , tabWidget(nullptr)
    , shortcutsTree(nullptr)
    , saveButton(nullptr)
    , resetButton(nullptr)
    , clearButton(nullptr)
    , keySequenceEdit(nullptr)
    , currentShortcutLabel(nullptr)
    , searchEdit(nullptr)
    , currentEditItem(nullptr)
    , m_canvas(nullptr)
{
    setWindowTitle(tr("Preferences"));
    resize(600, 400);
    
    setupUI();
    
    // Load custom shortcuts if any
    QSettings settings("Paintly", "Paintly");
    settings.beginGroup("Shortcuts");
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        customShortcuts[key] = QKeySequence(settings.value(key).toString());
    }
    settings.endGroup();
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::setCanvas(Canvas *canvas)
{
    m_canvas = canvas;
}

void PreferencesDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    tabWidget = new QTabWidget(this);
    
    // Setup shortcuts tab
    setupShortcutsTab();
    
    // Add tab widget to main layout
    mainLayout->addWidget(tabWidget);
    
    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
    
    // Connect accept signal to save settings
    connect(this, &QDialog::accepted, this, &PreferencesDialog::saveShortcuts);
    
    setLayout(mainLayout);
}

void PreferencesDialog::setupShortcutsTab()
{
    QWidget *shortcutsTab = new QWidget();
    QVBoxLayout *tabLayout = new QVBoxLayout(shortcutsTab);
    
    // Add search box at the top
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel(tr("Search:"), shortcutsTab);
    searchEdit = new QLineEdit(shortcutsTab);
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setPlaceholderText(tr("Type to filter shortcuts..."));
    connect(searchEdit, &QLineEdit::textChanged, this, &PreferencesDialog::filterShortcuts);
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchEdit);
    tabLayout->addLayout(searchLayout);
    
    // Create tree widget for shortcuts
    shortcutsTree = new QTreeWidget(shortcutsTab);
    shortcutsTree->setColumnCount(2);
    shortcutsTree->setHeaderLabels(QStringList() << tr("Command") << tr("Shortcut"));
    shortcutsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    shortcutsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    
    connect(shortcutsTree, &QTreeWidget::itemClicked, this, &PreferencesDialog::captureKeySequence);
    
    tabLayout->addWidget(shortcutsTree);
    
    // Key sequence editor section
    QHBoxLayout *keyEditLayout = new QHBoxLayout();
    
    // Label for currently editing shortcut
    currentShortcutLabel = new QLabel(tr("Select an action to edit its shortcut"), shortcutsTab);
    
    // Key sequence editor
    keySequenceEdit = new QKeySequenceEdit(shortcutsTab);
    keySequenceEdit->setEnabled(false);
    connect(keySequenceEdit, &QKeySequenceEdit::editingFinished, [this]() {
        if (currentEditItem) {
            QKeySequence newSequence = keySequenceEdit->keySequence();
            currentEditItem->setText(1, newSequence.toString(QKeySequence::NativeText));
            
            QString actionName = currentEditItem->data(0, Qt::UserRole).toString();
            customShortcuts[actionName] = newSequence;
        }
    });
    
    // Clear button
    clearButton = new QPushButton(tr("Clear"), shortcutsTab);
    clearButton->setEnabled(false);
    connect(clearButton, &QPushButton::clicked, this, &PreferencesDialog::clearShortcut);
    
    keyEditLayout->addWidget(currentShortcutLabel);
    keyEditLayout->addWidget(keySequenceEdit);
    keyEditLayout->addWidget(clearButton);
    
    tabLayout->addLayout(keyEditLayout);
    
    // Reset button
    QHBoxLayout *resetLayout = new QHBoxLayout();
    resetButton = new QPushButton(tr("Reset All to Default"), shortcutsTab);
    connect(resetButton, &QPushButton::clicked, this, &PreferencesDialog::resetShortcuts);
    resetLayout->addStretch();
    resetLayout->addWidget(resetButton);
    
    tabLayout->addLayout(resetLayout);
    
    // Add the tab to the widget
    tabWidget->addTab(shortcutsTab, tr("Shortcuts"));
}

void PreferencesDialog::setActions(const QList<QAction*> &actions)
{
    actionsList = actions;
    
    // Store original shortcuts
    for (QAction *action : actionsList) {
        if (action->text().isEmpty()) continue;
        
        QString actionName = action->text().remove('&');
        originalShortcuts[actionName] = action->shortcut();
        
        // If we don't have a custom shortcut for this action yet, use the original
        if (!customShortcuts.contains(actionName)) {
            customShortcuts[actionName] = action->shortcut();
        }
    }
    
    populateShortcutsTree();
}

void PreferencesDialog::populateShortcutsTree()
{
    shortcutsTree->clear();
    
    // Group actions by menu
    QMap<QString, QTreeWidgetItem*> categories;
    
    for (QAction *action : actionsList) {
        if (action->text().isEmpty()) continue;
        
        // Determine category from action parent
        QString category = "General";
        QMenu *parentMenu = qobject_cast<QMenu*>(action->parent());
        if (parentMenu) {
            category = parentMenu->title().remove('&');
        }
        
        // Create category item if needed
        if (!categories.contains(category)) {
            QTreeWidgetItem *categoryItem = new QTreeWidgetItem(shortcutsTree);
            categoryItem->setText(0, category);
            categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);
            QFont font = categoryItem->font(0);
            font.setBold(true);
            categoryItem->setFont(0, font);
            categories[category] = categoryItem;
        }
        
        // Add action item
        QString actionName = action->text().remove('&');
        QTreeWidgetItem *actionItem = new QTreeWidgetItem(categories[category]);
        actionItem->setText(0, actionName);
        actionItem->setData(0, Qt::UserRole, actionName);
        
        // Show custom shortcut if exists, otherwise show original
        QKeySequence shortcut = customShortcuts.value(actionName, action->shortcut());
        actionItem->setText(1, shortcut.toString(QKeySequence::NativeText));
    }
    
    shortcutsTree->expandAll();
}

void PreferencesDialog::captureKeySequence(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0 || !item->parent()) {
        keySequenceEdit->setEnabled(false);
        clearButton->setEnabled(false);
        currentShortcutLabel->setText(tr("Select an action to edit its shortcut"));
        currentEditItem = nullptr;
        return;
    }
    
    QString actionName = item->data(0, Qt::UserRole).toString();
    if (actionName.isEmpty()) return;
    
    currentEditItem = item;
    currentShortcutLabel->setText(tr("Enter new shortcut for: %1").arg(actionName));
    
    // Set current sequence
    QKeySequence currentSequence = QKeySequence(item->text(1));
    keySequenceEdit->setKeySequence(currentSequence);
    
    keySequenceEdit->setEnabled(true);
    clearButton->setEnabled(true);
    keySequenceEdit->setFocus();
}

void PreferencesDialog::clearShortcut()
{
    if (currentEditItem) {
        keySequenceEdit->clear();
        currentEditItem->setText(1, "");
        
        QString actionName = currentEditItem->data(0, Qt::UserRole).toString();
        customShortcuts[actionName] = QKeySequence();
    }
}

void PreferencesDialog::resetShortcuts()
{
    // Ask for confirmation
    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("Reset Shortcuts"),
        tr("Are you sure you want to reset all shortcuts to their default values?"),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (result == QMessageBox::Yes) {
        customShortcuts = originalShortcuts;
        populateShortcutsTree();
        
        // Clear current edit state
        keySequenceEdit->clear();
        keySequenceEdit->setEnabled(false);
        clearButton->setEnabled(false);
        currentShortcutLabel->setText(tr("Select an action to edit its shortcut"));
        currentEditItem = nullptr;
    }
}

void PreferencesDialog::saveShortcuts()
{
    // Apply shortcuts to actions
    for (QAction *action : actionsList) {
        if (action->text().isEmpty()) continue;
        
        QString actionName = action->text().remove('&');
        if (customShortcuts.contains(actionName)) {
            action->setShortcut(customShortcuts[actionName]);
        }
    }
    
    // Save to settings
    QSettings settings("Paintly", "Paintly");
    settings.beginGroup("Shortcuts");
    
    // Clear existing settings first
    settings.remove("");
    
    // Save new shortcuts
    QMap<QString, QKeySequence>::const_iterator i = customShortcuts.constBegin();
    while (i != customShortcuts.constEnd()) {
        settings.setValue(i.key(), i.value().toString());
        ++i;
    }
    
    settings.endGroup();
    settings.sync();
}

void PreferencesDialog::filterShortcuts(const QString &text)
{
    if (text.isEmpty()) {
        // Show all items when the search text is empty
        for (int i = 0; i < shortcutsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *categoryItem = shortcutsTree->topLevelItem(i);
            categoryItem->setHidden(false);
            
            // Show all child items
            for (int j = 0; j < categoryItem->childCount(); ++j) {
                categoryItem->child(j)->setHidden(false);
            }
        }
        return;
    }
    
    // Filter based on search text (case-insensitive)
    QString searchText = text.toLower();
    
    for (int i = 0; i < shortcutsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = shortcutsTree->topLevelItem(i);
        bool categoryHasVisibleChildren = false;
        
        // Check each child (action) item
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            QTreeWidgetItem *actionItem = categoryItem->child(j);
            
            // Check if the action name or shortcut contains the search text
            bool matches = actionItem->text(0).toLower().contains(searchText) || 
                         actionItem->text(1).toLower().contains(searchText);
            
            actionItem->setHidden(!matches);
            
            if (matches) {
                categoryHasVisibleChildren = true;
            }
        }
        
        // Only show category if it has visible children
        categoryItem->setHidden(!categoryHasVisibleChildren);
    }
    
    // Expand all items when searching
    shortcutsTree->expandAll();
} 