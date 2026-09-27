# -*- coding: utf-8 -*-
"""V271OfficeIntegration: the extension's runtime core.

Responsibilities:
- listen to document events globally (GlobalEventBroadcaster) and translate
  them into graph updates (open/save/close/favorite/workspace);
- debounce repeated save events per document;
- run network work on a background worker thread (never block the UI thread
  for graph/vector traffic);
- drive OIDC sign-in/out, status, sync, favorite, workspace and configure
  commands (called from the protocol handler on the main thread);
- seed document.recent entities from the native recent-documents list.

The graph is the authoritative mirror: entities are upserted by source_id,
so repeated events update existing entities instead of duplicating them.
"""

import json
import threading
import time

import unohelper

from com.sun.star.document import XDocumentEventListener

from . import (
    config as config_module,
    context as context_module,
    documentinfo,
    extract,
    favorites as favorites_module,
    graph as graph_module,
    logutil,
    oidc as oidc_module,
    recents as recents_module,
    vector as vector_module,
    workspace as workspace_module,
)
from .chunker import chunk_documents
from .dialogs import ask_text, show_message
from .unostrings import (
    EVENT_ON_CREATE,
    EVENT_ON_LOAD_FINISHED,
    EVENT_ON_NEW,
    EVENT_ON_PREPARE_UNLOAD,
    EVENT_ON_SAVE_AS_DONE,
    EVENT_ON_SAVE_DONE,
    EVENT_ON_UNLOAD,
    SERVICE_DESKTOP,
    SERVICE_GLOBAL_EVENT_BROADCASTER,
)
from .util import now_iso

_DEBOUNCE_LOAD = 1.0     # seconds before a load/open event is synced
_DEBOUNCE_SAVE = 1.5     # seconds before save events are synced (coalescing)
_SYNC_BATCH_WAIT = 0.25  # worker idle poll granularity


class _DocumentEventListener(unohelper.Base, XDocumentEventListener):
    """Receives global document events and forwards them to the integration."""

    def __init__(self, integration):
        self.integration = integration

    def documentEventOccured(self, event):
        try:
            self.integration.handle_event(event)
        except Exception as exc:
            logutil.error("documentEventOccured failed: %s" % exc)

    def disposing(self, source):
        pass


class V271Integration:
    _instance = None
    _instance_lock = threading.Lock()

    def __init__(self, ctx):
        self.ctx = ctx
        self._lock = threading.Lock()
        self._listener = None
        self._started = False
        self._worker = None
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._pending = {}     # source_id -> (due_epoch, payload)
        self._models = {}      # source_id -> model reference for text extraction
        self._oidc_cache = None
        self._last_sync_at = None
        self._last_error = None
        self._sync_count = 0

    # -- singleton ---------------------------------------------------------

    @classmethod
    def instance(cls, ctx=None):
        with cls._instance_lock:
            if cls._instance is None:
                if ctx is None:
                    import uno
                    ctx = uno.getComponentContext()
                cls._instance = cls(ctx)
            return cls._instance

    # -- lifecycle ---------------------------------------------------------

    def start(self):
        """Attach the global document-event listener and start the worker."""
        with self._lock:
            if self._started:
                return
            self._started = True
        try:
            broadcaster = self.ctx.getServiceManager().createInstanceWithContext(
                SERVICE_GLOBAL_EVENT_BROADCASTER, self.ctx)
            self._listener = _DocumentEventListener(self)
            broadcaster.addDocumentEventListener(self._listener)
            logutil.info("V271 integration started")
        except Exception as exc:
            logutil.error("could not attach document listener: %s" % exc)
            with self._lock:
                self._started = False
            return
        self._stop.clear()
        self._worker = threading.Thread(target=self._worker_loop,
                                        name="v271-sync", daemon=True)
        self._worker.start()
        if self._settings().sync_recents_on_start:
            self.queue_recents_sync()

    def shutdown(self):
        """Best-effort stop: flush pending local state, stop the worker."""
        with self._lock:
            self._started = False
            self._pending.clear()
            self._models.clear()
        self._stop.set()
        self._wake.set()
        if self._worker is not None:
            self._worker.join(timeout=2.0)
        logutil.info("V271 integration stopped")

    # -- settings / stores -------------------------------------------------

    def _settings(self):
        return config_module.settings_from_uno(self.ctx)

    def _store(self):
        return config_module.V271Store.from_uno(self.ctx)

    def _store_read(self):
        return config_module.V271Store.from_uno_read(self.ctx)

    def _oidc(self):
        settings = self._settings()
        if self._oidc_cache is None or \
                self._oidc_cache.settings.effective_issuer_url != settings.effective_issuer_url:
            self._oidc_cache = oidc_module.OidcClient(settings)
        return self._oidc_cache

    def _token_provider(self):
        def provider():
            try:
                if not self._oidc().is_signed_in():
                    return None
                return self._oidc().get_access_token()
            except Exception as exc:
                logutil.error("token provider failed: %s" % exc)
                return None
        return provider

    def _desktop(self):
        return self.ctx.getServiceManager().createInstanceWithContext(
            SERVICE_DESKTOP, self.ctx)

    def _current_model(self):
        try:
            return self._desktop().getCurrentComponent()
        except Exception:
            return None

    # -- document events ---------------------------------------------------

    def handle_event(self, event):
        try:
            name = event.EventName
            model = event.Source
        except Exception:
            return
        if name in (EVENT_ON_NEW, EVENT_ON_CREATE, EVENT_ON_LOAD_FINISHED):
            self._on_opened(model, name)
        elif name in (EVENT_ON_SAVE_DONE, EVENT_ON_SAVE_AS_DONE):
            self._on_saved(model, name)
        elif name in (EVENT_ON_PREPARE_UNLOAD, EVENT_ON_UNLOAD):
            self._on_closed(model)

    def _on_opened(self, model, event_name):
        payload = self._snapshot(model, opened=True)
        if payload is None:
            return
        self._queue(payload, delay=_DEBOUNCE_LOAD)

    def _on_saved(self, model, event_name):
        payload = self._snapshot(model, saved=True)
        if payload is None:
            return
        self._queue(payload, delay=_DEBOUNCE_SAVE, embed=True)

    def _on_closed(self, model):
        payload = self._snapshot(model, closed=True)
        if payload is None:
            return
        with self._lock:
            self._models.pop(payload["source_id"], None)
        self._queue(payload, delay=0.0)

    # -- snapshots ---------------------------------------------------------

    def _snapshot(self, model, opened=False, saved=False, closed=False):
        """Extract plain data from the model on the UI thread.

        The returned payload contains no UNO references except the optional
        'model' key used by the worker for text extraction.
        """
        try:
            url = documentinfo.document_url(model)
            settings = self._settings()
            store = self._store_read()
            entity = documentinfo.build_entity(
                model, url, settings, store,
                opened_at=None,
                edited_at=now_iso() if saved else None)
            return {
                "source_id": entity["source_id"],
                "url": url,
                "entity": entity,
                "saved": saved,
                "closed": closed,
                "embed": False,
                "model": model,
            }
        except Exception as exc:
            logutil.error("snapshot failed: %s" % exc)
            return None

    def _queue(self, payload, delay, embed=False):
        if payload is None:
            return
        source_id = payload["source_id"]
        if embed:
            payload["embed"] = True
        with self._lock:
            self._pending[source_id] = (time.time() + delay, payload)
            if payload.get("model") is not None:
                self._models[source_id] = payload["model"]
        self._wake.set()

    # -- worker ------------------------------------------------------------

    def _worker_loop(self):
        while not self._stop.is_set():
            now = time.time()
            due = []
            with self._lock:
                for source_id, (due_at, payload) in list(self._pending.items()):
                    if due_at <= now:
                        due.append((source_id, payload))
                        del self._pending[source_id]
            for source_id, payload in due:
                self._process(payload)
                with self._lock:
                    self._models.pop(source_id, None)
            if self._pending:
                _, (next_due, _) = min(self._pending.items(),
                                       key=lambda item: item[1][0])
                wait = max(0.0, next_due - time.time())
                self._wake.wait(timeout=min(wait, 5.0))
            else:
                self._wake.wait(timeout=_SYNC_BATCH_WAIT)
            self._wake.clear()

    def _process(self, payload):
        settings = self._settings()
        if not settings.is_configured():
            logutil.debug("sync skipped: V271 not configured")
            return
        entity = payload["entity"]
        source_id = payload["source_id"]
        token = self._token_provider()
        try:
            graph = graph_module.GraphClient(settings.api_base, token)
            if payload.get("favorite_removed"):
                # Unfavoriting deletes the mirror so the graph never keeps
                # stale document.favorite entities.
                result = graph.delete_entity(graph_module.DOCUMENT_FAVORITE, source_id)
                self._record_sync(ok=result.get("ok"), detail=result)
                return
            results = graph.sync_document(
                entity, recents=bool(payload.get("url")),
                favorite=bool(entity.get("favorite")))
            ok = all(r.get("ok") for r in results)
            if not ok:
                self._last_error = "graph upsert status: %s" % [
                    r.get("status") for r in results]
                logutil.warn(self._last_error)
            if settings.embed_on_save and payload.get("embed") and payload.get("url"):
                self._embed(source_id, entity, settings, token)
            self._record_sync(ok=ok, detail=results)
            logutil.info("synced %s (%d results)" % (source_id, len(results)))
        except Exception as exc:
            self._last_error = str(exc)
            logutil.error("sync failed for %s: %s" % (source_id, exc))

    def _record_sync(self, ok, detail=None):
        with self._lock:
            self._sync_count += 1
            self._last_sync_at = time.time()
            if ok:
                self._last_error = None

    def _embed(self, source_id, entity, settings, token):
        """Extract -> chunk -> central vector service (worker thread)."""
        model = None
        with self._lock:
            model = self._models.get(source_id)
        if model is None:
            return
        try:
            extracted = extract.extract_text(model).parts
            if not extracted:
                return
            chunks = chunk_documents(extracted, settings.max_chunk_size)
            if not chunks:
                return
            client = vector_module.VectorClient(settings.api_base, token)
            result = client.upsert_chunks(
                source_id, chunks,
                metadata={"display_name": entity.get("display_name"),
                          "workspace_id": entity.get("workspace_id")})
            if not result.get("ok"):
                logutil.warn("embed failed for %s: HTTP %s"
                             % (source_id, result.get("status")))
            else:
                logutil.info("embedded %d chunks for %s" % (len(chunks), source_id))
        except Exception as exc:
            logutil.error("embed failed for %s: %s" % (source_id, exc))

    # -- recents seeding ---------------------------------------------------

    def queue_recents_sync(self):
        settings = self._settings()
        if not settings.is_configured():
            return
        items = list(recents_module.iter_history(self.ctx, limit=50))
        if not items:
            return

        def worker():
            token = self._token_provider()
            graph = graph_module.GraphClient(settings.api_base, token)
            for item in items:
                entity = graph_module.make_document_entity(
                    source_id=item["url"],
                    display_name=item.get("title") or item["url"],
                    file_type=_filter_to_type(item.get("filter_name", "")),
                    uri=item["url"],
                    favorite=bool(item.get("pinned")),
                    entity_type=graph_module.DOCUMENT_RECENT,
                )
                try:
                    graph.upsert_entity(entity)
                except Exception as exc:
                    logutil.debug("recents seed failed for %s: %s" % (item["url"], exc))

        threading.Thread(target=worker, name="v271-recents", daemon=True).start()

    # -- commands (called on the main thread) ------------------------------

    def command_signin(self):
        settings = self._settings()
        if not settings.is_configured():
            show_message(self.ctx, "V271 sign-in",
                         "V271 is not configured yet.\nUse 'Configure…' to set the "
                         "V271 Base URL first.")
            return
        try:
            account = self._oidc().sign_in()
        except oidc_module.OidcError as exc:
            show_message(self.ctx, "V271 sign-in", "Sign-in failed:\n%s" % exc,
                         kind="error")
            return
        except Exception as exc:
            logutil.error("sign-in crashed: %s" % exc)
            show_message(self.ctx, "V271 sign-in",
                         "Sign-in failed unexpectedly: %s" % exc, kind="error")
            return
        name = account.get("name") or account.get("preferred_username") or \
            account.get("email") or account.get("sub") or "unknown"
        store = self._store()
        store.set_state("AccountDisplayName", name)
        show_message(self.ctx, "V271 sign-in",
                     "Signed in as %s." % name)
        self.queue_recents_sync()
        self.command_syncnow(silent=True)

    def command_signout(self):
        try:
            self._oidc().sign_out()
        except Exception as exc:
            logutil.error("sign-out failed: %s" % exc)
        store = self._store()
        store.set_state("AccountDisplayName", "")
        show_message(self.ctx, "V271", "Signed out. Local tokens were removed.")

    def command_status(self):
        oidc = self._oidc()
        settings = self._settings()
        store = self._store()
        lines = []
        if not settings.is_configured():
            lines.append("V271 is not configured (Base URL empty).")
        else:
            lines.append("Base URL: %s" % settings.api_base)
            account = store.get_state("AccountDisplayName", "")
            if oidc.is_signed_in():
                lines.append("Account: %s" % (account or "signed in"))
            else:
                lines.append("Account: not signed in")
        with self._lock:
            last = self._last_sync_at
            count = self._sync_count
            error = self._last_error
        if last:
            lines.append("Last sync: %s (%d syncs)"
                         % (time.strftime("%H:%M:%S", time.localtime(last)), count))
        if error:
            lines.append("Last error: %s" % error)
        with self._lock:
            pending = len(self._pending)
        lines.append("Pending syncs: %d" % pending)
        show_message(self.ctx, "V271 status", "\n".join(lines))

    def command_syncnow(self, silent=False):
        model = self._current_model()
        if model is not None:
            payload = self._snapshot(model, opened=True)
            if payload is not None:
                self._queue(payload, delay=0.0)
        self.queue_recents_sync()
        if not silent:
            show_message(self.ctx, "V271",
                         "Sync queued (current document and recent list).")

    def command_toggle_favorite(self):
        model = self._current_model()
        if model is None:
            show_message(self.ctx, "V271", "No document is open.", kind="warning")
            return
        settings = self._settings()
        store = self._store()
        url = documentinfo.document_url(model)
        entity = documentinfo.build_entity(model, url, settings, store)
        new_state, changed = favorites_module.toggle_favorite(store, entity)
        if changed:
            payload = {"source_id": entity["source_id"], "url": url,
                       "entity": entity, "saved": False, "closed": False,
                       "embed": False, "model": None,
                       "favorite_removed": not new_state}
            self._queue(payload, delay=0.0)
            show_message(self.ctx, "V271",
                         "Favorite %s for '%s'."
                         % ("set" if new_state else "removed",
                            entity["display_name"]))
        else:
            show_message(self.ctx, "V271", "No change.")

    def command_link_workspace(self):
        model = self._current_model()
        if model is None:
            show_message(self.ctx, "V271", "No document is open.", kind="warning")
            return
        store = self._store()
        current = workspace_module.current_workspace(store)
        initial = current["id"] if current else ""
        workspace_id = ask_text(self.ctx, "V271 — link workspace",
                                "Workspace id (V271/NAI):", initial)
        if workspace_id is None:
            return
        workspace_id = workspace_id.strip()
        name = None
        if workspace_id:
            name = ask_text(self.ctx, "V271 — link workspace",
                            "Workspace display name (optional):", workspace_id)
            name = (name or "").strip() or None
        settings = self._settings()
        url = documentinfo.document_url(model)
        entity = documentinfo.build_entity(model, url, settings, store)
        entity = workspace_module.link_document(store, entity, workspace_id, name)
        if workspace_id:
            workspace_module.set_current_workspace(store, workspace_id, name)
        self._queue({"source_id": entity["source_id"], "url": url,
                     "entity": entity, "saved": False, "closed": False,
                     "embed": False, "model": None}, delay=0.0)
        show_message(self.ctx, "V271",
                     "Document linked to workspace %r." % workspace_id)

    def command_configure(self):
        settings = self._settings()
        base_url = ask_text(self.ctx, "V271 — configure",
                            "V271 Base URL (e.g. https://v271.example.invalid):",
                            settings.api_base)
        if base_url is None:
            return
        base_url = base_url.strip().rstrip("/")
        store = self._store()
        if store.uses_uno:
            try:
                update = store._update
                general = update.getByName("General")
                from com.sun.star.beans import XPropertySet
                general.queryInterface(XPropertySet).setPropertyValue(
                    "BaseUrl", base_url)
                update.commitChanges()
            except Exception as exc:
                logutil.error("configure write failed: %s" % exc)
                show_message(self.ctx, "V271", "Could not save configuration: %s"
                             % exc, kind="error")
                return
        else:
            logutil.warn("configure: no update access (headless run)")
        show_message(self.ctx, "V271", "Base URL saved: %s" % base_url)

    def command_show_context(self):
        model = self._current_model()
        if model is None:
            show_message(self.ctx, "V271", "No document is open.", kind="warning")
            return
        settings = self._settings()
        store = self._store()
        context = context_module.build_context(model, settings, store)
        compact = {k: v for k, v in context.items() if k != "text_excerpt"}
        text = json.dumps(compact, ensure_ascii=False, indent=2)
        if len(text) > 1800:
            path = self._dump_context(context)
            show_message(self.ctx, "V271 — document context",
                         "Context written to:\n%s\n\nsource_id: %s"
                         % (path, context["source_id"]))
        else:
            show_message(self.ctx, "V271 — document context", text)

    def _dump_context(self, context):
        import os
        import tempfile
        path = os.path.join(tempfile.gettempdir(),
                            "v271-context-%s.json" % context["source_id"][:16])
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(context, handle, ensure_ascii=False, indent=2)
        return path


def _filter_to_type(filter_name):
    lowered = (filter_name or "").lower()
    if "word" in lowered or "writer" in lowered:
        return "text"
    if "excel" in lowered or "calc" in lowered:
        return "spreadsheet"
    if "powerpoint" in lowered or "impress" in lowered:
        return "presentation"
    if "draw" in lowered:
        return "drawing"
    return "other"