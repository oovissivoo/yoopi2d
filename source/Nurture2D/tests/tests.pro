#-------------------------------------------------
#
# Unit Test Project of Pencil2D
#
#-------------------------------------------------

! include( ../util/common.pri ) { error( Could not find the common.pri file! ) }

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
QT += core widgets gui xml multimedia svg concurrent

TARGET = tests

RESOURCES += data/tests.qrc

INCLUDEPATH += \
    ../core_lib/src/graphics \
    ../core_lib/src/graphics/bitmap \
    ../core_lib/src/graphics/vector \
    ../core_lib/src/interface \
    ../core_lib/src/structure \
    ../core_lib/src/tool \
    ../core_lib/src/util \
    ../core_lib/ui \
    ../core_lib/src/managers

HEADERS += \
    src/catch.hpp

SOURCES += \
    src/main.cpp \
    src/test_colormanager.cpp \
    src/test_layer.cpp \
    src/test_layerbitmap.cpp \
    src/test_layercamera.cpp \
    src/test_layermanager.cpp \
    src/test_layersound.cpp \
    src/test_layervector.cpp \
    src/test_object.cpp \
    src/test_filemanager.cpp \
    src/test_bitmapimage.cpp \
    src/test_bitmapbucket.cpp \
    src/test_cellbank.cpp \
    src/test_vectorimage.cpp \
    src/test_viewmanager.cpp \
    src/test_shared_undo.cpp

# --- core_lib ---

INCLUDEPATH += $$PWD/../core_lib/src

BUILDTYPE =
debug_and_release:CONFIG(debug,debug|release) BUILDTYPE = debug
debug_and_release:CONFIG(release,debug|release) BUILDTYPE = release

win32-msvc* {
    LIBS += -L$$OUT_PWD/../core_lib/$$BUILDTYPE/ -lcore_lib
    PRE_TARGETDEPS += $$OUT_PWD/../core_lib/$$BUILDTYPE/core_lib.lib
}

win32-g++ {
    LIBS += -L$$OUT_PWD/../core_lib/$$BUILDTYPE/ -lcore_lib
    PRE_TARGETDEPS += $$OUT_PWD/../core_lib/$$BUILDTYPE/libcore_lib.a
}

# --- mac os and linux
unix {
    LIBS += -L$$OUT_PWD/../core_lib/$$BUILDTYPE/ -lcore_lib
    PRE_TARGETDEPS += $$OUT_PWD/../core_lib/$$BUILDTYPE/libcore_lib.a
}

SOURCES += src/test_lasso_paste_bug.cpp \
           src/test_bitmap_undo.cpp \
           src/test_undo_integration.cpp \
           src/test_undo_timeline_ops.cpp \
           src/test_pencil_preset_activation_bug.cpp \
           src/test_pencil_preset_preservation.cpp \
           src/test_celllayerstack.cpp \
           src/test_cellbank_innerlayers.cpp \
           src/test_filemanager_innerlayers.cpp \
           src/test_cosmiclayerspane.cpp

# --- app include path (for cosmiclayerspane.h header reference in tests) ---
INCLUDEPATH += $$PWD/../app/src
