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

#include <svtools/popupwindowcontroller.hxx>
#include <svtools/toolboxcontroller.hxx>
#include <svtools/toolbarmenu.hxx>

#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/frame/XController.hpp>

#include <cppuhelper/implbase.hxx>
#include <cppuhelper/supportsservice.hxx>

#include <svx/writer2027typesystempopup.hxx>
#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027log.hxx>

#include <comphelper/servicehelper.hxx>

#include <vcl/toolbox.hxx>
#include <vcl/weld/Toolbar.hxx>

#include <unotxdoc.hxx>
#include <writer2027typesystem.hxx>

#include <exception>

using namespace com::sun::star;

namespace
{
// The Type System button (.uno:Writer2027TypeSystem) has no Sfx slot-pool
// entry (svidl omits these slots), so the generic dispatch path resolves to a
// null XDispatch. To make the button work this controller derives from
// svt::PopupWindowController: the framework owns the anchor and the popup
// lifecycle, and weldPopupWindow() returns the native WeldToolbarPopup list.
// The apply binds to THIS controller's frame (never SwModule::GetFirstView).
class Writer2027TypeSystemToolBoxControl final : public svt::PopupWindowController
{
public:
    explicit Writer2027TypeSystemToolBoxControl(
        const css::uno::Reference<css::uno::XComponentContext>& rxContext)
        : PopupWindowController(rxContext, css::uno::Reference<css::frame::XFrame>(),
                                u".uno:Writer2027TypeSystem"_ustr)
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

    using svt::PopupWindowController::initialize;

    // XStatusListener: no slot-pool status target, so keep the button enabled.
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

    // PopupWindowController
    virtual void SAL_CALL initialize(const css::uno::Sequence<css::uno::Any>& rArguments) override;
    virtual void SAL_CALL execute(sal_Int16 nKeyModifier) override;
    virtual css::uno::Reference<css::awt::XWindow> SAL_CALL createPopupWindow() override;
    virtual VclPtr<vcl::Window> createVclPopupWindow(vcl::Window* pParent) override;
    virtual std::unique_ptr<WeldToolbarPopup> weldPopupWindow() override;

private:
    void ApplyPreset(const OUString& rPresetId);
    DECL_LINK(OnApply, const OUString&, void);
};

void SAL_CALL Writer2027TypeSystemToolBoxControl::initialize(
    const css::uno::Sequence<css::uno::Any>& rArguments)
{
    PopupWindowController::initialize(rArguments);

    // The notebookbar hosts the button on a classic VCL SidebarToolBox (not a
    // weld TransportAsXWindow), so m_pToolbar is null and the popup must use
    // the InterimToolbarPopup path anchored by the framework.
    //
    // ONE canonical open path (spec 23): this is a plain GtkToolButton, so the
    // SidebarToolBox SelectHandler fires execute() — that is the supported
    // no-slot opening path. We deliberately do NOT add ToolBoxItemBits::DROPDOWN
    // here: doing so created a second (dropdown) entry point into the same
    // popup lifecycle and is the likely cause of "second click does nothing"
    // (the framework's dropdown machinery and our execute() hook contended for
    // popup state). If we are ever hosted on a weld toolbar, wire the native
    // popover instead (still via execute()).
    if (m_pToolbar)
    {
        mxPopoverContainer.reset(new ToolbarPopupContainer(m_pToolbar));
        m_pToolbar->set_item_popover(m_aCommandURL, mxPopoverContainer->getTopLevel());
    }
}

void SAL_CALL Writer2027TypeSystemToolBoxControl::execute(sal_Int16 /*nKeyModifier*/)
{
    // .uno:Writer2027TypeSystem has no Sfx slot-pool entry, so the inherited
    // ToolboxController::execute() would resolve a null XDispatch and do
    // nothing (the button appears dead). Here the controller IS the supported
    // toolbar-opening path: pressing the button opens the popup directly.
    svx::writer2027::Writer2027LogMessage("typesystem.controller.execute", u"fire"_ustr);
    try
    {
        createPopupWindow();
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemToolBoxControl::execute", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemToolBoxControl::execute", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemToolBoxControl::execute");
    }
}

css::uno::Reference<css::awt::XWindow> SAL_CALL
Writer2027TypeSystemToolBoxControl::createPopupWindow()
{
    // spec 23/38: log the lifecycle. Exactly one createPopupWindow per open is
    // expected; a second open that never reaches here would show up in the log.
    svx::writer2027::Writer2027LogMessage("typesystem.controller.createPopupWindow",
                                          u"fire"_ustr);
    return PopupWindowController::createPopupWindow();
}

IMPL_LINK(Writer2027TypeSystemToolBoxControl, OnApply, const OUString&, rPresetId, void)
{
    svx::writer2027::Writer2027LogMessage(
        "typesystem.apply",
        OUString::Concat(u"preset=") + rPresetId);
    try
    {
        ApplyPreset(rPresetId);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemToolBoxControl::OnApply", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemToolBoxControl::OnApply", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemToolBoxControl::OnApply");
    }
    EndPopupMode();
    svx::writer2027::Writer2027LogMessage("typesystem.popup.closed",
                                          u"after apply"_ustr);
}

std::unique_ptr<WeldToolbarPopup> Writer2027TypeSystemToolBoxControl::weldPopupWindow()
{
    svx::writer2027::Writer2027LogMessage("typesystem.popup.created", u"weld popover"_ustr);
    auto xPopup = svx::writer2027::Writer2027TypeSystemPopup::Create(m_pToolbar);
    xPopup->connect_select(LINK(this, Writer2027TypeSystemToolBoxControl, OnApply));

    // Highlight the preset currently detected in THIS frame's document.
    try
    {
        if (m_xFrame.is())
        {
            css::uno::Reference<css::frame::XController> xController = m_xFrame->getController();
            if (xController.is())
            {
                SwXTextDocument* pTextDoc
                    = comphelper::getFromUnoTunnel<SwXTextDocument>(
                        xController->getModel());
                if (pTextDoc)
                {
                    const FontList* pFontList = nullptr;
                    SwDoc* pDoc = pTextDoc->GetDocShell() ? pTextDoc->GetDocShell()->GetDoc()
                                                          : nullptr;
                    if (pDoc)
                    {
                        const OUString aPreset
                            = sw::writer2027typesystem::DetectCurrentTypeSystem(*pDoc,
                                                                                 pFontList);
                        xPopup->SetCurrentPreset(aPreset);
                    }
                }
            }
        }
    }
    catch (const css::uno::Exception&)
    {
        // Non-fatal: the list still shows, just without the highlight.
    }
    return xPopup;
}

VclPtr<vcl::Window> Writer2027TypeSystemToolBoxControl::createVclPopupWindow(vcl::Window* pParent)
{
    svx::writer2027::Writer2027LogMessage("typesystem.controller.createVclPopupWindow",
                                          u"fire"_ustr);
    // spec 25: never keep a stale popup reference across opens. A previous
    // open/close cycle may have left mxInterimPopover pointing at a disposed
    // window; releasing it here guarantees the second (and every later) click
    // creates a fresh popup instead of touching dead state.
    if (mxInterimPopover)
        mxInterimPopover.disposeAndClear();

    auto xPopup = svx::writer2027::Writer2027TypeSystemPopup::Create(pParent->GetFrameWeld());
    xPopup->connect_select(LINK(this, Writer2027TypeSystemToolBoxControl, OnApply));
    mxInterimPopover = VclPtr<InterimToolbarPopup>::Create(
        getFrameInterface(), pParent, std::move(xPopup));
    mxInterimPopover->Show();
    svx::writer2027::Writer2027LogMessage("typesystem.popup.created", u"open"_ustr);
    return mxInterimPopover;
}

void Writer2027TypeSystemToolBoxControl::ApplyPreset(const OUString& rPresetId)
{
    // Bind to the owning frame's document (never SwModule::GetFirstView).
    if (!m_xFrame.is())
    {
        svx::writer2027::Writer2027LogMessage("Writer2027TypeSystemToolBoxControl::ApplyPreset",
                                              u"no frame"_ustr);
        return;
    }

    css::uno::Reference<css::frame::XController> xController = m_xFrame->getController();
    if (!xController.is())
        return;
    SwXTextDocument* pTextDoc = comphelper::getFromUnoTunnel<SwXTextDocument>(
        xController->getModel());
    if (!pTextDoc || !pTextDoc->GetDocShell())
    {
        svx::writer2027::Writer2027LogMessage("Writer2027TypeSystemToolBoxControl::ApplyPreset",
                                              u"no writer doc on frame"_ustr);
        return;
    }

    const svx::writer2027::Writer2027TypeSystemCatalog& rCatalog
        = svx::writer2027::Writer2027TypeSystemCatalog::Get();
    const svx::writer2027::TypeSystemPreset* pPreset = rCatalog.FindPreset(rPresetId);
    if (!pPreset)
        return;
    const svx::writer2027::ResolvedTypeSystem aResolved
        = svx::writer2027::ResolveTypeSystem(*pPreset, nullptr);
    SwDoc* pDoc = pTextDoc->GetDocShell()->GetDoc();
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pPreset, aResolved, nullptr);
    svx::writer2027::Writer2027LogMessage(
        "Writer2027TypeSystemToolBoxControl::ApplyPreset",
        OUString::Concat(u"applied preset id=") + rPresetId);
}

} // namespace

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
lo_writer_Writer2027TypeSystemToolBoxControl_get_implementation(
    css::uno::XComponentContext* rContext, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Writer2027TypeSystemToolBoxControl(rContext));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */