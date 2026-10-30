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
#include "timelinecommands.h"
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
    auto* bank = layer->cellBank();
    auto* source = bank->getCellLayerStack(cell(layer, 1));
    source->layerAt(0)->image->drawEllipse(QRectF(0,0,30,30), QPen(Qt::red, 3), Qt::NoBrush,
                                         QPainter::CompositionMode_SourceOver, true);
    std::map<int, std::shared_ptr<const CellLayerStack>> frames;
    frames[18] = std::shared_ptr<const CellLayerStack>(source->deepCopy());
    PasteBitmapFramesCommand paste(layer->id(), frames, editor);
    paste.redo();
    const int pasted = cell(layer,18);
    require(layer->moveKeyFrame(18, -15), "move pasted drawing to frame 3");
    require(layer->addNewKeyFrameAt(18), "another drawing occupies original position");
    const int unrelated = cell(layer,18);
    paste.undo();
    require(!layer->keyExists(3), "undo removes moved pasted key without dangling cell reference");
    require(cell(layer,18) == unrelated && bank->getCell(unrelated), "undo preserves unrelated drawing at old position");
    QStringList attached;
    const Status saved = layer->save(temporary.path(), attached, [] {});
    require(saved.ok(), "layer remains saveable after moved paste undo");
    const int original = cell(layer,1);
    MakeUniqueDrawingCommand unique(1, layer->id(), editor);
    unique.redo();
    require(cell(layer,1) != original, "unique drawing created");
    require(layer->moveKeyFrame(1,1), "unique drawing moved");
    require(layer->addNewKeyFrameAt(1), "unrelated drawing at unique old position");
    const int replacement = cell(layer,1);
    unique.undo();
    require(cell(layer,2) == original, "undo uniqueness follows moved cell identity");
    require(cell(layer,1) == replacement, "undo uniqueness preserves unrelated old-position drawing");
    unique.redo();
    require(cell(layer,2) != original, "redo uniqueness follows moved key");
    attached.clear();
    require(layer->save(temporary.path(), attached, [] {}).ok(), "save after moved unique undo redo");
    std::printf("PASS: %d moved paste save checks\n", checks);
    std::fflush(stdout); std::_Exit(0);
}
