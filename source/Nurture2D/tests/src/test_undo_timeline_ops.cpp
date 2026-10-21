#include "catch.hpp"
#include "editor.h"
// #include "actioncommands.h" // Removed dependency
#include "object.h" // Added dependency
#include "undoredomanager.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "layermanager.h"

// Test to verify if Timeline Operations are Undoable
TEST_CASE("Timeline Operations Undo Coverage", "[undo_timeline]")
{
    Editor* editor = new Editor(nullptr);
    editor->init();
    Object* obj = new Object(); // Use default constructor
    editor->setObject(obj);
    editor->layers()->createBitmapLayer("Test Layer");
    
    SECTION("Add Keyframe Undo")
    {
        editor->scrubTo(1);
        int initialCount = editor->undoRedo()->undoStack()->count();
        
        // 1. Add Keyframe directly via Editor (which ActionCommands uses)
        editor->addNewKey();
        
        // 2. Check Undo Stack
        int afterAddCount = editor->undoRedo()->undoStack()->count();
        
        // If it's 0 (or same as initial), then NO UNDO was recorded.
        // If it's +1, then it is undoable.
        
        // Expectation: It FAILS (count is same) based on code reading.
        bool undoRecorded = (afterAddCount > initialCount);
        REQUIRE(undoRecorded == false); // Asserting the BUG
    }
    
    delete editor;
}
