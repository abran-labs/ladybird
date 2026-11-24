/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Math.h>
#include <UI/Qt/TabFolderDrag.h>

namespace Ladybird::TabFolderDrag {

static Optional<TabFolderDestination> visible_position(TabFolderTree const& tree, Vector<SidebarRow> const& rows, size_t index)
{
    auto item = rows[index].item;
    if (rows[index].preview && index > 0 && rows[index - 1].item.is_folder()) {
        // A collapsed folder shows its active descendant even if it is several levels down. Keep the drop in the
        // visible folder, beside the branch the preview stands for, rather than in a hidden subfolder.
        auto visible_parent = rows[index - 1].item.id;
        for (auto parent = tree.parent_of(item); parent.has_value() && *parent != visible_parent; parent = tree.parent_of(item))
            item = Item::folder(*parent);
    }
    return tree.position_of(item);
}

DepthRange depth_range(Vector<SidebarRow> const& rows, size_t gap)
{
    gap = min(gap, rows.size());
    DepthRange range;
    if (gap < rows.size())
        range.min = rows[gap].depth;
    if (gap > 0) {
        auto const& above = rows[gap - 1];
        range.max = above.depth + (above.item.is_folder() ? 1 : 0);
    }
    // The rows are in tree order, so the row below is never more than one deeper than the one above. Should they not be,
    // the row below wins.
    range.max = max(range.max, range.min);
    return range;
}

Optional<TabFolderDestination> destination(TabFolderTree const& tree, Vector<SidebarRow> const& rows, size_t gap, size_t depth)
{
    gap = min(gap, rows.size());
    auto range = depth_range(rows, gap);
    if (depth < range.min || depth > range.max)
        return {};

    if (rows.is_empty())
        return TabFolderDestination::at_end_of({});

    if (gap < rows.size() && rows[gap].depth == depth)
        return visible_position(tree, rows, gap);

    if (gap > 0) {
        auto const& above = rows[gap - 1];
        if (above.item.is_folder() && depth == above.depth + 1) {
            auto folder = tree.folder(above.item.id);
            if (!folder.has_value())
                return {};
            // An open folder whose header is right above a shallower row is empty, so its start is its end too.
            return TabFolderDestination::at_end_of(above.item.id);
        }
    }

    for (size_t i = gap; i > 0; --i) {
        auto const& row = rows[i - 1];
        if (row.depth != depth)
            continue;
        auto position = visible_position(tree, rows, i - 1);
        if (!position.has_value())
            return {};
        return TabFolderDestination { position->parent, position->index + 1 };
    }
    return {};
}

Optional<Target> target_for(TabFolderTree const& tree, Vector<SidebarRow> const& rows, Vector<Item> const& moving, double gap, double depth)
{
    auto rounded_gap = AK::floor(gap + 0.5);
    size_t chosen_gap = rounded_gap <= 0 ? 0 : min(static_cast<size_t>(rounded_gap), rows.size());
    auto range = depth_range(rows, chosen_gap);

    auto rounded_depth = AK::floor(depth + 0.5);
    size_t wanted = rounded_depth <= 0 ? 0 : static_cast<size_t>(rounded_depth);
    wanted = clamp(wanted, range.min, range.max);

    auto try_depth = [&](size_t candidate) -> Optional<Target> {
        if (candidate < range.min || candidate > range.max)
            return {};
        auto place = destination(tree, rows, chosen_gap, candidate);
        if (!place.has_value() || !tree.can_move_items(moving, *place))
            return {};
        return Target { chosen_gap, candidate, *place };
    };

    // The nearest depth first, then ever further away, shallower before deeper.
    for (size_t distance = 0; distance <= range.max - range.min; ++distance) {
        if (distance <= wanted) {
            if (auto target = try_depth(wanted - distance); target.has_value())
                return target;
        }
        if (distance > 0) {
            if (auto target = try_depth(wanted + distance); target.has_value())
                return target;
        }
    }
    return {};
}

Optional<Target> target_for_folder(TabFolderTree const& tree, SidebarRow const& row, Vector<Item> const& moving, size_t preview_gap)
{
    if (!row.item.is_folder())
        return {};
    auto place = TabFolderDestination::at_end_of(row.item.id);
    if (!tree.can_move_items(moving, place))
        return {};
    return Target { preview_gap, row.depth + 1, place };
}

Vector<Item> items_to_drag(Item pressed, Vector<Item> const& selection)
{
    if (selection.contains_slow(pressed))
        return selection;
    return { pressed };
}

}
