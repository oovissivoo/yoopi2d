#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QScreen>
#include <QStyleFactory>

#include "mainwindow.h"

static void applyDarkPalette()
{
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    const QColor bgColor("#353535");
    const QColor windowTextColor(Qt::white);
    const QColor baseColor("#2c2c2c");
    const QColor alternateBaseColor("#404040");
    const QColor textColor(Qt::white);
    const QColor highlightColor("#5c90ff");

    darkPalette.setColor(QPalette::Window, bgColor);
    darkPalette.setColor(QPalette::WindowText, windowTextColor);
    darkPalette.setColor(QPalette::Base, baseColor);
    darkPalette.setColor(QPalette::AlternateBase, alternateBaseColor);
    darkPalette.setColor(QPalette::Text, textColor);
    darkPalette.setColor(QPalette::Button, bgColor);
    darkPalette.setColor(QPalette::ButtonText, textColor);
    darkPalette.setColor(QPalette::Highlight, highlightColor);
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::black);

    QApplication::setPalette(darkPalette);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    applyDarkPalette();

    MainWindow window;

    if (auto *screen = QGuiApplication::primaryScreen()) {
        window.resize(screen->availableGeometry().size());
        window.move(screen->availableGeometry().topLeft());
    }

    window.show();
    return app.exec();
}
