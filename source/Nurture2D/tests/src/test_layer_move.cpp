#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "structure/layerbitmap.h"
#include "structure/bitmapkeyframe.h"
#include "structure/cellbank.h"

TEST_CASE("Layer Keyframe Move and Lookup Logic", "[layer]") {
    LayerBitmap layer(1);
    
    // Setup: Keyframe at 5
    layer.addNewKeyFrameAt(5);
    
    REQUIRE(layer.keyExists(5) == true);
    REQUIRE(layer.getLastKeyFrameAtPosition(5) != nullptr);
    REQUIRE(layer.getLastKeyFrameAtPosition(5)->pos() == 5);
    
    // Verify lookup before 5 returns nothing
    REQUIRE(layer.getLastKeyFrameAtPosition(4) == nullptr);
    
    // Verify lookup after 5 returns 5 (hold)
    REQUIRE(layer.getLastKeyFrameAtPosition(6) != nullptr);
    REQUIRE(layer.getLastKeyFrameAtPosition(6)->pos() == 5);
    
    SECTION("Move Keyframe 5 -> 8") {
        bool success = layer.moveKeyFrame(5, 3); // offset 3
        REQUIRE(success == true);
        
        // Check old position is empty
        REQUIRE(layer.keyExists(5) == false);
        
        // Check new position has key
        REQUIRE(layer.keyExists(8) == true);
        REQUIRE(layer.getKeyFrameAt(8) != nullptr);
        
        // CRITICAL: Check lookup at old position (5)
        // Should NOT find keyframe at 8. Should find nothing (or previous if it existed).
        KeyFrame* kfAt5 = layer.getLastKeyFrameAtPosition(5);
        if (kfAt5) {
            INFO("Found keyframe at: " << kfAt5->pos());
            REQUIRE(kfAt5->pos() != 8); 
            REQUIRE(kfAt5->pos() != 5); // Should definitely not be 5
        } else {
            SUCCEED("Correctly found no keyframe at 5");
        }
        
        // Check lookup at 8
        KeyFrame* kfAt8 = layer.getLastKeyFrameAtPosition(8);
        REQUIRE(kfAt8 != nullptr);
        REQUIRE(kfAt8->pos() == 8);
        
        // Check lookup at 9 (hold of 8)
        KeyFrame* kfAt9 = layer.getLastKeyFrameAtPosition(9);
        REQUIRE(kfAt9 != nullptr);
        REQUIRE(kfAt9->pos() == 8);
    }
    
    SECTION("Move Keyframe with Previous Keyframe (2, 5 -> 2, 8)") {
        layer.addNewKeyFrameAt(2);
        
        bool success = layer.moveKeyFrame(5, 3); // Move 5 -> 8
        REQUIRE(success == true);
        
        // At 5, should now find keyframe 2 (hold)
        KeyFrame* kfAt5 = layer.getLastKeyFrameAtPosition(5);
        REQUIRE(kfAt5 != nullptr);
        REQUIRE(kfAt5->pos() == 2); // Should NOT be 8!
    }
}
