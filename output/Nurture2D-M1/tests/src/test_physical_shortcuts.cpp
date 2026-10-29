#include <QApplication>
#include <QAction>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include "mainwindow2.h"
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    auto* window = new MainWindow2;
    window->show(); app.processEvents();
    int count = 0;
    // A unique combination avoids interference with the installed default bindings.
    QAction action(window);
    action.setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    window->addAction(&action);
    QObject::connect(&action, &QAction::triggered, [&]{ ++count; });
    auto send = [&](QWidget* target, int logical) {
        QKeyEvent reserve(QEvent::ShortcutOverride, logical, Qt::ControlModifier | Qt::AltModifier,
                          0, 11, 0);
        QApplication::sendEvent(target, &reserve);
        QKeyEvent press(QEvent::KeyPress, logical, Qt::ControlModifier | Qt::AltModifier,
                        0, 11, 0);
        QApplication::sendEvent(target, &press);
    };
    for (int logical : {0x0418, 0x0628, 0x05E0, int(Qt::Key_N)}) send(window, logical);
    if (count != 4) { std::fprintf(stderr, "FAIL: layout commands %d\n", count); return 1; }
    action.setEnabled(false); send(window, 0x0418);
    if (count != 4) return 2;
    action.setEnabled(true);
    QLineEdit input(window); input.show(); input.setFocus(); app.processEvents();
    send(&input, 0x0418);
    if (count != 4) return 3;
    std::puts("PASS: physical shortcuts across four layouts, disabled action and text focus");
    std::fflush(stdout); std::_Exit(0);
}
