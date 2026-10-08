# WRITER 2027 — UI Architecture Audit (Phase 1, step 1)

Audit of the **current local LibreOffice/core fork** UI-mode architecture,
traced end to end. No guesses: every path/symbol below was read from the
working tree at `D:\_SATIN_AI\LibreOffice`.

---

## 1. End-to-end flow (current, verified)

```text
User Interface dialog (cui)                     uipickerdlg.cxx / uitabpage.cxx / uimode.hrc
    ↓  radio list from hardcoded UIMODES_ARRAY
Apply / OK
    ↓  dispatch ".uno:ToolbarMode?Mode:string=<CommandArg>"   (uipickerdlg.cxx OnApplyClick)
    ↓  or "Apply to all": sets ActiveWriter/Calc/Impress/Draw  (same file)
sfx2 MiscExec_Impl SID_TOOLBAR_MODE             sfx2/source/appl/appserv.cxx:966
    ↓  reads <App>/Active, saves new CommandArg as Active, iterates frames
    ↓  per mode node: mandatory Toolbars, UserToolbars, Sidebar mode
    ↓  SID_NOTEBOOKBAR execute → SfxNotebookBar
SfxNotebookBar::ExecMethod / StateMethod        sfx2/source/notebookbar/SfxNotebookBar.cxx
    ↓  lcl_getNotebookbarFileName(eApp) = ActiveWriter/… (the CommandArg)
    ↓  sNewFile = rUIFile + ActiveWriter   (rUIFile = "modules/swriter/ui/")
SystemWindow::SetNotebookBar → NotebookBar      vcl/source/window/syswin.cxx:908
    ↓  loads <module>/ui/<file>.ui from soffice.cfg
window state / menubar / sidebar state          HasMenubar / Sidebar props of mode node
```

## 2. Concern → exact local file → symbol/config node → role

| Concern | Exact local file | Exact symbol / config node | Role |
|---|---|---|---|
| ToolbarMode schema | `officecfg/registry/schema/org/openoffice/Office/UI/ToolbarMode.xcs` | `ModeEntry` group (`Label`, `CommandArg`, `MenuPosition`, `IsExperimental`, `HasNotebookbar`, `Toolbars`, `UserToolbars`, `UIItemProperties`, `Sidebar`, `HasMenubar`); `Application` group (`Active`, `Modes` set); component props `ActiveWriter/Calc/Impress/Draw` | Defines every mode field |
| ToolbarMode data | `officecfg/registry/data/org/openoffice/Office/UI/ToolbarMode.xcu` | `Applications/Writer/Modes/{Default,Single,Sidebar,Tabbed,TabbedCompact,GroupedbarCompact,GroupedbarFull,ContextualSingle,ContextualGroups}`; `Applications/Writer/Active`; top-level `ActiveWriter` | Per-app mode registry + active state |
| ToolbarMode menu (hamburger) | `framework/source/uielement/toolbarmodemenucontroller.cxx` | `ToolbarModeMenuController`; reads `…/Applications/<App>/Modes`, dispatches `Mode=<CommandArg>` | Dynamic mode list (config-driven) |
| ToolbarMode dispatch | `sfx2/source/appl/appserv.cxx` | `SfxApplication::MiscExec_Impl` `case SID_TOOLBAR_MODE` (line 966) | Applies mode: saves Active, toolbars, sidebar, SID_NOTEBOOKBAR |
| Notebookbar runtime | `sfx2/source/notebookbar/SfxNotebookBar.cxx` | `SfxNotebookBar::IsActive/ExecMethod/StateMethod`, `lcl_getNotebookbarFileName`, `lcl_getCurrentImplConfigNode` | Loads module UI resource per active mode |
| Notebookbar widget | `vcl/source/control/notebookbar.cxx` | `NotebookBar` | Renders the .ui composition |
| SystemWindow wiring | `vcl/source/window/syswin.cxx` | `SystemWindow::SetNotebookBar` (line 908) | Creates NotebookBar from UI path |
| UI picker dialog | `cui/source/dialogs/uipickerdlg.cxx` | `UIPickerDialog::OnApplyClick`; Apply-to-all sets Active*; Apply dispatches `.uno:ToolbarMode` | User Interface dialog |
| UI picker tab page | `cui/source/dialogs/uitabpage.cxx` | `UITabPage` (radio buttons `rbButton1..9`, `GetCurrentMode`, `GetSelectedMode`, `UpdateImage`) | Lists modes with previews |
| UI picker mode array | `cui/inc/uimode.hrc` | `UIMODES_ARRAY[]` (description, mode name, preview file) | Hardcoded radio list |
| UI picker resources | `cui/uiconfig/ui/uitabpage.ui` / `uipickerdialog.ui` | `rbButton1..9`, `imImage`, `lbInfo` | Dialog layout |
| ToolbarMode preview images | `extras/source/toolbarmode/*.png|svg` | `default/single/sidebar/notebookbar*/…` | Selector previews |
| ToolbarMode image packaging | `vcl/Package_toolbarmode.mk` | `toolbarmode_images` → `$(LIBO_SHARE_FOLDER)/toolbarmode` | Installs previews |
| Writer UI resource packaging | `sw/UIConfig_swriter.mk` | `sw/uiconfig/swriter/ui/notebookbar*` entries | Installs notebookbar .ui into `soffice.cfg/modules/swriter/ui/` |
| Writer UI base path at startup | `sfx2/source/view/viewfrm.cxx` | `SfxNotebookBar::ReloadNotebookBar(u"modules/swriter/ui/")` (line 1625) | Base path for notebookbar file |

## 3. Key facts established

- A UI mode is identified by its **`CommandArg`** (single word / filename). The
  **active mode = `<App>/Active`** and the **notebookbar file = the CommandArg
  appended to `modules/swriter/ui/`**.
- `SfxNotebookBar::IsActive()` returns `HasNotebookbar` of the mode whose
  `CommandArg == Active`; modes without a matching node → notebookbar hidden.
- The hamburger **menu is config-driven** (reads `Modes` nodes); the **dialog
  list is hardcoded** (`UIMODES_ARRAY`).
- "Apply to all" in the dialog writes the selected `CommandArg` into
  `ActiveWriter/Calc/Impress/Draw` unconditionally — **no per-app
  availability check today** (relevant for a Writer-only mode).
- The current Writer 2027 Home lives **inside the default Tabbed resource**
  (`sw/uiconfig/swriter/ui/notebookbar.ui`), i.e. it is an island: switching
  UI mode can hide it, and the normal Tabbed UI is no longer the upstream one.
- No second preference store exists; persistence is the canonical
  `org.openoffice.Office.UI.ToolbarMode` config via `ConfigurationChanges`.

## 4. Migration inventory (step 2) — summary

The working-tree `notebookbar.ui` (vs `HEAD`) adds the Writer 2027
composition directly into the Tabbed resource:

| Change | Class |
|---|---|
| File menu `.uno:Writer2027PublishWeb` item | B command wiring |
| Home Typography section `bxHomeTypeSystem` `.uno:Writer2027TypeSystem` | A reusable Writer 2027 composition |
| Insert group `gdInsertWriter2027` (blocks, document kit, story) | A reusable Writer 2027 composition |
| View section: Story mode, canvas toggles | A reusable Writer 2027 composition |
| All `.uno:Writer2027*` bindings | B command wiring |

None of these are generic framework fixes (class C); they belong in a Writer
2027 profile resource, not in the default Tabbed UI.