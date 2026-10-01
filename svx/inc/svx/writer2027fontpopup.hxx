/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027FONTPOPUP_HXX
#define INCLUDED_SVX_WRITER2027FONTPOPUP_HXX

#include <svx/svxdllapi.h>

#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Popover.hxx>
#include <vcl/weld/TreeView.hxx>

#include <memory>

class FontList;
class vcl::Window;
namespace weld { class Entry; }
namespace svx::writer2027 { class FontPickerModel; }

namespace svx::writer2027
{

/** Writer 2027 premium font picker popup shell.

    Replaces the native ComboBox dropdown for the font-name control in Writer
    documents. The popup owns its own search field and a custom-rendered
    grouped row list driven by the shared FontPickerModel. Font selection is
    handed back to the caller, which keeps dispatching through the canonical
    `.uno:CharFontName` path.

    The popup is a weld::Popover (a VCL DockingWindow in popup mode), the same
    primitive the WeldToolbarPopup framework uses for toolbar dropdowns, so
    it works on Windows, GTK and Qt, anchors to the font control, flips above
    when there is no room below, and clamps to the active monitor's work area.
 */
class SVX_DLLPUBLIC Writer2027FontPopup
{
public:
    Writer2027FontPopup(FontPickerModel& rModel);
    ~Writer2027FontPopup();

    /** Open the popup anchored below rAnchorWin; empty rInitialQuery opens
        the root grouped view, anything else starts in search mode. */
    void Open(const FontList* pFontList, const OUString& rCurrentFamily,
              const OUString& rInitialQuery, vcl::Window& rAnchorWin);

    /** Close the popup if open (no selection made). */
    void Close();

    bool IsOpen() const { return mbOpen; }

    /** Refresh the model's "Current" pin when the document font changed. */
    void SetCurrentFamily(const FontList* pFontList, const OUString& rCurrentFamily);

    /** Move keyboard focus back into the search field. Used after the
        native combo's own focus handling ran (e.g. its arrow button grabs
        the entry focus while the dropdown is being suppressed). */
    void GrabSearchFocus()
    {
        if (m_xSearch)
            m_xSearch->grab_focus();
    }

    /** Called when the popup closes for any reason (selection, Escape,
        outside click). The owner restores any transient state here. */
    void connect_closed(const Link<Writer2027FontPopup&, void>& rLink) { m_aCloseHdl = rLink; }

    /** Called with the installed family name when the user picks a font.
        The owner dispatches it through the canonical `.uno:CharFontName`
        path; the popup closes itself right after. */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

private:
    DECL_LINK(SearchChangedHdl, weld::TextWidget&, void);
    DECL_LINK(SearchKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeKeyHdl, const KeyEvent&, bool);
    DECL_LINK(TreeSelectionHdl, weld::ItemView&, void);
    DECL_LINK(TreeMousePressHdl, const MouseEvent&, bool);
    DECL_LINK(RowGetSizeHdl, weld::TreeView::get_size_args, Size);
    DECL_LINK(RowRenderHdl, weld::TreeView::render_args, void);
    DECL_LINK(PopupClosedHdl, weld::Popover&, void);

    void RebuildRows();
    void ActivateRow(const FontPickerModel::Row& rRow);
    void ApplyFamily(const OUString& rFamily);
    void MoveCursor(int nDelta);
    void MoveCursorPage(int nDelta);
    void MoveCursorHomeOrEnd(bool bHome);
    int GetNextSelectableIndex(int nFrom, int nDelta) const;
    bool IsSelectableRow(const FontPickerModel::Row& rRow) const;
    void SelectRowIndex(int nIndex, bool bScroll);
    void HandleSearchEscape();
    void HandleTreeEscape();

    FontPickerModel& mrModel;
    std::unique_ptr<weld::Builder> m_xBuilder;
    std::unique_ptr<weld::Popover> m_xPopup;
    std::unique_ptr<weld::Entry> m_xSearch;
    std::unique_ptr<weld::TreeView> m_xRows;

    const FontList* mpFontList = nullptr;
    OUString maCurrentFamily;
    OUString maQuery;
    int mnLastSelectedIndex = -1;
    bool mbInternalMove = false; // selection moves done by the popup itself
    bool mbOpen = false;         // popup currently shown
    Link<Writer2027FontPopup&, void> m_aCloseHdl;
    Link<const OUString&, void> m_aSelectHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027FONTPOPUP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */