# -*- coding: utf-8 -*-
"""Tiny file logger for the V271 extension (no UI dependency).

Logs to <profile dir>/v271.log, also mirrors to stderr when running outside
LibreOffice. Never raises.
"""

import os
import sys
import threading

_lock = threading.Lock()
_log_path = None


def _path():
    global _log_path
    if _log_path is not None:
        return _log_path
    override = os.environ.get("V271_LOG_DIR", "").strip()
    base = override or os.environ.get("APPDATA") or os.path.expanduser("~/.config")
    _log_path = os.path.join(base, "v271", "v271.log")
    return _log_path


def _write(level, message):
    import datetime
    try:
        line = "%s %s %s\n" % (datetime.datetime.now().isoformat(timespec="seconds"),
                               level, message)
        with _lock:
            path = _path()
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "a", encoding="utf-8") as handle:
                handle.write(line)
        if not _in_office():
            sys.stderr.write(line)
    except Exception:
        pass


def _in_office():
    try:
        import uno  # noqa: F401
        return True
    except ImportError:
        return False


def info(message):
    _write("INFO", message)


def warn(message):
    _write("WARN", message)


def error(message):
    _write("ERROR", message)


def debug(message):
    if os.environ.get("V271_DEBUG"):
        _write("DEBUG", message)