/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <UI/Qt/TabFolderEditing.h>

#include <QApplication>
#include <QKeyEvent>

using namespace Ladybird;
using TabFolderEditing::Selection;

TEST_CASE(a_typed_name_is_trimmed)
{
    EXPECT_EQ(TabFolderEditing::accepted_folder_name("  Work \t"sv), "Work"_string);
    EXPECT_EQ(TabFolderEditing::accepted_folder_name("New Folder"sv), "New Folder"_string);
}

TEST_CASE(a_blank_name_is_not_accepted)
{
    EXPECT(!TabFolderEditing::accepted_folder_name(""sv).has_value());
    EXPECT(!TabFolderEditing::accepted_folder_name("   "sv).has_value());
}

TEST_CASE(a_name_longer_than_a_folder_takes_is_not_accepted)
{
    auto longest = MUST(String::repeated('a', TabFolderTree::MAX_FOLDER_NAME_BYTES));
    EXPECT_EQ(TabFolderEditing::accepted_folder_name(longest), longest);
    auto too_long = MUST(String::repeated('a', TabFolderTree::MAX_FOLDER_NAME_BYTES + 1));
    EXPECT(!TabFolderEditing::accepted_folder_name(too_long).has_value());
}

TEST_CASE(the_first_ctrl_click_takes_in_the_active_tab)
{
    auto selection = TabFolderEditing::toggled({}, 3, 1);
    EXPECT_EQ(selection, (Selection { { 1, 3 }, 3 }));

    selection = TabFolderEditing::toggled(selection, 5, 1);
    EXPECT_EQ(selection, (Selection { { 1, 3, 5 }, 5 }));
}

TEST_CASE(a_ctrl_click_on_a_chosen_tab_takes_it_out)
{
    auto selection = TabFolderEditing::toggled({ { 1, 3, 5 }, 5 }, 3, 1);
    EXPECT_EQ(selection, (Selection { { 1, 5 }, 3 }));

    // The active tab can leave too, and stays out on the next click.
    selection = TabFolderEditing::toggled(selection, 1, 1);
    EXPECT_EQ(selection, (Selection { { 5 }, 1 }));
    selection = TabFolderEditing::toggled(selection, 7, 1);
    EXPECT_EQ(selection, (Selection { { 5, 7 }, 7 }));
}

TEST_CASE(a_ctrl_click_on_the_active_tab_alone_chooses_only_it)
{
    EXPECT_EQ(TabFolderEditing::toggled({}, 1, 1), (Selection { { 1 }, 1 }));
    EXPECT_EQ(TabFolderEditing::toggled({}, 4, {}), (Selection { { 4 }, 4 }));
}

TEST_CASE(a_ctrl_click_after_deselecting_the_last_tab_does_not_reselect_it)
{
    auto none = TabFolderEditing::toggled({ { 1 }, 1 }, 1, 1);
    EXPECT(none.keys.is_empty());
    EXPECT_EQ(TabFolderEditing::toggled(none, 2, 1), (Selection { { 2 }, 2 }));
}

TEST_CASE(shift_replaces_the_range_without_moving_its_anchor)
{
    Vector<TabFolderSync::SidebarRow> rows {
        { TabFolderTree::Item::tab(1) },
        { TabFolderTree::Item::folder(1) },
        { TabFolderTree::Item::tab(2) },
        { TabFolderTree::Item::tab(3) },
    };
    auto selection = TabFolderEditing::extended({}, rows, 3, 1);
    EXPECT_EQ(selection, (Selection { { 1, 2, 3 }, 1 }));
    EXPECT_EQ(TabFolderEditing::extended(selection, rows, 2, 1), (Selection { { 1, 2 }, 1 }));
    EXPECT_EQ(TabFolderEditing::extended({ { 8 }, 8 }, rows, 3, 9), (Selection { { 3 }, 3 }));
}

TEST_CASE(right_click_on_a_selected_tab_targets_the_whole_selection)
{
    Selection selection { { 1, 3, 5 }, 1 };
    EXPECT_EQ(TabFolderEditing::context_tabs(selection, 3), (Vector<i64> { 1, 3, 5 }));
    EXPECT_EQ(TabFolderEditing::context_tabs(selection, 2), (Vector<i64> { 2 }));
}

TEST_CASE(provisional_folder_previews_placement_without_moving_tabs)
{
    using Item = TabFolderTree::Item;
    TabFolderTree tree;
    auto parent = tree.create_folder("Parent"_string).value();
    EXPECT(tree.add_tab(1, TabFolderDestination::at_end_of(parent)));
    EXPECT(tree.add_tab(2));
    EXPECT(tree.set_folder_collapsed(parent, true));
    auto before = tree.serialize();
    TabFolderEditing::FolderCreation creation { {}, { 2, 1 } };

    auto preview = tree;
    auto draft = creation.preview(preview).value();
    EXPECT_EQ(preview.children(parent), (Vector<Item> { Item::folder(draft), Item::tab(1) }));
    EXPECT(!preview.folder(parent)->collapsed);
    EXPECT_EQ(tree.serialize(), before);

    auto created = creation.commit(tree, "Work"_string).value();
    EXPECT_EQ(tree.parent_of(Item::folder(created)), parent);
    EXPECT_EQ(tree.tabs_inside(created), (Vector<i64> { 1, 2 }));
    EXPECT(!tree.folder(parent)->collapsed);
}

TEST_CASE(root_and_subfolder_drafts_do_not_exist_until_accepted)
{
    TabFolderTree tree;
    EXPECT(tree.add_tab(1));
    TabFolderEditing::FolderCreation root_creation;
    auto preview = tree;
    auto draft = root_creation.preview(preview).value();
    EXPECT_EQ(preview.children().last(), TabFolderTree::Item::folder(draft));
    EXPECT(!tree.has_folders());
    auto root = root_creation.commit(tree, "Work"_string).value();

    EXPECT(tree.set_folder_collapsed(root, true));
    TabFolderEditing::FolderCreation subfolder_creation { root, {} };
    preview = tree;
    auto subfolder_draft = subfolder_creation.preview(preview).value();
    EXPECT_EQ(preview.children(root).last(), TabFolderTree::Item::folder(subfolder_draft));
    EXPECT(tree.children(root).is_empty());
    EXPECT(tree.folder(root)->collapsed);
    auto subfolder = subfolder_creation.commit(tree, "Today"_string).value();
    EXPECT_EQ(tree.parent_of(TabFolderTree::Item::folder(subfolder)), root);
    EXPECT(!tree.folder(root)->collapsed);
}

TEST_CASE(inline_editor_reports_enter_or_escape_only_once)
{
    auto exercise = [](int key, Optional<String> expected) {
        QWidget parent;
        int reports = 0;
        Optional<String> actual;
        auto* editor = new TabFolderEditing::FolderNameEditor(&parent, QStringLiteral("New Folder"), [&](auto name) {
            ++reports;
            actual = move(name);
        });
        EXPECT_EQ(editor->selectedText().toStdString(), "New Folder");
        editor->setText(QStringLiteral("  Work "));
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
        QApplication::sendEvent(editor, &event);
        QApplication::sendEvent(editor, &event);
        EXPECT_EQ(reports, 1);
        EXPECT_EQ(actual, expected);
    };
    exercise(Qt::Key_Return, "Work"_string);
    exercise(Qt::Key_Escape, {});
}

TEST_CASE(invalid_enter_keeps_the_inline_editor_available_for_correction)
{
    QWidget parent;
    int reports = 0;
    auto* editor = new TabFolderEditing::FolderNameEditor(&parent, QStringLiteral("New Folder"), [&](Optional<String> name) {
        ++reports;
        EXPECT_EQ(name, "Work"_string);
    });
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    editor->setText(QStringLiteral("  "));
    QApplication::sendEvent(editor, &enter);
    EXPECT_EQ(reports, 0);
    editor->setText(QStringLiteral("Work"));
    QApplication::sendEvent(editor, &enter);
    EXPECT_EQ(reports, 1);
}
