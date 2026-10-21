
#include "catch.hpp"
#include "editor.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "object.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "tool/lassoselecttool.h"
#include "managers/selectionmanager.h"
#include "managers/undoredomanager.h"
#include "bitmapimage.h"
#include <QPainter>
#include <QMouseEvent>
#include "util/pointerevent.h"

// Subclass to expose private members
class TestLassoSelectTool : public LassoSelectTool {
public:
    using LassoSelectTool::LassoSelectTool;
    void publicPointerPressEvent(PointerEvent* event) {
        printf("DEBUG: Before pointerPressEvent\n"); fflush(stdout);
        if (!event) { printf("DEBUG: event is null\n"); fflush(stdout); }
        else { printf("DEBUG: event type: %d\n", event->eventType()); fflush(stdout); }
        pointerPressEvent(event);
        printf("DEBUG: After pointerPressEvent\n"); fflush(stdout);
    }
};

TEST_CASE("LassoSelectTool - Paste Persistence Bug")
{
    printf("DEBUG: Starting Test Case\n"); fflush(stdout);
    // Setup Editor and components
    Editor* editor = new Editor;
    printf("DEBUG: Editor created\n"); fflush(stdout);
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    printf("DEBUG: ScribbleArea created\n"); fflush(stdout);
    scribbleArea->setEditor(editor);
    editor->setScribbleArea(scribbleArea);
    // Initialize Object
    Object* obj = new Object();
    editor->setObject(obj);
    editor->init();
    printf("DEBUG: Editor initialized\n"); fflush(stdout);
    
    // Create a Bitmap Layer
    editor->layers()->createBitmapLayer("TestLayer");
    LayerBitmap* layer = static_cast<LayerBitmap*>(editor->layers()->currentLayer());
    REQUIRE(layer != nullptr);
    
    // Create a keyframe at frame 1
    editor->addNewKey();
    REQUIRE(layer->getKeyFrameAt(1) != nullptr);

    // Create a dummy image to "paste" (Red 10x10 square)
    QImage pasteImage(10, 10, QImage::Format_ARGB32_Premultiplied);
    pasteImage.fill(Qt::red);
    QPointF pastePos(20, 20);

    // Simulate Paste Action: Editor::paste would call beginFloatingFromImage
    printf("DEBUG: simulating paste\n"); fflush(stdout);
    editor->select()->beginFloatingFromImage(pasteImage, pastePos);
    printf("DEBUG: paste simulated\n"); fflush(stdout);

    REQUIRE(editor->select()->hasFloating() == true);
    REQUIRE(editor->select()->isFloatingFromPaste() == true);

    // Verify the underlying bitmap is empty at pastePos (since it's floating)
    BitmapImage* keyframe = layer->getLastBitmapImageAtFrame(editor->currentFrame());
    REQUIRE(keyframe != nullptr);
    QRgb pixelBefore = keyframe->constScanLine(20, 20);
    REQUIRE(pixelBefore == 0); // Assuming transparent initialization

    // Initialize Lasso Tool
    // ...
    TestLassoSelectTool* lasso = new TestLassoSelectTool(nullptr);
    lasso->initialize(editor);

    // Simulate clicking to start a new selection with Lasso (Ctrl+Click to force new selection)
    // This should commit the floating selection
    QMouseEvent qEvent(QEvent::MouseButtonPress, QPointF(50, 50), Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    PointerEvent fakeEvent(&qEvent, QPointF(50, 50));
    
    // Call pointerPressEvent
    lasso->publicPointerPressEvent(&fakeEvent);

    // Check if floating is gone
    REQUIRE(editor->select()->hasFloating() == false);

    // CRITICAL CHECK: Did the paste persist?
    // Check center pixel (25,25) which we verified in debug prints
    // Use pixel() method which handles bounds checking and coordinate translation
    // Note: We re-fetch the keyframe to be safe, though pointer shouldn't change
    keyframe = layer->getLastBitmapImageAtFrame(editor->currentFrame());
    QRgb pixelAfter = keyframe->pixel(25, 25);
    
    // Qt::red is #FFFF0000. QImage::Format_ARGB32_Premultiplied.
    // 255 alpha, 255 red.
    
    REQUIRE(qAlpha(pixelAfter) == 255);
    REQUIRE(qRed(pixelAfter) == 255);
    
    // Cleanup
    delete lasso;
    delete editor;
    delete scribbleArea;
}
