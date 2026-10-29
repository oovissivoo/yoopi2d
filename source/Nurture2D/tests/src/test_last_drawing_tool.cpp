#include <QApplication>
#include <QAction>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QSettings>
#include <cstdio>
#include <cstdlib>
#include "mainwindow2.h"
#include "editor.h"
#include "toolmanager.h"
#include "basetool.h"
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    auto* window = new MainWindow2;
    window->show(); app.processEvents();
    auto* tools = window->mEditor->tools();
    auto* action = window->findChild<QAction*>("returnToLastDrawingTool");
    if (!action || action->shortcut() != QKeySequence(Qt::Key_B)) return 1;
    for (ToolType drawing : {PENCIL, PEN, POLYLINE, CURVE, BSPLINE}) {
        tools->setCurrentTool(drawing);
        for (ToolType temporaryTool : {ERASER, HAND, EYEDROPPER, SELECT}) {
            tools->setCurrentTool(temporaryTool);
            action->trigger();
            if (tools->currentTool()->type() != drawing) return 2;
        }
    }
    tools->setCurrentTool(PENCIL); tools->setCurrentTool(ERASER);
    QKeyEvent reserve(QEvent::ShortcutOverride, 0x0418, Qt::NoModifier, 0, 11, 0);
    QApplication::sendEvent(window, &reserve);
    QKeyEvent press(QEvent::KeyPress, 0x0418, Qt::NoModifier, 0, 11, 0);
    QApplication::sendEvent(window, &press);
    if (tools->currentTool()->type() != PENCIL) return 3;
    tools->setCurrentTool(ERASER);
    QLineEdit field(window); field.show(); field.setFocus(); app.processEvents();
    QKeyEvent text(QEvent::KeyPress, 0x0418, Qt::NoModifier, 0, 11, 0, QString(QChar(0x0418)));
    QApplication::sendEvent(&field, &text);
    if (tools->currentTool()->type() != ERASER) return 4;
    std::puts("PASS: last drawing tool, five drawing tools, four temporary tools, physical B and text input");
    std::fflush(stdout); std::_Exit(0);
}
