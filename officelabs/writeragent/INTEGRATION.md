# OfficeLabs AI — WriterAgent Integration Manifest

This document describes how WriterAgent is bundled into the OfficeLabs
LibreOffice fork as a first-class, enabled-by-default product component.

| Field | Value |
|---|---|
| Upstream repository | `https://github.com/KeithCu/writeragent` |
| Pinned upstream commit | `78e03a31b2bef4ffea7b4b7cc112fe559177338d` (2026-10-01T01:29:40Z) |
| Local integration revision | see `git log -- officelabs/` (officelabs-ai integration commit) |
| Build target | `CustomTarget_writeragent-oxt` → `$(WORKDIR)/CustomTarget/officelabs/writeragent-oxt/WriterAgent.oxt` |
| Packaged destination | `$(INSTROOT)/$(LIBO_SHARE_FOLDER)/extensions/officelabs-ai` via `ExtensionPackage_officelabs-ai` |
| Installer component | `gid_Module_Optional_Extensions_OFFICELABSAI` (Default=YES), file list `Extension/officelabs-ai.filelist` (scp2) |
| Package identifier | `org.extension.writeragent` (unchanged — canonical ID, avoids duplicate copies) |
| Branding overlay | `officelabs/writeragent/officelabs-build/` (description.xml.tpl + label rewrite in `build-wa-oxt.sh`) |
| Update policy | Extension self-update removed; OfficeLabs application updater is the only update authority |
| Runtime config precedence | 1. user `writeragent.json` → 2. OfficeLabs deployment/admin config (future) → 3. OfficeLabs shipped defaults → 4. upstream defaults |
| MCP default binding | `127.0.0.1:18765` (loopback only, upstream default; never `0.0.0.0` unless user/admin opts in) |
| Known optional dependencies | Ollama/LM Studio/cloud providers, embeddings models, OCR, speech — lazy, on-demand; no network at startup |

## Architecture

```
officelabs/writeragent/            vendored WriterAgent snapshot (pinned)
  ├── UPSTREAM                     machine-readable pin (repository + commit)
  ├── OFFICELABS_PATCHES.md        every OfficeLabs divergence, why it exists
  ├── INTEGRATION.md               this manifest
  ├── vendor/                      runtime pip deps (PINNED.txt with sha256)
  ├── build-tools/                 build-time Python deps (pyyaml, polib)
  └── officelabs-build/
      ├── description.xml.tpl      rebranded, self-update removed
      └── build-wa-oxt.sh          deterministic OXT build
officelabs/CustomTarget_writeragent-oxt.mk     gbuild OXT producer
officelabs/ExtensionPackage_officelabs-ai.mk   gb_ExtensionPackage consumer
officelabs/Module_officelabs.mk                module wiring (OFFICELABS_AI)
configure.ac / config_host.mk.in               --enable-ext-officelabs-ai
distro-configs/OfficeLabs*.conf                AI enabled by default
scp2/source/extensions/*                        installer modules
setup_native/source/packinfo/packinfo_extensions.txt
bin/update-writeragent-upstream.py              upstream sync helper
```

```
vendored source -> build-wa-oxt.sh -> WriterAgent.oxt
    -> gb_ExtensionPackage -> INSTROOT/share/extensions/officelabs-ai
    -> officelabs-ai.filelist -> scp2 -> OfficeLabs installer
    -> clean install -> extension registry discovers bundled OfficeLabs AI
    -> Jobs.xcu OnStartApp -> org.extension.writeragent.Main
    -> OfficeLabs AI ready without any user installation step
```

## Build

`--enable-ext-officelabs-ai` (or the OfficeLabs distro configs, which set it)
adds `OFFICELABS_AI` to `BUILD_TYPE` and `-DWITH_EXTENSION_OFFICELABS_AI` to
`SCPDEFS`. With the flag on, `officelabs` module builds:

1. `CustomTarget_writeragent-oxt` — runs
   `officelabs/writeragent/officelabs-build/build-wa-oxt.sh` on a fresh copy
   of the vendored tree: upstream `generate_manifest.py`,
   `compile_translations.py`, then upstream `build_oxt.py --no-tests`
   (release semantics: Debug menu stripped, production code stripped,
   `plugin/lib/` populated from `vendor/`).
2. `ExtensionPackage_officelabs-ai` — `gb_ExtensionPackage` unpacks the OXT
   into `$(INSTROOT)/share/extensions/officelabs-ai` and writes
   `$(WORKDIR)/ExtensionPackage/officelabs-ai.filelist`.

The build is offline and deterministic:
- no `git clone`/`curl`/pip during `make`;
- no `unopkg add`, no LibreOffice launch, no `$HOME` writes;
- source = the pinned vendored snapshot; runtime deps = committed `vendor/`;
- build-time Python deps = committed `build-tools/` (pyyaml 6.0.2, polib 1.2.0);
- the OXT zip is timestamp-normalized via `gb_Helper_make_zip_deterministic`
  when `SOURCE_DATE_EPOCH` is set.

## Registration on a clean profile

No `unopkg add`, no Extension Manager interaction, no profile pre-seeding.
LibreOffice's normal extension deployment discovers
`share/extensions/officelabs-ai` on first start; bundled extensions are
trusted. `Jobs.xcu` registers `OnStartApp → org.extension.writeragent.StartupJob
→ org.extension.writeragent.Main` (unchanged from upstream). The startup job
registers lightweight capabilities; model clients, embeddings, scientific
processes, OCR, speech and web-research workers load lazily, so cold start
does not regress and a missing provider is configuration state, not failure.

## UI

- Sidebar deck `WriterAgentDeck` ("OfficeLabs AI") with `ChatPanel`, present
  in Writer, Calc, Impress and Draw contexts (upstream Sidebar.xcu).
- Menus/commands dispatch through the preserved `org.extension.writeragent:*`
  ProtocolHandler.
- Visible strings rebranded to "OfficeLabs AI" (description.xml, Addons.xcu
  menu title, sidebar deck title, eval dialog); internal IDs unchanged.
- Developer/test menu entries are stripped in the release OXT build (upstream
  `build_oxt.py --no-tests` behavior).

## Update workflow

```bash
python bin/update-writeragent-upstream.py <explicit-sha>
```

The helper fetches the exact SHA, verifies it, syncs the build-required
subset, preserves OfficeLabs-owned paths, rewrites `UPSTREAM`, and reports
changed files plus any vendored file that diverges from pristine upstream.
It never commits and never resolves conflicts silently. Release builds are
always pinned; floating master is never tracked.

## Runtime configuration

WriterAgent's own config (`writeragent.json` under the user profile,
OpenAI-compatible provider plumbing, no secrets hardcoded) is preserved.
OfficeLabs ships no API keys, endpoints or model downloads; without any
provider the sidebar shows configuration/onboarding state. MCP stays on
loopback `127.0.0.1:18765` by default.

## Diagnostics

Runtime logs follow upstream (`writeragent_debug.log` next to
`writeragent.json`). Build provenance for the bundled extension:
- `officelabs/writeragent/UPSTREAM` (upstream SHA);
- `git log -- officelabs/` (OfficeLabs patch revision);
- the OXT `description.xml` version (upstream `EXTENSION_VERSION`).

## Not in this slice (planned follow-ups)

- OfficeLabs ribbon/NotebookBar button routing into the WriterAgent sidebar
  (Layer 10 / slice 2). The deck is already reachable via View → Sidebar.
- Centralized OfficeLabs provider/settings UI bridging (slice 3).
- MCP status/settings surfaced in the OfficeLabs settings surface (slice 3).
- Full installer upgrade smoke (build A → build B) and MSI/dmg packaging run.