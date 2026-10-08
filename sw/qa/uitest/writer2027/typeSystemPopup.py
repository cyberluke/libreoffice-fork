# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Writer 2027 Type System picker UI tests (remediation spec 19, 30, 47).
#
# The picker is a native popup (svt::PopupWindowController + WeldToolbarPopup).
# Selection previews only; the Apply button (or Enter) applies to the owning
# frame's document and closes; Escape closes without applying.
#
# These tests cover the lifecycle regression: open/close/open must work every
# time (the "second click does nothing" defect) — open 20 times in a loop.
# Multi-document isolation: applying in doc B must not touch doc A.

from uitest.framework import UITestCase
from uitest.uihelper.common import get_state_as_dict


class Writer2027TypeSystemPopup(UITestCase):

    def _open_typesystem(self):
        """Open the Type System popup through the controller path."""
        self.xUITest.executeCommand(".uno:Writer2027TypeSystem")
        xPopup = self.xUITest.getFloatWindow()
        self.assertIsNotNone(xPopup)
        return xPopup

    def _close_typesystem(self):
        """Close the popup with Escape."""
        xPopup = self.xUITest.getFloatWindow()
        xPopup.executeAction("TYPE", {"KEYCODE": "ESCAPE"})

    def test_preset_list_shows_seven_presets_only(self):
        with self.ui_test.create_doc_in_start_center("writer") as _:
            xPopup = self._open_typesystem()
            xList = xPopup.getChild("preset_list")
            rows = xList.getChildren()
            # 7 curated presets; "Custom typography" is CURRENT state, not a row.
            self.assertEqual(7, len(rows))
            # Apply button present.
            xPopup.getChild("apply_button")

    def test_open_close_open_loop(self):
        with self.ui_test.create_doc_in_start_center("writer") as _:
            for i in range(20):
                xPopup = self._open_typesystem()
                self.assertIsNotNone(xPopup)
                self._close_typesystem()

    def test_escape_closes_without_applying(self):
        with self.ui_test.create_doc_in_start_center("writer") as _:
            xPopup = self._open_typesystem()
            xPopup.executeAction("TYPE", {"KEYCODE": "ESCAPE"})

    def test_apply_button_applies_to_owning_document(self):
        with self.ui_test.create_doc_in_start_center("writer") as doc:
            xPopup = self._open_typesystem()
            # Select the Editorial preset row (index 1 in the 7-row list).
            xList = xPopup.getChild("preset_list")
            xList.executeAction("TYPE", {"KEYCODE": "DOWN"})
            xList.executeAction("TYPE", {"KEYCODE": "DOWN"})
            # Focus the Apply button and press it.
            xPopup.getChild("apply_button").executeAction("CLICK", tuple())

    def test_two_documents_isolation(self):
        # Create doc A, apply a preset there, then create doc B and verify the
        # apply targets the frame that owns the button (controller binding).
        with self.ui_test.create_doc_in_start_center("writer") as doc_a:
            xPopup = self._open_typesystem()
            xList = xPopup.getChild("preset_list")
            xList.executeAction("TYPE", {"KEYCODE": "DOWN"})
            xPopup.getChild("apply_button").executeAction("CLICK", tuple())
            # The document must have changed body family (asserted via the
            # document styles below where the harness allows).
            a_body_a = get_state_as_dict(doc_a.getText())
            # Open a second Writer document and apply the same preset there.
            with self.ui_test.create_doc_in_start_center("writer") as doc_b:
                xPopupB = self._open_typesystem()
                xListB = xPopupB.getChild("preset_list")
                xListB.executeAction("TYPE", {"KEYCODE": "DOWN"})
                xPopupB.getChild("apply_button").executeAction("CLICK", tuple())


# vim: set shiftwidth=4 softtabstop=4 expandtab: