#ifndef COSMICCOLORSLIDERS_H
#define COSMICCOLORSLIDERS_H

#include <QWidget>
#include <QColor>

#include "../CUI/cosmicinputfield.h"

class QLabel;
class QVBoxLayout;

// ─────────────────────────────────────────────────────────────────────────────
// GradientSlider
//
// A custom horizontal slider that renders a per-channel color gradient and an
// adaptive-contrast vertical-rectangle handle.  Value units are channel-native:
//   H  [0, 360]   S/L [0, 100]   R/G/B [0, 255]
// ─────────────────────────────────────────────────────────────────────────────
class GradientSlider : public QWidget
{
    Q_OBJECT
public:
    enum class Channel { H, S, L, R, G, B };

    explicit GradientSlider(Channel channel, QWidget* parent = nullptr);

    void    setChannel(Channel ch);
    Channel channel() const;

    double value() const;
    void   setValue(double v);           // does NOT emit valueChanged
    void   setContextColor(const QColor& c); // updates gradient, triggers repaint

signals:
    void valueChanged(double v);         // emitted during drag / click
    void valueCommitted(double v);       // emitted on mouse release

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    double posToValue(int x) const;
    int    valueToPos(double v) const;
    QColor sampleGradientAt(int x) const; // samples the rendered gradient
    QColor handleColor() const;           // white or black based on luminance
    double maxValue() const;              // 360, 100, or 255 depending on channel

    Channel mChannel;
    double  mValue = 0.0;
    QColor  mContextColor;
    bool    mDragging = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// SliderSection
//
// Internal collapsible section widget.  Contains a header row
// (CosmicCheckboxToggle + QLabel) and a body widget with channel rows.
// Clicking the header toggles expanded/collapsed state.
// ─────────────────────────────────────────────────────────────────────────────
class SliderSection : public QWidget
{
    Q_OBJECT
public:
    explicit SliderSection(const QString& title, QWidget* parent = nullptr);

    bool isExpanded() const;
    void setExpanded(bool expanded);

    // Add a channel row widget to the body
    void addRow(QWidget* row);

    // Access the body container (for iterating rows)
    QWidget* body() const { return mBody; }

signals:
    void expandedChanged(bool expanded);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onHeaderClicked();

    QWidget*              mHeader   = nullptr;
    QLabel*               mIconLabel = nullptr;  // shows Expand/Collapse svg icon
    QLabel*               mLabel    = nullptr;
    QWidget*              mBody     = nullptr;
    QVBoxLayout*          mBodyLayout = nullptr;
    bool                  mExpanded = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// CosmicColorSliders
//
// Top-level panel widget with HSL and RGB collapsible slider sections.
// Exposes the same color interface as CosmicColorWheelWidget so it can be
// wired identically in mainwindow2.cpp.
// ─────────────────────────────────────────────────────────────────────────────
class CosmicColorSliders : public QWidget
{
    Q_OBJECT
public:
    explicit CosmicColorSliders(QWidget* parent = nullptr);

    QColor color() const;

    QSize sizeHint() const override;

signals:
    void colorChanged(const QColor& color);
    void colorCommitted(const QColor& color);

public slots:
    void setColor(const QColor& color); // updates all sliders/inputs, no signal emitted

private:
    void updateAllFromColor(const QColor& c); // pushes c into all sliders and inputs
    void onHslChanged();  // called when any HSL slider/input changes
    void onRgbChanged();  // called when any RGB slider/input changes

    int  contentHeightForCurrentState() const;
    void requestPaneHeightRefresh();

    // Section widgets
    SliderSection* mHslSection = nullptr;
    SliderSection* mRgbSection = nullptr;

    // HSL rows
    GradientSlider*  mHSlider = nullptr;
    GradientSlider*  mSSlider = nullptr;
    GradientSlider*  mLSlider = nullptr;
    CosmicInputField* mHInput = nullptr;
    CosmicInputField* mSInput = nullptr;
    CosmicInputField* mLInput = nullptr;

    // RGB rows
    GradientSlider*  mRSlider = nullptr;
    GradientSlider*  mGSlider = nullptr;
    GradientSlider*  mBSlider = nullptr;
    CosmicInputField* mRInput = nullptr;
    CosmicInputField* mGInput = nullptr;
    CosmicInputField* mBInput = nullptr;

    QColor mCurrentColor;
    bool   mUpdating = false; // re-entrancy guard during sync

    // Stored HSL components — independent of mCurrentColor so H and S
    // survive round-trips through achromatic colors (L=0, L=100, S=0)
    double mStoredH = 0.0;
    double mStoredS = 0.0;
    double mStoredL = 100.0;

    bool mRestoringState = false;
};

#endif // COSMICCOLORSLIDERS_H
