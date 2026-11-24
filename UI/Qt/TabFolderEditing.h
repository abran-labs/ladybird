/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Function.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Vector.h>
#include <UI/Qt/TabFolderSync.h>

#include <QLineEdit>
#include <QPointer>

// What the sidebar's folder name editor and its tab selection decide, and the editor itself.
namespace Ladybird::TabFolderEditing {

using TabKey = TabFolderTree::TabKey;

// What a folder that's just been made is called until the user names it.
inline constexpr auto NEW_FOLDER_NAME = "New Folder"sv;

// The name a folder takes from what was typed: without the space around it. Nothing when that leaves no name, or a
// name longer than a folder may have, so the editor leaves things as they were.
Optional<String> accepted_folder_name(StringView typed);

// The tabs chosen in the sidebar, and the one a Shift-click counts its range from.
struct Selection {
    Vector<TabKey> keys;
    Optional<TabKey> anchor;

    bool operator==(Selection const&) const = default;
};

// A Ctrl-click on a tab: it leaves the selection if it was in it, and joins it otherwise. The first Ctrl-click on
// another tab also takes in the active tab, as the tab already chosen, the way a browser's tab strip does.
Selection toggled(Selection, TabKey clicked, Optional<TabKey> active);
Selection extended(Selection, Vector<TabFolderSync::SidebarRow> const&, TabKey clicked, Optional<TabKey> active);
Vector<TabKey> context_tabs(Selection const&, TabKey clicked);

struct FolderCreation {
    Optional<TabFolderId> parent;
    Vector<TabKey> tabs;

    // Adds a placeholder to a disposable copy; the tabs don't move until the real tree accepts the edit.
    Optional<TabFolderId> preview(TabFolderTree&) const;
    Optional<TabFolderId> commit(TabFolderTree&, String name) const;
};

// A folder's name, edited in place over its row in the sidebar. Return or leaving the field with a name accepts it;
// Escape, or a field left blank, gives up. Either way, the editor reports once, and then goes away.
class FolderNameEditor final : public QLineEdit {
public:
    using Done = Function<void(Optional<String>)>;

    FolderNameEditor(QWidget* parent, QString const& text, Done);

    void cancel();

private:
    virtual bool event(QEvent*) override;
    virtual void keyPressEvent(QKeyEvent*) override;
    virtual void focusOutEvent(QFocusEvent*) override;

    void finish(Optional<String>);

    Done m_done;
    bool m_finished { false };
    QPointer<QWidget> m_previous_focus;
};

}
