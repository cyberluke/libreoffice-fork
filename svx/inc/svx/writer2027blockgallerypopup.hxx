/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027BLOCKGALLERYPOPUP_HXX
#define INCLUDED_SVX_WRITER2027BLOCKGALLERYPOPUP_HXX

#include <svx/svxdllapi.h>

#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Entry.hxx>
#include <vcl/weld/Popover.hxx>
#include <vcl/weld/TreeView.hxx>

#include <memory>
#include <vector>

class vcl::Window;
namespace svx::writer2027
{
struct EditorialBlockDefinition;
}

namespace svx::writer2027
{

/** Writer 2027 Insert Block gallery popup.

    A weld::Popover with a search entry and a custom-rendered card list:
    category header rows (Recommended / Editorial / Technical) followed by
    one card per block. Each card draws a lightweight miniature composition
    preview (pure vector drawing - no raster assets, crisp on HiDPI) plus the
    block name and description.

    Pure UI: hovering never mutates the document. Selecting a block closes
    the popup and hands the block id to the caller, which inserts it through
    the canonical Writer structural APIs (sw::writer2027blocks).
 */
class SVXCORE_DLLPUBLIC Writer2027BlockGalleryPopup
{
public:
    Writer2027BlockGalleryPopup();
    ~Writer2027BlockGalleryPopup();

    /** Open anchored below rAnchorWin. rActiveKitId is the kit id applied
        most recently in this session (may be empty); its recommended blocks
        are shown first inside the Recommended group. */
    void Open(const OUString& rActiveKitId, vcl::Window& rAnchorWin);

    /** Close the popup if open (no selection made). */
    void Close();

    bool IsOpen() const { return mbOpen; }

    /** Called when the popup closes for any reason (selection, Escape,
        outside click). */
    void connect_closed(const Link<Writer2027BlockGalleryPopup&, void>& rLink)
    {
        m_aCloseHdl = rLink;
    }

    /** Called with the block id when the user picks a block. The owner
        inserts it; the popup closes itself right after. */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

private:
    DECL_LINK(SearchChangedHdl, weld::TextWidget&, void);
    DECL_LINK(SearchActivateHdl, weld::Entry&, bool);
    DECL_LINK(SearchKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeSelectionHdl, weld::ItemView&, void);
    DECL_LINK(TreeMousePressHdl, const MouseEvent&, bool);
    DECL_LINK(RowGetSizeHdl, weld::TreeView::get_size_args, Size);
    DECL_LINK(RowRenderHdl, weld::TreeView::render_args, void);
    DECL_LINK(PopupClosedHdl, weld::Popover&, void);

    /// Paint one row. Separated from the Link wrapper so a paint-time
    /// exception cannot take down the application (see RowRenderHdl).
    void RowRender(vcl::RenderContext& rCtx, const tools::Rectangle& rRect, bool bSelected,
                   const OUString& rId);

    void RebuildRows();
    void ApplyBlock(const OUString& rBlockId);
    void MoveCursor(int nDelta);
    void SelectRowIndex(int nIndex, bool bScroll);
    int GetNextSelectableIndex(int nFrom, int nDelta) const;
    bool IsSelectableIndex(int nIndex) const;
    bool IsHeaderRow(int nIndex) const;

    /// Row ids: "h:<category>" (inert header) or "b:<block id>".
    std::vector<OUString> maRowIds;

    std::unique_ptr<weld::Builder> m_xBuilder;
    std::unique_ptr<weld::Popover> m_xPopup;
    std::unique_ptr<weld::Entry> m_xSearch;
    std::unique_ptr<weld::TreeView> m_xRows;

    OUString maActiveKitId;
    OUString maSearchText;
    int mnLastSelectedIndex = -1;
    bool mbInternalMove = false;
    bool mbOpen = false;
    Link<Writer2027BlockGalleryPopup&, void> m_aCloseHdl;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027BLOCKGALLERYPOPUP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */