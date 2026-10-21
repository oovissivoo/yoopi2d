#ifndef COSMICPROGRESSBAR_H
#define COSMICPROGRESSBAR_H

#include <QWidget>

// CosmicProgressBar — a thin, accent-colored rounded progress bar.
// Matches the macOS-style visual language used throughout the app.
// Height is fixed at 4px by default; override with setFixedHeight() if needed.
class CosmicProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicProgressBar(QWidget* parent = nullptr);

    void setRange(int min, int max);
    void setValue(int value);
    int  value() const { return mValue; }
    int  minimum() const { return mMin; }
    int  maximum() const { return mMax; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int mMin   = 0;
    int mMax   = 100;
    int mValue = 0;
};

#endif // COSMICPROGRESSBAR_H
