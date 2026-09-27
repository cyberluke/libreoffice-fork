# -*- coding: utf-8 -*-
"""Document text chunking for the V271 vector pipeline.

No local embedding model: this only produces (provenance, text) chunks that are
sent to the central V271 vector service.
"""

import re

_SPACE_RE = re.compile(r"\s+")


def chunk_text(text, provenance, max_chars=2000, overlap=100):
    """Split a piece of text into chunks of at most max_chars characters.

    Splits on whitespace boundaries; long unbreakable tokens are hard-split.
    Returns a list of (provenance, chunk_text) tuples.
    """
    if max_chars < 200:
        raise ValueError("max_chars must be >= 200")
    text = _SPACE_RE.sub(" ", text or "").strip()
    if not text:
        return []
    chunks = []
    start = 0
    length = len(text)
    while start < length:
        end = min(start + max_chars, length)
        if end < length:
            # Try to break at the last whitespace inside the window.
            cut = text.rfind(" ", start + max_chars // 2, end)
            if cut > start:
                end = cut
        chunk = text[start:end].strip()
        if chunk:
            chunks.append((provenance, chunk))
        if end >= length:
            break
        start = max(end - overlap, start + 1)
    return chunks


def chunk_documents(extracted, max_chars=2000, overlap=100):
    """Chunk a list of (provenance, text) extractions.

    Small adjacent pieces of the same provenance are merged up to max_chars.
    """
    result = []
    for provenance, text in extracted:
        if not text:
            continue
        result.extend(chunk_text(text, provenance, max_chars, overlap))
    return result