# -*- coding: utf-8 -*-
"""V271ProtocolHandler: com.sun.star.frame.ProtocolHandler component.

Handles vnd.v271.office:* URLs (Add-ons menu entries and direct dispatch):
  signin | signout | status | syncnow | favorite | linkworkspace |
  configure | context

Dispatches run on the main thread; network-heavy commands queue work on the
integration's background worker.
"""

import unohelper

from com.sun.star.beans import PropertyValue
from com.sun.star.frame import XDispatch, XDispatchProvider, XInitialization
from com.sun.star.lang import XServiceInfo
from com.sun.star.util import URL

from . import logutil
from .integration import V271Integration
from .unostrings import PROTOCOL_PREFIX

IMPLEMENTATION_NAME = "com.v271.office.ProtocolHandler"
SERVICE_NAME = "com.sun.star.frame.ProtocolHandler"

_COMMANDS = {
    "signin": lambda i: i.command_signin(),
    "signout": lambda i: i.command_signout(),
    "status": lambda i: i.command_status(),
    "syncnow": lambda i: i.command_syncnow(),
    "favorite": lambda i: i.command_toggle_favorite(),
    "linkworkspace": lambda i: i.command_link_workspace(),
    "configure": lambda i: i.command_configure(),
    "context": lambda i: i.command_show_context(),
}


class V271ProtocolHandler(unohelper.Base, XDispatchProvider, XDispatch,
                          XInitialization, XServiceInfo):

    def __init__(self, ctx):
        self.ctx = ctx
        self._current_url = None

    # -- XInitialization ---------------------------------------------------

    def initialize(self, arguments):
        try:
            for item in (arguments or ()):
                if getattr(item, "Name", None) == "URL":
                    self._current_url = item.Value
        except Exception:
            pass

    # -- XDispatchProvider -------------------------------------------------

    def getDispatcher(self, url):
        if url.Protocol == "vnd.v271.office" or \
                (url.Complete or "").startswith(PROTOCOL_PREFIX):
            return self
        return None

    def getSupportedURLPatterns(self):
        return ("vnd.v271.office:*",)

    # -- XDispatch ---------------------------------------------------------

    def dispatch(self, url, arguments):
        try:
            command = (url.Path or "").strip().lower()
            if not command and url.Complete:
                command = url.Complete.split(":", 1)[-1].strip().lower()
            handler = _COMMANDS.get(command)
            if handler is None:
                logutil.warn("unknown v271 command: %r" % command)
                return
            integration = V271Integration.instance(self.ctx)
            handler(integration)
        except Exception as exc:
            logutil.error("v271 dispatch failed: %s" % exc)

    def addStatusListener(self, control, url):
        pass

    def removeStatusListener(self, control, url):
        pass

    # -- XServiceInfo ------------------------------------------------------

    def getImplementationName(self):
        return IMPLEMENTATION_NAME

    def supportsService(self, service_name):
        return service_name == SERVICE_NAME

    def getSupportedServiceNames(self):
        return (SERVICE_NAME,)


g_ImplementationHelper = unohelper.ImplementationHelper()
g_ImplementationHelper.addImplementation(
    V271ProtocolHandler,
    IMPLEMENTATION_NAME,
    (SERVICE_NAME,),
)