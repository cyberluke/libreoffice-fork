/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
* This file is part of the LibreOffice project.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* This file incorporates work covered by the following license notice:
*
*   Licensed to the Apache Software Foundation (ASF) under one or more
*   contributor license agreements. See the NOTICE file distributed
*   with this work for additional information regarding copyright
*   ownership. The ASF licenses this file to you under the Apache
*   License, Version 2.0 (the "License"); you may not use this file
*   except in compliance with the License. You may obtain a copy of
*   the License at http://www.apache.org/licenses/LICENSE-2.0 .
*/

#include <vcl/toolkit/button.hxx>
#include <vcl/layout.hxx>
#include <bitmaps.hlst>
#include <NotebookbarPopup.hxx>
#include <PriorityHBox.hxx>
#include <PriorityMergedHBox.hxx>
#include <comphelper/lok.hxx>

#include <algorithm>

#define DUMMY_WIDTH 50
#define BUTTON_WIDTH 30

/*
* PriorityMergedHBox is a VclHBox which hides its own children if there is no sufficient space.
*/

PriorityMergedHBox::PriorityMergedHBox(vcl::Window* pParent)
    : PriorityHBox(pParent)
{
    m_pButton = VclPtr<PushButton>::Create(this, WB_FLATBUTTON);
    m_pButton->SetClickHdl(LINK(this, PriorityMergedHBox, PBClickHdl));
    m_pButton->SetModeImage(Image(StockImage::Yes, CHEVRON));
    m_pButton->set_width_request(25);
    m_pButton->set_pack_type(VclPackType::End);
    m_pButton->Show();
}

void PriorityMergedHBox::Resize()
{
    if (comphelper::LibreOfficeKit::isActive())
        return VclHBox::Resize();

    if (!m_bInitialized)
        Initialize();

    if (!m_bInitialized)
    {
        return VclHBox::Resize();
    }

    tools::Long nWidth = GetSizePixel().Width();
    tools::Long nCurrentWidth = VclHBox::calculateRequisition().getWidth() + BUTTON_WIDTH;

    // Build the collapse order. Children with an explicit priority (any value other than
    // VCL_PRIORITY_DEFAULT) are hidden in ascending priority order, i.e. the lowest
    // priority control collapses first. Children without an explicit priority keep the
    // legacy behavior of collapsing right-to-left (the last child collapses first), which
    // corresponds to the implicit priority (child-count - 1 - index).
    struct ChildEntry
    {
        vcl::Window* pWindow;
        int nPriority;
    };
    std::vector<ChildEntry> aChildren;
    aChildren.reserve(GetChildCount());
    for (int i = 0; i < GetChildCount(); ++i)
    {
        vcl::Window* pWindow = GetChild(i);
        int nPriority = GetChildCount() - 1 - i; // legacy right-to-left collapse order
        if (vcl::IPrioritable* pPrioritable = dynamic_cast<vcl::IPrioritable*>(pWindow);
            pPrioritable && pPrioritable->GetPriority() != VCL_PRIORITY_DEFAULT)
            nPriority = pPrioritable->GetPriority();
        aChildren.push_back({ pWindow, nPriority });
    }
    std::stable_sort(aChildren.begin(), aChildren.end(),
                     [](const ChildEntry& rL, const ChildEntry& rR)
                     { return rL.nPriority < rR.nPriority; });

    // Hide lower priority controls
    for (const ChildEntry& rEntry : aChildren)
    {
        vcl::Window* pWindow = rEntry.pWindow;

        if (nCurrentWidth <= nWidth)
            break;

        if (pWindow && pWindow->GetParent() == this && pWindow->IsVisible())
        {
            tools::Long nWindowWidth = pWindow->GetOutputSizePixel().Width();
            if (!nWindowWidth)
                nWindowWidth = getLayoutRequisition(*pWindow).Width() + get_spacing();

            if (nWindowWidth)
                nCurrentWidth -= nWindowWidth;
            else
                nCurrentWidth -= DUMMY_WIDTH;
            pWindow->Hide();
        }
    }

    // Show higher priority controls if we already have enough space
    for (auto it = aChildren.rbegin(); it != aChildren.rend(); ++it)
    {
        vcl::Window* pWindow = it->pWindow;

        if (pWindow->GetParent() != this)
        {
            continue;
        }

        if (pWindow && !pWindow->IsVisible())
        {
            pWindow->Show();
            nCurrentWidth += getLayoutRequisition(*pWindow).Width() + get_spacing();

            if (nCurrentWidth > nWidth)
            {
                pWindow->Hide();
                break;
            }
        }
    }

    VclHBox::Resize();

    if (HasHiddenChildren())
        m_pButton->Show();
    else
        m_pButton->Hide();
}

void PriorityMergedHBox::dispose()
{
    m_pButton.disposeAndClear();
    if (m_pPopup)
        m_pPopup.disposeAndClear();
    PriorityHBox::dispose();
}

bool PriorityMergedHBox::HasHiddenChildren() const
{
    for (int i = GetChildCount() - 1; i >= 0; i--)
    {
        vcl::Window* pWindow = GetChild(i);
        if (pWindow && pWindow->GetParent() == this && !pWindow->IsVisible())
            return true;
    }

    return false;
}

Size PriorityMergedHBox::calculateRequisition() const
{
    if (!m_bInitialized)
    {
        return VclHBox::calculateRequisition();
    }

    sal_uInt16 nVisibleChildren = 0;

    Size aSize;
    // find max height and total width
    for (vcl::Window* pChild = GetWindow(GetWindowType::FirstChild); pChild;
         pChild = pChild->GetWindow(GetWindowType::Next))
    {
        Size aChildSize = getLayoutRequisition(*pChild);
        if (!pChild->IsVisible())
            setPrimaryDimension(aChildSize, 0);
        else
        {
            ++nVisibleChildren;
            tools::Long nPrimaryDimension = getPrimaryDimension(aChildSize);
            nPrimaryDimension += pChild->get_padding() * 2;
            setPrimaryDimension(aChildSize, nPrimaryDimension);
        }
        accumulateMaxes(aChildSize, aSize);
    }

    return finalizeMaxes(aSize, nVisibleChildren);
}

IMPL_LINK(PriorityMergedHBox, PBClickHdl, Button*, /*pButton*/, void)
{
    if (m_pPopup)
        m_pPopup.disposeAndClear();

    m_pPopup = VclPtr<NotebookbarPopup>::Create(this);

    for (int i = 0; i < GetChildCount(); i++)
    {
        vcl::Window* pWindow = GetChild(i);
        if (pWindow != m_pButton)
        {
            if (!pWindow->IsVisible())
            {
                pWindow->Show();
                pWindow->SetParent(m_pPopup->getBox());
                // count is decreased because we moved child
                i--;
            }
        }
    }

    m_pPopup->hideSeparators(true);

    tools::Long x = m_pButton->GetPosPixel().getX();
    tools::Long y = m_pButton->GetPosPixel().getY() + GetSizePixel().Height();
    tools::Rectangle aRect(x, y, x, y);

    m_pPopup->StartPopupMode(aRect, FloatWinPopupFlags::Down | FloatWinPopupFlags::GrabFocus
                                        | FloatWinPopupFlags::AllMouseButtonClose);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
