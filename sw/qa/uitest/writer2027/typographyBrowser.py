# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Writer 2027 Typography Browser UI tests (remediation spec 30).
#
# The browser list is a custom drawing surface (Writer2027TypographyList) on a
# GtkDrawingArea, so the harness verifies open/search/close/reopen behaviour
# (the paint-level invariants are covered by the golden comparator layer):
# - open, search, clear search, close, reopen
# - no crash and a stable popup on repeated open/close

from uitest.framework import UITestCase


class Writer2027TypographyBrowser(UITestCase):

    def _open_font_browser(self):
        self.xUITest.executeCommand(".uno:CharFontName")
        return self.xUITest.getFloatWindow()

    def test_open_search_clear_close_reopen(self):
        with self.ui_test.create_doc_in_start_center("writer") as _:
            xPopup = self._open_font_browser()
            self.assertIsNotNone(xPopup)
            xSearch = xPopup.getChild("search")
            xSearch.executeAction("TYPE", {"TEXT": "Inter"})
            xSearch.executeAction("TYPE", {"KEYCODE": "ESCAPE"})
            xPopup.executeAction("TYPE", {"KEYCODE": "ESCAPE"})
            # Reopen.
            xPopup2 = self._open_font_browser()
            self.assertIsNotNone(xPopup2)
            xPopup2.executeAction("TYPE", {"KEYCODE": "ESCAPE"})

    def test_open_close_loop(self):
        with self.ui_test.create_doc_in_start_center("writer") as _:
            for i in range(10):
                xPopup = self._open_font_browser()
                self.assertIsNotNone(xPopup)
                xPopup.executeAction("TYPE", {"KEYCODE": "ESCAPE"})


# vim: set shiftwidth=4 softtabstop=4 expandtab: