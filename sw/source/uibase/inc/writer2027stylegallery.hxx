/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027STYLEGALLERY_HXX
#define INCLUDED_SW_WRITER2027STYLEGALLERY_HXX

#include <rtl/ustring.hxx>

#include <swdllapi.h>

#include <tools/gen.hxx>
#include <tools/color.hxx>
#include <tools/link.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>

#include <memory>
#include <vector>

class FontList;
class SwDoc;

namespace sw::writer2027stylegallery
{

/** One card in the Writer 2027 semantic Styles gallery (spec 55). */
struct StyleGalleryItem
{
    int meRole = -1;                // Writer2027SemanticStyle value
    int mnPoolId = -1;              // SwPoolFormatId value (stable identity)
    OUString maInternalName;        // pool programmatic style name for dispatch
    OUString maDisplayName;         // e.g. "Heading 1"
    OUString maEffectiveFamily;     // actual document font family of the style
    tools::Long maEffectiveHeight = 0; // twips
    sal_uInt16 mnWeight = 0;
    bool mbCurrent = false;         // is this the current paragraph style?
};

/** A custom horizontal semantic style gallery (spec V3 32-38, 55, 59).

    Frame-bound (spec 39): never uses document-global Current() shortcuts. The
    item set is built from the actual document style pool (spec 56), so it
    reflects real style edits, not the preset. Cards are drawn live from the
    item data (no global name-keyed bitmap cache, spec 40).
 */
class SW_DLLPUBLIC Writer2027StyleGallery : public vcl::Window
{
public:
    Writer2027StyleGallery(vcl::Window* pParent, WinBits nStyle = WB_TABSTOP);
    virtual ~Writer2027StyleGallery() override;
    virtual void dispose() override;

    void SetItems(std::vector<StyleGalleryItem> aItems);
    const std::vector<StyleGalleryItem>& GetItems() const { return maItems; }
    void SetCurrentStyle(int nCurrentPoolId);
    void SetFontList(const FontList* pFontList);
    void InternalPaint() { Invalidate(); }

    /** Called when a card is activated (click / Enter on a hovered card). */
    void connect_activate(const Link<const OUString&, void>& rLink) { m_aActivateHdl = rLink; }

    // Testable geometry.
    tools::Long GetCardWidthPx() const;
    int IndexAt(const Point& rPoint) const;
    tools::Long GetScrollOffset() const { return mnScrollOffsetPx; }

protected:
    virtual void Paint(vcl::RenderContext& rOut, const tools::Rectangle& rRect) override;
    virtual void MouseButtonDown(const MouseEvent& rEvent) override;
    virtual void MouseMove(const MouseEvent& rEvent) override;
    virtual void KeyInput(const KeyEvent& rEvent) override;
    virtual void Command(const CommandEvent& rEvent) override;
    /// Preferred natural size (used by the notebookbar toolbox layout).
    virtual Size GetOptimalSize() const override;

private:
    void ActivateAt(int nIndex);
    void ScrollBy(tools::Long nDeltaPx);
    tools::Long GetMaxScrollOffset() const;
    tools::Long DesiredHeightPx() const;
    /// Fill the parent (toolbox) horizontal extent while keeping the strip height.
    void FitToParent();
    DECL_LINK(ParentResizeHdl, VclWindowEvent&, void);
    double Scale() const;
    tools::Long CardWidthPx() const;
    tools::Long CardHeightPx() const;
    int IndexAtInternal(const Point& rPoint) const;

    const FontList* mpFontList = nullptr;
    std::vector<StyleGalleryItem> maItems;
    int mnCurrentPoolId = -1;
    int mnHoverIndex = -1;
    int mnFocusIndex = -1; // keyboard focus (distinct from hover/current, spec V4 31)
    tools::Long mnScrollOffsetPx = 0;
    Link<const OUString&, void> m_aActivateHdl;
    bool mbTrackingParent = false;
};

/** Build the gallery item set from the semantic descriptor map + the document
    style pool (spec 55/56/85). Uses the real current document styles. nCurrent
    PoolId is the pool id of the current paragraph style for the highlight. */
SW_DLLPUBLIC std::vector<StyleGalleryItem>
BuildStyleGalleryModel(SwDoc& rDoc, int nCurrentPoolId);

} // namespace sw::writer2027stylegallery

#endif // INCLUDED_SW_WRITER2027STYLEGALLERY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */