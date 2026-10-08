# WRITER 2027 — Type System Font Findings (Phase 6 companion)

> Working document for later processing: font acquisition, installation state,
> classification of scanned font packs, and the converged editorial shortlist.
> Last updated: 2026-10-01 (Phase 6, source-only, NOT BUILT/NOT TESTED).

## 1. Purpose

Phase 6 (Type Systems + Art Direction Presets) resolves preset font roles
(heading/body/mono/display) against the *installed* font enumeration. This file
records everything needed to (a) complete font acquisition, (b) run the visual
quality gate, and (c) later add validated pairings as new presets.

Resolution rules (recap):

```
preferred family -> installed? -> use exactly
                 -> no -> Phase 6 aliases ("SAP 72" -> "72")
                 -> no -> Phase 2 TypographyCatalog aliases ("Source Serif 4" -> "Source Serif")
                 -> no -> fallback1 -> fallback2
                 -> none installed -> role unresolved: style keeps its family (UI shows "N fonts missing")
```

No network, no downloads at runtime; no second font database; missing fonts are
always shown explicitly in the preset cards.

## 2. Installation method (this Windows machine)

- Per-user install: copy to `%LOCALAPPDATA%\Microsoft\Windows\Fonts` +
  `HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts` values.
- Verification: `System.Drawing.Text.InstalledFontCollection` family names
  (authoritative, no build needed).
- LibreOffice picks up new fonts after a restart (FontList is built at docshell init).

## 3. Already installed (free/open, verified via .NET)

| Family (installed name) | Source | License | Used by |
|---|---|---|---|
| Inter (+ weights) | rsms/inter 4.1 | OFL | Modern Product/Executive/Tech/Creative Agency fallbacks |
| JetBrains Mono | JetBrains 2.304 | OFL | mono role everywhere |
| IBM Plex Sans | IBM/plex 6.4.0 | OFL | Tech/Research heading fallbacks |
| IBM Plex Mono | IBM/plex 6.4.0 | OFL | Editorial/Research mono fallbacks |
| Source Serif 4 | adobe-fonts/source-serif 4.004 | OFL | Editorial heading fb2 ("Source Serif" via alias) |
| Instrument Serif | google/fonts | OFL | Editorial/Creative Agency heading fallbacks |
| Atkinson Hyperlegible | google/fonts | OFL | Accessible (native) |
| OpenDyslexic | antijingoist/opendyslexic | OFL | Accessible secondary option |
| **72** | SAP (Apache-2.0, d.dam.sap.com) | Apache-2.0 | **"SAP 72" via Phase 6 alias** (installs as "72") |
| **72 Mono** | SAP (Apache-2.0) | Apache-2.0 | **"SAP 72 Mono" via Phase 6 alias** |
| **Piazzolla** (+ italic) | google/fonts, VF | OFL | Editorial/Research candidates (opsz 8–30) |
| **Alegreya** (+ italic) | google/fonts, VF | OFL | Serif body candidate |
| **Alegreya Sans** (Regular/Italic/Medium/Bold) | google/fonts | OFL | Sans heading/body candidate |
| **Andada Pro** (+ italic) | google/fonts, VF | OFL | Display serif candidate |

80 files installed. Every current preset resolves fully on this machine
(commercial roles via designed fallbacks).

## 4. Commercial preset families — obtain from vendors (NOT downloadable freely)

| Family | Vendor | Roles | Minimum weights |
|---|---|---|---|
| Canela | Commercial Type | Editorial heading/display | Regular, Medium (500) |
| Söhne | Klim Type Foundry | Editorial body; Executive heading | Regular, Semibold (600) |
| Neue Montreal | Pangram Pangram | Modern Product heading (600); Creative Agency body | Regular, Semibold |
| Neue Machina | Pangram Pangram | Tech heading (600) | Semibold |
| Hatton | Pangram Pangram | Creative Agency heading/display (700) | Bold |
| Druk | Commercial Type | Creative Agency display fb (700) | Bold |

Alias notes: Klim may install as "Soehne" — already mapped in Phase 2 alias
table. Any other install-name mismatch => add one line to the Phase 6 alias
table in `svx/source/tbxctrls/writer2027typesystem.cxx`
(`aTypeSystemAliases`).

## 5. Recommended free packs (not yet installed)

- Google Fonts Cyrillic packs: serif (59 fam), sans (130 fam), mono (15 fam)
- Cormorant + Ysabeau (Catharsis/Thalmann) — display serif + grotesk, OFL
- Geist (Vercel) — modern technical sans, OFL
- Maple Mono — mono, OFL
- ParaType Public Type (PT Sans/Serif/Mono) — legit free ParaType set
- Банниковская (Bannikova) — ParaType freeware book serif

## 6. Pack scan log (2026-10-01)

All packs are torrent/marketplace rips; commercial content requires separately
held licenses. "Filler" = display/script/logo faces, basic Latin, no Czech, no
editorial value.

| # | Pack / content | Verdict | Keepers |
|---|---|---|---|
| 1 | Google Fonts CJK/Cyrillic packs, Cormorant+Ysabeau, Geist, Maple Mono, fan fonts | Free/novelty mix | Cormorant, Ysabeau, Geist, Maple Mono |
| 2 | Progressbar95 game fonts + strays (Open Sans, Roboto, Comic Neue, Selawk) | Pixel/retro, skip | Roboto/Roboto Slab only |
| 3 | Display bundle (Blacker, Felora, Juxta Sans Mono, TT Nooks, URW Akropolis…) | Display filler | Blacker, Felora, Juxta Sans Mono |
| 4 | Foundry pack (Sabon Cyrillic, Futura PT, TT Norms/Firs/Limes/Squares, Cera, Albertus Nova, ParaType set) | **Strong** | Sabon Cyrillic, TT Norms, Futura PT, TT Firs, Cera |
| 5 | Indie pack (Aristotelica, Codec, Cocogoose, Kabrio, TT Commons/Severs/Jenevers…) | Mixed | **TT Commons**, Codec, TT Severs, TT Jenevers |
| 6 | Handwritten pack (TT Wellingtons, TT Travels, Heading Pro, Juxta…) | Weak | TT Wellingtons, TT Travels |
| 7 | Display pack (Venti CF, Tecnica Stencil, TT Backwards…) | Weak | Venti CF |
| 8 | Adobe/Monotype + Parachute pack (Arno Pro, Gill Sans Nova, PF Bague/Beau/Centro/Libera, ALS Schlange Slab, Uni Neue, Verdana Pro) | **Strongest** | **Arno Pro, Gill Sans Nova**, PF Bague, PF Beau Sans, PF Centro Serif, ALS Schlange Slab, Uni Neue |
| 9 | Latin-American indie pack (Univia Pro, TT repeats, multi-script displays) | Repeats | Univia Pro |
| 10 | Indie/TypeType repeats (Intro Rust 214, Achille II, TT Inters…) | Weak | Intro (Fontfabric), Achille II, TT Inters |
| 11 | Display pack (Uni Neue repeat, TT Pubs…) | Zero yield | — |
| 12 | Letterhead Fonts (LHF) catalog — 35 vintage/script families | Genre gold, editorial zero | — |
| 13 | Ecofonts (Arial/Calibri/TNR/Trebuchet/Verdana Ecofont) | Ink-saving gimmick, skip | — |

## 7. Converged editorial shortlist (EN + CZ safe)

| Role | First pick | Alternatives |
|---|---|---|
| Serif body | **Arno Pro** ★ | Sabon Cyrillic, Achille II, Piazzolla (free), Alegreya (free) |
| Sans body/heading | **Gill Sans Nova** ★ | TT Commons, TT Norms, TT Inters, PF Bague, Intro, Uni Neue, TT Wellingtons, Venti CF |
| Display/Title | Futura PT | Blacker, TT Travels, ALS Schlange Slab, Cormorant (free) |
| Mono | Juxta Sans Mono | JetBrains Mono (installed) |

## 8. Language coverage notes

- EN: all listed families cover basic Latin natively.
- CZ requires Latin Extended-A: `á č ď é ě í ň ó ř š ť ú ů ý ž` (+ capitals).
  Tricky glyphs fonts often miss: `ď ť ů ř`.
- ✅ full: Inter, JetBrains Mono, IBM Plex, Source Serif 4, SAP 72/72 Mono,
  Atkinson, Arno Pro, Gill Sans Nova, Sabon Cyrillic, Futura PT, TT families,
  Parachute families, Cera, Venti CF, Univia Pro, Codec (verify), Intro (verify).
- ⚠️ verify at install: Instrument Serif, OpenDyslexic, Blacker, Juxta Sans
  Mono, Achille II, Uni Neue.
- Test pangram: `Příliš žluťoučký kůň úpěl ďábelské ódy`
- Writer renders Cyrillic through the Western slot (Type System sets
  Western/CJK/CTL all three); per-glyph fallback substitutes missing glyphs.

## 9. Alias registry (name -> installed family)

Phase 6 (`aTypeSystemAliases`, svx tbxctrls writer2027typesystem.cxx):

```
"SAP 72"      -> "72"
"SAP 72 Mono" -> "72 Mono"
```

Phase 2 (TypographyCatalog, frozen — do not edit): "Soehne" -> "Söhne",
"Source Serif 4"/"Source Serif Pro" -> "Source Serif",
"Inter Variable" -> "Inter", "JetBrainsMono Nerd Font" -> "JetBrains Mono", etc.

## 10. Future preset candidates (data-driven, one catalog entry each)

When the visual gate validates a pairing, add to
`aPresetSeeds` in `svx/source/tbxctrls/writer2027typesystem.cxx` (id, name res
id, fonts, scale, weights, colors) + one NC_ string. Candidates:

- `classic-editorial`: body Arno Pro / Sabon Cyrillic, heading Gill Sans Nova /
  TT Commons, mono Juxta Sans Mono, Editorial scale
- `alegreya`: body Alegreya, heading Alegreya Sans, mono JetBrains Mono,
  Balanced scale
- `piazzolla`: body Piazzolla (opsz text), heading Piazzolla (opsz display) or
  Blacker, mono JetBrains Mono, Editorial scale
- `product-geist`: body Geist, heading Geist/Uni Neue, mono Geist Mono,
  Balanced scale

## 11. Next steps

- [x] Install free four: Piazzolla, Alegreya, Alegreya Sans, Andada Pro (done 2026-10-01, verified via .NET; LO restart required)
- [ ] User installs licensed commercial: Arno Pro, Gill Sans Nova, Sabon Cyrillic,
      Futura PT, TT Commons, TT Norms, PF Bague, + six preset families (§4)
- [ ] Verify enumerated names + `ř ů ď ť` per family after install
- [ ] Run visual gate on the 7 existing presets (requires built LibreOffice)
- [ ] Wire winning pairings as new presets (§10) + re-run detection
- [ ] Optional: bundle free/open fonts in a later packaging phase