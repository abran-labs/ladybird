/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <UI/Qt/TabFolderTree.h>
#include <functional>

class QMenu;
class QPoint;
class QWidget;

namespace Ladybird::TabFolderMenus {

enum class Action {
    None,
    Rename,
    ChangeIcon,
    NewSubfolder,
    Unpack,
    Delete,
};

Action exec_folder(QWidget* parent, QPoint const& global_pos);

// The parent menu owns the new submenu. For a persistent parent menu, remove and delete the submenu after the popup.
void add_move_menu(QMenu&, TabFolderTree const&, std::function<void(TabFolderId)> move_tabs, std::function<void()> create_folder);

}
