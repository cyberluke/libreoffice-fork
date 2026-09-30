# Canonical Chat AI — Implementation Spec & Integration Points

**Status:** Spec only — no implementation in this document.
**Scope:** the one canonical chat-AI component and the integration points every app in the desktop pack uses. LibreOffice is the first (and reference) integration; other pack apps reuse the same bundle and protocol with a thin host adapter.
**Authoritative sources:** `officelabs/source/WebViewMessageHandler.cxx`, `officelabs/source/WebViewPanel.cxx`, `officelabs/source/CefInit.cxx`, `officelabs/source/TrustedUrl.cxx`, `officelabs/source/AgentHttp.cxx`, `officelabs/source/OfficeLabsJob.cxx`, `officelabs/inc/officelabs/CefDebugPort.hxx`, `officelabs/Package_cef.mk`, `officelabs/Package_cef_res.mk`. Where this spec and the code disagree, the code is authoritative today; discrepancies must be resolved by a protocol-version bump, not by silent drift.

---

## 1. Why this exists

The pack ships multiple apps that all need the same AI assistant experience (chat, document context, inline completion, rewrite, later vision/TTS). We will not build N different sidebars. One canonical implementation — a **static web bundle plus an agent protocol** — is embedded by each host through a **thin adapter**. This document fixes:

1. what the canonical component **is** and **must deliver** (Section 3),
2. the **bridge protocol** between the web UI and any host (Section 4),
3. the **agent protocol** the UI talks to directly (Section 5),
4. the **LibreOffice integration points** (already implemented; frozen surface, Section 6),
5. the **portable adapter contract** for other pack apps (Section 7),
6. **acceptance criteria** that define "implemented" (Section 8).

The painful failure modes we already hit once (missing CEF `icudtl.dat` → sidebar error dialog + hung Writer) are encoded as **deployment conformance requirements** (Section 6.3) so they cannot recur in any pack app.

---

## 2. Architecture (one picture)

```
┌────────────────────────────── Desktop app pack ──────────────────────────────┐
│                                                                              │
│  ┌─────────────────────────┐      ┌───────────────────────────────────────┐  │
│  │  Canonical Chat AI UI   │      │  Agent (sidecar service, per-machine) │  │
│  │  (officelabs-ui bundle) │      │  - OpenAI-compatible gateway          │  │
│  │  React + TS, static     │      │  - session token minting (0600 file)  │  │
│  │  files, NO host code    │      │  - chat streaming (SSE)               │  │
│  └───────────┬─────────────┘      │  - completions (mode=rewrite, …)      │  │
│              │                    │  - consent challenges & signed grants │  │
│              │  cefQuery()        │  - health                             │  │
│              │  (JS→host JSON)    └───────────────────▲───────────────────┘  │
│              │  window.officelabs                      │  HTTP + SSE          │
│              │  .__onMessage (host→JS)                 │  X-OfficeLabs-*     │
│  ┌───────────▼─────────────┐      ┌───────────────────┴───────────────────┐  │
│  │  Host adapter (thin)    │      │  Native features per host:           │  │
│  │  - hosts the bundle     │      │  - doc context / selection / edits   │  │
│  │  - implements bridge    │      │  - ghost-text inline completion      │  │
│  │  - trust rules          │      │  - undo scoping (undo_agent_top)     │  │
│  │  - session/consent      │      │  - restart / theme / external URLs   │  │
│  └───────────┬─────────────┘      └───────────────────────────────────────┘  │
│              │                                                                 │
│  LibreOffice (CEF)   ·   other pack app A (WebView2/…)   ·   app B (… )        │
└───────────────────────────────────────────────────────────────────────────────┘
```

Rules of the architecture:

- **The bundle never talks to the host app's internals directly.** Everything goes through the bridge (Section 4). It also never talks to the model/LLM through the host: chat is HTTP/SSE to the agent (Section 5).
- **The host never renders AI UI itself.** It embeds the bundle; it implements the bridge and native features.
- **The agent is one per machine**, not one per app — that is what makes chat state and consent portable across the pack.
- All three artifacts (bundle, host adapter, agent) version their protocol; a mismatch must be detectable at load time (Section 6.5).

---

## 3. The canonical component: `officelabs-ui` bundle

### 3.1 Deliverable
A **static web application** deployed as files under `<host install>/program/officelabs-ui/`, entry point `index.html`. No build-time coupling to the host; no runtime server required in production.

### 3.2 Stack expectations
- React + TypeScript, built with Vite (dev server serves the same app for `OFFICELABS_UI_DEV_URL`).
- `cefBridge.ts` is the **only** module allowed to touch the bridge API (the original UI already had this file; the contract below is its canonical form).
- Plain HTTP + SSE for chat; no WebSocket requirement.
- Optional (later phases): WebGPU-based local TTS (SuperTonic3) and canvas rendering for media; these are bundle-side and must not leak into the bridge.

### 3.3 Functional requirements (v1)
The bundle MUST implement, at minimum:

1. **Chat panel** (the sidebar): message list, input, streaming display from agent SSE, stop-generation, error/retry states (agent down → visible banner, backoff retry; session token 503 → retry).
2. **Session lifecycle**: on host message `session:newDocument` clear the chat conversation for the document (chat state is per-document, never cross-document); on app start, request `getAppType`/`getDocumentUrl` to initialize context.
3. **Consent flow**: render the agent's consent challenge and call `requestConsent`; on denial show the agent-provided error; never fabricate a grant.
4. **Theme**: on mount call `getActiveTheme`; apply host theme to the UI; when the user changes theme, call `requestOfficeRestart` (host-side) — the bundle must not restart anything itself.
5. **Document tools**: selection/context requests via `getSelection`/`getDocument`; edits applied through `applyEdit` (approve/reject of agent edits); rewrite via `rewriteGenerate`/`rewriteApply` (modes `replace` and `ghost`).
6. **External links**: open via `openExternalUrl` (never `window.open` directly), with the identical client-side validator (Section 4.3).
7. **Protocol version handshake** (Section 6.5).

### 3.4 Explicit non-requirements (do not build)
- No host-specific code paths (`if (host === 'libreoffice') …` is forbidden; capability differences come from the bridge, e.g. `getAppType`).
- No inline-completion rendering in the bundle — ghost text is a native host feature (Section 6.4); the bundle only triggers it via `rewriteApply mode=ghost`.
- No persistence of secrets. The bundle may persist chat history per document in `localStorage` (CEF cache is persistent — `%LOCALAPPDATA%\OfficeLabs\cef_data`), but never tokens or install proofs.

---

## 4. Bridge protocol (bundle ⇄ host)

Transport: CEF `cefQuery` in LibreOffice; the JSON-RPC-style equivalent in other hosts. All payloads are JSON objects.

### 4.1 JS → host (request types — frozen set, v1)

| `type` | Request fields | Success response | Failure (code, text) |
|---|---|---|---|
| `getDocument` | — | `{"text":"…"}` (empty when no doc) | 500 panel/doc errors |
| `getSelection` | — | `{"selection":"…"}` | 500 |
| `applyEdit` | `editId`, `action` (`"approve"`/`"reject"`) | `{"status":"approved"}` / `{"status":"rejected"}` | 400 unknown action/no doc, 500 |
| `getAppType` | — | `{"appType":"writer"\|"calc"\|"impress"\|"draw"}` (default `writer`) | 500 |
| `getDocumentUrl` | — | `{"url":"…"}` (empty when unsaved) | 500 |
| `getSessionToken` | — | `{"token":"…"}` | **503** (agent not up — UI must retry, never treat as success) |
| `requestConsent` | `challengeId` | `{"consentId":"…"}` (one-shot grant id) | 400 missing id, 403 denied |
| `requestOfficeRestart` | — | `{"success":true}` | 500 |
| `getActiveTheme` | — | `{"theme":"…"}` | (inline, no dispatch) |
| `openExternalUrl` | `url` (https only, ≤2048 chars, allowlist §4.3) | `{"success":true}` | 400 invalid, 500 failed |
| `rewriteGenerate` | `text`, `style?` (default `"improve"`) | `{"suggestion":"…"}` | 400, 502 agent error/empty (60 s budget) |
| `rewriteApply` | `suggestion`, `mode` (`"replace"` default, `"ghost"`) | `{"status":"applied"}` / `{"status":"ghost"}` | 400, 409 (ghost/inline unavailable) |
| `rewriteOpenDialog` | — | `{"status":"closed"}` (modal, returns when closed) | 400, 500 |

Error text is advisory; the **code** is the contract. Unknown `type` → 404 `"Unknown request type"`.

### 4.2 Host → JS

Host delivers `window.officelabs.__onMessage(json)` with JSON objects of shape `{"type":"<domain>:<event>", …}`.

| Message | Meaning |
|---|---|
| `{"type":"session:newDocument"}` | Document changed — bundle MUST clear per-document chat state |

Naming convention for future messages: `document:*`, `selection:*`, `theme:*`, `session:*`. Additions are additive-only; old bundles must ignore unknown messages.

### 4.3 Trust rules (security contract — must be replicated by every adapter)

1. A frame is **trusted** only if its URL is
   - `file://` under `<host install>/program/officelabs-ui/` (trailing slash load-bearing, no `..` anywhere), or
   - exactly the origin of `OFFICELABS_UI_DEV_URL` (dev only; routes/query allowed, other ports NOT).
2. `cefQuery` (all 13 types) is refused with 403 from any untrusted frame — checked at the query handler, not only at navigation.
3. `openExternalUrl`: `https://` only, length ≤ 2048, no control/whitespace/`\`` `;` `|` `&` `$` `<` `>` `"` `'` `\`. **The client-side validator in `cefBridge.ts` and the host-side validator must stay character-for-character identical** (they are security gates, not UX).
4. Session token: read from a 0600 file ≤ 256 bytes; 503 when absent/unreadable. The bundle holds the token (it must, to call the agent), but the **install proof is never handed to the page**.
5. Consent: the host shows the native dialog with agent-fetched content, signs the approval with an install secret the page never sees, and returns only a one-shot `consentId`. The grant is keyed to the agent's own digest, so a page cannot show one macro and run another.
6. `OFFICELABS_TRUST_LOG=<path>` is the opt-in audit trail for every trust decision (SAL_WARN alone compiles out in this fork — do not rely on it).

### 4.4 Threading contract (LibreOffice reference; adapter-relevant)

- cefQuery callbacks arrive on the browser-process UI thread.
- Document-touching handlers (getDocument, getSelection, applyEdit, getDocumentUrl, getAppType, rewrite*, requestOfficeRestart, openExternalUrl) **must dispatch to the VCL/main thread** (`Application::PostUserEvent`).
- `getSessionToken` and `getActiveTheme` answer inline (bounded reads) — the UI blocks on them before the first agent call; keep them O(1)-ish, never add remote/expensive work there.

---

## 5. Agent protocol (bundle → agent, direct)

The bundle talks to the agent **directly** (never proxied through the host). Chat/streaming is the UI's own HTTP/SSE traffic.

### 5.1 Base URL & headers
- Base: `http://127.0.0.1:<port>`, port from `OFFICELABS_AGENT_PORT`, **default 8766**.
- `X-OfficeLabs-Session: <token>` — required on every route except `/`, `/health` and the docs surface (agent-side rule; the UI must send it everywhere it can).
- `X-OfficeLabs-Install-Proof: <…>` — native-side only, never from the bundle.
- `X-OfficeLabs-Feature: <name>` — feature gating on routes (e.g. rewrite uses `/completions/` with `mode=rewrite`).
- `Content-Type: application/json` on POSTs.

### 5.2 Required endpoints (canonical minimum the UI depends on)
| Endpoint | Purpose | Semantics required |
|---|---|---|
| `GET /health` | liveness/version | 200 + `{"status":"ok", "protocolVersion":…}`; used for the banner/backoff |
| chat (SSE) | streaming conversation | request: `{messages, documentContext?, …}`; response: SSE events; must honor `X-OfficeLabs-Session`; must support stop |
| `/completions/` (`mode=rewrite`) | rewrite suggestions | request: `{text, style}`; response: suggestion text (see `fetchRewrite`/`parseRewriteSuggestion` in `AgentHttp.cxx` / `RewriteProtocol`) |
| consent challenge + grant | consent flow | challenge id issued by agent; grant consumed against agent digest; see `requestConsent` (Section 4.1) |
| docs surface | agent documentation | excluded from session requirement |

Exact request/response schemas of these endpoints belong to the agent spec (separate doc); this spec fixes the **semantics** the bundle and host depend on. Anything the bundle sends must be tolerant of agent restart (backoff, 503 → retry, never hang the sidebar).

---

## 6. LibreOffice integration points (reference implementation — mostly built)

### 6.1 Embedding surface (built, frozen)
- `WebViewPanel::getUIUrl()` — production loads `file://<instdir>/program/officelabs-ui/index.html`; dev loads `OFFICELABS_UI_DEV_URL` (e.g. `http://localhost:5173`).
- WebView docked via `officelabs/ui/webviewpanel.ui` (UIConfig_officelabs.mk); `OFFICELABS_STUDIO=1` opens the popup Studio window.
- Key forwarding (F5/Esc to the document, not the WebView) is host-native.

### 6.2 CEF configuration (built, frozen)
- `multi_threaded_message_loop=true` (Windows), `no_sandbox=true`, `windowless_rendering_enabled=false`.
- Cache: `%LOCALAPPDATA%\OfficeLabs\cef_data` (`root_cache_path` + `cache_path` `…/Default`) — persistent localStorage.
- Log: `%TEMP%\officelabs_cef_debug.log`, `LOGSEVERITY_VERBOSE` (Windows; `/tmp/…` is invalid there and must never be used).
- Debug port: `OFFICELABS_CEF_DEBUG_PORT` (1024–65535, 0/absent = off). **Shipped builds must never listen.**
- Subprocess: `officelabs_cef_subprocess.exe` beside `soffice.exe`.

### 6.3 CEF runtime deployment — CONFORMANCE REQUIREMENT (bug class already paid for)
Every host install MUST contain, next to the host binary:

- **Release/:** `libcef.dll`, `chrome_elf.dll`, `d3dcompiler_47.dll`, `libEGL.dll`, `libGLESv2.dll`, `vk_swiftshader.dll`, `vk_swiftshader_icd.json`, `vulkan-1.dll`, `v8_context_snapshot.bin`, `dxcompiler.dll`, `dxil.dll`
- **Resources/:** `icudtl.dat`, `resources.pak`, `chrome_100_percent.pak`, `chrome_200_percent.pak`, **all** `locales/*.pak`

Missing `icudtl.dat` = Chromium refuses to start (`Invalid file descriptor to ICU data received`) → sidebar error dialog + host hang. This is a **deployment check**, not a runtime check:
- LibreOffice: packages `cef` (Release) and `cef_res` (Resources, incl. wildcard locales) — `officelabs/Package_cef.mk`, `officelabs/Package_cef_res.mk`, registered in `Module_officelabs.mk` + `Repository.mk`. Do not regress these lists.
- Any new pack app embedding CEF must ship the identical file set.
- Pinned CEF: `cef_binary_148.0.10+g7ee53f5+chromium-148.0.7778.218_windows64`; a CEF upgrade is a pack-wide change (all adapters together), not per-app.

### 6.4 Native document features (built — the "integration points" the UI calls)
- **Ghost-text inline completion**: `InlineCompletionController` + `GhostTextWindow` + `InlineCompletionEligibility`; context budgets 2000 chars before / 500 after / 64 paragraph hops; 150 ms debounce; Tab accepts, Esc dismisses; undo scoped via `OfficeLabsJob`.
- **OfficeLabsJob** (`com.sun.star.task.XJob`): dispatch table — `ping`, `undo_agent_top` (`ContextTitle`, `MaxSteps≥1`). Adding a capability = one dispatch-table entry.
- **Rewrite**: native dialog (`RewriteDialog`) + `rewriteGenerate`/`rewriteApply` (`replace` | `ghost`) + `RewriteProtocol` (agent `/completions/` mode=rewrite).
- **Session token**: `readSessionToken()` (0600 file, ≤256 B); token minted by the agent at startup.
- **Agent base URL**: `OFFICELABS_AGENT_PORT` override, default `8766` (`AgentHttp.cxx`).

### 6.5 Protocol versioning (to be implemented)
- Add a `protocolVersion` handshake: bundle exposes `window.officelabs.protocolVersion`; host checks it on page load (and the agent reports it in `/health`).
- Rules: v1 now. Additive changes only within a major. On mismatch the host logs and still loads (old bundles keep working); breaking changes bump the major and the host refuses to pair mismatched bundle/host versions.
- The `cefBridge.ts` validator lockstep (§4.3.3) is part of versioning: a change to `isOpenableExternalUrl` must land on both sides in the same release.

### 6.6 Configuration surface (canonical env vars — all adapters honor these)
| Variable | Meaning |
|---|---|
| `OFFICELABS_UI_DEV_URL` | dev UI origin (also trusted origin) |
| `OFFICELABS_AGENT_PORT` | agent port (default 8766) |
| `OFFICELABS_CEF_DEBUG_PORT` | opt-in CDP port |
| `OFFICELABS_STUDIO` | `1` = open Studio window |
| `OFFICELABS_TRUST_LOG` | audit log path for trust decisions |
| `SAL_LOG` / `SAL_LOG_FILE` | host logging (LO) |

---

## 7. Adapter contract for other pack apps

A new pack app embeds the **same bundle** and must implement:

1. **Hosting**: load `program/officelabs-ui/index.html` (file://) or dev origin; WebView2/Electron/Tauri equivalent of CEF settings (§6.2) incl. persistent cache and verbose log; ship the full CEF file set (§6.3) when using CEF.
2. **Bridge**: the 13 query types (§4.1) with identical success/failure semantics; `window.officelabs.__onMessage` (§4.2).
3. **Trust**: §4.3 in full (trusted prefix, `..` rejection, https-only external URLs with the lockstep validator, session/consent rules). A weaker copy is a security bug.
4. **Threading**: document ops on the app's main thread; bounded inline answers for token/theme.
5. **Native features it can provide**: doc context/selection/edits, ghost-text completion, scoped undo, restart, theme. Adapters may return 409/404 for features they do not support — the bundle degrades (e.g. no rewrite button) instead of breaking.
6. **Agent access**: same base URL/headers (§5.1); the bundle is the only consumer of chat endpoints.

Acceptance for an adapter: the bundle passes its conformance checklist (Section 8) unchanged, with zero bundle-side changes.

---

## 8. Acceptance criteria ("implemented" means …)

### 8.1 Bundle
- [ ] Loads from `file://…/program/officelabs-ui/index.html` in production and from the dev origin in development (`OFFICELABS_UI_DEV_URL`), same build.
- [ ] Implements all 13 query types with the exact success/failure JSON of §4.1; unknown types never called.
- [ ] Handles `session:newDocument` (clears chat) and tolerates unknown host messages.
- [ ] Agent-down UX: banner + backoff retry; `getSessionToken` 503 → retry without erroring into a broken state; never blocks the host's main thread.
- [ ] Consent: renders challenge, calls `requestConsent`, handles 403, never fabricates `consentId`.
- [ ] Rewrite: generate → preview → apply (`replace`) and ghost (`ghost`) flows work; 409 handled.
- [ ] `cefBridge.ts isOpenableExternalUrl` is byte-identical in behavior to the host validator.
- [ ] `protocolVersion` handshake implemented and honored.

### 8.2 LibreOffice host (reference adapter — regression gates)
- [ ] `Package_cef` + `Package_cef_res` build and install the full §6.3 file set; a missing `icudtl.dat` fails the build/install, not the runtime.
- [ ] `soffice --writer` opens with the sidebar showing the bundle; process stays responsive; no error dialog; no hang (this exact regression: writer + sidebar + CEF init must stay green).
- [ ] Bridge conformance: all 13 handlers answer per §4.1 from a trusted frame; 403 from an untrusted frame.
- [ ] `%TEMP%\officelabs_cef_debug.log` exists and is verbose; no `Invalid logging destination` warning.
- [ ] Inline completion + `undo_agent_top` + rewrite native dialog still pass `CppunitTest_officelabs_*`.

### 8.3 Agent
- [ ] `/health` 200 with `protocolVersion`; session token minted at startup (0600, ≤256 B).
- [ ] Chat SSE streams; honors `X-OfficeLabs-Session`; 401 on bad/missing token.
- [ ] Consent challenge → signed grant → one-shot `consentId` consumption (never replayable).
- [ ] `/completions/ mode=rewrite` matches `RewriteProtocol` expectations.

### 8.4 Pack-wide
- [ ] A second pack app embeds the unmodified bundle through its adapter and passes §8.1 checklist.
- [ ] No host-specific branches exist in the bundle.

---

## 9. Explicit non-goals for this spec

- No UI pixel design, no model/LLM choice, no agent-internal schema (separate agent spec), no TTS/vision protocol yet (later phases extend the bundle only).
- No implementation now — this document is the contract to implement against.