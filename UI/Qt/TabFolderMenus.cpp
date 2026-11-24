/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/TabFolderMenus.h>

#include <UI/Qt/StringUtils.h>

#include <QHash>
#include <QMenu>
#include <QPointer>

namespace Ladybird::TabFolderMenus {

Action exec_folder(QWidget* parent, QPoint const& global_pos)
{
    QPointer menu = new QMenu(parent);
    auto add_action = [&](QString const& label, Action action) {
        menu->addAction(label)->setData(static_cast<int>(action));
    };
    add_action(QStringLiteral("Rename Folder…"), Action::Rename);
    add_action(QStringLiteral("Change Icon…"), Action::ChangeIcon);
    menu->addSeparator();
    add_action(QStringLiteral("New Subfolder…"), Action::NewSubfolder);
    menu->addSeparator();
    add_action(QStringLiteral("Unpack Folder"), Action::Unpack);
    add_action(QStringLiteral("Delete Folder"), Action::Delete);

    auto* selected = menu->exec(global_pos);
    if (!menu)
        return Action::None;
    auto action = selected ? static_cast<Action>(selected->data().toInt()) : Action::None;
    delete menu.data();
    return action;
}

void add_move_menu(QMenu& menu, TabFolderTree const& tree, std::function<void(TabFolderId)> move_tabs, std::function<void()> create_folder)
{
    auto* submenu = menu.addMenu(QStringLiteral("Move to Folder"));
    auto const new_folder_label = QStringLiteral("New Folder…");

    auto folders = tree.folders_in_order();
    Vector<QString> paths;
    Vector<QString> ancestor_paths;
    QHash<QString, size_t> path_counts;
    path_counts.insert(new_folder_label, 1);

    for (auto const& entry : folders) {
        ancestor_paths.resize(entry.depth);
        auto path = qstring_from_ak_string(tree.folder(entry.id)->name);
        if (path.isEmpty())
            path = QStringLiteral("Untitled Folder");
        if (!ancestor_paths.is_empty())
            path = ancestor_paths.last() + QStringLiteral(" / ") + path;
        ancestor_paths.append(path);
        paths.append(path);
        ++path_counts[path];
    }

    for (size_t i = 0; i < folders.size(); ++i) {
        auto id = folders[i].id;
        auto label = paths[i];
        // Full paths distinguish names under different parents; also distinguish identical rendered paths.
        if (path_counts.value(label) > 1) {
            auto suffix = QStringLiteral(" (%1)").arg(id);
            do {
                label += suffix;
            } while (path_counts.contains(label));
            path_counts.insert(label, 1);
        }
        // Ampersands in user names are literal, not Qt mnemonic markers.
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        submenu->addAction(label)->setData(static_cast<qlonglong>(id));
    }

    QObject::connect(submenu, &QMenu::triggered, submenu, [move_tabs = std::move(move_tabs)](QAction const* action) {
        auto destination = action->data();
        if (destination.isValid())
            move_tabs(destination.toLongLong());
    });

    if (!folders.is_empty())
        submenu->addSeparator();
    submenu->addAction(new_folder_label, std::move(create_folder))->setEnabled(tree.can_create_folder());
}

}
