# V271 Identity, Graph and Document Context integration

Connects the V271 LibreOffice fork to V271 Identity (OIDC Authorization
Code + PKCE), the V271 Personal Graph (`document.file`, `document.recent`,
`document.favorite`) and V271 AI context.

LibreOffice stays a full desktop application: documents remain local/cloud
files managed by LibreOffice; only metadata and extracted text mirror to the
central V271 services. Everything is built on **native extension points**
(no invasive core patches), per the integration spec.

## What is in this directory

```
v271/
  EXISTING_LIBREOFFICE_AUDIT.md     audit of fork integration points (spec §"Existing LibreOffice audit")
  office-extension/
    extension/                      the UNO extension source (.oxt payload)
      description.xml               extension metadata (com.v271.office.integration)
      META-INF/manifest.xml         python components + config schema/data
      V271.xcs / V271.xcu           org.openoffice.Office.V271 configuration
      Addons.xcu                    "V271" menu (sign-in/out, status, sync, favorite, workspace, context, configure)
      ProtocolHandler.xcu           vnd.v271.office:* protocol registration
      Jobs.xcu                      OnStartApp / OnCloseApp job binding
      python/v271/
        pkce.py                     PKCE (RFC 7636) primitives
        oidc.py                     OIDC client: discovery, authorize+PKCE, code exchange, refresh, revoke
        tokenstore.py               secure token storage (DPAPI on Windows, 0600 file elsewhere)
        http.py                     stdlib JSON HTTP client
        graph.py                    graph entity client (upsert/delete/list)
        vector.py                   central vector pipeline client
        chunker.py                  text chunking (no local model)
        fingerprint.py              size-guarded content fingerprints
        extract.py                  UNO text extraction with provenance (sheet/slide)
        documentinfo.py             source_id / entity building (URI identity, never display name)
        recents.py                  native recent-documents reader (ItemList/OrderList)
        favorites.py                favorite mirror (config + graph)
        workspace.py                workspace association (manual = authoritative)
        context.py                  typed document context + AI bridge
        integration.py              runtime core: global event listener, debounced worker, commands
        job.py / protocol.py        UNO components: Job, ProtocolHandler
        contextservice.py           com.v271.office.DocumentContext service
        dialogs.py                  message box + input dialog helpers
    build_oxt.ps1 / build_oxt.sh    build v271-office.oxt
    v271-office.oxt                 built extension package
  tests/
    run_all.py                      runs all unit tests (no LibreOffice needed)
    mock_v271_server.py             mock identity/graph/vector/AI service (stdlib only)
    test_pkce.py test_chunker.py test_fingerprint.py test_client_stack.py
  tools/
    evidence_capture.py             acceptance-criteria evidence (client stack, runs now)
    lo_evidence.py                  acceptance-criteria evidence inside a real LibreOffice
  evidence/
    evidence-client-stack.json      latest evidence run
```

## Build and install

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File v271/office-extension/build_oxt.ps1
# Linux/macOS
sh v271/office-extension/build_oxt.sh
```

Install into a LibreOffice build/profile:

```powershell
# per user (takes effect for the current profile)
unopkg add v271\office-extension\v271-office.oxt
# shared install (all users of the fork build)
unopkg add --shared v271\office-extension\v271-office.oxt
```

The extension is also intended to be pre-installed in the V271 fork builds
(see the fork's packaging step; the extension is a pure payload and does not
need to be compiled into the office).

## Configuration

Menu **V271 → Configure…** sets the V271 Base URL (persisted in
`org.openoffice.Office.V271/General/BaseUrl`). Environment variables
override empty configuration values (handy for headless/CI):

| Variable | Meaning |
|---|---|
| `V271_BASE_URL` | V271 API base, e.g. `https://v271.example.invalid` |
| `V271_ISSUER_URL` | OIDC issuer (default: `<base>/identity`) |
| `V271_CLIENT_ID` | OIDC client id (default `libreoffice-v271`) |
| `V271_REDIRECT_PORT` | loopback redirect port, `0` = ephemeral |
| `V271_SYNC_ON_SAVE` / `V271_EMBED_ON_SAVE` | `true`/`false` |
| `V271_MAX_CHUNK_SIZE` | embedding chunk size (default 2000) |
| `V271_ALLOW_INSECURE_TOKEN_STORAGE` | allow 0600-file token store when DPAPI is unavailable |

Menu commands: **Sign in…** (OIDC + PKCE in the system browser),
**Sign out** (revocation + local token wipe), **Status…**, **Sync now**,
**Toggle favorite**, **Link workspace…** (manual association is
authoritative), **Document context…**, **Configure…**.

## Runtime behavior

- On `OnStartApp` the job bootstrap attaches one `XDocumentEventListener`
  to `com.sun.star.frame.GlobalEventBroadcaster`; a background worker
  performs all network work.
- Open (`OnNew`/`OnCreate`/`OnLoadFinished`) and save
  (`OnSaveDone`/`OnSaveAsDone`) events are debounced per document and
  produce **upserts keyed by `source_id`** — repeated saves update the
  existing entity instead of duplicating it.
- On save, text is extracted with provenance (sheet:name, slide:N), chunked,
  and uploaded to the **central V271 vector service** (no local embedding
  model).
- At startup the native recent list (`org.openoffice.Office.Common/History/
  HistoryList`) seeds `document.recent` entities; the graph is the
  authoritative mirror — no NAI-specific duplicate storage is created.
- Tokens are stored with DPAPI on Windows (ctypes `CryptProtectData`) or in
  an owner-only (0600) file elsewhere; plaintext fallback requires the
  explicit allow flag.

## Acceptance criteria → implementation → evidence

| # | Criterion | Implementation | Evidence |
|---|---|---|---|
| 1 | Sign in via V271 OIDC + PKCE | `oidc.py` (discovery, S256, loopback redirect, code exchange, refresh) | `evidence/evidence-client-stack.json` `1_signin`; `tests` OIDC flow |
| 2 | Recent documents sync to Graph | `recents.py` + `graph.py` upsert | `2_recents_sync` |
| 3 | Favorite documents sync | `favorites.py` + `graph.py` upsert/delete | `3_favorites_sync` |
| 4 | Save/open events update, not duplicate | upsert-by-`source_id` + debounce in `integration.py` | `4_update_not_duplicate` |
| 5 | Current document context to V271 AI | `contextservice.py` + `context.py` | `5_document_context` (+ `lo_evidence.py` `document_context_service`) |
| 6 | Embeddings use central V271 vector service | `extract.py` → `chunker.py` → `vector.py` | `6_central_embeddings` |
| 7 | Semantic retrieval locates a document | `vector.py.query` | `7_semantic_retrieval` |
| 8 | NAI OS consumes the same entity | graph entity contract (type/source_id upsert); `document.file` served to consumers | `8_shared_entity` |
| 9 | Credentials stored securely | `tokenstore.py` (DPAPI / 0600) | `9_secure_token_storage` |

## Running the tests and evidence

```powershell
# unit tests (pure Python, no LibreOffice): 28 tests
python v271\tests\run_all.py

# acceptance evidence (client stack, no LibreOffice): 9/9 criteria
python v271\tools\evidence_capture.py

# LO-side evidence (requires a LibreOffice build + installed extension):
#   1. start the mock service:  python v271\tests\mock_v271_server.py
#   2. start LO headless with a UNO socket (see lo_evidence.py header)
#   3. V271_BASE_URL=<mock> python v271\tools\lo_evidence.py
```

## Backend contract (for the V271/NAI service)

The extension implements and the mock server validates this contract:

- OIDC: standard discovery at `<issuer>/.well-known/openid-configuration`;
  `authorize` (PKCE S256), `token` (`authorization_code` +
  `refresh_token`), `revoke`, `userinfo`. Loopback redirect
  `http://127.0.0.1:<port>/callback`, client `libreoffice-v271`.
- Graph: `POST /api/v1/graph/entities` (idempotent upsert keyed by
  `type` + `source_id`; `updated` flag in response),
  `DELETE /api/v1/graph/entities/{type}/{source_id}`,
  `GET /api/v1/graph/entities?type=...` → `{items: [...]}`.
- Vector: `POST /api/v1/vector/upsert` (`{source_id, chunks:
  [{text, provenance}], metadata}`), `POST /api/v1/vector/query`
  (`{query, k}` → `{items: [{source_id, score, snippet}]}`),
  `DELETE /api/v1/vector/documents/{source_id}`.
- AI: `POST /api/v1/ai/summarize` (`{source_id, display_name, text}` →
  `{summary}`).

Entity payload fields: `type`, `source_id`, `display_name`, `file_type`
(`text|spreadsheet|presentation|drawing|other`), `uri`, `last_opened`,
`last_edited`, `favorite`, `workspace_id`, `fingerprint`, `metadata`
(`module`, `service`, `unsaved`).

## Limitations (honest scope)

- The fork has no build environment here, so the LO-side evidence script
  (`tools/lo_evidence.py`) is provided but not executed; the client stack
  (all network, crypto, chunking, token storage, entity semantics) is fully
  exercised by the runnable tests and evidence harness.
- Unsaved documents get a session-scoped `unsaved:` id until first save;
  there is no stable document id in this LibreOffice version
  (see `EXISTING_LIBREOFFICE_AUDIT.md` §8).
- The OIDC flow runs on the main thread while the browser is open (modal);
  graph/vector traffic never blocks the UI (worker thread).
- The V271 service endpoints above are the contract this extension is
  written against; the mock server doubles as the reference implementation
  for backend work.