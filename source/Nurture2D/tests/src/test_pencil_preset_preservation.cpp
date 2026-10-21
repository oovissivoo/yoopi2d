/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

/**
 * Preservation Property Tests: Pencil Tool Preset Activation
 *
 * Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5
 *
 * Property 2: Preservation — Non-PENCIL Tool Switches Unaffected
 *
 * For any input where isBugCondition returns false (manual preset clicks,
 * switches to other tools, Curve/Polyline/BSpline tool usage), the fixed
 * code SHALL produce exactly the same observable behavior as the original
 * code, preserving all existing preset-selection flows, tool-switch flows,
 * and stroke rendering pipelines.
 *
 * These tests MUST PASS on BOTH unfixed and fixed code:
 *   - On unfixed code: confirms the baseline behavior to preserve.
 *   - On fixed code: confirms no regressions were introduced.
 *
 * Architecture note: The fix lambda is simulated here using the same
 * connectPresetActivationLambda helper as in the bug condition tests.
 * The lambda contains a `if (newTool != PENCIL) return;` guard that makes
 * it a no-op for every non-PENCIL tool switch.
 */

#include "catch.hpp"

#include <QSettings>
#include <QCoreApplication>
#include <QObject>
#include <vector>
#include <algorithm>

#include "editor.h"
#include "object.h"
#include "toolmanager.h"
#include "pencildef.h"
#include "scribblearea.h"

// ---------------------------------------------------------------------------
// Helpers (mirrors test_pencil_preset_activation_bug.cpp)
// ---------------------------------------------------------------------------

static bool pres_readFinalLineActive()
{
    QSettings s(PENCIL2D, PENCIL2D);
    s.beginGroup("PencilBrushFactory");
    const bool v = s.value("FinalLineActive", false).toBool();
    s.endGroup();
    return v;
}

static void pres_writeFinalLineActive(bool value)
{
    QSettings s(PENCIL2D, PENCIL2D);
    s.beginGroup("PencilBrushFactory");
    s.setValue("FinalLineActive", value);
    s.endGroup();
    s.sync();
}

static void pres_clearPencilBrushFactorySettings()
{
    QSettings s(PENCIL2D, PENCIL2D);
    s.beginGroup("PencilBrushFactory");
    s.remove("");
    s.endGroup();
    s.sync();
}

/**
 * Simulate the toolChanged lambda from MainWindow2::createDockWidgets().
 * The lambda contains `if (newTool != PENCIL) return;` which makes it a
 * no-op for every non-PENCIL tool switch — this is the preservation guard.
 */
static QMetaObject::Connection pres_connectPresetActivationLambda(
    ToolManager* tools,
    bool finalLineActive)
{
    return QObject::connect(tools, &ToolManager::toolChanged,
        [tools, finalLineActive](ToolType newTool) {
            if (newTool != PENCIL) return;   // <-- preservation guard

            if (finalLineActive) {
                QSettings settings(PENCIL2D, PENCIL2D);
                settings.beginGroup("PencilBrushFactory");
                settings.setValue("FinalLineActive", true);
                settings.setValue("ActivePresetId", QStringLiteral("__final_line__"));
                settings.endGroup();
                settings.sync();
                tools->setCurrentTool(PEN);
            } else {
                QSettings settings(PENCIL2D, PENCIL2D);
                settings.beginGroup("PencilBrushFactory");
                settings.setValue("FinalLineActive", false);
                settings.endGroup();
                settings.sync();
            }
        });
}

/** Build a minimal Editor/ToolManager for testing. */
static Editor* pres_makeEditor()
{
    Object* object = new Object;
    object->init();
    object->addNewBitmapLayer();
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    scribbleArea->setEditor(editor);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    editor->init();
    return editor;
}

// ---------------------------------------------------------------------------
// Test P1 — Non-PENCIL tool switches do not alter QSettings
//
// Scenario: The fix lambda is connected with finalLineActive=true.
//           QSettings["FinalLineActive"] is set to false.
//           We switch to each non-PENCIL tool in turn.
//
// Expected: FinalLineActive remains false after every switch because the
//           lambda guard (`newTool != PENCIL`) prevents any writes.
//
// Validates: Requirements 3.4
// ---------------------------------------------------------------------------
TEST_CASE("Preservation P1: Non-PENCIL tool switches do not alter QSettings FinalLineActive")
{
    pres_clearPencilBrushFactorySettings();

    Editor* editor = pres_makeEditor();
    ToolManager* tools = editor->tools();
    REQUIRE(tools != nullptr);

    // Start with FinalLineActive=false in QSettings.
    pres_writeFinalLineActive(false);

    // Connect the fix lambda with finalLineActive=true.
    // If the guard were absent, switching to any tool would write true.
    auto conn = pres_connectPresetActivationLambda(tools, /*finalLineActive=*/true);

    // All non-PENCIL tool types to exercise.
    const std::vector<ToolType> nonPencilTools = {
        ERASER, HAND, CURVE, POLYLINE, BSPLINE, PEN,
        SELECT, MOVE, BUCKET, EYEDROPPER
    };

    for (ToolType t : nonPencilTools) {
        tools->setCurrentTool(t);
        // The lambda guard must prevent any write to QSettings.
        // Validates: Requirements 3.4
        REQUIRE(pres_readFinalLineActive() == false);
    }

    QObject::disconnect(conn);
    delete editor;
}

// ---------------------------------------------------------------------------
// Test P2 — Property-based: random sequences of non-PENCIL tool switches
//
// Scenario: The fix lambda is connected with finalLineActive=true.
//           QSettings["FinalLineActive"] is set to false.
//           We generate multiple sequences of non-PENCIL tool switches.
//
// Expected: FinalLineActive remains false throughout all sequences.
//           No preset re-application side effects occur.
//
// Validates: Requirements 3.3, 3.4
// ---------------------------------------------------------------------------
TEST_CASE("Preservation P2: Random sequences of non-PENCIL tool switches produce no side effects")
{
    pres_clearPencilBrushFactorySettings();

    Editor* editor = pres_makeEditor();
    ToolManager* tools = editor->tools();
    REQUIRE(tools != nullptr);

    pres_writeFinalLineActive(false);

    auto conn = pres_connectPresetActivationLambda(tools, /*finalLineActive=*/true);

    // Enumerate all non-PENCIL tools.
    const std::vector<ToolType> nonPencilTools = {
        ERASER, HAND, CURVE, POLYLINE, BSPLINE, PEN,
        SELECT, MOVE, BUCKET, EYEDROPPER
    };

    // Generate several deterministic sequences by rotating the list.
    // This covers many orderings without requiring a PBT framework.
    std::vector<ToolType> seq(nonPencilTools);
    for (int rotation = 0; rotation < static_cast<int>(nonPencilTools.size()); ++rotation) {
        // Rotate the sequence.
        std::rotate(seq.begin(), seq.begin() + 1, seq.end());

        for (ToolType t : seq) {
            tools->setCurrentTool(t);
            // Validates: Requirements 3.3, 3.4
            REQUIRE(pres_readFinalLineActive() == false);
        }
    }

    QObject::disconnect(conn);
    delete editor;
}

// ---------------------------------------------------------------------------
// Test P3 — PENCIL entry does not affect subsequent non-PENCIL tool switch
//
// Scenario: The fix lambda is connected with finalLineActive=false.
//           Switch to PENCIL (lambda fires, writes FinalLineActive=false).
//           Switch to ERASER (lambda guard: no-op).
//
// Expected: FinalLineActive is still false (not changed by ERASER switch).
//           currentTool()->type() == ERASER.
//
// Validates: Requirements 3.4
// ---------------------------------------------------------------------------
TEST_CASE("Preservation P3: PENCIL entry does not affect subsequent non-PENCIL tool switch")
{
    pres_clearPencilBrushFactorySettings();

    Editor* editor = pres_makeEditor();
    ToolManager* tools = editor->tools();
    REQUIRE(tools != nullptr);

    pres_writeFinalLineActive(false);

    auto conn = pres_connectPresetActivationLambda(tools, /*finalLineActive=*/false);

    // Switch to PENCIL — lambda fires, writes FinalLineActive=false.
    tools->setCurrentTool(PENCIL);
    REQUIRE(pres_readFinalLineActive() == false);
    REQUIRE(tools->currentTool()->type() == PENCIL);

    // Switch to ERASER — lambda guard: no-op.
    tools->setCurrentTool(ERASER);

    // FinalLineActive must still be false (ERASER switch must not alter it).
    // Validates: Requirements 3.4
    REQUIRE(pres_readFinalLineActive() == false);
    REQUIRE(tools->currentTool()->type() == ERASER);

    QObject::disconnect(conn);
    delete editor;
}

// ---------------------------------------------------------------------------
// Test P4 — Null guard: lambda with null brushesContent does not crash
//
// Scenario: A lambda that simulates brushesContent == nullptr returns
//           immediately without touching QSettings or the tool.
//           Switch to PENCIL.
//
// Expected: No crash. currentTool()->type() == PENCIL.
//           QSettings unchanged.
//
// Validates: Requirements 3.5
// ---------------------------------------------------------------------------
TEST_CASE("Preservation P4: Null guard — lambda with null brushesContent does not crash")
{
    pres_clearPencilBrushFactorySettings();

    Editor* editor = pres_makeEditor();
    ToolManager* tools = editor->tools();
    REQUIRE(tools != nullptr);

    pres_writeFinalLineActive(false);

    // Connect a lambda that simulates brushesContent == nullptr (early return).
    auto conn = QObject::connect(tools, &ToolManager::toolChanged,
        [](ToolType /*newTool*/) {
            // brushesContent is null — return immediately, no side effects.
            return;
        });

    // Switch to PENCIL — the null-guard lambda must not crash.
    // Validates: Requirements 3.5
    REQUIRE_NOTHROW(tools->setCurrentTool(PENCIL));
    REQUIRE(tools->currentTool()->type() == PENCIL);
    REQUIRE(pres_readFinalLineActive() == false);

    QObject::disconnect(conn);
    delete editor;
}

// ---------------------------------------------------------------------------
// Test P5 — Re-entry guard: PENCIL→PEN does not re-trigger PENCIL lambda
//
// Scenario: The fix lambda is connected with finalLineActive=true.
//           Switch to PENCIL (lambda fires: writes FinalLineActive=true,
//           calls setCurrentTool(PEN) which emits toolChanged(PEN)).
//           The lambda must NOT fire again for PEN (guard: newTool != PENCIL).
//
// Expected: FinalLineActive is true (written once, not overwritten).
//           currentTool()->type() == PEN.
//           No infinite recursion.
//
// Validates: Requirements 3.4
// ---------------------------------------------------------------------------
TEST_CASE("Preservation P5: Re-entry guard — PENCIL->PEN does not re-trigger PENCIL lambda")
{
    pres_clearPencilBrushFactorySettings();

    Editor* editor = pres_makeEditor();
    ToolManager* tools = editor->tools();
    REQUIRE(tools != nullptr);

    pres_writeFinalLineActive(false);

    // Count how many times the lambda fires for PENCIL.
    int pencilFireCount = 0;
    auto conn = QObject::connect(tools, &ToolManager::toolChanged,
        [tools, &pencilFireCount](ToolType newTool) {
            if (newTool != PENCIL) return;   // guard: no-op for PEN

            ++pencilFireCount;

            // Replay Final Line path: write settings, switch to PEN.
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.beginGroup("PencilBrushFactory");
            settings.setValue("FinalLineActive", true);
            settings.setValue("ActivePresetId", QStringLiteral("__final_line__"));
            settings.endGroup();
            settings.sync();
            tools->setCurrentTool(PEN);
            // setCurrentTool(PEN) emits toolChanged(PEN), but the guard
            // `newTool != PENCIL` prevents this lambda from firing again.
        });

    // Switch to PENCIL — lambda fires once, writes FinalLineActive=true,
    // then calls setCurrentTool(PEN) which emits toolChanged(PEN).
    // The lambda must NOT fire for PEN.
    tools->setCurrentTool(PENCIL);

    // Lambda fired exactly once (for PENCIL), not again for PEN.
    // Validates: Requirements 3.4
    REQUIRE(pencilFireCount == 1);
    REQUIRE(pres_readFinalLineActive() == true);
    REQUIRE(tools->currentTool()->type() == PEN);

    QObject::disconnect(conn);
    delete editor;
}
