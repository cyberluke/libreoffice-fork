# -*- coding: utf-8 -*-
"""LO-side evidence capture for the V271 integration.

Drives a real LibreOffice build over UNO to exercise the extension's
event wiring end to end (open/save/favorite/context/extraction), against the
mock V271 service. Run after installing the extension:

  1. Build LibreOffice and start it headless with the UNO socket:
       soffice --headless --norestore --accept=socket,host=127.0.0.1,port=2002;urp; &

  2. Install the extension into that profile (first run):
       unopkg add --shared v271-office.oxt        # or per-user: unopkg add

  3. Point the extension at the mock service (env vars are read at runtime):
       V271_BASE_URL=http://127.0.0.1:<mockport> python tools/lo_evidence.py

  4. The script creates/saves/favorites a document, waits for the debounced
     sync, then verifies the mock server state and writes
     evidence/evidence-lo-events.json.

Requires the 'uno' Python module (use the python bundled with the build).
"""

import json
import os
import sys
import tempfile
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "evidence", "evidence-lo-events.json")

SOCKET = os.environ.get("V271_LO_SOCKET", "uno:socket,host=127.0.0.1,port=2002;urp;StarOffice.ComponentContext")
MOCK_URL = os.environ.get("V271_BASE_URL", "http://127.0.0.1:5555")


def connect():
    import uno
    from com.sun.star.beans import PropertyValue

    local_context = uno.getComponentContext()
    resolver = local_context.ServiceManager.createInstanceWithContext(
        "com.sun.star.bridge.UnoUrlResolver", local_context)
    ctx = resolver.resolve(SOCKET)
    smgr = ctx.ServiceManager
    desktop = smgr.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
    return ctx, desktop


def pv(name, value):
    from com.sun.star.beans import PropertyValue
    return PropertyValue(name, 0, value, 0)


def wait_for(predicate, timeout=20.0, interval=0.5):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(interval)
    return False


def mock_stats():
    with urllib.request.urlopen(MOCK_URL + "/_stats", timeout=5) as response:
        return json.loads(response.read().decode("utf-8"))


def main():
    if not os.environ.get("V271_BASE_URL"):
        print("WARNING: V271_BASE_URL not set; extension may not sync. "
              "Start the mock first: python tests/mock_v271_server.py")
    ctx, desktop = connect()
    evidence = {"harness": "v271/tools/lo_evidence.py", "criteria": {}}

    # Create and save a Writer document (triggers OnNew + OnSaveDone events).
    doc = desktop.loadComponentFromURL("private:factory/swriter", "_blank", 0, ())
    text = doc.getText()
    cursor = text.createTextCursor()
    text.insertString(cursor, "V271 integration evidence document "
                              "identity graph semantic retrieval.\n", 0)

    tmp_dir = tempfile.mkdtemp(prefix="v271-lo-evidence-")
    path = os.path.join(tmp_dir, "evidence-doc.odt")
    from com.sun.star.beans import PropertyValue
    props = (PropertyValue("FilterName", 0, "writer8", 0),)
    url = "file:///" + path.replace("\\", "/")
    doc.storeToURL(url, props)
    doc.store()  # plain save -> OnSaveDone again

    # Wait for the debounced graph sync to reach the mock.
    synced = wait_for(lambda: mock_stats().get("distinct_documents", 0) >= 1,
                      timeout=30)
    stats = mock_stats()
    evidence["criteria"]["events_synced"] = {
        "ok": synced,
        "stats_after_save": stats,
    }

    # Toggle favorite through the extension's protocol URL.
    frame = desktop.getCurrentFrame()
    helper = frame.getDispatchHelper()
    helper.executeDispatch(frame, "vnd.v271.office:favorite", "", 0, ())
    time.sleep(2)
    stats = mock_stats()
    fav_types = [key for key in stats.get("entity_keys", [])
                 if key and key[0] == "document.favorite"]
    evidence["criteria"]["favorite_via_dispatch"] = {
        "ok": len(fav_types) == 1,
        "favorite_entities": fav_types,
    }

    # Typed context via the DocumentContext service.
    try:
        service = ctx.ServiceManager.createInstanceWithContext(
            "com.v271.office.DocumentContext", ctx)
        props_out = service.execute((PropertyValue("Command", 0, "context", 0),))
        context = {p.Name: p.Value for p in props_out}
        evidence["criteria"]["document_context_service"] = {
            "ok": bool(context.get("source_id")) and bool(context.get("text_excerpt")),
            "context": {k: v for k, v in context.items() if k != "text_excerpt"},
            "text_excerpt_chars": len(context.get("text_excerpt", "")),
        }
    except Exception as exc:
        evidence["criteria"]["document_context_service"] = {
            "ok": False, "error": str(exc)}

    # Semantic retrieval must locate the saved document.
    try:
        from v271.config import settings_from_uno
        from v271 import vector as vector_module
        settings = settings_from_uno(ctx)
        client = vector_module.VectorClient(settings.api_base)
        hits = client.query("identity graph semantic retrieval", k=5)
        found = any("evidence-doc" in (h.get("source_id") or "") for h in hits)
        evidence["criteria"]["semantic_retrieval"] = {
            "ok": found, "hits": hits[:3]}
    except Exception as exc:
        evidence["criteria"]["semantic_retrieval"] = {"ok": False, "error": str(exc)}

    # Cleanup
    try:
        doc.close(False)
    except Exception:
        pass

    evidence["summary"] = {
        "passed": sum(1 for c in evidence["criteria"].values() if c.get("ok")),
        "total": len(evidence["criteria"]),
        "final_stats": mock_stats(),
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as handle:
        json.dump(evidence, handle, ensure_ascii=False, indent=2)
    print(json.dumps(evidence["summary"], indent=2))
    print("evidence written to", OUT)


if __name__ == "__main__":
    main()