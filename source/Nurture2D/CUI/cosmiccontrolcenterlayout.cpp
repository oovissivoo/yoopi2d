#include "cosmiccontrolcenterlayout.h"
#include <QDebug>
#include <QSet>
#include <algorithm>

CosmicControlCenterLayout::CosmicControlCenterLayout()
{
    std::fill(std::begin(mGrid), std::end(mGrid), -1);
}

// ── helpers ───────────────────────────────────────────────────────────────────

void CosmicControlCenterLayout::rebuildGrid()
{
    std::fill(std::begin(mGrid), std::end(mGrid), -1);
    for (int i = 0; i < mItems.size(); ++i) {
        const auto& item = mItems[i];
        if (item.cell >= 0 && item.cell < kCells)
            mGrid[item.cell] = i;
    }
}

int CosmicControlCenterLayout::findFreeRun(int span) const
{
    int run = 0;
    for (int c = 0; c < kCells; ++c) {
        if (mGrid[c] < 0) {
            ++run;
            if (run >= span) return c - span + 1;
        } else {
            run = 0;
        }
    }
    return -1;
}

// ── item registration ─────────────────────────────────────────────────────────

bool CosmicControlCenterLayout::addSingle(const QString& id, QAction* action)
{
    int cell = findFreeRun(1);
    if (cell < 0) return false;

    CosmicControlCenterItem item;
    item.type   = CosmicControlCenterItem::Type::Single;
    item.id     = id;
    item.cell   = cell;
    item.span   = 1;
    item.action = action;
    mItems.append(item);
    mGrid[cell] = mItems.size() - 1;
    return true;
}

bool CosmicControlCenterLayout::addGroup(const QString& groupId, const QVector<QAction*>& actions)
{
    const int span = actions.size();
    if (span <= 0) return false;

    int startCell = findFreeRun(span);
    if (startCell < 0) return false;

    for (int i = 0; i < span; ++i) {
        CosmicControlCenterItem item;
        item.type    = CosmicControlCenterItem::Type::Group;
        item.id      = groupId + QStringLiteral("_%1").arg(i);
        item.groupId = groupId;
        item.cell    = startCell + i;
        item.span    = (i == 0) ? span : 1; // span stored on first member only
        item.action  = actions[i];
        mItems.append(item);
        mGrid[startCell + i] = mItems.size() - 1;
    }
    return true;
}

void CosmicControlCenterLayout::addEmpty(int cell)
{
    // -1 means "find the next free cell automatically"
    if (cell < 0) {
        cell = findFreeRun(1);
        if (cell < 0) return; // grid full
    }
    if (cell >= kCells) return;
    if (mGrid[cell] >= 0) return; // already occupied

    CosmicControlCenterItem item;
    item.type = CosmicControlCenterItem::Type::Empty;
    item.id   = QStringLiteral("empty_%1").arg(cell);
    item.cell = cell;
    item.span = 1;
    mItems.append(item);
    mGrid[cell] = mItems.size() - 1;
}

// ── query ─────────────────────────────────────────────────────────────────────

CosmicControlCenterItem CosmicControlCenterLayout::itemAt(int cell) const
{
    if (cell < 0 || cell >= kCells) return {};
    int idx = mGrid[cell];
    if (idx < 0 || idx >= mItems.size()) return {};
    return mItems[idx];
}

QVector<int> CosmicControlCenterLayout::groupCells(int cell) const
{
    if (cell < 0 || cell >= kCells) return {};
    int idx = mGrid[cell];
    if (idx < 0) return {};
    const auto& item = mItems[idx];
    if (!item.isGroup()) return {};

    QVector<int> cells;
    const QString& gid = item.groupId;
    for (const auto& it : mItems) {
        if (it.groupId == gid && it.cell >= 0)
            cells.append(it.cell);
    }
    std::sort(cells.begin(), cells.end());
    return cells;
}

int CosmicControlCenterLayout::groupStart(int cell) const
{
    auto cells = groupCells(cell);
    return cells.isEmpty() ? -1 : cells.first();
}

int CosmicControlCenterLayout::groupSpan(int cell) const
{
    auto cells = groupCells(cell);
    return cells.isEmpty() ? 1 : cells.size();
}

// ── visibility ────────────────────────────────────────────────────────────────

void CosmicControlCenterLayout::hideByKey(const QString& key)
{
    // Save current cell positions before releasing them so showByKey can restore.
    QVector<int> savedCells;
    for (const auto& item : mItems) {
        bool matches = item.isGroup() ? (item.groupId == key) : (item.id == key);
        if (matches && item.cell >= 0)
            savedCells.append(item.cell);
    }
    if (!savedCells.isEmpty()) {
        std::sort(savedCells.begin(), savedCells.end());
        mSavedCells.insert(key, savedCells);
    }

    for (auto& item : mItems) {
        bool matches = item.isGroup() ? (item.groupId == key) : (item.id == key);
        if (!matches) continue;
        if (item.cell >= 0 && item.cell < kCells)
            mGrid[item.cell] = -1;
        item.cell = -1; // mark as unplaced
    }
}

void CosmicControlCenterLayout::showByKey(const QString& key)
{
    // Collect all items belonging to this key, in their stored order
    QVector<int> indices;
    for (int i = 0; i < mItems.size(); ++i) {
        const auto& item = mItems[i];
        bool matches = item.isGroup() ? (item.groupId == key) : (item.id == key);
        if (matches) indices.append(i);
    }
    if (indices.isEmpty()) return;

    const int span = indices.size();

    // Try to restore to the saved positions from before the hide.
    const QVector<int> saved = mSavedCells.value(key);
    bool canRestore = (saved.size() == span);
    if (canRestore) {
        for (int cell : saved) {
            if (cell < 0 || cell >= kCells || mGrid[cell] >= 0) {
                canRestore = false;
                break;
            }
        }
    }

    int startCell = -1;
    if (canRestore) {
        // Restore to original positions (may be non-contiguous for groups,
        // but in practice groups are always contiguous).
        for (int i = 0; i < indices.size(); ++i) {
            int cell = saved[i];
            mItems[indices[i]].cell = cell;
            mGrid[cell] = indices[i];
        }
        mSavedCells.remove(key);
        return;
    }

    // Saved positions are occupied — find the first free run of the right size.
    startCell = findFreeRun(span);
    if (startCell < 0) return; // no room

    for (int i = 0; i < indices.size(); ++i) {
        int cell = startCell + i;
        mItems[indices[i]].cell = cell;
        mGrid[cell] = indices[i];
    }
    mSavedCells.remove(key);
}

bool CosmicControlCenterLayout::isHiddenByKey(const QString& key) const
{
    for (const auto& item : mItems) {
        bool matches = item.isGroup() ? (item.groupId == key) : (item.id == key);
        if (matches) return item.cell < 0;
    }
    return false;
}

// ── drag / move ───────────────────────────────────────────────────────────────

// A logical block: an atomic unit that moves as one piece.
// Empty cells are NOT blocks — they are free space that gets filled after real blocks settle.
struct Block {
    QString key;
    int     anchor;
    int     span;
};

// Collect only real (non-empty) blocks, sorted by current anchor.
static QVector<Block> collectRealBlocks(const CosmicControlCenterLayout* layout)
{
    QVector<Block> blocks;
    QSet<QString> seenGroups;

    for (int c = 0; c < CosmicControlCenterLayout::kCells; ++c) {
        CosmicControlCenterItem item = layout->itemAt(c);
        if (!item.isValid() || item.isEmpty()) continue;

        if (item.isGroup()) {
            if (seenGroups.contains(item.groupId)) continue;
            seenGroups.insert(item.groupId);
            int start = layout->groupStart(c);
            int span  = layout->groupSpan(c);
            blocks.append({ item.groupId, start, span });
        } else {
            // Single
            blocks.append({ item.id, c, 1 });
        }
    }

    std::sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b){
        return a.anchor < b.anchor;
    });
    return blocks;
}

bool CosmicControlCenterLayout::moveItem(int fromCell, int toCell)
{
    if (fromCell < 0 || fromCell >= kCells) return false;
    if (toCell   < 0 || toCell   >= kCells) return false;

    CosmicControlCenterItem first = itemAt(fromCell);
    if (!first.isValid() || first.isEmpty()) return false;

    int anchor = fromCell;
    int span   = 1;
    QString dragKey;
    if (first.isGroup()) {
        anchor  = groupStart(fromCell);
        span    = groupSpan(fromCell);
        dragKey = first.groupId;
    } else {
        dragKey = first.id;
    }

    toCell = qBound(0, toCell, kCells - span);
    if (toCell == anchor) return false;

    // ── 1. Collect only real blocks (empties are free space, not obstacles) ───
    QVector<Block> blocks = collectRealBlocks(this);

    // ── 2. Set the dragged block's desired destination ────────────────────────
    for (Block& b : blocks) {
        if (b.key == dragKey) {
            b.anchor = toCell;
            break;
        }
    }

    // ── 3. Sort by desired anchor ─────────────────────────────────────────────
    std::sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b){
        return a.anchor < b.anchor;
    });

    // ── 4. Resolve overlaps with the dragged block as immovable anchor ────────
    // Find the dragged block's index after sort.
    int dragIdx = -1;
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks[i].key == dragKey) { dragIdx = i; break; }
    }

    if (dragIdx >= 0) {
        // Push blocks to the RIGHT of the dragged block rightward
        for (int i = dragIdx + 1; i < blocks.size(); ++i) {
            int prevTail = blocks[i-1].anchor + blocks[i-1].span;
            if (blocks[i].anchor < prevTail)
                blocks[i].anchor = prevTail;
        }
        // Push blocks to the LEFT of the dragged block leftward
        for (int i = dragIdx - 1; i >= 0; --i) {
            int nextStart = blocks[i+1].anchor;
            if (blocks[i].anchor + blocks[i].span > nextStart)
                blocks[i].anchor = nextStart - blocks[i].span;
        }
    }

    // ── 5. Clamp everything within [0, kCells) and re-resolve ─────────────────
    // Clamp left edge
    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks[i].anchor < 0) blocks[i].anchor = 0;
    }
    // Re-resolve right-side after left clamp (cascade right)
    for (int i = 1; i < blocks.size(); ++i) {
        int prevTail = blocks[i-1].anchor + blocks[i-1].span;
        if (blocks[i].anchor < prevTail)
            blocks[i].anchor = prevTail;
    }
    // Clamp right edge (cascade left)
    for (int i = blocks.size() - 1; i >= 0; --i) {
        int maxStart = kCells - blocks[i].span;
        if (blocks[i].anchor > maxStart) {
            blocks[i].anchor = maxStart;
            for (int j = i - 1; j >= 0; --j) {
                int prevMax = blocks[j+1].anchor - blocks[j].span;
                if (blocks[j].anchor > prevMax)
                    blocks[j].anchor = prevMax;
                else
                    break;
            }
        }
    }

    // ── 6. Build key → item-indices map from current grid state ───────────────
    QHash<QString, QVector<int>> keyToIndices;
    for (int c = 0; c < kCells; ++c) {
        int idx = mGrid[c];
        if (idx < 0) continue;
        const auto& item = mItems[idx];
        if (item.isEmpty()) continue; // empties are re-placed after
        QString key = item.isGroup() ? item.groupId : item.id;
        keyToIndices[key].append(idx);
        // indices are appended in cell order (c=0..49) so already sorted
    }

    // Also collect empty item indices
    QVector<int> emptyIndices;
    for (int c = 0; c < kCells; ++c) {
        int idx = mGrid[c];
        if (idx >= 0 && mItems[idx].isEmpty())
            emptyIndices.append(idx);
    }

    // ── 7. Clear grid and apply real blocks at their new positions ────────────
    std::fill(std::begin(mGrid), std::end(mGrid), -1);

    for (const Block& b : blocks) {
        const QVector<int>& indices = keyToIndices.value(b.key);
        for (int j = 0; j < indices.size(); ++j) {
            int newCell = b.anchor + j;
            if (newCell >= 0 && newCell < kCells) {
                mItems[indices[j]].cell = newCell;
                mGrid[newCell] = indices[j];
            }
        }
    }

    // ── 8. Re-place empty items into whatever cells are still free ────────────
    // This preserves the user's intentional spacers in the gaps between real blocks.
    int emptyIdx = 0;
    for (int c = 0; c < kCells && emptyIdx < emptyIndices.size(); ++c) {
        if (mGrid[c] < 0) {
            int idx = emptyIndices[emptyIdx++];
            mItems[idx].cell = c;
            mGrid[c] = idx;
        }
    }

    return true;
}

// ── persistence ───────────────────────────────────────────────────────────────

void CosmicControlCenterLayout::save(QSettings& s, const QString& prefix) const
{
    s.beginGroup(prefix);
    s.remove(QString()); // clear old keys

    s.setValue(QStringLiteral("count"), mItems.size());
    for (int i = 0; i < mItems.size(); ++i) {
        const auto& item = mItems[i];
        s.beginGroup(QStringLiteral("item_%1").arg(i));
        s.setValue(QStringLiteral("id"),      item.id);
        s.setValue(QStringLiteral("groupId"), item.groupId);
        s.setValue(QStringLiteral("cell"),    item.cell);
        s.setValue(QStringLiteral("span"),    item.span);
        s.setValue(QStringLiteral("type"),    static_cast<int>(item.type));
        s.endGroup();
    }

    // Persist saved (pre-hide) cell positions so they survive app restarts.
    s.beginGroup(QStringLiteral("savedCells"));
    s.remove(QString());
    for (auto it = mSavedCells.constBegin(); it != mSavedCells.constEnd(); ++it) {
        QStringList cellStrs;
        for (int c : it.value()) cellStrs.append(QString::number(c));
        s.setValue(it.key(), cellStrs.join(QLatin1Char(',')));
    }
    s.endGroup();

    s.endGroup();
}

void CosmicControlCenterLayout::load(QSettings& s, const QString& prefix)
{
    s.beginGroup(prefix);
    const int count = s.value(QStringLiteral("count"), 0).toInt();

    // Build a cell->id map from saved data
    QVector<QPair<int,QString>> saved; // (cell, id)
    for (int i = 0; i < count; ++i) {
        s.beginGroup(QStringLiteral("item_%1").arg(i));
        int  cell = s.value(QStringLiteral("cell"), -1).toInt();
        QString id = s.value(QStringLiteral("id")).toString();
        s.endGroup();
        if (cell >= 0 && cell < kCells)
            saved.append({cell, id});
    }

    // Restore saved (pre-hide) cell positions
    mSavedCells.clear();
    s.beginGroup(QStringLiteral("savedCells"));
    const QStringList savedKeys = s.childKeys();
    for (const QString& key : savedKeys) {
        const QString val = s.value(key).toString();
        if (val.isEmpty()) continue;
        QVector<int> cells;
        for (const QString& part : val.split(QLatin1Char(','))) {
            bool ok = false;
            int c = part.toInt(&ok);
            if (ok && c >= 0 && c < kCells) cells.append(c);
        }
        if (!cells.isEmpty()) mSavedCells.insert(key, cells);
    }
    s.endGroup();

    s.endGroup();

    // Re-apply saved cell positions to existing items by matching id
    for (const auto& pair : saved) {
        for (auto& item : mItems) {
            if (item.id == pair.second) {
                item.cell = pair.first;
                break;
            }
        }
    }
    rebuildGrid();
}

void CosmicControlCenterLayout::normalise()
{
    // Fill any truly empty cells with explicit Empty items so the grid is dense
    rebuildGrid();
    for (int c = 0; c < kCells; ++c) {
        if (mGrid[c] < 0) addEmpty(c);
    }
}
