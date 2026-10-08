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

#include <svtools/toolboxcontroller.hxx>

#include <com/sun/star/frame/FeatureStateEvent.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <cppuhelper/implbase.hxx>
#include <cppuhelper/supportsservice.hxx>

#include <svx/writer2027log.hxx>

#include <swmodule.hxx>
#include <view.hxx>

#include <vcl/toolbox.hxx>
#include <vcl/weld/Toolbar.hxx>

#include <exception>

using namespace com::sun::star;

namespace
{
// The Writer 2027 "Type System…" notebookbar button cannot use the normal
// .uno:Writer2027TypeSystem dispatch path: svidl deliberately omits these
// slots from the generated Sfx slot pool, so queryDispatch() returns null and
// the generic toolbar controller silently does nothing (and, before the null
// guard was added in GenericToolbarController::execute(), crashed with a null
// deref). This controller bypasses the slot pool entirely and opens the popup
// directly from the active SwView, exactly like the font dropdown's custom
// controller (SvxFrameToolBoxControl).
class Writer2027TypeSystemToolBoxControl_Base
    : public cppu::ImplInheritanceHelper<svt::ToolboxController, css::lang::XServiceInfo>
{
public:
    explicit Writer2027TypeSystemToolBoxControl_Base(
        const css::uno::Reference<css::uno::XComponentContext>& rxContext)
        : ImplInheritanceHelper(rxContext, css::uno::Reference<css::frame::XFrame>(),
                                u".uno:Writer2027TypeSystem"_ustr)
    {
    }
};

class Writer2027TypeSystemToolBoxControl final : public Writer2027TypeSystemToolBoxControl_Base
{
public:
    explicit Writer2027TypeSystemToolBoxControl(
        const css::uno::Reference<css::uno::XComponentContext>& rxContext)
        : Writer2027TypeSystemToolBoxControl_Base(rxContext)
    {
    }

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override
    {
        return u"lo.writer.Writer2027TypeSystemToolBoxControl"_ustr;
    }

    virtual sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override
    {
        return cppu::supportsService(this, rServiceName);
    }

    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override
    {
        return { u"com.sun.star.frame.ToolbarController"_ustr };
    }

    // XStatusListener: the command has no slot-pool status target, so always
    // keep the button enabled instead of letting a "not found" state gray it out.
    virtual void SAL_CALL statusChanged(const css::frame::FeatureStateEvent& /*rEvent*/) override
    {
        if (m_pToolbar)
            m_pToolbar->set_item_sensitive(m_aCommandURL, true);
        else
        {
            ToolBox* pToolBox = nullptr;
            ToolBoxItemId nId;
            if (getToolboxId(nId, &pToolBox))
                pToolBox->EnableItem(nId, true);
        }
    }

    // XToolbarController::execute - the notebookbar button was clicked.
    virtual void SAL_CALL execute(sal_Int16 /*KeyModifier*/) override
    {
        try
        {
            svx::writer2027::Writer2027LogMessage(
                "Writer2027TypeSystemToolBoxControl::execute", u"button clicked"_ustr);
            SwView* pView = SwModule::GetFirstView();
            if (pView)
            {
                svx::writer2027::Writer2027LogMessage(
                    "Writer2027TypeSystemToolBoxControl::execute",
                    OUString::Concat(u"pView=")
                        + OUString::number(reinterpret_cast<sal_IntPtr>(pView)));

                // Anchor the popup under the invoking toolbar button, not the
                // whole document window (which rendered the popup as a sidebar).
                tools::Rectangle aAnchorRect;
                ToolBox* pToolBox = nullptr;
                ToolBoxItemId nId;
                if (getToolboxId(nId, &pToolBox))
                    aAnchorRect = pToolBox->GetItemRect(nId);
                if (aAnchorRect.IsEmpty())
                    aAnchorRect = tools::Rectangle(
                        Point(0, 0), pView->GetViewFrame().GetWindow().GetSizePixel());

                pView->OpenWriter2027TypeSystemPopup(aAnchorRect);
                svx::writer2027::Writer2027LogMessage(
                    "Writer2027TypeSystemToolBoxControl::execute",
                    OUString::Concat(u"OpenWriter2027TypeSystemPopup returned, anchor=")
                        + OUString::number(aAnchorRect.GetWidth()) + u"x"
                        + OUString::number(aAnchorRect.GetHeight()));
            }
            else
                svx::writer2027::Writer2027LogMessage("Writer2027TypeSystemToolBoxControl::execute",
                                                      u"no active SwView"_ustr);
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException("Writer2027TypeSystemToolBoxControl::execute",
                                                    rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException("Writer2027TypeSystemToolBoxControl::execute",
                                                    rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027TypeSystemToolBoxControl::execute");
        }
    }
};

} // namespace

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
lo_writer_Writer2027TypeSystemToolBoxControl_get_implementation(
    css::uno::XComponentContext* rContext, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Writer2027TypeSystemToolBoxControl(rContext));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */