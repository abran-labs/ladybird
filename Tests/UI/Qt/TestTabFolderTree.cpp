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

TEST_CASE(folder_deletion_waits_for_every_descendant_tab_to_close)
{
    TabFolderTree tree;
    auto folder = tree.create_folder("Work"_string).value();
    auto child = tree.create_folder("Notes"_string, TabFolderDestination::at_end_of(folder)).value();
    EXPECT(tree.add_tab(1, TabFolderDestination::at_end_of(folder)));
    EXPECT(tree.add_tab(2, TabFolderDestination::at_end_of(child)));
    EXPECT(tree.remove_tab(1));

    // Tab 2 declined its close. The folder, its child, and the tab stay intact.
    EXPECT(!tree.delete_empty_folder(folder));
    EXPECT(tree.folder(child).has_value());
    EXPECT_EQ(tree.tabs_inside(folder), (Vector<i64> { 2 }));
    EXPECT(tree.remove_tab(2));
    EXPECT(tree.delete_empty_folder(folder));
    EXPECT(!tree.folder(child).has_value());
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

TEST_CASE(keeping_some_tabs_keeps_their_branches_and_the_empty_folders_in_them)
{
    // Work: kept tab 1, Ideas (empty, with Sketches), Confidential (forgotten tab 2, Drafts with forgotten tab 3 and Scratch),
    // Mixed (Deep with kept tab 4, Secret with forgotten tab 5, Notes). Root: Archive (forgotten tab 6), Lonely (empty).
    auto tree = tree_with_tabs({ 1, 2, 3, 4, 5, 6 });
    auto work = tree.create_folder_around("Work"_string, items({ tab(1) })).release_value();
    auto ideas = tree.create_folder("Ideas"_string, TabFolderTree::Destination::at_end_of(work)).release_value();
    auto sketches = tree.create_folder("Sketches"_string, TabFolderTree::Destination::at_end_of(ideas)).release_value();
    auto confidential = tree.create_folder("Confidential"_string, TabFolderTree::Destination::at_end_of(work)).release_value();
    EXPECT(tree.move_items(items({ tab(2) }), TabFolderTree::Destination::at_end_of(confidential)));
    auto drafts = tree.create_folder("Drafts"_string, TabFolderTree::Destination::at_end_of(confidential)).release_value();
    EXPECT(tree.move_items(items({ tab(3) }), TabFolderTree::Destination::at_end_of(drafts)));
    auto scratch = tree.create_folder("Scratch"_string, TabFolderTree::Destination::at_end_of(drafts)).release_value();
    auto mixed = tree.create_folder("Mixed"_string, TabFolderTree::Destination::at_end_of(work)).release_value();
    auto deep = tree.create_folder("Deep"_string, TabFolderTree::Destination::at_end_of(mixed)).release_value();
    EXPECT(tree.move_items(items({ tab(4) }), TabFolderTree::Destination::at_end_of(deep)));
    auto secret = tree.create_folder("Secret"_string, TabFolderTree::Destination::at_end_of(mixed)).release_value();
    EXPECT(tree.move_items(items({ tab(5) }), TabFolderTree::Destination::at_end_of(secret)));
    auto notes = tree.create_folder("Notes"_string, TabFolderTree::Destination::at_end_of(mixed)).release_value();
    auto archive = tree.create_folder_around("Archive"_string, items({ tab(6) })).release_value();
    auto lonely = tree.create_folder("Lonely"_string).release_value();

    auto kept = tree.with_only_tabs_kept([](auto key) { return key == 1 || key == 4; });
    EXPECT_EQ(kept.children(), items({ folder(work) }));
    EXPECT_EQ(kept.children(work), items({ tab(1), folder(ideas), folder(mixed) }));
    EXPECT_EQ(kept.children(ideas), items({ folder(sketches) }));
    EXPECT_EQ(kept.children(mixed), items({ folder(deep), folder(notes) }));
    EXPECT_EQ(kept.children(deep), items({ tab(4) }));
    for (auto gone : { confidential, drafts, scratch, secret, archive, lonely })
        EXPECT(!kept.folder(gone).has_value());
    EXPECT_EQ(kept.tabs_in_order(), keys({ 1, 4 }));
}

TEST_CASE(keeping_every_tab_drops_only_empty_folders_outside_tabbed_ones)
{
    auto tree = tree_with_tabs({ 1, 2 });
    auto outer = tree.create_folder_around("Outer"_string, items({ tab(2) })).release_value();
    auto inner = tree.create_folder_around("Inner"_string, items({ tab(2) })).release_value();
    auto empty_inside = tree.create_folder("EmptyInside"_string, TabFolderTree::Destination::at_end_of(outer)).release_value();
    auto empty = tree.create_folder("Empty"_string).release_value();
    auto empty_child = tree.create_folder("EmptyChild"_string, TabFolderTree::Destination::at_end_of(empty)).release_value();

    auto kept = tree.with_only_tabs_kept([](auto) { return true; });
    EXPECT(kept.folder(outer).has_value());
    EXPECT(kept.folder(inner).has_value());
    EXPECT(kept.folder(empty_inside).has_value());
    EXPECT(!kept.folder(empty).has_value());
    EXPECT(!kept.folder(empty_child).has_value());
    EXPECT_EQ(kept.children(outer), items({ folder(inner), folder(empty_inside) }));
}

static String string_of_bytes(size_t count, char ch = 'n')
{
    StringBuilder builder;
    for (size_t i = 0; i < count; ++i)
        builder.append(ch);
    return builder.to_string_without_validation();
}

TEST_CASE(folder_names_longer_than_the_limit_are_rejected_without_changing_the_tree)
{
    auto tree = tree_with_tabs({ 1 });
    auto longest = string_of_bytes(TabFolderTree::MAX_FOLDER_NAME_BYTES);
    auto too_long = string_of_bytes(TabFolderTree::MAX_FOLDER_NAME_BYTES + 1);

    auto a = tree.create_folder(longest);
    EXPECT(a.has_value());
    auto before = tree.serialize();
    EXPECT(!tree.create_folder(too_long).has_value());
    EXPECT(!tree.create_folder_around(too_long, items({ tab(1) })).has_value());
    EXPECT(!tree.rename_folder(*a, too_long));
    EXPECT_EQ(tree.serialize(), before);
    EXPECT(TabFolderTree::is_valid_folder_name(longest));
    EXPECT(!TabFolderTree::is_valid_folder_name(too_long));
}

TEST_CASE(folder_icons_must_be_a_short_nonempty_id)
{
    TabFolderTree tree;
    auto a = tree.create_folder("A"_string).release_value();
    EXPECT(tree.set_folder_icon(a, string_of_bytes(TabFolderTree::MAX_FOLDER_ICON_BYTES, 'i')));
    auto before = tree.serialize();
    EXPECT(!tree.set_folder_icon(a, string_of_bytes(TabFolderTree::MAX_FOLDER_ICON_BYTES + 1, 'i')));
    EXPECT(!tree.set_folder_icon(a, String {}));
    EXPECT_EQ(tree.serialize(), before);
    EXPECT(tree.set_folder_icon(a, {}));
}

// A chain of nested folders, the outermost first.
static Vector<TabFolderId> nest_folders(TabFolderTree& tree, size_t count, Optional<TabFolderId> parent = {})
{
    Vector<TabFolderId> chain;
    for (size_t i = 0; i < count; ++i) {
        auto id = tree.create_folder(MUST(String::formatted("level {}", i)), TabFolderTree::Destination::at_end_of(parent));
        EXPECT(id.has_value());
        if (!id.has_value())
            break;
        chain.append(*id);
        parent = id;
    }
    return chain;
}

TEST_CASE(folders_nest_only_up_to_the_depth_limit)
{
    auto tree = tree_with_tabs({ 1 });
    auto chain = nest_folders(tree, TabFolderTree::MAX_FOLDER_DEPTH);
    EXPECT_EQ(chain.size(), TabFolderTree::MAX_FOLDER_DEPTH);
    auto before = tree.serialize();

    EXPECT(!tree.create_folder("Too deep"_string, TabFolderTree::Destination::at_end_of(chain.last())).has_value());

    // Wrapping the outermost folder would push the innermost one past the limit, so nothing is created.
    EXPECT(!tree.create_folder_around("Wrapper"_string, items({ folder(chain.first()) })).has_value());
    EXPECT_EQ(tree.serialize(), before);

    // A tab can still go into the innermost folder; tabs don't add depth.
    EXPECT(tree.move_items(items({ tab(1) }), TabFolderTree::Destination::at_end_of(chain.last())));
}

TEST_CASE(moving_a_subtree_past_the_depth_limit_is_rejected)
{
    TabFolderTree tree;
    auto deep = nest_folders(tree, TabFolderTree::MAX_FOLDER_DEPTH - 1);
    auto pair = nest_folders(tree, 2);
    auto before = tree.serialize();

    EXPECT(!tree.can_move_items(items({ folder(pair.first()) }), TabFolderTree::Destination::at_end_of(deep.last())));
    EXPECT(!tree.move_items(items({ folder(pair.first()) }), TabFolderTree::Destination::at_end_of(deep.last())));
    EXPECT_EQ(tree.serialize(), before);

    // The innermost of the pair alone fits.
    EXPECT(tree.move_items(items({ folder(pair.last()) }), TabFolderTree::Destination::at_end_of(deep.last())));
}

TEST_CASE(a_window_holds_only_up_to_the_folder_limit)
{
    TabFolderTree tree;
    for (size_t i = 0; i < TabFolderTree::MAX_FOLDERS; ++i)
        EXPECT(tree.create_folder("f"_string).has_value());
    EXPECT(!tree.can_create_folder());
    EXPECT(!tree.create_folder("one more"_string).has_value());
}

TEST_CASE(every_accepted_tree_at_the_limits_round_trips)
{
    // The worst case for the saved size: control characters escape to six bytes each.
    auto worst_name = string_of_bytes(TabFolderTree::MAX_FOLDER_NAME_BYTES, '\x01');
    auto worst_icon = string_of_bytes(TabFolderTree::MAX_FOLDER_ICON_BYTES, '\x02');

    auto tree = tree_with_tabs({ 1, 2 });
    auto unrelated = tree.create_folder_around("Unrelated"_string, items({ tab(2) })).release_value();
    auto chain = nest_folders(tree, TabFolderTree::MAX_FOLDER_DEPTH);
    EXPECT(tree.move_items(items({ tab(1) }), TabFolderTree::Destination::at_end_of(chain.last())));
    while (tree.can_create_folder()) {
        auto id = tree.create_folder(worst_name).release_value();
        EXPECT(tree.set_folder_icon(id, worst_icon));
    }
    EXPECT(tree.rename_folder(chain.last(), worst_name));

    auto serialized = tree.serialize();
    EXPECT(serialized.byte_count() <= TabFolderTree::MAX_SERIALIZED_BYTES_WITHOUT_TABS);
    auto restored = TRY_OR_FAIL(TabFolderTree::deserialize(serialized, allowed({ 1, 2 })));
    EXPECT_EQ(restored.serialize(), serialized);
    EXPECT_EQ(restored.children(unrelated), items({ tab(2) }));
    EXPECT_EQ(restored.children(chain.last()), items({ tab(1) }));
}

TEST_CASE(deserializing_rejects_trees_past_the_limits)
{
    auto make_chain = [](size_t depth) {
        StringBuilder builder;
        builder.append(R"({"version":1,"root":)"sv);
        for (size_t i = 0; i < depth; ++i)
            builder.appendff(R"([{{"folder":{{"id":{},"name":"x","collapsed":false,"children":)", i + 1);
        builder.append("[]"sv);
        for (size_t i = 0; i < depth; ++i)
            builder.append("}}]"sv);
        builder.append('}');
        return builder.to_byte_string();
    };
    EXPECT(!TabFolderTree::deserialize(make_chain(TabFolderTree::MAX_FOLDER_DEPTH), {}).is_error());
    EXPECT(TabFolderTree::deserialize(make_chain(TabFolderTree::MAX_FOLDER_DEPTH + 1), {}).is_error());

    StringBuilder many;
    many.append(R"({"version":1,"root":[)"sv);
    for (size_t i = 0; i <= TabFolderTree::MAX_FOLDERS; ++i)
        many.appendff(R"({}{{"folder":{{"id":{},"name":"x","collapsed":false,"children":[]}}}})", i == 0 ? "" : ",", i + 1);
    many.append("]}"sv);
    EXPECT(TabFolderTree::deserialize(many.string_view(), {}).is_error());
}

TEST_CASE(new_folders_get_a_fresh_id_after_the_largest_possible_one)
{
    auto max_id = NumericLimits<TabFolderId>::max();
    auto tree = TRY_OR_FAIL(TabFolderTree::deserialize(MUST(String::formatted(R"({{"version":1,"root":[{{"tab":1}},{{"folder":{{"id":{},"name":"max","collapsed":false,"children":[]}}}}]}})", max_id)), allowed({ 1 })));
    EXPECT(tree.folder(max_id).has_value());

    auto created = tree.create_folder("A"_string);
    EXPECT(created.has_value());
    EXPECT(*created > 0 && *created != max_id);
    auto around = tree.create_folder_around("B"_string, items({ tab(1) }));
    EXPECT(around.has_value());
    EXPECT(*around > 0 && *around != max_id && *around != *created);
    EXPECT_EQ(tree.children(*around), items({ tab(1) }));

    auto restored = TRY_OR_FAIL(TabFolderTree::deserialize(tree.serialize(), allowed({ 1 })));
    EXPECT_EQ(restored.serialize(), tree.serialize());
}

TEST_CASE(splitting_keeps_each_empty_folder_in_exactly_one_half_with_its_ancestors)
{
    // Work holds pinned tab 1 and an empty Ideas folder (with an empty Sketches folder in it); tab 2 is unpinned at the root.
    auto tree = tree_with_tabs({ 1, 2 });
    auto work = tree.create_folder_around("Work"_string, items({ tab(1) })).release_value();
    auto ideas = tree.create_folder("Ideas"_string, TabFolderTree::Destination::at_end_of(work)).release_value();
    auto sketches = tree.create_folder("Sketches"_string, TabFolderTree::Destination::at_end_of(ideas)).release_value();
    auto loose = tree.create_folder("Loose"_string).release_value();
    auto reading = tree.create_folder_around("Reading"_string, items({ tab(2) })).release_value();
    auto later = tree.create_folder("Later"_string, TabFolderTree::Destination::at_end_of(reading)).release_value();

    auto split = tree.split([](auto key) { return key == 1; }, TabFolderTree::SplitHalf::Second);
    auto const& pinned = split.first;
    auto const& unpinned = split.second;

    EXPECT_EQ(pinned.children(), items({ folder(work) }));
    EXPECT_EQ(pinned.children(work), items({ tab(1), folder(ideas) }));
    EXPECT_EQ(pinned.children(ideas), items({ folder(sketches) }));
    EXPECT(!pinned.folder(loose).has_value());
    EXPECT(!pinned.folder(reading).has_value());

    EXPECT_EQ(unpinned.children(), items({ folder(reading), folder(loose) }));
    EXPECT_EQ(unpinned.children(reading), items({ tab(2), folder(later) }));
    EXPECT(!unpinned.folder(work).has_value());
    EXPECT(!unpinned.folder(ideas).has_value());
    EXPECT(!unpinned.folder(sketches).has_value());
}

TEST_CASE(splitting_puts_folders_without_a_tabbed_ancestor_in_the_default_half)
{
    auto tree = tree_with_tabs({ 1 });
    auto loose = tree.create_folder("Loose"_string).release_value();
    auto inner = tree.create_folder("Inner"_string, TabFolderTree::Destination::at_end_of(loose)).release_value();

    auto split = tree.split([](auto) { return true; }, TabFolderTree::SplitHalf::First);
    EXPECT_EQ(split.first.children(), items({ tab(1), folder(loose) }));
    EXPECT_EQ(split.first.children(loose), items({ folder(inner) }));
    EXPECT(!split.second.has_folders());
}

TEST_CASE(splitting_a_folder_with_tabs_on_both_sides_keeps_it_in_both)
{
    auto tree = tree_with_tabs({ 1, 2 });
    auto both = tree.create_folder_around("Both"_string, items({ tab(1), tab(2) })).release_value();
    auto empty = tree.create_folder("Empty"_string, TabFolderTree::Destination::at_end_of(both)).release_value();

    auto split = tree.split([](auto key) { return key == 1; }, TabFolderTree::SplitHalf::Second);
    // An empty folder under one that's in both halves goes to the default half only.
    EXPECT_EQ(split.first.children(both), items({ tab(1) }));
    EXPECT_EQ(split.second.children(both), items({ tab(2), folder(empty) }));
}

TEST_CASE(deserializing_can_keep_every_tab_reference)
{
    auto tree = TRY_OR_FAIL(TabFolderTree::deserialize_with(R"({"version":1,"root":[{"tab":5},{"folder":{"id":1,"name":"x","collapsed":false,"children":[{"tab":7},{"tab":5}]}}]})"sv, [](auto) { return true; }));
    EXPECT_EQ(tree.children(), items({ tab(5), folder(1) }));
    // A repeated reference still goes; it's the same tab.
    EXPECT_EQ(tree.children(1), items({ tab(7) }));
}

TEST_CASE(a_folder_color_is_one_of_the_named_colors_and_round_trips)
{
    auto tree = tree_with_tabs({ 1, 2 });
    auto a = tree.create_folder_around("A"_string, items({ tab(1) })).release_value();
    EXPECT(!tree.folder(a)->color.has_value());

    EXPECT(TabFolderTree::is_valid_folder_color("blue"sv));
    EXPECT(!TabFolderTree::is_valid_folder_color("chartreuse"sv));
    EXPECT(!TabFolderTree::is_valid_folder_color(""sv));
    EXPECT(!tree.set_folder_color(a, "chartreuse"_string));
    EXPECT(!tree.folder(a)->color.has_value());

    EXPECT(tree.set_folder_color(a, "green"_string));
    auto restored = TRY_OR_FAIL(TabFolderTree::deserialize(tree.serialize(), allowed({ 1, 2 })));
    EXPECT_EQ(restored.folder(a)->color, Optional<String> { "green"_string });
    EXPECT_EQ(restored.serialize(), tree.serialize());

    // Clearing the color goes back to the default, and the saved form leaves it out.
    EXPECT(tree.set_folder_color(a, {}));
    EXPECT(!tree.serialize().contains("color"sv));
}

TEST_CASE(a_saved_tree_without_colors_still_reads_and_a_bad_color_fails_it)
{
    auto allowed_tabs = allowed({ 1 });
    auto old = TRY_OR_FAIL(TabFolderTree::deserialize(R"({"version":1,"root":[{"folder":{"id":3,"name":"a","collapsed":false,"children":[{"tab":1}]}}]})"sv, allowed_tabs));
    EXPECT(!old.folder(3)->color.has_value());
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[{"folder":{"id":3,"name":"a","color":"mauve","collapsed":false,"children":[]}}]})"sv, allowed_tabs).is_error());
    EXPECT(TabFolderTree::deserialize(R"({"version":1,"root":[{"folder":{"id":3,"name":"a","color":7,"collapsed":false,"children":[]}}]})"sv, allowed_tabs).is_error());
}
