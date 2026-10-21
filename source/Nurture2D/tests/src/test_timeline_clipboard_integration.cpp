#include <QApplication>
#include <QAction>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QClipboard>
#include <QMimeData>
#include <QMenu>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "mainwindow2.h"
#include "editor.h"
#include "timelinecells.h"
#include "layermanager.h"
#include "clipboardmanager.h"
#include "undoredomanager.h"
#include "playbackmanager.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "celllayerstack.h"
#include "innerlayer.h"
#include "filemanager.h"
#include "object.h"
#include "scribblearea.h"

namespace {
int checks = 0;
void require(bool ok, const char* text) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
    ++checks;
}
int cell(LayerBitmap* layer, int frame) {
    auto* key = dynamic_cast<BitmapKeyFrame*>(layer->getKeyFrameAt(frame));
    require(key != nullptr, "numbered drawing exists");
    return key->cellId();
}
void clickFrame(TimeLineCells* tracks, Editor* editor, int frame) {
    tracks->hScrollChange(0);
    const auto visible = tracks->getVisibleLayerIndices();
    int row = visible.size() - 1 - visible.indexOf(editor->currentLayerIndex());
    QPointF point(TimeLineCells::getOffsetX() + frame * tracks->getFrameSize() + tracks->getFrameSize()/2,
                  TimeLineCells::getOffsetY() + row * tracks->getLayerHeight() + tracks->getLayerHeight()/2);
    QMouseEvent press(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tracks, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tracks, &release);
    QApplication::processEvents();
    require(editor->currentFrame() == frame, "simple click chooses exact timeline position");
}
void key(QWidget* target, quint32 physical, int logical, bool redo = false) {
    const Qt::KeyboardModifiers modifiers = Qt::ControlModifier | (redo ? Qt::ShiftModifier : Qt::NoModifier);
    QKeyEvent claim(QEvent::ShortcutOverride, logical, modifiers, 0, physical, 0);
    claim.setAccepted(false);
    QApplication::sendEvent(target, &claim);
    QKeyEvent press(QEvent::KeyPress, logical, modifiers, 0, physical, 0, QString(QChar(logical)));
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, logical, modifiers, 0, physical, 0);
    QApplication::sendEvent(target, &release);
    QApplication::processEvents();
}
QAction* undoAction(MainWindow2* window) {
    auto* edit = window->findChild<QMenu*>("menuEdit");
    require(edit != nullptr, "real Edit menu exists");
    for (auto* action : edit->actions())
        if (action->text().remove('&') == "Undo") return action;
    require(false, "real Undo action exists"); return nullptr;
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    auto* savedClipboard = new QMimeData;
    if (const auto* mime = app.clipboard()->mimeData())
        for (const auto& format : mime->formats()) savedClipboard->setData(format, mime->data(format));
    QTemporaryDir temporary;
    require(temporary.isValid(), "isolated test directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    auto* window = new MainWindow2;
    window->resize(1440, 900); window->show();
    app.processEvents();
    auto* editor = window->mEditor;
    auto* layer = dynamic_cast<LayerBitmap*>(editor->layers()->currentLayer());
    auto* tracks = window->findChild<TimeLineCells*>("timelineTracks");
    require(layer && tracks, "actual bitmap timeline");
    tracks->setFrameSize(20);
    editor->playback()->enableRangedPlayback(false);
    for (int frame : {6,14,22,30}) require(layer->addNewKeyFrameAt(frame), "add numbered drawing");
    auto* bank = layer->cellBank();
    const int sourceId = cell(layer, 6);
    const int oldFifth = cell(layer, 30);
    auto* source = bank->getCellLayerStack(sourceId);
    source->layerAt(0)->image->drawEllipse(QRectF(0,0,30,30), QPen(Qt::red, 3), Qt::NoBrush,
                                        QPainter::CompositionMode_SourceOver, true);
    bank->addInnerLayer(sourceId, "Second paper");
    source->layerAt(1)->image->drawEllipse(QRectF(40,0,20,20), QPen(Qt::blue, 2), Qt::NoBrush,
                                        QPainter::CompositionMode_SourceOver, true);
    source->layerAt(1)->setOpacity(0.4);
    source->invalidateComposite();
    const QImage original = *source->composite()->image();
    const auto originalOrder = bank->getAllCellIds();
    const QString oldFifthName = bank->getCellName(oldFifth);
    clickFrame(tracks, editor, 6);
    require(layer->isFrameSelected(6), "click without dragging keeps source selected");
    clickFrame(tracks, editor, 6);
    require(layer->isFrameSelected(6), "second click keeps source selected for copy");
    key(tracks, 8, 0x0421); // Command + physical C, Cyrillic logical С
    require(!editor->clipboards()->framesIsEmpty(), "physical Command+C copies the timeline frame");
    source->layerAt(0)->image->clear(); source->invalidateComposite();
    clickFrame(tracks, editor, 26);
    key(tracks, 9, 0x041c); // Command + physical V, Cyrillic logical М
    require(layer->keyExists(26), "Command+V inserts in an empty position");
    const int pasted = cell(layer, 26);
    require(pasted != sourceId, "copy has an independent drawing identity");
    auto* copy = bank->getCellLayerStack(pasted);
    require(copy && copy->layerCount() == 2, "all inner layers copied");
    require(copy->layerAt(1)->name == "Second paper" && qFuzzyCompare(copy->layerAt(1)->opacity, 0.4),
            "inner layer metadata copied");
    require(*copy->composite()->image() == original, "snapshot preserves content from copy time");
    require(bank->indexForCellId(pasted) == 5 && bank->indexForCellId(oldFifth) == 6,
            "inserted drawing is fifth and previous fifth becomes sixth");
    require(bank->getCellName(pasted).endsWith("005") && bank->getCellName(oldFifth).endsWith("006"),
            "visible numeric labels shift in order");
    require(layer->keyExists(30), "following drawing stays on its original timeline frame");
    require(editor->undoRedo()->activeStack() == editor->undoRedo()->undoStack(), "timeline routes Undo to structural history");
    require(undoAction(window)->isEnabled(), "actual Command+Z action enabled immediately after paste");
    key(tracks, 6, 0x042f);
    require(!layer->keyExists(26) && bank->getCellLayerStack(pasted) == nullptr, "Undo removes inserted drawing");
    require(bank->getAllCellIds() == originalOrder && bank->getCellName(oldFifth) == oldFifthName,
            "Undo restores numbering and bank order");
    key(tracks, 6, 0x042f, true);
    require(cell(layer,26) == pasted, "Redo restores same drawing identity");
    require(*bank->getCell(pasted)->image() == original, "Redo restores full content");
    clickFrame(tracks, editor, 35);
    key(tracks, 9, Qt::Key_V);
    const int secondCopy = cell(layer, 35);
    require(secondCopy != pasted && secondCopy != sourceId, "repeated paste creates another independent copy");
    const auto beforeOccupied = bank->getAllCellIds();
    clickFrame(tracks, editor, 30);
    key(tracks, 9, Qt::Key_V);
    require(cell(layer,30) == oldFifth && bank->getAllCellIds() == beforeOccupied,
            "occupied destination is not overwritten");
    auto* text = new QLineEdit(window); text->show(); text->setFocus();
    key(text,9,0x041c);
    require(bank->getAllCellIds() == beforeOccupied, "text focus does not paste timeline drawings");
    text->hide();
    // The complete document, including names, order and inner layers, survives save/reopen.
    editor->prepareSave();
    FileManager files;
    const QString path = temporary.filePath("clipboard.pclx");
    require(files.save(editor->object(),path).ok(), "save pasted animation");
    std::unique_ptr<Object> loaded(files.load(path));
    require(loaded != nullptr, "reopen pasted animation");
    auto* restored = dynamic_cast<LayerBitmap*>(loaded->getLayer(editor->currentLayerIndex()));
    require(restored && restored->keyExists(26) && restored->keyExists(35), "pasted drawings survive reopen");
    require(restored->cellBank()->getCellName(oldFifth).endsWith("006"), "numbering survives reopen");
    app.clipboard()->setText("external clipboard regression");
    app.processEvents();
    require(editor->clipboards()->framesIsEmpty(), "external copy replaces old timeline clipboard");

    auto* secondWindow = new MainWindow2;
    secondWindow->resize(1440,900); secondWindow->show();
    QApplication::setActiveWindow(secondWindow); app.processEvents();
    auto* second = secondWindow->mEditor;
    auto* two = dynamic_cast<LayerBitmap*>(second->layers()->currentLayer());
    auto* secondTracks = secondWindow->findChild<TimeLineCells*>("timelineTracks");
    require(two && secondTracks && two->addNewKeyFrameAt(8), "exact two-drawing regression document");
    secondTracks->setFrameSize(20);
    const int firstId = cell(two,1), followingId = cell(two,8);
    auto* secondBank = two->cellBank();
    auto* canvas = second->getScribbleArea();
    for (int cycle = 0; cycle < 8; ++cycle) {
        clickFrame(secondTracks,second,1); key(secondTracks,8,Qt::Key_C);
        clickFrame(secondTracks,second,4); key(secondTracks,9,Qt::Key_V);
        const int inserted = cell(two,4);
        require(secondBank->getCellName(firstId).endsWith("001")
                && secondBank->getCellName(inserted).endsWith("002")
                && secondBank->getCellName(followingId).endsWith("003"), "insert gives 1,2,3");
        require(undoAction(secondWindow)->isEnabled(), "Undo action enabled on every paste, without changing focus");
        const int index = second->undoRedo()->undoStack()->index();
        canvas->setFocus(); app.processEvents();
        require(undoAction(secondWindow)->isEnabled(), "Undo remains available after focusing canvas with empty drawing history");
        key(canvas,6,cycle%2 ? 0x042f : Qt::Key_Z);
        require(!two->keyExists(4) && second->undoRedo()->undoStack()->index() == index-1,
                "one physical Command+Z removes exactly one paste from canvas focus");
        require(secondBank->getCellName(firstId).endsWith("001")
                && secondBank->getCellName(followingId).endsWith("002"), "Undo paste restores 1,2 with no gap");
        key(canvas,6,Qt::Key_Z,true);
        require(cell(two,4) == inserted && secondBank->getCellName(followingId).endsWith("003"),
                "Command+Shift+Z restores copy and numbering");
        clickFrame(secondTracks,second,4);
        second->removeKey(); app.processEvents();
        require(!two->keyExists(4) && secondBank->getCellName(followingId).endsWith("002"),
                "minus/delete of copied frame leaves visible numbers 1,2");
        key(secondTracks,6,Qt::Key_Z);
        require(two->keyExists(4) && secondBank->getCellName(followingId).endsWith("003"),
                "Undo deletion restores 1,2,3");
        key(secondTracks,6,Qt::Key_Z);
        require(!two->keyExists(4) && secondBank->getAllCellIds().size() == 2
                && secondBank->getCellName(followingId).endsWith("002"),
                "Undo paste after Undo deletion restores exactly two original drawings numbered 1,2");
    }
    second->scrubTo(1); canvas->setFocus(); app.processEvents();
    auto* drawing = secondBank->getActiveInnerLayerImage(firstId);
    require(drawing && drawing->bounds().isEmpty(), "blank drawing for pixel Undo regression");
    const int structuralIndex = second->undoRedo()->undoStack()->index();
    const UndoSaveState* beforeStroke = second->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    drawing->drawEllipse(QRectF(0,0,40,40), QPen(Qt::green,3), Qt::NoBrush,
                         QPainter::CompositionMode_SourceOver,true);
    secondBank->getCellLayerStack(firstId)->invalidateComposite();
    second->undoRedo()->record(beforeStroke,"Regression stroke");
    require(undoAction(secondWindow)->isEnabled(), "drawing Undo action enabled after stroke");
    key(canvas,6,Qt::Key_Z);
    require(drawing->bounds().isEmpty() && second->undoRedo()->undoStack()->index() == structuralIndex,
            "Command+Z undoes drawing pixels without touching timeline history");
    key(canvas,6,0x042f,true);
    require(!drawing->bounds().isEmpty(), "Command+Shift+Z restores drawing pixels after structural history");
    app.clipboard()->setMimeData(savedClipboard);
    std::printf("PASS: %d timeline clipboard checks\n", checks); std::fflush(stdout); std::_Exit(0);
}
