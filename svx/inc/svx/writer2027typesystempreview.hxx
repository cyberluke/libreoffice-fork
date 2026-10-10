/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027TYPESYSTEMPREVIEW_HXX
#define INCLUDED_SVX_WRITER2027TYPESYSTEMPREVIEW_HXX

#include <svx/svxdllapi.h>

#include <rtl/ustring.hxx>

#include <vcl/weld/DrawingArea.hxx>
#include <tools/gen.hxx>

#include <memory>
#include <vector>

class FontList;
class vcl::RenderContext;

namespace svx::writer2027
{

/** Type System preview model (spec 10/52).

    Carries the resolved installed families (the SAME ones Apply will store),
    the scale, weights and theme-driven colors, plus fixed UI sample text.
    The preview draws each sample in its actual role font, so it is a truthful
    representation of what will be applied (spec 17). Never contains document
    text (spec 53).
 */
struct TypeSystemPreviewModel
{
    OUString maPresetId;
    OUString maDisplayName;

    OUString maHeadingFamily;
    OUString maBodyFamily;
    OUString maMonoFamily;
    OUString maDisplayFamily;

    sal_uInt16 mnHeadingWeight = 0;
    sal_uInt16 mnTitleWeight = 0;

    OUString maScaleLabelText;   // e.g. "Editorial · 12 / 15 / 18 / 24 / 28"

    // Structured fallback rows (spec V4 33): one row per role, shown as
    // "Role\nRequested -> Resolved", never one long comma-separated line.
    struct FallbackRow
    {
        OUString maRole;     // Heading / Body / Code / Display
        OUString maDetail;   // "Neue Montreal -> Inter" or "SAP 72 installed as 72"
    };
    std::vector<FallbackRow> maFallbackRows;

    // Fixed UI sample strings (spec 53).
    OUString maHeadingSample = u"A Better Way to Write"_ustr;
    OUString maBodySample = u"A clear, calm paragraph for long-form reading."_ustr;
    OUString maMonoSample = u"const mode = \"editorial\";"_ustr;
    bool mbPreviewing = false; // when the selected-for-preview differs from current
};

/** Custom font-rendered Type System preview surface (spec 10/11/52).

    Replaces the previous plain weld::Label previews. Each section (Heading /
    Body / Code, optional display title, Scale, Fallbacks) is separately
    clipped and drawn with its resolved role font so the preview visually
    matches Apply. Preview sizes communicate hierarchy (clamped), not exact
    document points (spec 11). No preview text may overlap another section.
 */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemPreview
{
public:
    explicit Writer2027TypeSystemPreview(weld::DrawingArea& rArea);
    ~Writer2027TypeSystemPreview();

    void SetModel(TypeSystemPreviewModel aModel);
    void SetFontList(const FontList* pFontList);
    const TypeSystemPreviewModel& GetModel() const { return maModel; }

    void SetViewportSize(const tools::Long nWidthPx, const tools::Long nHeightPx);
    void QueueDraw() { m_xArea.queue_draw(); }

private:
    DECL_LINK(DrawHdl, weld::DrawingArea::draw_args, void);

    void Paint(vcl::RenderContext& rCtx, const tools::Rectangle& rRect);
    void PaintSection(vcl::RenderContext& rCtx, const tools::Rectangle& rSection,
                      const OUString& rLabel, const OUString& rText, const OUString& rFamily,
                      sal_uInt16 nWeight, double fSizeLp);
    void PaintBody(vcl::RenderContext& rCtx, const tools::Rectangle& rSection,
                   const OUString& rLabel, const OUString& rText, const OUString& rFamily);

    tools::Long ClampLogical(tools::Long nLp) const;
    double Scale() const;

    weld::DrawingArea& m_xArea;
    const FontList* mpFontList = nullptr;
    TypeSystemPreviewModel maModel;
    tools::Long mnWidthPx = 0;
    tools::Long mnHeightPx = 0;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEMPREVIEW_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */