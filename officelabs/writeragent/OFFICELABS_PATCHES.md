# OfficeLabs Patches — WriterAgent Integration Divergences

Every divergence between the vendored WriterAgent tree in `officelabs/writeragent/`
and pristine upstream `78e03a31b2bef4ffea7b4b7cc112fe559177338d` is listed
here. Everything else is byte-identical to upstream.

The goal is to keep this list small. Overlays live in
`officelabs/writeragent/officelabs-build/`; they are applied at build time in
`build-wa-oxt.sh` and are never written back into the upstream files.

## 1. Vendored subset (not a code change)

The vendored tree contains only the build-required surface:

- vendored: `extension/`, `plugin/`, `locales/`, `scripts/`,
  `Makefile`, `AGENTS.md`, `LICENSE`, `pyproject.toml`,
  `requirements-vendor.txt`, `README.md`
- not vendored (dev/test/CI surface, not consumed by the build):
  `docs/`, `tests/`, `website/`, `wiki/`, `Showcase/`, `compute_service/`,
  `native/`, `contrib/`, `extension-core/`, `extension-harper/`, `.github/`
- `scripts/` minus the large dev/test data files `longdocsample.odt`,
  `lo_test_10k.csv`, `lo_test_10k.ods`, and the dev-only eval benchmark
  directory `scripts/prompt_optimization/`

`bin/update-writeragent-upstream.py` re-syncs exactly this subset.

## 2. OfficeLabs-owned additions inside `officelabs/writeragent/`

| Path | Why |
|---|---|
| `UPSTREAM` | machine-readable upstream pin (repository + commit) |
| `INTEGRATION.md` | integration manifest |
| `OFFICELABS_PATCHES.md` | this file |
| `officelabs-build/description.xml.tpl` | OfficeLabs description overlay: display-name "OfficeLabs AI", publisher "OfficeLabs", **upstream GitHub self-update feed removed** (OfficeLabs application updater is the only update authority) |
| `officelabs-build/build-wa-oxt.sh` | deterministic OXT build driver (upstream packaging scripts used unmodified) |
| `vendor/` | runtime pip dependencies from `requirements-vendor.txt`, pinned in `vendor/PINNED.txt` with wheel sha256 hashes (upstream obtains them via `uv pip install --target vendor` at build time; OfficeLabs commits them so `make` is offline) |
| `build-tools/` | build-time Python dependencies for the packaging scripts: PyYAML 6.0.2, polib 1.2.0, pinned with sha256 |

## 3. Build-time overlays (applied by `build-wa-oxt.sh` to the workdir copy)

| File | Change | Why |
|---|---|---|
| `extension/description.xml.tpl` | replaced by `officelabs-build/description.xml.tpl` | branding + remove self-update (Layer 8/9) |
| `extension/Addons.xcu` | label `WriterAgent` → `OfficeLabs AI` (case-sensitive sed; the only capital-W occurrence is the visible menu title; dispatch URLs `org.extension.writeragent:*` untouched) | Layer 9 — rebrand visible UI only |
| `extension/registry/org/openoffice/Office/UI/Sidebar.xcu` | deck title `WriterAgent` → `OfficeLabs AI` (deck id `WriterAgentDeck` unchanged) | Layer 9 |
| `extension/Dialogs/EvalDialog.xdl` | window title → "OfficeLabs AI Evaluation Dashboard" | Layer 9 |

The remaining "WriterAgent" occurrences in the OXT are internal identifiers
(`org.extension.writeragent.*`, `WriterAgentDeck`, service names in
`Accelerators.xcu`, `Jobs.xcu`, `ProtocolHandler.xcu`, TypeDetection
registrations, `idl`, `*.rdb`) and the GPL attribution in
`registration/license.txt`. They are intentionally preserved for upstream
syncability (Layer 9: "Internal service IDs can remain WriterAgent until a
later deliberate migration").

## 4. Deliberately not changed

- `plugin/` Python code: untouched (UNO threading invariants, queue/drain
  architecture, tool registry, config system, startup job — all upstream).
- `Jobs.xcu`, `ProtocolHandler.xcu`, `Accelerators.xcu`, MCP implementation,
  tool registry: untouched.
- No LibrePy.oxt / LibreHarper.oxt bundles: the full WriterAgent variant
  contains that functionality (upstream "one OXT at a time" rule).
- The debug/eval menu is removed by the upstream release build
  (`build_oxt.py --no-tests` strips the M17 Debug node), not by a local edit.

## 5. Update discipline

Run `python bin/update-writeragent-upstream.py <sha>` for a new pinned
revision. The script refuses non-SHA input, verifies the checkout, preserves
all section-2 paths, and reports any vendored file that differs from pristine
upstream (a conflict requiring review). It never commits and never resolves
conflicts silently. Upstream changes that touch `officelabs-build/` overlays
are printed as "OfficeLabs-overlaid files changed upstream" and require a
manual check of the overlay.