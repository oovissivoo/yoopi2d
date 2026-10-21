#include <QApplication>
#include <QPalette>
#include <QColor>
#include "mainwindow.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    qDebug() << "Starting Paintly application";
    
    QApplication app(argc, argv);
    qDebug() << "QApplication created";
    
    // Set dark theme
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    
    app.setPalette(darkPalette);
    
    MainWindow mainWindow;
    qDebug() << "MainWindow created";
    
    mainWindow.show();
    qDebug() << "MainWindow shown";
    
    return app.exec();
} 