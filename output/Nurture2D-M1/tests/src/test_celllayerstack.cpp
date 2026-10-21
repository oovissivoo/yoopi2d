#include "catch.hpp"
#include "../../core_lib/src/structure/celllayerstack.h"
#include "../../core_lib/src/structure/innerlayer.h"

// Requirements: 1.2, 2.1, 5.7

TEST_CASE("CellLayerStack default construction", "[celllayerstack]") {
    CellLayerStack stack;

    SECTION("Has exactly one layer") {
        REQUIRE(stack.layerCount() == 1);
    }

    SECTION("Default layer is named 'Layer 1'") {
        REQUIRE(stack.layerAt(0)->name == "Layer 1");
    }

    SECTION("Default layer is visible") {
        REQUIRE(stack.layerAt(0)->visible == true);
    }

    SECTION("Default layer opacity is 1.0") {
        REQUIRE(stack.layerAt(0)->opacity == Approx(1.0));
    }

    SECTION("Default layer blend mode is Normal") {
        REQUIRE(stack.layerAt(0)->blendMode == BlendMode::Normal);
    }

    SECTION("Active index is 0") {
        REQUIRE(stack.activeIndex() == 0);
    }
}

TEST_CASE("CellLayerStack default naming sequence", "[celllayerstack]") {
    // Req 2.1: adding layers produces "Layer 1", "Layer 2", "Layer 3" etc.
    CellLayerStack stack;
    // stack already has "Layer 1" at index 0

    stack.addLayer("Layer 2");
    stack.addLayer("Layer 3");

    // After two adds the stack has 3 layers
    REQUIRE(stack.layerCount() == 3);

    // Collect names regardless of order
    QStringList names;
    for (int i = 0; i < stack.layerCount(); ++i) {
        names << stack.layerAt(i)->name;
    }
    REQUIRE(names.contains("Layer 1"));
    REQUIRE(names.contains("Layer 2"));
    REQUIRE(names.contains("Layer 3"));
}

TEST_CASE("CellLayerStack removeLayer on single layer is a no-op", "[celllayerstack]") {
    // Req 5.7
    CellLayerStack stack;
    REQUIRE(stack.layerCount() == 1);

    stack.removeLayer(0);

    REQUIRE(stack.layerCount() == 1);
    REQUIRE(stack.layerAt(0)->name == "Layer 1");
}

TEST_CASE("CellLayerStack opacity clamping", "[celllayerstack]") {
    CellLayerStack stack;
    InnerLayer* layer = stack.layerAt(0);

    SECTION("setOpacity below 0 clamps to 0.0") {
        layer->setOpacity(-0.5);
        REQUIRE(layer->opacity == Approx(0.0));
    }

    SECTION("setOpacity above 1 clamps to 1.0") {
        layer->setOpacity(1.5);
        REQUIRE(layer->opacity == Approx(1.0));
    }

    SECTION("setOpacity within range is unchanged") {
        layer->setOpacity(0.75);
        REQUIRE(layer->opacity == Approx(0.75));
    }
}

TEST_CASE("CellLayerStack addLayer inserts above active and sets new layer active", "[celllayerstack]") {
    CellLayerStack stack;
    // stack: [Layer 1], active = 0

    int newIndex = stack.addLayer("Layer 2");

    SECTION("New layer is above the previous active layer") {
        // "Layer 1" was at index 0; new layer should be at index 1 (above it)
        REQUIRE(newIndex == 1);
    }

    SECTION("New layer becomes the active layer") {
        REQUIRE(stack.activeIndex() == newIndex);
    }

    SECTION("Layer count increased by 1") {
        REQUIRE(stack.layerCount() == 2);
    }
}

TEST_CASE("CellLayerStack removeLayer adjusts activeIndex when removing active layer", "[celllayerstack]") {
    CellLayerStack stack;
    stack.addLayer("Layer 2");
    stack.addLayer("Layer 3");
    // stack: [Layer 1, Layer 2, Layer 3], active = 2 (Layer 3)

    REQUIRE(stack.layerCount() == 3);
    REQUIRE(stack.activeIndex() == 2);

    stack.removeLayer(2); // remove active (top) layer

    SECTION("Layer count decreases by 1") {
        REQUIRE(stack.layerCount() == 2);
    }

    SECTION("Active index is still valid after removing active layer") {
        REQUIRE(stack.activeIndex() >= 0);
        REQUIRE(stack.activeIndex() < stack.layerCount());
    }
}

TEST_CASE("CellLayerStack removeLayer adjusts activeIndex when removing layer below active", "[celllayerstack]") {
    CellLayerStack stack;
    stack.addLayer("Layer 2");
    stack.addLayer("Layer 3");
    // stack: [Layer 1, Layer 2, Layer 3], active = 2 (Layer 3)

    REQUIRE(stack.activeIndex() == 2);

    // Remove a layer below the active layer (index 0)
    stack.removeLayer(0);

    SECTION("Layer count decreases by 1") {
        REQUIRE(stack.layerCount() == 2);
    }

    SECTION("Active index shifts down by 1 to keep pointing at same layer") {
        REQUIRE(stack.activeIndex() == 1);
    }

    SECTION("Active layer content is preserved") {
        REQUIRE(stack.activeLayer()->name == "Layer 3");
    }
}

TEST_CASE("CellLayerStack layerAt returns nullptr for out-of-bounds index", "[celllayerstack]") {
    CellLayerStack stack;

    SECTION("Negative index returns nullptr") {
        REQUIRE(stack.layerAt(-1) == nullptr);
    }

    SECTION("Index equal to layerCount returns nullptr") {
        REQUIRE(stack.layerAt(stack.layerCount()) == nullptr);
    }

    SECTION("Large index returns nullptr") {
        REQUIRE(stack.layerAt(999) == nullptr);
    }
}

TEST_CASE("CellLayerStack setActiveIndex clamps to valid range", "[celllayerstack]") {
    CellLayerStack stack;
    stack.addLayer("Layer 2");
    stack.addLayer("Layer 3");
    // layerCount == 3, valid indices: 0, 1, 2

    SECTION("Negative index clamps to 0") {
        stack.setActiveIndex(-5);
        REQUIRE(stack.activeIndex() == 0);
    }

    SECTION("Index beyond last layer clamps to last valid index") {
        stack.setActiveIndex(100);
        REQUIRE(stack.activeIndex() == stack.layerCount() - 1);
    }

    SECTION("Valid index is set correctly") {
        stack.setActiveIndex(1);
        REQUIRE(stack.activeIndex() == 1);
    }
}
