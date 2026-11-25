/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <UI/Qt/TabFolderTree.h>

namespace Ladybird::HorizontalDrop {

// A gap beside a top-level group or loose tab is a root destination, not the last visible tab's folder.
inline Optional<TabFolderDestination> beside_root(TabFolderTree const& tree, TabFolderTree::Item root, bool after)
{
    auto position = tree.position_of(root);
    if (!position.has_value() || position->parent.has_value())
        return {};
    position->index += after;
    return position;
}

// A flattened member is not a request to join its hidden subfolder. Reorder inside that subfolder only when every
// dragged tab already shares it; otherwise use its direct branch in the visible group as the insertion anchor.
inline Optional<TabFolderDestination> beside_member(TabFolderTree const& tree, TabFolderTree::Item member, TabFolderId group, Vector<TabFolderTree::Item> const& moving, bool after)
{
    auto position = tree.position_of(member);
    if (!position.has_value() || !tree.is_inside(member, group))
        return {};
    if (moving.contains([&](auto item) { return tree.parent_of(item) != position->parent; })) {
        while (position->parent != group)
            position = tree.position_of(TabFolderTree::Item::folder(position->parent.value()));
    }
    position->index += after;
    return position;
}

}
