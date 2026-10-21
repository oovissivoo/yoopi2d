#ifndef CELLLAYERSTACK_H
#define CELLLAYERSTACK_H

#include <QList>
#include <QString>

#include "innerlayer.h"

class BitmapImage;
class QUndoStack;

class CellLayerStack
{
public:
    // Creates one default InnerLayer named "Layer 1"
    CellLayerStack();

    // Takes ownership of an existing image, wraps it as InnerLayer[0] named "Layer 1"
    explicit CellLayerStack(BitmapImage* existingImage);

    ~CellLayerStack();

    // Non-copyable
    CellLayerStack(const CellLayerStack&)            = delete;
    CellLayerStack& operator=(const CellLayerStack&) = delete;

    // Deep copy for cell duplication
    CellLayerStack* deepCopy() const;

    // Layer management
    // Inserts a new InnerLayer above aboveIndex (or above current active if -1).
    // Returns the new layer's index.
    int  addLayer(const QString& name, int aboveIndex = -1);
    // No-op when layerCount() == 1; otherwise removes layer at index,
    // discards its QUndoStack, and adjusts mActiveIndex.
    void removeLayer(int index);
    int  layerCount() const;

    // Active layer
    int  activeIndex() const;
    void setActiveIndex(int index);
    InnerLayer*       activeLayer();
    const InnerLayer* activeLayer() const;

    // Layer access
    InnerLayer*       layerAt(int index);
    const InnerLayer* layerAt(int index) const;

    // Composite
    BitmapImage* composite();       // returns cached composite, recomputes if dirty
    void         invalidateComposite();

    // Move a layer from one index to another (for drag reorder)
    void moveLayer(int fromIndex, int toIndex);

    // Undo stack for active layer
    QUndoStack* activeUndoStack();

private:
    void recomputeComposite();

    QList<InnerLayer*> mLayers;       // owned pointers
    int                mActiveIndex    = 0;
    BitmapImage*       mComposite      = nullptr;  // owned
    bool               mCompositeDirty = true;
};

#endif // CELLLAYERSTACK_H
