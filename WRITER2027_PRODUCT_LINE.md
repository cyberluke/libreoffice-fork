# Writer 2027 — Product Line Implementation Status

**Scope:** the Reader-facing product experience built on top of the LibreOffice
engine. Writer 2027 is not a set of patches to old dialogs; it is a new Writer
**product experience** (typography, Type System, editorial blocks, digital
canvas, web publication, preflight) layered over the existing Writer engine.

**Design note.** Early phases were intentionally delivered "source-level,
minimal-surgical" so the ten backend slices could land without GUI automation.
That was correct for the engine; the frontend is undergoing a separate
"Writer 2027 visual constitution / 4K UX pass" (see *Open work*), because
lean-append into the legacy control set does not meet the 2027 product bar.

---

## 1. Product architecture (one model, one engine)

Writer 2027 deliberately ships **no second document model and no browser
runtime**. Everything—typography catalogs, Type System, blocks, web
export, preflight—consumes the **canonical Writer document structures**
(paragraph/character styles, headings, lists, tables, sections, images,
captions, links, bookmarks, footnotes) and the canonical dispatch path
(`.uno:CharFontName`, styles, etc.).

| Concern | Location | Role |
|---|---|---|
| Curated typography / font popup | `svx/source/tbxctrls/writer2027typography.cxx` + `writer2027fontpopup.cxx` | Typography Browser (Phase 2 / 2B) |
| Type System catalog | `svx/source/tbxctrls/writer2027typesystem.cxx` + `sw/source/core/doc/writer2027typesystem.cxx` | `TypographyCatalog` / `Writer2027TypeSystemCatalog` (Phase 6) |
| Document Kits + Editorial Blocks | `svx/source/tbxctrls/writer2027blocks.cxx` + `sw/source/uibase/wrtsh/writer2027blocks.cxx` | Composition systems (Phase 7) |
| Popover UIs | `writer2027typesystempopup.cxx`, `writer2027blockgallerypopup.cxx`, `writer2027documentkitpopup.cxx` | Custom-rendered weld popovers |
| Digital canvas presentation | `sw/source/core/view/writer2027view.cxx` | Workspace-around-page styling (Phase 4) |
| Web publication exporter | `sw/source/filter/writer2027web/writer2027web.cxx` (≈2900 L) | Semantic HTML+CSS export (Phase 8/9) |
| Preflight engine | `sw/source/filter/writer2027preflight/writer2027preflight.cxx` (≈1160 L) | Publication correctness gate (Phase 10) |
| Shared capabilities | `sw/inc/writer2027capabilities.hxx` | Single capability model for exporter+preflight |

**Design invariant (verbatim from the code):** *"preflight must never predict
exporter behavior with a divergent ruleset, and the exporter must never change
its output semantics while these helpers are extracted."* Both phases consume
`sw::writer2027capabilities` so they cannot drift apart.

---

## 2. Phase-by-phase status

### Phase 1 — Digital-first default document *(done, hardened)*
- Dark page / light text default document (V271 digital canvas).
- Page colors are *stored document* properties; light themes and high-contrast
  are respected (see Phase 4).
- Commit: `e8002ae76` "finalize Phase 1 digital-document defaults (hardening)".
- TODO removed from the closing summary; default document now ships clean.

### Phase 2 / 2B — Curated typography + premium **Typography Browser** *(done, crash fixed, UI redesigned)*
- **`TypographyCatalog`** — curated set of recommended families
  (`Inter`, `Atkinson Hyperlegible`, `OpenDyslexic`, `JetBrains Mono`,
  `Instrument Serif`, …) with category + mood metadata.
- **`FontPickerModel`** — grouped root view (Current / Recommended /
  Collections / Legacy), category drill-down, search view, legacy inventory,
  in-place navigation (`EnterLegacy`, `GoBack`, `EnterCategory`).
- **`Writer2027FontPopup`** — a `weld::Popover` with a custom-rendered
  `GtkTreeView` row list:
  - reserved preview column (glyph "Aa" in the candidate face),
  - family label line + metadata line (no text-over-text),
  - root grouped view + inline search (search is a *header*, top-aligned),
  - selection tint driven by model state (not the OS slab).
- **Deterministic regression test** added to
  `svx/qa/unit/svx-dialogs-test.cxx` (`openWriter2027FontPopup`): builds a
  real `FontList` from the default device, opens grouped + search views over
  the installed-font inventory, and reopens repeatedly. Suite: `OK (2)`.

**Crash fix (merged into this phase).** A hard access-violation on the
font-dropdown click was traced (cdb `CPPUNITTRACE` capture) to
`SvTreeListBox::SetEntryText` dereferencing a dangling `SvLBoxString` item.
Root cause: populating custom-rendered rows via the weld default
`set_text(rIter, text)` (col == −1). **Fix:** pass an explicit column
(`set_text(rIter, row.maText, 0)`), applied consistently to all four
custom-rendered popups (font, typesystem, document-kit, block-gallery).
Verified green with `CPPUNIT_TEST_NAME=openWriter2027FontPopup`.

**Geometry (phase 2B → 2027 bar).** The popover now self-sizes:
- width by work-area-proportional ratio in a golden-ratio-informed band
  (min ~420, ideal ~640, max ~720 logical px) — no longer a fixed 400 px strip;
- height capped to ~68 % of the work area (65–75 vh), never a full-screen column;
- font rows ≈ 60 logical px with a reserved preview column, so preview / name /
  metadata each get their own x-band.

### Phase 4 — Digital canvas presentation *(done)*
- `sw::writer2027view` styles the **workspace around the page** only; the page
  itself keeps its stored formatting. `IsWriter2027CanvasActive()` gates light
  themes / high-contrast.
- This is where the *golden-ratio workspace* is expected to compose the stage
  (~61.8 %) against auxiliary UI (~38.2 %) — see *Open work*.

### Phase 6 — Type System *(done)*
- **`TypeSystemPreset`** catalog: heading + body font pairing (e.g.
  "Editorial", "Modern · Neutral", "Accessible"), resolved per installed font.
- `ResolveTypeSystem` → `ResolvedTypeSystem` exposes heading/body/display roles.
- **`Writer2027TypeSystemPopup`** — custom-rendered popover; each row exposes
  resolved pairing (`Name — Heading + Body`) to assistive tech.
- Symbol export hardening: `SVXCORE_DLLPUBLIC` on the catalog + resolver so the
  test surface and Writer consumer link against svxcore.

### Phase 7 — Document Kits + Editorial Blocks *(done)*
- **Document Kit** = *composition* system: page geometry, section rhythm,
  recommended editorial blocks, a preferred (not locked) Type System.
- **Editorial Block** = reusable structural composition inserted into the real
  Writer document from canonical primitives only.
- `Writer2027BlockGalleryPopup` (≈670 L custom render) + `Writer2027DocumentKitPopup`
  provide the pickers; insertion path lives in `wrtsh/writer2027blocks.cxx`.

### Phase 8 / 9 — Web publication *(done)*
- `sw::writer2027web` exports the Writer model to a responsive, semantic
  web publication (HTML + CSS, `WebPublishFormat` variants).
- Writer remains the **authoring system** — no second model, no browser runtime.
- `.uno:Writer2027PublishWeb` is wired in the notebookbar UI.

### Phase 10 — Preflight *(done)*
- `sw::writer2027preflight` (≈1160 L): publication-correctness gate.
- **Shared capability model** (`writer2027capabilities.hxx`):
  - `SafeHref()` — URL scheme allow-list (relative, fragment, http/https/mailto/tel;
    rejects `javascript:`, `vbscript:`, `data:text/html`, …),
  - `MapGfx`/asset mapping — rejects formats that must not ship to a web page
    (TIFF, WMF/EMF, EPS, PDF, MOV, …).
- `publicationinspector.ui` + `SidebarDeck 'PublicationInspectorDeck'` surface
  issues/acceptance in the app.

### AI — OfficeLabs AI / WriterAgent *(bundled)*
- `b56b7902c` bundles WriterAgent as a first-class product component.
- Sidebar decks: `OfficeLabs AIDeck` (CEF `officelabs-ui` placeholder) and the
  Python `org.extension.writeragent` iso. Note: the two "AI" decks are
  consolidation work, not yet unified behind one canonical spec (see *Open work*).

---

## 3. Frontend / layout state

- The Writer 2027 Home is the **modified default `notebookbar.ui`** (source:
  `sw/uiconfig/swriter/ui/notebookbar.ui`; installed:
  `instdir/share/config/soffice.cfg/modules/swriter/ui/notebookbar.ui`), which
  wires `.uno:Writer2027PublishWeb`, `.uno:Writer2027TypeSystem`,
  `.uno:Writer2027InsertBlock`, `.uno:Writer2027DocumentKit`,
  `.uno:Writer2027Story`, typography, styles preview, and gallery.
- Build/runtime profile: `instdir/program/bootstrap.ini` → `UserInstallation=$ORIGIN/..`
  ⇒ profile lives at **`instdir/user`**.
- Layout reset: clear `Office.UI.WriterWindowState`, `Office.UI.ToolbarMode`,
  `UseVerticalNotebookbar` from `instdir/user/registrymodifications.xcu` and
  remove persisted toolbar/menubar overrides under `modules/swriter`.
  Reusable script: `reset_writer_layout.bat` (see *How the work is structured*).
  Re-show the sidebar with **View ▸ Sidebar / Ctrl+F5**.

---

## 4. Build & verification facts

- Runner: `cmd → vcvars64.bat (VS2022 MSVC 14.44) → msys bash → make -j16`.
- `make` is idempotent; `[build ALL]` confirms nothing is stale after a change.
- Writers 2027 backend ships in **`instdir/program/swlo.dll`** +
  **`svxcorelo.dll`** (the latter rebuilt on every svx popup change).
- Cppunit regression: `CppunitTest_svx_dialogs_test` +
  `CPPUNIT_TEST_NAME=openWriter2027FontPopup`;
  cdb crash capture via `CPPUNITTRACE` wrapper (`dbg.sh`).
- Diagnostics are temporary-only and were removed from the tree after each fix.

---

## 5. Open work / known gaps

1. **Writer 2027 visual constitution** (explicitly requested by product):
   a single spacing scale, control heights, typography, panel widths,
   golden-ratio regions (stage ≈ 61.8 %, auxiliary ≈ 38.2 %), density modes,
   4K behavior, popup geometry, cards/borders/elevation. Not yet written; each
   agent still invents UI locally.
2. **Header / Typography group** block still reads as the legacy "icon carpet":
   `[ Liberation Serif ▼ ] [12 pt ▼] A↑ A↓ x₂ x² …`. Desired: real product
   blocks (readable Type System control, reserved preview column, proper
   4K scale). Partially addressed only in the font picker so far.
3. **Template Studio + Master Layout** task (queued): first place the new
   visual language is non-negotiable — large cards, real thumbnails,
   archetypes, font-pairing metadata, Type System preview, whitespace,
   and 4K scaling. Propagate that visual language back into Writer.
4. **AI consolidation**: first-sidebar "AI" (CEF placeholder) vs last-deck
   "Office Labs AI" (Python iso) still split-brain; unify behind
   `v271/CHAT_AI_CANONICAL_SPEC.md`.
5. **Sidebar right edge** hosts `SwManageChangesDeck` / WriterAgentDeck /
   officelabs deck; deck management is not yet part of the golden-ratio
   workspace composition.
6. **Regression suite breadth**: only the font popup has an automated
   regression test; typesystem / document-kit / block-gallery popups share the
   same fixed code path but lack dedicated tests (recommended before the next
   UI pass).

---

## 6. Source map (quick reference)

**svx (shared UI + catalogs + popovers):**
- `writer2027typography.{hxx,cxx}` — curated font catalog + `FontPickerModel`.
- `writer2027fontpopup.{hxx,cxx}` — Typography Browser (`set_text(…,0)` fix).
- `writer2027typesystem.{hxx,cxx}` / `writer2027typesystempopup.*` — Type System.
- `writer2027blocks.{hxx,cxx}` — Document Kit + Editorial Block catalogs (data).
- `writer2027blockgallerypopup.*` / `writer2027documentkitpopup.*` — pickers.
- `svx/uiconfig/ui/writer2027*.ui` — popover layouts (font/type/blocks/kit).

**sw (engine / exporter / preflight / UI wiring):**
- `sw/inc/writer2027*.hxx` + `sw/source/…` — web export, preflight, capabilities,
  view (canvas), typesystem doc model, blocks insertion, publish dialog.
- `sw/uiconfig/swriter/ui/notebookbar.ui` — Writer 2027 Home (modified default).
- `sw/uiconfig/swriter/ui/writer2027publishdialog.ui` — publish dialog.

---

*Status snapshot: engine slices (Phases 1–10) are implemented and building;
the popup crash is fixed and regression-tested; the 4K visual pass and
Template Studio are the active frontend front. Backups of the pre-reset layout
profile: `instdir/user_layout_backup_20261003_131532`.*