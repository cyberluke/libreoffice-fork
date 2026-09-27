# -*- coding: utf-8 -*-
"""Access to LibreOffice's native recent-documents list.

The extension seeds the V271 graph from this list (document.recent entities)
but never duplicates it into a second local store -- the graph stays the
authoritative mirror and NAI reads from the graph.
"""

from . import logutil

CONFIG_HISTORY = "/org.openoffice.Office.Common/History/HistoryList"


def iter_history(ctx, limit=100):
    """Yield recent-document dicts {url, title, filter_name, pinned}.

    Reads the native ItemList/OrderList structure used by the Start Center.
    """
    if ctx is None:
        return
    try:
        provider = ctx.getServiceManager().createInstanceWithContext(
            "com.sun.star.configuration.ConfigurationProvider", ctx)
        from com.sun.star.beans import PropertyValue
        node = PropertyValue("nodepath", 0, CONFIG_HISTORY, 0)
        access = provider.createInstanceWithArguments(
            "com.sun.star.configuration.ConfigurationAccess", (node,))
    except Exception as exc:
        logutil.debug("history access failed: %s" % exc)
        return

    try:
        item_list = access.getByName("ItemList")
        order_list = access.getByName("OrderList")
        from com.sun.star.beans import XPropertySet
        order = []
        for index in range(order_list.getCount()):
            entry = order_list.getByIndex(index)
            ref = entry.queryInterface(XPropertySet).getPropertyValue("HistoryItemRef")
            if ref:
                order.append(ref)
        seen = set()
        for url in order:
            if url in seen or not item_list.hasByName(url):
                continue
            seen.add(url)
            try:
                item = item_list.getByName(url)
                props = item.queryInterface(XPropertySet)
                info = item.getPropertySetInfo()
                pinned = False
                if info.hasPropertyByName("Pinned"):
                    pinned = bool(props.getPropertyValue("Pinned"))
                yield {
                    "url": url,
                    "title": props.getPropertyValue("Title") or "",
                    "filter_name": props.getPropertyValue("Filter") or "",
                    "pinned": pinned,
                }
            except Exception:
                continue
            if len(seen) >= limit:
                return
    except Exception as exc:
        logutil.debug("history enumeration failed: %s" % exc)