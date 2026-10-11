# Third-party icon provenance

This repository bundles two bundled, selectable, native LibreOffice SVG icon
themes (Writer 2027) generated from upstream icon projects:

| Icon set | License | Upstream URL | Pinned revision |
|---|---|---|---|
| IBM Carbon Icons | Apache-2.0 | https://github.com/carbon-design-system/carbon | `47ab67155b185d56bf147b33cd9536e479ab0203` |
| Phosphor Icons | MIT | https://github.com/phosphor-icons/core | `2b75f3ad12b420c9504ef05df8d2564a28f8500e` |
| TDesign Icons | MIT | https://github.com/Tencent/tdesign-icons | `a8a01c7955e4d8b29b0e32cc090bb3421f58d5dd` |

Source snapshots / provenance data:

- `third_party/writer2027-icons/carbon/{LICENSE,SOURCE.json}`
- `third_party/writer2027-icons/phosphor/{LICENSE,SOURCE.json}`
- `third_party/writer2027-icons/tdesign/{LICENSE,SOURCE.json}`

Generated themes:

- `icon-themes/writer2027_carbon_svg/`
- `icon-themes/writer2027_phosphor_svg/`
- `icon-themes/tdesign_svg/`

The Writer 2027 themes are dark-only Writer 2027 products. The TDesign theme
is a general selectable theme with full Writer 2027 notebookbar coverage plus
the exclusive TDesign AI icon set (`ai-*`, `robot*`). The AI icon set is
additionally burned into every bundled icon theme (`cmd/sc_ai*.svg|png`, …)
so all AI actions/buttons feature the TDesign AI icons regardless of which
icon pack the user selects.

All generated SVG assets are committed; the normal LibreOffice `make` never
downloads icons.

## Modifications to upstream assets

### Carbon (Apache-2.0)

- Reframed / assigned to a LibreOffice optical target: `sc_*` = 16x16,
  `lc_*` = 24x24, `cmd/32/` = 32x32. If no native Carbon 16/24/20 asset exists,
  the 32x32 master is scaled (viewBox preserved, width/height set explicitly).
- Foreground recolored for the Writer 2027 dark theme (`#F2F4F8`, muted
  `#C6C6C6`). Carbon shape assets are not light/dark themes.
- Renamed to LibreOffice command resource names (`sc_<cmd>.svg`, ...).
- Some icons are custom-drawn in Carbon geometry where no upstream semantic
  equivalent exists (see `tools/writer2027-icons/custom/carbon/`).

### Phosphor (MIT)

- Weight selected by optical target: Bold `<name>-bold.svg` for 16px `sc_*`,
  Regular `<name>.svg` for 24px `lc_*` and 32px.
- Explicit dark-mode color substituted for `currentColor` (`#F2F4F8`).
- SVG `width` / `height` normalized for LibreOffice (viewBox 256 kept).
- Renamed to LibreOffice command resources.
- Some icons are custom-drawn in Phosphor geometry (see
  `tools/writer2027-icons/custom/phosphor/`).

### TDesign (MIT)

- Stroke-based 24x24 line icons; strokes (`black`) and body fills (`white`)
  are recolored to the Writer 2027 dark foreground (`#F2F4F8`).
- Renamed to LibreOffice command resources (`sc_<cmd>.svg`, …).
- The exclusive AI icon set (`ai`, `ai-1`, `ai-article`, `ai-book-open`,
  `ai-chart-bar`, `ai-coordinate-system`, `ai-cut`, `ai-edit`, `ai-edit-1`,
  `ai-education`, `ai-git-branch`, `ai-image`, `ai-image-1`, `ai-layout`,
  `ai-music`, `ai-screenshot`, `ai-search`, `ai-terminal`, `ai-terminal-1`,
  `ai-textformat-italic`, `ai-tool`, `ai-video`, `robot`, `robot-1`,
  `robot-2`) is burned into every bundled icon theme as command icons
  (`sc_*`/`lc_*`/`32/*`), the sidebar deck icon (`aideck`), the WriterAgent
  extension assets/menus, the v271 extension assets/menus, and the
  OfficeLabs CEF UI assets (`officelabs/ui/officelabs-ui/`).