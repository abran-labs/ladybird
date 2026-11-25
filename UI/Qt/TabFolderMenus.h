/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <UI/Qt/TabFolderTree.h>
#include <functional>

class QColor;
class QMenu;
class QString;
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

// What a menu calls the things in the tree: the sidebar calls them folders, while the horizontal strip calls them groups,
// as Chromium does.
enum class Terms {
    Folder,
    Group,
};

// How a folder's named color looks on the horizontal strip, in a light or a dark window. A folder without a color of its
// own takes one from its id, so that groups side by side tell apart, as Chromium gives each new group the next color.
QString effective_folder_color(TabFolderTree::Folder const&);
QColor folder_color(QString const& name, bool dark);
// What reads on a chip of that color.
QColor folder_color_text(QColor const& fill);

Action exec_folder(QWidget* parent, QPoint const& global_pos);

// The parent menu owns the new submenu. For a persistent parent menu, remove and delete the submenu after the popup.
void add_move_menu(QMenu&, TabFolderTree const&, Terms, std::function<void(TabFolderId)> move_tabs, std::function<void()> create_folder);

}
