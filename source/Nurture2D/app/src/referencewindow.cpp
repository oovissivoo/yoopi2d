#include "referencewindow.h"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "referencecanvas.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollBar>
#include <QListWidgetItem>
#include <QIcon>
#include <QPixmap>
#include <QImageReader>
#include <QPainter>
#include <QApplication>
#include <QAction>
#include <cmath>
#include <algorithm>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QFrame>
#include <QScrollArea>
#include "colormanager.h"
#include "../CUI/blenderstyletoggle.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "filedialog.h"
#include "mainwindow2.h"
#include "editor.h"
#include "object.h"
#include "layercamera.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "graphics/bitmap/bitmapimage.h"
#include "graphics/vector/vectorimage.h"

// Small helper: a QWidget that paints a semi-transparent rounded rect (squircle) background.
// Using paintEvent is the only reliable way to get rgba + border-radius on a non-top-level widget.
class SquirclePanel : public QWidget {
public:
    explicit SquirclePanel(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(31, 31, 31, 210));
        p.drawRoundedRect(rect(), 14, 14);
    }
};

// ReferenceGraphicsView: pans when space is held
ReferenceGraphicsView::ReferenceGraphicsView(QWidget* parent)
    : QGraphicsView(parent)
{
    setDragMode(QGraphicsView::NoDrag);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setBackgroundBrush(QColor(40, 40, 40));
    setFocusPolicy(Qt::StrongFocus);
    if (viewport()) {
        viewport()->setCursor(Qt::ArrowCursor);
        viewport()->setMouseTracking(true);
    }

    setMouseTracking(true);
}

bool ReferenceGraphicsView::event(QEvent* event)
{
    // Ensure the view becomes the active focus target when hovered/entered,
    // so key modifiers (Alt/Ctrl/Space) apply even before a click.
    if (event->type() == QEvent::Enter) {
        if (!hasFocus()) {
            setFocus(Qt::OtherFocusReason);
        }
        // Reset any stale pan mode when entering
        setDragMode(QGraphicsView::NoDrag);
        // If Alt is already held when entering, start sampling immediately
        if (mColorSampleEnabled && (QApplication::keyboardModifiers() & Qt::AltModifier)) {
            if (!mIsSampling) {
                mIsSampling = true;
                QPixmap px(":/icons/themes/playful/display/overlay-center.svg");
                if (!px.isNull()) {
                    px = px.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    setCursor(QCursor(px, px.width() / 2, px.height() / 2));
                    if (viewport()) viewport()->setCursor(QCursor(px, px.width() / 2, px.height() / 2));
                } else {
                    setCursor(Qt::CrossCursor);
                    if (viewport()) viewport()->setCursor(Qt::CrossCursor);
                }
            }
        } else {
            // Explicitly restore default cursor to avoid inheriting canvas hand cursor
            if (viewport()) viewport()->setCursor(Qt::ArrowCursor);
            setCursor(Qt::ArrowCursor);
         }
    } else if (event->type() == QEvent::Leave) {
        // Leaving the view ends sampling mode and restores cursor so we don't keep
        // sampling when cursor is over another dock/canvas.
        setDragMode(QGraphicsView::NoDrag);
        if (mIsSampling) {
            mIsSampling = false;
            unsetCursor();
            if (viewport()) viewport()->setCursor(Qt::ArrowCursor);
        }
    }
    return QGraphicsView::event(event);
}

void ReferenceGraphicsView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        mIsSpacePressed = true;
        setDragMode(QGraphicsView::ScrollHandDrag);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Alt) {
        // Begin sampling mode on Alt press
        if (mColorSampleEnabled && !mIsSampling) {
            mIsSampling = true;
            // Use eyedropper cursor with color preview
            QPixmap icon(":icons/general/cursor-eyedropper.svg");
            QPixmap pixmap(32, 32);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.drawPixmap(0, 0, icon);
            // Add color preview square (will be updated on mouse move)
            painter.setBrush(Qt::white);
            painter.drawRect(17, 17, 13, 13);
            painter.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::black); // Default color until we sample
            painter.drawRect(16, 16, 15, 15);
            painter.end();
            setCursor(QCursor(pixmap, 0, 15));
            if (viewport()) viewport()->setCursor(QCursor(pixmap, 0, 15));
            
            // Immediately sample color at current cursor position
            QPoint globalPos = QCursor::pos();
            QPoint localPos = mapFromGlobal(globalPos);
            if (rect().contains(localPos)) {
                QColor c = sampleColorAtViewportPos(localPos);
                if (c.isValid()) {
                    emit colorSampled(c);
                    updateEyedropperCursor(c);
                }
            }
            
        }
    }
    if (event->key() == Qt::Key_Control) {
        mIsCtrlPressed = true;
    }
    // Mirror core zoom shortcuts (common ones):
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Up) {
        emit requestZoomIn(); event->accept(); return;
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Down) {
        emit requestZoomOut(); event->accept(); return;
    }
    // Quick presets: 4->400, 3->300, 2->200, 1->100, Shift+2->50, Shift+3->33, Shift+4->25
    if (!event->modifiers()) {
        if (event->key() == Qt::Key_4) { emit requestZoomPreset(400); event->accept(); return; }
        if (event->key() == Qt::Key_3) { emit requestZoomPreset(300); event->accept(); return; }
        if (event->key() == Qt::Key_2) { emit requestZoomPreset(200); event->accept(); return; }
        if (event->key() == Qt::Key_1) { emit requestZoomPreset(100); event->accept(); return; }
    }
    if (event->modifiers() == Qt::ShiftModifier) {
        if (event->key() == Qt::Key_2) { emit requestZoomPreset(50); event->accept(); return; }
        if (event->key() == Qt::Key_3) { emit requestZoomPreset(33); event->accept(); return; }
        if (event->key() == Qt::Key_4) { emit requestZoomPreset(25); event->accept(); return; }
    }
    QGraphicsView::keyPressEvent(event);
}

void ReferenceGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        mIsSpacePressed = false;
        mIsZoomDragging = false;
        setDragMode(QGraphicsView::NoDrag);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Alt) {
        // End sampling mode on Alt release
        if (mIsSampling) {
            mIsSampling = false;
            unsetCursor();
            if (viewport()) viewport()->setCursor(Qt::ArrowCursor);
        }
    }
    if (event->key() == Qt::Key_Control) {
        mIsCtrlPressed = false;
        mIsZoomDragging = false;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void ReferenceGraphicsView::wheelEvent(QWheelEvent* event)
{
    // Mirror canvas: Ctrl + wheel zooms; Shift can reduce step; plain wheel scrolls
    const Qt::KeyboardModifiers mods = event->modifiers();
    if (mods & Qt::ControlModifier) {
        double numSteps = event->angleDelta().y() / 120.0; // 1 per notch
        double factor = std::pow(1.1, numSteps);
        if (mods & Qt::ShiftModifier) {
            factor = std::pow(1.05, numSteps);
        }
        emit requestScaleBy(factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void ReferenceGraphicsView::mousePressEvent(QMouseEvent* event)
{
    // Color sampling is now handled by Alt+hold, no need for click sampling
    // Start zoom drag if Ctrl+Space
    if (mIsSpacePressed && mIsCtrlPressed && event->button() == Qt::LeftButton) {
        mIsZoomDragging = true;
        mLastMousePos = event->pos();
        setCursor(Qt::SizeVerCursor);
        if (viewport()) viewport()->setCursor(Qt::SizeVerCursor);
        event->accept();
        return;
    }

    // Start panning on click-drag when not sampling
    if (!mIsSampling && event->button() == Qt::LeftButton) {
        mIsPanning = true;
        mPanLastPos = event->pos();
        setDragMode(QGraphicsView::ScrollHandDrag);
        // forward to base so default press behavior still occurs
    }
    QGraphicsView::mousePressEvent(event);
}

void ReferenceGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
    if (mIsPanning) {
        // Use default drag behavior by synthesizing a mouse move to the base
        QPoint delta = event->pos() - mPanLastPos;
        mPanLastPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    
    // Handle color sampling on mouse move
    if (mColorSampleEnabled && mIsSampling) {
        // Double-check that Alt is still held (in case key release was missed)
        if (event->modifiers() & Qt::AltModifier) {
            QColor c = sampleColorAtViewportPos(event->pos());
            if (c.isValid()) {
                emit colorSampled(c);
                updateEyedropperCursor(c);
            }
        } else {
            // Alt was released, stop sampling
            mIsSampling = false;
            unsetCursor();
            if (viewport()) viewport()->setCursor(Qt::ArrowCursor);
        }
        event->accept();
        return;
    }
    
    QGraphicsView::mouseMoveEvent(event);
}

void ReferenceGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
    if (mIsPanning && event->button() == Qt::LeftButton) {
        mIsPanning = false;
        setDragMode(QGraphicsView::NoDrag);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

QColor ReferenceGraphicsView::sampleColorAtViewportPos(const QPoint& viewportPos) const
{
    if (!mSourcePixmapItem || mSourcePixmapItem->pixmap().isNull()) {
        // Fallback: sample the displayed framebuffer if item is missing
        QImage img(size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        const_cast<ReferenceGraphicsView*>(this)->render(&p);
        p.end();
        if (img.rect().contains(viewportPos)) {
            return QColor::fromRgba(img.pixel(viewportPos));
        }
        return QColor();
    }

    // Map viewport to scene
    QPointF scenePt = mapToScene(viewportPos);
    // Map scene to item's local coordinates
    QPointF itemPt = mSourcePixmapItem->mapFromScene(scenePt);

    QPixmap px = mSourcePixmapItem->pixmap();
    if (px.isNull()) return QColor();
    QImage image = px.toImage();

    // Convert itemPt (floating) to image pixel coordinates taking into account device pixel ratio
    const qreal dpr = px.devicePixelRatioF();
    QPointF offset = mSourcePixmapItem->offset();
    qreal localX = (itemPt.x() - offset.x());
    qreal localY = (itemPt.y() - offset.y());
    int ix = static_cast<int>(std::floor(localX * dpr));
    int iy = static_cast<int>(std::floor(localY * dpr));
    if (ix < 0 || iy < 0 || ix >= image.width() || iy >= image.height()) {
        return QColor();
    }
    return QColor::fromRgba(image.pixel(ix, iy));
}

void ReferenceGraphicsView::updateEyedropperCursor(const QColor& color)
{
    if (!mIsSampling) return;
    
    // Create eyedropper cursor with color preview
    QPixmap icon(":icons/general/cursor-eyedropper.svg");
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.drawPixmap(0, 0, icon);
    
    // Add color preview square
    painter.setBrush(Qt::white);
    painter.drawRect(17, 17, 13, 13);
    painter.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    painter.drawRect(16, 16, 15, 15);
    painter.end();
    
    setCursor(QCursor(pixmap, 0, 15));
    if (viewport()) viewport()->setCursor(QCursor(pixmap, 0, 15));
}

// Track mouse move via event filter on the viewport
class RefViewEventFilter : public QObject {
public:
    RefViewEventFilter(ReferenceGraphicsView* v) : QObject(v), view(v) {}
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (view->mIsZoomDragging) {
                int dy = me->pos().y() - view->mLastMousePos.y();
                view->mLastMousePos = me->pos();
                // Similar to canvas smooth zoom: exponential scale by dy
                double factor = std::pow(100.0, (-dy) / (12.0 * 15.0));
                view->requestScaleBy(factor);
                return true;
            } else if (view->mColorSampleEnabled && (QApplication::keyboardModifiers() & Qt::AltModifier)) {
                // Continuous sampling while Alt is held: sample under cursor precisely
                if (!view->mIsSampling) {
                    // Enter sampling state and set eyedropper cursor
                    view->mIsSampling = true;
                    QPixmap icon(":icons/general/cursor-eyedropper.svg");
                    QPixmap pixmap(32, 32);
                    pixmap.fill(Qt::transparent);
                    QPainter p(&pixmap);
                    p.drawPixmap(0,0, icon);
                    p.setBrush(Qt::white);
                    p.drawRect(17,17,13,13);
                    p.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    p.setBrush(Qt::black);
                    p.drawRect(16,16,15,15);
                    p.end();
                    view->setCursor(QCursor(pixmap, 0, 15));
                    if (view->viewport()) view->viewport()->setCursor(QCursor(pixmap, 0, 15));
                }
                QColor c = view->sampleColorAtViewportPos(me->pos());
                if (c.isValid()) {
                    emit view->colorSampled(c);
                    view->updateEyedropperCursor(c);
                }
                return true;
            } else if (view->mIsSampling) {
                // Alt not held any more: exit sampling state
                view->mIsSampling = false;
                view->unsetCursor();
                if (view->viewport()) view->viewport()->setCursor(Qt::ArrowCursor);
            }
        } else if (ev->type() == QEvent::Enter) {
            // Focus and cursor management when entering the viewport
            if (!view->hasFocus()) {
                view->setFocus(Qt::OtherFocusReason);
            }
            view->setDragMode(QGraphicsView::NoDrag);
            if (view->mColorSampleEnabled && (QApplication::keyboardModifiers() & Qt::AltModifier)) {
                if (!view->mIsSampling) {
                    view->mIsSampling = true;
                    QPixmap px(":/icons/themes/playful/display/overlay-center.svg");
                    if (!px.isNull()) {
                        px = px.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        view->viewport()->setCursor(QCursor(px, px.width() / 2, px.height() / 2));
                    } else {
                        view->viewport()->setCursor(Qt::CrossCursor);
                    }
                }
            } else {
                view->viewport()->setCursor(Qt::ArrowCursor);
            }
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            if (view->mIsZoomDragging) {
                view->mIsZoomDragging = false;
                view->unsetCursor();
                if (view->viewport()) view->viewport()->setCursor(Qt::ArrowCursor);
                return true;
            }
        } else if (ev->type() == QEvent::Leave) {
            view->setDragMode(QGraphicsView::NoDrag);
            if (view->mIsSampling) {
                view->mIsSampling = false;
                view->unsetCursor();
            }
            if (view->viewport()) view->viewport()->setCursor(Qt::ArrowCursor);
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    ReferenceGraphicsView* view;
};

// Install the event filter after construction of the view
// (Do this in setupCenterView once mView exists.)
ReferenceWindow::ReferenceWindow(QWidget* parent)
    : BaseDockWidget(parent)
{
    setObjectName("ReferenceWindow");
    setTitle(tr("Reference Window"));
    setAcceptDrops(true);
}

void ReferenceWindow::initUI()
{
    mRoot = new QWidget(this);
    setDockContentWidget(mRoot);

    // Ensure toggle action has a visible title
    setWindowTitle(tr("Reference Window"));

    mRootLayout = new QVBoxLayout(mRoot);
    mRootLayout->setContentsMargins(6,6,6,6);
    mRootLayout->setSpacing(6);

    setupTopBar();
    setupCenterView();
    setupBottomBar();
    setupStripOverlay();

    // Position the controls overlays now that all widgets exist
    repositionControlsOverlay();

    // Track mCenterStack resizes to reposition the overlays
    if (mCenterStack) mCenterStack->installEventFilter(this);

    // Reference canvas should take focus to receive shortcuts when dock is active
    if (mCanvas) mCanvas->setFocusPolicy(Qt::StrongFocus);

    // Link color sampler to app color
    if (mCanvas) connect(mCanvas, &ReferenceCanvas::colorSampled, this, [this](const QColor& c){
        if (editor() && editor()->color()) editor()->color()->setFrontColor(c);
    });

    // Mirror canvas zoom actions (use MainWindow2 actions so shortcuts work)
    if (auto mw = qobject_cast<MainWindow2*>(parentWidget())) {
        // When the dock has focus, emitters update our own zoom
        connect(mw->findChild<QAction*>("actionZoom_In"), &QAction::triggered, this, &ReferenceWindow::onZoomInAction);
        connect(mw->findChild<QAction*>("actionZoom_Out"), &QAction::triggered, this, &ReferenceWindow::onZoomOutAction);
        connect(mw->findChild<QAction*>("actionZoom400"), &QAction::triggered, this, [this]{ onZoomPreset(400); });
        connect(mw->findChild<QAction*>("actionZoom300"), &QAction::triggered, this, [this]{ onZoomPreset(300); });
        connect(mw->findChild<QAction*>("actionZoom200"), &QAction::triggered, this, [this]{ onZoomPreset(200); });
        connect(mw->findChild<QAction*>("actionZoom100"), &QAction::triggered, this, [this]{ onZoomPreset(100); });
        connect(mw->findChild<QAction*>("actionZoom50"),  &QAction::triggered, this, [this]{ onZoomPreset(50);  });
        connect(mw->findChild<QAction*>("actionZoom33"),  &QAction::triggered, this, [this]{ onZoomPreset(33);  });
        connect(mw->findChild<QAction*>("actionZoom25"),  &QAction::triggered, this, [this]{ onZoomPreset(25);  });
    }

    // Internal canvas-to-window zoom requests
    if (mCanvas) connect(mCanvas, &ReferenceCanvas::requestZoomIn,  this, &ReferenceWindow::onZoomInAction);
    if (mCanvas) connect(mCanvas, &ReferenceCanvas::requestZoomOut, this, &ReferenceWindow::onZoomOutAction);
    if (mCanvas) connect(mCanvas, &ReferenceCanvas::requestZoomPreset, this, &ReferenceWindow::onZoomPreset);
    if (mCanvas) connect(mCanvas, &ReferenceCanvas::requestScaleBy, this, &ReferenceWindow::onScaleBy);

    updateUI();
    emitGalleryUpdate();
}

void ReferenceWindow::clearAllImagesSlot()
{
    clearAllImages();
}

void ReferenceWindow::setGalleryToggle(bool on)
{
    if (!mGalleryToggle) return;
    QSignalBlocker block(mGalleryToggle);
    mGalleryToggle->setChecked(on);
    // Do not emit galleryToggled here; caller (MainWindow2) handles overlay state
}

void ReferenceWindow::triggerGalleryToggle(bool on)
{
    if (!mGalleryToggle) return;
    // Use the normal setChecked which emits toggled if state changes
    mGalleryToggle->setChecked(on);
}

QWidget* ReferenceWindow::galleryToggleWidget() const
{
    return reinterpret_cast<QWidget*>(mGalleryToggle);
}

void ReferenceWindow::updateUI()
{
    // At init: no image selected
    if (mCurrentIndex >= 0 && mCurrentIndex < mImages.size()) {
        refreshPixmap();
    }
}

void ReferenceWindow::setupTopBar()
{
    mTopBar = new QWidget(mRoot);
    mTopBarLayout = new QHBoxLayout(mTopBar);
    mTopBarLayout->setContentsMargins(0,0,0,0);
    // Top bar is intentionally minimal; controls live in the overlay
    mRootLayout->addWidget(mTopBar, 0);
}

void ReferenceWindow::setupCenterView()
{
    mCenterStack = new QWidget(mRoot);
    auto centerLayout = new QVBoxLayout(mCenterStack);
    centerLayout->setContentsMargins(0,0,0,0);

    // Use ReferenceCanvas inside a scroll area to maintain scrollbar behavior
    mScene = new QGraphicsScene(mCenterStack);
    // create scroll area and canvas
    QScrollArea* scroll = new QScrollArea(mCenterStack);
    // Do not auto-resize the canvas; keep canvas intrinsic size (image * zoom)
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignCenter);
    // Always show scrollbars so their appearance doesn't shift layout when zoom changes
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    // Apply pill scrollbar styling to match tool panels
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 26px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  height: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-width: 26px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: transparent; width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
    ));
    // Make scroll viewport background match canvas background
    scroll->viewport()->setStyleSheet("background-color: rgb(40,40,40);");

    // Accept drag/drop on the actual image area (viewport + canvas)
    scroll->setAcceptDrops(true);
    if (scroll->viewport()) {
        scroll->viewport()->setAcceptDrops(true);
        scroll->viewport()->installEventFilter(this);
    }
    ReferenceCanvas* canvas = new ReferenceCanvas(scroll);
    canvas->setAcceptDrops(true);
    canvas->installEventFilter(this);
    scroll->setWidget(canvas);
    mCanvas = canvas;
    mView = nullptr; // no longer using ReferenceGraphicsView
    // connect sampling/zoom signals
    connect(canvas, &ReferenceCanvas::colorSampled, this, [this](const QColor& c){ if (editor() && editor()->color()) editor()->color()->setFrontColor(c); });
    connect(canvas, &ReferenceCanvas::requestScaleBy, this, &ReferenceWindow::onScaleBy);

    // Ensure the dock forwards focus to the inner view on hover/focus
    // so global shortcuts work without an explicit click
    // When using ReferenceCanvas inside a scroll area, set focus proxy to the scroll's widget
    setFocusProxy(canvas);

    // We'll keep a reference to the canvas via a member QGraphicsPixmapItem for compatibility
    mPixmapItem = nullptr;
    centerLayout->addWidget(scroll, 1);

    mRootLayout->addWidget(mCenterStack, 1);
}

bool ReferenceWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return BaseDockWidget::eventFilter(watched, event);
    }

    // Reposition the floating controls overlay whenever the image area resizes
    if (watched == mCenterStack && event->type() == QEvent::Resize) {
        repositionControlsOverlay();
        return false;
    }

    switch (event->type()) {
    case QEvent::DragEnter: {
        auto* e = static_cast<QDragEnterEvent*>(event);
        if (mimeHasImportableUrls(e->mimeData())) {
            e->acceptProposedAction();
            return true;
        }
        break;
    }
    case QEvent::DragMove: {
        auto* e = static_cast<QDragMoveEvent*>(event);
        if (mimeHasImportableUrls(e->mimeData())) {
            e->acceptProposedAction();
            return true;
        }
        break;
    }
    case QEvent::Drop: {
        auto* e = static_cast<QDropEvent*>(event);
        const QMimeData* mime = e->mimeData();
        if (mimeHasImportableUrls(mime)) {
            importFromMimeData(mime);
            emitGalleryUpdate();
            e->acceptProposedAction();
            return true;
        }
        break;
    }
    default:
        break;
    }

    return BaseDockWidget::eventFilter(watched, event);
}

ReferenceCanvas* ReferenceWindow::currentCanvas() const
{
    if (!mCenterStack) return nullptr;
    QScrollArea* scroll = mCenterStack->findChild<QScrollArea*>();
    if (!scroll) return nullptr;
    return qobject_cast<ReferenceCanvas*>(scroll->widget());
}

void ReferenceWindow::setupSliderBar()
{
    // Zoom slider removed; controls are now in the floating overlay
}

void ReferenceWindow::setupBottomBar()
{
    // Shared helpers
    auto makeBtn = [](QWidget* parent, const QString& icon, const QString& tip) -> CosmicButton* {
        auto* btn = new CosmicButton(parent);
        btn->setToolTip(tip);
        btn->setText("");
        btn->setIcon(QIcon(icon));
        btn->setIconSize(QSize(14, 14));
        btn->setFixedSize(22, 22);
        btn->setBordered(false);
        return btn;
    };
    auto makeSep = [](QWidget* parent) -> QFrame* {
        auto* sep = new QFrame(parent);
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setStyleSheet("background: rgba(255,255,255,40); border: none;");
        return sep;
    };

    // Press-and-hold zoom timer
    mZoomRepeatTimer = new QTimer(this);
    mZoomRepeatTimer->setInterval(60);
    connect(mZoomRepeatTimer, &QTimer::timeout, this, [this]{
        setZoomPercent(static_cast<int>(mZoomScale * 100) + mZoomRepeatDelta);
    });

    // ── TOP overlay: zoom in | fit | zoom out ───────────────────────────────
    mTopControlsOverlay = new SquirclePanel(mCenterStack);
    mTopControlsOverlay->setAutoFillBackground(false);

    auto* topLayout = new QHBoxLayout(mTopControlsOverlay);
    topLayout->setContentsMargins(8, 4, 8, 4);
    topLayout->setSpacing(2);

    mZoomInButton = makeBtn(mTopControlsOverlay, ":/Interaction/ZoomIn.svg", tr("Zoom in"));
    mResetZoomButton = makeBtn(mTopControlsOverlay, ":/Interaction/ReffWindowResetView.svg", tr("Fit to view"));
    mZoomOutButton = makeBtn(mTopControlsOverlay, ":/Interaction/ZoomOut.svg", tr("Zoom out"));

    // Zoom in: press-and-hold
    connect(mZoomInButton, &QPushButton::pressed, this, [this]{
        mZoomRepeatDelta = 5;
        setZoomPercent(static_cast<int>(mZoomScale * 100) + mZoomRepeatDelta);
        mZoomRepeatTimer->start();
    });
    connect(mZoomInButton, &QPushButton::released, this, [this]{ mZoomRepeatTimer->stop(); });

    // Zoom out: press-and-hold
    connect(mZoomOutButton, &QPushButton::pressed, this, [this]{
        mZoomRepeatDelta = -5;
        setZoomPercent(static_cast<int>(mZoomScale * 100) + mZoomRepeatDelta);
        mZoomRepeatTimer->start();
    });
    connect(mZoomOutButton, &QPushButton::released, this, [this]{ mZoomRepeatTimer->stop(); });

    // Fit to view
    connect(mResetZoomButton, &QPushButton::clicked, this, [this]{
        if (mCurrentIndex < 0 || mCurrentIndex >= mImages.size()) return;
        ReferenceCanvas* canvas = currentCanvas();
        if (canvas) {
            canvas->fitToView();
            mZoomScale = canvas->zoomScale();
        }
    });

    topLayout->addWidget(mZoomInButton);
    topLayout->addWidget(mResetZoomButton);
    topLayout->addWidget(mZoomOutButton);

    mTopControlsOverlay->adjustSize();
    mTopControlsOverlay->raise();
    mTopControlsOverlay->show();

    // ── BOTTOM overlay: prev/next | import/snapshot | gallery ──────────────
    mControlsOverlay = new SquirclePanel(mCenterStack);
    mControlsOverlay->setAutoFillBackground(false);

    auto* botLayout = new QHBoxLayout(mControlsOverlay);
    botLayout->setContentsMargins(8, 4, 8, 4);
    botLayout->setSpacing(2);

    mPrevButton = makeBtn(mControlsOverlay, ":/Interaction/FrameFirst.svg", tr("Previous image"));
    connect(mPrevButton, &QPushButton::clicked, this, [this]{
        if (mImages.isEmpty()) return;
        setCurrentIndex((mCurrentIndex - 1 + mImages.size()) % mImages.size());
    });

    mNextButton = makeBtn(mControlsOverlay, ":/Interaction/FrameLast.svg", tr("Next image"));
    connect(mNextButton, &QPushButton::clicked, this, [this]{
        if (mImages.isEmpty()) return;
        setCurrentIndex((mCurrentIndex + 1) % mImages.size());
    });

    botLayout->addWidget(mPrevButton);
    botLayout->addWidget(mNextButton);
    botLayout->addWidget(makeSep(mControlsOverlay));

    mGetCanvasButton = makeBtn(mControlsOverlay, ":/Interaction/ReffWindowSnapShot.svg", tr("Get canvas image"));
    connect(mGetCanvasButton, &QPushButton::clicked, this, &ReferenceWindow::getCanvasSnapshot);

    botLayout->addWidget(mGetCanvasButton);
    botLayout->addWidget(makeSep(mControlsOverlay));

    mGalleryToggle = new CosmicCheckboxToggle(mControlsOverlay);
    mGalleryToggle->setMode(CosmicCheckboxToggle::Custom);
    mGalleryToggle->setShowText(false);
    mGalleryToggle->setShowIcon(true);
    mGalleryToggle->setBordered(false);
    mGalleryToggle->setToolTip(tr("Show Gallery"));
    mGalleryToggle->setIcon(QIcon(":/Interaction/ReffWindowGallery.svg"));
    mGalleryToggle->setFixedSize(22, 22);
    connect(mGalleryToggle, &QAbstractButton::toggled, this, &ReferenceWindow::onGalleryToggled);

    mFileButton = makeBtn(mControlsOverlay, ":/Interaction/ReffWindowImport.svg", tr("Import image files"));
    connect(mFileButton, &QPushButton::clicked, this, &ReferenceWindow::openFiles);

    botLayout->addWidget(mGalleryToggle);
    botLayout->addWidget(mFileButton);

    mControlsOverlay->adjustSize();
    mControlsOverlay->raise();
    mControlsOverlay->show();
}

void ReferenceWindow::setupStripOverlay()
{
    // Overlay lives inside root; hidden by default and overlays center via raise()
    mStripOverlay = new QWidget(mRoot);
    mStripOverlay->setAutoFillBackground(true);
    QPalette pal = mStripOverlay->palette();
    pal.setColor(QPalette::Window, QColor(20,20,20,230));
    mStripOverlay->setPalette(pal);

    mStripLayout = new QVBoxLayout(mStripOverlay);
    mStripLayout->setContentsMargins(8,8,8,8);
    mStripLayout->setSpacing(6);

    // (Removed instruction label - strip should be closed/opened via checkbox)

    mThumbList = new QListWidget(mStripOverlay);
    mThumbList->setViewMode(QListView::IconMode);
    mThumbList->setResizeMode(QListView::Adjust);
    mThumbList->setMovement(QListView::Static);
    mThumbList->setFlow(QListView::LeftToRight);
    mThumbList->setIconSize(QSize(96,96));
    mThumbList->setSpacing(8);
    mThumbList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(mThumbList, &QListWidget::itemSelectionChanged, this, [this]{
        // no-op; selection used by delete
    });

    // Change to single-click selection: use itemPressed to respond immediately on click
    connect(mThumbList, &QListWidget::itemPressed, this, [this](QListWidgetItem* item){
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < mImages.size()) {
            setCurrentIndex(idx);
            // Dock strip is no longer controlled by toggle; no-op here
        }
    });

    mStripLayout->addWidget(mThumbList, 1);

    mStripButtons = new QWidget(mStripOverlay);
    mStripButtonsLayout = new QHBoxLayout(mStripButtons);
    mStripButtonsLayout->setContentsMargins(0,0,0,0);

    mTrashButton = new CosmicButton(mStripButtons);
    mTrashButton->setBordered(false);
    mTrashButton->setToolTip(tr("Delete selected"));
    mTrashButton->setIcon(QIcon(":/icons/themes/playful/timeline/frame-remove.svg"));
    mTrashButton->setIconSize(QSize(18, 18));
    mTrashButton->setFixedSize(28, 28);
    connect(mTrashButton, &QPushButton::clicked, this, &ReferenceWindow::deleteSelectedFromStrip);

    mClearAllButton = new CosmicButton(mStripButtons);
    mClearAllButton->setText(tr("Clear all"));
    connect(mClearAllButton, &QPushButton::clicked, this, &ReferenceWindow::clearAllImages);

    mStripButtonsLayout->addStretch(1);
    mStripButtonsLayout->addWidget(mTrashButton);
    mStripButtonsLayout->addWidget(mClearAllButton);

    mStripLayout->addWidget(mStripButtons, 0);

    // Initially hidden
    mStripOverlay->hide();
    mStripOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mRootLayout->addWidget(mStripOverlay, 0);
}

void ReferenceWindow::repositionControlsOverlay()
{
    const QSize parentSize = mCenterStack ? mCenterStack->size() : QSize();

    if (mControlsOverlay) {
        mControlsOverlay->adjustSize();
        const QSize s = mControlsOverlay->sizeHint();
        int x = (parentSize.width() - s.width()) / 2;
        int y = parentSize.height() - s.height() - 14;
        mControlsOverlay->setGeometry(x, y, s.width(), s.height());
        mControlsOverlay->raise();
    }

    if (mTopControlsOverlay) {
        mTopControlsOverlay->adjustSize();
        const QSize s = mTopControlsOverlay->sizeHint();
        int x = (parentSize.width() - s.width()) / 2;
        mTopControlsOverlay->setGeometry(x, 14, s.width(), s.height());
        mTopControlsOverlay->raise();
    }
}

void ReferenceWindow::resizeEvent(QResizeEvent* event)
{
    BaseDockWidget::resizeEvent(event);
    repositionControlsOverlay();
}

void ReferenceWindow::changeEvent(QEvent* event)
{
    BaseDockWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange) return;

    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    // Dock/window title
    setTitle(safetr("Reference Window"));
    setWindowTitle(safetr("Reference Window"));

    // Button tooltips
    if (mZoomInButton)    mZoomInButton->setToolTip(safetr("Zoom in"));
    if (mResetZoomButton) mResetZoomButton->setToolTip(safetr("Fit to view"));
    if (mZoomOutButton)   mZoomOutButton->setToolTip(safetr("Zoom out"));
    if (mPrevButton)      mPrevButton->setToolTip(safetr("Previous image"));
    if (mNextButton)      mNextButton->setToolTip(safetr("Next image"));
    if (mGetCanvasButton) mGetCanvasButton->setToolTip(safetr("Get canvas image"));
    if (mGalleryToggle)   mGalleryToggle->setToolTip(safetr("Show Gallery"));
    if (mFileButton)      mFileButton->setToolTip(safetr("Import image files"));
    if (mTrashButton)     mTrashButton->setToolTip(safetr("Delete selected"));
    if (mClearAllButton)  mClearAllButton->setText(safetr("Clear all"));

    // Rebuild thumbnails so "Canvas" label is retranslated
    rebuildThumbnails();
}

void ReferenceWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (mimeHasImportableUrls(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ReferenceWindow::dragMoveEvent(QDragMoveEvent* event)
{
    if (mimeHasImportableUrls(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ReferenceWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mimeHasImportableUrls(mime)) {
        event->ignore();
        return;
    }

    importFromMimeData(mime);
    emitGalleryUpdate();
    event->acceptProposedAction();
}

void ReferenceWindow::applyZoom(double scale)
{
    if (qFuzzyCompare(scale, mZoomScale)) return;
    mZoomScale = scale;

    // If using the new ReferenceCanvas, delegate zoom there to avoid QGraphicsView transforms
    if (mCanvas) {
        mCanvas->setZoomScale(mZoomScale);
        return;
    }

    // Fallback: old QGraphicsView behaviour
    if (mView && mView->viewport()) {
        QRect vpRect = mView->viewport()->rect();
        QPoint vpCenter = vpRect.center();
        QPointF sceneCenter = mView->mapToScene(vpCenter);

        mView->resetTransform();
        mView->scale(mZoomScale, mZoomScale);

        // Re-center the view on the same scene point
        mView->centerOn(sceneCenter);
        return;
    }

    if (mView) {
        mView->resetTransform();
        mView->scale(mZoomScale, mZoomScale);
    }
}

void ReferenceWindow::refreshPixmap()
{
    if (mCurrentIndex < 0 || mCurrentIndex >= mImages.size()) return;
    const RefImage& ref = mImages[mCurrentIndex];
    
    // Use the safer currentCanvas() method instead of manual widget finding
    ReferenceCanvas* canvas = currentCanvas();
    if (!canvas) return;
    
    canvas->setImage(ref.image);
    // Fit image to view to ensure proper display
    canvas->fitToView();
    // Update zoom scale and slider to match the fit-to-view scale
    double scale = canvas->zoomScale();
    if (!(scale > 0)) scale = 1.0;
    mZoomScale = scale;

    // Defer a second fitToView so it runs after the layout has fully settled
    // (the viewport size may not be final when called synchronously after a delete).
    QPointer<ReferenceWindow> self(this);
    QTimer::singleShot(0, this, [self]() {
        if (!self) return;
        ReferenceCanvas* c = self->currentCanvas();
        if (!c) return;
        c->fitToView();
        double s = c->zoomScale();
        if (!(s > 0)) s = 1.0;
        self->mZoomScale = s;
    });
}

void ReferenceWindow::openFiles()
{
    QStringList files = FileDialog::getOpenFileNames(this, FileType::IMAGE, tr("Import reference images"));
    for (const QString& path : files) {
        addOrSelectImageFromPath(QFileInfo(path).absoluteFilePath(), mActiveGalleryCategory);
    }
    emitGalleryUpdate();
}

bool ReferenceWindow::mimeHasImportableUrls(const QMimeData* mime) const
{
    if (!mime || !mime->hasUrls()) return false;

    const QList<QUrl> urls = mime->urls();
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;

        QFileInfo info(path);
        if (info.isDir()) {
            QDir dir(path);
            const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable | QDir::NoDotAndDotDot);
            for (const QFileInfo& fi : entries) {
                QImageReader reader(fi.absoluteFilePath());
                if (reader.canRead()) {
                    return true;
                }
            }
        } else if (info.isFile()) {
            QImageReader reader(info.absoluteFilePath());
            if (reader.canRead()) {
                return true;
            }
        }
    }
    return false;
}

void ReferenceWindow::importFromMimeData(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls()) return;

    const QList<QUrl> urls = mime->urls();
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) continue;

        QFileInfo info(path);
        if (info.isDir()) {
            // Use folder name as category
            const QString category = info.fileName();
            QDir dir(path);
            const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable | QDir::NoDotAndDotDot);
            for (const QFileInfo& fi : entries) {
                const QString filePath = fi.absoluteFilePath();
                QImageReader reader(filePath);
                if (!reader.canRead()) continue;
                addOrSelectImageFromPath(filePath, category);
            }
        } else if (info.isFile()) {
            const QString filePath = info.absoluteFilePath();
            QImageReader reader(filePath);
            if (!reader.canRead()) continue;
            addOrSelectImageFromPath(filePath, mActiveGalleryCategory);
        }
    }
}

void ReferenceWindow::addOrSelectImageFromPath(const QString& path, const QString& category)
{
    if (path.isEmpty()) return;
    if (mImportedPaths.contains(path)) {
        for (int i = 0; i < mImages.size(); ++i) {
            if (mImages[i].key == path) { setCurrentIndex(i); break; }
        }
        return;
    }

    QImageReader reader(path);
    QImage img;
    if (!reader.read(&img)) return;

    RefImage ri;
    ri.key = path;
    ri.image = img;
    ri.isCanvas = false;
    ri.category = category.isEmpty() ? QStringLiteral("General") : category;

    mImages.push_back(ri);
    mImportedPaths.insert(path);
    setCurrentIndex(mImages.size() - 1);
    rebuildThumbnails();
    emitGalleryUpdate();
}

void ReferenceWindow::setCurrentIndex(int index)
{
    if (index < 0 || index >= mImages.size()) return;
    mCurrentIndex = index;
    refreshPixmap();
    emitGalleryUpdate();
}

void ReferenceWindow::getCanvasSnapshot()
{
    QImage img = renderCurrentFrameComposite();
    if (img.isNull()) return;
    replaceOrCreateCanvasImage(img);
    rebuildThumbnails();
}

QImage ReferenceWindow::renderCurrentFrameComposite() const
{
    if (!editor() || !editor()->object()) return QImage();

    // Use camera view for size and transform similar to exporters
    LayerCamera* cameraLayer = editor()->layers()->getCameraLayerBelow(editor()->currentLayerIndex());
    if (!cameraLayer) return QImage();

    QSize camSize = cameraLayer->getViewSize();
    QImage image(camSize, QImage::Format_ARGB32_Premultiplied);
    // Fill background white (no transparency)
    image.fill(Qt::white);

    QPainter painter(&image);
    QTransform view = cameraLayer->getViewAtFrame(editor()->currentFrame());
    QTransform centralize; centralize.translate(camSize.width()/2.0, camSize.height()/2.0);
    painter.setWorldTransform(view * centralize);
    painter.setWindow(QRect(0, 0, camSize.width(), camSize.height()));

    // Draw ONLY the current layer and ONLY the exact current frame
    Layer* layer = editor()->layers()->currentLayer();
    if (layer) {
        if (layer->type() == Layer::BITMAP) {
            auto* bmpLayer = static_cast<LayerBitmap*>(layer);
            if (BitmapImage* bitmap = bmpLayer->getBitmapImageAtFrame(editor()->currentFrame())) {
                painter.setOpacity(bitmap->getOpacity());
                bitmap->paintImage(painter);
            }
        } else if (layer->type() == Layer::VECTOR) {
            auto* vecLayer = static_cast<LayerVector*>(layer);
            if (VectorImage* vimg = vecLayer->getVectorImageAtFrame(editor()->currentFrame())) {
                painter.setOpacity(vimg->getOpacity());
                vimg->paintImage(painter, *editor()->object(), false, false, true);
            }
        }
    }

    painter.end();

    return image;
}

void ReferenceWindow::replaceOrCreateCanvasImage(const QImage& img)
{
    // Keep only a single canvas snapshot; replace if exists
    if (mCanvasIndex >= 0 && mCanvasIndex < mImages.size()) {
        mImages[mCanvasIndex].image = img;
        setCurrentIndex(mCanvasIndex);
        emitGalleryUpdate();
        return;
    }

    RefImage ri;
    ri.key = QStringLiteral("canvas");
    ri.image = img;
    ri.isCanvas = true;

    mImages.push_back(ri);
    mCanvasIndex = mImages.size() - 1;
    setCurrentIndex(mCanvasIndex);
    emitGalleryUpdate();
}

void ReferenceWindow::showStrip(bool show)
{
    // When strip shown: only checkbox should be interactable. Make overlay consume events.
    if (show) {
        mStripOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        mStripOverlay->raise();
        mStripOverlay->show();
        rebuildThumbnails();
    } else {
        mStripOverlay->hide();
        mStripOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
}

void ReferenceWindow::rebuildThumbnails()
{
    if (!mStripOverlay || !mStripOverlay->isVisible()) return;
    if (!mThumbList) return;
    
    mThumbList->clear();
    for (int i = 0; i < mImages.size(); ++i) {
        const RefImage& ri = mImages[i];
        QImage thumb = ri.image.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QIcon icon(QPixmap::fromImage(thumb));

        auto* item = new QListWidgetItem(icon, ri.isCanvas ? tr("Canvas") : QFileInfo(ri.key).fileName());
        item->setData(Qt::UserRole, i);
        mThumbList->addItem(item);
        if (i == mCurrentIndex) item->setSelected(true);
    }
}

void ReferenceWindow::deleteSelectedFromStrip()
{
    // Safety check: ensure we have a valid thumb list
    if (!mThumbList) return;
    
    QList<int> selectedIdx;
    for (QListWidgetItem* it : mThumbList->selectedItems()) {
        selectedIdx.push_back(it->data(Qt::UserRole).toInt());
    }
    std::sort(selectedIdx.begin(), selectedIdx.end());
    selectedIdx.erase(std::unique(selectedIdx.begin(), selectedIdx.end()), selectedIdx.end());
    std::reverse(selectedIdx.begin(), selectedIdx.end());

    int newCurrent = mCurrentIndex;
    for (int idx : selectedIdx) {
        if (idx < 0 || idx >= mImages.size()) continue;
        if (!mImages[idx].isCanvas) {
            mImportedPaths.remove(mImages[idx].key);
        } else {
            if (mCanvasIndex == idx) mCanvasIndex = -1;
        }
        mImages.removeAt(idx);
        if (newCurrent == idx) newCurrent = -1;
        else if (newCurrent > idx) newCurrent -= 1;
    }

    if (mImages.isEmpty()) {
        mCurrentIndex = -1;
        mCanvasIndex = -1;
        // Clear the canvas instead of the unused mPixmapItem
        ReferenceCanvas* canvas = currentCanvas();
        if (canvas) {
            canvas->setImage(QImage());
        }
    } else {
        if (newCurrent < 0) newCurrent = 0;
        if (newCurrent >= mImages.size()) newCurrent = mImages.size() - 1;
        setCurrentIndex(newCurrent);
    }

    rebuildThumbnails();
}

void ReferenceWindow::clearAllImages()
{
    mImages.clear();
    mImportedPaths.clear();
    mCanvasIndex = -1;
    mCurrentIndex = -1;
    
    ReferenceCanvas* canvas = currentCanvas();
    if (canvas) canvas->setImage(QImage());
    
    rebuildThumbnails();
    emitGalleryUpdate();
}

void ReferenceWindow::deleteCategorySlot(const QString& category)
{
    for (int i = mImages.size() - 1; i >= 0; --i) {
        if (mImages[i].category == category) {
            if (!mImages[i].isCanvas)
                mImportedPaths.remove(mImages[i].key);
            else
                mCanvasIndex = -1;
            mImages.removeAt(i);
            if (mCurrentIndex >= mImages.size())
                mCurrentIndex = mImages.size() - 1;
        }
    }
    // Refresh the displayed image — the current one may have been deleted
    if (mImages.isEmpty()) {
        mCurrentIndex = -1;
        if (ReferenceCanvas* canvas = currentCanvas())
            canvas->setImage(QImage());
    } else {
        if (mCurrentIndex < 0) mCurrentIndex = 0;
        refreshPixmap();
    }
    rebuildThumbnails();
    emitGalleryUpdate();
}

void ReferenceWindow::setActiveCategorySlot(const QString& category)
{
    mActiveGalleryCategory = category.isEmpty() ? QStringLiteral("General") : category;
}

void ReferenceWindow::importFromMimeDataSlot(const QMimeData* mime)
{
    if (!mime) return;
    importFromMimeData(mime);
    emitGalleryUpdate();
}

bool ReferenceWindow::isActiveTarget() const
{
    // Consider the canvas (inside scroll) as active target as well
    if (mView && mView->hasFocus()) return true;
    ReferenceCanvas* c = const_cast<ReferenceWindow*>(this)->currentCanvas();
    if (c && c->hasFocus()) return true;
    return false;
}

void ReferenceWindow::setZoomPercent(int percent)
{
    percent = qBound(1, percent, 400);
    applyZoom(percent / 100.0);
}

void ReferenceWindow::onZoomInAction()
{
    if (!isActiveTarget()) return;
    setZoomPercent(static_cast<int>(mZoomScale * 100) + 10);
}

void ReferenceWindow::onZoomOutAction()
{
    if (!isActiveTarget()) return;
    setZoomPercent(static_cast<int>(mZoomScale * 100) - 10);
}

void ReferenceWindow::onZoomPreset(int percent)
{
    if (!isActiveTarget()) return;
    setZoomPercent(percent);
}

void ReferenceWindow::onScaleBy(double factor)
{
    if (!isActiveTarget()) return;
    int current = static_cast<int>(mZoomScale * 100);
    setZoomPercent(static_cast<int>(current * factor));
}

void ReferenceWindow::selectGalleryIndex(int index)
{
    if (index < 0 || index >= mImages.size()) return;
    setCurrentIndex(index);
}

void ReferenceWindow::emitGalleryUpdate()
{
    QVector<QImage> imgs;
    imgs.reserve(mImages.size());
    QStringList labels;
    labels.reserve(mImages.size());
    QStringList perImageCategories;
    perImageCategories.reserve(mImages.size());

    for (const RefImage& ri : mImages) {
        imgs.push_back(ri.image);
        labels.push_back(ri.isCanvas ? tr("Canvas") : QFileInfo(ri.key).fileName());
        perImageCategories.push_back(ri.category);
    }
    emit galleryContentUpdated(imgs, labels, mCurrentIndex, perImageCategories);
}

void ReferenceWindow::deleteImagesByIndices(const QList<int>& indices)
{
    if (indices.isEmpty()) return;
    QList<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    std::reverse(sorted.begin(), sorted.end());

    int newCurrent = mCurrentIndex;
    for (int idx : sorted) {
        if (idx < 0 || idx >= mImages.size()) continue;
        if (!mImages[idx].isCanvas) {
            mImportedPaths.remove(mImages[idx].key);
        } else {
            if (mCanvasIndex == idx) mCanvasIndex = -1;
        }
        mImages.removeAt(idx);
        if (newCurrent == idx) newCurrent = -1;
        else if (newCurrent > idx) newCurrent -= 1;
    }

    if (mImages.isEmpty()) {
        mCurrentIndex = -1;
        mCanvasIndex = -1;
        if (ReferenceCanvas* canvas = currentCanvas()) {
            canvas->setImage(QImage());
        }
    } else {
        if (newCurrent < 0) newCurrent = 0;
        if (newCurrent >= mImages.size()) newCurrent = mImages.size() - 1;
        setCurrentIndex(newCurrent);
    }

    rebuildThumbnails();
    emitGalleryUpdate();
}
