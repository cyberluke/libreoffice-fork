# -*- coding: utf-8 -*-
"""Mock V271 identity / graph / vector / AI service.

Standard-library only. Used by the unit tests and by the evidence harness to
verify the extension client stack end-to-end without a real V271 deployment.

Endpoints (mirroring the contract the extension implements):
  GET  /identity/.well-known/openid-configuration
  GET  /identity/authorize            (PKCE S256, returns redirect with code)
  POST /identity/token                (authorization_code / refresh_token)
  POST /identity/revoke
  GET  /identity/userinfo
  POST /api/v1/graph/entities         (upsert, keyed by type+source_id)
  DELETE /api/v1/graph/entities/{type}/{source_id}
  GET  /api/v1/graph/entities?type=...
  POST /api/v1/vector/upsert
  POST /api/v1/vector/query           (naive keyword scoring)
  DELETE /api/v1/vector/documents/{source_id}
  POST /api/v1/ai/summarize
  GET  /_stats                        (evidence counters)
"""

import base64
import hashlib
import json
import secrets
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ISSUER_PATH = "/identity"


class MockV271Server:
    """In-memory V271 service."""

    def __init__(self):
        self.lock = threading.Lock()
        self.entities = {}            # (type, source_id) -> entity dict
        self.vector_chunks = {}       # source_id -> list of {text, provenance}
        self.pending_codes = {}       # code -> verifier
        self.revoked_refresh = set()
        self.issued_tokens = 0
        self.upsert_count = 0
        self.query_count = 0
        self.summarize_count = 0
        self.accounts = {"test-user": {
            "sub": "sub-1234", "name": "Test User",
            "email": "test@v271.invalid", "preferred_username": "test-user"}}
        self.httpd = None

    # -- helpers -----------------------------------------------------------

    def base_url(self):
        return "http://127.0.0.1:%d" % self.httpd.server_address[1]

    def _make_token(self, extra_claims=None):
        claims = {
            "iss": self.base_url() + ISSUER_PATH,
            "sub": "sub-1234",
            "aud": "libreoffice-v271",
            "exp": int(time.time()) + 3600,
            "iat": int(time.time()),
            "name": "Test User",
            "email": "test@v271.invalid",
            "preferred_username": "test-user",
        }
        if extra_claims:
            claims.update(extra_claims)
        header = base64.urlsafe_b64encode(
            json.dumps({"alg": "none", "typ": "JWT"}).encode()).rstrip(b"=")
        payload = base64.urlsafe_b64encode(
            json.dumps(claims).encode()).rstrip(b"=")
        return (header + b"." + payload + b".").decode("ascii")

    # -- request handling --------------------------------------------------

    def handle(self, handler):
        method = handler.command
        parsed = urllib.parse.urlparse(handler.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        if path == "/_stats":
            self._json(handler, 200, self.stats())
            return
        if path == "/identity/.well-known/openid-configuration":
            base = self.base_url() + ISSUER_PATH
            self._json(handler, 200, {
                "issuer": base,
                "authorization_endpoint": base + "/authorize",
                "token_endpoint": base + "/token",
                "userinfo_endpoint": base + "/userinfo",
                "revocation_endpoint": base + "/revoke",
                "code_challenge_methods_supported": ["S256", "plain"],
            })
            return
        if path == "/identity/authorize":
            self._authorize(handler, query)
            return
        if path == "/identity/token" and method == "POST":
            self._token(handler, self._form(handler))
            return
        if path == "/identity/revoke" and method == "POST":
            form = self._form(handler)
            with self.lock:
                self.revoked_refresh.add(form.get("token", ""))
            self._json(handler, 200, {})
            return
        if path == "/identity/userinfo" and method == "GET":
            auth = handler.headers.get("Authorization", "")
            if not auth.startswith("Bearer "):
                self._json(handler, 401, {"error": "unauthorized"})
                return
            self._json(handler, 200, dict(self.accounts["test-user"]))
            return
        if path.startswith("/api/v1/graph/entities"):
            self._graph(handler, method, parsed, query)
            return
        if path == "/api/v1/vector/upsert" and method == "POST":
            self._vector_upsert(handler)
            return
        if path == "/api/v1/vector/query" and method == "POST":
            self._vector_query(handler)
            return
        if path.startswith("/api/v1/vector/documents/") and method == "DELETE":
            source_id = urllib.parse.unquote(path.rsplit("/", 1)[-1])
            with self.lock:
                self.vector_chunks.pop(source_id, None)
            self._json(handler, 200, {"deleted": True})
            return
        if path == "/api/v1/ai/summarize" and method == "POST":
            self._summarize(handler)
            return
        self._json(handler, 404, {"error": "not found"})

    # -- flows -------------------------------------------------------------

    def _authorize(self, handler, query):
        redirect_uri = (query.get("redirect_uri") or [""])[0]
        state = (query.get("state") or [""])[0]
        code_challenge = (query.get("code_challenge") or [""])[0]
        method = (query.get("code_challenge_method") or ["plain"])[0]
        if method != "S256" or not code_challenge:
            self._json(handler, 400, {"error": "invalid PKCE parameters"})
            return
        code = secrets.token_urlsafe(24)
        with self.lock:
            self.pending_codes[code] = {
                "challenge": code_challenge,
                "method": method,
            }
        sep = "&" if "?" in redirect_uri else "?"
        handler.send_response(302)
        handler.send_header("Location",
                            "%s%scode=%s&state=%s" % (redirect_uri, sep, code, state))
        handler.send_header("Content-Length", "0")
        handler.end_headers()

    def _token(self, handler, form):
        grant = form.get("grant_type")
        if grant == "authorization_code":
            code = form.get("code", "")
            verifier = form.get("code_verifier", "")
            with self.lock:
                pending = self.pending_codes.pop(code, None)
            if pending is None:
                self._json(handler, 400, {"error": "invalid_grant"})
                return
            if pending["method"] == "S256":
                digest = hashlib.sha256(verifier.encode()).digest()
                challenge = base64.urlsafe_b64encode(digest).rstrip(b"=").decode()
                if challenge != pending["challenge"]:
                    self._json(handler, 400, {"error": "invalid_grant: pkce mismatch"})
                    return
            self._issue_tokens(handler)
            return
        if grant == "refresh_token":
            refresh = form.get("refresh_token", "")
            with self.lock:
                if refresh in self.revoked_refresh:
                    self._json(handler, 401, {"error": "invalid_grant: revoked"})
                    return
            self._issue_tokens(handler)
            return
        self._json(handler, 400, {"error": "unsupported_grant_type"})

    def _issue_tokens(self, handler):
        with self.lock:
            self.issued_tokens += 1
        self._json(handler, 200, {
            "access_token": self._make_token(),
            "refresh_token": "rt-" + secrets.token_urlsafe(24),
            "id_token": self._make_token({"nonce": "n"}),
            "token_type": "Bearer",
            "expires_in": 3600,
            "scope": "openid profile offline_access",
        })

    def _graph(self, handler, method, parsed, query):
        if method == "POST":
            body = self._json_body(handler)
            if not isinstance(body, dict) or not body.get("type") or not body.get("source_id"):
                self._json(handler, 400, {"error": "type and source_id required"})
                return
            key = (body["type"], body["source_id"])
            with self.lock:
                existed = key in self.entities
                self.entities[key] = body
                self.upsert_count += 1
            self._json(handler, 200, {
                "ok": True, "updated": existed, "entity": key})
            return
        if method == "DELETE":
            parts = parsed.path.split("/")
            if len(parts) != 7 or parts[4] != "entities":
                self._json(handler, 400, {"error": "bad path"})
                return
            entity_type = urllib.parse.unquote(parts[5])
            source_id = urllib.parse.unquote(parts[6])
            with self.lock:
                removed = self.entities.pop((entity_type, source_id), None)
            self._json(handler, 200, {"deleted": removed is not None})
            return
        if method == "GET":
            entity_type = (query.get("type") or [""])[0]
            with self.lock:
                items = [e for (t, _), e in self.entities.items()
                         if not entity_type or t == entity_type]
            self._json(handler, 200, {"items": items})
            return
        self._json(handler, 405, {"error": "method not allowed"})

    def _vector_upsert(self, handler):
        body = self._json_body(handler)
        source_id = body.get("source_id")
        chunks = body.get("chunks") or []
        if not source_id or not isinstance(chunks, list):
            self._json(handler, 400, {"error": "source_id and chunks required"})
            return
        with self.lock:
            self.vector_chunks[source_id] = chunks
        self._json(handler, 200, {"ok": True, "chunks": len(chunks)})

    def _vector_query(self, handler):
        body = self._json_body(handler)
        query = (body.get("query") or "").lower()
        k = int(body.get("k") or 5)
        terms = [t for t in query.split() if len(t) > 2]
        scored = []
        with self.lock:
            for source_id, chunks in self.vector_chunks.items():
                haystack = " ".join(c.get("text", "") for c in chunks).lower()
                score = sum(1 for t in terms if t in haystack)
                if score > 0:
                    snippet = haystack[:160]
                    scored.append({
                        "source_id": source_id,
                        "score": score / max(len(terms), 1),
                        "snippet": snippet,
                    })
            self.query_count += 1
        scored.sort(key=lambda item: item["score"], reverse=True)
        self._json(handler, 200, {"items": scored[:k]})

    def _summarize(self, handler):
        body = self._json_body(handler)
        with self.lock:
            self.summarize_count += 1
        text = (body.get("text") or "")[:120]
        self._json(handler, 200, {
            "summary": "Summary of %s: %s" % (body.get("display_name", "doc"), text)})

    # -- plumbing ----------------------------------------------------------

    def stats(self):
        with self.lock:
            return {
                "entities": len(self.entities),
                "distinct_documents": len({s for (t, s) in self.entities
                                           if t == "document.file"}),
                "upsert_count": self.upsert_count,
                "vector_documents": len(self.vector_chunks),
                "issued_tokens": self.issued_tokens,
                "query_count": self.query_count,
                "summarize_count": self.summarize_count,
                "entity_keys": sorted(k for k in self.entities),
            }

    @staticmethod
    def _json(handler, status, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        handler.send_response(status)
        handler.send_header("Content-Type", "application/json; charset=utf-8")
        handler.send_header("Content-Length", str(len(body)))
        handler.end_headers()
        handler.wfile.write(body)

    @staticmethod
    def _form(handler):
        length = int(handler.headers.get("Content-Length") or 0)
        raw = handler.rfile.read(length).decode("utf-8", errors="replace")
        return {key: values[0] for key, values in
                urllib.parse.parse_qs(raw).items()}

    @staticmethod
    def _json_body(handler):
        length = int(handler.headers.get("Content-Length") or 0)
        raw = handler.rfile.read(length).decode("utf-8", errors="replace")
        try:
            return json.loads(raw)
        except ValueError:
            return None


class _Handler(BaseHTTPRequestHandler):
    def _dispatch(self):
        try:
            self.server.mock.handle(self)
        except Exception:
            import traceback
            traceback.print_exc()
            try:
                MockV271Server._json(self, 500, {"error": "internal"})
            except Exception:
                pass

    do_GET = _dispatch
    do_POST = _dispatch
    do_DELETE = _dispatch

    def log_message(self, *args):
        pass


def start_server():
    """Start a MockV271Server on an ephemeral port; returns the server."""
    server = MockV271Server()
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
    httpd.mock = server
    server.httpd = httpd
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return server


if __name__ == "__main__":
    server = start_server()
    print("mock v271 on", server.base_url())
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        pass