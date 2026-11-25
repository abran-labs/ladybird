/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/Color.h>
#include <UI/Qt/TabFolderMenus.h>

#include <UI/Qt/StringUtils.h>

#include <QColor>
#include <QHash>
#include <QMenu>
#include <QPointer>

namespace Ladybird::TabFolderMenus {

QString effective_folder_color(TabFolderTree::Folder const& folder)
{
    if (folder.color.has_value())
        return qstring_from_ak_string(*folder.color);
    auto const& colors = TabFolderTree::FOLDER_COLORS;
    auto index = static_cast<size_t>(folder.id < 0 ? -folder.id : folder.id) % colors.size();
    return qstring_from_ak_string(colors[index]);
}

QColor folder_color(QString const& name, bool dark)
{
    struct Entry {
        char const* name;
        QRgb light;
        QRgb dark;
    };
    static constexpr Entry entries[] {
        { "grey", 0x5F6368, 0xDADCE0 },
        { "blue", 0x1A73E8, 0x8AB4F8 },
        { "red", 0xD93025, 0xF28B82 },
        { "yellow", 0xE37400, 0xFDD663 },
        { "green", 0x1E8E3E, 0x81C995 },
        { "pink", 0xD01884, 0xFF8BCB },
        { "purple", 0x9334E6, 0xC58AF9 },
        { "cyan", 0x007B83, 0x78D9EC },
        { "orange", 0xE8710A, 0xFCAD70 },
    };
    for (auto const& entry : entries) {
        if (name == QLatin1String(entry.name))
            return QColor(dark ? entry.dark : entry.light);
    }
    return QColor(dark ? entries[0].dark : entries[0].light);
}

QColor folder_color_text(QColor const& fill)
{
    Gfx::Color background(fill.red(), fill.green(), fill.blue());
    return background.contrast_ratio(Gfx::Color::Black) >= background.contrast_ratio(Gfx::Color::White) ? Qt::black : Qt::white;
}

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

void add_move_menu(QMenu& menu, TabFolderTree const& tree, Terms terms, std::function<void(TabFolderId)> move_tabs, std::function<void()> create_folder)
{
    auto groups = terms == Terms::Group;
    auto* submenu = menu.addMenu(groups ? QStringLiteral("Move to Group") : QStringLiteral("Move to Folder"));
    auto const new_folder_label = groups ? QStringLiteral("New Group…") : QStringLiteral("New Folder…");
    auto const untitled_label = groups ? QStringLiteral("Untitled Group") : QStringLiteral("Untitled Folder");

    auto folders = tree.folders_in_order();
    Vector<QString> paths;
    Vector<QString> ancestor_paths;
    QHash<QString, size_t> path_counts;
    path_counts.insert(new_folder_label, 1);

    for (auto const& entry : folders) {
        ancestor_paths.resize(entry.depth);
        auto path = qstring_from_ak_string(tree.folder(entry.id)->name);
        if (path.isEmpty())
            path = untitled_label;
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
