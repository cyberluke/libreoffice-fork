# -*- coding: utf-8 -*-
"""V271IntegrationJob: com.sun.star.task.Job component.

Bound to OnStartApp (bootstrap the integration) and OnCloseApp (best-effort
shutdown flush) via the extension's Jobs.xcu registration.
"""

import unohelper

from com.sun.star.task import XJob

from . import logutil
from .integration import V271Integration
from .unostrings import EVENT_ON_CLOSE_APP, EVENT_ON_START_APP

IMPLEMENTATION_NAME = "com.v271.office.V271IntegrationJob"


class V271IntegrationJob(unohelper.Base, XJob):

    def __init__(self, ctx):
        self.ctx = ctx

    def execute(self, arguments):
        try:
            event_name = None
            for item in (arguments or ()):
                if getattr(item, "Name", None) == "EventName":
                    event_name = item.Value
                    break
            if event_name == EVENT_ON_CLOSE_APP:
                V271Integration.instance(self.ctx).shutdown()
            else:
                # OnStartApp (and any other document/app event): ensure the
                # integration is running.
                V271Integration.instance(self.ctx).start()
        except Exception as exc:
            logutil.error("V271IntegrationJob failed: %s" % exc)
        return None


g_ImplementationHelper = unohelper.ImplementationHelper()
g_ImplementationHelper.addImplementation(
    V271IntegrationJob,
    IMPLEMENTATION_NAME,
    ("com.sun.star.task.Job",),
)