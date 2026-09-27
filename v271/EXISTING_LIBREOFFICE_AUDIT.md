# Existing LibreOffice audit — integration points for V271

Audit of the V271 LibreOffice fork (upstream `LibreOffice/core` master,
commit `0dacab6ba`, 2026-09-27) performed for the V271 Identity, Graph and
Document Context integration. All paths verified against this tree.

## 1. Account framework

- **None.** LibreOffice core has no OIDC/OAuth account framework. Remote
  accounts exist only as file-access protocols:
  - WebDAV UCP: `ucb/source/ucp/webdav/`
  - CMIS UCP: `ucb/source/ucp/cmis/` (SharePoint/OpenCMIS)
  - "Online storage" providers (Google Drive, OneDrive, Nextcloud) are
    *separate* extensions outside core (`desktop` only references them via
    the provider protocol `vnd.sun.star.webdav` / config).
- **Implication:** the V271 OIDC client must be provided by the extension
  (see `python/v271/oidc.py`). No core account UI is touched, preserving
  upstream compatibility.

## 2. Recent-documents storage

- Config path: `org.openoffice.Office.Common/History/HistoryList`
  (`officecfg/registry/schema/org/openoffice/Office/Common.xcs`, group
  `History` at line 1509).
- Structure: `ItemList` (keyed by URL) + `OrderList` (ordered), template
  `HistoryItem` with `HistoryItemRef`, `Filter`, `Title`, `Thumbnail`,
  `ReadOnly`, and `Pinned` (added by tdf#155698).
- Access implementation: `unotools/source/config/historyoptions.cxx`
  (`SvtHistoryOptions::GetList`, `AppendItem`).
- Per-module histories exist too (`org.openoffice.Office.Writer/History`,
  `org.openoffice.Office.Calc/History`, ...), used by module-specific dialogs;
  the Start Center consumes the Common list.
- **Used by the extension:** `python/v271/recents.py` reads the Common
  ItemList/OrderList to seed `document.recent` graph entities. The graph is
  the authoritative mirror; no second local recents store is created.

## 3. Favorites / pins

- No native favorites UI. The closest primitive is the `Pinned` flag on
  history items (Start Center pinning, `historyoptions.cxx` lines 104, 158,
  224).
- **Used by the extension:** `python/v271/favorites.py` keeps the
  authoritative favorite list in the extension's own config node
  (`org.openoffice.Office.V271/Favorites`) and mirrors every change to
  `document.favorite` graph entities (upsert + delete on removal).

## 4. Document metadata APIs

- `com.sun.star.document.DocumentProperties` /
  `com.sun.star.document.XDocumentProperties` (offapi) — title, author,
  dates, keywords; exposed on every document via
  `com.sun.star.document.XDocumentPropertiesSupplier`.
- **Used by the extension:** `python/v271/documentinfo.py` reads the title
  for display purposes only; identity never relies on it.

## 5. Current AI integration

- None in core for document AI. The only network AI-like service is the
  LanguageTool remote grammar checker
  (`org.openoffice.Office.Linguistic/LanguageTool`, `cui/source/options/
  optlanguagetool.cxx`).
- **Implication:** the V271 AI bridge (summarize / semantic retrieval) is
  implemented in the extension against the central V271 services
  (`python/v271/context.py`, `python/v271/vector.py`); no local model is
  added.

## 6. Extension / plugin bridge points (preferred over core patches)

- `com.sun.star.task.Job` + `org.openoffice.Office.Jobs` event binding
  (schema `officecfg/registry/schema/org/openoffice/Office/Jobs.xcs`);
  event names in `unotools/source/config/eventcfg.cxx`
  (`OnStartApp`, `OnCloseApp`, `OnCreate`, `OnNew`, `OnLoadFinished`,
  `OnSave`, `OnSaveDone`, `OnSaveAs`, `OnSaveAsDone`, `OnPrepareUnload`,
  `OnUnload`, `OnModifyChanged`, ...).
- `com.sun.star.frame.ProtocolHandler` +
  `org.openoffice.Office.ProtocolHandler/HandlerSet`
  (schema `ProtocolHandler.xcs`; in-tree example `swext/mediawiki/src/
  registry/data/org/openoffice/Office/ProtocolHandler.xcu`).
- Add-ons menus: `org.openoffice.Office.Addons/AddonUI/OfficeMenuBar`
  (in-tree example `odk/examples/python/minimal-extension/Addons.xcu`).
- Extension configuration: an extension ships its own schema (`.xcs`) +
  data (`.xcu`) (`org.openoffice.Office.V271`), writable via
  `com.sun.star.configuration.ConfigurationUpdateAccess`.
- Python UNO components: pythonloader with
  `application/vnd.sun.star.uno-component;type=Python` manifest entries
  (in-tree example `odk/examples/python/minimal-extension/`).
- Set-element mutation pattern: free set element via the set's
  `XSingleServiceFactory` + `XNameContainer.insertByName` (canonical
  implementation `unotools/source/config/historyoptions.cxx` lines 261-287;
  configmgr `configmgr/source/access.cxx` `getFreeSetMember`).

## 7. File open/save events

- Global multiplexer: `com.sun.star.frame.GlobalEventBroadcaster`
  (implementation `sfx2/source/notify/globalevents.cxx`,
  registered in `sfx2/util/sfx.component` also as singleton
  `com.sun.star.frame.theGlobalEventBroadcaster`); implements
  `com.sun.star.document.XDocumentEventBroadcaster` and forwards
  `XDocumentEventListener::documentEventOccured(DocumentEvent)`.
- Per-document broadcasters: every `XModel` (SfxBaseModel,
  `sfx2/source/doc/sfxbasemodel.cxx` line 2597) implements
  `XDocumentEventBroadcaster`.
- Event names observed in this tree: `OnNew`, `OnCreate`,
  `OnLoadFinished`, `OnSave`, `OnSaveDone`, `OnSaveAs`, `OnSaveAsDone`,
  `OnPrepareUnload`, `OnUnload`, `OnModifyChanged`, `OnTitleChanged`, ...
- **Used by the extension:** `python/v271/integration.py` attaches one
  `XDocumentEventListener` to the GlobalEventBroadcaster and translates
  open/save/close events into debounced graph updates.

## 8. Document identity

- No stable document id is exposed by `DocumentProperties` in this tree
  version (checked `offapi/com/sun/star/document/DocumentProperties.idl`;
  only UCB transfer commands carry a `DocumentId`).
- **Used by the extension:** `python/v271/documentinfo.py` derives the
  `source_id` from the canonical local/cloud URI (never the mutable display
  name), falling back to a session-scoped `unsaved:<hash>` id until the
  document is saved once; a content fingerprint (`sha256`, size-guarded)
  is attached "where safe".

## 9. Collaboration / cloud storage support

- UCPs: WebDAV (`ucb/source/ucp/webdav/`), CMIS (`ucb/source/ucp/cmis/`),
  GIO, FTP, etc. Documents can live on remote URIs; the extension treats
  any URI (local or cloud) as the identity reference and leaves
  open/save transport to LibreOffice itself.
- **Implication:** V271 integration does not add a cloud shell; documents
  remain local/remote files managed by LibreOffice, and only metadata +
  extracted text mirror to the V271 graph/vector services.

## Conclusion

All V271 integration needs can be satisfied through native extension points
(listed in section 6) plus the global event broadcaster (section 7). No
invasive core patches are required; the fork's four default-configuration
changes (dark mode, Czech UI/locale/spell-check, MS save formats) are
isolated to `officecfg` defaults.