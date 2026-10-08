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

#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Popover.hxx>
#include <vcl/weld/TreeView.hxx>

#include <memory>
#include <vector>

class FontList;
class vcl::Window;
namespace svx::writer2027
{
struct ResolvedTypeSystem;
}

namespace svx::writer2027
{

/** Writer 2027 Type System picker popup.

    A weld::Popover with a custom-rendered card list: one card per curated
    preset showing the preset name, a heading/body pairing sample in the
    resolved installed families, the "Family + Family" pairing line and, when
    fonts are missing, an explicit "N fonts missing / Using: ..." block.

    Pure UI: hovering never mutates the document. Selecting a preset closes
    the popup and hands the preset id to the caller, which applies it through
    the canonical Writer style APIs.
 */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemPopup
{
public:
    Writer2027TypeSystemPopup();
    ~Writer2027TypeSystemPopup();

    /** Open anchored below the given anchor rectangle (the invoking toolbar
        button, in rAnchorWin's coordinate space). rCurrentPresetId is the
        preset currently detected in the document (empty = Custom). */
    void Open(const FontList* pFontList, const OUString& rCurrentPresetId,
              vcl::Window& rAnchorWin, tools::Rectangle rAnchorRect);

    /** Close the popup if open (no selection made). */
    void Close();

    bool IsOpen() const { return mbOpen; }

    /** Called when the popup closes for any reason (selection, Escape,
        outside click). */
    void connect_closed(const Link<Writer2027TypeSystemPopup&, void>& rLink)
    {
        m_aCloseHdl = rLink;
    }

    /** Called with the preset id when the user picks a preset. The owner
        applies it through the canonical style path; the popup closes itself
        right after. */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

private:
    DECL_LINK(TreeKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeSelectionHdl, weld::ItemView&, void);
    DECL_LINK(TreeMousePressHdl, const MouseEvent&, bool);
    DECL_LINK(RowGetSizeHdl, weld::TreeView::get_size_args, Size);
    DECL_LINK(RowRenderHdl, weld::TreeView::render_args, void);
    DECL_LINK(PopupClosedHdl, weld::Popover&, void);

    void RebuildRows();
    void ApplyPreset(const OUString& rPresetId);
    void MoveCursor(int nDelta);
    void SelectRowIndex(int nIndex, bool bScroll);
    int GetNextSelectableIndex(int nFrom, int nDelta) const;
    bool IsSelectableIndex(int nIndex) const;
    void RowRender(vcl::RenderContext& rCtx, const tools::Rectangle& rRect, bool bSelected,
                   const OUString& rId);

    std::unique_ptr<weld::Builder> m_xBuilder;
    std::unique_ptr<weld::Popover> m_xPopup;
    std::unique_ptr<weld::TreeView> m_xRows;

    const FontList* mpFontList = nullptr;
    OUString maCurrentPresetId;
    std::vector<OUString> maRowIds; // "custom" (inert) or "p:<preset id>"
    std::vector<ResolvedTypeSystem> maResolved; // parallel to the catalog
    int mnLastSelectedIndex = -1;
    bool mbInternalMove = false;
    bool mbOpen = false;
    // Content width (device px) the popup is sized to; the custom row-measure
    // callback returns this as the row width so the text is never clipped to a
    // narrow default cell (which previously truncated the sample lines).
    tools::Long mnPopupContentWidth = 0;
    Link<Writer2027TypeSystemPopup&, void> m_aCloseHdl;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEMPOPUP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */