# Writer 2027 Typography Workflow V3 Report

Branch: `lukas_dev` (pushed to `cyberluke/lukas_dev`)
Date: 2026-10-10
Spec: `writer2027-typography-typesystem-styles-workflow-v3-junior-spec.md`
Previous reviewed head: `00d42dfa932f99a875040fda4255db450a53aa9f`

## Commit

Full V3 changeset (will be one commit on top of `00d42dfa932f`).

## Branch head

TBD after commit + push.

## Font Browser root causes (Phase A)

1. **Specimen rect missing `nY`** (spec 4/71). `RebuildLayout()` built
   `maSpecimenRect` with `nPadV + nInnerPad` (no `nY`), so specimens for rows
   1+ were pinned near the top of the whole content surface; the hard row clip
   hid them — "only the first row gets an Aa". Fixed: every row sub-rectangle
   (specimen/title/meta) now uses `nY + ...` in content coordinates; paint
   subtracts `mnScrollOffsetPx` exactly once.
2. **Current-family selection erased on rebuild** (spec 5/49.2).
   `RebuildLayout()` reset `mnSelectedIndex = -1` after `SetCurrentFamily()`,
   so search/category/navigation rebuilds dropped the active highlight and
   keyboard starting state. Fixed: layout preserves selection by **stable
   family name** (`GetSelectedStableId` / `RestoreSelectionByStableId`), not by
   integer index; falls back to highlighting the current family if visible.
3. **Metadata contrast not selection/hover-aware** (spec 6/49.3/70). The same
   muted `aMuted` color was used on normal, hover AND selected backgrounds.
   Added reusable `Writer2027EnsureTextContrast(preferred, bg, fallback,
   ratio)` using real WCAG relative luminance; font-row title/`Aa`/meta now pick
   ≥4.5:1 foregrounds against the actual row background (normal/hover/selected).
4. **Scrollbar visual weight** (spec 49.5). Replaced the broad permanent track
   with a thin 4-6dp inset thumb, shown only when scrollable.

## Type System root causes (Phase B/C)

1. **Real FontList resolution** (spec 7/8/9). Previously the popup model, Apply
   and detection all used `ResolveTypeSystem(..., nullptr)`, which silently
   "accepts the requested family name" and never checked installed fonts — so
   previews/fallbacks/`missingCount` could be wrong and Apply stored unverified
   names. Added `ResolveWriter2027TypographyContext(frame)` (frame →
   SwDocShell → SwDoc + the canonical `SID_ATTR_CHAR_FONTLIST` FontList).
   `ApplyPreset` and popup detection now use the real FontList, and Apply
   **fails closed** (logs and returns) when the FontList is unavailable — it
   never applies unverified families.
2. **Same model for preview and Apply** (spec 30/31). The popup now receives
   the real FontList via `SetFontList()` before `SetCurrentPreset()`, so its
   `BuildModel()` resolves exactly the families Apply will store. The popup no
   longer independently `ResolveTypeSystem(..., nullptr)`.
3. **Preview sample text in plain labels** (spec 10/11/52/53). Replaced the
   `GtkLabel` previews with a `Writer2027TypeSystemPreview` drawing surface
   that draws Heading/Body/Code sample text in their **resolved role fonts**
   (from the real FontList), fixed samples (no document text), hierarchy
   clamped sizes, and per-section clips.
4. **Preset list was a backend TreeView with no custom mouse model**
   (spec 12/13/14). Replaced the `weld::TreeView` with a dedicated
   `Writer2027TypeSystemPresetList` drawing surface: 48lp rows, hover,
   selection (restrained accent + left rail), a "Current" dot marker, mouse
   hit-testing, keyboard Up/Down/Home/End/Enter/Escape, and one
   `selectedPresetId` shared by mouse and keyboard. Click previews; Apply (or
   Enter) applies — it never mutates on open/hover/click.
5. **Cramped layout** (spec 15/16). Widened the picker (target ~860lp width,
   right preview ~460lp), with explicit section spacing.

## Styles gallery root causes (Phase E)

1. **Replaced generic `.uno:StylesPreview` host** (spec 32/33/57). The Home
   notebookbar button (was `.uno:StylesPreview`) is now
   `.uno:Writer2027StyleGallery` hosted by a dedicated frame-bound
   `Writer2027StyleGalleryToolBoxControl` + `Writer2027StyleGallery` custom
   horizontal card drawing surface.
2. **Cards render from the actual document styles** (spec 35/55/56), not the
   preset: `BuildStyleGalleryModel` iterates the canonical semantic descriptor
   map and reads each pool style's real family/size/weight, so manual edits
   show immediately and the Type System → gallery refresh is coherent.
3. **Semantic card set** (spec 34): Body, Heading 1-6 (1-3 primary + 4-6
   available), Title, Subtitle, Quote, Caption, Code — not hundreds of generic
   styles.
4. **Mouse/keyboard/overflow** (spec 36/37/59): hover/click/Enter, Left/Right
   keyboard with auto-scroll, Home/End, horizontal wheel scroll, current
   paragraph style highlighted, no hidden wrapping.
5. **Apply dispatch** (spec 38): card activation dispatches `.uno:StyleApply`
   with `Template` = programmatic style name and `Family` = Para on the owning
   frame (never document-global Current()).

## Architecture implemented

```
SW layer
    writer2027typographymanager.hxx/.cxx
        DocumentTypographyContext (frame -> SwDocShell -> SwDoc + FontList)
        Writer2027SemanticStyle / Writer2027ScaleSlot descriptors (canonical map)
        EnsureSemanticStylesMaterialized (GetTextCollFromPool / GetCharFormatFromPool)
    writer2027typesystem.cxx          ApplyTypeSystem / DetectCurrentTypeSystem
                                      via the canonical semantic map, H1-H6, Text Body,
                                      full contract (spec 19/23/44/46)
    writer2027stylegallery.hxx/.cxx   custom horizontal card drawing surface + model build
    writer2027stylegalleryctrl.cxx    frame-bound ToolbarController (.uno:Writer2027StyleGallery)
    writer2027toolboxctrl.cxx         Type System controller now uses real FontList

SVX layer
    writer2027contrast.hxx            WCAG relative luminance + EnsureTextContrast
    writer2027typesystempresetlist    custom preset list drawing surface
    writer2027typesystempreview       font-rendered preview drawing surface
    writer2027typographylist.cxx      font browser geometry/selection/contrast fixes

Controller adapts SW document model -> SVX visual model (single source of truth).
```

## Semantic style map

One canonical table (`GetWriter2027SemanticStyleDescriptors`, sw
`writer2027typographymanager`) used by apply, detection, the gallery, preview
labels and tests:

- DefaultParagraphStyle/Body/Heading1-6/Title/Subtitle/Quote/Caption/CodeBlock/
  InlineCode each map to a pool id, font role (Body/Heading/Mono/Display) and a
  scale slot (spec 22). Apply and detect iterate the same table (spec 23/46),
  so a manual Heading 1 edit is detected as Custom and the gallery card shows
  the real edited family (spec 56/76).

## Real FontList integration

`ResolveWriter2027TypographyContext` acquires the canonical
`SID_ATTR_CHAR_FONTLIST` `SvxFontListItem` from the owning `SwDocShell` (the
same path `view0.cxx` uses; spec 8). Type System preview, Apply and detection
all use that one FontList and one resolved model (spec 31). Null FontList ⇒
Apply fails closed (spec 9). The SAP 72 → "72" aliases and fallback chains
(`Canela -> Instrument Serif`, etc.) resolve through the real enumeration, and
the popup shows honest "requested -> resolved" fallback text (spec 17/54).

## Existing-document behavior

Apply mutates semantic pool style definitions only (via the canonical map):
existing paragraphs using those styles update; custom styles derived from them
inherit; explicit direct formatting is untouched (spec 24/48). One Apply is one
undoable operation (spec 44/45). New documents stay "Custom typography" until a
preset is applied (spec 25). Detection compares the full managed contract, so a
manual style edit yields "Custom" (spec 26/46/76).

## Tests executed

| Command | Exit | Result | Artifact |
|---|---:|---|---|
| `make svx` (Phase A-D, j24 MSVC) | 0 | PASS (compiles, links, installs svxcorelo/svxlo) | build_svx_sw.log |
| `make sw` (Phase D-E, j24 MSVC) | 0 | PASS (compiles, links, installs swlo, sw.component) | build_sw_only.log |
| `make officecfg` (Controller.xcu) | 0 | PASS | build_officecfg.log |
| app launch `soffice.exe --writer` | — | PASS — process stays alive, only benign 0xE06D7363 startup EH noise, no AV/abort | %TEMP%/writer2027_crash.log |
| Type System popup open | — | IMPLEMENTED, NOT LIVE-CLICK-VERIFIED | — |
| font dropdown | — | IMPLEMENTED, NOT LIVE-CLICK-VERIFIED | — |
| Style gallery render | — | IMPLEMENTED, NOT LIVE-CLICK-VERIFIED | — |
| `make CppunitTest_sw_uibase_uiview` | blocked | NOT VERIFIED — external build corruption | — |
| `make UITest_sw_writer2027` | blocked | NOT VERIFIED — external build corruption | — |
| golden visual tests (fontbrowser/typesystem/styles) | blocked | NOT VERIFIED — no pinned visual runner | — |
| stress open/close | blocked | NOT VERIFIED | — |

## Visual artifacts

None captured: the pinned visual runner is not available on this machine, and
the UI-harness path is blocked by the external-build corruption (spec 87/89).
All visual claims below are **IMPLEMENTED, NOT VERIFIED**; screenshots must be
captured on the pinned runner to pass the gates.

## 100%

NOT VERIFIED (no real render gate run).

## 150%

NOT VERIFIED (no real render gate run).

## 200%

NOT VERIFIED (no real render gate run).

## Stress

NOT VERIFIED (open/close automation blocked).

## Remaining issues

1. **External incremental build corruption** (python3/liblangtag re-unpack)
   still blocks `make build`, `CppunitTest_*`, and `UITest_*` on this machine
   (spec 87). `make svx sw officecfg` are proven green.
2. **UI gates not executed**: golden visual tests, Type System/gallery mouse
   interaction, and stress require the pinned visual runner (spec 27). Report
   is IMPLEMENTED + build-verified + startup-smoke-verified, not
   UI-ACCEPTED/VERIFIED.
3. The dedicated gallery dispatches `.uno:StyleApply`; its current-style
   highlight uses the current paragraph pool id. Live mouse/keyboard behavior
   must be confirmed on the runner.

Legit status language: `TYPOGRAPHY BROWSER`, `TYPE SYSTEM`, and
`SEMANTIC STYLES WORKFLOW` are **IMPLEMENTED, PARTIALLY LIVE-VERIFIED**
(startup smoke = clean; click-path gates pending).