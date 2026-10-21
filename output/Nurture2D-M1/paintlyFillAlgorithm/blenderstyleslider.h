#ifndef BLENDERSTYLESLIDER_H
#define BLENDERSTYLESLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QLineEdit>
#include <QEvent>
#include <QColor>
#include <functional>

class BlenderStyleSlider : public QSlider
{
    Q_OBJECT

public:
    explicit BlenderStyleSlider(QWidget *parent = nullptr);
    explicit BlenderStyleSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~BlenderStyleSlider();

    static QColor defaultSliderColor();
    static QColor getAccentColor();
    
    // Real-time color update support
    void updateAccentColor();
    
    // Set default value for reset functionality
    void setDefaultValue(int value);
    
    // Set the display format for showing values
    void setValueDisplayFormat(const QString &format);
    
    // Set the property name to display on the left side
    void setPropertyName(const QString &name);
    
    // Show numeric value tooltip during interaction
    void setShowValueTooltip(bool show);
    
    // Set the background color of the slider bar
    void setSliderColor(const QColor &color);

    // Set the display value (what the user sees)
    void setDisplayValue(qreal displayValue);
    
    // Set callback to convert display value to internal slider value
    void setDisplayToSliderCallback(std::function<int(qreal)> callback);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onLineEditFinished();
    
private:
    int valueFromPosition(const QPoint &pos) const;
    void showValueInputDialog();
    void createLineEdit();
    void destroyLineEdit();
    void positionLineEdit();
    void autoDetectPropertyName();
    
    // Internal state tracking
    bool isDragging;
    int defaultValue;
    QString valueFormat;
    bool showValueTooltip;
    QColor sliderColor;
    
    // For tracking click vs. drag behavior
    
    
    // Inline editing
    QLineEdit *valueLineEdit;
    
    // For fine control calculations (removed)

    
    // Display value (mapped from parent, e.g., 0-18 instead of internal 0-100)
    qreal mDisplayValue;
    bool mUseDisplayValue;
    
    // Callback for converting display value to internal slider value
    std::function<int(qreal)> mDisplayToSliderCallback;
    
    // Property name to display on the left side
    QString mPropertyName;
    
};

#endif // BLENDERSTYLESLIDER_H 