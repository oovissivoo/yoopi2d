# Hybrid Canvas Stack Implementation

## Overview

The Hybrid Canvas Stack implements a 3-level drawing system inspired by Clip Studio Paint's approach, combining the performance benefits of vector drawing with the visual fidelity of raster output.

## Architecture

### Three Canvas Levels

1. **Top Canvas (Real-time Preview)**
   - Uses `ScribbleArea::mTiledBuffer` for immediate stroke feedback
   - Cleared at stroke start via `HybridCanvasStack::beginStroke()`
   - Provides smooth real-time drawing experience

2. **Middle Canvas (Vector Storage)**
   - Stores complete stroke data in `VectorImage` with enhanced metadata
   - Uses `EnhancedVectorStroke` for brush settings, pressure curves, texture stamps
   - Hidden from normal display but available for future vector editing
   - Persists stroke data for re-rendering and manipulation

3. **Bottom Canvas (Raster Output)**
   - Dedicated bitmap layer with "_raster" suffix
   - Contains the final visual output users see
   - Automatically updated when vector strokes are modified
   - Optimized for display performance

## Key Components

### HybridCanvasStack
- **Location**: `core_lib/src/interface/hybridcanvasstack.h/.cpp`
- **Purpose**: Facade managing the 3-canvas lifecycle
- **Key Methods**:
  - `beginStroke()`: Clears top preview buffer
  - `updateStroke()`: Handles real-time updates (placeholder)
  - `endStroke()`: Commits stroke to vector storage
  - `rerenderRasterFromVectors()`: Bakes vector data to raster layer
  - `requestRerenderBatch()`: Performance optimization for multiple updates

### EnhancedVectorStroke
- **Location**: `core_lib/src/graphics/vector/enhancedvectorstroke.h/.cpp`
- **Purpose**: Extended vector stroke model with comprehensive brush metadata
- **Features**:
  - Brush settings (tool type, width, feather, pressure, AA)
  - Texture data (preset, spacing, scatter, rotation, scale, flow)
  - Pressure curves for pen/pencil tools
  - Size/opacity curves for texture effects
  - Dab timing for re-stamping along paths
  - XML serialization for persistence

## Tool Integration

All drawing tools now follow the hybrid canvas workflow:

### Stroke Tools (PencilTool, PenTool, BrushTool)
- Inherit from `StrokeTool` which calls hybrid stack lifecycle
- Vector strokes stored in middle canvas with enhanced metadata
- Raster output automatically generated and displayed

### Path Tools (CurveTool, BSplineTool, PolylineTool)
- Direct integration with hybrid stack via `rerenderRasterFromVectors()`
- Vector paths stored for future editing
- Raster output committed to bottom canvas

## Performance Optimizations

### Batching
- Multiple rerender requests are batched using `requestRerenderBatch()`
- Uses `QTimer::singleShot(0)` for requestAnimationFrame-like behavior
- Expands bounds to include multiple update regions

### Dirty Rectangles
- Only affected regions are re-rendered when possible
- Canvas bounds hints minimize unnecessary work

### Memory Management
- Vector data stored efficiently with metadata compression
- Raster layers created on-demand
- Automatic cleanup of temporary preview buffers

## Usage

### For Tool Developers
```cpp
// In tool's paintVectorStroke() method:
if (mScribbleArea) {
    mScribbleArea->hybridStack()->rerenderRasterFromVectors(mEditor->currentFrame());
}
```

### For Canvas Operations
```cpp
// Access hybrid stack from ScribbleArea:
auto* stack = mScribbleArea->hybridStack();
stack->beginStroke();        // Clear preview
stack->updateStroke(rect);   // Update region
stack->endStroke();          // Commit to vector
```

## Benefits

1. **Performance**: Vector drawing is faster than raster for complex strokes
2. **Editability**: Vector data preserved for future manipulation
3. **Visual Fidelity**: Final output is high-quality raster
4. **Scalability**: System handles complex drawings efficiently
5. **Extensibility**: Enhanced metadata supports advanced brush features

## Future Enhancements

- Vector stroke editing tools
- Advanced brush engine integration
- GPU acceleration for raster baking
- Layer blending modes
- Animation-aware vector interpolation
