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
    auto* increase = window->findChild<QAction*>("increaseToolSize");
    auto* decrease = window->findChild<QAction*>("decreaseToolSize");
    if (!increase || !decrease || increase->shortcuts().isEmpty() || decrease->shortcuts().isEmpty()) return 1;
    for (ToolType type : {PENCIL, PEN, ERASER, POLYLINE, CURVE, BSPLINE}) {
        tools->setCurrentTool(type);
        tools->setWidth(10);
        increase->trigger();
        if (tools->currentTool()->properties.width != 11) return 2;
        decrease->trigger();
        if (tools->currentTool()->properties.width != 10) return 3;
        tools->setWidth(1); decrease->trigger();
        if (tools->currentTool()->properties.width < 1) return 4;
    }
    tools->setCurrentTool(PENCIL); tools->setWidth(10);
    for (auto pair : {qMakePair(30, 0x042A), qMakePair(33, 0x0425)}) {
        QKeyEvent reserve(QEvent::ShortcutOverride, pair.second, Qt::NoModifier, 0, pair.first, 0);
        QApplication::sendEvent(window, &reserve);
        QKeyEvent press(QEvent::KeyPress, pair.second, Qt::NoModifier, 0, pair.first, 0);
        QApplication::sendEvent(window, &press);
    }
    if (tools->currentTool()->properties.width != 10) return 5;
    std::puts("PASS: size up/down for six tools, minimum size and physical bracket keys");
    std::fflush(stdout); std::_Exit(0);
}
