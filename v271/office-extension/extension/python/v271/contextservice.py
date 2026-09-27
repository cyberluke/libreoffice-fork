# -*- coding: utf-8 -*-
"""DocumentContextService: com.v271.office.DocumentContext component.

Typed context interface for V271 AI: exposes the current document's context
and AI bridge calls. Implemented as a Job so it can be invoked from scripts
and from the protocol handler:

    job = ctx.ServiceManager.createInstance("com.v271.office.DocumentContext")
    props = job.execute((PropertyValue("Command", 0, "context", 0),))

Commands:
    context        -> PropertyValue[] with the current document context
    summarize      -> PropertyValue "Summary"
    findRelated    -> PropertyValue "Items" (JSON list of related documents)
"""

import json

import unohelper

from com.sun.star.beans import PropertyValue
from com.sun.star.lang import XServiceInfo
from com.sun.star.task import XJob

from . import config as config_module
from . import context as context_module
from . import logutil
from .integration import V271Integration

IMPLEMENTATION_NAME = "com.v271.office.DocumentContext"
SERVICE_NAME = "com.v271.office.DocumentContext"


class DocumentContextService(unohelper.Base, XJob, XServiceInfo):

    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, arguments):
        command = "context"
        query = ""
        for item in (arguments or ()):
            name = getattr(item, "Name", None)
            if name == "Command":
                command = str(item.Value)
            elif name == "Query":
                query = str(item.Value)
        command = (command or "context").strip().lower()
        try:
            if command == "context":
                return self._cmd_context()
            if command == "summarize":
                return self._cmd_summarize()
            if command == "findrelated":
                return self._cmd_find_related(query)
            logutil.warn("unknown DocumentContext command: %r" % command)
        except Exception as exc:
            logutil.error("DocumentContext %s failed: %s" % (command, exc))
        return ()

    # -- commands ----------------------------------------------------------

    def _current_model(self):
        desktop = self.ctx.getServiceManager().createInstanceWithContext(
            "com.sun.star.frame.Desktop", self.ctx)
        return desktop.getCurrentComponent()

    def _cmd_context(self):
        model = self._current_model()
        if model is None:
            return (PropertyValue("Error", 0, "no document open", 0),)
        settings = config_module.settings_from_uno(self.ctx)
        store = config_module.V271Store.from_uno(self.ctx)
        context = context_module.build_context(model, settings, store)
        return _context_to_properties(context)

    def _cmd_summarize(self):
        model = self._current_model()
        if model is None:
            return (PropertyValue("Error", 0, "no document open", 0),)
        settings = config_module.settings_from_uno(self.ctx)
        store = config_module.V271Store.from_uno(self.ctx)
        integration = V271Integration.instance(self.ctx)
        summary, error = context_module.summarize(
            model, settings, integration._token_provider(), store)
        if error:
            return (PropertyValue("Error", 0, error, 0),)
        return (PropertyValue("Summary", 0, summary or "", 0),)

    def _cmd_find_related(self, query):
        if not query:
            return (PropertyValue("Error", 0, "Query argument missing", 0),)
        settings = config_module.settings_from_uno(self.ctx)
        integration = V271Integration.instance(self.ctx)
        items = context_module.find_related(
            query, settings, integration._token_provider(), k=5)
        return (PropertyValue("Items", 0, json.dumps(items, ensure_ascii=False), 0),
                PropertyValue("Query", 0, query, 0))

    # -- XServiceInfo ------------------------------------------------------

    def getImplementationName(self):
        return IMPLEMENTATION_NAME

    def supportsService(self, service_name):
        return service_name == SERVICE_NAME

    def getSupportedServiceNames(self):
        return (SERVICE_NAME,)


def _context_to_properties(context):
    ordered = [
        "source_id", "display_name", "file_type", "uri", "workspace_id",
        "favorite", "fingerprint", "module", "opened_at", "chunk_count",
    ]
    props = []
    for key in ordered:
        if key in context:
            props.append(PropertyValue(key, 0, context[key], 0))
    props.append(PropertyValue("text_excerpt", 0, context.get("text_excerpt", ""), 0))
    return tuple(props)


g_ImplementationHelper = unohelper.ImplementationHelper()
g_ImplementationHelper.addImplementation(
    DocumentContextService,
    IMPLEMENTATION_NAME,
    (SERVICE_NAME,),
)