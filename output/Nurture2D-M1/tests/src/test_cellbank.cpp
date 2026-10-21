#include "catch.hpp"
#include <memory>
#include "cellbank.h"
#include "bitmapimage.h"
#include "celllayerstack.h"
#include "innerlayer.h"
#include "layerbitmap.h"

TEST_CASE("CellBank Basic Operations") {
    CellBank bank;

    SECTION("Add Cell") {
        BitmapImage* img = new BitmapImage();
        int id = bank.addCell(img, "Cell_1");

        REQUIRE(id == 1);
        REQUIRE(bank.getAllCellIds().count() == 1);
        // getCell() returns the composite (not the raw image pointer)
        REQUIRE(bank.getCell(id) != nullptr);
        // The raw inner layer image is the original pointer
        REQUIRE(bank.getActiveInnerLayerImage(id) == img);
        REQUIRE(bank.getCellName(id) == "Cell_1");
    }

    SECTION("Duplicate Cell") {
        BitmapImage* img = new BitmapImage();
        int id1 = bank.addCell(img, "Original");

        int id2 = bank.duplicateCell(id1);

        REQUIRE(id2 != id1);
        REQUIRE(bank.getAllCellIds().count() == 2);
        REQUIRE(bank.getCellName(id2) == "Original_copy");
        // Composites are different objects
        REQUIRE(bank.getCell(id1) != bank.getCell(id2));
    }

    SECTION("Remove Cell") {
        BitmapImage* img = new BitmapImage();
        int id = bank.addCell(img, "To Delete");

        bank.removeCell(id);

        REQUIRE(bank.getAllCellIds().isEmpty());
        REQUIRE(bank.getCell(id) == nullptr);
    }
}
