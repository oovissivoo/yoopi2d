/*

Pencil2D - Traditional Animation Software
This file adds a ReferenceWindow dock widget for displaying reference images.

*/
#ifndef REFERENCEWINDOW_H
#define REFERENCEWINDOW_H

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QList>
#include <QImage>
#include <QStringList>
#include <QGraphicsView>
#include <QListWidget>
#include <QTimer>
#include <QPointer>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>

#include "basedockwidget.h"
#include "referencecanvas.h"

class QGraphicsScene;
class QGraphicsPixmapItem;
class BlenderStyleToggle;
class CosmicCheckboxToggle;
class CosmicButton;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;

class ReferenceGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ReferenceGraphicsView(QWidget* parent = nullptr);

    void setColorSampleEnabled(bool enabled) { mColorSampleEnabled = enabled; }
    void setSourcePixmapItem(QGraphicsPixmapItem* item) { mSourcePixmapItem = item; }

    friend class RefViewEventFilter;

signals:
    void colorSampled(const QColor& color);
    void requestZoomIn();
    void requestZoomOut();
    void requestZoomPreset(int percent);
    void requestScaleBy(double factor);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    QColor sampleColorAtViewportPos(const QPoint& viewportPos) const;
    void updateEyedropperCursor(const QColor& color);

private:
    bool mColorSampleEnabled = true;
    bool mIsSpacePressed = false;
    bool mIsCtrlPressed = false;
    bool mIsZoomDragging = false;
    bool mIsSampling = false;
    bool mIsPanning = false;
    QPoint mLastMousePos;
    QPoint mPanLastPos;
    QGraphicsPixmapItem* mSourcePixmapItem = nullptr;
};

class ReferenceWindow : public BaseDockWidget
{
    Q_OBJECT
public:
    explicit ReferenceWindow(QWidget* parent = nullptr);

    void initUI() override;
    void updateUI() override;

    // Access to the Gallery toggle widget (for event filtering)
    QWidget* galleryToggleWidget() const;

signals:
    // Emitted when the Gallery toggle is switched
    void galleryToggled(bool on);
    // Emitted whenever the image list or selection changes
    void galleryContentUpdated(const QVector<QImage>& images, const QStringList& labels, int currentIndex, const QStringList& categories);

public slots:
    void selectGalleryIndex(int index);
    void emitGalleryUpdate();
    void deleteImagesByIndices(const QList<int>& indices);
    void clearAllImagesSlot();
    void deleteCategorySlot(const QString& category);
    void setActiveCategorySlot(const QString& category);
    void setGalleryToggle(bool on);
    void triggerGalleryToggle(bool on);
    void importFromMimeDataSlot(const QMimeData* mime);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onZoomInAction();
    void onZoomOutAction();
    void onZoomPreset(int percent);
    void onScaleBy(double factor);
    void onGalleryToggled(bool on) { emit galleryToggled(on); }

private:
    struct RefImage {
        QString key;      // absolute path for file imports, or "canvas" for canvas snapshot
        QImage image;
        bool isCanvas = false;
        QString category = QStringLiteral("General"); // folder name or "General"
    };

    // UI elements
    QWidget* mRoot = nullptr;
    QVBoxLayout* mRootLayout = nullptr;

    QWidget* mTopBar = nullptr;
    QHBoxLayout* mTopBarLayout = nullptr;
    BlenderStyleToggle* mGalleryToggleLegacy = nullptr;
    CosmicCheckboxToggle* mGalleryToggle = nullptr;

    QWidget* mCenterStack = nullptr; // stacked manually via show/hide

    // Image view
    ReferenceGraphicsView* mView = nullptr; // legacy pointer; now using ReferenceCanvas inside QScrollArea
    QGraphicsScene* mScene = nullptr;
    QGraphicsPixmapItem* mPixmapItem = nullptr;
    ReferenceCanvas* mCanvas = nullptr;

    // Bottom controls (now an overlay inside the image area)
    CosmicButton* mFileButton = nullptr;
    CosmicButton* mGetCanvasButton = nullptr;
    CosmicButton* mPrevButton = nullptr;
    CosmicButton* mResetZoomButton = nullptr;
    CosmicButton* mNextButton = nullptr;
    CosmicButton* mZoomInButton = nullptr;
    CosmicButton* mZoomOutButton = nullptr;

    // Squircle overlay that floats centered at the bottom of the image area
    QWidget* mControlsOverlay = nullptr;
    // Squircle overlay that floats centered at the top of the image area
    QWidget* mTopControlsOverlay = nullptr;

    // Press-and-hold zoom repeat
    QTimer* mZoomRepeatTimer = nullptr;
    int     mZoomRepeatDelta = 0;

    // Strip overlay
    QWidget* mStripOverlay = nullptr;
    QVBoxLayout* mStripLayout = nullptr;
    QListWidget* mThumbList = nullptr;
    QWidget* mStripButtons = nullptr;
    QHBoxLayout* mStripButtonsLayout = nullptr;
    CosmicButton* mTrashButton = nullptr;
    CosmicButton* mClearAllButton = nullptr;

    // Data
    QVector<RefImage> mImages;
    QSet<QString> mImportedPaths;
    int mCurrentIndex = -1;
    int mCanvasIndex = -1;
    double mZoomScale = 1.0;
    QString mActiveGalleryCategory = QStringLiteral("General"); // tracks overlay selection

    // Helpers
    void setupTopBar();
    void setupSliderBar();
    void setupCenterView();
    void setupBottomBar();
    void setupStripOverlay();
    void repositionControlsOverlay();

    // Helper to find the active ReferenceCanvas when used inside a QScrollArea
    class ReferenceCanvas* currentCanvas() const;

    void applyZoom(double scale);
    void refreshPixmap();

    void openFiles();
    void addOrSelectImageFromPath(const QString& path, const QString& category = QStringLiteral("General"));
    void importFromMimeData(const QMimeData* mime);
    bool mimeHasImportableUrls(const QMimeData* mime) const;
    void setCurrentIndex(int index);

    void getCanvasSnapshot();
    QImage renderCurrentFrameComposite() const;
    void replaceOrCreateCanvasImage(const QImage& img);

    void showStrip(bool show);
    void rebuildThumbnails();
    void deleteSelectedFromStrip();
    void clearAllImages();

    bool isActiveTarget() const;
    void setZoomPercent(int percent);

};

#endif // REFERENCEWINDOW_H