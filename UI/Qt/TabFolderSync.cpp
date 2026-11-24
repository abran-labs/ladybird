/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashTable.h>
#include <UI/Qt/TabFolderSync.h>

namespace Ladybird::TabFolderSync {

static Optional<size_t> index_in_window(Vector<WindowTab> const& tabs, TabKey key)
{
    return tabs.find_first_index_if([&](auto const& tab) { return tab.key == key; });
}

// The nearest tab of the same kind before the one at the index that the tree already holds, or else the nearest after it.
struct Neighbor {
    TabKey key { 0 };
    bool is_before { true };
};

static Optional<Neighbor> neighbor_in_tree(TabFolderTree const& tree, Vector<WindowTab> const& tabs, size_t index)
{
    auto pinned = tabs[index].pinned;
    for (size_t i = index; i > 0; --i) {
        auto const& candidate = tabs[i - 1];
        if (candidate.pinned == pinned && tree.contains(Item::tab(candidate.key)))
            return Neighbor { candidate.key, true };
    }
    for (size_t i = index + 1; i < tabs.size(); ++i) {
        auto const& candidate = tabs[i];
        if (candidate.pinned == pinned && tree.contains(Item::tab(candidate.key)))
            return Neighbor { candidate.key, false };
    }
    return {};
}

static Optional<TabFolderDestination> destination_beside(TabFolderTree const& tree, Neighbor neighbor)
{
    auto position = tree.position_of(Item::tab(neighbor.key));
    if (!position.has_value())
        return {};
    if (neighbor.is_before)
        ++position->index;
    return position;
}

void reconcile(TabFolderTree& tree, Vector<WindowTab> const& tabs)
{
    // Without folders, nothing but the window's order is worth keeping.
    if (!tree.has_folders()) {
        tree = {};
        for (auto const& tab : tabs)
            (void)tree.add_tab(tab.key);
        return;
    }

    HashTable<TabKey> in_window;
    for (auto const& tab : tabs)
        in_window.set(tab.key);
    for (auto key : tree.tabs_in_order()) {
        if (!in_window.contains(key))
            (void)tree.remove_tab(key);
    }

    for (size_t i = 0; i < tabs.size(); ++i) {
        auto key = tabs[i].key;
        if (tree.contains(Item::tab(key)))
            continue;
        TabFolderDestination destination;
        if (auto neighbor = neighbor_in_tree(tree, tabs, i); neighbor.has_value())
            destination = destination_beside(tree, *neighbor).value_or({});
        else if (tabs[i].pinned)
            destination = { {}, 0 };
        (void)tree.add_tab(key, destination);
    }
}

void follow_window_move(TabFolderTree& tree, Vector<WindowTab> const& tabs, TabKey moved)
{
    auto index = index_in_window(tabs, moved);
    if (!index.has_value() || tabs[*index].pinned || !tree.contains(Item::tab(moved)))
        return;
    auto neighbor = neighbor_in_tree(tree, tabs, *index);
    if (!neighbor.has_value())
        return;
    if (auto destination = destination_beside(tree, *neighbor); destination.has_value())
        (void)tree.move_items({ Item::tab(moved) }, *destination);
}

Vector<TabKey> window_order(TabFolderTree const& tree, Vector<WindowTab> const& tabs)
{
    Vector<TabKey> order;
    HashTable<TabKey> unpinned;
    for (auto const& tab : tabs) {
        if (tab.pinned)
            order.append(tab.key);
        else
            unpinned.set(tab.key);
    }
    for (auto key : tree.tabs_in_order()) {
        if (unpinned.remove(key))
            order.append(key);
    }
    // A tab the tree doesn't know yet keeps its place at the end.
    for (auto const& tab : tabs) {
        if (unpinned.contains(tab.key))
            order.append(tab.key);
    }
    return order;
}

Vector<SidebarRow> sidebar_rows(TabFolderTree const& tree, Vector<WindowTab> const& tabs, Optional<TabKey> active)
{
    HashTable<TabKey> pinned;
    for (auto const& tab : tabs) {
        if (tab.pinned)
            pinned.set(tab.key);
    }
    // A pinned tab shows on its tile, so it has nothing to preview.
    if (active.has_value() && (pinned.contains(*active) || !tree.contains(Item::tab(*active))))
        active.clear();

    Vector<SidebarRow> rows;
    auto walk = [&](auto& self, Vector<Item> const& items, size_t depth) -> void {
        for (auto item : items) {
            if (item.is_tab()) {
                if (!pinned.contains(item.id))
                    rows.append({ item, depth, false, TabFolderTree::FolderGlyph::Open });
                continue;
            }
            auto const& folder = tree.folder(item.id).value();
            if (!folder.collapsed) {
                rows.append({ item, depth, false, TabFolderTree::FolderGlyph::Open });
                self(self, tree.children(item.id), depth + 1);
                continue;
            }
            auto holds_active = active.has_value() && tree.is_inside(Item::tab(*active), item.id);
            rows.append({ item, depth, false, holds_active ? TabFolderTree::FolderGlyph::ClosedWithActiveTab : TabFolderTree::FolderGlyph::Closed });
            if (holds_active)
                rows.append({ Item::tab(*active), depth + 1, true, TabFolderTree::FolderGlyph::Open });
        }
    };
    walk(walk, tree.children(), 0);
    return rows;
}

Vector<TabKey> tab_range(Vector<SidebarRow> const& rows, TabKey anchor, TabKey target)
{
    auto row_of = [&](TabKey key) {
        return rows.find_first_index_if([&](auto const& row) { return row.item == Item::tab(key); });
    };
    auto from = row_of(anchor);
    auto to = row_of(target);
    if (!from.has_value() || !to.has_value())
        return {};
    if (*from > *to)
        swap(from, to);
    Vector<TabKey> range;
    for (auto i = *from; i <= *to; ++i) {
        if (rows[i].item.is_tab())
            range.append(rows[i].item.id);
    }
    return range;
}

Vector<SidebarRow> rows_without(TabFolderTree const& tree, Vector<SidebarRow> const& rows, Vector<Item> const& moving)
{
    Vector<SidebarRow> result;
    for (auto const& row : rows) {
        auto is_moving = moving.contains_slow(row.item) || moving.contains([&](auto item) {
            return item.is_folder() && tree.is_inside(row.item, item.id);
        });
        if (!is_moving)
            result.append(row);
    }
    return result;
}

Optional<TabFolderDestination> destination_for_gap(TabFolderTree const& tree, Vector<SidebarRow> const& rows, size_t gap)
{
    gap = min(gap, rows.size());
    if (gap > 0) {
        auto const& above = rows[gap - 1];
        if (above.item.is_folder()) {
            auto folder = tree.folder(above.item.id);
            auto previews_below = gap < rows.size() && rows[gap].preview;
            if (folder.has_value() && (!folder->collapsed || previews_below))
                return TabFolderDestination { above.item.id, 0 };
        }
    }
    if (gap < rows.size())
        return tree.position_of(rows[gap].item);
    return TabFolderDestination::at_end_of({});
}

}
