# -*- coding: utf-8 -*-
"""Graph + vector + OIDC end-to-end tests against the mock V271 server."""

import os
import sys
import tempfile
import threading
import unittest
import urllib.parse
import urllib.request

sys.path.insert(0, r"../office-extension/extension/python")
sys.path.insert(0, r".")

from mock_v271_server import start_server  # noqa: E402

from v271 import graph as graph_module  # noqa: E402
from v271 import http  # noqa: E402
from v271 import oidc as oidc_module  # noqa: E402
from v271 import vector as vector_module  # noqa: E402
from v271.config import Settings, settings_from_dict  # noqa: E402


def make_settings(base_url):
    return settings_from_dict({
        "base_url": base_url,
        "issuer_url": base_url + "/identity",
        "client_id": "libreoffice-v271",
        "scopes": "openid profile offline_access",
        "redirect_port": 0,
    })


class GraphClientTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.server = start_server()
        cls.base = cls.server.base_url()
        cls.graph = graph_module.GraphClient(cls.base)

    def test_upsert_then_update_no_duplicate(self):
        server = self.server
        source = "file:///docs/proposal-%s.odt" % id(self)
        entity = graph_module.make_document_entity(
            source_id=source,
            display_name="proposal.odt", file_type="text",
            uri=source,
            last_opened="2026-09-27T10:00:00Z")
        result = self.graph.upsert_entity(entity)
        self.assertTrue(result["ok"])
        entity["last_edited"] = "2026-09-27T11:00:00Z"
        result = self.graph.upsert_entity(entity)
        self.assertTrue(result["ok"])
        self.assertTrue(result["body"]["updated"])
        with server.lock:
            count = sum(1 for (t, s) in server.entities
                        if t == "document.file" and s == source)
        self.assertEqual(count, 1)
        with server.lock:
            stored = server.entities[("document.file", source)]
        self.assertEqual(stored["last_edited"], "2026-09-27T11:00:00Z")
        self.assertEqual(stored["display_name"], "proposal.odt")

    def test_sync_document_mirrors_recents(self):
        entity = graph_module.make_document_entity(
            source_id="file:///docs/sheet.xlsx", display_name="sheet.xlsx",
            file_type="spreadsheet", uri="file:///docs/sheet.xlsx")
        results = self.graph.sync_document(entity, recents=True)
        self.assertEqual(len(results), 2)
        with self.server.lock:
            self.assertIn(("document.file", "file:///docs/sheet.xlsx"),
                          self.server.entities)
            self.assertIn(("document.recent", "file:///docs/sheet.xlsx"),
                          self.server.entities)

    def test_delete_entity(self):
        entity = graph_module.make_document_entity(
            source_id="file:///docs/fav.odt", display_name="fav.odt")
        self.graph.upsert_entity(entity)
        result = self.graph.delete_entity("document.file", "file:///docs/fav.odt")
        self.assertTrue(result["ok"])
        with self.server.lock:
            self.assertNotIn(("document.file", "file:///docs/fav.odt"),
                             self.server.entities)


class VectorTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.server = start_server()
        cls.base = cls.server.base_url()
        cls.vector = vector_module.VectorClient(cls.base)

    def test_upsert_and_query(self):
        chunks = [("slide:1", "meeting about the proposal budget"),
                  ("slide:2", "quarterly revenue numbers")]
        result = self.vector.upsert_chunks("file:///docs/meeting.odp", chunks)
        self.assertTrue(result["ok"])
        items = self.vector.query("proposal budget", k=5)
        self.assertTrue(any(item["source_id"] == "file:///docs/meeting.odp"
                            for item in items))
        unrelated = self.vector.query("zzz quantum pasta", k=5)
        self.assertFalse(any(item["source_id"] == "file:///docs/meeting.odp"
                             for item in unrelated))

    def test_delete_document(self):
        self.vector.upsert_chunks("file:///docs/tmp.odt", [("document", "temp")])
        result = self.vector.delete_document("file:///docs/tmp.odt")
        self.assertTrue(result["ok"])
        with self.server.lock:
            self.assertNotIn("file:///docs/tmp.odt", self.server.vector_chunks)


class OidcFlowTests(unittest.TestCase):
    """Full Authorization Code + PKCE flow against the mock identity service."""

    @classmethod
    def setUpClass(cls):
        cls.server = start_server()
        cls.base = cls.server.base_url()
        cls.settings = make_settings(cls.base)

    def _client(self):
        from v271.tokenstore import TokenStore
        token_dir = tempfile.mkdtemp(prefix="v271-tokens-")
        self.addCleanup(self._cleanup, token_dir)
        store = TokenStore(profile_dir=token_dir, allow_insecure=True)
        return oidc_module.OidcClient(self.settings, token_store=store)

    @staticmethod
    def _cleanup(token_dir):
        import shutil
        shutil.rmtree(token_dir, ignore_errors=True)

    def test_discovery(self):
        endpoints = self._client().discover()
        self.assertEqual(endpoints["token_endpoint"],
                         self.base + "/identity/token")

    def test_full_sign_in_flow(self):
        client = self._client()
        # Simulate the browser: load the authorize URL and follow the redirect
        # to the loopback callback, then let the client exchange the code.
        original_open = oidc_module.webbrowser.open

        def fake_open(url, new=0):
            # Fetch the authorize URL without following the redirect: the
            # redirect target is our loopback callback server, which serves
            # exactly one request.
            class NoRedirect(urllib.request.HTTPRedirectHandler):
                def redirect_request(self, req, fp, code, msg, headers, newurl):
                    return None

            opener = urllib.request.build_opener(NoRedirect)
            response = opener.open(url, timeout=10)
            location = response.headers.get("Location")
            response.close()
            self.assertTrue(location and location.startswith("http://127.0.0.1:"))
            parsed_cb = urllib.parse.urlparse(location)
            query = urllib.parse.parse_qs(parsed_cb.query)
            self.assertIn("code", query)
            self.assertIn("state", query)
            # Deliver the code to the waiting callback server.
            import http.client
            host, port = parsed_cb.netloc.split(":")
            conn = http.client.HTTPConnection(host, int(port), timeout=10)
            conn.request("GET", parsed_cb.path + "?" + parsed_cb.query)
            conn.getresponse().read()
            conn.close()
            return True

        oidc_module.webbrowser.open = fake_open
        try:
            account = client.sign_in(timeout=30)
        finally:
            oidc_module.webbrowser.open = original_open

        self.assertEqual(account["sub"], "sub-1234")
        self.assertEqual(account["name"], "Test User")
        self.assertTrue(client.is_signed_in())
        token = client.get_access_token()
        self.assertTrue(token and token.startswith("eyJ"))

    def test_refresh(self):
        client = self._client()
        self.assertFalse(client.is_signed_in())
        # Use a stored token with an expired expires_at to force refresh.
        tokens = {
            "access_token": "expired-token",
            "refresh_token": "rt-fresh-" + "x" * 20,
            "id_token": "",
            "expires_at": 0,
        }
        client.token_store.save(client.profile_key, tokens)
        token = client.get_access_token()
        self.assertIsNotNone(token)
        self.assertNotEqual(token, "expired-token")

    def test_sign_out_revokes(self):
        client = self._client()
        tokens = {
            "access_token": "at-1",
            "refresh_token": "rt-revoke-" + "y" * 20,
            "id_token": "",
            "expires_at": 10 ** 12,
        }
        client.token_store.save(client.profile_key, tokens)
        client.sign_out()
        self.assertFalse(client.is_signed_in())
        with self.server.lock:
            self.assertIn("rt-revoke-" + "y" * 20, self.server.revoked_refresh)
        # A refresh with the revoked token must now fail.
        result = client._refresh("rt-revoke-" + "y" * 20)
        self.assertIsNone(result)


class ConfigTests(unittest.TestCase):

    def test_env_override(self):
        os.environ["V271_BASE_URL"] = "https://v271.example.invalid"
        os.environ["V271_SYNC_ON_SAVE"] = "false"
        try:
            settings = oidc_settings = None
            from v271.config import settings_from_env
            settings = settings_from_env()
        finally:
            os.environ.pop("V271_BASE_URL", None)
            os.environ.pop("V271_SYNC_ON_SAVE", None)
        self.assertEqual(settings.api_base, "https://v271.example.invalid")
        self.assertFalse(settings.sync_on_save)
        self.assertEqual(settings.effective_issuer_url,
                         "https://v271.example.invalid/identity")

    def test_settings_defaults(self):
        settings = Settings()
        self.assertFalse(settings.is_configured())
        self.assertEqual(settings.max_chunk_size, 2000)
        self.assertTrue(settings.sync_on_save)


if __name__ == "__main__":
    unittest.main()