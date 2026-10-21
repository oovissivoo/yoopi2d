#include "innerlayer.h"

#include "bitmapimage.h"

#include <QUndoStack>
#include <QPainter>
#include <algorithm>

QPainter::CompositionMode blendModeToCompositionMode(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:
        return QPainter::CompositionMode_SourceOver;
    case BlendMode::Multiply:
        return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen:
        return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay:
        return QPainter::CompositionMode_Overlay;
    case BlendMode::Luminosity:
        return QPainter::CompositionMode_Lighten;
    }
    return QPainter::CompositionMode_SourceOver;
}

InnerLayer::InnerLayer(const QString& layerName)
    : image(new BitmapImage)
    , undoStack(new QUndoStack)
    , name(layerName)
{
}

InnerLayer::InnerLayer(BitmapImage* existingImage, const QString& layerName)
    : image(existingImage)
    , undoStack(new QUndoStack)
    , name(layerName)
{
}

InnerLayer::~InnerLayer()
{
    delete image;
    delete undoStack;
}

InnerLayer::InnerLayer(InnerLayer&& other) noexcept
    : image(other.image)
    , undoStack(other.undoStack)
    , name(std::move(other.name))
    , visible(other.visible)
    , opacity(other.opacity)
    , blendMode(other.blendMode)
{
    other.image     = nullptr;
    other.undoStack = nullptr;
}

InnerLayer& InnerLayer::operator=(InnerLayer&& other) noexcept
{
    if (this != &other) {
        delete image;
        delete undoStack;

        image     = other.image;
        undoStack = other.undoStack;
        name      = std::move(other.name);
        visible   = other.visible;
        opacity   = other.opacity;
        blendMode = other.blendMode;

        other.image     = nullptr;
        other.undoStack = nullptr;
    }
    return *this;
}

void InnerLayer::setOpacity(qreal value)
{
    opacity = qBound(0.0, value, 1.0);
}
