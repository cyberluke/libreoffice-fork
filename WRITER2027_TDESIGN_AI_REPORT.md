# Writer 2027 TDesign Icon Pack + AI Icon Integration Report

## Summary

Added the Tencent TDesign icon set (https://github.com/Tencent/tdesign-icons)
as a bundled, selectable LibreOffice icon theme and burned its exclusive AI
icon set into every icon theme and every AI surface (WriterAgent AI, OfficeLabs
AI, Sidebar AI, v271 extension, officelabs-ui) so **all AI actions and buttons
feature the TDesign AI icons even when another icon pack is selected**.

## Upstream pin

| Source | Commit | License | Path |
|---|---|---|---|
| TDesign Icons | `a8a01c7955e4d8b29b0e32cc090bb3421f58d5dd` | MIT | `svg/` (2356 stroke-based 24x24 line icons) |

Pinned in `tools/writer2027-icons/icon-sources.lock.json` and
`third_party/writer2027-icons/tdesign/{LICENSE,SOURCE.json}`.

## 1. New TDesign icon theme (selectable pack)

`icon-themes/tdesign_svg/` — full Writer 2027 notebookbar coverage:

- **464 commands x 3 sizes** = 1392 SVGs (`cmd/sc_*.svg`, `cmd/lc_*.svg`,
  `cmd/32/*.svg`), 3 links.txt aliases, GENERATED.json provenance.
- Mapping: `tools/writer2027-icons/icon-map-tdesign.yaml` (generated from
  icon-map.yaml + curated Carbon->TDesign translation, every name
  probe-verified against the pinned catalog).
- **Coverage: 427/427 (100%)** for notebookbar_writer2027 (compare_coverage.py
  PASS), structural validation PASS (1392 SVGs).
- Display name "TDesign" via `IconThemeInfo::ThemeIdToDisplayName`.
- Registered in `configure.ac` (help text, default `with_theme`, validation
  case) — `checking which themes to include ... tdesign_svg` confirmed in a
  real autogen run.
- `images_tdesign_svg.zip` (536,478 bytes) built by the real
  `postprocess/CustomTarget_images.mk` pipeline and installed into
  `instdir/share/config/`.

## 2. AI icon set burned into EVERY bundled theme

The exclusive TDesign AI icons (`ai`, `ai-1`, `ai-article`, `ai-book-open`,
`ai-chart-bar`, `ai-coordinate-system`, `ai-cut`, `ai-edit`, `ai-edit-1`,
`ai-education`, `ai-git-branch`, `ai-image`, `ai-image-1`, `ai-layout`,
`ai-music`, `ai-screenshot`, `ai-search`, `ai-terminal`, `ai-terminal-1`,
`ai-textformat-italic`, `ai-tool`, `ai-video`, `robot`, `robot-1`, `robot-2`,
`aideck`) are present as command icons (`sc_*`/`lc_*`/`32/*`) in **all 23
bundled themes** (breeze, colibre, elementary, karasa_jaga, sifr, sukapura,
writer2027_carbon/phosphor, tdesign + their dark/svg variants):

- SVG themes get normalized SVGs (stroke `black`/fill `white` recolored to
  the Writer dark foreground `#F2F4F8`).
- PNG themes get PNGs rasterized with the real soffice.
- `tools/writer2027-icons/burn_ai_icons.py` (idempotent, deterministic);
  verified 78/78 per theme across 23/23 themes.
- Built zips verified to contain the icons: `images_tdesign_svg.zip`,
  `images_colibre.zip` (`cmd/sc_ai.png`), `images_writer2027_phosphor_svg.zip`
  etc.
- Because every installed theme archive carries the AI command icons, the
  runtime always resolves them **regardless of the selected pack** — no
  fallback subtleties.

## 3. AI surfaces wired to the TDesign AI icons

### Sidebar AI (native)
- `officecfg/.../UI/Sidebar.xcu` AIDeck already referenced
  `private:graphicrepository/cmd/lc_aideck.png` — the burned `lc_aideck`
  (TDesign `ai` mark) now resolves in every theme, so the sidebar AI deck tab
  shows the TDesign AI icon under any pack.
- `WriterCommands.xcu`: added first-class entries for `.uno:AI`, `.uno:Robot`,
  `.uno:AIToolbar`, `.uno:AISidebar` (icons resolve from the theme cmd/).

### WriterAgent AI extension (`officelabs/writeragent/`)
- `extension/assets/`: 26 AI SVGs + 78 AI PNGs (`ai_16/26/32.png` etc.) added
  (auto-included in the OXT by `cp -a`).
- `Addons.xcu`: `ImageIdentifier` added to 13 menu items
  (`vnd.sun.star.extension://org.extension.writeragent/assets/ai*.svg`).
- `description.xml` / `description.xml.tpl`: extension icon changed to
  `assets/ai.svg`.
- Chat sidebar hamburger menu (`plugin/chatbot/hamburger_menu.py`): Extend
  Selection → `ai`, Edit Selection → `ai-edit`, Search Nearby Files →
  `ai-search`, Text Analytics → `ai-chart-bar` (via the existing
  `menu_icon_filename` DPI-aware PNG loader).

### OfficeLabs CEF UI (`officelabs/ui/officelabs-ui/`)
- New in-tree UI bundle: `index.html` (branded "OfficeLabs AI" with AI action
  buttons) + `assets/` with 25 TDesign AI SVGs.
- `Package_officelabs_ui.mk` + `Repository.mk` registration deploy it to
  `instdir/program/officelabs-ui/` when `ENABLE_CEF=TRUE`.
- Staged into the running instdir (index.html + 25 AI assets).

### v271 office extension (`v271/office-extension/`)
- `extension/assets/`: 25 TDesign AI SVGs.
- `Addons.xcu`: `ImageIdentifier` added to all 8 menu items (sign-in →
  `ai-1`, status → `ai-terminal`, sync → `ai-git-branch`, etc.).

## Verification

| Gate | Result |
|---|---|
| configure.ac | accepts `tdesign_svg` (real autogen run) |
| coverage (notebookbar, 427 cmds) | 427/427 zero fallback |
| validate_theme.py (1392 SVGs) | PASS |
| drift_test.py (generator+burn reproducibility) | PASS |
| zips built by real make | images_tdesign_svg.zip + all 23 |
| AI icons inside zips | verified (sc_ai/lc_robot/lc_aideck in tdesign, colibre PNG, phosphor) |
| vcllo.dll rebuild | contains "TDesign" display name (IconThemeInfo change) |
| runtime launch + doc conversion | PASS (full stack loads, PDF export works) |
| officelabs-ui deployment | index.html + 25 AI assets in instdir |
| WriterAgent OXT sources | AI SVGs/PNGs + ImageIdentifiers + description icon |
| accidental file deletions (cleanup bug) | restored from git (38 files) |

## Files changed (this task)

- `configure.ac`, `Repository.mk`, `vcl/source/app/IconThemeInfo.cxx`,
  `officecfg/.../UI/WriterCommands.xcu`
- `tools/writer2027-icons/`: icon-map-tdesign.yaml (new), build_themes.py
  (tdesign source), burn_ai_icons.py (new), svg_normalize.py (stroke/white
  handling), drift_test.py (tdesign+burn), README.md
- `icon-themes/tdesign_svg/` (new theme, 1392 SVGs)
- `icon-themes/*/cmd/`: AI icons burned into all 23 themes
- `third_party/writer2027-icons/tdesign/` (LICENSE, SOURCE.json)
- `officelabs/`: Package_officelabs_ui.mk (new), Module_officelabs.mk,
  ui/officelabs-ui/ (new), writeragent extension assets/Addons.xcu/
  description.xml, hamburger_menu.py
- `v271/office-extension/extension/`: assets/ + Addons.xcu
- `THIRD_PARTY_ICONS.md`

## Remaining limitations

- The real officelabs-ui HTML bundle is a branded stub (the full CEF UI is
  not yet in-tree); the AI assets + package rule are in place so any real
  bundle drops into `officelabs/ui/officelabs-ui/` and inherits the icons.
- `Package_officelabs_ui` requires the full `make install` phase to copy into
  instdir (staged manually for the running instance).
- vcl CppunitTest execution still requires the test runner; changed code
  compiles and links.