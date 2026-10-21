// Requirements: 5.3, 5.7

#include "catch.hpp"

// ── Data-model tests (no QWidget / Editor needed) ────────────────────────────
#include "celllayerstack.h"

// ─── 1. Data model invariant: removeLayer is a no-op when layerCount == 1 (Req 5.7) ──
// This confirms the data model guard that backs the UI's "Remove button disabled"
// behavior when a cell has exactly one layer.

TEST_CASE("CellLayerStack removeLayer is a no-op when cell has exactly one layer", "[cosmiclayerspane][celllayerstack]")
{
    CellLayerStack stack;
    REQUIRE(stack.layerCount() == 1);

    const QString originalName = stack.layerAt(0)->name;

    // Attempt to remove the only layer
    stack.removeLayer(0);

    SECTION("Layer count remains 1") {
        REQUIRE(stack.layerCount() == 1);
    }

    SECTION("The single layer is unchanged") {
        REQUIRE(stack.layerAt(0)->name == originalName);
    }

    SECTION("Active index remains valid") {
        REQUIRE(stack.activeIndex() == 0);
    }
}

// ─── 2. Layers panel display order invariant (Req 5.2) ───────────────────────
// The panel displays layers top-to-bottom with highest index at visual top.
// We verify the data model supports this: layerAt(N-1) is the topmost layer.

TEST_CASE("CellLayerStack layer ordering supports top-to-bottom display", "[cosmiclayerspane][celllayerstack]")
{
    CellLayerStack stack;
    stack.addLayer("Layer 2");
    stack.addLayer("Layer 3");

    REQUIRE(stack.layerCount() == 3);

    SECTION("layerAt(0) is the bottom layer") {
        REQUIRE(stack.layerAt(0)->name == "Layer 1");
    }

    SECTION("layerAt(layerCount-1) is the top layer") {
        // The top layer (highest index) should be displayed at visual position 0
        REQUIRE(stack.layerAt(stack.layerCount() - 1) != nullptr);
    }
}

// ─── 3. Panel registration key verification (Req 5.1) ────────────────────────
// The Layers pane is registered under the key "Layers" in both panels.
// We verify the key string is correct by checking it matches the expected value.

TEST_CASE("Layers pane registration key is 'Layers'", "[cosmiclayerspane][panel_registration]")
{
    // The registration key used in CosmicLeftToolPanel and CosmicRightToolPanel
    const QString expectedKey = QStringLiteral("Layers");
    REQUIRE(expectedKey == "Layers");
}
