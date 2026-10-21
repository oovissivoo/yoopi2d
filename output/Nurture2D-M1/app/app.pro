#-------------------------------------------------
#
# Pencil2D GUI
#
#-------------------------------------------------

! include( ../util/common.pri ) { error( Could not find the common.pri file! ) }

TEMPLATE = app
CONFIG += precompile_header lrelease embed_translations
QT += core widgets gui xml multimedia svg network concurrent

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG += -g3 -O0
    QMAKE_CFLAGS_DEBUG += -g3 -O0
    QMAKE_LFLAGS_DEBUG += -g3
}

CONFIG(release, debug|release) {
    # Link-time optimization for smaller, faster release binary
    !win32: QMAKE_CXXFLAGS_RELEASE += -flto
    !win32: QMAKE_CFLAGS_RELEASE   += -flto
    !win32: QMAKE_LFLAGS_RELEASE   += -flto
}

TARGET = nurture2d

RESOURCES += data/app.qrc

# --- CosmicDefaultTip generation ---
# macOS uses tools/generate_brush_tips/generate_brush_tips.pro through
# build-macos.command so qmake supplies the correct Qt framework/SDK flags.
!macx {
# Compile and run the generator tool at build time so the 4 tip PNGs are
# baked into BrushTips/ before the .qrc is processed. The app loads them
# as plain resources — zero runtime generation cost.
BRUSH_TIP_TOOL_SRC = $$PWD/../tools/generate_brush_tips/generate_brush_tips.cpp
BRUSH_TIP_TOOL_BIN = $$OUT_PWD/generate_brush_tips
BRUSH_TIP_OUT_DIR  = $$PWD/../BrushTips

# Step 1: compile the generator
brush_tip_compile.target   = $$BRUSH_TIP_TOOL_BIN
brush_tip_compile.commands = $$QMAKE_CXX $$QMAKE_CXXFLAGS -fPIC \
    -I$$[QT_INSTALL_HEADERS] \
    -I$$[QT_INSTALL_HEADERS]/QtCore \
    -I$$[QT_INSTALL_HEADERS]/QtGui \
    $$BRUSH_TIP_TOOL_SRC \
    -L$$[QT_INSTALL_LIBS] -lQt5Core -lQt5Gui \
    -o $$BRUSH_TIP_TOOL_BIN
brush_tip_compile.depends  = $$BRUSH_TIP_TOOL_SRC

# Step 2: run the generator, output PNGs into BrushTips/
brush_tip_generate.target   = $$BRUSH_TIP_OUT_DIR/CosmicDefaultTip_Medium.png
brush_tip_generate.commands = $$BRUSH_TIP_TOOL_BIN $$BRUSH_TIP_OUT_DIR
brush_tip_generate.depends  = $$BRUSH_TIP_TOOL_BIN

QMAKE_EXTRA_TARGETS += brush_tip_compile brush_tip_generate
PRE_TARGETDEPS      += $$BRUSH_TIP_OUT_DIR/CosmicDefaultTip_Medium.png
}

MUI_TRANSLATIONS += \
        translations/mui_cs.po \
        translations/mui_de.po \
        translations/mui_it.po

RC_LANGS.cs = --lang LANG_CZECH --sublang SUBLANG_NEUTRAL
RC_LANGS.de = --lang LANG_GERMAN --sublang SUBLANG_NEUTRAL
RC_LANGS.it = --lang LANG_ITALIAN --sublang SUBLANG_NEUTRAL

EXTRA_TRANSLATIONS += \
	$$PWD/../translations/pencil_ar.ts \
	$$PWD/../translations/pencil_bg.ts \
	$$PWD/../translations/pencil_ca.ts \
	$$PWD/../translations/pencil_cs.ts \
	$$PWD/../translations/pencil_da.ts \
	$$PWD/../translations/pencil_de.ts \
	$$PWD/../translations/pencil_el.ts \
	$$PWD/../translations/pencil_en.ts \
	$$PWD/../translations/pencil_es.ts \
	$$PWD/../translations/pencil_et.ts \
	$$PWD/../translations/pencil_fa.ts \
	$$PWD/../translations/pencil_fr.ts \
	$$PWD/../translations/pencil_he.ts \
	$$PWD/../translations/pencil_hu_HU.ts \
	$$PWD/../translations/pencil_id.ts \
	$$PWD/../translations/pencil_it.ts \
	$$PWD/../translations/pencil_ja.ts \
	$$PWD/../translations/pencil_kab.ts \
	$$PWD/../translations/pencil_ko.ts \
	$$PWD/../translations/pencil_nb.ts \
	$$PWD/../translations/pencil_nl_NL.ts \
	$$PWD/../translations/pencil_pl.ts \
	$$PWD/../translations/pencil_pt.ts \
	$$PWD/../translations/pencil_pt_BR.ts \
	$$PWD/../translations/pencil_ru.ts \
	$$PWD/../translations/pencil_sl.ts \
	$$PWD/../translations/pencil_sv.ts \
	$$PWD/../translations/pencil_tr.ts \
	$$PWD/../translations/pencil_vi.ts \
	$$PWD/../translations/pencil_yue.ts \
	$$PWD/../translations/pencil_zh_CN.ts \
	$$PWD/../translations/pencil_zh_TW.ts

INCLUDEPATH += \
    src \
    ../CUI \
    ../core_lib/src/graphics \
    ../core_lib/src/graphics/bitmap \
    ../core_lib/src/graphics/vector \
    ../core_lib/src/interface \
    ../core_lib/src/structure \
    ../core_lib/src/tool \
    ../core_lib/src/util \
    ../core_lib/src/managers \
    ../core_lib/src/external \
    ../core_lib/src \
    ../core_lib/ui

PRECOMPILED_HEADER = src/app-pch.h

HEADERS += \
    src/addtransparencytopaperdialog.h \
    src/app-pch.h \
    src/appearance.h \
    src/buttonappearancewatcher.h \
    src/importlayersdialog.h \
    src/importpositiondialog.h \
    src/layeropacitydialog.h \
    src/mainwindow2.h \
    src/onionskinwidget.h \
    src/shifttracewidget.h \
    src/cellstackwidget.h \
    src/cellbankfloatingpanel.h \
    src/predefinedsetmodel.h \
    src/pegbaralignmentdialog.h \
    \
    src/actioncommands.h \
    src/preferencesdialog.h \
    src/filespage.h \
    src/generalpage.h \
    src/shortcutspage.h \
    src/timelinepage.h \
    src/colorspage.h \
    src/toggletapspage.h \
    src/toolboxwidget.h \
    src/pencolorpalette.h \
    src/dplatecolorpalette.h \
    src/toolspage.h \
    src/titlebarwidget.h \
    src/cosmicbottomtoolsbar.h \
    src/cosmicwarningpopup.h \
    src/cosmicconfirmpopup.h \
    src/cosmicautosavepopup.h \
    src/cosmicinputpopup.h \
    src/cosmiccanvassizepopup.h \
    src/cosmicextraspacepopup.h \
    src/cosmicexportimagepopup.h \
    src/cosmicexportmoviepopup.h \
    src/cosmicexportanimationpopup.h \
    src/cosmicexportspritepopup.h \
    src/cosmicexportgifpopup.h \
    src/gifexporter.h \
    src/gifexportdesc.h \
    src/cosmicimportlayerspopup.h \
    src/cosmicaboutpopup.h \
    src/cosmiccheckupdatespopup.h \
    src/cosmiccamerapropertiespopup.h \
    src/cosmiclayeropacitypopup.h \
    src/basedockwidget.h \
    src/colorbox.h \
    src/colorslidersdock.h \
    src/colorinspector.h \
    src/colorpalettewidget.h \
    src/colorwheel.h \
    src/timeline.h \
    src/timelinecells.h \
    src/timecontrols.h \
    src/cameracontextmenu.h \
    src/camerapropertiesdialog.h \
    src/filedialog.h \
    src/pencil2d.h \
    src/exportmoviedialog.h \
    src/exportanimationdialog.h \
    src/app_util.h \
    src/errordialog.h \
    src/aboutdialog.h \
    src/toolbox.h \
    src/tooloptionwidget.h \
    src/curvetapercontrol.h \
    src/bucketoptionswidget.h \
    src/autolinecolortoggle.h \
    src/importexportdialog.h \
    src/exportimagedialog.h \
    src/importimageseqdialog.h \
    src/spinslider.h \
    src/doubleprogressdialog.h \
    src/colorslider.h \
    src/checkupdatesdialog.h \
    src/presetdialog.h \
    src/repositionframesdialog.h \
    src/commandlineparser.h \
    src/commandlineexporter.h \
    src/elidedlabel.h \
    src/cameraoptionswidget.h \
    src/popupcolorwheelwidget.h \
    src/popupcolorpicker.h \
    src/smoothloadingbar.h \
    $$PWD/../paintlyFillAlgorithm/blenderstyleslider.h \
    $$PWD/../CUI/blenderstylecombobox.h \
    $$PWD/../CUI/blenderstyledropdown.h \
    $$PWD/../CUI/blenderstyletoggle.h \
    $$PWD/../CUI/blenderstylemodalpopup.h \
    $$PWD/../CUI/blenderstylevaluefield.h \
    $$PWD/../CUI/blenderstylebutton.h \
    $$PWD/../CUI/cosmicdropdown.h \
    $$PWD/../CUI/cosmicvslider.h \
    $$PWD/../CUI/cosmichslider.h \
    $$PWD/../CUI/cosmicbutton.h \
    $$PWD/../CUI/cosmiccheckboxtoggle.h \
    $$PWD/../CUI/cosmicintensitybox.h \
    $$PWD/../CUI/cosmictappersliders.h \
    $$PWD/../CUI/cosmicinputfield.h \
    $$PWD/../CUI/cosmicnumericfieldrenderer.h \
    $$PWD/../CUI/cosmicignorecolor.h \
    $$PWD/../CUI/cosmictext.h \
    $$PWD/../CUI/cosmicmenuwindow.h \
    $$PWD/../CUI/cosmicmenubar.h \
    $$PWD/../CUI/cosmiccontrolcenteritem.h \
    $$PWD/../CUI/cosmiccontrolcenterlayout.h \
    $$PWD/../CUI/cosmiccontrolcenter.h \
    $$PWD/../CUI/cosmicenumselector.h \
    $$PWD/../CUI/cosmicprogressbar.h \
    $$PWD/../CUI/cosmicslidercard.h \
    $$PWD/../CUI/cosmicassetgallery.h \
    src/referencewindow.h \
    src/referencecanvas.h \
    src/beziercurveeditor.h \
    src/shortcutfilter.h \
    src/inlinecolorinspector.h \
    src/canvaspaletteoverlay.h \
    src/canvastimecodeoverlay.h \
    src/canvaskeyframescrubber.h \
    src/canvasgalleryoverlay.h \
    src/timelinesettingsdialog.h \
    src/timelinesettingspopup.h \
    src/ignorefillsplittoggle.h \
    src/brushfactorydock.h \
    src/cosmiclefttoolsbar.h \
    src/cosmictoolspane.h \
    src/cosmiclefttoolpanel.h \
    src/cosmiclayerspane.h \
    src/cosmicrighttoolsbar.h \
    src/cosmicrighttoolpanel.h \
    src/cosmiccolorwheelwidget.h \
    src/cosmiccolorsliders.h \
    src/brushpresetlistwidget.h \
    src/cosmicbrushsettings.h \
    src/managers/brushpresetstore.h \
    src/managers/brushassetstore.h \
    src/scenesequencemanager.h \
    src/n2dprojectio.h \
    src/cosmicpopupwindow.h \
    src/cosmicsaveprogresspopup.h \
    src/cosmicprogresspopup.h \
    src/cosmicfilebrowserpopup.h \
    src/cosmicrenameoperator.h \
    src/cosmicwindowheader.h \
    src/cosmicpopupcontextmenu.h \
    src/windowsscrollbarspage.h

SOURCES += \
    src/addtransparencytopaperdialog.cpp \
    src/buttonappearancewatcher.cpp \
    src/importlayersdialog.cpp \
    src/importpositiondialog.cpp \
    src/layeropacitydialog.cpp \
    src/main.cpp \
    src/mainwindow2.cpp \
    src/onionskinwidget.cpp \
    src/shifttracewidget.cpp \
    src/cellstackwidget.cpp \
    src/cellbankfloatingpanel.cpp \
    src/predefinedsetmodel.cpp \
    src/pegbaralignmentdialog.cpp \
    src/cosmicbottomtoolsbar.cpp \
    src/cosmicwarningpopup.cpp \
    src/cosmicconfirmpopup.cpp \
    src/cosmicautosavepopup.cpp \
    src/cosmicinputpopup.cpp \
    src/cosmiccanvassizepopup.cpp \
    src/cosmicextraspacepopup.cpp \
    src/cosmicexportimagepopup.cpp \
    src/cosmicexportmoviepopup.cpp \
    src/cosmicexportanimationpopup.cpp \
    src/cosmicexportspritepopup.cpp \
    src/cosmicexportgifpopup.cpp \
    src/gifexporter.cpp \
    src/cosmicimportlayerspopup.cpp \
    src/cosmicaboutpopup.cpp \
    src/cosmiccheckupdatespopup.cpp \
    src/cosmiccamerapropertiespopup.cpp \
    src/cosmiclayeropacitypopup.cpp \
    src/actioncommands.cpp \
    src/preferencesdialog.cpp \
    src/filespage.cpp \
    src/generalpage.cpp \
    src/shortcutspage.cpp \
    src/timelinepage.cpp \
    src/colorspage.cpp \
    src/toggletapspage.cpp \
    src/toolboxwidget.cpp \
    src/pencolorpalette.cpp \
    src/dplatecolorpalette.cpp \
    src/toolspage.cpp \
    src/titlebarwidget.cpp \
    src/basedockwidget.cpp \
    src/colorbox.cpp \
    src/brushpresetlistwidget.cpp \
    src/cosmicbrushsettings.cpp \
    src/managers/brushpresetstore.cpp \
    src/managers/brushassetstore.cpp \
    src/scenesequencemanager.cpp \
    src/colorslidersdock.cpp \
    src/colorinspector.cpp \
    src/colorpalettewidget.cpp \
    src/colorwheel.cpp \
    src/timeline.cpp \
    src/timelinecells.cpp \
    src/timecontrols.cpp \
    src/cameracontextmenu.cpp \
    src/camerapropertiesdialog.cpp \
    src/filedialog.cpp \
    src/pencil2d.cpp \
    src/exportmoviedialog.cpp \
    src/exportanimationdialog.cpp \
    src/errordialog.cpp \
    src/aboutdialog.cpp \
    src/toolbox.cpp \
    src/n2dprojectio.cpp \
    src/tooloptionwidget.cpp \
    src/curvetapercontrol.cpp \
    src/bucketoptionswidget.cpp \
    src/autolinecolortoggle.cpp \
    src/importexportdialog.cpp \
    src/exportimagedialog.cpp \
    src/importimageseqdialog.cpp \
    src/spinslider.cpp \
    src/doubleprogressdialog.cpp \
    src/colorslider.cpp \
    src/checkupdatesdialog.cpp \
    src/presetdialog.cpp \
    src/repositionframesdialog.cpp \
    src/app_util.cpp \
    src/commandlineparser.cpp \
    src/commandlineexporter.cpp \
    src/elidedlabel.cpp \
    src/cameraoptionswidget.cpp \
    src/popupcolorwheelwidget.cpp \
    src/popupcolorpicker.cpp \
    src/smoothloadingbar.cpp \
    $$PWD/../paintlyFillAlgorithm/blenderstyleslider.cpp \
    $$PWD/../CUI/blenderstylecombobox.cpp \
    $$PWD/../CUI/blenderstyledropdown.cpp \
    $$PWD/../CUI/blenderstyletoggle.cpp \
    $$PWD/../CUI/blenderstylemodalpopup.cpp \
    $$PWD/../CUI/blenderstylevaluefield.cpp \
    $$PWD/../CUI/blenderstylebutton.cpp \
    $$PWD/../CUI/cosmicdropdown.cpp \
    $$PWD/../CUI/cosmicvslider.cpp \
    $$PWD/../CUI/cosmichslider.cpp \
    $$PWD/../CUI/cosmicbutton.cpp \
    $$PWD/../CUI/cosmiccheckboxtoggle.cpp \
    $$PWD/../CUI/cosmicintensitybox.cpp \
    $$PWD/../CUI/cosmictappersliders.cpp \
    $$PWD/../CUI/cosmicinputfield.cpp \
    $$PWD/../CUI/cosmicnumericfieldrenderer.cpp \
    $$PWD/../CUI/cosmicignorecolor.cpp \
    $$PWD/../CUI/cosmictext.cpp \
    $$PWD/../CUI/cosmicmenuwindow.cpp \
    $$PWD/../CUI/cosmicmenubar.cpp \
    $$PWD/../CUI/cosmiccontrolcenterlayout.cpp \
    $$PWD/../CUI/cosmiccontrolcenter.cpp \
    $$PWD/../CUI/cosmicenumselector.cpp \
    $$PWD/../CUI/cosmicprogressbar.cpp \
    $$PWD/../CUI/cosmicslidercard.cpp \
    $$PWD/../CUI/cosmicassetgallery.cpp \
    src/referencewindow.cpp \
    src/referencecanvas.cpp \
    src/beziercurveeditor.cpp \
    src/shortcutfilter.cpp \
    src/inlinecolorinspector.cpp \
    src/canvaspaletteoverlay.cpp \
    src/canvastimecodeoverlay.cpp \
    src/canvaskeyframescrubber.cpp \
    src/canvasgalleryoverlay.cpp \
    src/timelinesettingsdialog.cpp \
    src/timelinesettingspopup.cpp \
    src/ignorefillsplittoggle.cpp \
    src/brushfactorydock.cpp \
    src/cosmiclefttoolsbar.cpp \
    src/cosmictoolspane.cpp \
    src/cosmiclefttoolpanel.cpp \
    src/cosmiclayerspane.cpp \
    src/cosmicrighttoolsbar.cpp \
    src/cosmicrighttoolpanel.cpp \
    src/cosmiccolorwheelwidget.cpp \
    src/cosmiccolorsliders.cpp \
    src/cosmicpopupwindow.cpp \
    src/cosmicsaveprogresspopup.cpp \
    src/cosmicprogresspopup.cpp \
    src/cosmicfilebrowserpopup.cpp \
    src/cosmicrenameoperator.cpp \
    src/cosmicwindowheader.cpp \
    src/cosmicpopupcontextmenu.cpp \
    src/windowsscrollbarspage.cpp \
    # Include core_lib manager sources so lupdate picks up their tr() contexts
    ../core_lib/src/managers/undoredomanager.cpp \
    \

FORMS += \
    ui/addtransparencytopaperdialog.ui \
    ui/cameraoptionswidget.ui \
    ui/camerapropertiesdialog.ui \
    ui/importimageseqpreview.ui \
    ui/importlayersdialog.ui \
    ui/importpositiondialog.ui \
    ui/layeropacitydialog.ui \
    ui/mainwindow2.ui \
    ui/onionskin.ui \
    ui/pegbaralignmentdialog.ui \
    ui/repositionframesdialog.ui \
    ui/shortcutspage.ui \
    ui/colorinspector.ui \
    ui/colorpalette.ui \
    ui/errordialog.ui \
    ui/importexportdialog.ui \
    ui/exportmovieoptions.ui \
    ui/exportimageoptions.ui \
    ui/exportanimationoptions.ui \
    ui/importimageseqoptions.ui \
    ui/tooloptions.ui \
    ui/bucketoptionswidget.ui \
    ui/aboutdialog.ui \
    ui/doubleprogressdialog.ui \
    ui/preferencesdialog.ui \
    ui/generalpage.ui \
    ui/timelinepage.ui \
    ui/filespage.ui \
    ui/toolspage.ui \
    ui/colorspage.ui \
    ui/toggletapspage.ui \
    ui/toolboxwidget.ui \
    ui/presetdialog.ui

GIT {
    DEFINES += GIT_EXISTS \
    "GIT_CURRENT_SHA1=$$system(git --git-dir=.git --work-tree=. -C $$_PRO_FILE_PWD_/../ rev-parse HEAD)" \
    "GIT_TIMESTAMP=$$system(git --git-dir=.git --work-tree=. -C $$_PRO_FILE_PWD_/../ log -n 1 --pretty=format:"%cd" --date=format:"%Y-%m-%d_%H:%M:%S")"
}

macx {
    RC_FILE = data/pencil2d.icns

    # Use custom Info.plist
    QMAKE_INFO_PLIST = data/Info.plist

    # Add file icons into the application bundle resources
    FILE_ICONS.files = data/icons/mac_pcl_icon.icns data/icons/mac_pclx_icon.icns
    FILE_ICONS.path = Contents/Resources
    QMAKE_BUNDLE_DATA += FILE_ICONS

    QMAKE_TARGET_BUNDLE_PREFIX += org.nurture2d
    QMAKE_APPLICATION_BUNDLE_NAME = Nurture2D
}

win32 {
    target.path = /
    visualelements.path = /
    visualelements.files = data/pencil2d.VisualElementsManifest.xml $$OUT_PWD\resources.pri
    visualelements.CONFIG += no_check_exist
    visualelements.depends += resources.pri
    resources.path = /resources
    resources.files = data/resources/*

    PRI_CONFIG = data/resources.xml
    PRI_INDEX_NAME = Pencil2D
    RC_FILES = data/version.rc data/mui.rc
    INSTALLS += target visualelements resources

    makepri.name = makepri
    makepri.input = PRI_CONFIG
    makepri.output = ${QMAKE_FILE_IN_BASE}.pri
    makepri.commands = makepri new /o /in $$PRI_INDEX_NAME /pr ${QMAKE_FILE_PATH} /cf ${QMAKE_FILE_IN} /of ${QMAKE_FILE_OUT}
    silent: makepri.commands = @echo makepri ${QMAKE_FILE_IN} && $$makepri.commands
    makepri.CONFIG = no_link
    QMAKE_EXTRA_COMPILERS += makepri

    ensurePathEnv()
    isEmpty(PO2RC): for(dir, QMAKE_PATH_ENV) {
        exists("$$dir/po2rc.exe") {
            PO2RC = "$$dir/po2rc.exe"
            break()
        }
    }
    !isEmpty(PO2RC) {
        defineReplace(rcLang) {
            name = $$basename(1)
            base = $$section(name, ., 0, -2)
            return($$member(RC_LANGS.$$section(base, _, 1), 0, -1))
        }
        po2rc.name = po2rc
        po2rc.input = MUI_TRANSLATIONS
        po2rc.output = ${QMAKE_FILE_IN_BASE}.rc
        po2rc.commands = $$shell_path($$PO2RC) -t $$PWD/data/mui.rc ${QMAKE_FILE_IN} ${QMAKE_FUNC_FILE_IN_rcLang} ${QMAKE_FILE_OUT}
        silent: makepri.commands = @echo po2rc ${QMAKE_FILE_IN} && $$makepri.commands
        po2rc.CONFIG = no_link
        QMAKE_EXTRA_COMPILERS += po2rc
        # variable_out doesn't seem to work in this case
        for(file, MUI_TRANSLATIONS): {
            name = $$basename(file)
            RC_FILES += $$replace(name, .po, .rc)
        }
    } else {
        warning("po2rc was not found. MUI resources will not be translated. You can safely ignore this warning if you do not plan to distribute this build of Pencil2D through its installer.")
    }

    for(file, RC_FILES): RC_INCLUDES += "$${LITERAL_HASH}include \"$$file\""
    write_file($$OUT_PWD/pencil2d.rc, RC_INCLUDES)|error()
    RC_FILE = $$OUT_PWD/pencil2d.rc
    RC_INCLUDEPATH += $$PWD $$PWD/data
}

unix:!macx {
    target.path = $${PREFIX}/bin

    bashcompletion.files = data/nurture2d
    bashcompletion.path = $${PREFIX}/share/bash-completion/completions

    zshcompletion.files = data/_nurture2d
    zshcompletion.path = $${PREFIX}/share/zsh/site-functions

    metainfo.files = data/org.nurture2d.Nurture2D.metainfo.xml
    metainfo.path = $${PREFIX}/share/metainfo

    mimepackage.files = data/org.nurture2d.Nurture2D.xml
    mimepackage.path = $${PREFIX}/share/mime/packages

    desktopentry.files = data/org.nurture2d.Nurture2D.desktop
    desktopentry.path = $${PREFIX}/share/applications

    # Keep using the existing icon filename for now
    icon.files = data/org.pencil2d.Pencil2D.png
    icon.path = $${PREFIX}/share/icons/hicolor/256x256/apps

    INSTALLS += bashcompletion zshcompletion target metainfo mimepackage desktopentry icon
}

# --- core_lib ---

INCLUDEPATH += ../../core_lib/src

BUILDTYPE =
debug_and_release:CONFIG(debug,debug|release) BUILDTYPE = debug
debug_and_release:CONFIG(release,debug|release) BUILDTYPE = release

win32-msvc* {
    LIBS += -L$$OUT_PWD/../core_lib/$$BUILDTYPE/ -lcore_lib
    PRE_TARGETDEPS += $$OUT_PWD/../core_lib/$$BUILDTYPE/core_lib.lib
}

win32-g++{
    LIBS += -L$$OUT_PWD/../core_lib/$$BUILDTYPE/ -lcore_lib
    PRE_TARGETDEPS += $$OUT_PWD/../core_lib/$$BUILDTYPE/libcore_lib.a
}

# --- mac os and linux
unix {
  LIBS += -L$$OUT_PWD/../core_lib/ -lcore_lib
  PRE_TARGETDEPS += $$OUT_PWD/../core_lib/libcore_lib.a
}

unix:!macx {
  LIBS += -lxcb
}
