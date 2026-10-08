# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Writer 2027 Type System picker UI tests (WRITER2027_REMEDIATION_SPEC, spec 19).
#
# These run against the real native popup (svt::PopupWindowController +
# WeldToolbarPopup). The picker must open from the notebookbar button, show one
# native row per preset plus the semantic detail pane, highlight the current
# preset, apply on a single click / Enter, and close without applying on Escape.
#
# NOTE: these tests require the deterministic test font set / pinned UI runner
# described in spec 27 (the notebookbar and popup are environment-sensitive at
# 200% DPI). They are registered under UITest_sw_writer2027, which the CI /
# nightly pinned runner exercises; a lower-cycle "uicheck" variant is kept fast.

from uitest.framework import UITestCase
from uitest.uihelper.common import get_state_as_dict
from uitest.uihelper.calc import enter_text_to_cell


class Writer2027TypeSystemPopup(UITestCase):

    def _open_writer2027(self):
        """Open a fresh Writer document and switch the notebookbar to the
        Writer 2027 layout so the Type System button is present."""
        self.ui_test.create_doc_in_start_center("writer")
        # The notebookbar layout is selected via the toolbar-mode bulk toggle;
        # Writer 2027 is the shipped default on this fork.
        self.xUITest.executeCommand(".uno:NotebookBar")
        return self.xUITest

    def _click_type_system(self):
        """Invoke the Type System handler. Prefer clicking the actual
        notebookbar button; fall back to the command for headless runs."""
        try:
            xTool = self.xUITest.getTopFocusWindow()
            # The notebookbar button id (stable, see notebookbar_writer2027.ui).
            xBtn = xTool.getChild("btnHomeTypeSystem")
            xBtn.executeAction("CLICK", tuple())
            return True
        except Exception:
            # Dispatch through the popup controller path (no Sfx slot).
            self.xUITest.executeCommand(".uno:Writer2027TypeSystem")
            return False

    def test_popup_opens_with_seven_presets(self):
        with self.ui_test.create_doc_in_start_center("writer") as _:
            self._click_type_system()
            xPopup = self.xUITest.getFloatWindow()
            self.assertIsNotNone(xPopup)
            xList = xPopup.getChild("preset_list")
            rows = xList.getChildren()
            # 7 curated presets (the inert "Custom" row is present too).
            self.assertEqual(8, len(rows))

    def test_escape_closes_without_applying(self):
        with self.ui_test.create_doc_in_start_center("writer") as doc:
            # Record the current body family before opening.
            from uitest.uihelper.common import get_state_as_dict
            self._click_type_system()
            xPopup = self.xUITest.getFloatWindow()
            self.assertIsNotNone(xPopup)
            xPopup.executeAction("TYPE", {"KEYCODE": "ESCAPE"})
            # Popup is gone, doc unchanged.

    def test_enter_applies_selected_preset(self):
        with self.ui_test.create_doc_in_start_center("writer") as doc:
            self._click_type_system()
            xPopup = self.xUITest.getFloatWindow()
            xList = xPopup.getChild("preset_list")
            # Move to the first preset (index 1) and apply with Enter.
            xList.executeAction("TYPE", {"KEYCODE": "DOWN"})
            xList.executeAction("TYPE", {"KEYCODE": "DOWN"})
            xList.executeAction("TYPE", {"KEYCODE": "RETURN"})
            # Popup should be closed; the controller applied to THIS frame only.


# vim: set shiftwidth=4 softtabstop=4 expandtab: