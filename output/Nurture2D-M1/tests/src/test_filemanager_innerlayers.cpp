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

// Requirements: 7.4, 7.5

#include "catch.hpp"

#include <QDomDocument>
#include <QDomElement>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QImage>

#include "layerbitmap.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "celllayerstack.h"
#include "innerlayer.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

// Serialize a LayerBitmap to a QDomDocument and return the <layer> element.
static QDomElement serializeLayer(const LayerBitmap& layer, QDomDocument& doc)
{
    doc.clear();
    QDomElement root = doc.createElement("document");
    doc.appendChild(root);
    QDomElement layerElem = layer.createDomElement(doc);
    root.appendChild(layerElem);
    return layerElem;
}

// Find the first <image> child element of a <layer> element.
static QDomElement firstImageElement(const QDomElement& layerElem)
{
    QDomNode child = layerElem.firstChild();
    while (!child.isNull()) {
        QDomElement e = child.toElement();
        if (!e.isNull() && e.tagName() == "image") {
            return e;
        }
        child = child.nextSibling();
    }
    return QDomElement();
}

// Write a minimal 1x1 transparent PNG to disk so loadDomElement can resolve it.
static bool writeDummyPng(const QString& path)
{
    QImage img(1, 1, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    return img.save(path, "PNG");
}

// ── Test 1: Legacy file loading (Req 7.4) ─────────────────────────────────────

TEST_CASE("FileManager: legacy format loads as single inner layer named 'Layer 1'",
          "[filemanager][innerlayers][legacy]")
{
    // 1. Create a LayerBitmap with one cell.
    LayerBitmap srcLayer(1);
    srcLayer.setName("TestLayer");
    REQUIRE(srcLayer.addNewKeyFrameAt(1));

    // 2. Serialize to XML.
    QDomDocument doc;
    QDomElement layerElem = serializeLayer(srcLayer, doc);

    // 3. Find the <image> element and strip the <layers> child to simulate a legacy file.
    QDomElement imageElem = firstImageElement(layerElem);
    REQUIRE(!imageElem.isNull());

    QDomElement legacyLayersChild = imageElem.firstChildElement("layers");
    if (!legacyLayersChild.isNull()) {
        imageElem.removeChild(legacyLayersChild);
    }

    // Verify the <layers> child is gone.
    REQUIRE(imageElem.firstChildElement("layers").isNull());

    // 4. Write the composite PNG to a temp dir so loadDomElement can find it.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // The src attribute tells us the filename to write.
    QString srcAttr = imageElem.attribute("src");
    REQUIRE(!srcAttr.isEmpty());
    QString pngPath = tempDir.filePath(srcAttr);
    REQUIRE(writeDummyPng(pngPath));

    // 5. Load back using loadDomElement.
    LayerBitmap dstLayer(1);
    dstLayer.loadDomElement(layerElem, tempDir.path(), [](){});

    // 6. Assert: exactly 1 keyframe was loaded.
    REQUIRE(dstLayer.keyFrameCount() == 1);

    // 7. Retrieve the cell's layer stack.
    CellBank* bank = dstLayer.cellBank();
    REQUIRE(bank != nullptr);

    QList<int> ids = bank->getAllCellIds();
    REQUIRE(ids.size() == 1);
    int cellId = ids.first();

    CellLayerStack* stack = bank->getCellLayerStack(cellId);
    REQUIRE(stack != nullptr);

    // 8. Assert: exactly 1 inner layer.
    REQUIRE(stack->layerCount() == 1);

    // 9. Assert: the inner layer is named "Layer 1" with default properties.
    const InnerLayer* layer0 = stack->layerAt(0);
    REQUIRE(layer0 != nullptr);
    CHECK(layer0->name == "Layer 1");
    CHECK(layer0->visible == true);
    CHECK(layer0->opacity == Approx(1.0));
    CHECK(layer0->blendMode == BlendMode::Normal);
}

// ── Test 2: New format round-trip (Req 7.1, 7.2, 7.3) ────────────────────────

TEST_CASE("FileManager: new format round-trip preserves inner layer properties",
          "[filemanager][innerlayers][roundtrip]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // 1. Create a LayerBitmap with one cell.
    LayerBitmap srcLayer(2);
    srcLayer.setName("RoundTripLayer");
    REQUIRE(srcLayer.addNewKeyFrameAt(1));

    CellBank* bank = srcLayer.cellBank();
    REQUIRE(bank != nullptr);

    QList<int> ids = bank->getAllCellIds();
    REQUIRE(ids.size() == 1);
    int cellId = ids.first();

    // 2. Add a second inner layer and configure both layers.
    bank->addInnerLayer(cellId, "Lines");

    CellLayerStack* stack = bank->getCellLayerStack(cellId);
    REQUIRE(stack != nullptr);
    REQUIRE(stack->layerCount() == 2);

    // Configure layer 0 (bottom).
    InnerLayer* layer0 = stack->layerAt(0);
    REQUIRE(layer0 != nullptr);
    layer0->name      = "Color";
    layer0->visible   = true;
    layer0->setOpacity(0.75);
    layer0->blendMode = BlendMode::Multiply;

    // Configure layer 1 (top).
    InnerLayer* layer1 = stack->layerAt(1);
    REQUIRE(layer1 != nullptr);
    layer1->name      = "Lines";
    layer1->visible   = false;
    layer1->setOpacity(0.5);
    layer1->blendMode = BlendMode::Screen;

    // 3. Serialize to XML.
    QDomDocument doc;
    QDomElement layerElem = serializeLayer(srcLayer, doc);

    // 4. Write the per-inner-layer PNG files to the temp dir.
    //    The src attributes in <layer> elements tell us the filenames.
    QDomElement imageElem = firstImageElement(layerElem);
    REQUIRE(!imageElem.isNull());

    QDomElement layersElem = imageElem.firstChildElement("layers");
    REQUIRE(!layersElem.isNull());

    QDomElement layerTag = layersElem.firstChildElement("layer");
    while (!layerTag.isNull()) {
        QString src = layerTag.attribute("src");
        if (!src.isEmpty()) {
            QString path = tempDir.filePath(src);
            writeDummyPng(path);
        }
        layerTag = layerTag.nextSiblingElement("layer");
    }

    // Also write the composite PNG referenced by the <image> element.
    QString compositeSrc = imageElem.attribute("src");
    if (!compositeSrc.isEmpty()) {
        writeDummyPng(tempDir.filePath(compositeSrc));
    }

    // 5. Load back using loadDomElement.
    LayerBitmap dstLayer(2);
    dstLayer.loadDomElement(layerElem, tempDir.path(), [](){});

    // 6. Assert: exactly 1 keyframe.
    REQUIRE(dstLayer.keyFrameCount() == 1);

    CellBank* dstBank = dstLayer.cellBank();
    REQUIRE(dstBank != nullptr);

    QList<int> dstIds = dstBank->getAllCellIds();
    REQUIRE(dstIds.size() == 1);
    int dstCellId = dstIds.first();

    CellLayerStack* dstStack = dstBank->getCellLayerStack(dstCellId);
    REQUIRE(dstStack != nullptr);

    // 7. Assert: layer count preserved.
    REQUIRE(dstStack->layerCount() == 2);

    // 8. Assert: layer 0 properties preserved.
    const InnerLayer* dst0 = dstStack->layerAt(0);
    REQUIRE(dst0 != nullptr);
    CHECK(dst0->name      == "Color");
    CHECK(dst0->visible   == true);
    CHECK(dst0->opacity   == Approx(0.75));
    CHECK(dst0->blendMode == BlendMode::Multiply);

    // 9. Assert: layer 1 properties preserved.
    const InnerLayer* dst1 = dstStack->layerAt(1);
    REQUIRE(dst1 != nullptr);
    CHECK(dst1->name      == "Lines");
    CHECK(dst1->visible   == false);
    CHECK(dst1->opacity   == Approx(0.5));
    CHECK(dst1->blendMode == BlendMode::Screen);
}

// ── Test 3: Missing layer file substitution (Req 7.5) ─────────────────────────

TEST_CASE("FileManager: missing inner layer file is substituted with transparent image",
          "[filemanager][innerlayers][missing-file]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    // Build a minimal XML document that references a non-existent PNG file
    // inside a <layers> child, simulating a corrupt or partially-extracted archive.
    QDomDocument doc;
    QDomElement root = doc.createElement("document");
    doc.appendChild(root);

    // <layer> element (outer timeline layer)
    QDomElement layerElem = doc.createElement("layer");
    layerElem.setAttribute("name", "MissingFileLayer");
    layerElem.setAttribute("id", "3");
    layerElem.setAttribute("visibility", "1");
    layerElem.setAttribute("type", "1");
    root.appendChild(layerElem);

    // Write a real composite PNG so the <image> entry passes path resolution.
    const QString compositeSrc = "L003_C000001.png";
    REQUIRE(writeDummyPng(tempDir.filePath(compositeSrc)));

    // <image> element
    QDomElement imageElem = doc.createElement("image");
    imageElem.setAttribute("frame", "1");
    imageElem.setAttribute("src", compositeSrc);
    imageElem.setAttribute("cellId", "1");
    imageElem.setAttribute("cellName", "test_cell");
    imageElem.setAttribute("topLeftX", "0");
    imageElem.setAttribute("topLeftY", "0");
    imageElem.setAttribute("opacity", "1.0");
    layerElem.appendChild(imageElem);

    // <layers> child with two <layer> entries — both reference non-existent files.
    QDomElement layersTag = doc.createElement("layers");
    layersTag.setAttribute("activeIndex", "0");
    imageElem.appendChild(layersTag);

    QDomElement innerLayer0 = doc.createElement("layer");
    innerLayer0.setAttribute("index", "0");
    innerLayer0.setAttribute("src", "DOES_NOT_EXIST_L0.png");
    innerLayer0.setAttribute("name", "Color");
    innerLayer0.setAttribute("visible", "true");
    innerLayer0.setAttribute("opacity", "1.0");
    innerLayer0.setAttribute("blendMode", "Normal");
    innerLayer0.setAttribute("topLeftX", "0");
    innerLayer0.setAttribute("topLeftY", "0");
    layersTag.appendChild(innerLayer0);

    QDomElement innerLayer1 = doc.createElement("layer");
    innerLayer1.setAttribute("index", "1");
    innerLayer1.setAttribute("src", "DOES_NOT_EXIST_L1.png");
    innerLayer1.setAttribute("name", "Lines");
    innerLayer1.setAttribute("visible", "true");
    innerLayer1.setAttribute("opacity", "1.0");
    innerLayer1.setAttribute("blendMode", "Normal");
    innerLayer1.setAttribute("topLeftX", "0");
    innerLayer1.setAttribute("topLeftY", "0");
    layersTag.appendChild(innerLayer1);

    // Load — must not crash or abort.
    LayerBitmap dstLayer(3);
    // This call must complete without crashing.
    dstLayer.loadDomElement(layerElem, tempDir.path(), [](){});

    // Assert: the cell was still created (load did not abort).
    CellBank* bank = dstLayer.cellBank();
    REQUIRE(bank != nullptr);

    QList<int> ids = bank->getAllCellIds();
    REQUIRE(ids.size() == 1);
    int cellId = ids.first();

    CellLayerStack* stack = bank->getCellLayerStack(cellId);
    REQUIRE(stack != nullptr);

    // Assert: both inner layers are present (transparent substitution applied).
    CHECK(stack->layerCount() == 2);

    // Assert: each substituted layer has a non-null (transparent) image.
    for (int i = 0; i < stack->layerCount(); ++i) {
        const InnerLayer* il = stack->layerAt(i);
        REQUIRE(il != nullptr);
        CHECK(il->image != nullptr);
    }
}
