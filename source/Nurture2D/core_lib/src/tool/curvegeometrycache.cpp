#include "curvegeometrycache.h"
#include <algorithm>

void CurveGeometryCache::resize(int numPoints)
{
    // Number of segments = numPoints - 1 for open curves
    // For BSpline closed curves, it's numPoints (wraps around)
    // We'll size based on worst case (closed) and caller manages actual usage
    const int segmentCount = qMax(0, numPoints);
    
    if (mSegments.size() != segmentCount) {
        mSegments.resize(segmentCount);
        // New segments start dirty
        for (auto& seg : mSegments) {
            seg.isDirty = true;
        }
    }
}

void CurveGeometryCache::markSegmentDirty(int segmentIndex)
{
    if (segmentIndex >= 0 && segmentIndex < mSegments.size()) {
        mSegments[segmentIndex].isDirty = true;
    }
}

void CurveGeometryCache::markNearbySegmentsDirty(int pointIndex)
{
    // For a point at index i, mark segments [i-1, i, i+1] as dirty
    // This handles the segment before and after the edited point
    const int before = pointIndex - 1;
    const int current = pointIndex;
    const int after = pointIndex + 1;
    
    markSegmentDirty(before);
    markSegmentDirty(current);
    markSegmentDirty(after);
}

void CurveGeometryCache::markAllDirty()
{
    for (auto& seg : mSegments) {
        seg.isDirty = true;
    }
}

bool CurveGeometryCache::isSegmentDirty(int segmentIndex) const
{
    if (segmentIndex >= 0 && segmentIndex < mSegments.size()) {
        return mSegments[segmentIndex].isDirty;
    }
    return true; // Out of bounds = assume dirty
}

CurveSegmentCache& CurveGeometryCache::getSegment(int segmentIndex)
{
    static CurveSegmentCache dummy;
    if (segmentIndex >= 0 && segmentIndex < mSegments.size()) {
        return mSegments[segmentIndex];
    }
    return dummy;
}

const CurveSegmentCache& CurveGeometryCache::getSegment(int segmentIndex) const
{
    static CurveSegmentCache dummy;
    if (segmentIndex >= 0 && segmentIndex < mSegments.size()) {
        return mSegments[segmentIndex];
    }
    return dummy;
}

void CurveGeometryCache::clear()
{
    mSegments.clear();
}
