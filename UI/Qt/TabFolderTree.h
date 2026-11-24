/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/HashTable.h>
#include <AK/NumericLimits.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Ladybird {

using TabFolderId = i64;

// A place among a parent's children, which is the root when there's no parent. The index counts the parent's children
// as they are before a move, the way a landing line between two rows shows it, and is clamped to them.
struct TabFolderDestination {
    Optional<TabFolderId> parent;
    size_t index { NumericLimits<size_t>::max() };

    static TabFolderDestination at_end_of(Optional<TabFolderId> parent) { return { parent, NumericLimits<size_t>::max() }; }
};

// One window's folders, in the order the vertical sidebar shows them, together with which folder each of the window's
// tabs is in. Folders hold tabs and other folders. A tab outside every folder sits at the root, in its place among the
// root's folders. The tree knows nothing of pinning, of which tab is current, or of the tabs themselves: a tab is just a
// key, which is the tab's session tab id wherever the tree is saved with the session.
class TabFolderTree {
public:
    using TabKey = i64;

    struct Item {
        enum class Kind : u8 {
            Tab,
            Folder,
        };

        Kind kind { Kind::Tab };
        i64 id { 0 };

        static Item tab(TabKey key) { return { Kind::Tab, key }; }
        static Item folder(TabFolderId id) { return { Kind::Folder, id }; }

        bool is_tab() const { return kind == Kind::Tab; }
        bool is_folder() const { return kind == Kind::Folder; }

        bool operator==(Item const&) const = default;
    };

    struct Folder {
        TabFolderId id { 0 };
        String name;
        // A stable id from the folder icon catalog, or nothing for the default folder icon.
        Optional<String> icon;
        bool collapsed { false };
    };

    using Destination = TabFolderDestination;

    enum class FolderGlyph : u8 {
        Open,
        Closed,
        // Collapsed, with the active tab somewhere inside it.
        ClosedWithActiveTab,
    };

    // A row of the sidebar. A collapsed folder hides what it holds, except that the outermost collapsed folder around
    // the active tab shows that tab, as a preview row, right below its header.
    struct Row {
        Item item;
        size_t depth { 0 };
        bool is_active_tab_preview { false };
        FolderGlyph glyph { FolderGlyph::Open };

        bool operator==(Row const&) const = default;
    };

    struct FolderEntry {
        TabFolderId id { 0 };
        size_t depth { 0 };

        bool operator==(FolderEntry const&) const = default;
    };

    bool has_folders() const { return !m_folders.is_empty(); }
    bool contains(Item) const;
    Optional<Folder const&> folder(TabFolderId) const;
    Vector<Item> const& children(Optional<TabFolderId> parent = {}) const;
    // The folder that holds the item, or nothing for an item at the root or one that isn't in the tree.
    Optional<TabFolderId> parent_of(Item) const;
    Optional<Destination> position_of(Item) const;
    // Whether the item is inside the folder, at any depth. A folder isn't inside itself.
    bool is_inside(Item, TabFolderId ancestor) const;

    // Every tab, in the order the rows list them with every folder open.
    Vector<TabKey> tabs_in_order() const;
    // The tabs inside the folder, at any depth, in order.
    Vector<TabKey> tabs_inside(TabFolderId) const;
    // Every folder, in order, with its depth; the destinations that Move to Folder offers.
    Vector<FolderEntry> folders_in_order() const;
    Vector<Row> visible_rows(Optional<TabKey> active_tab) const;

    // Tab lifecycle. A tab that's already in the tree stays where it is.
    bool add_tab(TabKey, Destination = {});
    bool remove_tab(TabKey);
    // Brings the tree's tabs in line with the window's: a tab that's gone leaves, and a new tab goes right after the
    // nearest tab before it in the window's order, in that tab's folder.
    void reconcile_tabs(Vector<TabKey> const& window_order);

    Optional<TabFolderId> create_folder(String name, Destination = {});
    // A new folder in the place of the first of the items, holding all of them in their order.
    Optional<TabFolderId> create_folder_around(String name, Vector<Item> const& items);
    bool rename_folder(TabFolderId, String name);
    bool set_folder_icon(TabFolderId, Optional<String> icon);
    bool set_folder_collapsed(TabFolderId, bool collapsed);

    // Moves the items, in their order in the tree whatever order they're listed in, to the destination. An item inside
    // another one that's moving moves along with it. A move that would put a folder inside itself changes nothing.
    bool can_move_items(Vector<Item> const&, Destination const&) const;
    bool move_items(Vector<Item> const&, Destination const&);

    // Removes only the folder: what it held takes its place in its parent, in the same order.
    bool unpack_folder(TabFolderId);
    // Removes the folder with every folder and tab inside it, and returns those tabs, in order, for the caller to close.
    Vector<TabKey> delete_folder(TabFolderId);
    // Drops every folder with no tab anywhere inside it, for a window that keeps only some of its tabs.
    void remove_folders_without_tabs();

    // The saved form references tabs by their keys. Reading drops a tab reference that's not allowed or that repeats one
    // before it; anything else malformed fails the whole tree.
    String serialize() const;
    static ErrorOr<TabFolderTree> deserialize(StringView, HashTable<TabKey> const& allowed_tabs);
    // The same tree, with each tab's key mapped to another one; a tab that maps to nothing is dropped.
    TabFolderTree with_remapped_tabs(Function<Optional<TabKey>(TabKey)> const&) const;

private:
    struct FolderNode {
        Folder folder;
        Optional<TabFolderId> parent;
        Vector<Item> children;
    };

    Vector<Item>* children_of(Optional<TabFolderId>);
    Vector<Item> const* children_of(Optional<TabFolderId>) const;
    void set_parent(Item, Optional<TabFolderId>);
    void detach(Item);
    void insert(Item, Optional<TabFolderId> parent, size_t index);
    Vector<Item> preorder() const;
    Vector<Item> normalized_selection(Vector<Item> const&) const;
    TabFolderId allocate_folder_id();

    Vector<Item> m_root;
    HashMap<TabFolderId, FolderNode> m_folders;
    HashMap<TabKey, Optional<TabFolderId>> m_tab_parents;
    TabFolderId m_last_folder_id { 0 };
};

}
