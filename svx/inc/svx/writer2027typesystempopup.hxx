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
#include <vcl/weld/Label.hxx>
#include <vcl/weld/TreeView.hxx>
#include <tools/link.hxx>

#include <memory>
#include <vector>

namespace svx::writer2027
{

/** One preset row in the Writer 2027 Type System picker (spec 4.1).

    Pure model data: built once per open, before any geometry or widget
    population, with the resolved semantic roles stored explicitly so the UI
    never re-resolves fonts during paint/layout.
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
    DPI rather than as a sidebar (spec 2.1). Content is a native one-line-per-
    preset list plus a semantic detail pane (Heading / Body / Display / Code /
    scale / fallback status). Pure UI: selecting a preset closes the popup and
    hands the preset id to the owner via the select link; nothing is mutated on
    open or hover.
 */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemPopup : public WeldToolbarPopup
{
public:
    Writer2027TypeSystemPopup(weld::Widget* pParent);
    ~Writer2027TypeSystemPopup() override;

    /** Return the popup to the owner (PopupWindowController::weldPopupWindow). */
    static std::unique_ptr<Writer2027TypeSystemPopup> Create(weld::Widget* pParent);

    /** Called (by the owner) before showing with the current preset id
        (empty = custom) so the list highlights it. */
    void SetCurrentPreset(const OUString& rPresetId);

    /** Connect the apply handler; receives the preset id on selection. */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

    // Exposed for tests (geometry invariant checks).
    const std::vector<TypeSystemPickerRow>& GetModelRows() const { return maModel; }

private:
    DECL_LINK(TreeSelectionHdl, weld::ItemView&, void);
    DECL_LINK(TreeKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeMouseHdl, const MouseEvent&, bool);

    void BuildModel();      // spec 4.1: pure data, no tree/geometry
    void PopulatePresetList(); // spec 4.3: clear + repopulate the tree
    void UpdateDetailPanel();  // spec 4.3: semantic role text under the list
    void ApplySelected();

    virtual void GrabFocus() override;

    std::unique_ptr<weld::TreeView> m_xRows;
    std::unique_ptr<weld::Label> m_xDetailName;
    std::unique_ptr<weld::Label> m_xRoleHeading;
    std::unique_ptr<weld::Label> m_xRoleBody;
    std::unique_ptr<weld::Label> m_xRoleDisplay;
    std::unique_ptr<weld::Label> m_xRoleMono;
    std::unique_ptr<weld::Label> m_xRoleScale;
    std::unique_ptr<weld::Label> m_xFallbackStatus;

    std::vector<TypeSystemPickerRow> maModel;
    OUString maCurrentPreset; // empty = custom
    bool mbInternalMove = false;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEMPOPUP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */