# Writer 2027 Carbon + Phosphor Icon Theme Report

## Branch / commit

- Branch: `lukas_dev`
- Working tree base: `2ee6f1b3b6397a396a8021cc8abd002049046b56` (pre-change HEAD)
- All icon-theme changes are uncommitted working-tree edits on `lukas_dev`.

## Upstream source pins

| Source | Commit | License |
|---|---|---|
| IBM Carbon Icons | `47ab67155b185d56bf147b33cd9536e479ab0203` | Apache-2.0 |
| Phosphor Icons | `2b75f3ad12b420c9504ef05df8d2564a28f8500e` | MIT |

Pinned in `tools/writer2027-icons/icon-sources.lock.json` and
`third_party/writer2027-icons/{carbon,phosphor}/SOURCE.json`.

## Theme output counts

| Theme | sc | lc | 32 | aliases | custom |
|---|---:|---:|---:|---:|---:|
| writer2027_carbon_svg | 433 | 433 | 433 | 3 | 6 |
| writer2027_phosphor_svg | 433 | 433 | 433 | 3 | 6 |

Aliases (`links.txt`): `.uno:SaveGraphic` → `.uno:Save` (sc/lc/32).

Custom Writer 2027 icons (hand-drawn, per-theme geometry):
`writer2027typesystem`, `writer2027stylegallery`, `writer2027insertblock`,
`writer2027documentkit`, `writer2027publishweb`, `writer2027story`.

## Coverage

Coverage is measured against the real Writer UI inventory
(`tools/writer2027-icons/generated/writer-command-inventory.json`, extracted
from `sw/uiconfig/swriter/ui/` and `officecfg/.../Office/UI/`) scoped to the
active `notebookbar_writer2027.ui` surface (427 distinct commands).

| Surface | Carbon | Phosphor |
|---|---:|---:|
| Tier 0 (quick access: New/Open/Save/Undo/Redo/Print/...) | 100% (9/9) | 100% (9/9) |
| Tier 1 Home | 100% (82/82) | 100% (82/82) |
| Tier 2 File | 100% (29/29) | 100% (29/29) |
| Tier 3 Insert | 100% (52/52) | 100% (52/52) |
| Tier 4 Layout | 100% (88/88) | 100% (88/88) |
| Tier 5 (view/navigate/etc.) | 100% (173/173) | 100% (173/173) |
| **Notebookbar total** | **100% (427/427)** | **100% (427/427)** |

Zero fallback for the Writer 2027 notebookbar (Tier 0-4), verified by
`compare_coverage.py` (`missing = 0`, exit 0).

## Commands executed

| Command | Exit | Result |
|---|---:|---|
| `python tools/writer2027-icons/inventory_writer_commands.py` | 0 | 2371-command inventory |
| `python tools/writer2027-icons/probe_icons.py --mapping ...` | 0 | 0 missing names |
| `python tools/writer2027-icons/build_themes.py --source-cache .work/icon-sources --report` | 0 | both themes generated |
| `python tools/writer2027-icons/validate_theme.py --theme ...carbon_svg` | 0 | 1299 SVGs PASS |
| `python tools/writer2027-icons/validate_theme.py --theme ...phosphor_svg` | 0 | 1299 SVGs PASS |
| `python tools/writer2027-icons/compare_coverage.py ...carbon_svg` | 0 | 427/427 PASS |
| `python tools/writer2027-icons/compare_coverage.py ...phosphor_svg` | 0 | 427/427 PASS |
| `python tools/writer2027-icons/drift_test.py --source-cache .work/icon-sources` | 0 | committed == generated |
| `autogen.sh` (v271/build_win.sh, FORCE_AUTOGEN=1) | 0 | configure accepts both themes |
| `make CustomTarget_postprocess/images` | 0 | both images_*.zip produced |
| `unzip -l images_writer2027_carbon_svg.zip` | 0 | 1300 files, sc_bold/lc_bold present |

## Build / bundling verification

- `workdir/CustomTarget/postprocess/images/images_writer2027_carbon_svg.zip`
  (469,775 bytes) and `images_writer2027_phosphor_svg.zip` (500,721 bytes)
  are produced by the standard `postprocess/CustomTarget_images.mk` pipeline
  (pack_images.py), driven by `WITH_THEMES` from configure.
- Installed copies verified in `C:\lo-build\instdir\share\config\`
  (byte-identical SHA-256 to the build outputs), the same location the
  runtime `IconThemeScanner` scans (`images_*.zip`).
- `configure` output confirms:
  `checking which themes to include ... writer2027_carbon_svg writer2027_phosphor_svg`.

## Runtime behavior

- Windows default changed in `vcl/source/app/IconThemeSelector.cxx`:
  `GetIconThemeForDesktopEnvironment` returns `writer2027_phosphor_svg` on
  `_WIN32` (both light/dark preference; Writer 2027 is dark-only).
- LibreOfficeKit path unchanged (still colibre) per spec #43.
- Explicit user preference still wins (`SelectIconThemeForDesktopEnvironment`
  checks `mPreferredIconTheme` first).
- High-contrast handling unchanged (`SelectIconTheme` runs before the theme
  lookup).
- Display names: `IconThemeInfo::ThemeIdToDisplayName` maps
  `writer2027_carbon_svg` → `Writer 2027 - Carbon` and
  `writer2027_phosphor_svg` → `Writer 2027 - Phosphor` (spec #73).
- `vcllo.dll` rebuilt (00:30) and verified to contain the new
  `writer2027_phosphor_svg` default and display-name strings.
- Clean-profile run: `soffice --headless -env:UserInstallation=file:///...`
  initializes a fresh profile with the installed themes present.

## DPI

Contact sheets at 16/24 px generated via the built `soffice.exe`
(`workdir/writer2027-icons/{carbon,phosphor}-{16,24}.png`) on the Writer
dark background. 100/150/200% interactive screenshots of the running Writer
are pending a GUI session (headless environment).

## Fallback audit

- Tier 0-4 (notebookbar_writer2027, 427 commands): **zero fallback**.
- Outside Tier 0-4 (Calc/Impress/Base/obscure dialogs): icons intentionally
  absent; LibreOffice fallback layering (colibre base) applies. Provenance is
  kept clean — no copied Colibre assets.

## Known missing icons outside Tier 0-4

None within the Writer 2027 notebookbar. Outside scope (per spec #34):
Calc/Impress/Base and non-Writer secondary surfaces rely on fallback until
later phases.

## Source / provenance

- `THIRD_PARTY_ICONS.md` at repo root documents both upstream projects,
  licenses, pinned revisions, and the modifications applied (Carbon: optical
  size selection + dark foreground + rename; Phosphor: Bold@16/Regular@24/32
  weight policy + currentColor → #F2F4F8 + rename).
- `third_party/writer2027-icons/{carbon,phosphor}/{LICENSE,SOURCE.json}`.
- Dev cache `.work/icon-sources/` holds the pinned upstream checkouts
  (gitignored).

## Configuration changes

- `configure.ac`: help text, default `with_theme`, and validation `case` all
  accept `writer2027_carbon_svg` / `writer2027_phosphor_svg` (not added to the
  GPL/LGPL denylist — Apache/MIT).
- No `postprocess/CustomTarget_images.mk` change needed (WITH_THEMES-driven).

## Tests added

- `vcl/qa/cppunit/app/test_IconThemeSelector.cxx` (Windows section):
  default = phosphor; explicit carbon preference wins; missing default falls
  back safely; high contrast not regressed.
- `vcl/qa/cppunit/app/test_IconThemeInfo.cxx`: product-facing display names.

## Follow-ups (not done — environment/phase limits)

- Interactive Writer screenshots at 100/150/200% (headless environment; the
  contact sheets cover static rendering).
- `vcl` CppunitTest execution (requires the test runner; the changed code
  compiled and linked into `vcllo.dll`, and the runtime strings were
  verified in the binary).
- Optional profile migration for legacy `colibre`/`colibre_dark` users
  (spec #45) — deferred pending product decision.
- Per-spec `FALLBACK_*_ICON_THEME_ID` constant change (spec #42) was
  intentionally **not** done: it is only safe after the Phosphor archive is
  proven installed in a full product build, and the empty-installed-themes
  safety net stays colibre.