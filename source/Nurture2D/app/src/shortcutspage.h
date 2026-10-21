#ifndef SHORTCUTSPAGE_H
#define SHORTCUTSPAGE_H

#include <QWidget>
#include <QModelIndex>
#include <QKeySequence>
#include <QSettings>
#include <QHash>
#include <QPointer>

class QTreeView;
class QStandardItem;
class QStandardItemModel;
class QVBoxLayout;
class QLabel;
class QLineEdit;
class PreferenceManager;

namespace Ui
{
    class ShortcutsPage;
}

class ShortcutsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ShortcutsPage(QWidget* parent = nullptr);

    void setManager( PreferenceManager* p ) { mManager = p; updateHighlightColor(); }
    ~ShortcutsPage() override;

protected:
    void changeEvent(QEvent* e) override;

private slots:
    void tableItemClicked(const QModelIndex&);
    void keyCapLineEditTextChanged();
    void onKeySequenceLiveChanged(const QKeySequence& seq);
    void saveShortcutsButtonClicked();
    void loadShortcutsButtonClicked();
    void restoreShortcutsButtonClicked();
    void clearButtonClicked();
    void onSearchTextChanged(const QString& text);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    bool isKeySequenceExist(const QSettings&, QString, QKeySequence);
    void removeDuplicateKeySequence(QSettings*, QKeySequence);
    void treeModelLoadShortcutsSetting();
    void loadCategoryModel(QStandardItemModel* model, const QStringList& cmdNames);
    void rebuildCategoryShortcutsList(QStandardItemModel* model, QVBoxLayout* listLayout);
    void reloadAllCategoryPanes();
    void retranslatePageStrings();
    void updateHighlightColor();
    void onAccentColorChanged();

    void setActiveShortcutModel(QStandardItemModel* model, const QModelIndex& index);
    void cancelRemap(); // Cancel any in-progress remap, restoring the original shortcut label
    void refreshAfterSave(); // Reload category panes + re-run search if search is active

    QStandardItemModel* m_treeModel = nullptr;
    QStandardItemModel* m_fileTreeModel = nullptr;
    QStandardItemModel* m_editTreeModel = nullptr;
    QStandardItemModel* m_viewTreeModel = nullptr;
    QStandardItemModel* m_layerTreeModel = nullptr;
    QStandardItemModel* m_toolsTreeModel = nullptr;
    QStandardItemModel* m_timelineTabTreeModel = nullptr;
    QStandardItemModel* m_sidePanelsTabTreeModel = nullptr;
    QStandardItemModel* m_papersPaneTreeModel = nullptr;
    QStandardItemModel* m_controlCenterTabTreeModel = nullptr;
    QStandardItemModel* m_controlCenterPaletteTreeModel = nullptr;
    QStandardItemModel* m_controlCenterAutoFillTreeModel = nullptr;
    QStandardItemModel* m_controlCenterVisibilityTreeModel = nullptr;
    QStandardItemModel* m_searchResultsTreeModel = nullptr;
    QStandardItemModel* m_activeModel = nullptr;
    QModelIndex m_currentItemIndex;

    QStandardItemModel* m_remapModel = nullptr;
    QModelIndex m_remapIndex;
    QPointer<QWidget> m_remapRow;
    QPointer<QWidget> m_remapShortcutContainer;
    QPointer<QLabel> m_remapShortcutLabel;
    QString m_remapOriginalText;
    bool m_settingKeySequenceProgrammatically = false;

    QHash<const QObject*, QModelIndex> m_shortcutRowIndex;
    QHash<const QObject*, QStandardItemModel*> m_shortcutRowModel;

    Ui::ShortcutsPage* ui = nullptr;

    PreferenceManager* mManager = nullptr;
    QColor mAccentColor;
};

#endif // SHORTCUTSPAGE_H
