# PenTool Migration to PencilTool Pipeline

## Objective
Migrate PenTool to use PencilTool's optimized rendering pipeline while maintaining simple solid circle output (non-textured). This will give PenTool the performance benefits of caching, threading, and screen-space spacing without changing visual appearance.

## Performance Gains Expected

### Current PenTool Performance
- 1000 dabs/sec (single-threaded)
- No caching (every dab rendered fresh)
- No screen-space optimization (same dab count at all zoom levels)
- Simple velocity-based spacing

### After Migration (Optimized PenTool)
- **800-1000 dabs/sec** with cache hits (multi-threaded)
- **100-dab LRU cache** (pre-rendered solid circles)
- **2-4x faster at low zoom** (screen-space spacing)
- **Velocity + curvature spacing** (better quality)
- **Multi-core utilization** (worker threads pre-render)

## Architecture Changes

### Current: PenTool Direct Rendering
```
drawStroke() → Calculate spacing → drawPen() 
→ TiledBuffer.drawBrush() → QPainter.drawEllipse()
```

### After: PenTool Cached Rendering
```
drawStroke() → Calculate spacing (screen-space) → paintAt()
→ prerenderCircleDab() [cached, threaded] → drawImageOverlay()
→ TiledBuffer.drawImage() → Composite pre-rendered circle
```

## Implementation Strategy

### Phase 1: Add Caching Infrastructure to PenTool ✅
1. Add dab cache (LRU, 100 entries)
2. Add cache key structure (size, antialiasing, color)
3. Add `prerenderCircleDab()` method (renders solid circle to QImage)
4. Add `schedulePrerenderDab()` for background pre-rendering

### Phase 2: Add Screen-Space Spacing ✅
1. Copy screen-space spacing logic from PencilTool
2. Calculate spacing based on screen pixels, not canvas pixels
3. Automatic 2-4x performance boost at low zoom

### Phase 3: Add Predictive Caching ✅
1. Add `warmNextDabs()` method
2. Predict future dab sizes based on velocity
3. Pre-render ahead of stroke for zero-latency

### Phase 4: Integrate with Existing PenTool ✅
1. Keep all existing features (pressure curves, smoothing, etc.)
2. Replace direct `drawPen()` calls with cached `paintAt()`
3. Maintain visual output identical to current

### Phase 5: Testing & Validation ✅
1. Verify visual output unchanged
2. Measure performance improvements
3. Test at various zoom levels
4. Test with/without antialiasing

## Key Differences from PencilTool

### PencilTool (Textured)
- Loads P1.png texture
- Applies rotation, masking, coloring
- Natural jitter effects
- Dual pressure curves

### Optimized PenTool (Solid)
- Renders solid circle (QPainter.drawEllipse)
- No texture, no rotation needed
- No jitter (clean output)
- Single pressure curve (width only)

## Code Reuse

### Can Reuse from PencilTool:
- ✅ Cache structure and management
- ✅ Screen-space spacing algorithm
- ✅ Predictive pre-warming logic
- ✅ Thread-safe dab pre-rendering pattern
- ✅ Adaptive spacing calculation

### PenTool-Specific:
- ❌ Solid circle rendering (no texture)
- ❌ Single pressure curve
- ❌ No jitter effects
- ❌ Existing Krita-style smoothing

## Benefits

1. **Performance**: 2-4x faster at low zoom
2. **Efficiency**: Cache eliminates redundant rendering
3. **Responsiveness**: Multi-threading keeps UI smooth
4. **Scalability**: Better CPU utilization on multi-core systems
5. **Consistency**: Both tools use same optimized architecture

## Risks & Mitigation

### Risk 1: Visual Output Changes
**Mitigation**: Extensive testing, side-by-side comparison with old PenTool

### Risk 2: Cache Memory Usage
**Mitigation**: Limited to 100 dabs (~2-5 MB), same as PencilTool

### Risk 3: Threading Complexity
**Mitigation**: Use proven thread-safe pattern from PencilTool

### Risk 4: Regression in Smoothing
**Mitigation**: Keep all existing smoothing algorithms intact

## Timeline Estimate

- Phase 1 (Caching): 2-3 hours
- Phase 2 (Screen-space): 1 hour  
- Phase 3 (Predictive): 1 hour
- Phase 4 (Integration): 2 hours
- Phase 5 (Testing): 1-2 hours

**Total**: 7-9 hours of development

## Success Metrics

- ✅ Visual output identical to current PenTool
- ✅ 2-4x performance improvement at 25% zoom
- ✅ Cache hit rate > 60%
- ✅ Multi-core CPU utilization
- ✅ No crashes or thread safety issues
- ✅ Antialiasing benefits from caching
