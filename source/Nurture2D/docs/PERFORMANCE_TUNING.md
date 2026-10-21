Pencil Tool Performance Tuning

Overview
The Pencil tool includes several performance optimizations that can significantly improve drawing responsiveness, especially on slower hardware or when using large brush sizes. These optimizations reduce overhead while maintaining full drawing quality.

Performance Features
1. Deferred Screen Updates
- What it does: Batches screen repaints to reduce Qt event loop overhead
- Default: Enabled, 16ms interval (~60fps)
- When to adjust: Increase interval (20-33ms) on slower systems; decrease (8-12ms) on fast systems
- Trade-off: Higher intervals reduce CPU usage but may make the stroke preview feel slightly delayed

2. Direct Image Compositing
- What it does: Bypasses QPainter overhead by compositing dabs directly at the pixel level
- Default: Enabled
- Performance gain: ~30-50% faster dab stamping
- When to disable: Only for debugging rendering issues
- Quality impact: None

3. Adaptive Batch Sizing
- What it does: Dynamically adjusts tile flush batch size based on drawing speed
- Default: Enabled
- How it works: Fast drawing (>100 dabs/sec) uses larger batches (up to 64); slow drawing uses smaller batches (down to 16)

4. Cache Warming
- What it does: Preloads common dab sizes into caches at stroke start
- Default: Enabled
- Performance gain: Reduces cache misses for the first dabs of each stroke

5. Input Event Throttling
- What it does: Limits processing of high-frequency tablet input events
- Default: Disabled
- Recommended interval: 4-8ms (125-250Hz effective rate)
- Trade-off: Slight reduction in stroke smoothness at very high speeds

Recommended Settings
- Fast Systems: Deferred Updates 12ms, Direct Compositing ON, Adaptive Batching ON, Cache Warming ON, Throttle OFF
- Slower Systems: Deferred Updates 25ms, Direct Compositing ON, Adaptive Batching ON, Cache Warming ON, Throttle ON @ 6ms
- Large Brushes (>100px): Deferred Updates 20ms, Direct Compositing ON, Adaptive Batching ON, Cache Warming ON, Throttle OFF

Measuring Performance
1. Enable performance logging in Preferences → Advanced
2. Draw a test stroke with and without optimizations
3. Check console output for timing metrics (drawTexturedDab, commitSegment)

Technical Details
- Deferred updates accumulate dirty regions and batch update() calls
- Direct compositing uses manual alpha blending on QImage scanlines
- Adaptive batching monitors dabs/second and adjusts batch sizes
- Cache warming precomputes common dab sizes
- Input throttling checks elapsed time between processed events

See core_lib/src/tool/penciltool.cpp for implementation details.

