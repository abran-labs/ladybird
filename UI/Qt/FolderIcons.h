/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <UI/Qt/TabFolderTree.h>

#include <QColor>
#include <QIcon>
#include <QRect>
#include <QString>
#include <optional>

class QWidget;

namespace Ladybird::FolderIcons {

// An empty or unrecognized ID uses the default folder glyph.
QIcon icon(QString const& id, TabFolderTree::FolderGlyph state, QColor const& color);

// Anchor to the folder row in screen coordinates. An empty ID selects Default;
// no value leaves the choice unchanged.
std::optional<QString> choose(QWidget* parent, QString const& current, QRect const& anchor_global);

}
