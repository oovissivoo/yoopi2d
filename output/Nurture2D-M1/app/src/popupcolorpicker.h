/*
Popup Color Picker
A custom color picker dialog that uses our built-in ColorWheel instead of OS-dependent QColorDialog.
*/

#ifndef POPUPCOLORPICKER_H
#define POPUPCOLORPICKER_H

#include <QDialog>
#include <QColor>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include "colorwheel.h"

class PopupColorPicker : public QDialog
{
    Q_OBJECT

public:
    explicit PopupColorPicker(QWidget *parent = nullptr);
    
    // Set the initial color and return the selected color
    QColor getColor(const QColor &initialColor);

private slots:
    void onColorChanged(const QColor &color);

private:
    ColorWheel *colorWheel;
    QLabel *previewLabel;
    QColor currentColor;
};

#endif // POPUPCOLORPICKER_H
