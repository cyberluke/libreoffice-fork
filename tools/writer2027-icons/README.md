# writer2027-icons — Writer 2027 icon theme tooling

Generates the bundled LibreOffice icon themes from pinned upstream icon
sources:

| Theme | Source | License |
|---|---|---|
| `icon-themes/writer2027_carbon_svg/` | IBM Carbon Icons | Apache-2.0 |
| `icon-themes/writer2027_phosphor_svg/` | Phosphor Icons | MIT |
| `icon-themes/tdesign_svg/` | TDesign Icons | MIT |

The Writer 2027 themes are dark-only Writer 2027 products. The TDesign theme
is a general selectable theme with full Writer 2027 notebookbar coverage plus
the exclusive TDesign AI icon set. The generated SVG assets are committed to
the repository; the normal LibreOffice build never downloads icons.

## Layout

```text
tools/writer2027-icons/
├── README.md
├── icon-map.yaml              # hand-curated .uno: -> icon-name mapping (carbon/phosphor)
├── icon-map-tdesign.yaml      # generated .uno: -> TDesign icon-name mapping
├── icon-sources.lock.json     # pinned upstream commits + source paths
├── build_themes.py            # deterministic theme generator (carbon/phosphor/tdesign)
├── inventory_writer_commands.py  # Writer UI .uno: command inventory
├── validate_theme.py          # structural/safety/size validator
├── compare_coverage.py        # zero-fallback coverage gate
├── svg_normalize.py           # upstream SVG -> LibreOffice SVG normalizer
├── burn_ai_icons.py           # burns TDesign AI icons into every bundled theme
├── probe_icons.py             # dev helper: verify mapped names exist
├── lookup_icons.py            # dev helper: search upstream catalogs
├── generated/                 # inventory + coverage reports (gitignored outputs)
└── custom/
    ├── carbon/                # hand-drawn Writer 2027 custom icons (Carbon geometry)
    └── phosphor/              # hand-drawn Writer 2027 custom icons (Phosphor geometry)
```

## How to fetch pinned sources (dev-only)

```bash
python tools/writer2027-icons/build_themes.py --fetch --source-cache .work/icon-sources
```

This clones/updates the two upstream repos into `.work/icon-sources/`, checks
out the exact pinned SHAs from `icon-sources.lock.json`, and refuses to proceed
if the working tree does not match the pin. Never silently use another
revision.

## How to regenerate the themes

```bash
python tools/writer2027-icons/build_themes.py --source-cache .work/icon-sources --report
```

The generator:

1. reads `icon-map.yaml` (carbon/phosphor) and `icon-map-tdesign.yaml`
   (tdesign) command mappings,
2. resolves each `.uno:` command to the correct Carbon/Phosphor/TDesign SVG,
3. normalizes it (optical size, dark foreground, viewBox preserved),
4. writes `icon-themes/writer2027_{carbon,phosphor}_svg/cmd/{sc_,lc_,32/}*.svg`
   and `icon-themes/tdesign_svg/cmd/{sc_,lc_,32/}*.svg`,
5. emits `links.txt` aliases and a `GENERATED.json` provenance manifest,
6. writes `generated/coverage-{carbon,phosphor,tdesign}.md` with `--report`.

Output is deterministic: same mapping + same pinned sources => identical bytes.

## How to re-burn the TDesign AI icon set into every theme

The exclusive TDesign AI icons (`ai-*`, `robot*`, `aideck`) must appear in
every bundled icon theme so AI buttons show them regardless of the selected
pack:

```bash
python tools/writer2027-icons/burn_ai_icons.py
```

This writes `cmd/sc_<ai>.svg|png`, `cmd/lc_<ai>.svg|png` and `cmd/32/<ai>` for
all 26 AI icons into all 23 bundled themes (SVG for `*_svg` themes, PNG for
raster themes, rasterized with the built soffice). Requires the tdesign
source cache (`--fetch`) and a built soffice for the PNG themes.

## How to add a command mapping

1. Make sure the command actually exists in the Writer UI. The inventory is
   the source of truth:

   ```bash
   python tools/writer2027-icons/inventory_writer_commands.py
   ```

2. Find the exact `.uno:` ID in
   `generated/writer-command-inventory.json`.

3. Find the upstream icon names:

   ```bash
   python tools/writer2027-icons/lookup_icons.py table --carbon
   python tools/writer2027-icons/lookup_icons.py table --phosphor
   ```

4. Add an entry to `icon-map.yaml`:

   ```yaml
   ".uno:InsertTable":
     output: inserttable
     tier: 3
     status: exact
     carbon: table
     phosphor: table
   ```

   Status values: `exact` (upstream expresses the LO operation), `semantic`
   (closest upstream equivalent), `custom` (Writer-specific hand-drawn icon).

   For an alias (same icon as another command, no new file):

   ```yaml
   ".uno:SaveGraphic":
     target: ".uno:Save"
   ```

5. Verify names exist and regenerate:

   ```bash
   python tools/writer2027-icons/probe_icons.py --mapping tools/writer2027-icons/icon-map.yaml
   python tools/writer2027-icons/build_themes.py --source-cache .work/icon-sources
   ```

## How to add a custom icon

If neither Carbon nor Phosphor has a correct icon for a Writer 2027 command:

1. Search aliases/tags upstream and related icon families first.
2. Do not choose a misleading icon just for coverage.
3. Mark the mapping `status: custom`.
4. Draw the icon in that theme's geometry:

   - Carbon: `tools/writer2027-icons/custom/carbon/<name>.svg`
     (32x32 viewBox, filled geometry, black fill)
   - Phosphor: `tools/writer2027-icons/custom/phosphor/<name>.svg`
     (256x256 viewBox, filled round geometry, `fill="currentColor"`)

5. Reference it as `carbon: custom:<name>` / `phosphor: custom:<name>`.
6. Regenerate.

Never reuse a Carbon custom icon inside the Phosphor theme or vice versa.

## How to run coverage

```bash
python tools/writer2027-icons/compare_coverage.py \
  --inventory tools/writer2027-icons/generated/writer-command-inventory.json \
  --theme icon-themes/writer2027_carbon_svg
python tools/writer2027-icons/compare_coverage.py \
  --inventory tools/writer2027-icons/generated/writer-command-inventory.json \
  --theme icon-themes/writer2027_phosphor_svg
```

`missing = 0` is required for the Writer 2027 notebookbar (Tier 0-4). The
command exits non-zero otherwise.

## How to validate a theme

```bash
python tools/writer2027-icons/validate_theme.py --theme icon-themes/writer2027_carbon_svg
python tools/writer2027-icons/validate_theme.py --theme icon-themes/writer2027_phosphor_svg
```

Checks every SVG for: no scripts/foreignObject/external refs/embedded rasters,
valid XML, valid viewBox, explicit size (16/24/32), at least one drawable.

## How to create contact sheets (dev)

```bash
python tools/writer2027-icons/build_themes.py --contact-sheets
```

Renders per-theme contact sheets (icon + command name + source icon name) into
`workdir/writer2027-icons/` for quick visual review.

## How to update the upstream revision

1. Pick the new pinned SHA and update `icon-sources.lock.json`.
2. Update `third_party/writer2027-icons/{carbon,phosphor}/SOURCE.json` with
   the new commit.
3. Regenerate with `--fetch`:

   ```bash
   python tools/writer2027-icons/build_themes.py --fetch --source-cache .work/icon-sources
   ```

4. Inspect `git diff` on the generated themes. Expect size/geometry drift;
   reject anything that changes semantics.
5. Run `validate_theme.py`, `compare_coverage.py`, and the visual contact
   sheets.
6. Commit the new `SOURCE.json` pins together with the regenerated assets.

## How to validate licenses

- `third_party/writer2027-icons/carbon/LICENSE` — Apache-2.0 (upstream commit
  pinned in `SOURCE.json`).
- `third_party/writer2027-icons/phosphor/LICENSE` — MIT (upstream commit
  pinned in `SOURCE.json`).
- `THIRD_PARTY_ICONS.md` at the repository root documents both sources,
  modifications, and the pinned revisions.

## Requirements

- Python 3.9+ with `PyYAML` and `lxml`.
- Network access is required only for `--fetch` (dev). Regeneration from a
  populated `.work/icon-sources` cache is fully offline.

## CI drift gate

```bash
python tools/writer2027-icons/build_themes.py --source-cache <pinned-ci-source-cache>
git diff --exit-code -- icon-themes/writer2027_carbon_svg icon-themes/writer2027_phosphor_svg
```

This verifies the committed assets match the generator + mapping + pinned
sources. Icon regeneration CI is separate from the LibreOffice build CI; the
normal build never needs network access for icons.