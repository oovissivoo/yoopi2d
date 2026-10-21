#ifndef COSMICSLIDERCARD_H
#define COSMICSLIDERCARD_H

#include <QWidget>
#include <QString>

class CosmicHSlider;
class QLabel;
class QVBoxLayout;

// CosmicSliderCard - a labelled slider inside a #1a1a1a rounded card.
//
// Layout:
//   ┌─────────────────────────────┐
//   │  Label text                 │
//   │  [══════════|═══════]  42   │
//   └─────────────────────────────┘
//
// The card background is #1a1a1a with CosmicUiMetrics::cornerRadius().
// The inner CosmicHSlider is exposed via slider() for signal connections.
class CosmicSliderCard : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicSliderCard(const QString& label, QWidget* parent = nullptr);

    CosmicHSlider* slider() const { return mSlider; }

    // Convenience pass-throughs so callers don't always need to reach into slider()
    void setRange(int min, int max);
    void setValue(int value);
    int  value() const;

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    CosmicHSlider* mSlider  = nullptr;
    QLabel*        mLabel   = nullptr;
};

#endif // COSMICSLIDERCARD_H
