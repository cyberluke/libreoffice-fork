/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027TYPESYSTEMPRESETLIST_HXX
#define INCLUDED_SVX_WRITER2027TYPESYSTEMPRESETLIST_HXX

#include <svx/svxdllapi.h>

#include <rtl/ustring.hxx>

#include <vcl/weld/DrawingArea.hxx>
#include <tools/link.hxx>
#include <tools/gen.hxx>

#include <memory>
#include <vector>

class vcl::RenderContext;

namespace svx::writer2027
{

/** One preset row in the Writer 2027 Type System preset list. */
struct TypeSystemPresetListRow
{
    OUString maPresetId;
    OUString maDisplayName;
    bool mbCurrent = false;   // applied in the current document
};

/** Custom, explicitly-drawn Type System preset list (spec 12/13/14).

    Replaces the previous weld::TreeView. Owns row geometry, hover, selected
    and current-applied state, mouse hit-testing, keyboard navigation, focus
    and painting — the same model the font browser uses. Mouse and keyboard
    operate on the same selectedPresetId; there is no separate state machine
    (spec 14). Suggested preset row height is 48 logical px (spec 13).
 */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemPresetList
{
public:
    explicit Writer2027TypeSystemPresetList(weld::DrawingArea& rArea);
    ~Writer2027TypeSystemPresetList();

    void SetRows(std::vector<TypeSystemPresetListRow> aRows);
    const std::vector<TypeSystemPresetListRow>& GetRows() const { return maRows; }

    /** Set the applied ("current") preset id. Highlights the matching row with
        a "Current" marker (spec 13). */
    void SetCurrentPreset(const OUString& rPresetId);

    void SetViewportSize(const tools::Long nWidthPx, const tools::Long nHeightPx);

    /** Selected preset id (the row the user currently previews). */
    const OUString& GetSelectedPresetId() const { return maSelectedId; }
    /** The applied preset id. */
    const OUString& GetCurrentPresetId() const { return maCurrentId; }

    void QueueDraw() { m_xArea.queue_draw(); }
    void GrabFocus() { m_xArea.grab_focus(); }

    /** Called when the highlighted preset changes (selection). */
    void connect_changed(const Link<const OUString&, void>& rLink) { m_aChangedHdl = rLink; }
    /** Called when the user activates (Enter/click-select behavior controlled
        by the popup: click only previews, Apply applies). */
    void connect_activate(const Link<const OUString&, void>& rLink) { m_aActivateHdl = rLink; }

    // Testable geometry / hit-test.
    tools::Long GetRowHeightPx() const;
    int HitTestAt(const Point& rPoint) const;

private:
    DECL_LINK(DrawHdl, weld::DrawingArea::draw_args, void);
    DECL_LINK(MousePressHdl, const MouseEvent&, bool);
    DECL_LINK(MouseMoveHdl, const MouseEvent&, bool);
    DECL_LINK(KeyHdl, const KeyEvent&, bool);
    DECL_LINK(CommandHdl, const CommandEvent&, bool);

    void Paint(vcl::RenderContext& rCtx, const tools::Rectangle& rRect);
    void PaintRow(vcl::RenderContext& rCtx, const tools::Rectangle& rRowRect,
                  const TypeSystemPresetListRow& rRow, bool bSelected, bool bHover);
    int IndexAt(const Point& rPoint) const;
    void SelectIndex(int nIndex);
    tools::Long RowHeightPx() const;
    double GetScale() const;

    weld::DrawingArea& m_xArea;
    std::vector<TypeSystemPresetListRow> maRows;

    OUString maSelectedId;
    OUString maCurrentId;
    int mnSelectedIndex = -1;
    int mnHoverIndex = -1;

    tools::Long mnWidthPx = 0;
    tools::Long mnHeightPx = 0;

    Link<const OUString&, void> m_aChangedHdl;
    Link<const OUString&, void> m_aActivateHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEMPRESETLIST_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */