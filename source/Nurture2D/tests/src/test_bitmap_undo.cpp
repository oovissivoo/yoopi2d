#include "catch.hpp"
#include "editor.h"
#include "undoredomanager.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "object.h"
#include "layermanager.h"
#include <QUndoStack>
#include <QDebug>

TEST_CASE("Bitmap Layer Undo Redo Fix", "[undo][bitmap]")
{
    // Setup
    Editor* editor = new Editor();
    editor->init();
    
    // Create Object
    Object* obj = new Object();
    obj->init(); 
    obj->addNewBitmapLayer(); 
    obj->addNewVectorLayer(); // Add dummy layer to support index 1 if selected
    
    Layer* bitmapLayer = nullptr;
    int count = obj->getLayerCount();
    int bitmapIndex = -1;
    for (int i = 0; i < count; ++i) {
        Layer* l = obj->getLayer(i);
        // qDebug() << "Layer " << i << " type: " << l->type();
        if (l->type() == Layer::BITMAP) {
            bitmapLayer = l;
            bitmapIndex = i;
            obj->data()->setCurrentLayer(i);
            break;
        }
    }
    REQUIRE(count > 0);
    REQUIRE(bitmapLayer != nullptr);
    REQUIRE(bitmapIndex != -1);
    qDebug() << "Setting Object";
    editor->setObject(obj);
    
    // Ensure we are on the bitmap layer
    qDebug() << "Setting Current Layer Index: " << bitmapIndex;
    editor->setCurrentLayerIndex(bitmapIndex);
    
    LayerManager* layerMgr = editor->layers();
    
    // Find the Bitmap Layer (should be index 2 or type BITMAP)
    // Object init: Camera, Vector, Bitmap.
    LayerBitmap* layer = nullptr;
    int layerIndex = -1;
    for(int i=0; i<layerMgr->count(); ++i) {
        if(layerMgr->getLayer(i)->type() == Layer::BITMAP) {
            layer = static_cast<LayerBitmap*>(layerMgr->getLayer(i));
            layerIndex = i;
            break;
        }
    }
    REQUIRE(layer != nullptr);
    REQUIRE(layerIndex != -1);
    
    // Ensure we are active on this layer
    qDebug() << "Setting Layer Active again";
    editor->setCurrentLayerIndex(layerIndex);
    
    qDebug() << "Scrubbing to 1";
    editor->scrubTo(1);
    REQUIRE(editor->currentFrame() == 1);
    
    // Add KeyFrame at current frame (1)
    qDebug() << "Adding New Key";
    editor->addNewKey(); 
    qDebug() << "Key Added";
    
    // Force to frame 1 if it moved (ensures synchronization between test assumption and editor state)
    if (editor->currentFrame() != 1) {
        editor->scrubTo(1);
    }
    
    int cf = editor->currentFrame();
    // getBitmapImageAtFrame returns the composite; use the active inner layer for write target
    KeyFrame* kf2 = layer->getKeyFrameAt(cf);
    if (!kf2) kf2 = layer->getLastKeyFrameAtPosition(cf);
    REQUIRE(kf2 != nullptr);
    auto* bkf2 = dynamic_cast<BitmapKeyFrame*>(kf2);
    REQUIRE(bkf2 != nullptr);
    BitmapImage* img = layer->cellBank()->getActiveInnerLayerImage(bkf2->cellId());
    REQUIRE(img != nullptr);
    
    // Set Active Keyframe for Undo Context - REMOVED (Global Stack)
    // editor->undoRedo()->setActiveKeyFrame(layer->getKeyFrameAt(cf));
    
    // Setup initial state: Transparent
    img->clear();
    REQUIRE(qAlpha(img->pixel(0, 0)) == 0);
    
    // ------------------------------------------------
    // ACTION 1: Draw a Red Pixel
    // ------------------------------------------------
    
    // 1. Capture State BEFORE modification
    const UndoSaveState* beforeRed = editor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    
    // 2. Modify
    img->setPixel(0, 0, qRgba(255, 0, 0, 255));
    REQUIRE(qRed(img->pixel(0, 0)) == 255);
    
    // 3. Record Undo
    editor->undoRedo()->record(beforeRed, "Draw Red");
    
    // ------------------------------------------------
    // ACTION 2: Draw a Blue Pixel
    // ------------------------------------------------
    
    // 1. Capture State BEFORE modification
    const UndoSaveState* beforeBlue = editor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    
    // 2. Modify
    img->setPixel(0, 0, qRgba(0, 0, 255, 255));
    REQUIRE(qBlue(img->pixel(0, 0)) == 255);
    
    // 3. Record Undo
    editor->undoRedo()->record(beforeBlue, "Draw Blue");
    
    // ------------------------------------------------
    // VERIFY UNDO
    // ------------------------------------------------
    
    // Undo Action 2 (Blue -> Red)
    editor->undoRedo()->activeStack()->undo();
    
    // Check pixel is RED
    // This requires that UndoRedoManager successfully restored the PREVIOUS image state
    // which was Red.
    REQUIRE(qRed(img->pixel(0, 0)) == 255);
    REQUIRE(qBlue(img->pixel(0, 0)) == 0);
    
    // Undo Action 1 (Red -> Transparent)
    editor->undoRedo()->activeStack()->undo();
    
    // Check pixel is Transparent
    REQUIRE(qAlpha(img->pixel(0, 0)) == 0);
    
    // ------------------------------------------------
    // VERIFY REDO
    // ------------------------------------------------
    
    // Redo Action 1 (Transparent -> Red)
    editor->undoRedo()->activeStack()->redo();
    REQUIRE(qRed(img->pixel(0, 0)) == 255);
    
    // Redo Action 2 (Red -> Blue)
    editor->undoRedo()->activeStack()->redo();
    REQUIRE(qBlue(img->pixel(0, 0)) == 255);

    delete editor;
    // Object likely deleted by Editor or should be managed differently if shared. 
    // Given the crash, assume Editor took ownership or double delete occurred.
}
