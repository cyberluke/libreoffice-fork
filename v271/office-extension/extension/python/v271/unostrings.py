# -*- coding: utf-8 -*-
"""UNO service / interface name constants for the V271 extension."""

SERVICE_DESKTOP = "com.sun.star.frame.Desktop"
SERVICE_GLOBAL_EVENT_BROADCASTER = "com.sun.star.frame.GlobalEventBroadcaster"
SERVICE_CONFIGURATION_PROVIDER = "com.sun.star.configuration.ConfigurationProvider"
SERVICE_CONFIGURATION_ACCESS = "com.sun.star.configuration.ConfigurationAccess"
SERVICE_CONFIGURATION_UPDATE_ACCESS = "com.sun.star.configuration.ConfigurationUpdateAccess"
SERVICE_TOOLKIT = "com.sun.star.awt.Toolkit"

IMPLEMENTATION_JOB = "com.v271.office.V271IntegrationJob"
IMPLEMENTATION_PROTOCOL_HANDLER = "com.v271.office.ProtocolHandler"
IMPLEMENTATION_DOCUMENT_CONTEXT = "com.v271.office.DocumentContext"

SERVICE_JOB = "com.sun.star.task.Job"
SERVICE_PROTOCOL_HANDLER = "com.sun.star.frame.ProtocolHandler"
SERVICE_DOCUMENT_CONTEXT = "com.v271.office.DocumentContext"

PROTOCOL_PREFIX = "vnd.v271.office:"

# Document event names delivered by the GlobalEventBroadcaster.
EVENT_ON_NEW = "OnNew"
EVENT_ON_CREATE = "OnCreate"
EVENT_ON_LOAD_FINISHED = "OnLoadFinished"
EVENT_ON_SAVE = "OnSave"
EVENT_ON_SAVE_DONE = "OnSaveDone"
EVENT_ON_SAVE_AS = "OnSaveAs"
EVENT_ON_SAVE_AS_DONE = "OnSaveAsDone"
EVENT_ON_PREPARE_UNLOAD = "OnPrepareUnload"
EVENT_ON_UNLOAD = "OnUnload"
EVENT_ON_CLOSE_APP = "OnCloseApp"
EVENT_ON_START_APP = "OnStartApp"