/*
Smooth Loading Bar Widget
A loading bar that smoothly animates from 0 to 100% with customizable colors.
Used to provide visual feedback during preference updates.
*/

#ifndef SMOOTHLOADINGBAR_H
#define SMOOTHLOADINGBAR_H

#include <QWidget>
#include <QTimer>
#include <QColor>

class SmoothLoadingBar : public QWidget
{
    Q_OBJECT

public:
    explicit SmoothLoadingBar(QWidget* parent = nullptr);
    ~SmoothLoadingBar() override;

    void setBackgroundColor(const QColor& color);
    void setLoadingColor(const QColor& color);
    void setDuration(int milliseconds);
    
    void start();
    void reset();
    bool isLoading() const { return mIsLoading; }

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updateProgress();

private:
    QTimer* mTimer;
    QColor mBackgroundColor;
    QColor mLoadingColor;
    qreal mProgress;
    qreal mTargetProgress;
    int mDuration;
    qint64 mStartTime;
    bool mIsLoading;
    
    static constexpr int UPDATE_INTERVAL = 16; // ~60fps
};

#endif // SMOOTHLOADINGBAR_H
