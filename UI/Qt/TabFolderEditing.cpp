/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/StringUtils.h>
#include <UI/Qt/TabFolderEditing.h>

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QValidator>

namespace Ladybird::TabFolderEditing {

Optional<String> accepted_folder_name(StringView typed)
{
    auto name = typed.trim_whitespace();
    if (name.is_empty() || !TabFolderTree::is_valid_folder_name(name))
        return {};
    auto result = String::from_utf8(name);
    if (result.is_error())
        return {};
    return result.release_value();
}

Selection toggled(Selection selection, TabKey clicked, Optional<TabKey> active)
{
    if (selection.keys.is_empty() && !selection.anchor.has_value() && active.has_value() && *active != clicked)
        selection.keys.append(*active);
    if (!selection.keys.remove_all_matching([&](auto key) { return key == clicked; }))
        selection.keys.append(clicked);
    selection.anchor = clicked;
    return selection;
}

Selection extended(Selection selection, Vector<TabFolderSync::SidebarRow> const& rows, TabKey clicked, Optional<TabKey> active)
{
    auto anchor = selection.anchor.value_or(active.value_or(clicked));
    auto keys = TabFolderSync::tab_range(rows, anchor, clicked);
    if (keys.is_empty() && active.has_value()) {
        anchor = *active;
        keys = TabFolderSync::tab_range(rows, anchor, clicked);
    }
    if (keys.is_empty())
        return { { clicked }, clicked };
    return { move(keys), anchor };
}

Vector<TabKey> context_tabs(Selection const& selection, TabKey clicked, Vector<TabKey> const& window_order)
{
    if (!selection.keys.contains_slow(clicked))
        return { clicked };
    Vector<TabKey> keys;
    for (auto key : window_order) {
        if (selection.keys.contains_slow(key))
            keys.append(key);
    }
    return keys;
}

static void reveal_folder(TabFolderTree& tree, TabFolderId folder)
{
    for (auto parent = tree.parent_of(TabFolderTree::Item::folder(folder)); parent.has_value(); parent = tree.parent_of(TabFolderTree::Item::folder(*parent)))
        (void)tree.set_folder_collapsed(*parent, false);
}

TabFolderDestination FolderCreation::empty_folder_destination() const
{
    if (placement == Placement::FirstChild)
        return { parent, 0 };
    return TabFolderDestination::at_end_of(parent);
}

Optional<TabFolderId> FolderCreation::preview(TabFolderTree& tree) const
{
    auto destination = empty_folder_destination();
    if (!tabs.is_empty()) {
        for (auto key : tabs) {
            if (!tree.contains(TabFolderTree::Item::tab(key)))
                return {};
        }
        for (auto key : tree.tabs_in_order()) {
            if (tabs.contains_slow(key)) {
                destination = tree.position_of(TabFolderTree::Item::tab(key)).value();
                break;
            }
        }
    }
    auto id = tree.create_folder(MUST(String::from_utf8(NEW_FOLDER_NAME)), destination);
    if (id.has_value())
        reveal_folder(tree, *id);
    return id;
}

Optional<TabFolderId> FolderCreation::commit(TabFolderTree& tree, String name) const
{
    Vector<TabFolderTree::Item> items;
    for (auto key : tabs)
        items.append(TabFolderTree::Item::tab(key));
    auto id = items.is_empty()
        ? tree.create_folder(move(name), empty_folder_destination())
        : tree.create_folder_around(move(name), items);
    if (id.has_value())
        reveal_folder(tree, *id);
    return id;
}

namespace {

// Folder names are limited in UTF-8 bytes, which a line edit's own limit, in characters, doesn't see.
class FolderNameValidator final : public QValidator {
public:
    using QValidator::QValidator;

    virtual State validate(QString& input, int&) const override
    {
        return input.toUtf8().size() <= static_cast<qsizetype>(TabFolderTree::MAX_FOLDER_NAME_BYTES) ? Acceptable : Invalid;
    }
};

}

FolderNameEditor::FolderNameEditor(QWidget* parent, QString const& text, Done done)
    : QLineEdit(text, parent)
    , m_done(AK::move(done))
    , m_previous_focus(QApplication::focusWidget())
{
    setObjectName("LadybirdFolderNameEditor");
    setFrame(false);
    setMaxLength(TabFolderTree::MAX_FOLDER_NAME_BYTES);
    setValidator(new FolderNameValidator(this));
    setAccessibleName(QStringLiteral("Folder name"));
    selectAll();
}

void FolderNameEditor::cancel()
{
    finish({});
}

bool FolderNameEditor::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        event->accept();
        return true;
    }
    return QLineEdit::event(event);
}

void FolderNameEditor::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        event->accept();
        finish({});
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        event->accept();
        if (auto name = accepted_folder_name(ak_string_from_qstring(text())); name.has_value())
            finish(AK::move(name));
        return;
    default:
        QLineEdit::keyPressEvent(event);
    }
}

void FolderNameEditor::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);
    // A context menu or another popup opened over the field takes the focus only for a while.
    if (event->reason() == Qt::PopupFocusReason || event->reason() == Qt::ActiveWindowFocusReason)
        return;
    finish(accepted_folder_name(ak_string_from_qstring(text())));
}

void FolderNameEditor::finish(Optional<String> name)
{
    if (m_finished)
        return;
    m_finished = true;
    auto done = AK::move(m_done);
    // The field gives the focus back before it goes, so the page or field that had it keeps it.
    if (hasFocus() && m_previous_focus && m_previous_focus != this)
        m_previous_focus->setFocus(Qt::OtherFocusReason);
    hide();
    deleteLater();
    if (done)
        done(AK::move(name));
}

}
