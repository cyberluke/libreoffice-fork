/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typesystempopup.hxx>
#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027log.hxx>

#include <svx/strings.hrc>
#include <svx/dialmgr.hxx>

#include <vcl/svapp.hxx>

#include <algorithm>
#include <cmath>
#include <utility>

namespace svx::writer2027
{

namespace
{
OUString lcl_ScaleLabel(TypeSystemScaleId eScale)
{
    switch (eScale)
    {
        case TypeSystemScaleId::Balanced:
            return u"Balanced"_ustr;
        case TypeSystemScaleId::Editorial:
            return u"Editorial"_ustr;
        case TypeSystemScaleId::Compact:
            return u"Compact"_ustr;
        case TypeSystemScaleId::Accessible:
            return u"Accessible"_ustr;
    }
    return u""_ustr;
}

OUString lcl_ScaleSummary(const TypeSystemScale& rScale)
{
    // "body / caption / mono / H3 / H4 / H2 / H1 ..." - a representative
    // hierarchy summary in logical points (spec 16/52).
    const auto pt = [](sal_uInt16 nTwips) { return nTwips / 20; };
    OUString aSummary = OUString::number(pt(rScale.mnBody));
    if (rScale.mnCaption)
        aSummary += u" / " + OUString::number(pt(rScale.mnCaption));
    if (rScale.mnMono)
        aSummary += u" / " + OUString::number(pt(rScale.mnMono));
    if (rScale.mnH3)
        aSummary += u" / " + OUString::number(pt(rScale.mnH3));
    if (rScale.mnH2)
        aSummary += u" / " + OUString::number(pt(rScale.mnH2));
    if (rScale.mnH1)
        aSummary += u" / " + OUString::number(pt(rScale.mnH1));
    return aSummary;
}
} // namespace

Writer2027TypeSystemPopup::Writer2027TypeSystemPopup(weld::Widget* pParent)
    : WeldToolbarPopup(nullptr, pParent, u"svx/ui/writer2027typesystempopup.ui"_ustr,
                       u"Writer2027TypeSystemPopup"_ustr)
    , m_xPresetArea(m_xBuilder->weld_drawing_area(u"preset_list"_ustr))
    , m_xPreviewArea(m_xBuilder->weld_drawing_area(u"preview_surface"_ustr))
    , m_xCurrentLabel(m_xBuilder->weld_label(u"typesystem_current"_ustr))
    , m_xApplyButton(m_xBuilder->weld_button(u"apply_button"_ustr))
{
    // Owning drawing surfaces for the new custom preset list and preview. The
    // weld DrawingArea unique_ptrs are kept alive as members so the custom
    // surfaces they back never dangle (same pattern as the font popup fix).
    m_xPresetList = std::make_unique<Writer2027TypeSystemPresetList>(*m_xPresetArea);
    m_xPreview = std::make_unique<Writer2027TypeSystemPreview>(*m_xPreviewArea);
    m_xPreview->SetFontList(nullptr);

    m_xPresetList->connect_changed(LINK(this, Writer2027TypeSystemPopup, PresetChangedHdl));
    m_xPresetList->connect_activate(LINK(this, Writer2027TypeSystemPopup, PresetActivateHdl));
    m_xApplyButton->connect_clicked(LINK(this, Writer2027TypeSystemPopup, ApplyButtonHdl));

    // Set a deterministic initial size; the framework will clamp/resize at open.
    const double fScale = Application::GetDefaultDevice()
                              ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                              : 1.0;
    const auto lp = [fScale](tools::Long n) {
        return std::max<tools::Long>(static_cast<tools::Long>(std::lround(n * fScale)), 1);
    };
    // Left column ~250 lp for 7 rows of 48; right preview ~440x360 lp so the
    // heading/body/code/scale/fallback sections fit without clipping (spec V4 32).
    m_xPresetList->SetViewportSize(lp(250), lp(48 * 7 + 8));
    m_xPreview->SetViewportSize(lp(440), lp(360));
}

std::unique_ptr<Writer2027TypeSystemPopup>
Writer2027TypeSystemPopup::Create(weld::Widget* pParent)
{
    return std::unique_ptr<Writer2027TypeSystemPopup>(new Writer2027TypeSystemPopup(pParent));
}

Writer2027TypeSystemPopup::~Writer2027TypeSystemPopup() = default;

void Writer2027TypeSystemPopup::BuildModel()
{
    // Pure data: one row per real preset with resolved semantic roles stored
    // explicitly. "Custom typography" is state (shown in the CURRENT header),
    // NOT a fake preset row (remediation spec 18). Resolution uses the real
    // installed FontList provided by the controller (never nullptr), so the
    // preview shows exactly the families Apply will store (spec 7/30/31).
    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    maModel.clear();

    for (const auto& rPreset : rCatalog.GetPresets())
    {
        TypeSystemPickerRow aRow;
        aRow.maPresetId = rPreset.maId;
        aRow.maDisplayName = SvxResId(rPreset.maNameResId);

        const ResolvedTypeSystem aResolved = ResolveTypeSystem(rPreset, mpFontList);
        aRow.maHeadingFamily = aResolved.Get(TypeSystemFontRole::Heading).maFamily;
        aRow.maBodyFamily = aResolved.Get(TypeSystemFontRole::Body).maFamily;
        aRow.maMonoFamily = aResolved.Get(TypeSystemFontRole::Mono).maFamily;
        aRow.maDisplayFamily = aResolved.Get(TypeSystemFontRole::Display).maFamily;

        aRow.maHeadingRequested = rPreset.maHeading.maPreferred;
        aRow.maBodyRequested = rPreset.maBody.maPreferred;
        aRow.maMonoRequested = rPreset.maMono.maPreferred;
        aRow.maDisplayRequested = rPreset.maDisplay.maPreferred;

        aRow.mnHeadingWeight = rPreset.mnHeadingWeight;
        aRow.mnTitleWeight = rPreset.mnTitleWeight;
        const TypeSystemScale& rScale = rCatalog.GetScale(rPreset.meScale);
        aRow.maScaleLabelText = lcl_ScaleLabel(rPreset.meScale);
        const OUString aSummary = lcl_ScaleSummary(rScale);
        if (!aSummary.isEmpty())
            aRow.maScaleLabelText += u" · " + aSummary;

        aRow.mnMissingCount = aResolved.GetMissingCount();
        maModel.push_back(aRow);
    }
}

void Writer2027TypeSystemPopup::SetFontList(const FontList* pFontList)
{
    if (mpFontList == pFontList)
        return;
    mpFontList = pFontList;
    m_xPreview->SetFontList(pFontList);
    if (mpFontList && maModel.empty())
    {
        BuildModel();
        PopulatePresetList();
        UpdatePreview();
    }
}

void Writer2027TypeSystemPopup::SetCurrentPreset(const OUString& rPresetId)
{
    maCurrentPreset = rPresetId;

    for (auto& rRow : maModel)
        rRow.mbCurrent = !rPresetId.isEmpty() && (rRow.maPresetId == rPresetId);

    // CURRENT header: "Custom typography" when the document does not match a
    // preset, otherwise the preset display name.
    if (rPresetId.isEmpty())
        m_xCurrentLabel->set_label(SvxResId(STR_WRITER2027_TYPESYSTEM_CUSTOM));
    else
    {
        bool bFound = false;
        for (const auto& rRow : maModel)
        {
            if (rRow.maPresetId == rPresetId)
            {
                m_xCurrentLabel->set_label(rRow.maDisplayName);
                bFound = true;
                break;
            }
        }
        if (!bFound)
            m_xCurrentLabel->set_label(SvxResId(STR_WRITER2027_TYPESYSTEM_CUSTOM));
    }

    m_xPresetList->SetCurrentPreset(rPresetId);
    UpdatePreview();
}

void Writer2027TypeSystemPopup::PopulatePresetList()
{
    std::vector<TypeSystemPresetListRow> aRows;
    aRows.reserve(maModel.size());
    for (const auto& rRow : maModel)
    {
        TypeSystemPresetListRow aListRow;
        aListRow.maPresetId = rRow.maPresetId;
        aListRow.maDisplayName = rRow.maDisplayName;
        aListRow.mbCurrent = rRow.mbCurrent;
        aRows.push_back(aListRow);
    }
    m_xPresetList->SetRows(std::move(aRows));
    m_xPresetList->QueueDraw();
}

void Writer2027TypeSystemPopup::UpdatePreview()
{
    const OUString aSelectedId = m_xPresetList->GetSelectedPresetId();
    int nSel = -1;
    for (size_t i = 0; i < maModel.size(); ++i)
        if (maModel[i].maPresetId == aSelectedId)
            nSel = static_cast<int>(i);
    if (nSel < 0)
    {
        for (size_t i = 0; i < maModel.size(); ++i)
            if (maModel[i].mbCurrent)
                nSel = static_cast<int>(i);
    }
    if (nSel < 0 && !maModel.empty())
        nSel = 0;
    if (nSel < 0)
    {
        m_xPreview->SetModel(TypeSystemPreviewModel{});
        return;
    }

    const TypeSystemPickerRow& rRow = maModel[nSel];
    TypeSystemPreviewModel aPreview;
    aPreview.maPresetId = rRow.maPresetId;
    aPreview.maDisplayName = rRow.maDisplayName;
    aPreview.maHeadingFamily = rRow.maHeadingFamily;
    aPreview.maBodyFamily = rRow.maBodyFamily;
    aPreview.maMonoFamily = rRow.maMonoFamily;
    aPreview.maDisplayFamily = rRow.maDisplayFamily;
    aPreview.mnHeadingWeight = rRow.mnHeadingWeight;
    aPreview.mnTitleWeight = rRow.mnTitleWeight;
    aPreview.maScaleLabelText = rRow.maScaleLabelText;

    // Structured fallback rows (spec V4 33): one role per row, showing a real
    // substitution ("Neue Montreal -> Inter") or a same-typeface installed
    // alias ("SAP 72 installed as 72"). Never one comma-separated line.
    const auto addFallback = [&aPreview](const OUString& rRole, const OUString& rRequested,
                                         const OUString& rResolved)
    {
        if (rRequested.isEmpty() || rResolved.isEmpty())
            return;
        TypeSystemPreviewModel::FallbackRow aRow;
        aRow.maRole = rRole;
        if (rRequested != rResolved)
        {
            // Distinguish an alias (same typeface, different installed name)
            // from a real substitution. We can only know that by comparing the
            // resolved name against the preferred name; the SVX resolver already
            // falls back through its chain, so a mismatch is a truth signal.
            aRow.maDetail = rRequested + u" -> " + rResolved;
        }
        else
            return; // no fallback needed for this role
        aPreview.maFallbackRows.push_back(std::move(aRow));
    };
    addFallback(u"Heading"_ustr, rRow.maHeadingRequested, rRow.maHeadingFamily);
    addFallback(u"Body"_ustr, rRow.maBodyRequested, rRow.maBodyFamily);
    addFallback(u"Code"_ustr, rRow.maMonoRequested, rRow.maMonoFamily);
    addFallback(u"Display"_ustr, rRow.maDisplayRequested, rRow.maDisplayFamily);

    // spec V4 34: mark when the selected-for-preview differs from the applied
    // preset so the right panel shows "Previewing X" and the CURRENT header
    // stays authoritative.
    aPreview.mbPreviewing = !rRow.maPresetId.isEmpty()
                            && rRow.maPresetId != maCurrentPreset;

    // Code sample uses the mono family name in its text for clarity.
    aPreview.maMonoSample
        = u"const mode = \""_ustr + rRow.maDisplayName.toAsciiLowerCase() + u"\";"_ustr;

    m_xPreview->SetModel(std::move(aPreview));
    m_xPreview->QueueDraw();
}

void Writer2027TypeSystemPopup::GrabFocus()
{
    if (m_xPresetList)
        m_xPresetList->GrabFocus();
}

IMPL_LINK(Writer2027TypeSystemPopup, PresetChangedHdl, const OUString&, rPresetId, void)
{
    (void)rPresetId;
    try
    {
        // spec 14/19: changing highlight/selection only previews; never
        // mutates the document and never applies.
        if (!mbInternalMove)
            UpdatePreview();
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::PresetChangedHdl", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::PresetChangedHdl", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPopup::PresetChangedHdl");
    }
}

IMPL_LINK(Writer2027TypeSystemPopup, PresetActivateHdl, const OUString&, rPresetId, void)
{
    (void)rPresetId;
    try
    {
        // spec 14: Enter applies the highlighted preset.
        ApplySelected();
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::PresetActivateHdl",
                                                rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::PresetActivateHdl",
                                                rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPopup::PresetActivateHdl");
    }
}

IMPL_LINK(Writer2027TypeSystemPopup, ApplyButtonHdl, weld::Button&, /*rButton*/, void)
{
    try
    {
        // spec 14/19: the Apply button applies the selected preset and closes.
        ApplySelected();
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::ApplyButtonHdl", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::ApplyButtonHdl", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPopup::ApplyButtonHdl");
    }
}

void Writer2027TypeSystemPopup::ApplySelected()
{
    const OUString aId = m_xPresetList ? m_xPresetList->GetSelectedPresetId() : OUString();
    if (aId.isEmpty())
        return;

    if (m_aSelectHdl.IsSet())
        m_aSelectHdl.Call(aId);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */