/**
 * Simple test program for BlenderStyleValueField
 * 
 * To compile and run (from CUI directory):
 * qmake && make && ./test_valuefield
 * 
 * Or add to a test project to see the widget in action.
 */

#include "blenderstylevaluefield.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Create main window
    QWidget window;
    window.setWindowTitle(QObject::tr("BlenderStyleValueField Test"));
    window.setStyleSheet("QWidget { background-color: #2D2D30; }");
    window.resize(400, 500);
    
    auto* layout = new QVBoxLayout(&window);
    layout->setSpacing(16);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    auto* title = new QLabel(QObject::tr("BlenderStyleValueField Examples"), &window);
    title->setStyleSheet("QLabel { color: white; font-size: 16px; font-weight: bold; }");
    layout->addWidget(title);
    
    // Value display label
    auto* valueLabel = new QLabel(QObject::tr("Current values will appear here"), &window);
    valueLabel->setStyleSheet("QLabel { color: #5BA5FF; font-size: 12px; }");
    layout->addWidget(valueLabel);
    
    layout->addSpacing(10);
    
    // Example 1: Rotation (0-360°)
    auto* rotationLabel = new QLabel("Example 1: Rotation (0-360°)", &window);
    rotationLabel->setStyleSheet("QLabel { color: #DCDCDC; font-size: 11px; }");
    layout->addWidget(rotationLabel);
    
    auto* rotationField = new BlenderStyleValueField(&window);
    rotationField->setLabel("Rotation");
    rotationField->setRange(0.0, 360.0);
    rotationField->setValue(45.0);
    rotationField->setDecimals(1);
    rotationField->setSingleStep(1.0);
    layout->addWidget(rotationField);
    
    // Example 2: Opacity (0.0-1.0)
    auto* opacityLabel = new QLabel("Example 2: Opacity (0.0-1.0)", &window);
    opacityLabel->setStyleSheet("QLabel { color: #DCDCDC; font-size: 11px; }");
    layout->addWidget(opacityLabel);
    
    auto* opacityField = new BlenderStyleValueField(&window);
    opacityField->setLabel("Opacity");
    opacityField->setRange(0.0, 1.0);
    opacityField->setValue(0.750);
    opacityField->setDecimals(3);
    opacityField->setSingleStep(0.01);
    layout->addWidget(opacityField);
    
    // Example 3: Zoom (0.1-10.0)
    auto* zoomLabel = new QLabel("Example 3: Zoom (0.1-10.0)", &window);
    zoomLabel->setStyleSheet("QLabel { color: #DCDCDC; font-size: 11px; }");
    layout->addWidget(zoomLabel);
    
    auto* zoomField = new BlenderStyleValueField(&window);
    zoomField->setLabel("Zoom");
    zoomField->setRange(0.1, 10.0);
    zoomField->setValue(1.0);
    zoomField->setDecimals(2);
    zoomField->setSingleStep(0.1);
    layout->addWidget(zoomField);
    
    // Example 4: Frame Offset (-100 to 100)
    auto* offsetLabel = new QLabel("Example 4: Frame Offset (-100 to 100)", &window);
    offsetLabel->setStyleSheet("QLabel { color: #DCDCDC; font-size: 11px; }");
    layout->addWidget(offsetLabel);
    
    auto* offsetField = new BlenderStyleValueField(&window);
    offsetField->setLabel("Offset");
    offsetField->setRange(-100.0, 100.0);
    offsetField->setValue(0.0);
    offsetField->setDecimals(0);
    offsetField->setSingleStep(1.0);
    layout->addWidget(offsetField);
    
    // Example 5: Temperature (-50 to 50)
    auto* tempLabel = new QLabel("Example 5: Temperature (-50 to 50)", &window);
    tempLabel->setStyleSheet("QLabel { color: #DCDCDC; font-size: 11px; }");
    layout->addWidget(tempLabel);
    
    auto* tempField = new BlenderStyleValueField(&window);
    tempField->setLabel("Temp");
    tempField->setRange(-50.0, 50.0);
    tempField->setValue(20.5);
    tempField->setDecimals(1);
    tempField->setSingleStep(0.5);
    layout->addWidget(tempField);
    
    layout->addStretch();
    
    // Instructions
    auto* instructions = new QLabel(
        "Instructions:\n"
        "• Drag on center field to adjust value\n"
        "• Click on center field to type value\n"
        "• Click arrow buttons to increment/decrement\n"
        "• Scroll wheel to adjust (Shift: fine, Ctrl: coarse)\n"
        "• Hold Shift while dragging for fine control\n"
        "• Hold Ctrl while dragging for coarse control",
        &window
    );
    instructions->setStyleSheet("QLabel { color: #999999; font-size: 10px; }");
    instructions->setWordWrap(true);
    layout->addWidget(instructions);
    
    // Connect all value changes to update the display
    QObject::connect(rotationField, &BlenderStyleValueField::valueChanged, [valueLabel](double v) {
        valueLabel->setText(QString("Rotation: %1°").arg(v, 0, 'f', 1));
    });
    
    QObject::connect(opacityField, &BlenderStyleValueField::valueChanged, [valueLabel](double v) {
        valueLabel->setText(QString("Opacity: %1").arg(v, 0, 'f', 3));
    });
    
    QObject::connect(zoomField, &BlenderStyleValueField::valueChanged, [valueLabel](double v) {
        valueLabel->setText(QString("Zoom: %1×").arg(v, 0, 'f', 2));
    });
    
    QObject::connect(offsetField, &BlenderStyleValueField::valueChanged, [valueLabel](double v) {
        valueLabel->setText(QString("Offset: %1 frames").arg(v, 0, 'f', 0));
    });
    
    QObject::connect(tempField, &BlenderStyleValueField::valueChanged, [valueLabel](double v) {
        valueLabel->setText(QString("Temperature: %1°C").arg(v, 0, 'f', 1));
    });
    
    // Debug output
    QObject::connect(rotationField, &BlenderStyleValueField::valueChanged, [](double v) {
        qDebug() << "Rotation changed:" << v;
    });
    
    window.show();
    
    return app.exec();
}

