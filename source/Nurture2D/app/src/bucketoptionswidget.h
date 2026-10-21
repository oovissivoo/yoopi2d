/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef BUCKETOPTIONSWIDGET_H
#define BUCKETOPTIONSWIDGET_H

#include <QWidget>
#include <QList>
#include <QVector>
#include <QColor>
#include <QCheckBox>
#include <QLabel>
#include "basedockwidget.h"
#include "pencildef.h"

class Editor;
class Layer;
class CosmicDropDown;
class CosmicCheckboxToggle;
class CosmicButton;
class CosmicIgnoreColor;
class CosmicInputField;

namespace Ui {
class BucketOptionsWidget;
}

class BucketOptionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BucketOptionsWidget(Editor* editor, QWidget* parent);

    // Auto-line fill UI builds programmatically
    ~BucketOptionsWidget();

    void setStrokeWidth(qreal value);
    void setColorToleranceEnabled(bool enabled);
    // Deprecated: Expand Fill removed but keep no-op setters to satisfy signal handlers
    void setFillExpandEnabled(bool enabled);
    void setFillExpand(int value);
    void setColorTolerance(int tolerance);
    void setFillReferenceMode(int referenceMode);
    void setFillMode(int mode);
    void setGapFillMode(int mode);

    void onPropertyChanged(ToolType, const ToolPropertyType propertyType);
    void onLayerChanged(int);

    // Explicitly refresh UI from current tool properties
    void updateUI();

private slots:
    void onIgnoreColorClicked();
    void onEyedropperColorPicked();
    void onIgnoreInFillToggled(bool checked);

private:
    void updatePropertyVisibility();
    void buildAutoFillUI();

    Ui::BucketOptionsWidget *ui;
    Editor* mEditor = nullptr;

    // Auto-line fill controls (legacy; master/color checkboxes no longer exposed in this widget)
    QCheckBox* mMasterAutoFill = nullptr;
    QList<QCheckBox*> mColorChecks;
    QVector<QRgb> mColorValues;

    // Ignore color
    // New cosmic control: composite CosmicIgnoreColor widget inside CosmicSplitToolLayout
    QRgb  mIgnoreColor = 0;
    bool  mEyedropperMode = false;
    bool  mIgnoreInFillEnabled = true;

    // const static int MAX_EXPAND = 25; // removed with Expand Fill
    const static int MAX_COLOR_TOLERANCE = 100;
    const static int MAX_STROKE_THICKNESS = 200;

    // New cosmic dropdowns replacing QComboBox widgets from UI
    CosmicDropDown* mReferenceLayerDropDown = nullptr;
    CosmicDropDown* mBlendModeDropDown = nullptr;
    CosmicDropDown* mGapFillDropDown = nullptr;

    // New Blender-style toggles replacing QCheckBox widgets
    CosmicCheckboxToggle* mColorReplaceToggle = nullptr;
    CosmicCheckboxToggle* mSameColorToggle = nullptr;
    CosmicCheckboxToggle* mMultiKeyframeFillToggle = nullptr;
    CosmicIgnoreColor*    mIgnoreColorWidget = nullptr;

    // Container panel for bitmap bucket options built with CosmicSplitToolLayout
    QWidget*             mCosmicBucketPanel = nullptr;

    CosmicInputField*     mStrokeThicknessField = nullptr;
    bool                 mStrokeThicknessEditActive = false;

    // Row labels for CosmicSplitToolLayout entries — needed for retranslation
    QLabel* mLabelLayer        = nullptr;
    QLabel* mLabelReplaceColor = nullptr;
    QLabel* mLabelSameColor    = nullptr;
    QLabel* mLabelMultiKF      = nullptr;
    QLabel* mLabelIgnoreColor  = nullptr;

protected:
    void changeEvent(QEvent* e) override;
};

#endif // BUCKETOPTIONSWIDGET_H
