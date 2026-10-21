#include "catch.hpp"
#include <memory>
#include "cellbank.h"
#include "bitmapimage.h"
#include "celllayerstack.h"
#include "innerlayer.h"

// Requirements: 3.1, 4.1

// Helper: create a CellBank with one cell and return the cell id
static int makeCell(CellBank& bank)
{
    BitmapImage* img = new BitmapImage();
    return bank.addCell(img, "TestCell");
}

// ── 1. getCell() returns the composite, not the raw layer image ───────────────

TEST_CASE("CellBank::getCell returns composite image", "[cellbank][innerlayers]")
{
    CellBank bank;
    int id = makeCell(bank);

    BitmapImage* composite = bank.getCell(id);
    BitmapImage* rawLayer  = bank.getActiveInnerLayerImage(id);

    SECTION("getCell returns non-null") {
        REQUIRE(composite != nullptr);
    }

    SECTION("With one layer, composite pointer differs from raw layer pointer") {
        // composite() is a separately owned BitmapImage built from the layer(s)
        REQUIRE(composite != rawLayer);
    }
}

// ── 2. getActiveInnerLayerImage() returns the active layer's image ────────────

TEST_CASE("CellBank::getActiveInnerLayerImage returns active layer image", "[cellbank][innerlayers]")
{
    CellBank bank;
    int id = makeCell(bank);

    BitmapImage* layerImg = bank.getActiveInnerLayerImage(id);

    SECTION("Returns non-null for a valid cell") {
        REQUIRE(layerImg != nullptr);
    }

    SECTION("Matches the image stored in the active InnerLayer") {
        CellLayerStack* stack = bank.getCellLayerStack(id);
        REQUIRE(stack != nullptr);
        REQUIRE(stack->activeLayer() != nullptr);
        REQUIRE(layerImg == stack->activeLayer()->image);
    }

    SECTION("Does not equal the composite returned by getCell") {
        REQUIRE(layerImg != bank.getCell(id));
    }
}

// ── 3. After addInnerLayer(), undo stack count matches new layer count ─────────

TEST_CASE("CellBank::addInnerLayer undo stack count matches layer count", "[cellbank][innerlayers]")
{
    CellBank bank;
    int id = makeCell(bank);

    CellLayerStack* stack = bank.getCellLayerStack(id);
    REQUIRE(stack != nullptr);

    // Start: 1 layer → 1 undo stack
    REQUIRE(stack->layerCount() == 1);
    REQUIRE(bank.getInnerLayerUndoStack(id, 0) != nullptr);

    // Add a second layer
    bank.addInnerLayer(id, "Layer 2");
    REQUIRE(stack->layerCount() == 2);

    SECTION("Each layer has its own undo stack after first add") {
        REQUIRE(bank.getInnerLayerUndoStack(id, 0) != nullptr);
        REQUIRE(bank.getInnerLayerUndoStack(id, 1) != nullptr);
    }

    // Add a third layer
    bank.addInnerLayer(id, "Layer 3");
    REQUIRE(stack->layerCount() == 3);

    SECTION("Each layer has its own undo stack after second add") {
        REQUIRE(bank.getInnerLayerUndoStack(id, 0) != nullptr);
        REQUIRE(bank.getInnerLayerUndoStack(id, 1) != nullptr);
        REQUIRE(bank.getInnerLayerUndoStack(id, 2) != nullptr);
    }
}

// ── 4. After removeInnerLayer(), undo stack count matches new layer count ──────

TEST_CASE("CellBank::removeInnerLayer undo stack count matches layer count", "[cellbank][innerlayers]")
{
    CellBank bank;
    int id = makeCell(bank);

    bank.addInnerLayer(id, "Layer 2");
    bank.addInnerLayer(id, "Layer 3");

    CellLayerStack* stack = bank.getCellLayerStack(id);
    REQUIRE(stack->layerCount() == 3);

    // Remove layer at index 2
    bank.removeInnerLayer(id, 2);
    REQUIRE(stack->layerCount() == 2);

    SECTION("Remaining layers still have undo stacks") {
        REQUIRE(bank.getInnerLayerUndoStack(id, 0) != nullptr);
        REQUIRE(bank.getInnerLayerUndoStack(id, 1) != nullptr);
    }

    SECTION("Removed layer index no longer has a stack") {
        REQUIRE(bank.getInnerLayerUndoStack(id, 2) == nullptr);
    }
}

// ── 5. Null cell guard: getActiveInnerLayerImage(-999) returns nullptr ─────────

TEST_CASE("CellBank::getActiveInnerLayerImage returns nullptr for invalid cell", "[cellbank][innerlayers]")
{
    CellBank bank;
    REQUIRE(bank.getActiveInnerLayerImage(-999) == nullptr);
}

// ── 6. Null cell guard: getCellLayerStack(-999) returns nullptr ────────────────

TEST_CASE("CellBank::getCellLayerStack returns nullptr for invalid cell", "[cellbank][innerlayers]")
{
    CellBank bank;
    REQUIRE(bank.getCellLayerStack(-999) == nullptr);
}

// ── 7. Null cell guard: getActiveInnerLayerUndoStack(-999) returns nullptr ─────

TEST_CASE("CellBank::getActiveInnerLayerUndoStack returns nullptr for invalid cell", "[cellbank][innerlayers]")
{
    CellBank bank;
    REQUIRE(bank.getActiveInnerLayerUndoStack(-999) == nullptr);
}

// ── 8. setActiveInnerLayerIndex / activeInnerLayerIndex round-trip ─────────────

TEST_CASE("CellBank active inner layer index round-trip", "[cellbank][innerlayers]")
{
    CellBank bank;
    int id = makeCell(bank);

    bank.addInnerLayer(id, "Layer 2");
    bank.addInnerLayer(id, "Layer 3");

    SECTION("Set to index 0 and read back") {
        bank.setActiveInnerLayerIndex(id, 0);
        REQUIRE(bank.activeInnerLayerIndex(id) == 0);
    }

    SECTION("Set to index 1 and read back") {
        bank.setActiveInnerLayerIndex(id, 1);
        REQUIRE(bank.activeInnerLayerIndex(id) == 1);
    }

    SECTION("Set to index 2 and read back") {
        bank.setActiveInnerLayerIndex(id, 2);
        REQUIRE(bank.activeInnerLayerIndex(id) == 2);
    }

    SECTION("Invalid cell id returns -1") {
        REQUIRE(bank.activeInnerLayerIndex(-999) == -1);
    }
}

// ── 9. getInnerLayerUndoStack returns the correct stack for each layer ─────────

TEST_CASE("CellBank::getInnerLayerUndoStack returns correct stack per layer", "[cellbank][innerlayers]")
{
    CellBank bank;
    int id = makeCell(bank);

    bank.addInnerLayer(id, "Layer 2");
    bank.addInnerLayer(id, "Layer 3");

    CellLayerStack* stack = bank.getCellLayerStack(id);
    REQUIRE(stack != nullptr);
    REQUIRE(stack->layerCount() == 3);

    SECTION("Each index returns a distinct non-null undo stack") {
        QUndoStack* s0 = bank.getInnerLayerUndoStack(id, 0);
        QUndoStack* s1 = bank.getInnerLayerUndoStack(id, 1);
        QUndoStack* s2 = bank.getInnerLayerUndoStack(id, 2);

        REQUIRE(s0 != nullptr);
        REQUIRE(s1 != nullptr);
        REQUIRE(s2 != nullptr);

        REQUIRE(s0 != s1);
        REQUIRE(s1 != s2);
        REQUIRE(s0 != s2);
    }

    SECTION("Stack matches the InnerLayer's own undoStack pointer") {
        for (int i = 0; i < stack->layerCount(); ++i) {
            REQUIRE(bank.getInnerLayerUndoStack(id, i) == stack->layerAt(i)->undoStack);
        }
    }

    SECTION("Out-of-bounds index returns nullptr") {
        REQUIRE(bank.getInnerLayerUndoStack(id, 99) == nullptr);
    }
}
