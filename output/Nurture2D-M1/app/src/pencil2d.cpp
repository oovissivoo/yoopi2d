
/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "pencil2d.h"

#include <memory>

#include <QDebug>
#include <QFileOpenEvent>
#include <QIcon>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QStyleFactory>
#include <QFile>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
 #include <QScrollBar>
 #include <QMouseEvent>
 #include <QStyleOptionSlider>

// Static app-level translator — shared across the whole process lifetime.
QTranslator* Pencil2D::s_appTranslator = nullptr;
 #include <QCoreApplication>

#include "commandlineexporter.h"
#include "commandlineparser.h"
#include "mainwindow2.h"
#include "pencildef.h"
#include "platformhandler.h"
#include "cosmictext.h"


#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <clocale>
#endif

 namespace {
 static QStyleOptionSlider makeScrollBarStyleOption(const QScrollBar* sb)
 {
     QStyleOptionSlider opt;
     if (!sb) {
         return opt;
     }

     opt.initFrom(sb);
     opt.orientation = sb->orientation();
     opt.minimum = sb->minimum();
     opt.maximum = sb->maximum();
     opt.sliderPosition = sb->sliderPosition();
     opt.sliderValue = sb->value();
     opt.singleStep = sb->singleStep();
     opt.pageStep = sb->pageStep();
     opt.upsideDown = sb->invertedAppearance();
     opt.subControls = QStyle::SC_All;
     opt.activeSubControls = QStyle::SC_None;
     return opt;
 }

 class ScrollBarAbsoluteClickFilter final : public QObject
 {
 public:
     explicit ScrollBarAbsoluteClickFilter(QObject* parent = nullptr)
         : QObject(parent)
     {
     }

 protected:
     bool eventFilter(QObject* watched, QEvent* event) override
     {
         if (!event || event->type() != QEvent::MouseButtonPress) {
             return QObject::eventFilter(watched, event);
         }

         auto* sb = qobject_cast<QScrollBar*>(watched);
         if (!sb) {
             return QObject::eventFilter(watched, event);
         }

         auto* me = static_cast<QMouseEvent*>(event);
         if (me->button() != Qt::LeftButton) {
             return QObject::eventFilter(watched, event);
         }

         if (mSynthesizingPress) {
             return QObject::eventFilter(watched, event);
         }

         QStyleOptionSlider opt = makeScrollBarStyleOption(sb);
         const QRect grooveRect = sb->style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, sb);
         const QRect handleRect = sb->style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, sb);

         const QPoint localPos = me->pos();
         if (!grooveRect.contains(localPos) || handleRect.contains(localPos)) {
             return QObject::eventFilter(watched, event);
         }

         const bool isHorizontal = (sb->orientation() == Qt::Horizontal);
         const int clickCoord = isHorizontal ? localPos.x() : localPos.y();
         const int grooveStart = isHorizontal ? grooveRect.left() : grooveRect.top();
         const int grooveLen = isHorizontal ? grooveRect.width() : grooveRect.height();
         const int handleLen = isHorizontal ? handleRect.width() : handleRect.height();

         const int available = qMax(0, grooveLen - handleLen);
         const int rel = clickCoord - grooveStart - (handleLen / 2);
         const int posInGroove = qBound(0, rel, available);

         const int newValue = QStyle::sliderValueFromPosition(
             sb->minimum(),
             sb->maximum(),
             posInGroove,
             available,
             opt.upsideDown);

         sb->setValue(newValue);

         opt = makeScrollBarStyleOption(sb);
         const QRect newHandleRect = sb->style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, sb);
         const QPoint handleCenter = newHandleRect.center();

         mSynthesizingPress = true;
         QMouseEvent synthesized(
             QEvent::MouseButtonPress,
             handleCenter,
             sb->mapToGlobal(handleCenter),
             Qt::LeftButton,
             Qt::LeftButton,
             me->modifiers());
         QCoreApplication::sendEvent(sb, &synthesized);
         mSynthesizingPress = false;

         event->accept();
         return true;
     }

 private:
     bool mSynthesizingPress = false;
 };
 }

Pencil2D::Pencil2D(int& argc, char** argv) :
    QApplication(argc, argv)
{
    setOrganizationName("Nurture2D");
    setOrganizationDomain("nurture2d.org");
    setApplicationName("Nurture2D");
    setApplicationDisplayName("Nurture2D");
    setApplicationVersion(APP_VERSION);
    setWindowIcon(QIcon(":/pencil2d.ico"));

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    setDesktopFileName("org.nurture2d.Nurture2D");
#endif
}

void Pencil2D::initialize()
{
    installTranslators();

    // Load Inter font family from embedded resources
    const QStringList interFonts = {
        ":/fonts/Inter-Thin-BETA.otf",
        ":/fonts/Inter-ExtraLight-BETA.otf",
        ":/fonts/Inter-Light-BETA.otf",
        ":/fonts/Inter-Regular.otf",
        ":/fonts/Inter-Medium.otf",
        ":/fonts/Inter-SemiBold.otf",
        ":/fonts/Inter-Bold.otf",
        ":/fonts/Inter-ExtraBold.otf",
        ":/fonts/Inter-Black.otf",
        ":/fonts/Inter-ThinItalic-BETA.otf",
        ":/fonts/Inter-ExtraLightItalic-BETA.otf",
        ":/fonts/Inter-LightItalic-BETA.otf",
        ":/fonts/Inter-Italic.otf",
        ":/fonts/Inter-MediumItalic.otf",
        ":/fonts/Inter-SemiBoldItalic.otf",
        ":/fonts/Inter-BoldItalic.otf",
        ":/fonts/Inter-ExtraBoldItalic.otf",
        ":/fonts/Inter-BlackItalic.otf",
    };
    for (const QString& path : interFonts) {
        int id = QFontDatabase::addApplicationFont(path);
        if (id == -1) {
            qWarning() << "Failed to load font:" << path;
        }
    }

    // Verify the exact family name Qt registered for Inter (may differ from "Inter")
    QString interFamily = QStringLiteral("Inter");
    {
        // Find the family name from the Regular weight — that's the canonical one
        int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Regular.otf"));
        if (id != -1) {
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) {
                interFamily = families.first();
                qDebug() << "Inter font registered as family:" << interFamily;
            }
        }
    }

    // Set Inter as the global application font, using cosmicTextSize from settings
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        const int textSize = qMax(6, settings.value(QStringLiteral("cosmicTextSize"), 9).toInt());
        QFont appFont;
        appFont.setFamily(interFamily);
        appFont.setPointSize(textSize);
        appFont.setWeight(QFont::Normal);
        appFont.setStyle(QFont::StyleNormal);
        appFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
            QFont::PreferAntialias | QFont::PreferMatch));
        appFont.setHintingPreference(QFont::PreferFullHinting);
        setFont(appFont);

        // Store the verified family name so CosmicText::font() uses the same one
        CosmicText::setFontFamily(interFamily);
    }

    setStyle(QStyleFactory::create("Fusion"));

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#262626"));
    pal.setColor(QPalette::Base, QColor("#353535"));
    pal.setColor(QPalette::AlternateBase, QColor("#353535"));
    pal.setColor(QPalette::Button, QColor("#353535"));
    pal.setColor(QPalette::Text, QColor("#e6e6e6"));
    pal.setColor(QPalette::WindowText, QColor("#e6e6e6"));
    pal.setColor(QPalette::ButtonText, QColor("#e6e6e6"));
    pal.setColor(QPalette::Mid, QColor("#3a3a3a"));
    pal.setColor(QPalette::Dark, QColor("#1f1f1f"));
    pal.setColor(QPalette::Light, QColor("#4a4a4a"));
    pal.setColor(QPalette::Highlight, QColor("#00a7ff"));
    pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    setPalette(pal);

    QFile darkQss(":/styles/dark_theme.qss");
    if (darkQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(darkQss.readAll()));
    }

    installEventFilter(new ScrollBarAbsoluteClickFilter(this));
}

Pencil2D::~Pencil2D()
{
    // with a std::unique_ptr member variable,
    // you need a non-default destructor to avoid compilation error.
}

Status Pencil2D::handleCommandLineOptions(QSplashScreen* splash)
{
    CommandLineParser parser;
    parser.process(arguments());

#ifndef QT_DEBUG
    if (isInstanceOpen()) {
        return Status::SAFE;
    }
#endif

    QString inputPath = parser.inputPath();
    QStringList outputPaths = parser.outputPaths();

    if (outputPaths.isEmpty())
    {
        prepareGuiStartup(inputPath, splash);
        return Status::OK;
    }

    // Can't construct the editor directly, need to make a main window instead because... reasons
    mainWindow.reset(new MainWindow2);
    CommandLineExporter exporter(mainWindow->mEditor);
    if (exporter.process(inputPath,
                         outputPaths,
                         parser.camera(),
                         parser.width(),
                         parser.height(),
                         parser.startFrame(),
                         parser.endFrame(),
                         parser.transparency()))
    {
        return Status::SAFE;
    }
    return Status::FAIL;
}

bool Pencil2D::isInstanceOpen()
{
    QDir appDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("Nurture2D");
    appDir.mkpath(".");
    mProcessLock.reset(new QLockFile(appDir.absoluteFilePath("nurture2d-process.lock")));
    if (!mProcessLock->tryLock(10))
    {
        QMessageBox::StandardButton clickedButton = QMessageBox::warning(nullptr, tr("Warning"), tr("An instance of Nurture2D is already open. Running multiple instances of Nurture2D simultaneously is not recommended and could potentially result in data loss and other unexpected behavior."), QMessageBox::Close | QMessageBox::Open, QMessageBox::Close);
        if (clickedButton != QMessageBox::Open)
        {
            return true;
        }
    }
    return false;
}

bool Pencil2D::event(QEvent* event)
{
    if (event->type() == QEvent::FileOpen)
    {
        auto fileOpenEvent = dynamic_cast<QFileOpenEvent*>(event);
        Q_ASSERT(fileOpenEvent);
        emit openFileRequested(fileOpenEvent->file());
        return true;
    }
    return QApplication::event(event);
}

void Pencil2D::installTranslators()
{
    QSettings setting(PENCIL2D, PENCIL2D);
    QString userLocale = setting.value(SETTING_LANGUAGE).toString();
    QLocale locale = userLocale.isEmpty() ? QLocale::system() : QLocale(userLocale);
    QLocale::setDefault(locale);

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    // In versions prior to 5.14, Qt's DOM implementation erroneously used
    // locale-dependent string conversion for double attributes (QTBUG-80068).
    // To work around this, we override the numeric locale category to use the
    // C locale.
    std::setlocale(LC_NUMERIC, "C");
#endif

    std::unique_ptr<QTranslator> qtTranslator(new QTranslator(this));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (qtTranslator->load(locale, "qt", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
#else
    if (qtTranslator->load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
#endif
    {
        installTranslator(qtTranslator.release());
    }

    // Use switchAppTranslator so the static pointer is set correctly from the start.
    // This ensures languageChanged() can properly replace it later.
    switchAppTranslator(userLocale);
}

bool Pencil2D::switchAppTranslator(const QString& localeCode)
{
    QLocale locale = localeCode.isEmpty() ? QLocale::system() : QLocale(localeCode);

    auto* newTr = new QTranslator(qApp);
    const bool loaded = newTr->load(locale, "pencil", "_", ":/i18n/");

    if (loaded) {
        // Remove and delete the old translator before installing the new one.
        if (s_appTranslator) {
            qApp->removeTranslator(s_appTranslator);
            delete s_appTranslator;
        }
        s_appTranslator = newTr;
        qApp->installTranslator(s_appTranslator);
    } else {
        delete newTr;
        // If no translation found, remove the old one so we fall back to English.
        if (s_appTranslator) {
            qApp->removeTranslator(s_appTranslator);
            delete s_appTranslator;
            s_appTranslator = nullptr;
        }
    }
    return loaded;
}

void Pencil2D::prepareGuiStartup(const QString& inputPath, QSplashScreen* splash)
{
    PlatformHandler::configurePlatformSpecificSettings();

    try {
        mainWindow.reset(new MainWindow2);
        if (!mainWindow) {
            throw std::runtime_error("Failed to create MainWindow2");
        }

        connect(this, &Pencil2D::openFileRequested, mainWindow.get(), &MainWindow2::openFile);
        mainWindow->show();
        mainWindow->showNormal();
        mainWindow->raise();
        mainWindow->activateWindow();

        // Close splash only after the main window is fully shown
        if (splash) {
            splash->finish(mainWindow.get());
        }

        if (!inputPath.isEmpty()) {
            mainWindow->openStartupFile(inputPath);
        }
    } catch (const std::exception& e) {
        qCritical() << "Fatal error during GUI startup:" << e.what();
        if (splash) splash->close();
        QMessageBox::critical(nullptr, tr("Startup Error"),
            tr("Nurture2D failed to initialize the main window: %1").arg(e.what()));
        throw;
    }
}
