#ifndef BLENDERSTYLESLIDER_H
#define BLENDERSTYLESLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QMenu>
#include <QLineEdit>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QEvent>

class BlenderStyleSlider : public QSlider
{
    Q_OBJECT

public:
    explicit BlenderStyleSlider(QWidget *parent = nullptr);
    explicit BlenderStyleSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~BlenderStyleSlider();
    
    // Set default value for reset functionality
    void setDefaultValue(int value);
    
    // Set the display format for showing values
    void setValueDisplayFormat(const QString &format);
    
    // Show numeric value tooltip during interaction
    void setShowValueTooltip(bool show);
    
    // Set the background color of the slider bar
    void setSliderColor(const QColor &color);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void resetToDefault();
    void setValueDialog();
    void copyValueToClipboard();
    void pasteValueFromClipboard();
    void onLineEditFinished();
    
private:
    void showContextMenu(const QPoint &pos);
    void showValueInputDialog();
    void createLineEdit();
    void destroyLineEdit();
    void positionLineEdit();
    
    // Internal state tracking
    bool isDragging;
    QPoint dragStartPosition;
    int dragStartValue;
    int defaultValue;
    QString valueFormat;
    bool showValueTooltip;
    QColor sliderColor;
    
    // For tracking click vs. drag behavior
    qint64 pressTime;   // Time when mouse was pressed
    QPoint pressPos;    // Position where mouse was pressed
    bool hasMoved;      // Whether mouse has moved significantly during drag
    
    // Inline editing
    QLineEdit *valueLineEdit;
    
    // For fine control calculations
    int calculateValueForDragDistance(int distance, bool fineControl);
    
    // Context menu
    QMenu *contextMenu;

    // Global drag accumulator for infinite drag
    QPoint lastCursorPosGlobal;   // last known global cursor position
    int totalDragPixels = 0;      // accumulated pixel delta since press
};

#endif // BLENDERSTYLESLIDER_H 