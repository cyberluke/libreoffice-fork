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
const OUString ROW_CUSTOM = u"custom"_ustr;

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
} // namespace

Writer2027TypeSystemPopup::Writer2027TypeSystemPopup(weld::Widget* pParent)
    : WeldToolbarPopup(nullptr, pParent, u"svx/ui/writer2027typesystempopup.ui"_ustr,
                       u"Writer2027TypeSystemPopup"_ustr)
    , m_xRows(m_xBuilder->weld_tree_view(u"preset_list"_ustr))
    , m_xDetailName(m_xBuilder->weld_label(u"detail_name"_ustr))
    , m_xRoleHeading(m_xBuilder->weld_label(u"role_heading"_ustr))
    , m_xRoleBody(m_xBuilder->weld_label(u"role_body"_ustr))
    , m_xRoleDisplay(m_xBuilder->weld_label(u"role_display"_ustr))
    , m_xRoleMono(m_xBuilder->weld_label(u"role_mono"_ustr))
    , m_xRoleScale(m_xBuilder->weld_label(u"role_scale"_ustr))
    , m_xFallbackStatus(m_xBuilder->weld_label(u"fallback_status"_ustr))
{
    m_xRows->set_selection_mode(SelectionMode::Single);
    m_xRows->connect_selection_changed(LINK(this, Writer2027TypeSystemPopup, TreeSelectionHdl));
    m_xRows->connect_key_press(LINK(this, Writer2027TypeSystemPopup, TreeKeyHdl));
    m_xRows->connect_mouse_press(LINK(this, Writer2027TypeSystemPopup, TreeMouseHdl));

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
    // spec 4.1: pure data. One row per preset with the resolved semantic
    // roles stored explicitly; no tree operations, no rendering, no geometry.
    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    maModel.clear();

    // "Custom document typography" is state, not a preset: model it explicitly
    // but inert, so it never dispatches an apply (spec 4.1).
    {
        TypeSystemPickerRow aCustom;
        aCustom.maPresetId.clear();
        aCustom.maDisplayName = SvxResId(STR_WRITER2027_TYPESYSTEM_CUSTOM);
        maModel.push_back(aCustom);
    }

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

        // Current-preset comparison uses the still-current detection result;
        // SetCurrentPreset() marks the matching row after the owner passes the
        // detected id.
        maModel.push_back(aRow);
    }
}

void Writer2027TypeSystemPopup::SetCurrentPreset(const OUString& rPresetId)
{
    if (rPresetId.isEmpty())
        return;
    maCurrentPreset = rPresetId;

    const int nCount = static_cast<int>(maModel.size());
    for (int i = 0; i < nCount; ++i)
    {
        TypeSystemPickerRow& rRow = maModel[i];
        rRow.mbCurrent = !rPresetId.isEmpty() && (rRow.maPresetId == rPresetId);
    }

    // Correct the highlight only; the list itself is already populated.
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
    // spec 4.3: populate only after the model exists.
    mbInternalMove = true;
    m_xRows->freeze();
    m_xRows->clear();
    for (const auto& rRow : maModel)
    {
        std::unique_ptr<weld::TreeIter> xIter(m_xRows->make_iterator());
        m_xRows->append(xIter.get());
        m_xRows->set_text(*xIter, rRow.maDisplayName, 0);
        m_xRows->set_id(*xIter, rRow.maPresetId.isEmpty() ? ROW_CUSTOM
                                                          : lcl_RowTagForPreset(rRow.maPresetId));
    }
    // Initial selection: the current preset if matched, otherwise Custom.
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
    // Find the currently selected model row (fall back to the highlighted
    // current row, then the first preset).
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
    m_xDetailName->set_label(
        u"Selected: "_ustr + (rRow.maPresetId.isEmpty() ? SvxResId(STR_WRITER2027_TYPESYSTEM_CUSTOM)
                                                        : rRow.maDisplayName));
    m_xRoleHeading->set_label(u"Heading: "_ustr
                              + (rRow.maHeadingFamily.isEmpty() ? u"—"_ustr
                                                                : rRow.maHeadingFamily));
    m_xRoleBody->set_label(u"Body: "_ustr
                           + (rRow.maBodyFamily.isEmpty() ? u"—"_ustr : rRow.maBodyFamily));
    m_xRoleDisplay->set_label(u"Display: "_ustr
                              + (rRow.maDisplayFamily.isEmpty() ? u"—"_ustr
                                                                : rRow.maDisplayFamily));
    m_xRoleMono->set_label(u"Code: "_ustr
                           + (rRow.maMonoFamily.isEmpty() ? u"—"_ustr : rRow.maMonoFamily));

    if (rRow.maPresetId.isEmpty())
    {
        m_xRoleScale->set_label(u"Scale / rhythm: —"_ustr);
        m_xFallbackStatus->set_label(u"Fallbacks: —"_ustr);
        return;
    }

    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    const TypeSystemPreset* pPreset = rCatalog.FindPreset(rRow.maPresetId);
    if (pPreset)
    {
        const TypeSystemScale& rScale = rCatalog.GetScale(pPreset->meScale);
        OUString aScaleText = lcl_ScaleLabel(pPreset->meScale);
        if (rScale.mnBody > 0)
            aScaleText += u" · body " + OUString::number(rScale.mnBody / 20) + "pt";
        if (rScale.mnLineSpacingPercent > 0)
            aScaleText += u" · " + OUString::number(rScale.mnLineSpacingPercent) + "%";
        m_xRoleScale->set_label(u"Scale / rhythm: "_ustr + aScaleText);

        if (rRow.mnMissingCount > 0)
        {
            OUString aMsg
                = SvxResId(STR_WRITER2027_TYPESYSTEM_FONTS_MISSING).replaceFirst(
                    u"%1"_ustr, OUString::number(rRow.mnMissingCount));
            m_xFallbackStatus->set_label(aMsg);
        }
        else
            m_xFallbackStatus->set_label(u"Fallbacks: all preferred fonts installed"_ustr);
    }
    else
    {
        m_xRoleScale->set_label(u"Scale / rhythm: —"_ustr);
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
    // spec 5.2: changing highlight/keyboard selection only previews the detail
    // pane; it never mutates the document.
    if (!mbInternalMove)
        UpdateDetailPanel();
}

IMPL_LINK(Writer2027TypeSystemPopup, TreeKeyHdl, const KeyEvent&, rKEvt, bool)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    if (rKeyCode.GetCode() == KEY_RETURN && !rKeyCode.IsShift() && !rKeyCode.IsMod1()
        && !rKeyCode.IsMod2() && !rKeyCode.IsMod3())
    {
        // spec 5.3: Enter applies the highlighted preset.
        ApplySelected();
        return true;
    }
    if (rKeyCode.GetCode() == KEY_ESCAPE)
        return true; // the popup framework closes on Escape (spec 5.4)
    return false;
}

IMPL_LINK(Writer2027TypeSystemPopup, TreeMouseHdl, const MouseEvent&, rEvent, bool)
{
    // spec 5.3: a single (left) click on a preset applies it and closes.
    if (!rEvent.IsLeft())
        return false;

    std::unique_ptr<weld::TreeIter> xIter
        = m_xRows->get_dest_row_at_pos(rEvent.GetPosPixel(), false, false);
    if (!xIter)
        return false;
    const int nIndex = m_xRows->get_iter_index_in_parent(*xIter);
    if (nIndex < 0 || nIndex >= static_cast<int>(maModel.size()))
        return false;
    if (maModel[nIndex].maPresetId.isEmpty())
        return true; // "Custom" label is inert
    ApplySelected();
    return true;
}

void Writer2027TypeSystemPopup::ApplySelected()
{
    const int nRow = m_xRows ? m_xRows->get_selected_index() : -1;
    if (nRow < 0 || nRow >= static_cast<int>(maModel.size()))
        return;
    const TypeSystemPickerRow& rRow = maModel[nRow];
    if (rRow.maPresetId.isEmpty())
        return; // inert "Custom" label never applies

    // Report + clear the highlight synchronously so a re-open sees a clean list.
    mbInternalMove = true;
    m_xRows->unselect_all();
    mbInternalMove = false;

    if (m_aSelectHdl.IsSet())
        m_aSelectHdl.Call(rRow.maPresetId);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */