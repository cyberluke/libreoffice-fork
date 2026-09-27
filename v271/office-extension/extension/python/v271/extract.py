# -*- coding: utf-8 -*-
"""Text extraction from LibreOffice documents.

No separate extraction model: this uses the document's own UNO API to obtain
plain text with provenance (page/slide/sheet/section where available), which
is then chunked and handed to the central V271 vector pipeline.
"""

from . import logutil

_MAX_CELLS = 10000          # hard cap on extracted Calc cells
_MAX_TOTAL_CHARS = 500000   # safety cap on extracted text


class ExtractedDocument:
    def __init__(self, parts):
        self.parts = parts  # list of (provenance, text)

    @property
    def total_chars(self):
        return sum(len(text) for _, text in self.parts)

    def as_list(self):
        return list(self.parts)


def _clip(parts):
    total = 0
    clipped = []
    for provenance, text in parts:
        if total >= _MAX_TOTAL_CHARS:
            break
        remaining = _MAX_TOTAL_CHARS - total
        if len(text) > remaining:
            text = text[:remaining]
        clipped.append((provenance, text))
        total += len(text)
    return clipped


def _extract_writer(model):
    from com.sun.star.text import XText
    text = model.queryInterface(XText)
    if text is None:
        return []
    content = text.getString()
    if not content or not content.strip():
        return []
    return [("document", content.strip())]


def _extract_calc(model):
    from com.sun.star.sheet import XSpreadsheetDocument
    doc = model.queryInterface(XSpreadsheetDocument)
    if doc is None:
        return []
    parts = []
    sheets = doc.getSheets()
    count = min(sheets.getCount(), 64)
    for index in range(count):
        sheet = sheets.getByIndex(index)
        sheet_name = sheet.getName()
        try:
            cursor = sheet.createCursor()
            cursor.gotoEndOfUsedArea(False)
            addr = cursor.getRangeAddress()
            max_col = min(int(addr.EndColumn), 127)
            max_row = min(int(addr.EndRow), 9999)
        except Exception as exc:
            logutil.debug("calc cursor failed on %r: %s" % (sheet_name, exc))
            max_col, max_row = 63, 999
        cells = 0
        lines = []
        for row in range(max_row + 1):
            row_parts = []
            for col in range(max_col + 1):
                if cells >= _MAX_CELLS:
                    break
                try:
                    cell = sheet.getCellByPosition(col, row)
                    value = cell.getString()
                except Exception:
                    value = ""
                cells += 1
                if value and value.strip():
                    row_parts.append(value.strip().replace("\n", " "))
            if row_parts:
                lines.append(" | ".join(row_parts))
            if len(lines) >= 512:
                break
        if lines:
            parts.append(("sheet:%s" % sheet_name, "\n".join(lines)))
    return parts


def _extract_draw_impress(model):
    from com.sun.star.drawing import XDrawPagesSupplier
    from com.sun.star.text import XText
    supplier = model.queryInterface(XDrawPagesSupplier)
    if supplier is None:
        return []
    parts = []
    pages = supplier.getDrawPages()
    count = min(pages.getCount(), 512)
    for index in range(count):
        page = pages.getByIndex(index)
        shapes = page.getShapes()
        lines = []
        for shape_index in range(shapes.getCount()):
            shape = shapes.getByIndex(shape_index)
            text = shape.queryInterface(XText)
            if text is None:
                continue
            content = text.getString()
            if content and content.strip():
                lines.append(content.strip().replace("\n", " "))
        if lines:
            parts.append(("slide:%d" % (index + 1), "\n".join(lines)))
    return parts


def extract_text(model):
    """Extract (provenance, text) parts from a document model.

    Never raises; returns an ExtractedDocument (possibly empty).
    """
    if model is None:
        return ExtractedDocument([])
    parts = []
    for extractor in (_extract_writer, _extract_calc, _extract_draw_impress):
        try:
            parts = extractor(model)
            if parts:
                break
        except Exception as exc:
            logutil.debug("extractor %s failed: %s" % (getattr(extractor, "__name__", "?"), exc))
    return ExtractedDocument(_clip(parts))