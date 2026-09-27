# -*- coding: utf-8 -*-
"""Document identity and entity building.

Identity rule: the source_id is the canonical local/cloud URI reference
(never the mutable display name). Unsaved documents get a session-scoped
'unsaved:' id which is replaced by the file URI as soon as the document is
saved once.
"""

import hashlib

from . import fingerprint as fp_module
from . import logutil
from .util import now_iso, safe_str

_FILE_TYPES = {
    "com.sun.star.text.TextDocument": "text",
    "com.sun.star.text.WebDocument": "text",
    "com.sun.star.sheet.SpreadsheetDocument": "spreadsheet",
    "com.sun.star.presentation.PresentationDocument": "presentation",
    "com.sun.star.drawing.DrawingDocument": "drawing",
}

_MODULE_NAMES = {
    "com.sun.star.text.TextDocument": "Writer",
    "com.sun.star.text.WebDocument": "Writer/Web",
    "com.sun.star.sheet.SpreadsheetDocument": "Calc",
    "com.sun.star.presentation.PresentationDocument": "Impress",
    "com.sun.star.drawing.DrawingDocument": "Draw",
    "com.sun.star.formula.FormulaProperties": "Math",
}


def document_service_name(model):
    """Canonical document service name, e.g. com.sun.star.text.TextDocument."""
    try:
        services = model.getSupportedServiceNames()
        for candidate in _FILE_TYPES:
            if candidate in services:
                return candidate
    except Exception:
        pass
    return None


def module_name(model):
    """Best-effort module name (Writer/Calc/Impress/Draw/Math/Other)."""
    try:
        services = model.getSupportedServiceNames()
        for candidate in _MODULE_NAMES:
            if candidate in services:
                return _MODULE_NAMES[candidate]
    except Exception:
        pass
    return "Other"


def file_type(model):
    try:
        services = model.getSupportedServiceNames()
        for candidate, ftype in _FILE_TYPES.items():
            if candidate in services:
                return ftype
    except Exception:
        pass
    return "other"


def document_url(model):
    """Canonical URL of the document ('' when unsaved)."""
    try:
        return model.getURL() or ""
    except Exception:
        return ""


def display_name(model, url):
    """Prefer the URL's last path segment; fall back to document properties."""
    if url:
        segment = url.rstrip("/").rsplit("/", 1)[-1]
        if segment:
            from urllib.parse import unquote
            return unquote(segment)
    try:
        props = model.getDocumentProperties()
        title = props.Title
        if title:
            return title
    except Exception:
        pass
    return "Untitled document"


def _unsaved_source_id(model):
    material = "unsaved"
    try:
        props = model.getDocumentProperties()
        material += "|" + safe_str(props.Title)
        creation = props.CreationDate
        if creation is not None:
            material += "|%04d-%02d-%02dT%02d:%02d:%02d" % (
                creation.Year, creation.Month, creation.Day,
                creation.Hours, creation.Minutes, creation.Seconds)
    except Exception:
        pass
    return "unsaved:" + hashlib.sha1(material.encode("utf-8")).hexdigest()[:24]


def source_id(model, url):
    """Stable identity: file/cloud URL when available, else unsaved: id."""
    if url:
        return url
    return _unsaved_source_id(model)


def fingerprint(model, url, settings):
    """Content fingerprint where safe (mode 'file', size-guarded)."""
    if not settings or settings.fingerprint_mode != "file":
        return None
    if not url:
        return None
    try:
        from urllib.parse import urlparse, unquote
        parsed = urlparse(url)
        if parsed.scheme != "file":
            return None
        path = unquote(parsed.path)
        if os_nt() and path.startswith("/") and len(path) > 3 and path[2] == ":":
            path = path[1:]  # /C:/... -> C:/...
        return fp_module.fingerprint_file(path, settings.max_fingerprint_bytes)
    except Exception as exc:
        logutil.debug("fingerprint skipped: %s" % exc)
        return None


def os_nt():
    import sys
    return sys.platform.startswith("win")


def build_entity(model, url, settings, store, opened_at=None, edited_at=None,
                 favorite=None, workspace_id=None, fingerprint_value=None,
                 entity_type=None):
    """Build a graph entity payload for a document model.

    All values are plain JSON-safe types; no UNO references are retained.
    """
    from .graph import DOCUMENT_FILE, make_document_entity
    source = source_id(model, url)
    fav = favorite
    if fav is None and store is not None:
        fav = store.get_favorite(source) is not None
    workspace = workspace_id
    if workspace is None and store is not None:
        workspace = store.current_workspace_id() or None
    if fingerprint_value is None:
        fingerprint_value = fingerprint(model, url, settings)
    metadata = {
        "module": module_name(model),
        "service": document_service_name(model) or "",
        "unsaved": not bool(url),
    }
    return make_document_entity(
        source_id=source,
        display_name=display_name(model, url),
        file_type=file_type(model),
        uri=url or None,
        last_opened=opened_at or now_iso(),
        last_edited=edited_at,
        favorite=bool(fav),
        workspace_id=workspace,
        fingerprint=fingerprint_value,
        metadata=metadata,
        entity_type=entity_type or DOCUMENT_FILE,
    )