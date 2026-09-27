# -*- coding: utf-8 -*-
"""Small shared helpers (ISO time, safe conversions)."""

import datetime


def now_iso():
    """Current UTC time as ISO-8601 with seconds and Z suffix."""
    return datetime.datetime.now(datetime.timezone.utc).replace(
        microsecond=0).isoformat().replace("+00:00", "Z")


def iso_from_uno_datetime(value):
    """Convert com.sun.star.util.DateTime to an ISO-8601 string.

    The UNO DateTime carries Year..Nanoseconds fields; timezone handling of
    legacy documents is best-effort (treated as local time).
    """
    if value is None:
        return None
    try:
        return "%04d-%02d-%02dT%02d:%02d:%02d" % (
            value.Year, value.Month, value.Day,
            value.Hours, value.Minutes, value.Seconds)
    except AttributeError:
        return None


def safe_str(value, fallback=""):
    if value is None:
        return fallback
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value)