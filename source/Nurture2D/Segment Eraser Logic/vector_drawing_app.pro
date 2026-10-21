# Vector Drawing Application Project File

QT += core gui widgets

TARGET = vector_drawing_app
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    drawingcanvas.cpp \
    colorwheel.cpp

HEADERS += \
    mainwindow.h \
    drawingcanvas.h \
    colorwheel.h

RESOURCES += \
    resources.qrc

# Include icons directory
INCLUDEPATH += .

# Enable C++11 features
CONFIG += c++11
