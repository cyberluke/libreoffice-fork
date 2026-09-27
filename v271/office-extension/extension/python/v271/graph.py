# -*- coding: utf-8 -*-
"""V271 Personal Graph client.

Entity contract (upsert semantics -- same source_id updates the existing
entity instead of creating a duplicate):

  POST {base}/api/v1/graph/entities
  {
    "type": "document.file" | "document.recent" | "document.favorite",
    "source_id": "<stable id: canonical URI / file URL>",
    "display_name": "...",
    "file_type": "text" | "spreadsheet" | "presentation" | "drawing" | "other",
    "uri": "file:///... or cloud URL",
    "last_opened": "ISO-8601 or null",
    "last_edited": "ISO-8601 or null",
    "favorite": false,
    "workspace_id": "..." or null,
    "fingerprint": "sha256:..." or null,
    "metadata": { "module": "Writer", ... }
  }

  DELETE {base}/api/v1/graph/entities/{type}/{url-encoded source_id}
  GET   {base}/api/v1/graph/entities?type=document.recent
"""

import json
import urllib.parse

from . import http

DOCUMENT_FILE = "document.file"
DOCUMENT_RECENT = "document.recent"
DOCUMENT_FAVORITE = "document.favorite"


def make_document_entity(source_id, display_name, file_type="other", uri=None,
                         last_opened=None, last_edited=None, favorite=False,
                         workspace_id=None, fingerprint=None, metadata=None,
                         entity_type=DOCUMENT_FILE):
    """Build a graph entity payload (all values JSON-safe)."""
    return {
        "type": entity_type,
        "source_id": source_id,
        "display_name": display_name or "",
        "file_type": file_type,
        "uri": uri,
        "last_opened": last_opened,
        "last_edited": last_edited,
        "favorite": bool(favorite),
        "workspace_id": workspace_id,
        "fingerprint": fingerprint,
        "metadata": metadata or {},
    }


class GraphClient:
    """Thin client for the V271 graph entity endpoints."""

    def __init__(self, base_url, token_provider=None):
        self.base_url = (base_url or "").rstrip("/")
        self.token_provider = token_provider  # callable returning access token or None

    def _endpoint(self, path):
        return self.base_url + path

    def _bearer(self):
        if self.token_provider is None:
            return None
        token = self.token_provider()
        return token or None

    def upsert_entity(self, entity):
        """Create or update one entity. Idempotent by source_id."""
        status, body = http.request_json(
            "POST", self._endpoint("/api/v1/graph/entities"),
            payload=entity, bearer=self._bearer())
        return {"ok": 200 <= status < 300, "status": status, "body": body}

    def delete_entity(self, entity_type, source_id):
        """Delete one entity (e.g. when a favorite is removed)."""
        quoted = urllib.parse.quote(source_id, safe="")
        status, body = http.request_json(
            "DELETE", self._endpoint("/api/v1/graph/entities/%s/%s" % (entity_type, quoted)),
            bearer=self._bearer())
        return {"ok": 200 <= status < 300, "status": status, "body": body}

    def list_entities(self, entity_type):
        """List entities of one type: returns list of dicts."""
        url = self._endpoint("/api/v1/graph/entities") + "?type=" + urllib.parse.quote(entity_type)
        status, body = http.request_json("GET", url, bearer=self._bearer())
        if 200 <= status < 300 and isinstance(body, dict):
            items = body.get("items")
            if isinstance(items, list):
                return items
            if isinstance(body.get("entities"), list):
                return body["entities"]
        return []

    def sync_document(self, entity, recents=True, favorite=False):
        """Sync a document.file entity plus optional recent/favorite mirrors.

        All writes are upserts keyed by source_id, so repeated saves update
        the existing entities instead of duplicating them.
        """
        results = []
        results.append(self.upsert_entity(entity))
        if recents:
            recent = dict(entity)
            recent["type"] = DOCUMENT_RECENT
            results.append(self.upsert_entity(recent))
        if favorite:
            fav = dict(entity)
            fav["type"] = DOCUMENT_FAVORITE
            results.append(self.upsert_entity(fav))
        return results