# -*- coding: utf-8 -*-
"""PKCE (RFC 7636) primitives for the V271 OIDC desktop client."""

import base64
import hashlib
import os
import secrets


def b64url(data: bytes) -> str:
    """Base64url without padding, as required by RFC 7636 / OAuth 2.0."""
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def generate_verifier(length: int = 64) -> str:
    """Generate a code_verifier (43-128 chars, unreserved characters)."""
    if not 43 <= length <= 128:
        raise ValueError("code_verifier must be between 43 and 128 characters")
    alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
    return "".join(secrets.choice(alphabet) for _ in range(length))


def derive_challenge(verifier: str, method: str = "S256") -> str:
    """Derive the code_challenge from a code_verifier."""
    if method == "S256":
        digest = hashlib.sha256(verifier.encode("utf-8")).digest()
        return b64url(digest)
    if method == "plain":
        return verifier
    raise ValueError("unsupported PKCE method: %r" % (method,))


def generate_state() -> str:
    """Generate an OAuth state value (CSRF protection for the redirect)."""
    return b64url(os.urandom(24))