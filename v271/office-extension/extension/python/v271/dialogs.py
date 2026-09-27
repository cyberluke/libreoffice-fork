# -*- coding: utf-8 -*-
"""Main-thread UI helpers (message box + simple input dialog).

Everything is guarded: a failing dialog must never crash the office, so all
errors are logged instead.
"""

from . import logutil
from .unostrings import SERVICE_TOOLKIT


def _toolkit(ctx):
    return ctx.getServiceManager().createInstanceWithContext(SERVICE_TOOLKIT, ctx)


def _parent_window(ctx):
    try:
        desktop = ctx.getServiceManager().createInstanceWithContext(
            "com.sun.star.frame.Desktop", ctx)
        frame = desktop.getCurrentFrame()
        if frame is None:
            return None
        window = frame.getContainerWindow()
        if window is None:
            return None
        from com.sun.star.awt import XWindowPeer
        peer = window.queryInterface(XWindowPeer)
        return peer
    except Exception:
        return None


def show_message(ctx, title, text, kind="info"):
    """Show a modal message box. kind: info|warning|error|query."""
    try:
        toolkit = _toolkit(ctx)
        from com.sun.star.awt import MessageBoxType, MessageBoxButtons
        types = {"info": MessageBoxType.MESSAGEBOX_INFO,
                 "warning": MessageBoxType.MESSAGEBOX_WARNING,
                 "error": MessageBoxType.MESSAGEBOX_ERROR,
                 "query": MessageBoxType.MESSAGEBOX_QUERY}
        buttons = MessageBoxButtons.BUTTONS_OK
        box = toolkit.createMessageBox(
            _parent_window(ctx), types.get(kind, MessageBoxType.MESSAGEBOX_INFO),
            buttons, title or "V271", text or "")
        box.execute()
    except Exception as exc:
        logutil.error("message box failed (%s): %s" % (title, exc))


def ask_text(ctx, title, label, initial=""):
    """Ask for a single text value. Returns the string or None on cancel."""
    try:
        smgr = ctx.getServiceManager()
        model = smgr.createInstanceWithContext(
            "com.sun.star.awt.UnoControlDialogModel", ctx)
        model.setPropertyValue("Title", title or "V271")
        model.setPropertyValue("Width", 340)
        model.setPropertyValue("Height", 150)
        model.setPropertyValue("Closeable", True)
        model.setPropertyValue("Moveable", True)

        from com.sun.star.beans import PropertyValue

        def pv(name, value):
            return PropertyValue(name, 0, value, 0)

        model.insertByName("prompt", (
            pv("Type", "Label"), pv("PositionX", 12), pv("PositionY", 12),
            pv("Width", 316), pv("Height", 20), pv("Label", label)))
        model.insertByName("value", (
            pv("Type", "Edit"), pv("PositionX", 12), pv("PositionY", 38),
            pv("Width", 316), pv("Height", 26), pv("Text", initial)))
        model.insertByName("ok", (
            pv("Type", "Button"), pv("PositionX", 130), pv("PositionY", 84),
            pv("Width", 96), pv("Height", 28), pv("Label", "OK"),
            pv("PushButtonType", 0), pv("DefaultButton", True)))
        model.insertByName("cancel", (
            pv("Type", "Button"), pv("PositionX", 232), pv("PositionY", 84),
            pv("Width", 96), pv("Height", 28), pv("Label", "Cancel"),
            pv("PushButtonType", 1)))

        toolkit = _toolkit(ctx)
        dialog = toolkit.createDialog(model)
        result = dialog.execute()
        if result != 1:
            return None
        edit = model.getByName("value")
        return edit.getPropertyValue("Text") or ""
    except Exception as exc:
        logutil.error("input dialog failed (%s): %s" % (title, exc))
        return None