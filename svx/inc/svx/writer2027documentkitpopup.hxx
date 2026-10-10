/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027DOCUMENTKITPOPUP_HXX
#define INCLUDED_SVX_WRITER2027DOCUMENTKITPOPUP_HXX

#include <svx/svxdllapi.h>

#include <svtools/toolbarmenu.hxx>

#include <vcl/weld/Builder.hxx>
#include <vcl/weld/TreeView.hxx>

#include <memory>
#include <vector>

namespace com::sun::star::frame { class XFrame; }
namespace weld { class Widget; }

namespace svx::writer2027
{

/** Writer 2027 Document Kit picker popup.

    A WeldToolbarPopup (framework-owned toolbar popover) with a
    custom-rendered card list: one card per curated kit showing the kit name,
    its recommended Type System, a description and the recommended use. Pure
    UI: selecting a kit closes the popup and hands the kit id to the caller,
    which applies it (Type System offer + session kit state) through the
    canonical paths.

    Deriving from WeldToolbarPopup lets svt::PopupWindowController anchor the
    popover under the toolbar button at any DPI (same scheme as the Type System
    picker); the popover's direct child must carry the stable id "container".
 */
class SVXCORE_DLLPUBLIC Writer2027DocumentKitPopup final : public WeldToolbarPopup
{
public:
    Writer2027DocumentKitPopup(const css::uno::Reference<css::frame::XFrame>& xFrame,
                               weld::Widget* pParent);
    ~Writer2027DocumentKitPopup() override;

    virtual void GrabFocus() override;

    /** Called when the popup closes for any reason. */
    void connect_closed(const Link<Writer2027DocumentKitPopup&, void>& rLink)
    {
        m_aCloseHdl = rLink;
    }

    /** Called with the kit id when the user picks a kit. */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

private:
    DECL_LINK(TreeKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeSelectionHdl, weld::ItemView&, void);
    DECL_LINK(TreeMousePressHdl, const MouseEvent&, bool);
    DECL_LINK(RowGetSizeHdl, weld::TreeView::get_size_args, Size);
    DECL_LINK(RowRenderHdl, weld::TreeView::render_args, void);

    /// Paint one row. Separated from the Link wrapper so a paint-time
    /// exception cannot take down the application (see RowRenderHdl).
    void RowRender(vcl::RenderContext& rCtx, const tools::Rectangle& rRect, bool bSelected,
                   const OUString& rId);

    void RebuildRows();
    void ApplyKit(const OUString& rKitId);
    void MoveCursor(int nDelta);
    void SelectRowIndex(int nIndex, bool bScroll);
    int GetNextSelectableIndex(int nFrom, int nDelta) const;

    std::vector<OUString> maRowIds; // "k:<kit id>"

    std::unique_ptr<weld::TreeView> m_xRows;

    int mnLastSelectedIndex = -1;
    bool mbInternalMove = false;
    Link<Writer2027DocumentKitPopup&, void> m_aCloseHdl;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027DOCUMENTKITPOPUP_HXX