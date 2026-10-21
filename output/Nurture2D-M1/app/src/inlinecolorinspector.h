/* InlineColorInspector - embeddable version of ColorInspector */

#ifndef INLINECOLORINSPECTOR_H
#define INLINECOLORINSPECTOR_H

#include <QWidget>
#include <QColor>

class Ui_ColorInspector; // forward for ui struct name (from ui_colorinspector.h)
class QTabWidget;
class QSpinBox;
class ColorSlider;
class BlenderStyleButton;
class BlenderStyleToggle;
class QLineEdit;

class InlineColorInspector : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit InlineColorInspector(QWidget* parent = nullptr);
    ~InlineColorInspector() override;

    QColor color() const { return mCurrentColor; }

signals:
    void colorChanged(const QColor& c);

public slots:
    void setColor(QColor newColor);

private slots:
    void onColorSpecChanged();
    void onColorChanged();
    void onColorChanged(const QColor& color);

private:
    void updateControls();

    QWidget* mInner = nullptr;
    struct UiProxy {
        QTabWidget* colorSpecTabWidget = nullptr;
        QWidget* rgbTab = nullptr;
        QWidget* hsvTab = nullptr;
        ColorSlider* redSlider = nullptr;
        ColorSlider* greenSlider = nullptr;
        ColorSlider* blueSlider = nullptr;
        ColorSlider* rgbAlphaSlider = nullptr;
        ColorSlider* hueSlider = nullptr;
        ColorSlider* saturationSlider = nullptr;
        ColorSlider* valueSlider = nullptr;
        ColorSlider* hsvAlphaSlider = nullptr;
        QWidget* colorWrapper = nullptr;
        QWidget* color = nullptr;
        QSpinBox* redSpinBox = nullptr;
        QSpinBox* greenSpinBox = nullptr;
        QSpinBox* blueSpinBox = nullptr;
        QSpinBox* rgbAlphaSpinBox = nullptr;
        QSpinBox* hueSpinBox = nullptr;
        QSpinBox* saturationSpinBox = nullptr;
        QSpinBox* valueSpinBox = nullptr;
        QSpinBox* hsvAlphaSpinBox = nullptr;
        BlenderStyleToggle* hsvModeToggle = nullptr;
        BlenderStyleToggle* rgbModeToggle = nullptr;
        QLineEdit* hueEdit = nullptr;
        QLineEdit* saturationEdit = nullptr;
        QLineEdit* valueEdit = nullptr;
        QLineEdit* redEdit = nullptr;
        QLineEdit* greenEdit = nullptr;
        QLineEdit* blueEdit = nullptr;
    } ui;

    bool isRgbColors = true;
    QColor mCurrentColor = Qt::red;
};

#endif // INLINECOLORINSPECTOR_H


