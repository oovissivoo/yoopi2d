/*
  CanvasGalleryOverlay - gallery overlay in canvas area
*/
#ifndef CANVASGALLERYOVERLAY_H
#define CANVASGALLERYOVERLAY_H

#include "cosmicpopupwindow.h"

#include <QVector>
#include <QImage>
#include <QStringList>
#include <QPixmap>
#include <QMap>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class CosmicButton;
class CosmicHSlider;
class CosmicWindowHeader;
class QMimeData;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class CanvasGalleryOverlay : public CosmicPopupWindow
{
    Q_OBJECT
public:
    explicit CanvasGalleryOverlay(QWidget* parent = nullptr);

    void setImages(const QVector<QImage>& images, const QStringList& labels, int currentIndex,
                   const QStringList& categories = QStringList());

    QString activeCategory() const { return mActiveCategory; }

    void showOverlay();
    void showOverlayAt(const QPoint& globalCenter);
    void hideOverlay();

signals:
    void imageActivated(int index);
    void requestDeleteSelected(const QList<int>& indices);
    void requestClearAll();
    void requestDeleteCategory(const QString& category);
    void requestClose();
    void activeCategoryChanged(const QString& category);
    void requestImportMimeData(const QMimeData* mime);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    // Chrome
    CosmicWindowHeader* mWindowHeader = nullptr;
    QPixmap mLogoPixmap;

    // Content layout
    QWidget*     mContentWidget  = nullptr;
    QWidget*     mSearchBar      = nullptr;
    QWidget*     mFolderListArea = nullptr;
    QWidget*     mGalleryArea    = nullptr;
    QListWidget* mCategoryList   = nullptr;
    QMap<QString, CosmicButton*> mCategoryTrashBtns; // category name → trash button

    void updateContentGeometry();
    void rebuildCategoryList();
    void repositionCategoryButtons();
    void applyCategoryFilter();

    // Grid + controls
    QListWidget*   mGrid           = nullptr;
    QLineEdit*     mSearch         = nullptr;
    CosmicHSlider* mThumbSlider    = nullptr;
    CosmicButton*  mTrashButton    = nullptr;
    int mThumbSize = 160;
    int mSpacing   = 12;

    QVector<QImage> mImagesCache;
    QStringList     mLabelsCache;
    QStringList     mCategoriesCache;
    QStringList     mUniqueCategoriesCache;
    QString         mActiveCategory = QStringLiteral("General");
    int             mCurrentIndexCache = -1;

    void rebuildGrid(const QVector<QImage>& images, const QStringList& labels, int currentIndex);
    void filterItems(const QString& text);
    void setThumbSize(int px);
};

#endif // CANVASGALLERYOVERLAY_H
