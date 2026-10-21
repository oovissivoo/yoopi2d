#include "scribblearea.h"
#include <QCoreApplication>

void ScribbleArea::showProgress(qreal progress, const QString& label)
{
    mShowProgress = true;
    mProgressValue = qBound(0.0, progress, 1.0);
    mProgressLabel = label;
    update(); // Trigger immediate repaint to show progress
    QCoreApplication::processEvents(); // Force UI update
}

void ScribbleArea::hideProgress()
{
    mShowProgress = false;
    mProgressValue = 0.0;
    mProgressLabel.clear();
    update();
}
