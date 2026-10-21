/*
Popup Color Picker Implementation
A custom color picker dialog that uses our built-in ColorWheel instead of OS-dependent QColorDialog.
*/

#include "popupcolorpicker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

PopupColorPicker::PopupColorPicker(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Color"));
    setModal(true);
    resize(300, 400);
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Create the color wheel
    colorWheel = new ColorWheel(this);
    connect(colorWheel, &ColorWheel::colorChanged, this, &PopupColorPicker::onColorChanged);
    mainLayout->addWidget(colorWheel);
    
    // Create preview label
    previewLabel = new QLabel(this);
    previewLabel->setFixedSize(80, 30);
    previewLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    previewLabel->setAutoFillBackground(true);
    
    QHBoxLayout *previewLayout = new QHBoxLayout();
    previewLayout->addWidget(new QLabel(tr("Selected Color:"), this));
    previewLayout->addWidget(previewLabel);
    previewLayout->addStretch();
    mainLayout->addLayout(previewLayout);
    
    // Create buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);
}

QColor PopupColorPicker::getColor(const QColor &initialColor)
{
    // Set the initial color
    currentColor = initialColor;
    colorWheel->setColor(initialColor);
    
    // Update preview
    QPalette pal = previewLabel->palette();
    pal.setColor(QPalette::Window, initialColor);
    previewLabel->setPalette(pal);
    
    // Show dialog and return the chosen color or initial color if canceled
    if (exec() == QDialog::Accepted) {
        return currentColor;
    } else {
        return initialColor;
    }
}

void PopupColorPicker::onColorChanged(const QColor &color)
{
    currentColor = color;
    
    // Update preview
    QPalette pal = previewLabel->palette();
    pal.setColor(QPalette::Window, color);
    previewLabel->setPalette(pal);
}
