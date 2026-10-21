#ifndef COSMICIGNORECOLOR_H
#define COSMICIGNORECOLOR_H

#include <QWidget>
#include <QColor>

class CosmicCheckboxToggle;
class ColorSwatch;

// CosmicIgnoreColor
//
// Composite control used for bucket "Ignore color" options. It bundles:
// - An enable checkbox that toggles ignore-in-fill on/off.
// - A color swatch checkbox that represents the current ignore color
//   and can be used to enter a temporary color-pick mode.
//
// This widget is purely UI plumbing; tool logic (eyedropper, Editor,
// etc.) is handled by higher-level code.
class CosmicIgnoreColor : public QWidget
{
    Q_OBJECT

public:
    explicit CosmicIgnoreColor(QWidget* parent = nullptr);

    bool ignoreEnabled() const;
    QColor ignoreColor() const { return mColor; }
    bool colorPickerActive() const;

public slots:
    void setIgnoreEnabled(bool on);
    void setIgnoreColor(const QColor& color);
    void setColorPickerActive(bool on);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void ignoreEnabledChanged(bool on);
    // Emitted when the color swatch checkbox is toggled by the user.
    // The parent can use this to enter/exit temporary color-pick mode.
    void colorPickToggled(bool on);

private:
    CosmicCheckboxToggle* mEnableToggle = nullptr;
    ColorSwatch*          mColorSwatch  = nullptr;
    QColor                mColor;
    bool                  mPickerActive = false;
};

#endif // COSMICIGNORECOLOR_H
