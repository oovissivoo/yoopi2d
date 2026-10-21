#ifndef COSMICCONTROLCENTERLAYOUT_H
#define COSMICCONTROLCENTERLAYOUT_H

#include "cosmiccontrolcenteritem.h"
#include <QVector>
#include <QString>
#include <QHash>
#include <QSettings>

/**
 * CosmicControlCenterLayout
 *
 * Owns the 50-cell grid and all placement/persistence logic.
 * Pure data — no Qt widgets, no painting.
 *
 * Grid rules:
 *  - 50 fixed cells (indices 0-49).
 *  - Each cell holds at most one item.
 *  - A Group occupies consecutive cells; all members share a groupId.
 *  - Empty cells are explicit items of type Empty.
 *
 * Drag / push rules:
 *  - Moving a single item or a group pushes blocking items along the row.
 *  - If the destination + span would exceed cell 49, the move is clamped.
 *  - Items are never destroyed by a push — they wrap around the freed space.
 */
class CosmicControlCenterLayout
{
public:
    static constexpr int kCells = 50;

    CosmicControlCenterLayout();

    // ── item registration ────────────────────────────────────────────────────
    /** Append a single-cell item at the next free cell. Returns false if full. */
    bool addSingle(const QString& id, QAction* action);

    /** Append a group starting at the next free run of `span` cells. */
    bool addGroup(const QString& groupId, const QVector<QAction*>& actions);

    /** Insert an explicit empty spacer at `cell`. */
    void addEmpty(int cell);

    // ── query ────────────────────────────────────────────────────────────────
    /** Item at cell index, or an invalid item if the cell is truly unoccupied. */
    CosmicControlCenterItem itemAt(int cell) const;

    /** All cells that belong to the same group as `cell`. Empty if not a group. */
    QVector<int> groupCells(int cell) const;

    /** First cell of the group that owns `cell` (-1 if not a group). */
    int groupStart(int cell) const;

    /** Span (width in cells) of the group that owns `cell` (1 if single/empty). */
    int groupSpan(int cell) const;

    const QVector<CosmicControlCenterItem>& items() const { return mItems; }

    // ── visibility ───────────────────────────────────────────────────────────
    /**
     * Hide a group or single by key (groupId or item id).
     * Releases their cells back to the grid. Items remain in mItems with cell=-1.
     */
    void hideByKey(const QString& key);

    /**
     * Show a previously hidden group or single by key.
     * Tries to restore to the last known cell positions; if those cells are
     * occupied, finds the next free run and places it there.
     */
    void showByKey(const QString& key);

    bool isHiddenByKey(const QString& key) const;

    // ── drag / move ──────────────────────────────────────────────────────────
    /**
     * Move the item (or group) whose anchor is at `fromCell` so that its
     * first cell lands at `toCell`.  Blocking items are pushed rightward
     * (or leftward if there is no room on the right).
     * Returns true if the move was applied.
     */
    bool moveItem(int fromCell, int toCell);

    // ── persistence ──────────────────────────────────────────────────────────
    void save(QSettings& s, const QString& prefix) const;
    void load(QSettings& s, const QString& prefix);

    /** Rebuild the grid from scratch (called after load to fill gaps). */
    void normalise();

private:
    // mGrid[i] = index into mItems, or -1 for truly empty (no explicit spacer)
    int  mGrid[kCells];
    QVector<CosmicControlCenterItem> mItems;
    // Saved cell positions for hidden groups/singles, keyed by groupId or item id.
    // Populated by hideByKey so showByKey can restore the original position.
    QHash<QString, QVector<int>> mSavedCells;

    void rebuildGrid();
    int  findFreeRun(int span) const;   // first cell of a free run of `span`
    void shiftRight(int fromCell, int count); // push items right by `count`
    void shiftLeft (int fromCell, int count); // push items left  by `count`
};

#endif // COSMICCONTROLCENTERLAYOUT_H
