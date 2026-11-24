# Zen-style tab folders in the Ladybird fork

## Goal

Make the Linux vertical tab sidebar work like a folder tree: persistent,
named folders contain live tabs and other folders. This is not a separate
Chrome-style color grouping feature.

## Approach

Implement the interaction in Ladybird's Qt tab bar, using its existing tab
objects and session store. Use Zen's public MPL-2.0 source, styles, tests, and
the provided screenshots as visual and behavioral references; recreate the
design with native Qt painting and original icons rather than copying code
or assets. A sidebar-only overlay would be faster
to prototype, but would not reliably restore nested folders or preserve
membership as tabs move. Porting Zen's code would couple the design to Firefox
internals that Ladybird does not have.

## Sidebar behavior

- Match Zen's compact folder rows and nested indentation, folder icon
  silhouettes, active-tab preview, and subtle expand/collapse motion.
  Use Ladybird's theme colors, fonts, and scaling so the result feels native.
- Create empty folders from the sidebar, or create a folder around the
  selected tabs. Rename folders from their context menu.
- Folder context menu: **Rename Folder**, **Change Icon**, **New Subfolder**,
  **Unpack Folder**, and **Delete Folder**. **Change Icon** opens a compact
  grid of monochrome symbols with a **Default** option. Use a reusable icon
  catalog with stable IDs so Spaces can use it later. Persist each folder's
  icon choice; a custom icon still needs a visible active-descendant cue.
- Indent child tabs and folders. Expand and collapse with a short animation.
  Show distinct folder icons for open, closed, and closed with the active tab
  inside (including inside a nested descendant). Collapsing does not close
  tabs or switch away from the active page. A collapsed folder shows only
  the active descendant tab beneath its header. If the user activates a tab
  outside the folder, hide this row and return to the normal closed icon
  immediately; the folder stays collapsed throughout. The next active tab
  determines the preview, not a remembered selection. With multiple
  collapsed ancestors, show the preview once under the outermost one.
- Ctrl-click toggles selection; Shift-click selects a range of visible tabs.
  Right-clicking a selected tab acts on the whole selection. **Move to
  Folder** lists existing folder destinations and **New Folder** for both
  individual tabs and multi-selection.
- Drag tabs, selected tabs, and folders within the sidebar. A landing line
  shows the exact insertion position and depth; distinguish dropping into a
  folder from dropping before or after it. Reject dropping a folder into
  itself or a descendant. Keep the relative order of tabs moved together.
- **Unpack Folder** removes only the folder and promotes its direct contents
  to its parent at the same position. **Delete Folder** closes every tab in
  its tree and removes all descendant folders. Make the destructive action
  distinct from unpacking and confirm when tabs will close.
- Folder membership does not automatically pin or unpin a tab. Keep existing
  tab pinning controls independent.

## Persistence and verification

- Persist folder identity, name, icon choice, parent, sibling order, collapsed
  state, and membership alongside window and tab session data. Full session restore
  brings back the hierarchy, empty folders, and active tab. An existing profile
  with no folder metadata restores as before. Do not persist private-window data.
- Verify session round-trips and compatibility with older sessions. Test
  nested moves, multi-tab ordering, cycle rejection, unpack versus delete,
  and switching away from an active tab preview in a collapsed folder.
  Smoke-test drag feedback and icons in a
  separate profile before trying the build with everyday tabs.
- Out of scope: **Unload All Tabs**, **Change Space**, **Convert Folder to
  Space**, Live Folders, sync, and new horizontal-tab controls. Do not show
  placeholder menu actions for these.
