#ifndef COSMICIMPORTLAYERSPOPUP_H
#define COSMICIMPORTLAYERSPOPUP_H

#include "cosmicpopupwindow.h"
#include "object.h"
#include "editor.h"
#include <QPointer>
#include <memory>

class CosmicWindowHeader;
class CosmicButton;
class QLabel;
class QListWidget;
class QListWidgetItem;

// Redesigned Import Layers popup.
// Cards: Source File | Layer Selection
class CosmicImportLayersPopup final : public CosmicPopupWindow
{
    Q_OBJECT

public:
    explicit CosmicImportLayersPopup(QWidget* parent);

    void setCore(Editor* editor);

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void onBrowseFile();
    void onSelectionChanged();
    void onImport();

private:
    void buildUI();
    void loadLayersFromFile();
    void loadKeyFrames(Layer* importedLayer);

    Editor*       mEditor = nullptr;
    QString       mFileName;
    QList<int>    mItemsSelected;
    std::unique_ptr<Object> mImportObject;

    QPointer<CosmicWindowHeader> mHeader;
    QPointer<QLabel>             mFileLabel;
    QPointer<QListWidget>        mLayerList;
    QPointer<QLabel>             mEmptyHint;
    QPointer<CosmicButton>       mImportBtn;
    QPointer<CosmicButton>       mCancelBtn;
};

#endif // COSMICIMPORTLAYERSPOPUP_H
