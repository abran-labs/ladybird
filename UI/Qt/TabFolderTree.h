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

    // Limits that every change enforces, so that whatever the tree accepts, it also reads back. A change past one is
    // refused and leaves the tree as it was: the mutators return false or nothing. The UI checks these ahead of time
    // with is_valid_folder_name(), is_valid_folder_icon(), can_create_folder() and can_move_items(), to disable an
    // action or keep a name field short, rather than to explain a refusal afterwards.
    static constexpr size_t MAX_FOLDER_NAME_BYTES = 1024;
    static constexpr size_t MAX_FOLDER_ICON_BYTES = 128;
    // Folders inside folders; a folder at the root is at depth 1. Tabs don't count.
    static constexpr size_t MAX_FOLDER_DEPTH = 32;
    static constexpr size_t MAX_FOLDERS = 256;
    // What the folders alone can take up when saved, at most: every name and icon escaped at six bytes a byte.
    static constexpr size_t MAX_SERIALIZED_FOLDER_BYTES = 6 * MAX_FOLDER_NAME_BYTES + 6 * MAX_FOLDER_ICON_BYTES + 128;
    static constexpr size_t MAX_SERIALIZED_BYTES_WITHOUT_TABS = MAX_FOLDERS * MAX_SERIALIZED_FOLDER_BYTES + 64;
    // What each tab adds when saved, at most.
    static constexpr size_t MAX_SERIALIZED_TAB_BYTES = 32;

    static bool is_valid_folder_name(StringView name) { return name.length() <= MAX_FOLDER_NAME_BYTES; }
    static bool is_valid_folder_icon(StringView icon) { return !icon.is_empty() && icon.length() <= MAX_FOLDER_ICON_BYTES; }

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

    // Whether another folder fits in the window. Where it can go also depends on depth; see can_create_folder_in().
    bool can_create_folder() const { return m_folders.size() < MAX_FOLDERS; }
    bool can_create_folder_in(Optional<TabFolderId> parent) const;
    Optional<TabFolderId> create_folder(String name, Destination = {});
    // A new folder in the place of the first of the items, holding all of them in their order. Refused when that would
    // put a folder among the items past the depth limit.
    Optional<TabFolderId> create_folder_around(String name, Vector<Item> const& items);
    bool rename_folder(TabFolderId, String name);
    bool set_folder_icon(TabFolderId, Optional<String> icon);
    bool set_folder_collapsed(TabFolderId, bool collapsed);

    // Moves the items, in their order in the tree whatever order they're listed in, to the destination. An item inside
    // another one that's moving moves along with it. A move that would put a folder inside itself, or any folder past the
    // depth limit, changes nothing.
    bool can_move_items(Vector<Item> const&, Destination const&) const;
    bool move_items(Vector<Item> const&, Destination const&);

    // Removes only the folder: what it held takes its place in its parent, in the same order.
    bool unpack_folder(TabFolderId);
    // Removes the folder with every folder and tab inside it, and returns those tabs, in order, for the caller to close.
    Vector<TabKey> delete_folder(TabFolderId);
    // The tree with only the tabs that keep says to, for a window that forgets the rest. A folder stays when it holds a
    // kept tab. A folder that holds no tab at all stays when the nearest folder around it that holds tabs holds a kept one,
    // so an empty folder stays in a kept branch, and goes with a group of only forgotten tabs, or at the root. A folder
    // whose tabs are all forgotten goes, along with everything in it, so none of its names stay.
    TabFolderTree with_only_tabs_kept(Function<bool(TabKey)> const& keep) const;

    enum class SplitHalf : u8 {
        First,
        Second,
    };
    struct Split;
    // Splits the tree in two by its tabs: a tab goes to the first half when in_first says so. A folder goes to each half
    // that holds a tab of it. A folder without tabs goes along with its nearest ancestor that has some, when that one is
    // in just one half, and to the default half otherwise, so each empty folder lands in exactly one half together with
    // all of its ancestors.
    Split split(Function<bool(TabKey)> const& in_first, SplitHalf default_half) const;

    // The saved form references tabs by their keys. Reading drops a tab reference that's not allowed or that repeats one
    // before it; anything else malformed, or past a limit, fails the whole tree. Anything serialize() writes reads back.
    // Its size is at most MAX_SERIALIZED_BYTES_WITHOUT_TABS plus MAX_SERIALIZED_TAB_BYTES a tab.
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
    Optional<TabFolderId> allocate_folder_id() const;
    // How deep the deepest folder inside the item is, counting the item itself when it's a folder.
    size_t subtree_folder_depth(Item) const;
    // The depth a folder placed in the parent would be at.
    size_t depth_in(Optional<TabFolderId> parent) const;

    Vector<Item> m_root;
    HashMap<TabFolderId, FolderNode> m_folders;
    HashMap<TabKey, Optional<TabFolderId>> m_tab_parents;
    TabFolderId m_last_folder_id { 0 };
};

struct TabFolderTree::Split {
    TabFolderTree first;
    TabFolderTree second;
};

}
