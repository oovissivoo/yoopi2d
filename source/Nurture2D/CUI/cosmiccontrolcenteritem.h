#ifndef COSMICCONTROLCENTERITEM_H
#define COSMICCONTROLCENTERITEM_H

#include <QString>
#include <QAction>

/**
 * Describes one logical item in the CosmicControlCenter grid.
 *
 * Single  — occupies exactly 1 cell, wraps one QAction.
 * Group   — occupies `span` consecutive cells, all sharing the same groupId.
 *           Every member cell is stored as a separate item with the same
 *           groupId and increasing cellIndex. They are always moved together.
 * Empty   — a blank spacer cell the user can place intentionally.
 */
struct CosmicControlCenterItem
{
    enum class Type { Single, Group, Empty };

    Type    type      = Type::Empty;
    QString id;           // unique stable id (used for QSettings persistence)
    QString groupId;      // non-empty for Group members; all members share this
    int     cell      = -1; // 0-based slot in the 50-cell grid
    int     span      = 1;  // number of cells this item occupies (always 1 per struct,
                             // but span on the *first* member tells the group width)
    QAction* action   = nullptr; // may be null for Empty

    bool isValid()  const { return cell >= 0; }
    bool isEmpty()  const { return type == Type::Empty; }
    bool isGroup()  const { return type == Type::Group; }
    bool isSingle() const { return type == Type::Single; }
};

#endif // COSMICCONTROLCENTERITEM_H
