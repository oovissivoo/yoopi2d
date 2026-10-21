// Regression coverage for the real window, QAction routes and physical macOS keys.
// Link with the app's objects via tests/keyframe_navigation.mk. No user settings
// are read or changed: the runner uses temporary INI settings and a new document.
#include <QApplication>
#include <QAction>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

#include "mainwindow2.h"
#include "editor.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "bitmapimage.h"
#include "filemanager.h"
#include "layermanager.h"
#include "pencilsettings.h"
#include "scribblearea.h"

namespace {
int checks = 0;

void require(bool ok, const char* description)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
    ++checks;
}

void expectFrame(Editor* editor, int expected, const char* description)
{
    if (editor->currentFrame() != expected) {
        std::fprintf(stderr, "FAIL: %s: expected %d, got %d\n",
                     description, expected, editor->currentFrame());
        std::exit(1);
    }
    ++checks;
}

void physicalKey(QWidget* receiver, int logicalKey, quint32 virtualKey,
                 Qt::KeyboardModifiers modifiers, const QString& text)
{
    QKeyEvent claim(QEvent::ShortcutOverride, logicalKey, modifiers,
                    0, virtualKey, 0, text);
    QApplication::sendEvent(receiver, &claim);
    QKeyEvent press(QEvent::KeyPress, logicalKey, modifiers,
                    0, virtualKey, 0, text);
    QApplication::sendEvent(receiver, &press);
    QKeyEvent release(QEvent::KeyRelease, logicalKey, modifiers,
                      0, virtualKey, 0, text);
    QApplication::sendEvent(receiver, &release);
}
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QTemporaryDir settingsDirectory;
    require(settingsDirectory.isValid(), "temporary settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory.path());
    auto* window = new MainWindow2;
    window->show();
    QApplication::setActiveWindow(window);
    application.processEvents();

    Editor* editor = window->mEditor;
    auto* layer = dynamic_cast<LayerBitmap*>(editor->layers()->currentLayer());
    require(layer != nullptr, "active bitmap layer");
    require(layer->keyExists(1), "first numbered drawing");
    require(layer->addNewKeyFrameAt(6), "second drawing on timeline frame 6");
    require(layer->addNewKeyFrameAt(14), "third drawing on timeline frame 14");
    require(layer->addKeyFrame(3, new BitmapKeyFrame), "unnumbered placeholder on frame 3");

    auto* nextAction = window->findChild<QAction*>("actionNext_Frame");
    auto* previousAction = window->findChild<QAction*>("actionPrevious_Frame");
    auto* nextButton = window->findChild<QPushButton*>("nextDrawingButton");
    auto* previousButton = window->findChild<QPushButton*>("previousDrawingButton");
    require(nextAction && previousAction && nextButton && previousButton,
            "actual window actions and transport buttons");
    editor->scrubTo(1);
    nextAction->trigger();
    expectFrame(editor, 6, "ordinary Next Frame action skips empty positions");
    nextButton->click();
    expectFrame(editor, 14, "transport button reaches drawing 3");
    nextButton->click();
    expectFrame(editor, 14, "last drawing boundary");
    previousButton->click();
    expectFrame(editor, 6, "transport button reaches drawing 2");
    previousAction->trigger();
    expectFrame(editor, 1, "ordinary Previous Frame action reaches drawing 1");
    previousAction->trigger();
    expectFrame(editor, 1, "first drawing boundary");
    editor->scrubTo(9);
    previousButton->click();
    expectFrame(editor, 6, "backward navigation from a held frame");
    editor->scrubTo(9);
    nextButton->click();
    expectFrame(editor, 14, "forward navigation from a held frame");

    ScribbleArea* canvas = editor->getScribbleArea();
    canvas->setFocus();
    editor->scrubTo(1);
    physicalKey(canvas, Qt::Key_Right, 124, Qt::NoModifier, QString());
    expectFrame(editor, 2, "right arrow advances one timeline frame");
    physicalKey(canvas, Qt::Key_Right, 124, Qt::NoModifier, QString());
    expectFrame(editor, 3, "right arrow visits an unnumbered position");
    physicalKey(canvas, Qt::Key_Right, 124, Qt::NoModifier, QString());
    expectFrame(editor, 4, "right arrow visits a held frame");
    physicalKey(canvas, Qt::Key_Left, 123, Qt::NoModifier, QString());
    expectFrame(editor, 3, "left arrow steps back one frame");
    physicalKey(canvas, Qt::Key_Left, 123, Qt::NoModifier, QString());
    expectFrame(editor, 2, "left arrow visits every intermediate frame");
    physicalKey(canvas, Qt::Key_Left, 123, Qt::NoModifier, QString());
    expectFrame(editor, 1, "left arrow reaches first frame");
    physicalKey(canvas, Qt::Key_Left, 123, Qt::NoModifier, QString());
    expectFrame(editor, 1, "left arrow stops at first frame");
    editor->scrubTo(9);
    physicalKey(canvas, Qt::Key_Right, 124, Qt::NoModifier, QString());
    expectFrame(editor, 10, "right arrow stays sequential between drawings");
    physicalKey(canvas, Qt::Key_Left, 123, Qt::NoModifier, QString());
    expectFrame(editor, 9, "left arrow stays sequential between drawings");

#ifdef Q_OS_MACOS
    const struct { int next; int previous; const char* name; } layouts[] = {
        {Qt::Key_Period, Qt::Key_Comma, "Latin"},
        {0x042e, 0x0411, "Cyrillic"},
        {0x0632, 0x0648, "Arabic"},
        {Qt::Key_Semicolon, Qt::Key_Comma, "French"},
    };
    for (const auto& layout : layouts) {
        for (Qt::KeyboardModifiers modifiers : {
                 Qt::KeyboardModifiers(Qt::NoModifier), Qt::KeyboardModifiers(Qt::ShiftModifier),
                 Qt::KeyboardModifiers(Qt::AltModifier)}) {
            editor->scrubTo(1);
            physicalKey(canvas, layout.next, 47, modifiers, QString(QChar(layout.next)));
            expectFrame(editor, 6, layout.name);
            physicalKey(canvas, layout.next, 47, modifiers, QString(QChar(layout.next)));
            expectFrame(editor, 14, "one keypress causes exactly one drawing jump");
            physicalKey(canvas, layout.previous, 43, modifiers, QString(QChar(layout.previous)));
            expectFrame(editor, 6, "physical comma reaches middle drawing");
            physicalKey(canvas, layout.previous, 43, modifiers, QString(QChar(layout.previous)));
            expectFrame(editor, 1, "physical comma reaches first drawing");
        }
    }

    auto* input = new QLineEdit(window);
    input->show();
    input->setFocus();
    editor->scrubTo(6);
    physicalKey(input, 0x042e, 47, Qt::NoModifier, QString::fromUtf8("ю"));
    expectFrame(editor, 6, "typing in a text field does not navigate");
    require(input->text() == QString::fromUtf8("ю"), "text field receives the layout's character");
    input->hide();
    canvas->setFocus();

    pencilSettings().setValue("shortcuts/CmdGotoNextFrame", QString());
    nextAction->setShortcut(QKeySequence());
    editor->scrubTo(1);
    physicalKey(canvas, 0x042e, 47, Qt::NoModifier, QString::fromUtf8("ю"));
    expectFrame(editor, 1, "disabled/customized ordinary shortcut is respected");
#endif

    if (argc == 2) {
        // Optional disposable document for a follow-up Cocoa/trackpad UI check.
        layer->removeKeyFrame(3);
        int offset = 0;
        for (int frame : {1, 6, 14}) {
            layer->getBitmapImageAtFrame(frame)->drawEllipse(
                QRectF(-160 + offset, -100, 160, 160), QPen(Qt::black, 5),
                Qt::NoBrush, QPainter::CompositionMode_SourceOver, true);
            layer->setModified(frame, true);
            offset += 130;
        }
        editor->scrubTo(1);
        FileManager files;
        require(files.save(editor->object(), QString::fromLocal8Bit(argv[1])).ok(),
                "save disposable navigation fixture");
    }

    std::printf("PASS: %d checks: actions, buttons, gaps, boundaries, arrows, physical keys, layouts, text focus.\n", checks);
    std::fflush(stdout);
    settingsDirectory.remove();
    // Do not invoke the document close workflow for this disposable test window.
    std::_Exit(0);
}
