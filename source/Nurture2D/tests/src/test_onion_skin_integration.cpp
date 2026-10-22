#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include "mainwindow2.h"
#include "editor.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "preferencemanager.h"
#include "shifttracemanager.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "celllayerstack.h"
#include "innerlayer.h"
#include "canvaspainter.h"
#include "object.h"

namespace {
int checks = 0;
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
    ++checks;
}
QColor pixel(const QImage& image, int x, qreal dpr = 1) {
    return image.pixelColor(qRound((400 + x) * dpr), qRound(200 * dpr));
}
QImage render(Editor* editor, qreal dpr = 1, bool playing = false) {
    QPixmap canvas(qRound(800 * dpr), qRound(400 * dpr));
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);
    CanvasPainter painter(canvas);
    painter.setShiftTraceManager(editor->shiftTrace());
    painter.setPaintSettings(editor->object(), editor->currentLayerIndex(), editor->currentFrame(), nullptr);
    QTransform view; view.translate(400, 200);
    painter.setViewTransform(view, view.inverted());
    // Reproduce the old independent renderer with untinted previous/next skins.
    OnionSkinPainterOptions options;
    options.skinPrevFrames = true; options.skinNextFrames = true;
    options.framesToSkinPrev = 5; options.framesToSkinNext = 5;
    options.maxOpacity = 100; options.minOpacity = 20; options.isAbsolute = true;
    options.isPlaying = playing;
    painter.setOnionSkinOptions(options);
    painter.paint(QRect(0, 0, 800, 400));
    return canvas.toImage();
}
void currentOnly(Editor* editor, int currentX, qreal dpr) {
    const auto image = render(editor, dpr);
    for (int x : {-200, 0, 200}) {
        const auto color = pixel(image, x, dpr);
        require(x == currentX ? color == QColor(Qt::black) : color.alpha() == 0,
                "lamp off renders only the current drawing, without black or tinted ghosts");
    }
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "isolated settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    auto* window = new MainWindow2;
    window->resize(1440, 900); window->show(); app.processEvents();
    auto* editor = window->mEditor;
    auto* layer = dynamic_cast<LayerBitmap*>(editor->layers()->currentLayer());
    require(layer != nullptr, "real bitmap layer");
    for (int frame : {5, 9}) require(layer->addNewKeyFrameAt(frame), "independent drawing added");
    const QList<int> frames{1, 5, 9};
    const QList<int> positions{-200, 0, 200};
    for (int i = 0; i < frames.size(); ++i) {
        const auto* key = dynamic_cast<BitmapKeyFrame*>(layer->getKeyFrameAt(frames[i]));
        require(key != nullptr, "drawing has a cell");
        auto* stack = layer->cellBank()->getCellLayerStack(key->cellId());
        stack->activeLayer()->image->drawRect(QRectF(positions[i] - 15, -15, 30, 30),
            Qt::NoPen, QBrush(Qt::black), QPainter::CompositionMode_SourceOver, false);
        stack->invalidateComposite();
    }
    auto* manager = editor->shiftTrace();
    for (const auto entry : manager->entries()) {
        manager->setEntryVisible(entry.id, qAbs(entry.exposureOffset) == 1);
        manager->setEntryOpacity(entry.id, entry.exposureOffset < 0 ? 41 : 30);
    }
    manager->setPrevTintEnabled(true); manager->setPrevTintColor(Qt::blue);
    manager->setNextTintEnabled(true); manager->setNextTintColor(QColor(255, 136, 0));
    manager->setHideAll(false);
    editor->scrubTo(5); app.processEvents();
    auto* prefs = editor->preference();
    prefs->set(SETTING::PREV_ONION, true); prefs->set(SETTING::NEXT_ONION, true);
    QAbstractButton* lamp = nullptr;
    for (auto* button : window->findChildren<QAbstractButton*>())
        if (button->toolTip() == "Toggle Shift and Trace") lamp = button;
    require(lamp && lamp->isChecked(), "actual timeline lamp is enabled");
    auto* previous = window->findChild<QAction*>("actionOnionPrev");
    auto* next = window->findChild<QAction*>("actionOnionNext");
    require(previous && next, "actual previous/next menu and keyboard actions");

    for (qreal dpr : {1.0, 2.0}) {
        auto image = render(editor, dpr);
        const auto prev = pixel(image, -200, dpr);
        const auto future = pixel(image, 200, dpr);
        require(prev.blue() > 250 && prev.red() == 0 && prev.alpha() >= 102 && prev.alpha() <= 105,
                "previous ghost has exactly the configured blue tint and opacity, without a black duplicate");
        require(future.red() > 250 && future.blue() == 0 && future.alpha() >= 75 && future.alpha() <= 78,
                "next ghost has exactly the configured tint and opacity");
        require(pixel(image, 0, dpr) == QColor(Qt::black), "current drawing stays fully black");
    }
    // Warm real canvas caches before using the same button the user clicks.
    auto* area = editor->getScribbleArea();
    area->grab(); editor->scrubTo(9); app.processEvents(); area->grab();
    editor->scrubTo(5); app.processEvents();
    lamp->click(); app.processEvents();
    require(manager->hideAll(), "actual lamp disables onion skins");
    require(!previous->isChecked() && !next->isChecked(), "menu actions reflect the master lamp");
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i : {0, 1, 2, 1}) {
            editor->scrubTo(frames[i]); app.processEvents();
            currentOnly(editor, positions[i], 1);
            currentOnly(editor, positions[i], 2);
        }
    }
    editor->scrubTo(5); app.processEvents();
    const auto off = area->grab().toImage();
    lamp->click(); app.processEvents(); area->grab();
    lamp->click(); app.processEvents();
    require(area->grab().toImage() == off, "real canvas returns to identical pixels after toggling lamp on/off");
    previous->trigger(); app.processEvents();
    require(!manager->hideAll() && lamp->isChecked() && previous->isChecked(),
            "previous menu/keyboard action enables the same lamp and configured slots");
    require(pixel(render(editor), -200).blue() > 250, "previous menu action uses the tinted slot renderer");
    previous->trigger(); app.processEvents();
    require(pixel(render(editor), -200).alpha() == 0, "previous menu action hides previous slots without black fallback");
    require(pixel(render(editor), 200).alpha() > 0, "previous toggle preserves next slots");
    previous->trigger(); app.processEvents();
    editor->scrubTo(6); app.processEvents();
    require(pixel(render(editor), 0) == QColor(Qt::black), "held exposure keeps its current drawing");
    require(pixel(render(editor), -200).alpha() <= 105, "held exposure does not duplicate the preceding ghost");
    const auto playing = render(editor, 1, true);
    require(pixel(playing, -200).alpha() == 0 && pixel(playing, 200).alpha() == 0,
            "playback omits onion skins");
    const auto* key = static_cast<BitmapKeyFrame*>(layer->getKeyFrameAt(5));
    auto* actual = layer->cellBank()->getActiveInnerLayerImage(key->cellId());
    require(QColor::fromRgba(actual->pixel(0, 0)) == QColor(Qt::black), "onion toggles preserve the original drawing pixels");
    std::printf("PASS: %d onion skin integration checks\n", checks); std::fflush(stdout);
    std::_Exit(0);
}
