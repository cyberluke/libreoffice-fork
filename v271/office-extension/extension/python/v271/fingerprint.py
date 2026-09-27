# -*- coding: utf-8 -*-
"""Stable document fingerprints.

'Document fingerprint where safe': local files are hashed (SHA-256) with a
size guard so huge or remote documents are skipped. The fingerprint is only an
identity *aid* -- the source_id (canonical URI) remains the primary identity,
never the mutable display name.
"""

import hashlib
import os

_CHUNK = 1024 * 1024  # 1 MiB read buffer


def fingerprint_file(path, max_bytes=100 * 1024 * 1024):
    """Return 'sha256:<hex>' for a local file, or None if unsafe.

    Returns None when the file is missing, unreadable, larger than max_bytes,
    or not a regular file.
    """
    try:
        if not os.path.isfile(path):
            return None
        size = os.path.getsize(path)
        if size > max_bytes:
            return None
        hasher = hashlib.sha256()
        with open(path, "rb") as handle:
            while True:
                block = handle.read(_CHUNK)
                if not block:
                    break
                hasher.update(block)
        return "sha256:" + hasher.hexdigest()
    except (OSError, IOError):
        return None


def fingerprint_text(text, salt=""):
    """Return 'sha256:<hex>' for a text blob (used for unsaved documents)."""
    hasher = hashlib.sha256()
    hasher.update((salt or "").encode("utf-8"))
    hasher.update((text or "").encode("utf-8", errors="replace"))
    return "sha256:" + hasher.hexdigest()