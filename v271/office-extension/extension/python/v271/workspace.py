# -*- coding: utf-8 -*-
"""Workspace/project association.

Manual association is authoritative: the user links a document (or sets the
current workspace) through the UI; the extension only reports the association
to the graph, it never overwrites a manual choice based on graph state.
"""

from . import logutil


def current_workspace(store):
    """Return {id, name} of the current workspace, or None."""
    workspace_id = store.current_workspace_id()
    if not workspace_id:
        return None
    workspace = store.get_workspace(workspace_id)
    if workspace:
        return workspace
    return {"id": workspace_id, "name": workspace_id}


def set_current_workspace(store, workspace_id, name=None):
    """Set the workspace that new/opened documents are associated with."""
    workspace_id = (workspace_id or "").strip()
    if not workspace_id:
        store.set_state("CurrentWorkspaceId", "")
        return None
    if name and not store.get_workspace(workspace_id):
        store.add_workspace(workspace_id, name)
    store.set_state("CurrentWorkspaceId", workspace_id)
    return store.get_workspace(workspace_id) or {"id": workspace_id, "name": name or workspace_id}


def link_document(store, entity, workspace_id, name=None):
    """Associate a document entity with a workspace (manual, authoritative)."""
    workspace_id = (workspace_id or "").strip()
    if name and not store.get_workspace(workspace_id):
        store.add_workspace(workspace_id, name)
    entity["workspace_id"] = workspace_id or None
    logutil.info("linked %s to workspace %r" % (entity.get("source_id"), workspace_id))
    return entity