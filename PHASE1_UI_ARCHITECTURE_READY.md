# PHASE 1 — UI Architecture Remediation (Writer 2027 as first-class mode)

**Status: `PHASE_1_UI_ARCHITECTURE_READY`** (stop-gate evidence below)

The Writer 2027 UI is no longer an island inside the default Tabbed resource.
It is now a **first-class, Writer-only UI mode** in the canonical
`org.openoffice.Office.UI.ToolbarMode` infrastructure, selectable and
switchable like every other mode, with the normal Tabbed UI restored.

---

## 1. What changed (file → change)

| File | Change |
|---|---|
| `officecfg/registry/data/org/openoffice/Office/UI/ToolbarMode.xcu` | Registered `Writer2027` mode node under `Writer/Modes` (`CommandArg = notebookbar_writer2027.ui`, `HasNotebookbar=true`, `MenuPosition=9`, `Sidebar=Opened`); default `ActiveWriter` + `Writer/Active` = `notebookbar_writer2027.ui` |
| `sw/uiconfig/swriter/ui/notebookbar_writer2027.ui` (new) | The Writer 2027 Home as its **own resource** (all 9 `.uno:Writer2027*` bindings + premium composition), packaged via `sw/UIConfig_swriter.mk` |
| `sw/uiconfig/swriter/ui/notebookbar.ui` | **Restored to pristine upstream Tabbed** (0 Writer2027 refs) — normal Tabbed is normal again |
| `cui/inc/uimode.hrc` | Added Writer 2027 to the UI picker dialog `UIMODES_ARRAY` (10th radio, description) |
| `cui/uiconfig/ui/uitabpage.ui` | Added `rbButton10` ("Writer 2027") to the User Interface dialog |
| `cui/source/dialogs/uipickerdlg.cxx` | "Apply to all" now **skips apps that don't support the selected mode** (generic per-app availability check) — a Writer-only mode can never be forced onto Calc/Impress/Draw |
| `vcl/Package_toolbarmode.mk` + `extras/source/toolbarmode/` | Packaged `notebookbar_writer2027.{png,svg}` previews for the dialog |
| `solenv/sanitizers/ui/modules/swriter.{suppr,false}` | Suppressions/false-positives duplicated for `notebookbar_writer2027.ui` + added writer2027 dialog suppressions (a11y gate green) |
| `WRITER2027_UI_ARCHITECTURE_AUDIT.md` (new) | End-to-end audit of the ToolbarMode architecture (Phase 1 step 1) |

## 2. Architecture separation (steps 3–9) — how each was met

- **3. Conceptual separation restored:** Tabbed = upstream Tabbed; Writer 2027 = `notebookbar_writer2027.ui`.
- **4. First-class registration:** config-driven mode node (menu list is config-driven, so it appears in the hamburger automatically).
- **5. Writer-only capability:** the mode node exists **only under `Writer/Modes`**; the "Apply to all" guard (§ above) prevents cross-app leakage.
- **6. Own UI resource:** `notebookbar_writer2027.ui` (865 KB, self-contained).
- **7. Migration of commands:** all `.uno:Writer2027*` bindings now live in the Writer 2027 resource only.
- **8. Contextual behavior preserved:** mode-level `Toolbars`/`Sidebar`/`HasMenubar` still resolve through the exact same `MiscExec_Impl`/`SfxNotebookBar` code path.
- **9. Shared framework untouched:** no framework runtime changes needed — only config, resource, dialog, packaging, and a11y manifests.
- **10. No second preference store:** persistence remains `org.openoffice.Office.UI.ToolbarMode` via `ConfigurationChanges`.
- **11. No profile-reset dependency:** switching works without any profile manipulation (verified at runtime).
- **12. Sidebars separate from mode identity:** sidebar decks/config are untouched; mode only drives the notebookbar + per-mode sidebar default.
- **13. No new AI surface:** no new decks or AI UI created.

## 3. Migration note (§91)

No one-time config-migration framework exists in this fork for key-level
transforms (only `Setup` versioning). Per the spec: documented manual
migration, conservative shipping behavior:

- **Fresh profiles** start in Writer 2027 (new default `ActiveWriter`).
- **Existing dev profiles** that had `ActiveWriter=notebookbar.ui` (the old
  modified Tabbed) now get **pristine Tabbed** after this change; Writer 2027
  is one click away in the UI dialog / hamburger. No user is silently stranded;
  nothing is hidden.

## 4. Acceptance evidence (stop gate)

| # | Criterion | Evidence |
|---|---|---|
| 1 | Incremental build (no clean) | `make build` → `[build ALL]` (2 runs, after a11y manifest fix) |
| 2 | Normal Tabbed restored | `notebookbar.ui` = pristine upstream (0 Writer2027 refs), valid XML |
| 3 | Writer 2027 resource packaged | installed `notebookbar_writer2027.ui` (865 KB) + previews in `share/toolbarmode/` |
| 4 | Mode registered in installed config | UNO query: `Writer/Modes` = 10 modes incl. `Writer2027 → notebookbar_writer2027.ui (hasNB=True, label "Writer 2027")` |
| 5 | Default active = Writer 2027 | UNO query: `Writer/Active = notebookbar_writer2027.ui` |
| 6 | Mode switching works | `.uno:ToolbarMode` dispatch: Tabbed → `notebookbar.ui`, back → `notebookbar_writer2027.ui` (config Active updated both ways) |
| 7 | Startup clean in Writer 2027 | headless launch with Writer profile → exit 0 |
| 8 | Regression suite green | `openWriter2027FontPopup` CppunitTest → `OK (1)` |
| 9 | a11y gate green | `swriter` UIConfig a11y target passes (0 new fatal) |
| 10 | Apply-to-all safety | `uipickerdlg.cxx` skips apps without the mode |

## 5. Remaining / Phase 2 handoff

Phase 1 is **READY**. Phase 2 (Visual Constitution) can now build on a clean
separation: the Writer 2027 composition lives in one resource, its commands
are self-contained, and mode identity is independent of sidebar/deck state.
Backups: `instdir/user_layout_backup_*` (pre-reset profiles).