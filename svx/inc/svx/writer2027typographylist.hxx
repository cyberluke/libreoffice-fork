/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027TYPOGRAPHYLIST_HXX
#define INCLUDED_SVX_WRITER2027TYPOGRAPHYLIST_HXX

#include <svx/svxdllapi.h>

#include <svx/writer2027typography.hxx>

#include <vcl/weld/DrawingArea.hxx>
#include <tools/link.hxx>
#include <tools/gen.hxx>

#include <memory>
#include <vector>

class FontList;
class vcl::RenderContext;

namespace svx::writer2027
{

/** Explicit layout rectangle for one typography row (remediation spec 12). */
struct TypographyRowLayout
{
    int mnModelIndex = -1; // index into the model row vector
    tools::Rectangle maRowRect;      // full row
    tools::Rectangle maSpecimenRect; // specimen box (font rows)
    tools::Rectangle maTitleRect;    // family-name line (font rows)
    tools::Rectangle maMetaRect;     // metadata line (font rows)
};

/** Writer 2027 Typography Browser list surface.

    A custom, explicitly laid-out list drawn on a weld::DrawingArea, replacing
    the custom-rendered weld::TreeView (remediation spec 6-13). LibreOffice owns
    the popup shell, focus and anchoring; this class owns row geometry,
    painting, hit-testing, hover/selection state, keyboard navigation and
    scrolling.

    Invariant (spec 11): nothing from row N may draw outside rowRect[N]. Every
    row is painted inside a hard clip and independently testable.
 */
class SVXCORE_DLLPUBLIC Writer2027TypographyList
{
public:
    explicit Writer2027TypographyList(weld::DrawingArea& rArea);
    ~Writer2027TypographyList();

    /** Attach the current model rows (the popup's FontPickerModel::GetRows()).
        Does not copy: the caller must keep the vector alive. */
    void SetRows(const std::vector<FontPickerModel::Row>* pRows);

    void SetFontList(const FontList* pFontList);
    void SetCurrentFamily(const OUString& rFamily);

    /** Rebuild row geometry from the model (spec 8/9) and reset scroll/hover. */
    void RebuildLayout();

    /** The visible viewport (device px). Height drives the scroll clamp. */
    void SetViewportSize(const tools::Long nWidthPx, const tools::Long nHeightPx);

    /** Notify a paint is wanted (search rebuild, state change). */
    void QueueDraw() { m_xArea.queue_draw(); }

    /** Called when the user activates a font row; receives the family name.
        The popup dispatches it through .uno:CharFontName and closes. */
    void connect_select(const Link<const OUString&, void>& rLink) { m_aSelectHdl = rLink; }

    /** Called when the user activates a non-font row (Back / Category /
        Legacy). The popup mutates its model and repopulates this list. */
    void connect_activate(const Link<const FontPickerModel::Row&, bool>& rLink)
    {
        m_aActivateHdl = rLink;
    }

    int GetSelectedIndex() const { return mnSelectedIndex; }
    int GetHoverIndex() const { return mnHoverIndex; }
    const std::vector<TypographyRowLayout>& GetLayout() const { return maLayout; }
    tools::Long GetScrollOffset() const { return mnScrollOffsetPx; }

    void GrabFocus() { m_xArea.grab_focus(); }
    void SelectFirstSelectable();

    // Geometry helpers exposed for tests (spec 32).
    static tools::Long GetRowHeightPx(FontPickerModel::RowKind eKind, double fScale);

private:
    DECL_LINK(DrawHdl, weld::DrawingArea::draw_args, void);
    DECL_LINK(MousePressHdl, const MouseEvent&, bool);
    DECL_LINK(MouseMoveHdl, const MouseEvent&, bool);
    DECL_LINK(KeyHdl, const KeyEvent&, bool);
    DECL_LINK(CommandHdl, const CommandEvent&, bool);

    void Paint(vcl::RenderContext& rCtx, const tools::Rectangle& rRect);
    void PaintRow(vcl::RenderContext& rCtx, const TypographyRowLayout& rLayout,
                  const FontPickerModel::Row& rRow, bool bSelected, bool bHover);
    void PaintFontRow(vcl::RenderContext& rCtx, const TypographyRowLayout& rLayout,
                      const FontPickerModel::Row& rRow, bool bSelected, bool bHover);
    void PaintScrollBar(vcl::RenderContext& rCtx, const tools::Rectangle& rRect);

    int HitTest(const Point& rPoint) const;
    int GetNextSelectableIndex(int nFrom, int nDelta) const;
    bool IsSelectable(const FontPickerModel::Row& rRow) const;
    void SelectIndex(int nIndex, bool bScroll);
    void ActivateRow(int nIndex);
    void ScrollBy(tools::Long nDeltaPx);
    void EnsureSelectedVisible();
    tools::Long MaxContentOffset() const;
    double GetScale() const;

    weld::DrawingArea& m_xArea;
    const std::vector<FontPickerModel::Row>* mpRows = nullptr;
    const FontList* mpFontList = nullptr;
    OUString maCurrentFamily;

    int mnSelectedIndex = -1;
    int mnHoverIndex = -1;

    tools::Long mnScrollOffsetPx = 0;
    tools::Long mnContentHeightPx = 0;
    tools::Long mnViewportHeightPx = 0;
    tools::Long mnWidthPx = 0;

    std::vector<TypographyRowLayout> maLayout;
    Link<const OUString&, void> m_aSelectHdl;
    Link<const FontPickerModel::Row&, bool> m_aActivateHdl;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPOGRAPHYLIST_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */