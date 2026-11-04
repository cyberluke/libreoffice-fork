# NAI Office

**Your documents. Native office engines. AI assistance where it belongs: inside the workflow.**

**NAI Office** is NANOTRIK's document productivity product built from a **real LibreOffice source fork**, not a web imitation or an Office extension masquerading as an entire suite. It retains LibreOffice's C++ document core, UNO APIs and established document-format machinery while adding native AI integration surfaces and a separate V271 document-context integration.

NAI Office is part of the **[NAI software ecosystem](https://github.com/cyberluke/NAI-OS)**. The NAI OS repository contains the Windows shell and integration surface; **this repository is the office application's source tree**.

**Status:** active engineering source fork. Features described here have concrete source paths, but this README does not promise a finished public installer, a hosted AI service or that every proposed cross-app contract is already shipping.

## What lives in this repository

| Layer | Role | Source |
| --- | --- | --- |
| **LibreOffice document engines** | Writer, Calc, Impress/Draw, document models, formatting, import/export and UNO. Native C++ and supporting platform code. | [`sw/`](sw/), [`sc/`](sc/), [`sd/`](sd/), [`sfx2/`](sfx2/), [`vcl/`](vcl/), [`framework/`](framework/) |
| **OfficeLabs native integration** | Additional C++ host module for AI UI embedding, document-controller interactions, inline completion/rewrite and agent-transport boundaries. `OfficeLabs` is an internal implementation name retained in this fork. | [`officelabs/`](officelabs/) |
| **AI webview bridge** | Conditional Chromium Embedded Framework (CEF) host, message bridge, security/consent and trust boundaries. Requires the configured build/runtime dependencies. | [`officelabs/source/`](officelabs/source/), [`officelabs/Module_officelabs.mk`](officelabs/Module_officelabs.mk) |
| **V271 document integration** | UNO extension for OIDC Authorization Code + PKCE, document identity, recent/favorite metadata, document-context extraction and graph/vector-service integration. | [`v271/`](v271/), [`v271/README.md`](v271/README.md) |
| **Upstream-sync engineering** | Tracks changes to inherited LibreOffice files, native build scripts and intentionally modified integration points to reduce merge regressions. | [`upstream-patches.list`](upstream-patches.list), [`upstream-sync.sh`](upstream-sync.sh) |

## The office experience

**Edit real documents.** The native office suite provides the actual document model, formatting engine and file compatibility. AI features are added around the existing document context and editing model rather than replacing it with a simplified editor.

**Work in context.** The OfficeLabs module contains dedicated native code for document interaction, inline completion, rewrites, CEF webview hosting, request transport and consent boundaries. These source components are not a claim that an AI backend is bundled or universally available out of the box.

**Connect documents to the wider NAI environment when configured.** The `v271/` extension implements an explicit identity and document-metadata path, with configurable V271 services. Document extraction and embeddings use a central vector service where this integration is enabled. The presence of that path does **not** imply all document contents remain offline or are automatically shared: inspect configuration, authorization and service endpoints for the actual deployment.

**Keep a shared protocol, not duplicated AI sidebars.** A canonical cross-application chat component is described in [`v271/CHAT_AI_CANONICAL_SPEC.md`](v271/CHAT_AI_CANONICAL_SPEC.md). **That document explicitly labels itself a specification.** Its planned portable deployment is not represented here as an implemented feature of every NAI application.

## Architecture

```text
NAI Office (this repository)
│
├── LibreOffice core (C++ / UNO / VCL / sfx2)
│   ├── sw/                  Writer
│   ├── sc/                  Calc
│   ├── sd/                  Draw / Impress
│   └── framework/           menus, toolbars, runtime structure
│
├── officelabs/              NAI/OfficeLabs native host module
│   ├── source/              CEF, document controller, inline and rewrite
│   ├── inc/                 native interfaces
│   └── writeragent/         optional bundled agent extension assets
│
└── v271/                    optional V271 UNO integration
    ├── office-extension/    OIDC + document graph/context code
    ├── tests/               extension tests
    └── evidence/            recorded extension acceptance evidence
```

The fork remains a **full LibreOffice tree**, with multiple build targets and native/runtime prerequisites. Integrations deliberately have separate build and verification contracts.

## Build and verification

For the upstream engine's supported build chains and prerequisites, start with the **[preserved LibreOffice technical README](README.LIBREOFFICE-UPSTREAM.md)** and the official [LibreOffice developer documentation](https://wiki.documentfoundation.org/Development/How_to_build).

For this fork's additional integration surfaces:

- [`officelabs/Module_officelabs.mk`](officelabs/Module_officelabs.mk) registers the native OfficeLabs module. CEF targets are conditional on `ENABLE_CEF`; the WriterAgent extension target is conditional on `OFFICELABS_AI`.
- [`v271/README.md`](v271/README.md) documents the V271 UNO extension's build, configuration, test commands, evidence expectations and backend contract.
- [`upstream-patches.list`](upstream-patches.list) and [`upstream-sync.sh`](upstream-sync.sh) define the fork's upstream merge-discipline.
- [`build-sandbox.sh`](build-sandbox.sh) and [`test-build-sandbox.sh`](test-build-sandbox.sh) provide specialized build-sandbox tooling for the existing engineering environment.

**Verification statement:** README-only changes have not been built or tested. Historical test counts, backend availability and current installer readiness must be verified in the target environment before they become release claims. Do not use upstream LibreOffice release badges as evidence of NAI Office CI status.

## Ecosystem boundaries

NAI Office is one independent NAI application, alongside [NAI OS](https://github.com/cyberluke/NAI-OS), PaintBrush, Mail, Browser and other products developed in separate public or private repositories. The office document engine, embedded AI UI, model runtime and NAI desktop shell are not interchangeable components; integration should be claimed only where its particular transport and runtime exist.

## LibreOffice heritage, attribution and licenses

This fork inherits the LibreOffice codebase created and maintained by **[The Document Foundation](https://www.documentfoundation.org/)** and the upstream contributor community. The original inherited README has been preserved **unchanged** as [`README.LIBREOFFICE-UPSTREAM.md`](README.LIBREOFFICE-UPSTREAM.md). It provides the original developer/module map and upstream links.

The repository contains multiple licensing notices, including [`COPYING`](COPYING), [`COPYING.LGPL`](COPYING.LGPL) and [`COPYING.MPL`](COPYING.MPL), together with licenses for individual components and dependencies. The NAI Office branding does not change these upstream obligations or imply endorsement from The Document Foundation.
