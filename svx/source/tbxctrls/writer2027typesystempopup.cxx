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

#include <vcl/weld/TreeView.hxx>

#include <algorithm>

namespace svx::writer2027
{

namespace
{
OUString lcl_RowTagForPreset(const OUString& rPresetId)
{
    return u"p:"_ustr + rPresetId;
}

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
    // "12 / 14 / 16 / 20 / 26 / 36" (body, caption, mono, H3, H2, H1-ish sizes;
    // fixed UI sample per remediation spec 43).
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
    , m_xRows(m_xBuilder->weld_tree_view(u"preset_list"_ustr))
    , m_xCurrentLabel(m_xBuilder->weld_label(u"typesystem_current"_ustr))
    , m_xDetailName(m_xBuilder->weld_label(u"detail_name"_ustr))
    , m_xRoleHeading(m_xBuilder->weld_label(u"role_heading"_ustr))
    , m_xPreviewHeading(m_xBuilder->weld_label(u"preview_heading"_ustr))
    , m_xRoleBody(m_xBuilder->weld_label(u"role_body"_ustr))
    , m_xPreviewBody(m_xBuilder->weld_label(u"preview_body"_ustr))
    , m_xRoleMono(m_xBuilder->weld_label(u"role_mono"_ustr))
    , m_xPreviewMono(m_xBuilder->weld_label(u"preview_mono"_ustr))
    , m_xRoleScale(m_xBuilder->weld_label(u"role_scale"_ustr))
    , m_xFallbackStatus(m_xBuilder->weld_label(u"fallback_status"_ustr))
    , m_xApplyButton(m_xBuilder->weld_button(u"apply_button"_ustr))
{
    m_xRows->set_selection_mode(SelectionMode::Single);
    m_xRows->connect_selection_changed(LINK(this, Writer2027TypeSystemPopup, TreeSelectionHdl));
    m_xRows->connect_key_press(LINK(this, Writer2027TypeSystemPopup, TreeKeyHdl));
    m_xApplyButton->connect_clicked(LINK(this, Writer2027TypeSystemPopup, ApplyButtonHdl));

    BuildModel();
    PopulatePresetList();
    UpdateDetailPanel();
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
    // NOT a fake preset row (remediation spec 18).
    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    maModel.clear();

    for (const auto& rPreset : rCatalog.GetPresets())
    {
        TypeSystemPickerRow aRow;
        aRow.maPresetId = rPreset.maId;
        aRow.maDisplayName = SvxResId(rPreset.maNameResId);

        const ResolvedTypeSystem aResolved = ResolveTypeSystem(rPreset, nullptr);
        aRow.maHeadingFamily = aResolved.Get(TypeSystemFontRole::Heading).maFamily;
        aRow.maBodyFamily = aResolved.Get(TypeSystemFontRole::Body).maFamily;
        aRow.maMonoFamily = aResolved.Get(TypeSystemFontRole::Mono).maFamily;
        aRow.maDisplayFamily = aResolved.Get(TypeSystemFontRole::Display).maFamily;
        aRow.mnMissingCount = aResolved.GetMissingCount();
        maModel.push_back(aRow);
    }
}

void Writer2027TypeSystemPopup::SetCurrentPreset(const OUString& rPresetId)
{
    maCurrentPreset = rPresetId;

    const int nCount = static_cast<int>(maModel.size());
    for (int i = 0; i < nCount; ++i)
    {
        TypeSystemPickerRow& rRow = maModel[i];
        rRow.mbCurrent = !rPresetId.isEmpty() && (rRow.maPresetId == rPresetId);
    }

    // CURRENT header: "Custom typography" when the document does not match a
    // preset, otherwise the preset display name (spec 18).
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

    // Highlight the matching preset row (no-op selection already there).
    mbInternalMove = true;
    m_xRows->freeze();
    for (int i = 0; i < nCount; ++i)
    {
        if (maModel[i].mbCurrent)
        {
            m_xRows->select(i);
            if (auto xIter = m_xRows->get_iterator(i))
                m_xRows->scroll_to_row(*xIter);
            break;
        }
    }
    m_xRows->thaw();
    mbInternalMove = false;

    UpdateDetailPanel();
}

void Writer2027TypeSystemPopup::PopulatePresetList()
{
    // Populate only after the model exists; presets only (no Custom row).
    mbInternalMove = true;
    m_xRows->freeze();
    m_xRows->clear();
    for (const auto& rRow : maModel)
    {
        std::unique_ptr<weld::TreeIter> xIter(m_xRows->make_iterator());
        m_xRows->append(xIter.get());
        m_xRows->set_text(*xIter, rRow.maDisplayName, 0);
        m_xRows->set_id(*xIter, lcl_RowTagForPreset(rRow.maPresetId));
    }
    // Initial selection: the current preset if matched, otherwise the first.
    const int nCount = static_cast<int>(maModel.size());
    int nSelect = 0;
    for (int i = 0; i < nCount; ++i)
    {
        if (maModel[i].mbCurrent)
        {
            nSelect = i;
            break;
        }
    }
    m_xRows->select(nSelect);
    m_xRows->thaw();
    mbInternalMove = false;
}

void Writer2027TypeSystemPopup::UpdateDetailPanel()
{
    int nSel = m_xRows ? m_xRows->get_selected_index() : -1;
    if (nSel < 0 || nSel >= static_cast<int>(maModel.size()))
    {
        for (int i = 0; i < static_cast<int>(maModel.size()); ++i)
        {
            if (maModel[i].mbCurrent)
            {
                nSel = i;
                break;
            }
        }
    }
    if (nSel < 0 || nSel >= static_cast<int>(maModel.size()))
        nSel = 0;

    const TypeSystemPickerRow& rRow = maModel[nSel];
    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    const TypeSystemPreset* pPreset = rCatalog.FindPreset(rRow.maPresetId);

    // Fixed UI sample text (spec 43) — never document content.
    m_xDetailName->set_label(rRow.maDisplayName);
    m_xRoleHeading->set_label(u"Heading · "_ustr
                              + (rRow.maHeadingFamily.isEmpty() ? u"—"_ustr
                                                                : rRow.maHeadingFamily));
    m_xPreviewHeading->set_label(u"A Better Way to Write"_ustr);
    m_xRoleBody->set_label(u"Body · "_ustr
                           + (rRow.maBodyFamily.isEmpty() ? u"—"_ustr : rRow.maBodyFamily));
    m_xPreviewBody->set_label(
        u"A clear, calm paragraph for long-form reading."_ustr);
    m_xRoleMono->set_label(u"Code · "_ustr
                           + (rRow.maMonoFamily.isEmpty() ? u"—"_ustr : rRow.maMonoFamily));
    m_xPreviewMono->set_label(u"const mode = \""_ustr + rRow.maPresetId + u"\";"_ustr);

    if (pPreset)
    {
        const TypeSystemScale& rScale = rCatalog.GetScale(pPreset->meScale);
        OUString aScaleText = lcl_ScaleLabel(pPreset->meScale);
        const OUString aSummary = lcl_ScaleSummary(rScale);
        if (!aSummary.isEmpty())
            aScaleText += u" · " + aSummary;
        m_xRoleScale->set_label(u"Scale · "_ustr + aScaleText);

        if (rRow.mnMissingCount > 0)
        {
            OUString aMsg = SvxResId(STR_WRITER2027_TYPESYSTEM_FONTS_MISSING)
                                .replaceFirst(u"%1"_ustr, OUString::number(rRow.mnMissingCount));
            m_xFallbackStatus->set_label(aMsg);
        }
        else
            m_xFallbackStatus->set_label(u"Fallbacks: all preferred fonts installed"_ustr);
    }
    else
    {
        m_xRoleScale->set_label(u"Scale · —"_ustr);
        m_xFallbackStatus->set_label(u"Fallbacks: —"_ustr);
    }
}

void Writer2027TypeSystemPopup::GrabFocus()
{
    if (m_xRows)
        m_xRows->grab_focus();
}

IMPL_LINK(Writer2027TypeSystemPopup, TreeSelectionHdl, weld::ItemView&, /*rView*/, void)
{
    // spec 19: changing highlight/keyboard selection only previews the detail
    // panel; it never mutates the document and never applies.
    if (!mbInternalMove)
        UpdateDetailPanel();
}

IMPL_LINK(Writer2027TypeSystemPopup, TreeKeyHdl, const KeyEvent&, rKEvt, bool)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    if (rKeyCode.GetCode() == KEY_RETURN && !rKeyCode.IsShift() && !rKeyCode.IsMod1()
        && !rKeyCode.IsMod2() && !rKeyCode.IsMod3())
    {
        // spec 19: Enter applies the highlighted preset.
        ApplySelected();
        return true;
    }
    if (rKeyCode.GetCode() == KEY_ESCAPE)
        return true; // the popup framework closes on Escape (spec 5.4)
    return false;
}

IMPL_LINK(Writer2027TypeSystemPopup, ApplyButtonHdl, weld::Button&, /*rButton*/, void)
{
    // spec 19: the Apply button applies the selected preset and closes.
    ApplySelected();
}

void Writer2027TypeSystemPopup::ApplySelected()
{
    const int nRow = m_xRows ? m_xRows->get_selected_index() : -1;
    if (nRow < 0 || nRow >= static_cast<int>(maModel.size()))
        return;
    const TypeSystemPickerRow& rRow = maModel[nRow];
    if (rRow.maPresetId.isEmpty())
        return;

    // Report + clear the highlight synchronously so a re-open sees a clean list.
    mbInternalMove = true;
    m_xRows->unselect_all();
    mbInternalMove = false;

    if (m_aSelectHdl.IsSet())
        m_aSelectHdl.Call(rRow.maPresetId);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */