# -*- coding: utf-8 -*-
"""V271 vector pipeline client.

No local embedding model: extracted document text is chunked locally
(chunker.py) and uploaded to the central V271 vector service. Provenance
(page/slide/sheet/section) is preserved per chunk.

  POST {base}/api/v1/vector/upsert
  {
    "source_id": "...",
    "chunks": [ {"text": "...", "provenance": "slide 3"} ]
  }

  POST {base}/api/v1/vector/query
  { "query": "...", "k": 5 }
  -> { "items": [ {"source_id": "...", "score": 0.83, "snippet": "..."} ] }
"""

from . import http


class VectorClient:

    def __init__(self, base_url, token_provider=None):
        self.base_url = (base_url or "").rstrip("/")
        self.token_provider = token_provider

    def _bearer(self):
        if self.token_provider is None:
            return None
        return self.token_provider() or None

    def upsert_chunks(self, source_id, chunks, metadata=None):
        """Upload (provenance, text) chunks for one document."""
        payload = {
            "source_id": source_id,
            "chunks": [{"text": text, "provenance": provenance}
                       for provenance, text in chunks],
            "metadata": metadata or {},
        }
        status, body = http.request_json(
            "POST", self.base_url + "/api/v1/vector/upsert",
            payload=payload, bearer=self._bearer())
        return {"ok": 200 <= status < 300, "status": status, "body": body}

    def query(self, query, k=5):
        """Semantic retrieval: locate documents by meaning."""
        payload = {"query": query, "k": int(k)}
        status, body = http.request_json(
            "POST", self.base_url + "/api/v1/vector/query",
            payload=payload, bearer=self._bearer())
        if 200 <= status < 300 and isinstance(body, dict):
            items = body.get("items")
            if isinstance(items, list):
                return items
        return []

    def delete_document(self, source_id):
        """Drop all vectors for a document."""
        import urllib.parse
        quoted = urllib.parse.quote(source_id, safe="")
        status, body = http.request_json(
            "DELETE", self.base_url + "/api/v1/vector/documents/" + quoted,
            bearer=self._bearer())
        return {"ok": 200 <= status < 300, "status": status, "body": body}