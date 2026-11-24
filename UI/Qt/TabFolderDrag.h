/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Vector.h>
#include <UI/Qt/TabFolderSync.h>
#include <UI/Qt/TabFolderTree.h>

// Where tabs and folders dragged along the vertical sidebar's rows land. The rows that stay make way for the ones moving
// at a gap between two of them, and how far the dragged card sits to the right picks the depth there: below a folder's
// last row, a card held to the left leaves the folder, and one held to the right stays in it. Nothing here knows about
// pixels; the tab bar turns the card's place into a fractional gap and depth.
namespace Ladybird::TabFolderDrag {

using Item = TabFolderTree::Item;
using TabFolderSync::SidebarRow;

// The depths that items dropped in the gap before rows[gap] can take, of rows that leave out the items moving. The
// deepest is inside the row above when that's a folder, or beside it otherwise; the shallowest is beside the row below,
// or the root past the last row.
struct DepthRange {
    size_t min { 0 };
    size_t max { 0 };
};
DepthRange depth_range(Vector<SidebarRow> const& rows, size_t gap);

// Where items dropped in the gap, at the depth, land, or nothing for a depth that the gap doesn't offer. At the depth of
// the row below, that's that row's place; inside the folder above, the start of an open one or the end of a collapsed
// one; at any other depth, right after the row above that's at the depth, which is the folder around the row above, or
// the row itself. An active preview stands for its branch inside the visible folder. This says nothing of whether the
// items may go there; see TabFolderTree::can_move_items().
Optional<TabFolderDestination> destination(TabFolderTree const&, Vector<SidebarRow> const& rows, size_t gap, size_t depth);

struct Target {
    size_t gap { 0 };
    size_t depth { 0 };
    TabFolderDestination destination;
};
// The place for the items when the card is at a fractional gap and depth: the nearest gap, then the nearest depth there
// that the items may move to, or nothing when no depth there takes them.
Optional<Target> target_for(TabFolderTree const&, Vector<SidebarRow> const& rows, Vector<Item> const& moving, double gap, double depth);

// A hit on the folder itself keeps the preview gap still and appends to that folder, or rejects the drop.
Optional<Target> target_for_folder(TabFolderTree const&, SidebarRow const&, Vector<Item> const& moving, size_t preview_gap);

// What a drag that starts on an item carries: the whole selection when the item is part of it, the item alone otherwise.
Vector<Item> items_to_drag(Item pressed, Vector<Item> const& selection);

}
