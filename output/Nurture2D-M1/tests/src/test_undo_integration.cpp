#include "catch.hpp"
#include "editor.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "undoredomanager.h"
#include "object.h"
#include "layermanager.h"

// Subclass Editor to access protected members if needed, or just use public API
// We need to simulate the environment

TEST_CASE("UndoRedoManager Context Integration", "[undo_integration]")
{
    // 1. Setup minimal Editor environment
    // Editor needs to be instantiated. It creates its own managers.
    Editor* editor = new Editor(nullptr);
    editor->init();
    
    // Create a new Object (animation) and set it
    Object* obj = new Object();
    editor->setObject(obj);
    
    // 2. Create a Bitmap Layer
    editor->layers()->createBitmapLayer("Test Layer");
    LayerBitmap* layer = static_cast<LayerBitmap*>(editor->layers()->currentLayer());
    REQUIRE(layer != nullptr);
    REQUIRE(layer->type() == Layer::BITMAP);
    
    // 3. Add Keyframe 1 (Creates Cell)
    editor->scrubTo(1);
    editor->addNewKey();
    
    BitmapKeyFrame* kf1 = dynamic_cast<BitmapKeyFrame*>(layer->getKeyFrameAt(1));
    REQUIRE(kf1 != nullptr);
    int cellId = kf1->cellId();
    REQUIRE(cellId != -1);
    
    // Verify initial context
    UndoRedoManager* undoMgr = editor->undoRedo();
    QUndoStack* globalStack = undoMgr->undoStack();
    REQUIRE(globalStack != nullptr); // Always valid global stack
    
    // 4. Simulate CellStackWidget behavior: Add Keyframe 10 sharing Cell 1 manually
    // This bypasses Editor::addNewKey which scrubs
    
    editor->scrubTo(10); // User scrubs to 10
    
    // In Global Stack model, stack should remain the same
    REQUIRE(undoMgr->undoStack() == globalStack);
    
    // NOW, we ADD a new keyframe at 10 manually, like CellStackWidget does.
    BitmapKeyFrame* kf10 = new BitmapKeyFrame(cellId);
    kf10->setCellBank(layer->cellBank());
    layer->addKeyFrame(10, kf10);
    
    // Verify stack is still the global stack
    REQUIRE(undoMgr->undoStack() == globalStack);
    
    // 5. Apply the "Fix" (Simulate what CellStackWidget will do)
    // Call scrubTo(10) again.
    editor->scrubTo(10);
    
    // 6. Verify Context
    // Should still be using global stack
    REQUIRE(undoMgr->undoStack() == globalStack);
    
    // 7. Verify Undo Operation on Shared Data
    // Modifying one keyframe (kf10) should be undoable via the global stack
    // (Actual modification tests are in test_shared_undo.cpp)
    
    delete editor;
}
