# WRITER 2027 UI PROFILE ARCHITECTURE

Status: Phase 1 complete + verified; Phase 2 engineering complete. This document is the architecture artifact (§89) for the Writer 2027 UI profile.

---

## 1. ToolbarMode integration

Writer 2027 is a first-class ToolbarMode, not a modification of the default Tabbed mode.

- Schema/data: `officecfg/registry/data/org/openoffice/Office/UI/ToolbarMode.xcu`
- Mode node: `Writer2027`
  - `Label` = "Writer 2027"
  - `CommandArg` = `notebookbar_writer2027.ui`
  - `HasNotebookbar` = `true`
  - `IsExperimental` = `false`
  - `MenuPosition` = `9`
  - `Sidebar` = `Opened`
- Active defaults: top-level `ActiveWriter` = `notebookbar_writer2027.ui`; `Applications/Writer/Active` = `notebookbar_writer2027.ui`
- Other applications keep `notebookbar.ui` — Writer 2027 is Writer-only.

## 2. Profile id / identity

- **Profile id (mode key)**: `Writer2027`
- **UI resource**: `sw/uiconfig/swriter/ui/notebookbar_writer2027.ui` (own resource; superset copy of the old Writer-2027-modified notebookbar)
- **Display name**: "Writer 2027" (shown in the UI picker and hamburger toolbar-mode menu)

## 3. Resource mapping

| Resource | Path |
|---|---|
| Notebookbar | `sw/uiconfig/swriter/ui/notebookbar_writer2027.ui` |
| Shared popup menu | reuses the notebookbar popupmenu |
| Toolbar-mode preview | `extras/source/toolbarmode/notebookbar_writer2027.{svg,png}` |
| Mode registration | `officecfg/.../ToolbarMode.xcu` |
| UI picker entry | `cui/inc/uimode.hrc` (UIMODES_ARRAY, 10th) + `cui/uiconfig/ui/uitabpage.ui` (`rbButton10`) |
| Packaging | `sw/UIConfig_swriter.mk`, `vcl/Package_toolbarmode.mk` |
| a11y suppressions | `solenv/sanitizers/ui/modules/swriter.suppr` (writer2027 refs), `swriter.false` |

## 4. Application support

Writer-only mode. The "Apply to all" action in the UI picker dialog is gated per application by `lcl_appSupportsMode()` (`cui/source/dialogs/uipickerdlg.cxx`) — a mode is applied to another app only if that app's ToolbarMode Modes list exposes the same `CommandArg`. Writer 2027 is never forced onto Calc/Impress/Draw.

## 5. Persistence

Mode selection persists through the standard ToolbarMode configuration APIs:

- Active mode: `org.openoffice.Office.UI.ToolbarMode/ActiveWriter` + `Applications/Writer/Active`
- Written by the ToolbarMode dispatch handler (`.uno:ToolbarMode?Mode=...`) and the UI picker apply path
- No direct `registrymodifications.xcu` editing in product code (§92); dev repair scripts operate on a disposable dev profile only

## 6. Switching

- Dispatch: `.uno:ToolbarMode?Mode:string=notebookbar_writer2027.ui` → `SfxApplication::MiscExec_Impl` `case SID_TOOLBAR_MODE` → `SfxNotebookBar` reload
- Verified at runtime: Tabbed ↔ Writer2027 both directions; Writer 2027 fully preserved after switch-back (§93)
- Hamburger menu: config-driven via `ToolbarMode.xcu` mode entries
- UI picker dialog: hardcoded `UIMODES_ARRAY` (`cui/inc/uimode.hrc`) — 10 modes including Writer2027, radio button `rbButton10` in `uitabpage.ui`

## 7. Shared framework extensions

Shared, application-consumable capabilities introduced in Phase 2 (spec §79 — generic, no Writer2027 baked into generic names):

| Extension | Location | Other consumers |
|---|---|---|
| `AdaptivePopoverGeometry` (width/height clamp vs work area, density) | `svx/inc/svx/writer2027visual.hxx` | any app's custom popovers |
| `PanelWidth` classes | same | sidebar deck width policy |
| `WorkspaceCompositionPolicy` (golden-ratio stage/aux split) | same | any wide-layout composition |
| Density enum (Compact/Standard/Studio) | same | adaptive density framework |
| Apply-to-all per-app mode support | `cui/.../uipickerdlg.cxx` `lcl_appSupportsMode` | any app with mode-specific profiles |

## 8. Writer-specific composition

Stays in the Writer 2027 profile / Writer code (spec §80):

- Typography group arrangement (`notebookbar_writer2027.ui`)
- Type System control + popup (`sw` slot `FN_WRITER2027_TYPE_SYSTEM`, `svx` popup)
- Story button placement (`FN_WRITER2027_STORY`)
- Writer page-stage semantics (`sw/inc/writer2027view.hxx`, canvas border/gap, authoring zoom)
- Style gallery order (Home → Styles section)

## 9. Upgrade / migration from modified Tabbed (spec §90-91)

- The old Writer-2027-modified `notebookbar.ui` was restored to pristine upstream (`5fc1550a1f31^`); all Writer 2027-only composition moved to `notebookbar_writer2027.ui`.
- No ad-hoc profile migration code was added (spec §91): there is no clean existing configuration migration framework for per-mode UI state in this fork. Migration is **manual and documented**: `reset_writer_layout.bat` clears stale Writer UI state (WriterWindowState / ToolbarMode / UseVerticalNotebookbar) from the disposable dev profile; a fresh default profile selects Writer 2027 because `ActiveWriter` defaults to it.
- Shipping behavior stays conservative: existing profiles keep their chosen mode until the user switches.

## 10. Phase 1 evidence recap

- Runtime: UNO query confirmed `Writer/Active = notebookbar_writer2027.ui`, 10 modes incl. Writer2027, `.uno:ToolbarMode` switching verified.
- See `PHASE1_UI_ARCHITECTURE_READY.md` for the full stop-gate evidence table.
- Phase 2 engineering builds green (`[build ALL]`); regression test `openWriter2027FontPopup` passes (`OK`).