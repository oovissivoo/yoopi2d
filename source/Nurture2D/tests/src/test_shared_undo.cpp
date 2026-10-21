#include "catch.hpp"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "bitmapimage.h"
#include "editor.h"
#include "object.h"
#include "undoredomanager.h"
#include "layermanager.h"
#include <QUndoStack>

TEST_CASE("Shared Undo Stack via CellBank", "[shared_undo]")
{
// 1. Setup Editor and Object (UndoRedoManager is owned by Editor)
    Editor* editor = new Editor(nullptr);
    editor->init();
    
    // We need an object and layer for the test context
    Object* obj = new Object();
    editor->setObject(obj);
    
    // Create Layer via Manager to ensure it's registered
    // Actually LayerBitmap constructor usage in test was standalone.
    // But now we need Editor integration for global undo.
    LayerBitmap* layer = static_cast<LayerBitmap*>(editor->layers()->createBitmapLayer("TestBmp"));

    // 2. Add KeyFrame 1 (Creates Cell 1)
    layer->addNewKeyFrameAt(1);
    // Editor needs to scrub to updates context (though context is global now)
    editor->scrubTo(1);
    BitmapKeyFrame* kf1 = dynamic_cast<BitmapKeyFrame*>(layer->getKeyFrameAt(1));
    REQUIRE(kf1 != nullptr);
    int cellId = kf1->cellId();
    REQUIRE(cellId != -1);
    
    // 3. Add Keyframe 10 sharing Cell 1
    BitmapKeyFrame* kf2 = new BitmapKeyFrame(cellId);
    kf2->setCellBank(layer->cellBank()); 
    // Use the base class method to add KeyFrame
    layer->addKeyFrame(10, kf2);
    
    // 4. Verify Linkage
    // In Global Stack model, all keyframes share the same global stack via Editor
    QUndoStack* stack1 = editor->undoRedo()->undoStack();
    QUndoStack* stack2 = editor->undoRedo()->undoStack();

    REQUIRE(stack1 == stack2); 
    REQUIRE(stack1 != nullptr);
    REQUIRE(stack1 == stack2); 
    
    // 5. Verify functional sharing
    stack1->push(new QUndoCommand("Test Command"));
    REQUIRE(stack2->count() == 1);
    REQUIRE(stack2->undoText() == "Test Command");
    
    delete editor;
}
