/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Vector.h>
#include <UI/Qt/TabFolderTree.h>

// How a window's folder tree and its native tab strip stay in step, and what the vertical sidebar shows of them. The
// strip stays the authority for which tabs exist, which is current and which are pinned; the tree decides the order of
// the unpinned tabs and where each one sits. Pinned tabs keep their tiles at the top of the sidebar, in the strip's
// order, while still belonging to whatever folder they were in.
namespace Ladybird::TabFolderSync {

using TabKey = TabFolderTree::TabKey;
using Item = TabFolderTree::Item;

struct WindowTab {
    TabKey key { 0 };
    bool pinned { false };
};

struct SidebarRow {
    Item item;
    size_t depth { 0 };
    // The active tab, shown under the outermost collapsed folder around it.
    bool preview { false };
    TabFolderTree::FolderGlyph glyph { TabFolderTree::FolderGlyph::Open };

    bool operator==(SidebarRow const&) const = default;
};

// Brings the tree in line with the window's tabs. A tab that's gone leaves, and the folders stay. A new tab goes right
// after the tab before it of the same kind (pinned or not), in that tab's folder, or else right before the tab after it.
// An explicit New Tab instead goes at the end of the root. Without folders, the tree simply takes the window's order.
void reconcile(TabFolderTree&, Vector<WindowTab> const&, Optional<TabKey> new_tab_at_root = {});

// The window moved an unpinned tab, as the horizontal strip does: the tab follows to the unpinned tab now before it, or
// else to the one now after it. A pinned tab keeps its place in the tree.
void follow_window_move(TabFolderTree&, Vector<WindowTab> const&, TabKey moved);

// The horizontal strip moved an unpinned tab. Like follow_window_move(), but a tab that lands beside a collapsed
// top-level folder it wasn't in goes beside that folder at the root, rather than into a group whose tabs are hidden.
void follow_strip_move(TabFolderTree&, Vector<WindowTab> const&, TabKey moved);

// The order the native strip should have: the pinned tabs as the window has them, then the unpinned ones in tree order.
Vector<TabKey> window_order(TabFolderTree const&, Vector<WindowTab> const&);

// One move of a tab within the strip, from the index it has to the one it takes.
struct Move {
    size_t from { 0 };
    size_t to { 0 };

    bool operator==(Move const&) const = default;
};
// The moves, in order, that bring the strip from the order it has to the wanted one, which lists the same tabs. Each
// move is from the index its tab has once the moves before it are done, the way a strip's own moves are.
Vector<Move> moves_to_reorder(Vector<TabKey> const& current, Vector<TabKey> const& wanted);

// The sidebar's rows below the pinned tiles. Pinned tabs have no row, and only an unpinned active tab previews.
Vector<SidebarRow> sidebar_rows(TabFolderTree const&, Vector<WindowTab> const&, Optional<TabKey> active);

// The horizontal strip's view of the folders, modeled on Chromium's tab groups: each top-level folder is a chip that
// comes right before the folder's first unpinned tab, and all of that folder's tabs, at any depth, belong to its group.
// Nested folders keep their places in the tree, and are only reached through their top-level chip. A collapsed chip hides
// its tabs, except the active one, so the page on screen always has its tab. Pinned tabs show no group.
struct StripGroup {
    TabFolderId folder { 0 };
    // The unpinned tab the chip comes right before, whether shown or not, or nothing for a chip after every tab.
    Optional<TabKey> before_tab;
    bool collapsed { false };

    bool operator==(StripGroup const&) const = default;
};
struct StripGroups {
    Vector<StripGroup> chips;
    // For each of the window's tabs, in the window's order: the top-level folder it's in, and whether it's hidden.
    Vector<Optional<TabFolderId>> group_of_tab;
    Vector<bool> hidden;
};
StripGroups strip_groups(TabFolderTree const&, Vector<WindowTab> const&, Optional<TabKey> active);

// The tabs of the visible rows from one tab's row to another's, in row order, or nothing if either has no row.
Vector<TabKey> tab_range(Vector<SidebarRow> const&, TabKey anchor, TabKey target);

// The rows without the items being dragged and anything inside them.
Vector<SidebarRow> rows_without(TabFolderTree const&, Vector<SidebarRow> const&, Vector<Item> const& moving);

// Where items land when dropped in the gap before rows[gap], of rows that leave out the items moving. Below an open or
// previewing folder's header, that's the start of the folder; before any other row, that row's place; past the last
// row, the end of the root. The index counts the siblings as they are before the move.
Optional<TabFolderDestination> destination_for_gap(TabFolderTree const&, Vector<SidebarRow> const& rows, size_t gap);

}
