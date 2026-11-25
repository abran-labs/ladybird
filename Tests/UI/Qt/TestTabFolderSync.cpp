/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <UI/Qt/TabFolderSync.h>

using namespace Ladybird;

using Item = TabFolderTree::Item;
using Glyph = TabFolderTree::FolderGlyph;
using TabFolderSync::SidebarRow;
using TabFolderSync::WindowTab;

static Item tab(i64 key) { return Item::tab(key); }
static Item folder(TabFolderId id) { return Item::folder(id); }

static Vector<WindowTab> window(std::initializer_list<WindowTab> list)
{
    Vector<WindowTab> result;
    for (auto tab : list)
        result.append(tab);
    return result;
}

static WindowTab unpinned(i64 key) { return { key, false }; }
static WindowTab pinned(i64 key) { return { key, true }; }

static Vector<i64> keys(std::initializer_list<i64> list)
{
    Vector<i64> result;
    for (auto key : list)
        result.append(key);
    return result;
}

static Vector<Item> items(std::initializer_list<Item> list)
{
    Vector<Item> result;
    for (auto item : list)
        result.append(item);
    return result;
}

static Vector<Item> row_items(Vector<SidebarRow> const& rows)
{
    Vector<Item> result;
    for (auto const& row : rows)
        result.append(row.item);
    return result;
}

// Work holds tabs 2 and 3; tabs 1 and 4 are at the root.
static TabFolderTree work_tree(TabFolderId& work)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) }));
    work = tree.create_folder_around("Work"_string, items({ tab(2), tab(3) })).release_value();
    return tree;
}

TEST_CASE(without_folders_the_tree_mirrors_the_window_order)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, window({ pinned(5), unpinned(1), unpinned(2) }));
    EXPECT_EQ(tree.tabs_in_order(), keys({ 5, 1, 2 }));

    TabFolderSync::reconcile(tree, window({ pinned(5), unpinned(2), unpinned(1), unpinned(3) }));
    EXPECT_EQ(tree.tabs_in_order(), keys({ 5, 2, 1, 3 }));
    EXPECT(!tree.has_folders());
}

TEST_CASE(a_new_tab_joins_the_folder_of_the_tab_before_it)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);

    // A tab opened after tab 2 lands inside Work, right after it.
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2), unpinned(9), unpinned(3), unpinned(4) }));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(9), tab(3) }));
    EXPECT_EQ(TabFolderSync::window_order(tree, window({ unpinned(1), unpinned(2), unpinned(9), unpinned(3), unpinned(4) })), keys({ 1, 2, 9, 3, 4 }));
}

TEST_CASE(an_explicit_new_tab_goes_to_the_root_even_when_every_existing_tab_is_in_a_folder)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2) }));
    auto work = tree.create_folder_around("Work"_string, items({ tab(1), tab(2) })).release_value();
    EXPECT(tree.set_folder_collapsed(work, true));

    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3) });
    TabFolderSync::reconcile(tree, tabs, 3);
    EXPECT_EQ(tree.children(), items({ folder(work), tab(3) }));
    EXPECT_EQ(tree.children(work), items({ tab(1), tab(2) }));

    // A later sync has no opening intent, and the active tab still has its own root row, not a folder preview.
    TabFolderSync::reconcile(tree, tabs);
    Vector<SidebarRow> expected_rows { { folder(work), 0, false, Glyph::Closed }, { tab(3), 0, false, Glyph::Open } };
    EXPECT_EQ(TabFolderSync::sidebar_rows(tree, tabs, 3), expected_rows);
}

TEST_CASE(a_new_first_unpinned_tab_goes_before_the_next_unpinned_one_not_into_a_pinned_tabs_folder)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, window({ pinned(1), unpinned(2) }));
    auto pins = tree.create_folder_around("Pins"_string, items({ tab(1) })).release_value();

    TabFolderSync::reconcile(tree, window({ pinned(1), unpinned(7), unpinned(2) }));
    EXPECT_EQ(tree.children(pins), items({ tab(1) }));
    EXPECT_EQ(tree.children(), items({ folder(pins), tab(7), tab(2) }));
}

TEST_CASE(a_closed_tab_leaves_its_folder_and_the_empty_folder_stays)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(4) }));
    EXPECT(tree.children(work).is_empty());
    EXPECT_EQ(tree.children(), items({ tab(1), folder(work), tab(4) }));
}

TEST_CASE(a_tab_the_window_moved_follows_its_new_neighbor)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);

    // The horizontal strip moved tab 4 between tabs 2 and 3: it joins Work.
    auto moved = window({ unpinned(1), unpinned(2), unpinned(4), unpinned(3) });
    TabFolderSync::follow_window_move(tree, moved, 4);
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(4), tab(3) }));
    EXPECT_EQ(TabFolderSync::window_order(tree, moved), keys({ 1, 2, 4, 3 }));

    // Moved to the front, tab 3 goes before tab 1 at the root.
    moved = window({ unpinned(3), unpinned(1), unpinned(2), unpinned(4) });
    TabFolderSync::follow_window_move(tree, moved, 3);
    EXPECT_EQ(tree.children(), items({ tab(3), tab(1), folder(work) }));
}

TEST_CASE(the_window_order_puts_pinned_tabs_first_whatever_their_folders)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2), unpinned(3) }));
    auto work = tree.create_folder_around("Work"_string, items({ tab(2), tab(3) })).release_value();
    EXPECT(tree.move_items(items({ tab(1) }), TabFolderDestination::at_end_of(work)));

    // Tab 3 is pinned: it stays in Work, but leads the window.
    EXPECT_EQ(TabFolderSync::window_order(tree, window({ pinned(3), unpinned(2), unpinned(1) })), keys({ 3, 2, 1 }));
}

TEST_CASE(sidebar_rows_leave_pinned_tabs_to_the_pinned_tiles)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto tabs = window({ pinned(2), unpinned(1), unpinned(3), unpinned(4) });
    auto rows = TabFolderSync::sidebar_rows(tree, tabs, 3);
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(work), tab(3), tab(4) }));
    EXPECT_EQ(rows[2].depth, 1u);
}

TEST_CASE(a_collapsed_folder_previews_only_the_active_unpinned_tab)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(tree.set_folder_collapsed(work, true));
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });

    auto rows = TabFolderSync::sidebar_rows(tree, tabs, 3);
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(work), tab(3), tab(4) }));
    EXPECT(rows[2].preview);
    EXPECT_EQ(rows[1].glyph, Glyph::ClosedWithActiveTab);

    // Switching away hides the preview at once, and the folder stays collapsed.
    rows = TabFolderSync::sidebar_rows(tree, tabs, 1);
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(work), tab(4) }));
    EXPECT_EQ(rows[1].glyph, Glyph::Closed);
    EXPECT(tree.folder(work)->collapsed);

    // A pinned active tab is already on its tile, so it never previews.
    rows = TabFolderSync::sidebar_rows(tree, window({ pinned(3), unpinned(1), unpinned(2), unpinned(4) }), 3);
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(work), tab(4) }));
    EXPECT_EQ(rows[1].glyph, Glyph::Closed);
}

TEST_CASE(nested_collapsed_folders_preview_once_under_the_outermost)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();
    EXPECT(tree.set_folder_collapsed(inner, true));
    EXPECT(tree.set_folder_collapsed(work, true));

    auto rows = TabFolderSync::sidebar_rows(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) }), 3);
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(work), tab(3), tab(4) }));
    EXPECT_EQ(rows[2].depth, 1u);
}

TEST_CASE(a_shift_range_covers_the_visible_tab_rows_between_in_either_direction)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto rows = TabFolderSync::sidebar_rows(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) }), 1);
    EXPECT_EQ(TabFolderSync::tab_range(rows, 1, 3), keys({ 1, 2, 3 }));
    EXPECT_EQ(TabFolderSync::tab_range(rows, 4, 2), keys({ 2, 3, 4 }));

    // Hidden tabs inside a collapsed folder aren't part of a range.
    EXPECT(tree.set_folder_collapsed(work, true));
    rows = TabFolderSync::sidebar_rows(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) }), 1);
    EXPECT_EQ(TabFolderSync::tab_range(rows, 1, 4), keys({ 1, 4 }));
    EXPECT(TabFolderSync::tab_range(rows, 1, 3).is_empty());
}

TEST_CASE(rows_left_while_dragging_drop_the_moving_items_and_their_contents)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto rows = TabFolderSync::sidebar_rows(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) }), 1);
    EXPECT_EQ(row_items(TabFolderSync::rows_without(tree, rows, items({ folder(work) }))), items({ tab(1), tab(4) }));
    EXPECT_EQ(row_items(TabFolderSync::rows_without(tree, rows, items({ tab(1), tab(3) }))), items({ folder(work), tab(2), tab(4) }));
}

TEST_CASE(a_gap_lands_before_the_row_below_it_at_that_rows_depth)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });
    auto moving = items({ tab(1) });
    auto rows = TabFolderSync::rows_without(tree, TabFolderSync::sidebar_rows(tree, tabs, 1), moving);
    // Rows: Work, 2, 3, 4.

    // Between Work's header and its first tab: inside Work, at its start.
    auto destination = TabFolderSync::destination_for_gap(tree, rows, 1).release_value();
    EXPECT_EQ(destination.parent, Optional<TabFolderId> { work });
    EXPECT_EQ(destination.index, 0u);
    EXPECT(tree.move_items(moving, destination));
    EXPECT_EQ(tree.children(work), items({ tab(1), tab(2), tab(3) }));

    // Past the last row: the end of the root.
    destination = TabFolderSync::destination_for_gap(tree, rows, rows.size()).release_value();
    EXPECT(!destination.parent.has_value());
    EXPECT(tree.move_items(items({ tab(2), tab(1) }), destination));
    EXPECT_EQ(tree.children(), items({ folder(work), tab(4), tab(1), tab(2) }));
}

TEST_CASE(a_gap_below_a_collapsed_header_lands_inside_that_folder)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(tree.set_folder_collapsed(work, true));
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });
    auto moving = items({ tab(1) });
    auto rows = TabFolderSync::rows_without(tree, TabFolderSync::sidebar_rows(tree, tabs, 3), moving);
    // Rows: Work, 3 (preview), 4.

    auto destination = TabFolderSync::destination_for_gap(tree, rows, 1).release_value();
    EXPECT_EQ(destination.parent, Optional<TabFolderId> { work });
    EXPECT_EQ(destination.index, 0u);
}

// Carries out a plan of moves the way a tab bar's moveTab(from, to) does: each one applies to the order the one before
// it left.
static Vector<i64> after_moves(Vector<i64> order, Vector<TabFolderSync::Move> const& moves)
{
    for (auto const& move : moves) {
        EXPECT(move.from < order.size());
        EXPECT(move.to < order.size());
        if (move.from >= order.size() || move.to >= order.size())
            continue;
        auto key = order[move.from];
        order.remove(move.from);
        order.insert(move.to, key);
    }
    return order;
}

TEST_CASE(reordering_the_strip_takes_the_wanted_order_with_one_move_at_a_time)
{
    // Tabs 2 and 3 moved into a folder that now sits before tab 1, and tab 4 stayed after it.
    auto moves = TabFolderSync::moves_to_reorder(keys({ 1, 2, 3, 4 }), keys({ 2, 3, 1, 4 }));
    EXPECT_EQ(after_moves(keys({ 1, 2, 3, 4 }), moves), keys({ 2, 3, 1, 4 }));

    // Tabs cross in pairs: 1 and 3 each step over the tab after them.
    moves = TabFolderSync::moves_to_reorder(keys({ 1, 2, 3, 4 }), keys({ 2, 4, 1, 3 }));
    EXPECT_EQ(after_moves(keys({ 1, 2, 3, 4 }), moves), keys({ 2, 4, 1, 3 }));
    EXPECT_EQ(moves.size(), 2u);
}

static Vector<TabFolderSync::Move> moves_list(std::initializer_list<TabFolderSync::Move> list)
{
    Vector<TabFolderSync::Move> result;
    for (auto move : list)
        result.append(move);
    return result;
}

TEST_CASE(reordering_the_strip_leaves_every_tab_that_is_already_in_order_alone)
{
    // A folder at the top goes to the bottom: only its tab moves, to the end, however many tabs follow it.
    EXPECT_EQ(TabFolderSync::moves_to_reorder(keys({ 1, 2, 3, 4, 5 }), keys({ 2, 3, 4, 5, 1 })), moves_list({ { 0, 4 } }));

    // The same folder comes back from the bottom, and again only its tab moves.
    EXPECT_EQ(TabFolderSync::moves_to_reorder(keys({ 2, 3, 4, 5, 1 }), keys({ 1, 2, 3, 4, 5 })), moves_list({ { 4, 0 } }));

    // The tab in the middle goes to the end; the others never move.
    EXPECT_EQ(TabFolderSync::moves_to_reorder(keys({ 1, 2, 3, 4 }), keys({ 1, 3, 4, 2 })), moves_list({ { 1, 3 } }));
}

TEST_CASE(a_noncontiguous_selection_keeps_its_order_wherever_it_lands)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    // The selection is listed out of order; the tree's order wins.
    EXPECT(tree.move_items(items({ tab(4), tab(1) }), TabFolderDestination::at_end_of(work)));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(3), tab(1), tab(4) }));
    EXPECT_EQ(TabFolderSync::window_order(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) })), keys({ 2, 3, 1, 4 }));
}

using TabFolderSync::StripGroup;

static Vector<Optional<TabFolderId>> groups(std::initializer_list<Optional<TabFolderId>> list)
{
    Vector<Optional<TabFolderId>> result;
    for (auto group : list)
        result.append(group);
    return result;
}

static Vector<bool> flags(std::initializer_list<bool> list)
{
    Vector<bool> result;
    for (auto flag : list)
        result.append(flag);
    return result;
}

TEST_CASE(without_folders_the_horizontal_strip_has_no_groups_and_hides_nothing)
{
    TabFolderTree tree;
    auto tabs = window({ pinned(5), unpinned(1), unpinned(2) });
    TabFolderSync::reconcile(tree, tabs);
    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    EXPECT(strip.chips.is_empty());
    EXPECT_EQ(strip.group_of_tab, groups({ {}, {}, {} }));
    EXPECT_EQ(strip.hidden, flags({ false, false, false }));
}

TEST_CASE(a_top_level_folder_is_a_chip_before_its_first_tab)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });
    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    Vector<StripGroup> expected { { work, 2, false } };
    EXPECT_EQ(strip.chips, expected);
    EXPECT_EQ(strip.group_of_tab, groups({ {}, work, work, {} }));
    EXPECT_EQ(strip.hidden, flags({ false, false, false, false }));
}

TEST_CASE(expanded_parent_shows_nested_chip_and_its_collapse_only_hides_its_own_members)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();
    EXPECT(tree.set_folder_collapsed(inner, true));
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });
    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    Vector<StripGroup> expected { { work, 2, false }, { inner, 3, true, 1 } };
    EXPECT_EQ(strip.chips, expected);
    EXPECT_EQ(strip.group_of_tab, groups({ {}, work, inner, {} }));
    EXPECT_EQ(strip.hidden, flags({ false, false, true, false }));
    // The tree itself is untouched: the nested folder is still there, still collapsed.
    EXPECT_EQ(tree.parent_of(folder(inner)), Optional<TabFolderId> { work });
    EXPECT(tree.folder(inner)->collapsed);
}

TEST_CASE(an_empty_nested_chip_has_an_anchor_until_its_parent_collapses)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto empty = tree.create_folder("Empty"_string, TabFolderDestination::at_end_of(work)).release_value();
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });
    auto strip = TabFolderSync::strip_groups(tree, tabs, 3);
    Vector<StripGroup> expanded { { work, 2, false }, { empty, 4, false, 1 } };
    EXPECT_EQ(strip.chips, expanded);
    EXPECT(tree.set_folder_collapsed(work, true));
    strip = TabFolderSync::strip_groups(tree, tabs, 3);
    Vector<StripGroup> collapsed { { work, 2, true } };
    EXPECT_EQ(strip.chips, collapsed);
    EXPECT_EQ(strip.hidden, flags({ false, true, false, false }));
    EXPECT(tree.set_folder_collapsed(work, false));
    EXPECT_EQ(TabFolderSync::strip_groups(tree, tabs, 3).chips, expanded);
}

TEST_CASE(a_collapsed_chip_hides_its_tabs_but_never_the_active_one)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(tree.set_folder_collapsed(work, true));
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });

    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    Vector<StripGroup> expected { { work, 2, true } };
    EXPECT_EQ(strip.chips, expected);
    EXPECT_EQ(strip.hidden, flags({ false, true, true, false }));

    strip = TabFolderSync::strip_groups(tree, tabs, 3);
    EXPECT_EQ(strip.hidden, flags({ false, true, false, false }));
}

TEST_CASE(pinned_tabs_show_no_group_and_an_empty_folder_keeps_its_place)
{
    TabFolderTree tree;
    auto tabs = window({ pinned(9), unpinned(1), unpinned(2) });
    TabFolderSync::reconcile(tree, tabs);
    auto pins = tree.create_folder_around("Pins"_string, items({ tab(9) })).release_value();
    auto empty = tree.create_folder("Empty"_string, TabFolderDestination { {}, 2 }).release_value();
    // Root: Pins(9), 1, Empty, 2.
    EXPECT_EQ(tree.children(), items({ folder(pins), tab(1), folder(empty), tab(2) }));
    EXPECT(tree.set_folder_collapsed(pins, true));

    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    Vector<StripGroup> expected { { pins, 1, true }, { empty, 2, false } };
    EXPECT_EQ(strip.chips, expected);
    EXPECT_EQ(strip.group_of_tab, groups({ {}, {}, {} }));
    EXPECT_EQ(strip.hidden, flags({ false, false, false }));
}

TEST_CASE(a_folder_after_every_tab_sits_at_the_end_of_the_strip)
{
    TabFolderTree tree;
    auto tabs = window({ unpinned(1), unpinned(2) });
    TabFolderSync::reconcile(tree, tabs);
    auto last = tree.create_folder_around("Last"_string, items({ tab(2) })).release_value();
    auto trailing = tree.create_folder("Trailing"_string).release_value();
    EXPECT(tree.set_folder_collapsed(last, true));

    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    Vector<StripGroup> expected { { last, 2, true }, { trailing, {}, false } };
    EXPECT_EQ(strip.chips, expected);
    EXPECT_EQ(strip.hidden, flags({ false, true }));
}

TEST_CASE(a_tab_dropped_beside_a_collapsed_group_on_the_strip_stays_outside_it)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(tree.set_folder_collapsed(work, true));

    // Tab 1 is dropped right after the collapsed Work chip, whose tabs 2 and 3 are hidden: it lands after the folder.
    auto moved = window({ unpinned(2), unpinned(3), unpinned(1), unpinned(4) });
    TabFolderSync::follow_strip_move(tree, moved, 1);
    EXPECT_EQ(tree.children(), items({ folder(work), tab(1), tab(4) }));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(3) }));

    // Tab 4 dropped in front of the collapsed group lands before the folder, not inside it.
    moved = window({ unpinned(4), unpinned(2), unpinned(3), unpinned(1) });
    TabFolderSync::follow_strip_move(tree, moved, 4);
    EXPECT_EQ(tree.children(), items({ tab(4), folder(work), tab(1) }));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(3) }));
}

TEST_CASE(a_tab_moved_within_or_into_an_open_group_on_the_strip_follows_its_neighbor)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();

    // Into the open group, next to a tab of a nested folder: it joins that nested folder, as the sidebar would show it.
    auto moved = window({ unpinned(2), unpinned(3), unpinned(1), unpinned(4) });
    TabFolderSync::follow_strip_move(tree, moved, 1);
    EXPECT_EQ(tree.children(inner), items({ tab(3), tab(1) }));

    // A tab of a collapsed group moving among its own hidden siblings stays in it.
    EXPECT(tree.set_folder_collapsed(work, true));
    moved = window({ unpinned(3), unpinned(2), unpinned(1), unpinned(4) });
    TabFolderSync::follow_strip_move(tree, moved, 2);
    EXPECT(tree.is_inside(tab(2), work));
}

TEST_CASE(a_strip_pass_over_a_nested_tree_leaves_its_folders_and_their_look_alone)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();
    EXPECT(tree.rename_folder(inner, "Notes"_string));
    EXPECT(tree.set_folder_icon(inner, "star"_string));
    EXPECT(tree.set_folder_color(inner, "green"_string));

    // The strip lays the group out flat, and every refresh hands the window's flat order back to the tree.
    auto tabs = window({ unpinned(1), unpinned(2), unpinned(3), unpinned(4) });
    TabFolderSync::reconcile(tree, tabs);
    TabFolderSync::reconcile(tree, tabs);

    EXPECT_EQ(tree.parent_of(folder(inner)), Optional<TabFolderId> { work });
    EXPECT_EQ(tree.tabs_inside(work), keys({ 2, 3 }));
    EXPECT_EQ(tree.folder(inner)->name, "Notes"_string);
    EXPECT_EQ(tree.folder(inner)->icon.value_or(""_string), "star"_string);
    EXPECT_EQ(tree.folder(inner)->color.value_or(""_string), "green"_string);

    auto strip = TabFolderSync::strip_groups(tree, tabs, 1);
    Vector<StripGroup> expected { { work, 2, false }, { inner, 3, false, 1 } };
    EXPECT_EQ(strip.chips, expected);
    EXPECT_EQ(strip.group_of_tab, groups({ {}, work, inner, {} }));
}

TEST_CASE(a_tab_added_to_a_group_joins_the_group_rather_than_the_subgroup_holding_the_tab_before_it)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();

    // A tab opened after 3 follows its neighbor into Inner.
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2), unpinned(3), unpinned(9), unpinned(4) }));
    EXPECT_EQ(tree.children(inner), items({ tab(3), tab(9) }));

    // New Tab in Group puts it in the group itself, where the chip it came from is.
    EXPECT(tree.move_items(items({ tab(9) }), TabFolderDestination::at_end_of(work)));
    EXPECT_EQ(tree.children(work), items({ tab(2), folder(inner), tab(9) }));
    EXPECT_EQ(tree.parent_of(tab(9)), Optional<TabFolderId> { work });
}

TEST_CASE(a_tab_added_to_an_empty_group_ends_up_inside_it)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2) }));
    auto empty = tree.create_folder("Empty"_string).release_value();

    // A tab opened at the end of the strip lands beside tab 2, at the root; the group then takes it in.
    TabFolderSync::reconcile(tree, window({ unpinned(1), unpinned(2), unpinned(9) }));
    EXPECT_EQ(tree.parent_of(tab(9)), Optional<TabFolderId> {});
    EXPECT(tree.move_items(items({ tab(9) }), TabFolderDestination::at_end_of(empty)));
    EXPECT_EQ(tree.children(empty), items({ tab(9) }));
    EXPECT_EQ(tree.children(), items({ tab(1), tab(2), folder(empty) }));
}
