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
#ifndef CANVASPALETTEOVERLAY_H
#define CANVASPALETTEOVERLAY_H

#include <QWidget>
#include <QColor>
#include <QPoint>
class QRegion;
class QEvent;

class PenColorPalette;
class AutoLineColorToggle;
class Editor;
class CosmicCheckboxToggle;
class QBoxLayout;
class QContextMenuEvent;

class CanvasPaletteOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasPaletteOverlay(Editor* editor, QWidget* parent = nullptr);
    bool isPresetColor(const QColor& color) const;
    PenColorPalette* getPalette() const { return mPalette; }
    QWidget* getControlsContainer() const { return mControlsContainer; }
    AutoLineColorToggle* getAutoLineToggle() const { return mAutoLineToggle; }

signals:
    void colorChanged(const QColor& color);

public slots:
    void setColorFromExternal(const QColor& color);
    void updatePosition();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    Editor* mEditor = nullptr;
    PenColorPalette* mPalette = nullptr;
    QWidget* mPenContainer = nullptr;
    QWidget* mControlsContainer = nullptr; // Container for future controls
    QWidget* mRowSeparator = nullptr;      // Separator between controls and pens
    AutoLineColorToggle* mAutoLineToggle = nullptr; // Auto-line fill color toggles
    CosmicCheckboxToggle* mExpandToggle = nullptr; // Expand / collapse auto-line controls
    int mHeaderHeight = 42; // Fixed height for the header bar
    int mExpandedWidth = -1; // Remember width when expanded so collapse keeps width

    // Layout building blocks for horizontal/vertical orientations
    QBoxLayout* mMainLayout = nullptr;        // Root layout for the overlay contents
    QWidget* mContentContainer = nullptr;     // Holds controls + separator + pens
    QBoxLayout* mContentLayout = nullptr;     // Direction switches between rows/columns
    QBoxLayout* mControlsLayout = nullptr;    // Layout inside mControlsContainer
    QBoxLayout* mPenLayout = nullptr;         // Layout inside mPenContainer

    enum class DockLocation {
        Left,
        Right,
    };

    enum class Orientation {
        Horizontal,
        Vertical,
    };

    DockLocation mDockLocation = DockLocation::Left;
    Orientation mOrientation = Orientation::Horizontal;
    bool mSwapLayout = false; // When true, pen and bucket UI positions are flipped

    void applyOrientation();
    void showOptionsMenu(const QPoint& globalPos);
};

#endif // CANVASPALETTEOVERLAY_H


