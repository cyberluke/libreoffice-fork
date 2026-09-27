# -*- coding: utf-8 -*-
"""Favorite documents (local mirror of the graph document.favorite state).

The extension keeps the authoritative favorite list in the V271 config node
and mirrors every change to the V271 graph. Removal propagates a delete so
the graph never keeps stale favorites.
"""

from .util import now_iso


def is_favorite(store, source_id):
    return store.get_favorite(source_id) is not None


def toggle_favorite(store, entity):
    """Toggle favorite state for a document entity.

    Returns (favorite, changed) -- favorite is the new state.
    """
    source_id = entity.get("source_id")
    if store.get_favorite(source_id) is not None:
        store.remove_favorite(source_id)
        return False, True
    store.add_favorite(
        source_id,
        entity.get("display_name", ""),
        entity.get("uri") or "",
        now_iso(),
    )
    return True, True


def list_favorites(store):
    return store.list_favorites()