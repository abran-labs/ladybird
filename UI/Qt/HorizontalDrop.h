/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <UI/Qt/TabFolderTree.h>

namespace Ladybird::HorizontalDrop {

// A chip edge inserts beside the item at its own depth; joining the folder is a separate chip-body target.
inline Optional<TabFolderDestination> beside(TabFolderTree const& tree, TabFolderTree::Item item, bool after)
{
    auto position = tree.position_of(item);
    if (position.has_value())
        position->index += after;
    return position;
}

}
