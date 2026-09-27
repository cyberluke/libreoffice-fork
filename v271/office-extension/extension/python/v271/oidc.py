# -*- coding: utf-8 -*-
"""V271 OIDC desktop client (Authorization Code + PKCE, RFC 7636).

Flow:
  1. discover endpoints from the issuer (with documented fallbacks)
  2. open the authorization URL in the system browser with an S256 PKCE
     challenge and a random state
  3. receive the code on a loopback redirect (http://127.0.0.1:<port>/callback)
  4. exchange the code for tokens (verifier proof)
  5. refresh with refresh_token before expiry; revoke on sign-out

Tokens are kept in TokenStore (DPAPI on Windows / 0600 file elsewhere).
"""

import base64
import json
import os
import socket
import threading
import time
import urllib.parse
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer

from . import http, pkce
from .tokenstore import TokenStore

DEFAULT_SCOPES = "openid profile offline_access"


class OidcError(Exception):
    """Raised for OIDC protocol failures (user cancelled, bad response...)."""


class _CallbackHandler(BaseHTTPRequestHandler):
    """Serves exactly one /callback request carrying the authorization code."""

    result = None  # (error, code, state) set by do_GET

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        query = urllib.parse.parse_qs(parsed.query)
        code = query.get("code", [None])[0]
        state = query.get("state", [None])[0]
        error = query.get("error", [None])[0]
        _CallbackHandler.result = (error, code, state)
        body = (b"<html><body><h3>V271 sign-in</h3>"
                b"<p>You can close this window and return to LibreOffice.</p>"
                b"</body></html>")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


class OidcClient:
    """OIDC client bound to one issuer profile."""

    def __init__(self, settings, token_store=None):
        self.settings = settings
        self.issuer = settings.effective_issuer_url
        self.token_store = token_store or TokenStore(
            allow_insecure=settings.allow_insecure_token_storage)
        self.profile_key = TokenStore.profile_key_for(self.issuer)
        self._endpoints = None
        self._lock = threading.Lock()

    # -- discovery ---------------------------------------------------------

    def discover(self, force=False):
        if self._endpoints is not None and not force:
            return self._endpoints
        endpoints = None
        try:
            status, body = http.request_json(
                "GET", self.issuer + "/.well-known/openid-configuration",
                timeout=10.0)
            if 200 <= status < 300 and isinstance(body, dict):
                endpoints = {
                    "authorization_endpoint": body.get("authorization_endpoint"),
                    "token_endpoint": body.get("token_endpoint"),
                    "userinfo_endpoint": body.get("userinfo_endpoint"),
                    "revocation_endpoint": body.get("revocation_endpoint"),
                }
        except (http.HttpError, http.NetworkError):
            endpoints = None
        if endpoints is None:
            # Documented fallback layout for the V271 identity service.
            endpoints = {
                "authorization_endpoint": self.issuer + "/authorize",
                "token_endpoint": self.issuer + "/token",
                "userinfo_endpoint": self.issuer + "/userinfo",
                "revocation_endpoint": self.issuer + "/revoke",
            }
        self._endpoints = endpoints
        return endpoints

    # -- helpers -----------------------------------------------------------

    def _redirect_uri(self, port):
        return "http://127.0.0.1:%d/callback" % port

    def _start_callback_server(self):
        port = int(self.settings.redirect_port or 0)
        server = HTTPServer(("127.0.0.1", port), _CallbackHandler)
        return server

    def _open_browser(self, url):
        try:
            if webbrowser.open(url, new=1):
                return True
        except Exception:
            pass
        try:
            if os.name == "nt":
                os.startfile(url)  # noqa: S606 - intentional browser open
                return True
        except Exception:
            pass
        return False

    # -- authorization -----------------------------------------------------

    def sign_in(self, timeout=300):
        """Run the full authorization code + PKCE flow. Returns account dict.

        Blocks until the browser callback arrives (or timeout/cancel).
        """
        if not self.issuer:
            raise OidcError("V271 issuer URL is not configured")
        endpoints = self.discover()

        verifier = pkce.generate_verifier()
        challenge = pkce.derive_challenge(verifier)
        state = pkce.generate_state()

        server = self._start_callback_server()
        port = server.server_address[1]
        redirect_uri = self._redirect_uri(port)
        _CallbackHandler.result = None

        params = {
            "response_type": "code",
            "client_id": self.settings.client_id,
            "redirect_uri": redirect_uri,
            "scope": self.settings.scopes or DEFAULT_SCOPES,
            "state": state,
            "code_challenge": challenge,
            "code_challenge_method": "S256",
            "nonce": pkce.generate_state(),
        }
        auth_url = (endpoints["authorization_endpoint"] + "?" +
                    urllib.parse.urlencode(params))

        received = threading.Event()

        def serve():
            try:
                server.handle_request()
            finally:
                received.set()

        thread = threading.Thread(target=serve, daemon=True)
        thread.start()

        opened = self._open_browser(auth_url)
        if not opened:
            raise OidcError(
                "Could not open the browser. Open this URL manually:\n%s" % auth_url)

        if not received.wait(timeout):
            server.server_close()
            raise OidcError("Sign-in timed out after %d seconds" % timeout)
        server.server_close()

        error, code, returned_state = _CallbackHandler.result
        if error:
            raise OidcError("Authorization failed: %s" % error)
        if code is None:
            raise OidcError("Authorization callback carried no code")
        if returned_state != state:
            raise OidcError("Authorization state mismatch (possible CSRF)")

        tokens = self._exchange_code(code, verifier, redirect_uri)
        self.token_store.save(self.profile_key, tokens)
        return self.account_from_tokens(tokens)

    def _exchange_code(self, code, verifier, redirect_uri):
        form = {
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": redirect_uri,
            "client_id": self.settings.client_id,
            "code_verifier": verifier,
        }
        endpoints = self.discover()
        status, body = http.post_form(endpoints["token_endpoint"], form)
        if not 200 <= status < 300 or not isinstance(body, dict):
            raise OidcError("Token exchange failed (HTTP %d)" % status)
        return self._normalize_tokens(body)

    def _normalize_tokens(self, body):
        tokens = {
            "access_token": body.get("access_token", ""),
            "refresh_token": body.get("refresh_token", ""),
            "id_token": body.get("id_token", ""),
            "token_type": body.get("token_type", "Bearer"),
            "scope": body.get("scope", ""),
        }
        expires_in = body.get("expires_in")
        now = time.time()
        if isinstance(expires_in, (int, float)) and expires_in > 0:
            tokens["expires_at"] = now + float(expires_in) - 60
        else:
            exp = self._id_token_claim(tokens.get("id_token"), "exp")
            tokens["expires_at"] = float(exp) if exp else (now + 3600)
        return tokens

    # -- tokens ------------------------------------------------------------

    def stored_tokens(self):
        return self.token_store.load(self.profile_key)

    def is_signed_in(self):
        tokens = self.stored_tokens()
        return bool(tokens and tokens.get("access_token"))

    def get_access_token(self):
        """Return a valid access token, refreshing if needed (thread-safe)."""
        with self._lock:
            tokens = self.stored_tokens()
            if not tokens or not tokens.get("access_token"):
                return None
            expires_at = tokens.get("expires_at") or 0
            if time.time() < float(expires_at):
                return tokens["access_token"]
            if tokens.get("refresh_token"):
                refreshed = self._refresh(tokens["refresh_token"])
                if refreshed:
                    self.token_store.save(self.profile_key, refreshed)
                    return refreshed["access_token"]
            return None

    def _refresh(self, refresh_token):
        endpoints = self.discover()
        form = {
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
            "client_id": self.settings.client_id,
        }
        try:
            status, body = http.post_form(endpoints["token_endpoint"], form)
        except (http.HttpError, http.NetworkError):
            return None
        if not 200 <= status < 300 or not isinstance(body, dict):
            return None
        base = self.stored_tokens() or {}
        tokens = self._normalize_tokens(body)
        tokens.setdefault("refresh_token", base.get("refresh_token", ""))
        return tokens

    def sign_out(self):
        """Revoke the refresh token when supported, then drop local tokens."""
        tokens = self.stored_tokens()
        if tokens and tokens.get("refresh_token"):
            endpoints = self.discover()
            revocation = endpoints.get("revocation_endpoint")
            if revocation:
                try:
                    http.post_form(revocation, {
                        "token": tokens["refresh_token"],
                        "token_type_hint": "refresh_token",
                        "client_id": self.settings.client_id,
                    })
                except (http.HttpError, http.NetworkError):
                    pass
        self.token_store.delete(self.profile_key)

    # -- account info ------------------------------------------------------

    def account_from_tokens(self, tokens):
        claims = self._id_token_claims(tokens.get("id_token") or "")
        account = {
            "sub": claims.get("sub") or "",
            "name": claims.get("name") or "",
            "email": claims.get("email") or "",
            "preferred_username": claims.get("preferred_username") or "",
        }
        if not account["name"] and not account["email"]:
            info = self._userinfo(tokens.get("access_token"))
            account.update(info)
        return account

    def current_account(self):
        tokens = self.stored_tokens()
        if not tokens:
            return None
        return self.account_from_tokens(tokens)

    def _userinfo(self, access_token):
        endpoints = self.discover()
        userinfo = endpoints.get("userinfo_endpoint")
        if not userinfo:
            return {}
        try:
            status, body = http.request_json("GET", userinfo, bearer=access_token)
            if 200 <= status < 300 and isinstance(body, dict):
                return {
                    "sub": body.get("sub", ""),
                    "name": body.get("name", ""),
                    "email": body.get("email", ""),
                    "preferred_username": body.get("preferred_username", ""),
                }
        except (http.HttpError, http.NetworkError):
            pass
        return {}

    # -- id_token parsing --------------------------------------------------

    @staticmethod
    def _id_token_payload(id_token):
        try:
            parts = id_token.split(".")
            if len(parts) != 3:
                return {}
            payload = parts[1]
            payload += "=" * (-len(payload) % 4)
            return json.loads(base64.urlsafe_b64decode(payload).decode("utf-8"))
        except Exception:
            return {}

    def _id_token_claims(self, id_token):
        return self._id_token_payload(id_token)

    def _id_token_claim(self, id_token, name):
        return self._id_token_payload(id_token).get(name)