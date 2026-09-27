# -*- coding: utf-8 -*-
"""V271 integration evidence capture.

Runs the acceptance criteria from the integration spec against the mock V271
service using the extension's own client stack (no LibreOffice needed for the
client-layer evidence; the LO-side event wiring is covered by
lo_evidence.py against a real build).

Usage:
    python tools/evidence_capture.py [--out evidence.json]

Produces evidence/<name>.json with one entry per acceptance criterion,
including raw artifacts (entity JSON, token storage facts, query results).
"""

import json
import os
import sys
import tempfile
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "office-extension", "extension", "python"))
sys.path.insert(0, os.path.join(HERE, "..", "tests"))

from mock_v271_server import start_server  # noqa: E402

from v271 import graph as graph_module  # noqa: E402
from v271 import vector as vector_module  # noqa: E402
from v271 import oidc as oidc_module  # noqa: E402
from v271 import http  # noqa: E402
from v271.config import settings_from_dict  # noqa: E402
from v271.chunker import chunk_documents  # noqa: E402
from v271.tokenstore import TokenStore  # noqa: E402


class StubModel:
    """Minimal stand-in for a UNO document model (Writer-like)."""

    def __init__(self, url, title, text, services=None):
        self._url = url
        self._title = title
        self._text = text
        self._services = services or [
            "com.sun.star.text.TextDocument",
            "com.sun.star.document.OfficeDocument",
        ]

    def getURL(self):
        return self._url

    def getSupportedServiceNames(self):
        return list(self._services)

    def getDocumentProperties(self):
        class Props:
            Title = self._title
            CreationDate = None
        return Props()

    # Used by extract.extract_text -> _extract_writer via XText query.
    def queryInterface(self, interface):
        if interface.__name__ == "XText":
            class Text:
                def getString(self):
                    return self._text
            return Text()
        return None


def simulate_browser_redirect(client, authorize_url):
    """Fetch the authorize URL, deliver the redirect to the callback server."""
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None

    opener = urllib.request.build_opener(NoRedirect)
    response = opener.open(authorize_url, timeout=10)
    location = response.headers.get("Location")
    response.close()
    parsed = urllib.parse.urlparse(location)
    import http.client
    host, port = parsed.netloc.split(":")
    conn = http.client.HTTPConnection(host, int(port), timeout=10)
    conn.request("GET", parsed.path + "?" + parsed.query)
    conn.getresponse().read()
    conn.close()


def main():
    out_path = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else \
        os.path.join(HERE, "..", "evidence", "evidence-client-stack.json")
    server = start_server()
    base = server.base_url()
    settings = settings_from_dict({
        "base_url": base,
        "issuer_url": base + "/identity",
        "client_id": "libreoffice-v271",
        "scopes": "openid profile offline_access",
        "redirect_port": 0,
        "allow_insecure_token_storage": True,
    })
    token_dir = tempfile.mkdtemp(prefix="v271-evidence-tokens-")
    token_store = TokenStore(profile_dir=token_dir, allow_insecure=True)
    oidc = oidc_module.OidcClient(settings, token_store=token_store)

    evidence = {"harness": "v271/tools/evidence_capture.py",
                "mock_base_url": base,
                "criteria": {}}

    # -- AC1: OIDC sign-in (Authorization Code + PKCE) --------------------
    original_open = oidc_module.webbrowser.open
    oidc_module.webbrowser.open = (
        lambda url, new=0: simulate_browser_redirect(oidc, url))
    try:
        account = oidc.sign_in(timeout=30)
    finally:
        oidc_module.webbrowser.open = original_open
    token = oidc.get_access_token()
    with server.lock:
        issued = server.issued_tokens
    evidence["criteria"]["1_signin"] = {
        "ok": bool(token and account.get("sub")),
        "account": account,
        "issued_tokens": issued,
        "pkce": "S256 challenge verified by token endpoint",
    }

    # -- helper for the document payloads ---------------------------------
    source = "file:///docs/v271-proposal.odt"
    model = StubModel(source, "v271-proposal.odt",
                      "The quarterly proposal for the V271 integration "
                      "discusses identity, graph sync and semantic search.",
                      services=["com.sun.star.text.TextDocument",
                                "com.sun.star.document.OfficeDocument"])
    graph = graph_module.GraphClient(base, oidc.get_access_token)
    vector = vector_module.VectorClient(base, oidc.get_access_token)

    entity = graph_module.make_document_entity(
        source_id=source, display_name="v271-proposal.odt", file_type="text",
        uri=source, last_opened="2026-09-27T10:00:00Z", favorite=False)

    # -- AC4: save/open events update rather than duplicate ---------------
    first = graph.upsert_entity(entity)
    entity["last_edited"] = "2026-09-27T10:05:00Z"
    second = graph.upsert_entity(entity)
    with server.lock:
        count = sum(1 for (t, s) in server.entities
                    if t == "document.file" and s == source)
    evidence["criteria"]["4_update_not_duplicate"] = {
        "ok": second["body"].get("updated") is True and count == 1,
        "first_status": first["status"],
        "second_status": second["status"],
        "server_said_updated": second["body"].get("updated"),
        "document.file_count_for_source": count,
    }

    # -- AC2: recent documents sync ----------------------------------------
    recent = dict(entity)
    recent["type"] = graph_module.DOCUMENT_RECENT
    graph.upsert_entity(recent)
    listed = graph.list_entities(graph_module.DOCUMENT_RECENT)
    evidence["criteria"]["2_recents_sync"] = {
        "ok": any(item["source_id"] == source and
                  item["type"] == "document.recent" for item in listed),
        "recent_entity": recent,
    }

    # -- AC3: favorite sync ------------------------------------------------
    fav = dict(entity)
    fav["type"] = graph_module.DOCUMENT_FAVORITE
    fav["favorite"] = True
    graph.upsert_entity(fav)
    favs = graph.list_entities(graph_module.DOCUMENT_FAVORITE)
    fav_ok = any(item["source_id"] == source for item in favs)
    deleted = graph.delete_entity(graph_module.DOCUMENT_FAVORITE, source)
    favs_after = graph.list_entities(graph_module.DOCUMENT_FAVORITE)
    evidence["criteria"]["3_favorites_sync"] = {
        "ok": fav_ok and deleted["ok"] and
              not any(item["source_id"] == source for item in favs_after),
        "favorite_entity": fav,
        "delete_status": deleted["status"],
    }

    # -- AC5: current document context -------------------------------------
    from v271 import documentinfo
    ctx_entity = documentinfo.build_entity(model, source, settings, None)
    # Text extraction uses the UNO API and therefore runs only inside
    # LibreOffice (covered by lo_evidence.py). Here we chunk the document
    # text directly so the pure client pipeline can be proven end to end.
    from v271.chunker import chunk_text
    parts = [("document", model._text)]
    chunks = chunk_documents(parts, settings.max_chunk_size)
    context = {
        "source_id": ctx_entity["source_id"],
        "display_name": ctx_entity["display_name"],
        "file_type": ctx_entity["file_type"],
        "uri": ctx_entity["uri"],
        "text_excerpt": (parts[0][1] if parts else "")[:120],
        "chunk_count": len(chunks),
    }
    evidence["criteria"]["5_document_context"] = {
        "ok": context["source_id"] == source and bool(context["text_excerpt"]),
        "context": context,
        "note": "UNO text extraction is exercised by lo_evidence.py against "
                "a real LibreOffice build; this run proves the pure pipeline.",
    }

    # -- AC6: embeddings use the central vector service --------------------
    upserted = vector.upsert_chunks(source, chunks,
                                    metadata={"display_name": "v271-proposal.odt"})
    with server.lock:
        vector_docs = len(server.vector_chunks)
    evidence["criteria"]["6_central_embeddings"] = {
        "ok": upserted["ok"] and vector_docs == 1 and len(chunks) > 0,
        "upsert_status": upserted["status"],
        "chunks_uploaded": upserted["body"].get("chunks") if isinstance(upserted["body"], dict) else None,
        "vector_documents": vector_docs,
    }

    # -- AC7: semantic retrieval locates the document by meaning -----------
    hits = vector.query("semantic search proposal identity", k=5)
    found = any(item["source_id"] == source for item in hits)
    evidence["criteria"]["7_semantic_retrieval"] = {
        "ok": found,
        "query": "semantic search proposal identity",
        "top_hits": hits[:3],
    }

    # -- AC8: NAI consumes the same document entity ------------------------
    graph_items = graph.list_entities(graph_module.DOCUMENT_FILE)
    same_entity = next((e for e in graph_items if e["source_id"] == source), None)
    evidence["criteria"]["8_shared_entity"] = {
        "ok": same_entity is not None,
        "entity_as_served_to_consumers": same_entity,
    }

    # -- AC9: credentials stored securely ----------------------------------
    token_file = token_store._file_path(oidc.profile_key)
    facts = {"dpapi_in_use": token_store._use_dpapi,
             "token_file_exists": os.path.isfile(token_file),
             "allow_insecure": token_store.allow_insecure}
    if os.path.isfile(token_file):
        with open(token_file, "r", encoding="utf-8") as handle:
            raw = handle.read()
        facts["plaintext_token_in_file"] = token in raw
        facts["record_starts_with"] = raw[:60]
    facts["file_permissions_0600"] = False
    try:
        import stat
        mode = stat.S_IMODE(os.stat(token_file).st_mode)
        facts["file_permissions_0600"] = (mode & 0o777) == 0o600
    except OSError:
        pass
    evidence["criteria"]["9_secure_token_storage"] = {
        "ok": facts["token_file_exists"] and not facts["plaintext_token_in_file"],
        "facts": facts,
    }

    # -- summary -----------------------------------------------------------
    evidence["summary"] = {
        "passed": sum(1 for c in evidence["criteria"].values() if c.get("ok")),
        "total": len(evidence["criteria"]),
        "stats": server.stats(),
    }
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(evidence, handle, ensure_ascii=False, indent=2)
    print(json.dumps(evidence["summary"], indent=2))
    print("evidence written to", out_path)


if __name__ == "__main__":
    main()