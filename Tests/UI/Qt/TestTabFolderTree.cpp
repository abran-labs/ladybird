/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <UI/Qt/TabFolderTree.h>

using namespace Ladybird;

using Item = TabFolderTree::Item;
using Row = TabFolderTree::Row;
using Glyph = TabFolderTree::FolderGlyph;

static Item tab(i64 key) { return Item::tab(key); }
static Item folder(TabFolderId id) { return Item::folder(id); }

static TabFolderTree tree_with_tabs(std::initializer_list<i64> keys)
{
    TabFolderTree tree;
    for (auto key : keys)
        EXPECT(tree.add_tab(key));
    return tree;
}

static Vector<Item> items(std::initializer_list<Item> list)
{
    Vector<Item> result;
    for (auto item : list)
        result.append(item);
    return result;
}

static HashTable<i64> allowed(std::initializer_list<i64> list)
{
    HashTable<i64> result;
    for (auto key : list)
        result.set(key);
    return result;
}

static Vector<i64> keys(std::initializer_list<i64> list)
{
    Vector<i64> result;
    for (auto key : list)
        result.append(key);
    return result;
}

TEST_CASE(tabs_without_folders_sit_at_the_root_in_order)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    EXPECT_EQ(tree.children(), items({ tab(1), tab(2), tab(3) }));
    EXPECT(!tree.has_folders());
    EXPECT(!tree.add_tab(2));
    EXPECT_EQ(tree.tabs_in_order(), keys({ 1, 2, 3 }));
}

TEST_CASE(folders_nest_and_keep_their_order)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    auto outer = tree.create_folder("Outer"_string, { .parent = {}, .index = 1 }).release_value();
    auto inner = tree.create_folder("Inner"_string, TabFolderTree::Destination::at_end_of(outer)).release_value();
    EXPECT(tree.move_items(items({ tab(3) }), TabFolderTree::Destination::at_end_of(inner)));

    EXPECT_EQ(tree.children(), items({ tab(1), folder(outer), tab(2) }));
    EXPECT_EQ(tree.children(outer), items({ folder(inner) }));
    EXPECT_EQ(tree.children(inner), items({ tab(3) }));
    EXPECT_EQ(tree.parent_of(tab(3)), Optional<TabFolderId> { inner });
    EXPECT_EQ(tree.parent_of(folder(inner)), Optional<TabFolderId> { outer });
    EXPECT(tree.is_inside(tab(3), outer));
    EXPECT(!tree.is_inside(folder(outer), outer));
    EXPECT_EQ(tree.tabs_in_order(), keys({ 1, 3, 2 }));
    EXPECT_EQ(tree.tabs_inside(outer), keys({ 3 }));

    auto folders = tree.folders_in_order();
    EXPECT_EQ(folders.size(), 2u);
    EXPECT_EQ(folders[0], (TabFolderTree::FolderEntry { outer, 0 }));
    EXPECT_EQ(folders[1], (TabFolderTree::FolderEntry { inner, 1 }));
}

TEST_CASE(folder_ids_stay_unique_after_deleting_the_newest)
{
    TabFolderTree tree;
    auto a = tree.create_folder("A"_string).release_value();
    auto b = tree.create_folder("B"_string).release_value();
    (void)tree.delete_folder(b);
    auto c = tree.create_folder("C"_string).release_value();
    EXPECT_NE(c, a);
    EXPECT_NE(c, b);
}

TEST_CASE(moving_a_noncontiguous_selection_keeps_its_relative_order)
{
    auto tree = tree_with_tabs({ 1, 2, 3, 4, 5 });
    auto target = tree.create_folder("Target"_string).release_value();

    // Listed out of order; the tree's order wins.
    EXPECT(tree.move_items(items({ tab(4), tab(1), tab(3) }), TabFolderTree::Destination::at_end_of(target)));
    EXPECT_EQ(tree.children(target), items({ tab(1), tab(3), tab(4) }));
    EXPECT_EQ(tree.children(), items({ tab(2), tab(5), folder(target) }));
}

TEST_CASE(moving_within_a_parent_uses_the_index_before_the_move)
{
    auto tree = tree_with_tabs({ 1, 2, 3, 4, 5 });
    // Landing line between 4 and 5, which is index 4 before 1 and 2 leave.
    EXPECT(tree.move_items(items({ tab(1), tab(2) }), { .parent = {}, .index = 4 }));
    EXPECT_EQ(tree.children(), items({ tab(3), tab(4), tab(1), tab(2), tab(5) }));

    // Landing line at the very top.
    EXPECT(tree.move_items(items({ tab(5), tab(2) }), { .parent = {}, .index = 0 }));
    EXPECT_EQ(tree.children(), items({ tab(2), tab(5), tab(3), tab(4), tab(1) }));
}

TEST_CASE(moving_a_folder_and_its_own_child_moves_the_child_along)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    auto a = tree.create_folder_around("A"_string, items({ tab(1), tab(2) })).release_value();
    EXPECT(tree.move_items(items({ tab(2), folder(a) }), TabFolderTree::Destination::at_end_of({})));
    EXPECT_EQ(tree.children(), items({ tab(3), folder(a) }));
    EXPECT_EQ(tree.children(a), items({ tab(1), tab(2) }));
}

TEST_CASE(a_folder_cannot_move_into_itself_or_a_descendant)
{
    auto tree = tree_with_tabs({ 1 });
    auto outer = tree.create_folder("Outer"_string).release_value();
    auto inner = tree.create_folder("Inner"_string, TabFolderTree::Destination::at_end_of(outer)).release_value();
    auto deepest = tree.create_folder("Deepest"_string, TabFolderTree::Destination::at_end_of(inner)).release_value();
    auto before = tree.serialize();

    EXPECT(!tree.can_move_items(items({ folder(outer) }), TabFolderTree::Destination::at_end_of(outer)));
    EXPECT(!tree.move_items(items({ folder(outer) }), TabFolderTree::Destination::at_end_of(outer)));
    EXPECT(!tree.move_items(items({ folder(outer) }), TabFolderTree::Destination::at_end_of(deepest)));
    // A cycle anywhere in a batch rejects the whole batch.
    EXPECT(!tree.move_items(items({ tab(1), folder(inner) }), TabFolderTree::Destination::at_end_of(deepest)));
    EXPECT_EQ(tree.serialize(), before);

    // Moving out to an ancestor is fine.
    EXPECT(tree.can_move_items(items({ folder(deepest) }), TabFolderTree::Destination::at_end_of({})));
}

TEST_CASE(moves_to_a_missing_folder_or_of_missing_items_are_rejected)
{
    auto tree = tree_with_tabs({ 1 });
    EXPECT(!tree.move_items(items({ tab(1) }), TabFolderTree::Destination::at_end_of(99)));
    EXPECT(!tree.move_items(items({ tab(42) }), TabFolderTree::Destination::at_end_of({})));
    EXPECT(!tree.move_items({}, TabFolderTree::Destination::at_end_of({})));
}

TEST_CASE(creating_a_folder_around_a_selection_takes_the_first_items_place)
{
    auto tree = tree_with_tabs({ 1, 2, 3, 4 });
    auto a = tree.create_folder_around("A"_string, items({ tab(4), tab(2) })).release_value();
    EXPECT_EQ(tree.children(), items({ tab(1), folder(a), tab(3) }));
    EXPECT_EQ(tree.children(a), items({ tab(2), tab(4) }));
    EXPECT_EQ(tree.folder(a)->name, "A"_string);
}

TEST_CASE(unpacking_promotes_direct_children_in_place)
{
    auto tree = tree_with_tabs({ 1, 2, 3, 4 });
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2), tab(3) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();
    EXPECT_EQ(tree.children(outer), items({ tab(2), folder(inner) }));

    EXPECT(tree.unpack_folder(outer));
    EXPECT_EQ(tree.children(), items({ tab(1), tab(2), folder(inner), tab(4) }));
    EXPECT_EQ(tree.children(inner), items({ tab(3) }));
    EXPECT(!tree.folder(outer).has_value());
    EXPECT_EQ(tree.parent_of(tab(2)), Optional<TabFolderId> {});
    EXPECT_EQ(tree.parent_of(folder(inner)), Optional<TabFolderId> {});
    EXPECT_EQ(tree.tabs_in_order(), keys({ 1, 2, 3, 4 }));
}

TEST_CASE(deleting_a_folder_returns_every_tab_in_its_tree_and_removes_descendants)
{
    auto tree = tree_with_tabs({ 1, 2, 3, 4 });
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2), tab(3) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();

    EXPECT_EQ(tree.delete_folder(outer), keys({ 2, 3 }));
    EXPECT_EQ(tree.children(), items({ tab(1), tab(4) }));
    EXPECT(!tree.folder(outer).has_value());
    EXPECT(!tree.folder(inner).has_value());
    EXPECT(!tree.contains(tab(2)));
    EXPECT(!tree.contains(tab(3)));
}

TEST_CASE(removing_a_tab_leaves_its_folder)
{
    auto tree = tree_with_tabs({ 1, 2 });
    auto a = tree.create_folder_around("A"_string, items({ tab(1) })).release_value();
    EXPECT(tree.remove_tab(1));
    EXPECT(!tree.remove_tab(1));
    EXPECT(tree.folder(a).has_value());
    EXPECT(tree.children(a).is_empty());
}

TEST_CASE(reconciling_places_new_tabs_after_their_predecessor_and_drops_gone_ones)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    auto a = tree.create_folder_around("A"_string, items({ tab(2) })).release_value();

    tree.reconcile_tabs(keys({ 0, 1, 2, 7, 3 }));
    EXPECT_EQ(tree.children(), items({ tab(0), tab(1), folder(a), tab(3) }));
    EXPECT_EQ(tree.children(a), items({ tab(2), tab(7) }));

    tree.reconcile_tabs(keys({ 0, 7 }));
    EXPECT_EQ(tree.children(), items({ tab(0), folder(a) }));
    EXPECT_EQ(tree.children(a), items({ tab(7) }));
}

TEST_CASE(rows_indent_children_of_open_folders)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2), tab(3) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(3) })).release_value();

    auto rows = tree.visible_rows(1);
    Vector<Row> expected {
        { tab(1), 0, false, Glyph::Open },
        { folder(outer), 0, false, Glyph::Open },
        { tab(2), 1, false, Glyph::Open },
        { folder(inner), 1, false, Glyph::Open },
        { tab(3), 2, false, Glyph::Open },
    };
    EXPECT_EQ(rows, expected);
}

TEST_CASE(a_collapsed_folder_previews_only_its_active_descendant)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    auto a = tree.create_folder_around("A"_string, items({ tab(2), tab(3) })).release_value();
    EXPECT(tree.set_folder_collapsed(a, true));

    Vector<Row> with_active_inside {
        { tab(1), 0, false, Glyph::Open },
        { folder(a), 0, false, Glyph::ClosedWithActiveTab },
        { tab(3), 1, true, Glyph::Open },
    };
    EXPECT_EQ(tree.visible_rows(3), with_active_inside);

    // Switching away hides the preview at once, and the folder stays collapsed.
    Vector<Row> with_active_outside {
        { tab(1), 0, false, Glyph::Open },
        { folder(a), 0, false, Glyph::Closed },
    };
    EXPECT_EQ(tree.visible_rows(1), with_active_outside);
    EXPECT(tree.folder(a)->collapsed);

    // The next active tab decides the preview, not a remembered one.
    Vector<Row> with_other_active_inside {
        { tab(1), 0, false, Glyph::Open },
        { folder(a), 0, false, Glyph::ClosedWithActiveTab },
        { tab(2), 1, true, Glyph::Open },
    };
    EXPECT_EQ(tree.visible_rows(2), with_other_active_inside);
}

TEST_CASE(nested_collapsed_folders_preview_once_under_the_outermost)
{
    auto tree = tree_with_tabs({ 1, 2 });
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(2) })).release_value();
    EXPECT(tree.set_folder_collapsed(outer, true));
    EXPECT(tree.set_folder_collapsed(inner, true));

    Vector<Row> expected {
        { tab(1), 0, false, Glyph::Open },
        { folder(outer), 0, false, Glyph::ClosedWithActiveTab },
        { tab(2), 1, true, Glyph::Open },
    };
    EXPECT_EQ(tree.visible_rows(2), expected);

    // With only the inner one collapsed, the preview sits under it.
    EXPECT(tree.set_folder_collapsed(outer, false));
    Vector<Row> expected_inner {
        { tab(1), 0, false, Glyph::Open },
        { folder(outer), 0, false, Glyph::Open },
        { folder(inner), 1, false, Glyph::ClosedWithActiveTab },
        { tab(2), 2, true, Glyph::Open },
    };
    EXPECT_EQ(tree.visible_rows(2), expected_inner);
}

TEST_CASE(folder_attributes_change_only_existing_folders)
{
    TabFolderTree tree;
    auto a = tree.create_folder("A"_string).release_value();
    EXPECT(tree.rename_folder(a, "Renamed"_string));
    EXPECT(tree.set_folder_icon(a, "star"_string));
    EXPECT_EQ(tree.folder(a)->name, "Renamed"_string);
    EXPECT_EQ(tree.folder(a)->icon, Optional<String> { "star"_string });
    EXPECT(tree.set_folder_icon(a, {}));
    EXPECT(!tree.folder(a)->icon.has_value());
    EXPECT(!tree.rename_folder(99, "X"_string));
    EXPECT(!tree.set_folder_collapsed(99, true));
}

TEST_CASE(serialization_round_trips_the_whole_tree)
{
    auto tree = tree_with_tabs({ 10, 20, 30 });
    auto outer = tree.create_folder_around("Work \"stuff\""_string, items({ tab(20), tab(30) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(30) })).release_value();
    auto empty = tree.create_folder("Empty"_string).release_value();
    EXPECT(tree.set_folder_icon(inner, "book"_string));
    EXPECT(tree.set_folder_collapsed(outer, true));

    auto restored = TRY_OR_FAIL(TabFolderTree::deserialize(tree.serialize(), allowed({ 10, 20, 30 })));
    EXPECT_EQ(restored.serialize(), tree.serialize());
    EXPECT_EQ(restored.children(), items({ tab(10), folder(outer), folder(empty) }));
    EXPECT(restored.folder(outer)->collapsed);
    EXPECT_EQ(restored.folder(inner)->icon, Optional<String> { "book"_string });
    EXPECT_EQ(restored.folder(outer)->name, "Work \"stuff\""_string);

    // New folders after a restore don't reuse saved ids.
    auto added = restored.create_folder("New"_string).release_value();
    EXPECT(added != outer && added != inner && added != empty);
}

TEST_CASE(deserializing_drops_unknown_and_repeated_tabs)
{
    auto tree = tree_with_tabs({ 1, 2, 3 });
    auto a = tree.create_folder_around("A"_string, items({ tab(2), tab(3) })).release_value();
    auto restored = TRY_OR_FAIL(TabFolderTree::deserialize(tree.serialize(), allowed({ 1, 3 })));
    EXPECT_EQ(restored.children(), items({ tab(1), folder(a) }));
    EXPECT_EQ(restored.children(a), items({ tab(3) }));

    auto repeated = TRY_OR_FAIL(TabFolderTree::deserialize(R"({"version":1,"root":[{"tab":1},{"folder":{"id":4,"name":"x","collapsed":false,"children":[{"tab":1}]}}]})"sv, allowed({ 1 })));
    EXPECT_EQ(repeated.children(), items({ tab(1), folder(4) }));
    EXPECT(repeated.children(4).is_empty());
}

TEST_CASE(deserializing_rejects_malformed_trees)
{
    auto allowed_tabs = allowed({ 1 });
    EXPECT(TabFolderTree::deserialize("not json"sv, allowed_tabs).is_error());
    EXPECT(TabFolderTree::deserialize("[]"sv, allowed_tabs).is_error());
    EXPECT(TabFolderTree::deserialize(R"({"version":2,"root":[]})"sv, allowed_tabs).is_error());
    EXPECT(TabFolderTree::deserialize(R"({"version":1})"sv, allowed_tabs).is_error());
    // A folder id that repeats, or isn't positive.
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[{"folder":{"id":1,"name":"a","collapsed":false,"children":[]}},{"folder":{"id":1,"name":"b","collapsed":false,"children":[]}}]})"sv, allowed_tabs).is_error());
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[{"folder":{"id":0,"name":"a","collapsed":false,"children":[]}}]})"sv, allowed_tabs).is_error());
    // A folder without a name.
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[{"folder":{"id":1,"collapsed":false,"children":[]}}]})"sv, allowed_tabs).is_error());
    // An entry that's neither a tab nor a folder.
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[{"space":1}]})"sv, allowed_tabs).is_error());
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[]})"sv, allowed_tabs).value().children().is_empty());
}

TEST_CASE(remapping_tabs_keeps_the_structure)
{
    auto tree = tree_with_tabs({ 100, 200, 300 });
    auto a = tree.create_folder_around("A"_string, items({ tab(200), tab(300) })).release_value();
    auto remapped = tree.with_remapped_tabs([](i64 key) -> Optional<i64> {
        if (key == 300)
            return {};
        return key / 100 - 1;
    });
    EXPECT_EQ(remapped.children(), items({ tab(0), folder(a) }));
    EXPECT_EQ(remapped.children(a), items({ tab(1) }));
    EXPECT_EQ(remapped.folder(a)->name, "A"_string);
}

TEST_CASE(removing_folders_without_tabs_keeps_those_with_tabs_deep_inside)
{
    auto tree = tree_with_tabs({ 1, 2 });
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(2) })).release_value();
    auto empty_inside = tree.create_folder("EmptyInside"_string, TabFolderTree::Destination::at_end_of(outer)).release_value();
    auto empty = tree.create_folder("Empty"_string).release_value();

    tree.remove_folders_without_tabs();
    EXPECT(tree.folder(outer).has_value());
    EXPECT(tree.folder(inner).has_value());
    EXPECT(!tree.folder(empty_inside).has_value());
    EXPECT(!tree.folder(empty).has_value());
    EXPECT_EQ(tree.children(outer), items({ folder(inner) }));
}
