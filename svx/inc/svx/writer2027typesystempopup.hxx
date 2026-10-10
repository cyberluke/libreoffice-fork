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
#include <svx/writer2027typesystempresetlist.hxx>
#include <svx/writer2027typesystempreview.hxx>
#include <svtools/toolbarmenu.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/Label.hxx>
#include <tools/link.hxx>

#include <memory>
#include <vector>

class FontList;

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

    OUString maHeadingRequested;
    OUString maBodyRequested;
    OUString maMonoRequested;
    OUString maDisplayRequested;

    sal_uInt16 mnHeadingWeight = 0;
    sal_uInt16 mnTitleWeight = 0;
    OUString maScaleLabelText;

    int mnMissingCount = 0; // roles whose preferred family is not installed
    bool mbCurrent = false;
};

/** Writer 2027 Type System picker content, hosted as a WeldToolbarPopup.

    The framework's PopupWindowController owns the anchor (the invoking toolbar
    item) and the popup lifecycle, so the picker opens under the button at any
    DPI. Product layout (spec V3 15/16): left column = CURRENT state + PRESETS
    list on a custom drawing surface (Writer2027TypeSystemPresetList), right
    column = one rich font-rendered preview surface (Writer2027TypeSystemPreview)
    that draws each sample in its resolved role font, and an Apply button.

    Interaction (spec 14): selecting/highlighting a preset only previews the
    detail panel; the Apply button (or Enter) applies the selected system and
    closes. Escape closes without applying. Nothing is mutated on open, hover,
    or click-selection.
 */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemPopup : public WeldToolbarPopup
{
public:
    Writer2027TypeSystemPopup(weld::Widget* pParent);
    ~Writer2027TypeSystemPopup() override;

    /** Return the popup to the owner (PopupWindowController::weldPopupWindow). */
    static std::unique_ptr<Writer2027TypeSystemPopup> Create(weld::Widget* pParent);

    /** Provide the real installed FontList from the owning document so the
        popup resolves presets exactly as Apply does (spec 7/8/30/31). Must be
        called before SetCurrentPreset once per open. */
    void SetFontList(const FontList* pFontList);

    /** Called (by the owner) after SetFontList with the current preset id
        (empty = custom) so the list highlights it and the CURRENT header is
        set. */
    void SetCurrentPreset(const OUString& rPresetId);

    /** Connect the apply handler; receives the preset id when the user
        applies (Apply button / Enter). */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

    // Exposed for tests (geometry invariant checks).
    const std::vector<TypeSystemPickerRow>& GetModelRows() const { return maModel; }

private:
    DECL_LINK(PresetChangedHdl, const OUString&, void);
    DECL_LINK(PresetActivateHdl, const OUString&, void);
    DECL_LINK(ApplyButtonHdl, weld::Button&, void);

    void BuildModel();         // pure data, no geometry
    void PopulatePresetList(); // push resolved model rows into the preset list surface
    void UpdatePreview();      // build the font-rendered preview model for the selected preset
    void ApplySelected();      // apply the selected preset via the select link

    virtual void GrabFocus() override;

    std::unique_ptr<weld::DrawingArea> m_xPresetArea;
    std::unique_ptr<Writer2027TypeSystemPresetList> m_xPresetList;
    std::unique_ptr<weld::DrawingArea> m_xPreviewArea;
    std::unique_ptr<Writer2027TypeSystemPreview> m_xPreview;
    std::unique_ptr<weld::Label> m_xCurrentLabel;
    std::unique_ptr<weld::Button> m_xApplyButton;

    std::vector<TypeSystemPickerRow> maModel;
    OUString maCurrentPreset; // empty = custom
    const FontList* mpFontList = nullptr;
    bool mbInternalMove = false;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEMPOPUP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */