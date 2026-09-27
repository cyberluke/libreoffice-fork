# -*- coding: utf-8 -*-
"""Typed document context for V271 AI.

Builds a plain-dict context for the current document and provides the AI
bridge calls (summarize / related-document lookup) against the central V271
AI + vector services. No local model is used.
"""

from . import extract, logutil
from . import documentinfo
from . import vector as vector_module


def build_context(model, settings, store, include_text=True, text_limit=4000):
    """Return a context dict for a document model.

    Keys: source_id, display_name, file_type, uri, workspace_id, favorite,
    fingerprint, module, text_excerpt, opened_at.
    """
    url = documentinfo.document_url(model)
    entity = documentinfo.build_entity(model, url, settings, store)
    context = {
        "source_id": entity["source_id"],
        "display_name": entity["display_name"],
        "file_type": entity["file_type"],
        "uri": entity["uri"],
        "workspace_id": entity["workspace_id"],
        "favorite": entity["favorite"],
        "fingerprint": entity["fingerprint"],
        "module": entity["metadata"].get("module", "Other"),
        "opened_at": entity["last_opened"],
        "text_excerpt": "",
        "chunk_count": 0,
    }
    if include_text:
        parts = extract.extract_text(model).parts
        excerpt = "\n".join(text for _, text in parts)
        if len(excerpt) > text_limit:
            excerpt = excerpt[:text_limit] + "…"
        context["text_excerpt"] = excerpt
        context["chunk_count"] = len(parts)
    return context


def summarize(model, settings, token_provider, store):
    """Ask the V271 AI service to summarize the current document.

    Returns (summary_text, error_or_none).
    """
    if not settings.is_configured():
        return None, "V271 is not configured (set BaseUrl first)"
    context = build_context(model, settings, store)
    payload = {
        "source_id": context["source_id"],
        "display_name": context["display_name"],
        "text": context["text_excerpt"],
        "workspace_id": context["workspace_id"],
    }
    try:
        from . import http
        status, body = http.request_json(
            "POST", settings.ai_url() + "/summarize", payload=payload,
            bearer=(token_provider() if token_provider else None))
        if 200 <= status < 300 and isinstance(body, dict):
            return body.get("summary") or body.get("text") or "", None
        return None, "Summarize failed (HTTP %d)" % status
    except http.NetworkError as exc:
        return None, "Network error: %s" % exc
    except http.HttpError as exc:
        return None, str(exc)


def find_related(query, settings, token_provider, k=5):
    """Semantic retrieval over the V271 vector store.

    Returns a list of {source_id, display_name, score, snippet} (best effort).
    """
    if not settings.is_configured():
        return []
    client = vector_module.VectorClient(settings.api_base, token_provider)
    try:
        items = client.query(query, k=k)
    except Exception as exc:
        logutil.debug("vector query failed: %s" % exc)
        return []
    results = []
    for item in items or []:
        if not isinstance(item, dict):
            continue
        results.append({
            "source_id": item.get("source_id", ""),
            "display_name": item.get("display_name", item.get("source_id", "")),
            "score": item.get("score"),
            "snippet": item.get("snippet", ""),
        })
    return results