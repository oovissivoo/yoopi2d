
#include "log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <cstdio>

Q_LOGGING_CATEGORY(logCanvasPainter, "core.canvasPainter");
Q_LOGGING_CATEGORY(logFileManager, "core.FileManager");

namespace {
QMutex logFileMutex;
QFile* appLogFile = nullptr;

void appMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const char* level = "INFO";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARNING"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }

    QString source;
    if (context.file && context.line > 0) {
        source = QStringLiteral(" [%1:%2]").arg(QString::fromUtf8(context.file)).arg(context.line);
    }
    const QByteArray line = QStringLiteral("%1 [%2]%3 %4\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
             QString::fromLatin1(level), source, message)
        .toUtf8();

    {
        QMutexLocker lock(&logFileMutex);
        if (appLogFile && appLogFile->isOpen()) {
            appLogFile->write(line);
            appLogFile->flush();
        }
    }
    std::fwrite(line.constData(), 1, size_t(line.size()), stderr);
}
}

void initCategoryLogging()
{
    const QString logRules =
        "*.debug=false\n"
        "default.debug=true\n"
        "core.canvasPainter.debug=false\n"
        "core.fileManager.debug=false";

    QLoggingCategory::setFilterRules(logRules);

    const QString logDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                     .filePath(QStringLiteral("logs"));
    if (!QDir().mkpath(logDirectory)) {
        std::fputs("[Yoopi2D] Could not create the application log directory.\n", stderr);
        return;
    }

    static QFile logFile(QDir(logDirectory).filePath(QStringLiteral("yoopi2d.log")));
    const qint64 maxLogSize = 5 * 1024 * 1024;
    if (QFileInfo::exists(logFile.fileName()) && QFileInfo(logFile.fileName()).size() > maxLogSize) {
        QFile::remove(logFile.fileName() + QStringLiteral(".1"));
        QFile::rename(logFile.fileName(), logFile.fileName() + QStringLiteral(".1"));
    }
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        std::fputs("[Yoopi2D] Could not open the application log file.\n", stderr);
        return;
    }

    appLogFile = &logFile;
    qInstallMessageHandler(appMessageHandler);
}
