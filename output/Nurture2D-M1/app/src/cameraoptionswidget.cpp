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
#include "cameraoptionswidget.h"
#include "ui_cameraoptionswidget.h"

#include <QEvent>

#include "editor.h"
#include "toolmanager.h"
#include "layermanager.h"

#include "cameratool.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/blenderstyletoggle.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include <QHBoxLayout>

CameraOptionsWidget::CameraOptionsWidget(Editor* editor, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CameraOptionsWidget), mEditor(editor)
{
    ui->setupUi(this);

    auto toolMan = mEditor->tools();

    // Replace showCameraPathCheckBox with BlenderStyleToggle
    if (!mShowCameraPathToggle && ui->showCameraPathCheckBox) {
        mShowCameraPathToggle = new BlenderStyleToggle(this);
        mShowCameraPathToggle->setObjectName("showCameraPathToggle");
        mShowCameraPathToggle->setText(ui->showCameraPathCheckBox->text());
        mShowCameraPathToggle->setToolTip(ui->showCameraPathCheckBox->toolTip());
        mShowCameraPathToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        if (auto layoutHost = this->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_5"))) {
            int idx = layoutHost->indexOf(ui->showCameraPathCheckBox);
            if (idx < 0) idx = 0;
            layoutHost->insertWidget(idx, mShowCameraPathToggle);
            ui->showCameraPathCheckBox->hide();
        } else if (ui->showCameraPathCheckBox->parentWidget() && ui->showCameraPathCheckBox->parentWidget()->layout()) {
            ui->showCameraPathCheckBox->parentWidget()->layout()->addWidget(mShowCameraPathToggle);
            ui->showCameraPathCheckBox->hide();
        }
        connect(mShowCameraPathToggle, &BlenderStyleToggle::toggled, toolMan, &ToolManager::setShowCameraPath);
    }
    // Keep legacy checkbox connection as fallback (hidden)
    connect(ui->showCameraPathCheckBox, &QCheckBox::clicked, toolMan, &ToolManager::setShowCameraPath);

    // Replace pathColorComboBox with CosmicDropDown
    if (!mPathColorDropDown && ui->pathColorComboBox) {
        mPathColorDropDown = new CosmicDropDown(this);
        mPathColorDropDown->setOptions({tr("Red"), tr("Blue"), tr("Green"), tr("Black"), tr("White")});
        if (auto layoutHost = this->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_5"))) {
            int idx = layoutHost->indexOf(ui->pathColorComboBox);
            if (idx < 0) idx = layoutHost->count();
            layoutHost->insertWidget(idx, mPathColorDropDown);
            ui->pathColorComboBox->hide();
        } else {
            ui->pathColorComboBox->parentWidget()->layout()->addWidget(mPathColorDropDown);
            ui->pathColorComboBox->hide();
        }
        connect(mPathColorDropDown, &CosmicDropDown::currentIndexChanged, toolMan, &ToolManager::setCameraPathDotColor);
    }
    connect(ui->btnResetPath, &QPushButton::clicked, toolMan, &ToolManager::resetCameraPath);

    connect(ui->resetAllButton, &QPushButton::clicked, toolMan, [=] {
        toolMan->resetCameraTransform(CameraFieldOption::RESET_FIELD);
    });
    connect(ui->resetTranslationButton, &QPushButton::clicked, toolMan, [=] {
        toolMan->resetCameraTransform(CameraFieldOption::RESET_TRANSLATION);
    });
    connect(ui->resetRotationButton, &QPushButton::clicked, toolMan, [=] {
        toolMan->resetCameraTransform(CameraFieldOption::RESET_ROTATION);
    });
    connect(ui->resetScaleButton, &QPushButton::clicked, toolMan, [=] {
        toolMan->resetCameraTransform(CameraFieldOption::RESET_SCALING);
    });

    connect(toolMan, &ToolManager::toolPropertyChanged, this, &CameraOptionsWidget::onToolPropertyChanged);

    connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, &CameraOptionsWidget::updateUI);
    connect(mEditor->tools(), &ToolManager::toolChanged, this, &CameraOptionsWidget::updateUI);

    mCameraTool = static_cast<CameraTool*>(mEditor->tools()->getTool(CAMERA));
}

// No-op placeholder removed; MainWindow2 handles global accent updates.

CameraOptionsWidget::~CameraOptionsWidget()
{
    delete ui;
}

void CameraOptionsWidget::updateUI()
{

    Q_ASSERT(mCameraTool->type() == CAMERA);

    Properties p = mCameraTool->properties;

    setShowCameraPath(p.cameraShowPath);
    setPathDotColorType(p.cameraPathDotColorType);
}

void CameraOptionsWidget::onToolPropertyChanged(ToolType, ToolPropertyType ePropertyType)
{
    const Properties& p = mCameraTool->properties;

    switch (ePropertyType)
    {
    case CAMERAPATH: { setShowCameraPath(p.cameraShowPath); break; }
    default:
        break;
    }
}

void CameraOptionsWidget::setShowCameraPath(bool showCameraPath)
{
    if (mShowCameraPathToggle) {
        QSignalBlocker b(mShowCameraPathToggle);
        mShowCameraPathToggle->setChecked(showCameraPath);
    }
    QSignalBlocker b2(ui->showCameraPathCheckBox);
    ui->showCameraPathCheckBox->setChecked(showCameraPath);
}

void CameraOptionsWidget::setPathDotColorType(DotColorType index)
{
    if (mPathColorDropDown) {
        QSignalBlocker b(mPathColorDropDown);
        mPathColorDropDown->setCurrentIndex(static_cast<int>(index));
    }
}

void CameraOptionsWidget::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);

        // Update the path color dropdown options
        if (mPathColorDropDown) {
            const int idx = mPathColorDropDown->currentIndex();
            QSignalBlocker b(mPathColorDropDown);
            auto safetr = [this](const char* src) -> QString {
                const QString t = tr(src);
                return t.isEmpty() ? QString::fromUtf8(src) : t;
            };
            mPathColorDropDown->setOptions({ safetr("Red"), safetr("Blue"), safetr("Green"), safetr("Black"), safetr("White") });
            mPathColorDropDown->setCurrentIndex(idx);
        }

        // Update the show camera path toggle text (mirrors the hidden checkbox)
        if (mShowCameraPathToggle && ui->showCameraPathCheckBox) {
            const QString t = ui->showCameraPathCheckBox->text();
            if (!t.isEmpty()) mShowCameraPathToggle->setText(t);
        }
    }
}
