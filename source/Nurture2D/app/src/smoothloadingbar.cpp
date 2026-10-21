/*
Smooth Loading Bar Widget Implementation
A loading bar that smoothly animates from 0 to 100% with customizable colors.
Used to provide visual feedback during preference updates.
*/

#include "smoothloadingbar.h"
#include <QPainter>
#include <QDateTime>
#include <QtMath>

SmoothLoadingBar::SmoothLoadingBar(QWidget* parent)
    : QWidget(parent)
    , mTimer(nullptr)
    , mBackgroundColor(QColor(60, 60, 60))
    , mLoadingColor(Qt::blue)
    , mProgress(0.0)
    , mTargetProgress(0.0)
    , mDuration(500)
    , mStartTime(0)
    , mIsLoading(false)
{
    setFixedHeight(3);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    mTimer = new QTimer(this);
    mTimer->setInterval(UPDATE_INTERVAL);
    connect(mTimer, &QTimer::timeout, this, &SmoothLoadingBar::updateProgress);
}

SmoothLoadingBar::~SmoothLoadingBar()
{
}

void SmoothLoadingBar::setBackgroundColor(const QColor& color)
{
    mBackgroundColor = color;
    update();
}

void SmoothLoadingBar::setLoadingColor(const QColor& color)
{
    mLoadingColor = color;
    update();
}

void SmoothLoadingBar::setDuration(int milliseconds)
{
    mDuration = milliseconds;
}

void SmoothLoadingBar::start()
{
    mProgress = 0.0;
    mTargetProgress = 1.0;
    mStartTime = QDateTime::currentMSecsSinceEpoch();
    mIsLoading = true;
    mTimer->start();
    update();
}

void SmoothLoadingBar::reset()
{
    mTimer->stop();
    mProgress = 0.0;
    mTargetProgress = 0.0;
    mIsLoading = false;
    update();
}

void SmoothLoadingBar::updateProgress()
{
    if (!mIsLoading) {
        return;
    }
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = currentTime - mStartTime;
    
    // Calculate progress using ease-out cubic easing
    qreal t = qMin(1.0, static_cast<qreal>(elapsed) / mDuration);
    qreal easedT = 1.0 - qPow(1.0 - t, 3.0); // Cubic ease-out
    
    mProgress = easedT;
    
    update();
    
    // Check if animation is complete
    if (mProgress >= 0.99) {
        mProgress = 1.0;
        mTimer->stop();
        mIsLoading = false;
        update();
        emit finished();
    }
}

void SmoothLoadingBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    painter.fillRect(rect(), mBackgroundColor);
    
    // Draw loading progress
    if (mProgress > 0.0) {
        int progressWidth = static_cast<int>(width() * mProgress);
        QRect progressRect(0, 0, progressWidth, height());
        painter.fillRect(progressRect, mLoadingColor);
    }
}
