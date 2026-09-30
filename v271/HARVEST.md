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
  Karinje, created 2025-11-27, abandoned 2026-03-06): full OpenAI-compatible
  C++ client — libcurl POST to ApiURL with `Authorization: Bearer`, config
  group `Linguistic/AIWritingAssistant` (ApiURL/AuthKey/ModelName/Enabled),
  options page in `cui`, rewrite dialog (5 styles, apply as tracked
  changes), parses `choices[0].message.content`. 24 files, +1371/-1.
- **194673** "sc: Add AI Explain Formula" (same author, created 2025-11-27,
  abandoned 2026-01-29): same helper pattern for Calc; dialog shows cell
  ref + formula, auto-explains. 16 files, +686/-0. NOT standalone — reuses
  the `Linguistic.xcs` schema added by 194671.
- Both fetched anonymously into `refs/harvest/194671` + `refs/harvest/194673`
  via `git fetch https://git.libreoffice.org/core refs/changes/71/194671/2`
  (Gerrit advertises this ref in its own REST `fetch` metadata — earlier
  failure was from trying the GitHub mirror instead of `git.libreoffice.org`).
  Exported to `v271/harvest/194671-ai-writing-assistant.patch` (72 KB) and
  `v271/harvest/194673-ai-explain-formula.patch` (36 KB).
- Quality verdict (code inspected): **promising sketches, not merge-ready**.
  CI never passed (PS1: macOS linker error; PS2: clang-format; 194673:
  `-Werror=shadow` in `cellsh3.cxx` — never fixed). No human Code-Review
  vote ever (+1/+2 absent); abandoned by the review bot for inactivity.
  Code issues: blocking libcurl on the UI thread with `Application::Yield()`
  (UI freeze, no cancel/streaming), hardcoded `/tmp/ai_debug.log` debug
  logging, `boost::property_tree` temperature-as-string hack, generic error
  strings. ~10 months stale vs current master.
- Gap vs `officelabs/`: (1) rewrite-selection-with-style-presets incl.
  tracked-changes apply — officelabs only has ghost-text inline completion;
  (2) formula explanation — absent in officelabs; (3) user-configurable
  endpoint/auth/model via Tools→Options — officelabs `AgentHttp` targets a
  fixed local agent `http://127.0.0.1:8766` (env `OFFICELABS_AGENT_PORT`
  only), no config UI, no key storage.

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

## 6. Port: rewrite dialog + ghost-text integration (2026-09-28)

The gap list in §3 was acted on: gap (1) is now implemented in-repo against
the officelabs panel, using the 194671 dialog shape but **no sw dependency**
(UNO-only document access) and **no blocking UI calls** (worker-thread fetch).
All of it is CEF-gated (`ENABLE_CEF`), like the panel itself.

- `officelabs/inc/officelabs/RewriteProtocol.hxx` + `source/RewriteProtocol.cxx`
  (new): extends the agent `/completions/` protocol with `mode=rewrite` —
  `{"text","style","mode":"rewrite","max_suggestions":1}` with
  `X-OfficeLabs-Feature: rewrite`, 60 s budget; response parsed with the
  existing `parseFirstSuggestion` shape (`{"suggestions":[{"text"}]}`);
  newlines preserved (rewrite ≠ single-line completion). Styles: clarity,
  formal, concise, simpler, grammar, improve.
- `DocumentController::replaceSelection()` (new): undoable selection
  replacement via `XText::insertString(range, text, bAbsorb=true)` inside an
  undo context; collapsed caret falls back to insert.
- `InlineCompletionController::showExternalSuggestion(text, bReplaceSelection)`
  (new): shows an externally produced suggestion (rewrite) as ghost text at
  the caret with the standard interaction — **Tab accepts** (replacing the
  selection), Escape/typing/mouse dismisses. `stillValid()` is skipped for
  external suggestions (stale request context), but Tab refuses when the
  selection it was anchored to is gone; type-through is disabled (a rewrite
  does not continue typed text). `hideGhost()` clears the external flags.
- `WebViewMessageHandler`: new cefQuery types `rewriteGenerate`
  ({text, style} → `{"suggestion": ...}`, fetch on a detached worker),
  `rewriteApply` ({suggestion, mode: "replace"|"ghost"}), `rewriteOpenDialog`
  (native modal dialog). The React UI (external repo) can drive the whole
  flow with these three calls.
- `RewriteDialog` (new, `officelabs/ui/rewrite.ui` — ported from 194671's
  `airewritedialog.ui`): style combo, read-only original, editable
  suggestion, Generate (async, status label), Replace (undoable). Tracked-
  changes apply was dropped — no sw shell access from officelabs; redline
  apply belongs to a future sw-side UNO service.
- Build: `Library_officelabs.mk` (+2 objects, CEF block), `UIConfig_officelabs.mk`
  (+rewrite.ui), `CppunitTest_officelabs_controller.mk` (+test_rewrite_protocol,
  +curl external), new `qa/cppunit/test_rewrite_protocol.cxx` (5 pure-function
  tests).
- Agent-side contract (sidecar): implement `mode=rewrite` on `/completions/`
  keyed off the `style` field; reply is the standard `suggestions[].text`
  envelope. Not verified against a live agent yet (no build env).

## 7. Other context

- The GitHub mirror has 65 PRs, none merged (development is gerrit-only).
- V271 baseline commit `2f2825a534ab` holds the officecfg branding + v271
  extension + kilo config (created before the merge to keep it clean).
- Pher217 fork clone kept at `%TEMP%\kilo\pher217` (shallow, blob-less) for
  reference; upstream refresh process: their `/upstream-refresh` skill pattern.