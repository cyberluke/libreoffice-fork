/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027TYPESYSTEMPOPUP_HXX
#define INCLUDED_SVX_WRITER2027TYPESYSTEMPOPUP_HXX

#include <svx/svxdllapi.h>

#include <svx/writer2027typesystem.hxx>
#include <svtools/toolbarmenu.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/Label.hxx>
#include <vcl/weld/TreeView.hxx>
#include <tools/link.hxx>

#include <memory>
#include <vector>

namespace svx::writer2027
{

/** One preset row in the Writer 2027 Type System picker.

    Pure model data: built once per open, before any geometry or widget
    population, with the resolved semantic roles stored explicitly so the UI
    never re-resolves fonts during paint/layout. Only real presets live here;
    "Custom typography" is document state shown in the CURRENT header, never a
    fake selectable row (remediation spec 18).
*/
struct TypeSystemPickerRow
{
    OUString maPresetId;
    OUString maDisplayName;

    OUString maHeadingFamily;
    OUString maBodyFamily;
    OUString maMonoFamily;
    OUString maDisplayFamily;

    int mnMissingCount = 0; // roles whose preferred family is not installed
    bool mbCurrent = false;
};

/** Writer 2027 Type System picker content, hosted as a WeldToolbarPopup.

    The framework's PopupWindowController owns the anchor (the invoking toolbar
    item) and the popup lifecycle, so the picker opens under the button at any
    DPI rather than as a sidebar. Product layout (remediation spec 17):
    left column = CURRENT state + PRESETS list (selectable systems only),
    right column = one rich preview panel (Heading / Body / Code / Scale /
    Fallbacks) with fixed UI sample text, and an Apply button.

    Interaction (spec 19): selecting/highlighting a preset only previews the
    detail panel; the Apply button (or Enter) applies the selected system to
    the owning frame's document and closes. Escape closes without applying.
    Nothing is mutated on open or hover.
 */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemPopup : public WeldToolbarPopup
{
public:
    Writer2027TypeSystemPopup(weld::Widget* pParent);
    ~Writer2027TypeSystemPopup() override;

    /** Return the popup to the owner (PopupWindowController::weldPopupWindow). */
    static std::unique_ptr<Writer2027TypeSystemPopup> Create(weld::Widget* pParent);

    /** Called (by the owner) before showing with the current preset id
        (empty = custom) so the list highlights it and the CURRENT header is
        set. */
    void SetCurrentPreset(const OUString& rPresetId);

    /** Connect the apply handler; receives the preset id when the user
        applies (Apply button / Enter). */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

    // Exposed for tests (geometry invariant checks).
    const std::vector<TypeSystemPickerRow>& GetModelRows() const { return maModel; }

private:
    DECL_LINK(TreeSelectionHdl, weld::ItemView&, void);
    DECL_LINK(TreeKeyHdl, const KeyEvent&, bool);
    DECL_LINK(ApplyButtonHdl, weld::Button&, void);

    void BuildModel();         // pure data, no tree/geometry
    void PopulatePresetList(); // clear + repopulate the tree (presets only)
    void UpdateDetailPanel();  // fill the preview panel for the selected preset
    void ApplySelected();      // apply the selected preset via the select link

    virtual void GrabFocus() override;

    std::unique_ptr<weld::TreeView> m_xRows;
    std::unique_ptr<weld::Label> m_xCurrentLabel;
    std::unique_ptr<weld::Label> m_xDetailName;
    std::unique_ptr<weld::Label> m_xRoleHeading;
    std::unique_ptr<weld::Label> m_xPreviewHeading;
    std::unique_ptr<weld::Label> m_xRoleBody;
    std::unique_ptr<weld::Label> m_xPreviewBody;
    std::unique_ptr<weld::Label> m_xRoleMono;
    std::unique_ptr<weld::Label> m_xPreviewMono;
    std::unique_ptr<weld::Label> m_xRoleScale;
    std::unique_ptr<weld::Label> m_xFallbackStatus;
    std::unique_ptr<weld::Button> m_xApplyButton;

    std::vector<TypeSystemPickerRow> maModel;
    OUString maCurrentPreset; // empty = custom
    bool mbInternalMove = false;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEMPOPUP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */