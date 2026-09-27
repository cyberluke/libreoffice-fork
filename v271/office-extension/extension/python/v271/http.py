# -*- coding: utf-8 -*-
"""Minimal JSON HTTP client used by the V271 extension.

Standard library only (urllib), so it runs both inside LibreOffice's bundled
Python and in plain system Python (unit tests, evidence harness).
"""

import json
import urllib.error
import urllib.request


class HttpError(Exception):
    """Raised for non-2xx responses; carries status and the parsed body."""

    def __init__(self, status, body, url):
        super().__init__("HTTP %d from %s: %s" % (status, url, _preview(body)))
        self.status = status
        self.body = body
        self.url = url


class NetworkError(Exception):
    """Raised when the V271 endpoint cannot be reached at all."""


def _preview(body, limit=400):
    if isinstance(body, dict):
        body = json.dumps(body, ensure_ascii=False)
    text = str(body)[:limit]
    return text


def request_json(method, url, payload=None, headers=None, timeout=20.0,
                 bearer=None):
    """Perform a JSON request and return (status, parsed_json).

    payload is serialized as JSON. bearer adds an Authorization header.
    Raises HttpError for non-2xx, NetworkError for transport failures.
    """
    req_headers = {"Accept": "application/json"}
    if payload is not None:
        req_headers["Content-Type"] = "application/json"
    if bearer:
        req_headers["Authorization"] = "Bearer %s" % (bearer,)
    if headers:
        req_headers.update(headers)
    data = None
    if payload is not None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    request = urllib.request.Request(url, data=data, headers=req_headers,
                                     method=method.upper())
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read()
            status = response.status
    except urllib.error.HTTPError as error:
        raw = error.read() if hasattr(error, "read") else b""
        try:
            parsed = json.loads(raw.decode("utf-8", errors="replace"))
        except (ValueError, UnicodeDecodeError):
            parsed = raw.decode("utf-8", errors="replace")
        raise HttpError(error.code, parsed, url)
    except urllib.error.URLError as error:
        raise NetworkError("%s: %s" % (url, error.reason))
    except OSError as error:
        raise NetworkError("%s: %s" % (url, error))

    if not body:
        return status, None
    try:
        return status, json.loads(body.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return status, body.decode("utf-8", errors="replace")


def post_form(url, form, timeout=20.0):
    """application/x-www-form-urlencoded POST (OAuth token endpoint)."""
    import urllib.parse
    data = urllib.parse.urlencode(form).encode("utf-8")
    request = urllib.request.Request(url, data=data, method="POST")
    request.add_header("Accept", "application/json")
    request.add_header("Content-Type", "application/x-www-form-urlencoded")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read()
            status = response.status
    except urllib.error.HTTPError as error:
        raw = error.read() if hasattr(error, "read") else b""
        try:
            parsed = json.loads(raw.decode("utf-8", errors="replace"))
        except (ValueError, UnicodeDecodeError):
            parsed = raw.decode("utf-8", errors="replace")
        raise HttpError(error.code, parsed, url)
    except urllib.error.URLError as error:
        raise NetworkError("%s: %s" % (url, error.reason))
    except OSError as error:
        raise NetworkError("%s: %s" % (url, error))
    if not body:
        return status, None
    try:
        return status, json.loads(body.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return status, body.decode("utf-8", errors="replace")