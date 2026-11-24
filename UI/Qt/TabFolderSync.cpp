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

Vector<Move> moves_to_reorder(Vector<TabKey> const& current, Vector<TabKey> const& wanted)
{
    // A tab that already comes after the same tabs as it will at the end never has to move, whatever is around it: the
    // other tabs go around it. Those that stay form the longest run whose places in the wanted order rise.
    auto const none = NumericLimits<size_t>::max();
    auto place_in_wanted = [&](TabKey key) {
        auto index = wanted.find_first_index(key);
        return index.has_value() ? *index : none;
    };
    auto tab_count = current.size();
    Vector<size_t> places;
    for (auto key : current)
        places.append(place_in_wanted(key));

    Vector<size_t> longest_ending_at;
    longest_ending_at.resize(tab_count);
    size_t longest = 0;
    for (size_t i = 0; i < tab_count; ++i) {
        if (places[i] == none)
            continue;
        longest_ending_at[i] = 1;
        for (size_t before = 0; before < i; ++before) {
            if (places[before] < places[i])
                longest_ending_at[i] = max(longest_ending_at[i], longest_ending_at[before] + 1);
        }
        longest = max(longest, longest_ending_at[i]);
    }
    // A tab that's in its place for good never moves again. Those that stay are there from the start.
    HashTable<TabKey> in_place_for_good;
    for (size_t i = tab_count; i > 0 && longest > 0; --i) {
        if (longest_ending_at[i - 1] == longest) {
            in_place_for_good.set(current[i - 1]);
            --longest;
        }
    }

    // Each other tab goes in turn, in the order it ends up in, right behind the tab before it there that's in place for
    // good. Everything that ends up before it is there by then, and nothing later passes it, so it moves this once.
    auto order = current;
    Vector<Move> moves;
    for (auto key : wanted) {
        auto from = order.find_first_index(key);
        if (!from.has_value())
            break;
        if (in_place_for_good.contains(key))
            continue;
        auto place = place_in_wanted(key);
        size_t to = 0;
        for (size_t i = 0; i < order.size(); ++i) {
            if (in_place_for_good.contains(order[i]) && place_in_wanted(order[i]) < place)
                to = i + 1;
        }
        // The tab leaves its place first, as a strip's own move takes it.
        if (*from < to)
            --to;
        if (*from != to) {
            order.remove(*from);
            order.insert(to, key);
            moves.append({ *from, to });
        }
        in_place_for_good.set(key);
    }
    return moves;
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
