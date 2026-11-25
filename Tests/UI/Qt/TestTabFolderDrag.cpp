/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <UI/Qt/HorizontalDrop.h>
#include <UI/Qt/TabFolderDrag.h>
#include <UI/Qt/TabFolderSync.h>

using namespace Ladybird;

using Item = TabFolderTree::Item;
using TabFolderSync::SidebarRow;
using TabFolderSync::WindowTab;

static Item tab(i64 key) { return Item::tab(key); }
static Item folder(TabFolderId id) { return Item::folder(id); }

static Vector<Item> items(std::initializer_list<Item> list)
{
    Vector<Item> result;
    for (auto item : list)
        result.append(item);
    return result;
}

static Vector<WindowTab> unpinned_tabs(std::initializer_list<i64> keys)
{
    Vector<WindowTab> result;
    for (auto key : keys)
        result.append({ key, false });
    return result;
}

static Vector<Item> row_items(Vector<SidebarRow> const& rows)
{
    Vector<Item> result;
    for (auto const& row : rows)
        result.append(row.item);
    return result;
}

// Rows: 1, Work, 2, 3, 4, where Work holds tabs 2 and 3.
static TabFolderTree work_tree(TabFolderId& work)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, unpinned_tabs({ 1, 2, 3, 4 }));
    work = tree.create_folder_around("Work"_string, items({ tab(2), tab(3) })).release_value();
    return tree;
}

static Vector<SidebarRow> rows_while_dragging(TabFolderTree const& tree, Vector<Item> const& moving, Optional<i64> active = {})
{
    auto rows = TabFolderSync::sidebar_rows(tree, unpinned_tabs({ 1, 2, 3, 4, 5, 6 }), active);
    return TabFolderSync::rows_without(tree, rows, moving);
}

static bool drop(TabFolderTree& tree, Vector<Item> const& moving, size_t gap, size_t depth)
{
    auto rows = rows_while_dragging(tree, moving);
    auto destination = TabFolderDrag::destination(tree, rows, gap, depth);
    if (!destination.has_value())
        return false;
    return tree.move_items(moving, *destination);
}

TEST_CASE(visual_gaps_skip_dragged_subtrees_and_nonadjacent_selected_rows)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto tabs = unpinned_tabs({ 1, 2, 3, 4, 5 });
    TabFolderSync::reconcile(tree, tabs);
    auto rows = TabFolderSync::sidebar_rows(tree, tabs, {});
    auto moving = items({ folder(work), tab(5) });
    auto remaining = TabFolderSync::rows_without(tree, rows, moving);
    EXPECT_EQ(row_items(remaining), items({ tab(1), tab(4) }));
    EXPECT_EQ(TabFolderDrag::gaps_for_visible_slots(rows, remaining), (Vector<size_t> { 0, 1, 1, 1, 1, 2, 2 }));
}

TEST_CASE(a_gap_offers_the_depths_between_the_row_below_and_one_inside_the_row_above)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto rows = rows_while_dragging(tree, items({ tab(4) }));
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(work), tab(2), tab(3) }));

    auto range = TabFolderDrag::depth_range(rows, 0);
    EXPECT_EQ(range.min, 0u);
    EXPECT_EQ(range.max, 0u);

    // Between a tab and a folder at the root: only the root.
    range = TabFolderDrag::depth_range(rows, 1);
    EXPECT_EQ(range.min, 0u);
    EXPECT_EQ(range.max, 0u);

    // Right below an open folder's header, above its first tab: only inside it.
    range = TabFolderDrag::depth_range(rows, 2);
    EXPECT_EQ(range.min, 1u);
    EXPECT_EQ(range.max, 1u);

    // Below a folder's last tab, at the end: inside the folder, or unindented out of it.
    range = TabFolderDrag::depth_range(rows, 4);
    EXPECT_EQ(range.min, 0u);
    EXPECT_EQ(range.max, 1u);
}

TEST_CASE(a_tab_moves_out_of_its_folder_by_unindenting)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    // Without tab 3, the rows are 1, Work, 2, 4. The gap between 2 and 4, at the root, is right after Work.
    EXPECT(drop(tree, items({ tab(3) }), 3, 0));
    EXPECT_EQ(tree.children(), items({ tab(1), folder(work), tab(3), tab(4) }));
    EXPECT_EQ(tree.children(work), items({ tab(2) }));

    // The same gap, indented, puts it back at the end of Work.
    EXPECT(drop(tree, items({ tab(3) }), 3, 1));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(3) }));
    EXPECT_EQ(tree.children(), items({ tab(1), folder(work), tab(4) }));
}

TEST_CASE(a_tab_moves_above_and_below_a_whole_folder)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);

    // Tab 4, between tab 1 and Work's header: above the folder.
    EXPECT(drop(tree, items({ tab(4) }), 1, 0));
    EXPECT_EQ(tree.children(), items({ tab(1), tab(4), folder(work) }));

    // Tab 1, below Work's last tab at the root: below the whole folder.
    // Rows without it: 4, Work, 2, 3.
    EXPECT(drop(tree, items({ tab(1) }), 4, 0));
    EXPECT_EQ(tree.children(), items({ tab(4), folder(work), tab(1) }));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(3) }));
}

TEST_CASE(a_gap_right_below_an_open_header_lands_at_the_start_of_the_folder)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(drop(tree, items({ tab(4) }), 2, 1));
    EXPECT_EQ(tree.children(work), items({ tab(4), tab(2), tab(3) }));
}

TEST_CASE(a_collapsed_folder_takes_a_tab_at_its_end_when_indented_below_its_header)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(tree.set_folder_collapsed(work, true));
    auto rows = rows_while_dragging(tree, items({ tab(1) }));
    // Rows: Work (collapsed), 4.
    EXPECT_EQ(row_items(rows), items({ folder(work), tab(4) }));
    auto range = TabFolderDrag::depth_range(rows, 1);
    EXPECT_EQ(range.min, 0u);
    EXPECT_EQ(range.max, 1u);

    EXPECT(drop(tree, items({ tab(1) }), 1, 1));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(3), tab(1) }));

    // At the root instead, it goes right after the collapsed folder.
    EXPECT(drop(tree, items({ tab(1) }), 1, 0));
    EXPECT_EQ(tree.children(), items({ folder(work), tab(1), tab(4) }));
}

TEST_CASE(a_gap_around_a_collapsed_folders_preview_stays_beside_that_tab_in_the_folder)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    EXPECT(tree.set_folder_collapsed(work, true));
    auto moving = items({ tab(1) });
    auto rows = rows_while_dragging(tree, moving, 3);
    // Rows: Work, 3 (preview), 4.
    EXPECT_EQ(row_items(rows), items({ folder(work), tab(3), tab(4) }));

    // Between the header and the preview: before the previewed tab.
    auto range = TabFolderDrag::depth_range(rows, 1);
    EXPECT_EQ(range.min, 1u);
    EXPECT_EQ(range.max, 1u);
    auto destination = TabFolderDrag::destination(tree, rows, 1, 1).release_value();
    EXPECT_EQ(destination.parent, Optional<TabFolderId> { work });
    EXPECT_EQ(destination.index, 1u);

    // Below the preview, indented: right after it.
    destination = TabFolderDrag::destination(tree, rows, 2, 1).release_value();
    EXPECT_EQ(destination.parent, Optional<TabFolderId> { work });
    EXPECT_EQ(destination.index, 2u);
}

TEST_CASE(a_folder_drags_with_its_contents_and_nests_by_indenting)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, unpinned_tabs({ 1, 2, 3, 4 }));
    auto work = tree.create_folder_around("Work"_string, items({ tab(1), tab(2) })).release_value();
    auto play = tree.create_folder_around("Play"_string, items({ tab(3) })).release_value();
    // Rows: Work, 1, 2, Play, 3, 4.

    auto moving = items({ folder(play) });
    auto rows = rows_while_dragging(tree, moving);
    EXPECT_EQ(row_items(rows), items({ folder(work), tab(1), tab(2), tab(4) }));

    // Above Work.
    EXPECT(drop(tree, moving, 0, 0));
    EXPECT_EQ(tree.children(), items({ folder(play), folder(work), tab(4) }));
    EXPECT_EQ(tree.children(play), items({ tab(3) }));

    // Indented below Work's last tab: nested in Work, with its tab.
    // Rows without Play: Work, 1, 2, 4.
    EXPECT(drop(tree, moving, 3, 1));
    EXPECT_EQ(tree.children(work), items({ tab(1), tab(2), folder(play) }));
    EXPECT_EQ(tree.children(play), items({ tab(3) }));
    EXPECT_EQ(tree.tabs_in_order(), (Vector<i64> { 1, 2, 3, 4 }));
}

TEST_CASE(the_target_follows_the_card_center_and_the_nearest_allowed_depth)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto moving = items({ tab(4) });
    auto rows = rows_while_dragging(tree, moving);
    // Rows: 1, Work, 2, 3; slots 0 to 4.

    // The card's center in slot 1, asking for a deep indent, only gets the root there.
    auto target = TabFolderDrag::target_for(tree, rows, moving, 1.4, 3.0).release_value();
    EXPECT_EQ(target.gap, 1u);
    EXPECT_EQ(target.depth, 0u);
    EXPECT(!target.destination.parent.has_value());
    EXPECT_EQ(target.destination.index, 1u);

    // Past the end, the slot clamps to the last one, and the depth rounds to the nearest allowed.
    target = TabFolderDrag::target_for(tree, rows, moving, 9.0, 0.6).release_value();
    EXPECT_EQ(target.gap, 4u);
    EXPECT_EQ(target.depth, 1u);
    EXPECT_EQ(target.destination.parent, Optional<TabFolderId> { work });

    target = TabFolderDrag::target_for(tree, rows, moving, -3.0, 0.0).release_value();
    EXPECT_EQ(target.gap, 0u);
}

TEST_CASE(a_folder_too_deep_to_nest_falls_back_to_a_shallower_depth)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, unpinned_tabs({ 1, 2 }));
    // A chain of folders as deep as allowed holds tab 1; Other holds tab 2 at the root.
    Optional<TabFolderId> innermost;
    for (size_t depth = 0; depth < TabFolderTree::MAX_FOLDER_DEPTH; ++depth)
        innermost = tree.create_folder("Deep"_string, TabFolderDestination::at_end_of(innermost)).release_value();
    EXPECT(tree.move_items(items({ tab(1) }), TabFolderDestination::at_end_of(innermost)));
    auto other = tree.create_folder_around("Other"_string, items({ tab(2) })).release_value();

    auto moving = items({ folder(other) });
    auto rows = rows_while_dragging(tree, moving);
    // Below tab 1, the deepest depth would put Other inside the innermost folder, past the limit.
    auto range = TabFolderDrag::depth_range(rows, rows.size());
    EXPECT_EQ(range.max, TabFolderTree::MAX_FOLDER_DEPTH);
    auto deepest = TabFolderDrag::destination(tree, rows, rows.size(), range.max).release_value();
    EXPECT(!tree.can_move_items(moving, deepest));
    auto target = TabFolderDrag::target_for(tree, rows, moving, static_cast<double>(rows.size()), static_cast<double>(range.max)).release_value();
    EXPECT_EQ(target.depth, range.max - 1);
}

TEST_CASE(a_direct_folder_target_preserves_the_visual_gap_and_never_falls_back_to_a_sibling)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto empty = tree.create_folder("Empty"_string).release_value();
    EXPECT(tree.set_folder_collapsed(empty, true));
    auto moving = items({ folder(work) });
    auto rows = rows_while_dragging(tree, moving);
    auto target = TabFolderDrag::target_for_folder(tree, rows.last(), moving, 1).release_value();
    EXPECT_EQ(target.gap, 1u);
    EXPECT_EQ(target.depth, 1u);
    EXPECT_EQ(target.destination.parent, Optional<TabFolderId> { empty });
    EXPECT(tree.move_items(moving, target.destination));
    EXPECT(tree.folder(empty)->collapsed);
    EXPECT(!TabFolderDrag::target_for_folder(tree, { folder(work), 1 }, items({ folder(empty) }), 0).has_value());
}

TEST_CASE(a_press_on_a_selected_tab_drags_the_whole_selection)
{
    auto selected = items({ tab(1), tab(3), tab(4) });
    EXPECT_EQ(TabFolderDrag::items_to_drag(tab(3), selected), selected);
    // A tab outside the selection drags alone, and so does a folder.
    EXPECT_EQ(TabFolderDrag::items_to_drag(tab(2), selected), items({ tab(2) }));
    EXPECT_EQ(TabFolderDrag::items_to_drag(folder(7), selected), items({ folder(7) }));
    EXPECT_EQ(TabFolderDrag::items_to_drag(tab(1), items({ tab(1) })), items({ tab(1) }));
}

TEST_CASE(a_nested_active_preview_does_not_target_its_hidden_parent)
{
    TabFolderTree tree;
    TabFolderSync::reconcile(tree, unpinned_tabs({ 1, 2, 3, 4 }));
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2), tab(3) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(2) })).release_value();
    EXPECT(tree.set_folder_collapsed(outer, true));

    auto moving = items({ tab(4) });
    auto rows = rows_while_dragging(tree, moving, 2);
    EXPECT_EQ(row_items(rows), items({ tab(1), folder(outer), tab(2) }));
    auto before = TabFolderDrag::destination(tree, rows, 2, 1).release_value();
    auto after = TabFolderDrag::destination(tree, rows, 3, 1).release_value();
    EXPECT_EQ(before.parent, Optional<TabFolderId> { outer });
    EXPECT_EQ(before.index, 0u);
    EXPECT_EQ(after.parent, Optional<TabFolderId> { outer });
    EXPECT_EQ(after.index, 1u);
    EXPECT(tree.move_items(moving, after));
    EXPECT_EQ(tree.children(outer), items({ folder(inner), tab(4), tab(3) }));
    EXPECT_EQ(tree.children(inner), items({ tab(2) }));
}

TEST_CASE(dragging_every_visible_row_leaves_a_root_target)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto moving = items({ tab(1), folder(work), tab(4) });
    auto rows = rows_while_dragging(tree, moving);
    EXPECT(rows.is_empty());
    auto target = TabFolderDrag::target_for(tree, rows, moving, 0.0, 0.0);
    EXPECT(target.has_value());
    if (!target.has_value())
        return;
    EXPECT(!target->destination.parent.has_value());
    EXPECT(tree.move_items(moving, target->destination));
    EXPECT_EQ(tree.children(), moving);
}

TEST_CASE(a_discontiguous_selection_drops_in_tree_order_into_a_folder)
{
    TabFolderId work = 0;
    auto tree = work_tree(work);
    auto moving = TabFolderDrag::items_to_drag(tab(4), items({ tab(4), tab(1), tab(3) }));
    auto rows = rows_while_dragging(tree, moving);
    EXPECT_EQ(row_items(rows), items({ folder(work), tab(2) }));
    auto target = TabFolderDrag::target_for(tree, rows, moving, 2.0, 1.0).release_value();
    EXPECT(tree.move_items(moving, target.destination));
    EXPECT_EQ(tree.children(), items({ folder(work) }));
    EXPECT_EQ(tree.children(work), items({ tab(2), tab(1), tab(3), tab(4) }));
}

TEST_CASE(horizontal_drops_use_the_chip_not_the_hidden_subfolder_as_the_destination)
{
    TabFolderId work;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, { tab(3) }).value();
    auto place = HorizontalDrop::beside_member(tree, tab(3), work, { tab(4) }, true);
    EXPECT(place.has_value());
    EXPECT(tree.move_items({ tab(4) }, *place));
    EXPECT_EQ(tree.children(work), items({ tab(2), folder(inner), tab(4) }));
    EXPECT_EQ(tree.children(inner), items({ tab(3) }));
}

TEST_CASE(horizontal_root_gap_moves_a_whole_closed_group_then_ungroups_a_member)
{
    TabFolderId work;
    auto tree = work_tree(work);
    auto inner = tree.create_folder_around("Inner"_string, { tab(3) }).value();
    EXPECT(tree.set_folder_collapsed(work, true));
    auto end = HorizontalDrop::beside_root(tree, tab(4), true);
    EXPECT(end.has_value());
    EXPECT(tree.move_items({ folder(work) }, *end));
    EXPECT_EQ(tree.children(), items({ tab(1), tab(4), folder(work) }));
    EXPECT_EQ(tree.children(work), items({ tab(2), folder(inner) }));
    EXPECT_EQ(tree.children(inner), items({ tab(3) }));

    auto gap = HorizontalDrop::beside_root(tree, folder(work), false);
    EXPECT(gap.has_value());
    EXPECT(tree.move_items({ tab(3) }, *gap));
    EXPECT_EQ(tree.children(), items({ tab(1), tab(4), tab(3), folder(work) }));
    EXPECT(tree.folder(work)->collapsed);
}
