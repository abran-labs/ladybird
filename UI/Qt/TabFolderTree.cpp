/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <UI/Qt/TabFolderTree.h>

namespace Ladybird {

static constexpr i64 TAB_FOLDER_TREE_FORMAT_VERSION = 1;
static constexpr size_t MAX_FOLDER_NAME_BYTES = 1024;
static constexpr size_t MAX_FOLDER_ICON_BYTES = 128;

static Vector<TabFolderTree::Item> const& empty_children()
{
    static Vector<TabFolderTree::Item> const empty;
    return empty;
}

Vector<TabFolderTree::Item>* TabFolderTree::children_of(Optional<TabFolderId> parent)
{
    if (!parent.has_value())
        return &m_root;
    auto node = m_folders.find(*parent);
    return node == m_folders.end() ? nullptr : &node->value.children;
}

Vector<TabFolderTree::Item> const* TabFolderTree::children_of(Optional<TabFolderId> parent) const
{
    if (!parent.has_value())
        return &m_root;
    auto node = m_folders.find(*parent);
    return node == m_folders.end() ? nullptr : &node->value.children;
}

bool TabFolderTree::contains(Item item) const
{
    if (item.is_tab())
        return m_tab_parents.contains(item.id);
    return m_folders.contains(item.id);
}

Optional<TabFolderTree::Folder const&> TabFolderTree::folder(TabFolderId id) const
{
    auto node = m_folders.find(id);
    if (node == m_folders.end())
        return {};
    return node->value.folder;
}

Vector<TabFolderTree::Item> const& TabFolderTree::children(Optional<TabFolderId> parent) const
{
    if (auto const* children = children_of(parent))
        return *children;
    return empty_children();
}

Optional<TabFolderId> TabFolderTree::parent_of(Item item) const
{
    if (item.is_tab()) {
        auto parent = m_tab_parents.find(item.id);
        return parent == m_tab_parents.end() ? Optional<TabFolderId> {} : parent->value;
    }
    auto node = m_folders.find(item.id);
    return node == m_folders.end() ? Optional<TabFolderId> {} : node->value.parent;
}

Optional<TabFolderTree::Destination> TabFolderTree::position_of(Item item) const
{
    if (!contains(item))
        return {};
    auto parent = parent_of(item);
    auto index = children(parent).find_first_index(item);
    VERIFY(index.has_value());
    return Destination { parent, *index };
}

bool TabFolderTree::is_inside(Item item, TabFolderId ancestor) const
{
    if (!contains(item))
        return false;
    for (auto parent = parent_of(item); parent.has_value(); parent = parent_of(Item::folder(*parent))) {
        if (*parent == ancestor)
            return true;
    }
    return false;
}

Vector<TabFolderTree::Item> TabFolderTree::preorder() const
{
    Vector<Item> result;
    auto walk = [&](auto& self, Vector<Item> const& items) -> void {
        for (auto item : items) {
            result.append(item);
            if (item.is_folder())
                self(self, children(item.id));
        }
    };
    walk(walk, m_root);
    return result;
}

Vector<TabFolderTree::TabKey> TabFolderTree::tabs_in_order() const
{
    Vector<TabKey> result;
    for (auto item : preorder()) {
        if (item.is_tab())
            result.append(item.id);
    }
    return result;
}

Vector<TabFolderTree::TabKey> TabFolderTree::tabs_inside(TabFolderId id) const
{
    Vector<TabKey> result;
    auto walk = [&](auto& self, Vector<Item> const& items) -> void {
        for (auto item : items) {
            if (item.is_tab())
                result.append(item.id);
            else
                self(self, children(item.id));
        }
    };
    if (m_folders.contains(id))
        walk(walk, children(id));
    return result;
}

Vector<TabFolderTree::FolderEntry> TabFolderTree::folders_in_order() const
{
    Vector<FolderEntry> result;
    auto walk = [&](auto& self, Vector<Item> const& items, size_t depth) -> void {
        for (auto item : items) {
            if (!item.is_folder())
                continue;
            result.append({ item.id, depth });
            self(self, children(item.id), depth + 1);
        }
    };
    walk(walk, m_root, 0);
    return result;
}

Vector<TabFolderTree::Row> TabFolderTree::visible_rows(Optional<TabKey> active_tab) const
{
    if (active_tab.has_value() && !m_tab_parents.contains(*active_tab))
        active_tab.clear();

    Vector<Row> rows;
    auto walk = [&](auto& self, Vector<Item> const& items, size_t depth) -> void {
        for (auto item : items) {
            if (item.is_tab()) {
                rows.append({ item, depth, false, FolderGlyph::Open });
                continue;
            }

            auto const& node = m_folders.get(item.id).value();
            if (!node.folder.collapsed) {
                rows.append({ item, depth, false, FolderGlyph::Open });
                self(self, node.children, depth + 1);
                continue;
            }

            // Nothing inside a collapsed folder shows, so a collapsed folder further in never adds a second preview.
            auto holds_active_tab = active_tab.has_value() && is_inside(Item::tab(*active_tab), item.id);
            rows.append({ item, depth, false, holds_active_tab ? FolderGlyph::ClosedWithActiveTab : FolderGlyph::Closed });
            if (holds_active_tab)
                rows.append({ Item::tab(*active_tab), depth + 1, true, FolderGlyph::Open });
        }
    };
    walk(walk, m_root, 0);
    return rows;
}

void TabFolderTree::set_parent(Item item, Optional<TabFolderId> parent)
{
    if (item.is_tab())
        m_tab_parents.set(item.id, parent);
    else
        m_folders.find(item.id)->value.parent = parent;
}

void TabFolderTree::detach(Item item)
{
    auto* siblings = children_of(parent_of(item));
    VERIFY(siblings);
    siblings->remove_first_matching([&](auto candidate) { return candidate == item; });
}

void TabFolderTree::insert(Item item, Optional<TabFolderId> parent, size_t index)
{
    auto* siblings = children_of(parent);
    VERIFY(siblings);
    siblings->insert(min(index, siblings->size()), item);
    set_parent(item, parent);
}

TabFolderId TabFolderTree::allocate_folder_id()
{
    return ++m_last_folder_id;
}

bool TabFolderTree::add_tab(TabKey key, Destination destination)
{
    if (m_tab_parents.contains(key) || !children_of(destination.parent))
        return false;
    insert(Item::tab(key), destination.parent, destination.index);
    return true;
}

bool TabFolderTree::remove_tab(TabKey key)
{
    if (!m_tab_parents.contains(key))
        return false;
    detach(Item::tab(key));
    m_tab_parents.remove(key);
    return true;
}

void TabFolderTree::reconcile_tabs(Vector<TabKey> const& window_order)
{
    HashTable<TabKey> in_window;
    for (auto key : window_order)
        in_window.set(key);

    Vector<TabKey> gone;
    for (auto const& it : m_tab_parents) {
        if (!in_window.contains(it.key))
            gone.append(it.key);
    }
    for (auto key : gone)
        remove_tab(key);

    Optional<TabKey> previous;
    for (auto key : window_order) {
        if (!m_tab_parents.contains(key)) {
            if (previous.has_value()) {
                auto position = position_of(Item::tab(*previous)).release_value();
                insert(Item::tab(key), position.parent, position.index + 1);
            } else {
                insert(Item::tab(key), {}, 0);
            }
        }
        previous = key;
    }
}

Optional<TabFolderId> TabFolderTree::create_folder(String name, Destination destination)
{
    if (!children_of(destination.parent))
        return {};
    auto id = allocate_folder_id();
    m_folders.set(id, FolderNode { .folder = { .id = id, .name = move(name), .icon = {}, .collapsed = false }, .parent = {}, .children = {} });
    insert(Item::folder(id), destination.parent, destination.index);
    return id;
}

Optional<TabFolderId> TabFolderTree::create_folder_around(String name, Vector<Item> const& items)
{
    if (items.is_empty())
        return {};
    for (auto item : items) {
        if (!contains(item))
            return {};
    }

    auto selection = normalized_selection(items);
    auto position = position_of(selection.first()).release_value();
    auto id = create_folder(move(name), position).release_value();
    auto moved = move_items(selection, Destination::at_end_of(id));
    VERIFY(moved);
    return id;
}

bool TabFolderTree::rename_folder(TabFolderId id, String name)
{
    auto node = m_folders.find(id);
    if (node == m_folders.end())
        return false;
    node->value.folder.name = move(name);
    return true;
}

bool TabFolderTree::set_folder_icon(TabFolderId id, Optional<String> icon)
{
    auto node = m_folders.find(id);
    if (node == m_folders.end())
        return false;
    node->value.folder.icon = move(icon);
    return true;
}

bool TabFolderTree::set_folder_collapsed(TabFolderId id, bool collapsed)
{
    auto node = m_folders.find(id);
    if (node == m_folders.end())
        return false;
    node->value.folder.collapsed = collapsed;
    return true;
}

// The selected items in tree order, without any that's inside another selected folder.
Vector<TabFolderTree::Item> TabFolderTree::normalized_selection(Vector<Item> const& items) const
{
    Vector<Item> result;
    for (auto item : preorder()) {
        if (!items.contains_slow(item))
            continue;
        auto inside_selected_folder = result.contains([&](auto selected) {
            return selected.is_folder() && is_inside(item, selected.id);
        });
        if (!inside_selected_folder)
            result.append(item);
    }
    return result;
}

bool TabFolderTree::can_move_items(Vector<Item> const& items, Destination const& destination) const
{
    if (items.is_empty() || !children_of(destination.parent))
        return false;
    for (auto item : items) {
        if (!contains(item))
            return false;
        if (item.is_folder() && destination.parent.has_value()) {
            if (*destination.parent == item.id || is_inside(Item::folder(*destination.parent), item.id))
                return false;
        }
    }
    return true;
}

bool TabFolderTree::move_items(Vector<Item> const& items, Destination const& destination)
{
    if (!can_move_items(items, destination))
        return false;

    auto selection = normalized_selection(items);

    // The destination index counts the siblings that are there before the move, so the moving ones ahead of it don't
    // count once they're gone.
    auto const& siblings = children(destination.parent);
    auto index = min(destination.index, siblings.size());
    size_t moving_before_index = 0;
    for (size_t i = 0; i < index; ++i) {
        if (selection.contains_slow(siblings[i]))
            ++moving_before_index;
    }
    index -= moving_before_index;

    for (auto item : selection)
        detach(item);
    for (auto item : selection)
        insert(item, destination.parent, index++);
    return true;
}

bool TabFolderTree::unpack_folder(TabFolderId id)
{
    auto position = position_of(Item::folder(id));
    if (!position.has_value())
        return false;

    auto contents = move(m_folders.find(id)->value.children);
    detach(Item::folder(id));
    m_folders.remove(id);

    auto index = position->index;
    for (auto item : contents)
        insert(item, position->parent, index++);
    return true;
}

Vector<TabFolderTree::TabKey> TabFolderTree::delete_folder(TabFolderId id)
{
    if (!m_folders.contains(id))
        return {};

    auto tabs = tabs_inside(id);
    Vector<TabFolderId> folders { id };
    for (auto const& it : m_folders) {
        if (is_inside(Item::folder(it.key), id))
            folders.append(it.key);
    }

    detach(Item::folder(id));
    for (auto key : tabs)
        m_tab_parents.remove(key);
    for (auto folder_id : folders)
        m_folders.remove(folder_id);
    return tabs;
}

void TabFolderTree::remove_folders_without_tabs()
{
    for (auto const& entry : folders_in_order()) {
        if (m_folders.contains(entry.id) && tabs_inside(entry.id).is_empty())
            (void)delete_folder(entry.id);
    }
}

static JsonArray serialize_items(TabFolderTree const& tree, Vector<TabFolderTree::Item> const& items)
{
    JsonArray array;
    for (auto item : items) {
        JsonObject entry;
        if (item.is_tab()) {
            entry.set("tab"sv, item.id);
        } else {
            auto const& folder = tree.folder(item.id).value();
            JsonObject folder_object;
            folder_object.set("id"sv, folder.id);
            folder_object.set("name"sv, folder.name);
            if (folder.icon.has_value())
                folder_object.set("icon"sv, *folder.icon);
            folder_object.set("collapsed"sv, folder.collapsed);
            folder_object.set("children"sv, serialize_items(tree, tree.children(item.id)));
            entry.set("folder"sv, move(folder_object));
        }
        array.must_append(move(entry));
    }
    return array;
}

String TabFolderTree::serialize() const
{
    JsonObject object;
    object.set("version"sv, TAB_FOLDER_TREE_FORMAT_VERSION);
    object.set("root"sv, serialize_items(*this, m_root));
    return object.serialized();
}

ErrorOr<TabFolderTree> TabFolderTree::deserialize(StringView serialized, HashTable<TabKey> const& allowed_tabs)
{
    auto json = TRY(JsonValue::from_string(serialized));
    if (!json.is_object())
        return Error::from_string_literal("Tab folder tree is not an object");
    auto const& object = json.as_object();
    if (object.get_i64("version"sv) != TAB_FOLDER_TREE_FORMAT_VERSION)
        return Error::from_string_literal("Tab folder tree has an unsupported version");
    auto root = object.get_array("root"sv);
    if (!root.has_value())
        return Error::from_string_literal("Tab folder tree has no root");

    TabFolderTree tree;
    auto read_items = [&](auto& self, JsonArray const& array, Optional<TabFolderId> parent) -> ErrorOr<void> {
        for (size_t i = 0; i < array.size(); ++i) {
            auto const& value = array.at(i);
            if (!value.is_object() || value.as_object().size() != 1)
                return Error::from_string_literal("Tab folder tree entry is not a tab or a folder");
            auto const& entry = value.as_object();

            if (entry.has("tab"sv)) {
                auto key = entry.get_i64("tab"sv);
                if (!key.has_value())
                    return Error::from_string_literal("Tab folder tree tab is not an integer");
                if (allowed_tabs.contains(*key) && !tree.m_tab_parents.contains(*key))
                    tree.insert(Item::tab(*key), parent, NumericLimits<size_t>::max());
                continue;
            }

            auto folder_object = entry.get_object("folder"sv);
            if (!folder_object.has_value())
                return Error::from_string_literal("Tab folder tree entry is not a tab or a folder");
            auto id = folder_object->get_i64("id"sv);
            if (!id.has_value() || *id <= 0 || tree.m_folders.contains(*id))
                return Error::from_string_literal("Tab folder has an invalid id");
            auto name = folder_object->get_string("name"sv);
            if (!name.has_value() || name->byte_count() > MAX_FOLDER_NAME_BYTES)
                return Error::from_string_literal("Tab folder has an invalid name");
            Optional<String> icon;
            if (folder_object->has("icon"sv)) {
                auto icon_value = folder_object->get_string("icon"sv);
                if (!icon_value.has_value() || icon_value->byte_count() > MAX_FOLDER_ICON_BYTES)
                    return Error::from_string_literal("Tab folder has an invalid icon");
                icon = *icon_value;
            }
            auto collapsed = false;
            if (folder_object->has("collapsed"sv)) {
                auto collapsed_value = folder_object->get_bool("collapsed"sv);
                if (!collapsed_value.has_value())
                    return Error::from_string_literal("Tab folder has an invalid collapsed state");
                collapsed = *collapsed_value;
            }
            auto children = folder_object->get_array("children"sv);
            if (!children.has_value())
                return Error::from_string_literal("Tab folder has no children");

            tree.m_folders.set(*id, FolderNode { .folder = { .id = *id, .name = *name, .icon = move(icon), .collapsed = collapsed }, .parent = {}, .children = {} });
            tree.insert(Item::folder(*id), parent, NumericLimits<size_t>::max());
            tree.m_last_folder_id = max(tree.m_last_folder_id, *id);
            TRY(self(self, *children, *id));
        }
        return {};
    };
    TRY(read_items(read_items, *root, {}));
    return tree;
}

TabFolderTree TabFolderTree::with_remapped_tabs(Function<Optional<TabKey>(TabKey)> const& remap) const
{
    TabFolderTree tree;
    tree.m_last_folder_id = m_last_folder_id;
    auto copy = [&](auto& self, Vector<Item> const& items, Optional<TabFolderId> parent) -> void {
        for (auto item : items) {
            if (item.is_tab()) {
                auto key = remap(item.id);
                if (key.has_value() && !tree.m_tab_parents.contains(*key))
                    tree.insert(Item::tab(*key), parent, NumericLimits<size_t>::max());
                continue;
            }
            auto const& node = m_folders.get(item.id).value();
            tree.m_folders.set(item.id, FolderNode { .folder = node.folder, .parent = {}, .children = {} });
            tree.insert(item, parent, NumericLimits<size_t>::max());
            self(self, node.children, item.id);
        }
    };
    copy(copy, m_root, {});
    return tree;
}

}
