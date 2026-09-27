# -*- coding: utf-8 -*-
"""V271 integration configuration.

Reads from the org.openoffice.Office.V271 configuration node when running
inside LibreOffice (ConfigurationProvider), and from environment variables
when running outside (tests / evidence harness / headless tools).

Set elements are manipulated the way configmgr requires: a free set element
is created through the set's XSingleServiceFactory, inserted by name, then
filled through XPropertySet (see unotools/source/config/historyoptions.cxx
for the canonical in-tree pattern).
"""

import os

CONFIG_ROOT = "/org.openoffice.Office.V271"

ENV_MAP = {
    "base_url": "V271_BASE_URL",
    "issuer_url": "V271_ISSUER_URL",
    "client_id": "V271_CLIENT_ID",
    "scopes": "V271_SCOPES",
    "redirect_port": "V271_REDIRECT_PORT",
    "sync_on_save": "V271_SYNC_ON_SAVE",
    "embed_on_save": "V271_EMBED_ON_SAVE",
    "sync_recents_on_start": "V271_SYNC_RECENTS_ON_START",
    "max_chunk_size": "V271_MAX_CHUNK_SIZE",
    "fingerprint_mode": "V271_FINGERPRINT_MODE",
    "max_fingerprint_bytes": "V271_MAX_FINGERPRINT_BYTES",
    "allow_insecure_token_storage": "V271_ALLOW_INSECURE_TOKEN_STORAGE",
}


class Settings:
    """Plain settings snapshot (no UNO types)."""

    def __init__(self):
        self.base_url = ""
        self.issuer_url = ""
        self.client_id = "libreoffice-v271"
        self.scopes = "openid profile offline_access"
        self.redirect_port = 0
        self.sync_on_save = True
        self.embed_on_save = True
        self.sync_recents_on_start = True
        self.max_chunk_size = 2000
        self.fingerprint_mode = "file"
        self.max_fingerprint_bytes = 100 * 1024 * 1024
        self.allow_insecure_token_storage = False

    @property
    def effective_issuer_url(self):
        if self.issuer_url:
            return self.issuer_url.rstrip("/")
        return (self.base_url or "").rstrip("/") + "/identity"

    @property
    def api_base(self):
        return (self.base_url or "").rstrip("/")

    def is_configured(self):
        return bool(self.api_base)

    def graph_url(self):
        return self.api_base + "/api/v1/graph/entities"

    def vector_url(self):
        return self.api_base + "/api/v1/vector"

    def ai_url(self):
        return self.api_base + "/api/v1/ai"

    def to_dict(self):
        return {key: getattr(self, key) for key in ENV_MAP}


def settings_from_dict(values):
    settings = Settings()
    for key in ENV_MAP:
        if key in values and values[key] is not None:
            setattr(settings, key, values[key])
    return settings


def settings_from_env():
    """Load settings from environment variables (outside LibreOffice)."""
    values = {}
    for key, env_name in ENV_MAP.items():
        raw = os.environ.get(env_name)
        if raw is None or raw == "":
            continue
        value = raw
        if key in ("redirect_port", "max_chunk_size", "max_fingerprint_bytes"):
            try:
                value = int(raw)
            except ValueError:
                continue
        elif key in ("sync_on_save", "embed_on_save", "sync_recents_on_start",
                     "allow_insecure_token_storage"):
            value = raw.strip().lower() in ("1", "true", "yes", "on")
        values[key] = value
    return settings_from_dict(values)


def _uno_access(ctx, service_name):
    provider = ctx.getServiceManager().createInstanceWithContext(
        "com.sun.star.configuration.ConfigurationProvider", ctx)
    from com.sun.star.beans import PropertyValue
    node = PropertyValue("nodepath", 0, CONFIG_ROOT, 0)
    return provider.createInstanceWithArguments(service_name, (node,))


def settings_from_uno(ctx):
    """Load settings from the org.openoffice.Office.V271 node.

    Environment variables (V271_*) override empty/unset configuration values,
    which keeps headless and evidence runs configurable without touching the
    user profile.
    """
    settings = Settings()
    env = settings_from_env()
    try:
        access = _uno_access(ctx, "com.sun.star.configuration.ConfigurationAccess")
        general = access.getByName("General")
        from com.sun.star.beans import XPropertySet
        general_props = general.queryInterface(XPropertySet)
        for key in ENV_MAP:
            try:
                settings.__dict__[key] = general_props.getPropertyValue(key)
            except Exception:
                pass
    except Exception:
        # Node missing (extension not yet registered) -> environment fallback.
        for key in ENV_MAP:
            setattr(settings, key, getattr(env, key))
        return settings
    # Environment overrides empty/zero configuration values.
    for key in ENV_MAP:
        configured = getattr(settings, key)
        env_value = getattr(env, key)
        if env_value in (None, "", 0, False):
            continue
        if configured in (None, "", 0, False):
            setattr(settings, key, env_value)
    return settings


class V271Store:
    """Read/write access to V271 state (Favorites, Workspaces, State)."""

    def __init__(self, read_access=None, update_access=None, memory=None):
        self._read = read_access
        self._update = update_access
        self._memory = memory if memory is not None else {}

    @classmethod
    def from_uno(cls, ctx):
        try:
            return cls(
                _uno_access(ctx, "com.sun.star.configuration.ConfigurationAccess"),
                _uno_access(ctx, "com.sun.star.configuration.ConfigurationUpdateAccess"),
            )
        except Exception:
            return cls(memory={})

    @classmethod
    def from_uno_read(cls, ctx):
        """Read-only store (cheap): for per-event snapshots."""
        try:
            return cls(
                _uno_access(ctx, "com.sun.star.configuration.ConfigurationAccess"),
                None,
            )
        except Exception:
            return cls(memory={})

    @classmethod
    def from_memory(cls):
        return cls(memory={})

    @property
    def uses_uno(self):
        return self._update is not None

    # -- low-level helpers -------------------------------------------------

    def _group_props(self, group_name):
        from com.sun.star.beans import XPropertySet
        group = self._read.getByName(group_name)
        return group.queryInterface(XPropertySet)

    def _set_update(self, set_name):
        """Return (container, factory) for an update set."""
        from com.sun.star.container import XNameContainer
        from com.sun.star.lang import XSingleServiceFactory
        set_node = self._update.getByName(set_name)
        container = set_node.queryInterface(XNameContainer)
        factory = set_node.queryInterface(XSingleServiceFactory)
        return container, factory

    # -- State -------------------------------------------------------------

    def get_state(self, name, default=None):
        if self._read is not None:
            try:
                value = self._group_props("State").getPropertyValue(name)
                return value if value is not None else default
            except Exception:
                return default
        return self._memory.get("state", {}).get(name, default)

    def set_state(self, name, value):
        if self._update is not None:
            try:
                from com.sun.star.beans import XPropertySet
                group = self._update.getByName("State")
                group.queryInterface(XPropertySet).setPropertyValue(name, value)
                self._update.commitChanges()
                return
            except Exception:
                pass
        self._memory.setdefault("state", {})[name] = value

    # -- Favorites ---------------------------------------------------------

    def list_favorites(self):
        result = []
        if self._read is not None:
            try:
                favorites = self._read.getByName("Favorites")
                for name in favorites.getElementNames():
                    entry = favorites.getByName(name)
                    from com.sun.star.beans import XPropertySet
                    props = entry.queryInterface(XPropertySet)
                    result.append({
                        "source_id": props.getPropertyValue("SourceId"),
                        "display_name": props.getPropertyValue("DisplayName"),
                        "uri": props.getPropertyValue("Uri"),
                        "added_at": props.getPropertyValue("AddedAt"),
                        "node": name,
                    })
                return result
            except Exception:
                return result
        return list(self._memory.get("favorites", {}).values())

    def get_favorite(self, source_id):
        for entry in self.list_favorites():
            if entry["source_id"] == source_id:
                return entry
        return None

    def add_favorite(self, source_id, display_name, uri, added_at):
        node_name = _node_name(source_id)
        if self._update is not None:
            try:
                container, factory = self._set_update("Favorites")
                element = factory.createInstance()
                container.insertByName(node_name, element)
                from com.sun.star.beans import XPropertySet
                props = element.queryInterface(XPropertySet)
                props.setPropertyValue("SourceId", source_id)
                props.setPropertyValue("DisplayName", display_name)
                props.setPropertyValue("Uri", uri)
                props.setPropertyValue("AddedAt", added_at)
                self._update.commitChanges()
                return True
            except Exception:
                pass
        self._memory.setdefault("favorites", {})[node_name] = {
            "source_id": source_id,
            "display_name": display_name,
            "uri": uri,
            "added_at": added_at,
            "node": node_name,
        }
        return True

    def remove_favorite(self, source_id):
        node_name = _node_name(source_id)
        if self._update is not None:
            try:
                container, _ = self._set_update("Favorites")
                if container.hasByName(node_name):
                    container.removeByName(node_name)
                    self._update.commitChanges()
                return True
            except Exception:
                pass
        self._memory.setdefault("favorites", {}).pop(node_name, None)
        return True

    # -- Workspaces --------------------------------------------------------

    def list_workspaces(self):
        result = []
        if self._read is not None:
            try:
                workspaces = self._read.getByName("Workspaces")
                for name in workspaces.getElementNames():
                    entry = workspaces.getByName(name)
                    from com.sun.star.beans import XPropertySet
                    props = entry.queryInterface(XPropertySet)
                    result.append({"id": props.getPropertyValue("Id"),
                                   "name": props.getPropertyValue("Name")})
                return result
            except Exception:
                return result
        return list(self._memory.get("workspaces", {}).values())

    def get_workspace(self, workspace_id):
        for entry in self.list_workspaces():
            if entry["id"] == workspace_id:
                return entry
        return None

    def add_workspace(self, workspace_id, name):
        node_name = _node_name(workspace_id)
        if self._update is not None:
            try:
                container, factory = self._set_update("Workspaces")
                element = factory.createInstance()
                container.insertByName(node_name, element)
                from com.sun.star.beans import XPropertySet
                props = element.queryInterface(XPropertySet)
                props.setPropertyValue("Id", workspace_id)
                props.setPropertyValue("Name", name)
                self._update.commitChanges()
                return True
            except Exception:
                pass
        self._memory.setdefault("workspaces", {})[node_name] = {
            "id": workspace_id, "name": name}
        return True

    def current_workspace_id(self):
        return self.get_state("CurrentWorkspaceId", "") or ""


def _node_name(source_id):
    import hashlib
    return hashlib.sha1(source_id.encode("utf-8")).hexdigest()[:16]