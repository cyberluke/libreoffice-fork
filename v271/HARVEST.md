# V271 Harvest Record (2026-09-27)

Everything harvested from upstream research + the Pher217 OfficeLabs fork on
this day, and what was merged into this tree.

## 1. Merged: Pher217/libreoffice-fork (OfficeLabs) — commit `25ca0324fcc1`

313 commits ahead of upstream master (April–Sept 2026), merged as a curated
3-way merge onto V271 master. Upstream base = `e8c17d8aec` (their last
refresh); our master is ~1 week newer, so all 313 are their custom commits.

### What came in (the AI assistant machinery)

- **`officelabs/` module** — the full AI assistant implementation:
  - CEF embedded Chromium: `CefInit`, `CefDebugPort`, `cef_subprocess_main`,
    `Package_cef.mk`, mac helper bundles; build flag `--with-cef=DIR`
    (`configure.ac`, `ENABLE_CEF`)
  - `WebViewPanel` (+ `WebViewPanelHostWin.cxx` / `WebViewPanelHostMac.mm`),
    `WebViewMessageHandler` (JS↔C++ bridge), `webviewpanel.ui`
  - Agent client: `AgentHttp` (loopback to a local agent, session/install-proof
    tokens), `AgentIdentity`, `TrustedUrl` (security boundary), `ConsentBridge`
  - Document integration: `DocumentController`, `InlineCompletionController`,
    `InlineCompletionEligibility`, `GhostTextWindow` (Copilot-style ghost text)
  - `StudioWindow` (CEF Views window), `OfficeLabsJob` UNO component
    (`ai.officelabs.OfficeLabsJob`), cppunit tests
- **AIDeck "AI Assistant" sidebar deck** registered in
  `officecfg/.../UI/Sidebar.xcu` for Text/Sheet/Presentation/Drawing, panel at
  `private:resource/toolpanel/SwPanelFactory/AIAssistantPanel`
- 3-theme system (`GetOLColors`), Windows dark title bar + per-user theme,
  "never show community infobars" policy, sidebar never collapses on first start
- Windows/macOS build fixes (harfbuzz pkg-config, gpgmepp disable, AUMID, icons)
- Tooling: `CLAUDE.md`, `build-sandbox.sh`, `build_helper.sh`,
  `.claude/skills/upstream-refresh/` (the fork-bump skill), `.github/workflows/upstream-watch.yml`

### Curation decisions

- **Kept** `dictionaries`, `helpcontent2`, `translations` submodules and
  `sanitize-ubsan-excludelist` from upstream (their fork had dropped them).
- **Dropped** committed junk: `build.log`, `build_new.log`,
  `build_session.log`, `build_session2.log`, `config_attempt1.log`,
  `config.guess.bak`, `config.sub.bak`, `deleted_files.txt`, `windres`, `yes`.
- **Conflict resolutions** (all conflict hunks merged manually):
  - `external/libxml2|xmlsec`: took upstream (cmake migration supersedes their
    nmake workaround); `external/lpsolve`: deleted (upstream removed lp_solve).
  - `sfx2/sidebar/{Deck,Panel,Theme,TitleBar,ThemePanel}.cxx`: both sides'
    changes merged; dropped their `connect_get_property_tree` call (API removed
    upstream).
  - `SidebarChildWindow.cxx`: took theirs (no first-start collapse).
  - `backingwindow.cxx`: took upstream (configurable donate button; their
    hide-everything variant dropped).
  - `sw/Library_sw.mk`: both blocks (upstream shlwapi + their `ENABLE_CEF`).
  - `vcl/win/app/salinst.cxx`: upstream refactors + their uxtheme
    preferred-app-mode block (`<win/svsys.h>` path kept from upstream).

### Build caveat

`officelabs/` is wired into `Repository.mk`/`RepositoryModule_host.mk` and
`Library_officelabs` builds unconditionally. The CEF targets are gated on
`ENABLE_CEF` (`--with-cef`), but confirm a **no-CEF build** compiles the
library before relying on it. License: MPL-2.0 (LO fork), CEF BSD — portable.

## 2. Applied: gerrit 211390 — sidebar crash fix — commit (after 25ca0324fcc1)

`tdf#173676 Fix crash in Sidebar UpdateConfigurations()` (Hossein, open on
gerrit, 2026-09-23) — null-guards `mpTabBar`. Applied cleanly to the merged
tree. Patch saved at `v271/harvest/211390-sidebar-crashfix.patch`.

## 3. Reference: abandoned upstream AI changes (gerrit, NOT merged)

- **194671** "Add AI Writing Assistant for Writer (rewrite with AI)" (Sanjay
  Karinje, abandoned 2026-03-06): full OpenAI-compatible C++ client —
  libcurl POST to ApiURL with `Authorization: Bearer`, config group
  `Linguistic/AIWritingAssistant` (ApiURL/AuthKey/ModelName/Enabled),
  options page in `cui`, rewrite dialog (5 styles), parses
  `choices[0].message.content`. 23 files.
- **194673** "sc: Add AI Explain Formula" (same author, abandoned 2026-01-29):
  same helper pattern for Calc. 16 files.
- Fetch from gerrit web UI (Download → patch) — `refs/changes/*` is not
  fetchable anonymously; the REST `/patch` endpoint is bot-gated for direct
  clients.

## 4. Corrected fact: no C++ WebView ever existed in LibreOffice

Checked the full 520k-commit history (all release tags 7.0 → 26.2): there is
no `include/vcl/WebView.hxx` or any webview API. The "tdf#105303: drop webview
files too" commit (`810fdf5e5623`, 2023-09-23) removed only the Impress HTML
export wizard's Perl/ASP assets (`sd/res/webview/*` — archived at
`v271/harvest/webview-port-reference.tar`).

**However:** WebView2 IS used in current LO development — the **CODA-W/CODA-Q**
project (Tor Lillqvist, 2026-09) builds a WebView2-based window shell with a
`{call,args}`/`{signal,args}` JS bridge. It lives on
`origin/collaboraoffice/online` (not master). That branch is the reference for
a native WebView2 embed without CEF.

## 5. Other context

- The GitHub mirror has 65 PRs, none merged (development is gerrit-only).
- V271 baseline commit `2f2825a534ab` holds the officecfg branding + v271
  extension + kilo config (created before the merge to keep it clean).
- Pher217 fork clone kept at `%TEMP%\kilo\pher217` (shallow, blob-less) for
  reference; upstream refresh process: their `/upstream-refresh` skill pattern.