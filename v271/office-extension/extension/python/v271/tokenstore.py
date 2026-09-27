# -*- coding: utf-8 -*-
"""Secure token storage for the V271 OIDC client.

Windows: DPAPI (CryptProtectData/CryptUnprotectData) via ctypes -- tokens are
encrypted with the user's logon credentials, no extra password needed.
Other platforms: a JSON file with owner-only permissions (0600).

An explicit allow-insecure flag is required before falling back to plaintext
storage, so credentials are never silently written unencrypted.
"""

import base64
import json
import os
import sys
import tempfile

_PROFILE_DIR_OVERRIDE = os.environ.get("V271_TOKEN_DIR", "").strip()


def _default_profile_dir():
    if _PROFILE_DIR_OVERRIDE:
        return _PROFILE_DIR_OVERRIDE
    if sys.platform.startswith("win"):
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
        return os.path.join(base, "v271")
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(base, "v271")


def _dpapi_protect(data: bytes) -> bytes:
    import ctypes
    from ctypes import wintypes

    class DATA_BLOB(ctypes.Structure):
        _fields_ = [("cbData", wintypes.DWORD),
                    ("pbData", ctypes.POINTER(ctypes.c_byte))]

    def blob_from_bytes(raw):
        buf = ctypes.create_string_buffer(raw, len(raw))
        return DATA_BLOB(len(raw), ctypes.cast(buf, ctypes.POINTER(ctypes.c_byte)))

    crypt32 = ctypes.windll.crypt32
    kernel32 = ctypes.windll.kernel32
    local_free = kernel32.LocalFree

    in_blob = blob_from_bytes(data)
    out_blob = DATA_BLOB()
    ok = crypt32.CryptProtectData(ctypes.byref(in_blob), None, None, None, None,
                                  0, ctypes.byref(out_blob))
    if not ok:
        raise OSError("CryptProtectData failed (error %d)" % kernel32.GetLastError())
    try:
        raw = ctypes.string_at(out_blob.pbData, out_blob.cbData)
        return bytes(raw)
    finally:
        local_free(out_blob.pbData)


def _dpapi_unprotect(data: bytes) -> bytes:
    import ctypes
    from ctypes import wintypes

    class DATA_BLOB(ctypes.Structure):
        _fields_ = [("cbData", wintypes.DWORD),
                    ("pbData", ctypes.POINTER(ctypes.c_byte))]

    def blob_from_bytes(raw):
        buf = ctypes.create_string_buffer(raw, len(raw))
        return DATA_BLOB(len(raw), ctypes.cast(buf, ctypes.POINTER(ctypes.c_byte)))

    crypt32 = ctypes.windll.crypt32
    kernel32 = ctypes.windll.kernel32
    local_free = kernel32.LocalFree

    in_blob = blob_from_bytes(data)
    out_blob = DATA_BLOB()
    ok = crypt32.CryptUnprotectData(ctypes.byref(in_blob), None, None, None, None,
                                    0, ctypes.byref(out_blob))
    if not ok:
        raise OSError("CryptUnprotectData failed (error %d)" % kernel32.GetLastError())
    try:
        raw = ctypes.string_at(out_blob.pbData, out_blob.cbData)
        return bytes(raw)
    finally:
        local_free(out_blob.pbData)


def _chmod_owner_only(path):
    try:
        os.chmod(path, 0o600)
    except OSError:
        pass


class TokenStore:
    """Loads/saves token payloads per profile key (e.g. issuer host hash)."""

    def __init__(self, profile_dir=None, allow_insecure=False):
        self.profile_dir = profile_dir or _default_profile_dir()
        self.allow_insecure = allow_insecure
        self._use_dpapi = bool(sys.platform.startswith("win"))
        if self._use_dpapi:
            try:
                import ctypes  # noqa: F401
                ctypes.windll.crypt32  # noqa: B018 - verify availability
            except (ImportError, AttributeError, OSError):
                self._use_dpapi = False

    # -- storage backend ---------------------------------------------------

    def _file_path(self, profile_key):
        safe = "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in profile_key)
        return os.path.join(self.profile_dir, "tokens-%s.json" % safe)

    def _encrypt(self, payload_bytes):
        if self._use_dpapi:
            return {"dpapi": True, "data": base64.b64encode(
                _dpapi_protect(payload_bytes)).decode("ascii")}
        if not self.allow_insecure:
            raise RuntimeError(
                "No OS-level token protection available and "
                "AllowInsecureTokenStorage is disabled")
        return {"dpapi": False, "data": base64.b64encode(payload_bytes).decode("ascii")}

    def _decrypt(self, record):
        raw = base64.b64decode(record.get("data", "").encode("ascii"))
        if record.get("dpapi"):
            if not self._use_dpapi:
                raise RuntimeError("Token was protected with DPAPI, unavailable here")
            return _dpapi_unprotect(raw)
        return raw

    # -- public API --------------------------------------------------------

    def save(self, profile_key, payload):
        os.makedirs(self.profile_dir, exist_ok=True)
        raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        record = self._encrypt(raw)
        path = self._file_path(profile_key)
        tmp_path = path + ".tmp"
        with open(tmp_path, "w", encoding="utf-8") as handle:
            json.dump(record, handle, ensure_ascii=False)
        _chmod_owner_only(tmp_path)
        os.replace(tmp_path, path)

    def load(self, profile_key):
        path = self._file_path(profile_key)
        if not os.path.isfile(path):
            return None
        try:
            with open(path, "r", encoding="utf-8") as handle:
                record = json.load(handle)
            raw = self._decrypt(record)
            return json.loads(raw.decode("utf-8"))
        except (OSError, ValueError, KeyError, RuntimeError):
            return None

    def delete(self, profile_key):
        path = self._file_path(profile_key)
        try:
            os.remove(path)
        except OSError:
            pass

    @staticmethod
    def profile_key_for(issuer_url):
        import hashlib
        return hashlib.sha256(issuer_url.encode("utf-8")).hexdigest()[:24]


def secure_temp_dir_hint():
    """Informational helper for diagnostics."""
    return tempfile.gettempdir()