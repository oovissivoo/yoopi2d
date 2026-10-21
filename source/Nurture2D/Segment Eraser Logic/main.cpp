#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("Vector Drawing App");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("SEGAL");

    MainWindow window;
    window.show();

    return app.exec();
}
