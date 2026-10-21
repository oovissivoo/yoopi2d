// Exercise the real timeline fields and playback manager with isolated settings.
#include <QApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "mainwindow2.h"
#include "editor.h"
#include "filemanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "timeline.h"
#include "timecontrols.h"

namespace {
int checks = 0;
void require(bool ok, const char* message)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
    ++checks;
}

void typeFrame(QSpinBox* field, const QString& text)
{
    field->setFocus();
    auto* input = field->findChild<QLineEdit*>();
    require(input != nullptr, "numeric field has a text editor");
    input->setText(text);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(field, &enter);
    QApplication::processEvents();
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "isolated settings and document directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    auto* window = new MainWindow2;
    window->resize(1440, 900);
    window->show();
    app.processEvents();
    auto* timeline = window->findChild<TimeLine*>();
    auto* controls = window->findChild<TimeControls*>();
    auto* start = window->findChild<QSpinBox*>("playbackRangeStart");
    auto* end = window->findChild<QSpinBox*>("playbackRangeEnd");
    require(timeline && controls && start && end, "real timeline range widgets");
    require(start->isVisible() && end->isVisible(), "both numeric fields are visible");
    require(controls->rangeWidget()->mapToGlobal(QPoint()).x()
            < controls->trailingWidget()->mapToGlobal(QPoint()).x(),
            "range block appears left of FPS");
    auto* playback = window->mEditor->playback();
    typeFrame(end, "11");
    typeFrame(start, "3");
    require(timeline->getRangeLower() == 3 && timeline->getRangeUpper() == 11,
            "typed values update both red marker positions");
    require(playback->startFrame() == 3 && playback->endFrame() == 11,
            "playback uses the inclusive typed range");
    playback->setLooping(false);
    playback->setFps(90);
    window->mEditor->scrubTo(1);
    QEventLoop playing;
    QObject::connect(playback, &PlaybackManager::playStateChanged, &playing,
                     [&playing](bool active) { if (!active) playing.quit(); });
    QTimer::singleShot(2000, &playing, &QEventLoop::quit);
    playback->play();
    require(window->mEditor->currentFrame() == 3, "Play starts at the typed lower boundary");
    playing.exec();
    require(!playback->isPlaying() && window->mEditor->currentFrame() == 11,
            "actual timed playback stops on the typed final frame");
    timeline->setRangeLower(5);
    timeline->setRangeUpper(18);
    require(start->value() == 5 && end->value() == 18,
            "marker drag path updates numeric fields");
    typeFrame(start, "30");
    require(start->value() == 30 && end->value() == 31,
            "moving start beyond end keeps range valid");
    require(playback->startFrame() == 30 && playback->endFrame() == 31,
            "playback receives adjusted end as well as start");
    typeFrame(end, "1500");
    require(end->value() == 1500 && playback->endFrame() == 1500,
            "numeric entry extends beyond current timeline length and 99 frames");
    timeline->onObjectLoaded();
    require(end->value() == 1500, "UI refresh preserves a large range");

    FileManager files;
    require(playback->save(window->mEditor->object()).ok(), "save playback state to document");
    QString path = temporary.filePath("range.pclx");
    require(files.save(window->mEditor->object(), path).ok(), "save range in animation file");
    std::unique_ptr<Object> restored(files.load(path));
    require(restored != nullptr, "reopen animation file");
    require(playback->load(restored.get()).ok(), "restore playback range from reopened file");
    controls->updateUI();
    require(start->value() == 30 && end->value() == 1500,
            "saved range returns to the numeric fields");

    // Loading another document must reset the previous end-field minimum.
    std::unique_ptr<Object> fresh(new Object);
    require(playback->load(fresh.get()).ok(), "load another document's playback state");
    controls->updateUI();
    require(start->value() == playback->markInFrame()
            && end->value() == playback->markOutFrame(),
            "document switch resets values and constraints");
    timeline->setRangeState(false);
    require(!start->isEnabled() && !end->isEnabled(), "disabled range disables numeric fields");
    timeline->setRangeState(true);
    require(start->isEnabled() && end->isEnabled(), "enabled range enables numeric fields");
    window->grab().save("/private/tmp/yoopi2d-playback-range-preview.png");
    std::printf("PASS: %d playback range checks\n", checks);
    std::fflush(stdout);
    std::_Exit(0);
}
