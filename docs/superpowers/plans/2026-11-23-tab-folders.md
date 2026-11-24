# Vertical tab folders implementation plan

> For agentic workers: use subagent-driven-development or executing-plans. Start with the approved spec.

**Goal:** Build the nested vertical folder sidebar in the user's fork and verify the launchable build.

**Architecture:** Keep existing Qt tabs and page lifetimes authoritative. Represent folder ordering and membership separately from active-page selection. Connect the native vertical view to session restoration without changing the horizontal UI.

**Tech stack:** C++/Qt, Ladybird session store, Ninja.

**Spec:** `Documentation/ForkTabFolders.md`

## Global constraints

Keep membership independent of pinning. No new dependency, upstream PR, private-session persistence, Spaces, Live Folders, or horizontal-tab UI. Use original icons and Zen as design reference. Never test migration or destructive actions on the user's profile.

## Review focus

Nested cycle rejection; active preview after switching away; order of a noncontiguous selection; deletion versus non-destructive unpacking; empty folders and old sessions after restart.

### Task 1: Window folder state and persistence

- [ ] Add focused tests for hierarchy, stable siblings during batch moves, cycles, unpack/delete, and round-trip under session recovery. Verify failures before implementation.
- [ ] Implement per-window folder identity, ordering, icon, collapsed state, and tab membership. Extend `UI/Qt/SessionStore.{h,cpp}` and the restore path. Test against a disposable legacy profile and normal/private windows.

### Task 2: Native vertical view and actions

- [ ] Integrate in `UI/Qt/TabBar.{h,cpp}` / `BrowserWindow.cpp`: reconcile native tab lifecycle, current index and pin state with visible folder rows. Only one preview under outermost collapsed ancestor; no page switch on collapse.
- [ ] Add selection ranges, destination menu, sidebar add, rename, nested creation, unpack, confirmed recursive delete, folder icon chooser and three-state glyphs.
- [ ] Implement multi-item drag and drop, live landing line with depth, into-folder feedback, and short expand/collapse animation. Test reorder, nested preview and background tab selection.

### Task 3: Build and acceptance

- [ ] Run focused model/session/UI tests, then build `ladybird`; ensure associated shared libraries reach the launchable build. Exercise full browser with disposable profile: create, nest, multi-select, move, unpack/delete, restart and compare. Check the original horizontal tab strip and private session without modifying the real profile.
- [ ] Review against spec, fix findings, provide exact restart and feature entry points.
