# WriterAgent - AI Writing Assistant for LibreOffice
# Copyright (c) 2026 KeithCu
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for the chat sidebar layout contract.

These verify the *truthful* ``LayoutSize.Minimum`` machinery: a panel must
report the height at which its fixed (non-scrollable) bottom cluster plus a
minimum transcript still fit, so ``sfx2 DeckLayouter`` turns on the outer
vertical scrollbar exactly when the docked height is below that floor.
Reporting a flat 100 px (the old bug) made LibreOffice believe the fixed
controls can always be shrunk into view, which clipped their bottom row with
no scrollbar to reach it.

``module.match`` rule: source module ``plugin/chatbot/panel_resize.py``.
"""

from __future__ import annotations

import sys

import pytest

sys.path.insert(0, "plugin")  # noqa: PLC0415 -- repo-root module layout, not packaging

from plugin.chatbot.panel_resize import (  # noqa: E402
    _PanelResizeListener,
    compute_chat_panel_layout,
    compute_min_panel_height,
)

# Geometry mirroring ChatPanelDialog.xdl: response top=16 height=110,
# status top=128 (gap 2 below response). The bottom cluster spans well below.
SNAPSHOT: dict[str, tuple[int, int, int, int]] = {
    "response": (8, 16, 300, 110),
    "status": (8, 128, 300, 25),
    "query_label": (8, 160, 60, 20),
    "query": (8, 182, 300, 40),
    "send": (8, 224, 90, 32),
    "stop": (104, 224, 90, 32),
    "clear": (200, 224, 90, 32),
    "chk_voice": (240, 160, 24, 20),
    "chat_mode_selector": (8, 258, 300, 30),
    "model_label": (8, 290, 60, 22),
    "model_selector": (8, 314, 300, 30),
    "image_model_selector": (8, 346, 300, 24),
    "aspect_ratio_selector": (8, 372, 300, 24),
    "base_size_label": (8, 398, 60, 22),
    "base_size_input": (8, 422, 120, 24),
    "slash_popup": (8, 500, 300, 200),  # overlay; parked, not the floor
}


def test_compute_min_panel_height_is_truthful_floor() -> None:
    # Independent expected value, hardcoded from the literal SNAPSHOT geometry
    # (NOT recomputed through the production formula, so this catches a wrong
    # bottom_margin/response_gap/min_response_height constant in the future):
    #   transcript top        = response.y        = 16
    #   min transcript height = _MIN_RESPONSE     = 30
    #   response gap          = _XDL_GAP          = 2
    #   fixed cluster height  = 446 (base_size_input bottom) - 128 (status top) = 318
    #   bottom margin         = _BOTTOM_MARGIN    = 20
    #   => floor = 16 + 30 + 2 + 318 + 20 = 386
    assert compute_min_panel_height(SNAPSHOT) == 386
    # Definitely far above the old broken flat 100 px default.
    assert compute_min_panel_height(SNAPSHOT) > 100


def test_layout_never_overlaps_transcript_below_minimum() -> None:
    """A pure layout call below the reachable floor must not overlap the fixed
    bottom cluster with the transcript — the cluster stays at its natural spot
    and the transcript at its minimum, ready for the deck scrollbar to reveal."""
    min_h = compute_min_panel_height(SNAPSHOT)
    layouts = compute_chat_panel_layout(320, min_h - 50, SNAPSHOT)
    response = layouts["response"]
    status = layouts["status"]
    assert response.height == 30  # transcript at its floor
    # Cluster never collides with the transcript: status sits below it + gap.
    assert status.y >= response.y + response.height + 2


def test_layout_at_minimum_is_reachable() -> None:
    min_h = compute_min_panel_height(SNAPSHOT)
    layouts = compute_chat_panel_layout(320, min_h, SNAPSHOT)
    assert "response" in layouts
    response = layouts["response"]
    # At the floor the transcript is exactly its minimum height.
    assert response.height == 30
    # Every visible control sits on or inside the panel with the bottom margin
    # to spare: fixed bottom cluster is reachable, nothing is pushed offscreen.
    max_bottom = max(rect.y + rect.height for name, rect in layouts.items() if name != "slash_popup")
    assert max_bottom <= min_h - 20  # _BOTTOM_MARGIN
    # The transcript bottom leaves room for the cluster + gap below it.
    assert layouts["status"].y >= response.y + response.height + 2


def test_layout_grows_response_above_minimum() -> None:
    layouts = compute_chat_panel_layout(320, 600, SNAPSHOT)
    response = layouts["response"]
    assert response.height > 30
    max_bottom = max(rect.y + rect.height for name, rect in layouts.items() if name != "slash_popup")
    assert max_bottom <= 600


def test_min_height_requires_response_control() -> None:
    # Without the transcript there is no response to size the cluster against.
    assert compute_min_panel_height({}) == 0
    no_response = {k: v for k, v in SNAPSHOT.items() if k != "response"}
    assert compute_min_panel_height(no_response) == 0


class _Rect:
    """Attributed rectangle matching UNO awt.Rectangle (X/Y/Width/Height)."""

    def __init__(self, x: int, y: int, w: int, h: int) -> None:
        self.X = x
        self.Y = y
        self.Width = w
        self.Height = h

    def __iter__(self):
        return iter((self.X, self.Y, self.Width, self.Height))

    def __repr__(self) -> str:
        return f"_Rect({self.X}, {self.Y}, {self.Width}, {self.Height})"


class _FakeCtrl:
    def __init__(self, x: int, y: int, w: int, h: int) -> None:
        self.rect = _Rect(x, y, w, h)

    def getPosSize(self) -> _Rect:
        return self.rect

    def setPosSize(self, x: int, y: int, w: int, h: int, _flags: int) -> None:
        self.rect = _Rect(x, y, w, h)


class _FakeWin:
    def __init__(self, w: int, h: int) -> None:
        self.rect = _Rect(0, 0, w, h)

    def getPosSize(self) -> _Rect:
        return self.rect


def _make_listener() -> _PanelResizeListener:
    controls = {name: _FakeCtrl(*rect) for name, rect in SNAPSHOT.items()}
    listener = _PanelResizeListener(controls)
    return listener


def test_listener_reports_snapshot_min_height() -> None:
    listener = _make_listener()
    assert listener.min_panel_height == 0  # no snapshot yet
    listener._capture_snapshot(_FakeWin(320, 600))
    assert listener.min_panel_height == compute_min_panel_height(SNAPSHOT)
    assert listener.min_panel_height > 100


def test_relayout_clamps_short_allocation_to_minimum() -> None:
    """A deck that hands a height below the real floor must not crush the
    fixed cluster; the layout must come out as if the panel were the minimum
    tall (the outer deck scrollbar then reveals the overflow)."""
    listener = _make_listener()
    min_h = compute_min_panel_height(SNAPSHOT)
    # Pre-seed the snapshot the way the deck's first real call does.
    listener._capture_snapshot(_FakeWin(320, min_h))
    listener._viewport_w = 320
    win = _FakeWin(320, min_h - 40)  # too short: below the reachable floor
    listener.relayout_now(win)
    # Fixed bottom controls still reachable inside the (virtual minimum) panel.
    response = listener._c["response"].rect
    status = listener._c["status"].rect
    assert response.Height == 30  # staying at the min transcript height
    assert status.Y >= response.Y + response.Height + 2  # not overlapping the transcript
    bottom_controls = [
        listener._c[n].rect for n in (
            "send", "stop", "clear", "chat_mode_selector", "model_selector",
            "image_model_selector", "base_size_input", "aspect_ratio_selector",
        )
    ]
    # Nothing is pushed above the viewport top; the cluster retained its
    # natural vertical ordering (no negative Y / collapse).
    assert all(rect.Y >= response.Y for rect in bottom_controls)


def test_relayout_normal_height_is_not_clamped() -> None:
    listener = _make_listener()
    listener._capture_snapshot(_FakeWin(320, 600))
    listener._viewport_w = 320
    listener.relayout_now(_FakeWin(320, 600))
    response = listener._c["response"].rect
    assert response.Height > 30  # grew to fill the taller panel