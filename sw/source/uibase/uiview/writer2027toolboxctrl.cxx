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
#include <svx/writer2027blocks.hxx>
#include <svx/writer2027documentkitpopup.hxx>
#include <svx/writer2027blockgallerypopup.hxx>
#include <svx/writer2027log.hxx>

#include <comphelper/servicehelper.hxx>

#include <vcl/toolbox.hxx>
#include <vcl/weld/Toolbar.hxx>

#include <unotxdoc.hxx>
#include <writer2027typesystem.hxx>
#include <writer2027typographymanager.hxx>
#include <writer2027blocks.hxx>
#include <editsh.hxx>
#include <docsh.hxx>

#include <editeng/flstitem.hxx>
#include <svl/itemset.hxx>
#include <svl/whichranges.hxx>

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
    // spec 36: this factory is called from the framework's C boundary. Any
    // exception escaping here terminates the process (fail-fast 0xC0000409),
    // so the whole body is a hard exception boundary: log and return null
    // instead of crashing.
    try
    {
    svx::writer2027::Writer2027LogMessage("typesystem.popup.created", u"weld popover"_ustr);
    auto xPopup = svx::writer2027::Writer2027TypeSystemPopup::Create(m_pToolbar);
    xPopup->connect_select(LINK(this, Writer2027TypeSystemToolBoxControl, OnApply));

    // Provide the real installed FontList FIRST (uses the same context, same
    // resolved model as Apply) so the picker previews exactly what Apply will
    // store (spec 8/30/31). Then highlight the detected preset.
    const auto aContext
        = sw::writer2027typographymanager::ResolveWriter2027TypographyContext(m_xFrame);
    if (aContext.pFontList)
        xPopup->SetFontList(aContext.pFontList);

    // Highlight the preset currently detected in THIS frame's document using
    // the REAL installed FontList from the owning doc shell (spec 8/30).
    try
    {
        if (aContext.pDoc && aContext.pDocShell)
        {
            const OUString aPreset
                = sw::writer2027typesystem::DetectCurrentTypeSystem(*aContext.pDoc,
                                                                     aContext.pFontList);
            xPopup->SetCurrentPreset(aPreset);
        }
    }
    catch (const css::uno::Exception&)
    {
        // Non-fatal: the list still shows, just without the highlight.
    }
    return xPopup;
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "Writer2027TypeSystemToolBoxControl::weldPopupWindow", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "Writer2027TypeSystemToolBoxControl::weldPopupWindow", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException(
            "Writer2027TypeSystemToolBoxControl::weldPopupWindow");
    }
    return nullptr;
}

VclPtr<vcl::Window> Writer2027TypeSystemToolBoxControl::createVclPopupWindow(vcl::Window* pParent)
{
    // spec 36: called from the framework's C boundary; a thrown exception here
    // terminates the process (fail-fast). Hard boundary: log and return null.
    try
    {
        svx::writer2027::Writer2027LogMessage("typesystem.controller.createVclPopupWindow",
                                              u"fire"_ustr);
        // spec 25: never keep a stale popup reference across opens.
        if (mxInterimPopover)
            mxInterimPopover.disposeAndClear();

        auto xPopup
            = svx::writer2027::Writer2027TypeSystemPopup::Create(pParent->GetFrameWeld());
        xPopup->connect_select(LINK(this, Writer2027TypeSystemToolBoxControl, OnApply));
        const auto aContext
            = sw::writer2027typographymanager::ResolveWriter2027TypographyContext(m_xFrame);
        if (aContext.pFontList)
            xPopup->SetFontList(aContext.pFontList);
        if (aContext.pDoc && aContext.pDocShell)
        {
            const OUString aPreset
                = sw::writer2027typesystem::DetectCurrentTypeSystem(*aContext.pDoc,
                                                                     aContext.pFontList);
            xPopup->SetCurrentPreset(aPreset);
        }
        mxInterimPopover = VclPtr<InterimToolbarPopup>::Create(
            getFrameInterface(), pParent, std::move(xPopup));
        mxInterimPopover->Show();
        svx::writer2027::Writer2027LogMessage("typesystem.popup.created", u"open"_ustr);
        return mxInterimPopover;
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "Writer2027TypeSystemToolBoxControl::createVclPopupWindow", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "Writer2027TypeSystemToolBoxControl::createVclPopupWindow", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException(
            "Writer2027TypeSystemToolBoxControl::createVclPopupWindow");
    }
    return nullptr;
}

void Writer2027TypeSystemToolBoxControl::ApplyPreset(const OUString& rPresetId)
{
    // Bind to the owning frame's document (never SwModule::GetFirstView) and
    // use the REAL installed FontList from the owning doc shell so preview and
    // apply share one resolved model (spec 7/8/31). A null FontList must never
    // silently mean "pretend every family exists": fail closed instead.
    if (!m_xFrame.is())
    {
        svx::writer2027::Writer2027LogMessage("Writer2027TypeSystemToolBoxControl::ApplyPreset",
                                              u"no frame"_ustr);
        return;
    }

    // Full V4 migration transaction: semantic styles + managed direct override
    // cleanup + caret/insertion cleanup + post-verification + binding refresh,
    // all in one undo group (spec V4 3-13). Never leaves StartUndo unbalanced.
    const auto aResult
        = sw::writer2027typographymanager::ApplyPresetTransaction(m_xFrame, rPresetId);
    svx::writer2027::Writer2027LogMessage(
        aResult.success ? "writer2027typesystem.apply.ok" : "writer2027typesystem.apply.verify_failed",
        OUString::Concat(u"preset=") + rPresetId
            + u" before=" + OUString::number(aResult.before.Total())
            + u" after=" + OUString::number(aResult.after.Total())
            + u" detected=" + aResult.detectedPresetAfter);
}

// Resolve the owning SwView from THIS controller's frame (never a
// document-global shortcut). Null if no writable Writer view is attached.
SwView* lcl_FrameSwView(const css::uno::Reference<css::frame::XFrame>& rFrame)
{
    if (!rFrame.is())
        return nullptr;
    try
    {
        css::uno::Reference<css::frame::XController> xController = rFrame->getController();
        if (!xController.is())
            return nullptr;
        SwXTextDocument* pTextDoc
            = comphelper::getFromUnoTunnel<SwXTextDocument>(xController->getModel());
        if (!pTextDoc || !pTextDoc->GetDocShell())
            return nullptr;
        return pTextDoc->GetDocShell()->GetView();
    }
    catch (const css::uno::Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    return nullptr;
}

// Editorial Block gallery button (.uno:Writer2027InsertBlock). A
// svt::PopupWindowController so the framework anchors the WeldToolbarPopup
// gallery under the button at any DPI (same scheme as the Type System button).
class Writer2027InsertBlockToolBoxControl final : public svt::PopupWindowController
{
public:
    explicit Writer2027InsertBlockToolBoxControl(
        const css::uno::Reference<css::uno::XComponentContext>& rxContext)
        : PopupWindowController(rxContext, css::uno::Reference<css::frame::XFrame>(),
                                u".uno:Writer2027InsertBlock"_ustr)
    {
    }

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override
    {
        return u"lo.writer.Writer2027InsertBlockToolBoxControl"_ustr;
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

    virtual void SAL_CALL initialize(const css::uno::Sequence<css::uno::Any>& rArguments) override;
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

    // Mirrors Writer2027TypeSystemToolBoxControl::initialize(): the notebookbar
    // may host the button on a weld toolbar (m_pToolbar set) OR a classic VCL
    // SidebarToolBox (m_pToolbar null). On a weld toolbar the inherited
    // PopupWindowController::createPopupWindow() routes through
    // mxPopoverContainer, so it must exist before the first click.
    virtual void SAL_CALL execute(sal_Int16 /*nKeyModifier*/) override
    {
        svx::writer2027::Writer2027LogMessage("insertblock.controller.execute", u"fire"_ustr);
        try
        {
            createPopupWindow();
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException("Writer2027InsertBlockToolBoxControl::execute",
                                                    rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException("Writer2027InsertBlockToolBoxControl::execute",
                                                    rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027InsertBlockToolBoxControl::execute");
        }
    }

    virtual std::unique_ptr<WeldToolbarPopup> weldPopupWindow() override
    {
        svx::writer2027::Writer2027LogMessage("insertblock.popup.weld", u"fire"_ustr);
        try
        {
            auto xPopup = std::make_unique<svx::writer2027::Writer2027BlockGalleryPopup>(
                getFrameInterface(), m_pToolbar);
            xPopup->SetActiveKitId(maActiveKitId);
            xPopup->connect_select(LINK(this, Writer2027InsertBlockToolBoxControl, OnSelect));
            return xPopup;
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027InsertBlockToolBoxControl::weldPopupWindow", rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027InsertBlockToolBoxControl::weldPopupWindow", rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027InsertBlockToolBoxControl::weldPopupWindow");
        }
        return nullptr;
    }

    virtual VclPtr<vcl::Window> createVclPopupWindow(vcl::Window* pParent) override
    {
        svx::writer2027::Writer2027LogMessage("insertblock.popup.vcl", u"fire"_ustr);
        try
        {
            if (mxInterimPopover)
                mxInterimPopover.disposeAndClear();
            auto xPopup = std::make_unique<svx::writer2027::Writer2027BlockGalleryPopup>(
                getFrameInterface(), pParent->GetFrameWeld());
            xPopup->SetActiveKitId(maActiveKitId);
            xPopup->connect_select(LINK(this, Writer2027InsertBlockToolBoxControl, OnSelect));
            mxInterimPopover = VclPtr<InterimToolbarPopup>::Create(
                getFrameInterface(), pParent, std::move(xPopup));
            mxInterimPopover->Show();
            return mxInterimPopover;
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027InsertBlockToolBoxControl::createVclPopupWindow", rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027InsertBlockToolBoxControl::createVclPopupWindow", rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027InsertBlockToolBoxControl::createVclPopupWindow");
        }
        return nullptr;
    }

private:
    DECL_LINK(OnSelect, const OUString&, void);

    OUString maActiveKitId;
};

void SAL_CALL Writer2027InsertBlockToolBoxControl::initialize(
    const css::uno::Sequence<css::uno::Any>& rArguments)
{
    PopupWindowController::initialize(rArguments);

    // On a weld toolbar the inherited PopupWindowController::createPopupWindow()
    // routes through mxPopoverContainer, so have one ready before first click.
    // On the classic VCL SidebarToolBox (m_pToolbar null, the notebookbar case)
    // createVclPopupWindow() is used instead and this is a harmless no-op.
    if (m_pToolbar)
    {
        mxPopoverContainer.reset(new ToolbarPopupContainer(m_pToolbar));
        m_pToolbar->set_item_popover(m_aCommandURL, mxPopoverContainer->getTopLevel());
    }
}

IMPL_LINK(Writer2027InsertBlockToolBoxControl, OnSelect, const OUString&, rBlockId, void)
{
    try
    {
        if (SwView* pView = lcl_FrameSwView(m_xFrame))
        {
            if (pView->GetDocShell() && pView->GetDocShell()->IsReadOnly())
            {
                EndPopupMode();
                return;
            }
            const svx::writer2027::EditorialBlockDefinition* pBlock
                = svx::writer2027::Writer2027EditorialBlockCatalog::Get().FindBlock(rBlockId);
            if (pBlock)
                sw::writer2027blocks::InsertEditorialBlock(*pView->GetWrtShellPtr(), *pBlock,
                                                           nullptr);
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027InsertBlockToolBoxControl::OnSelect",
                                                rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027InsertBlockToolBoxControl::OnSelect",
                                                rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027InsertBlockToolBoxControl::OnSelect");
    }
    EndPopupMode();
}

// Document Kit button (.uno:Writer2027DocumentKit). svt::PopupWindowController
// anchors the WeldToolbarPopup kit picker under the button.
class Writer2027DocumentKitToolBoxControl final : public svt::PopupWindowController
{
public:
    explicit Writer2027DocumentKitToolBoxControl(
        const css::uno::Reference<css::uno::XComponentContext>& rxContext)
        : PopupWindowController(rxContext, css::uno::Reference<css::frame::XFrame>(),
                                u".uno:Writer2027DocumentKit"_ustr)
    {
    }

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override
    {
        return u"lo.writer.Writer2027DocumentKitToolBoxControl"_ustr;
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

    virtual void SAL_CALL initialize(const css::uno::Sequence<css::uno::Any>& rArguments) override;
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

    virtual void SAL_CALL execute(sal_Int16 /*nKeyModifier*/) override
    {
        svx::writer2027::Writer2027LogMessage("documentkit.controller.execute", u"fire"_ustr);
        try
        {
            createPopupWindow();
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException("Writer2027DocumentKitToolBoxControl::execute",
                                                    rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException("Writer2027DocumentKitToolBoxControl::execute",
                                                    rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027DocumentKitToolBoxControl::execute");
        }
    }

    virtual std::unique_ptr<WeldToolbarPopup> weldPopupWindow() override
    {
        svx::writer2027::Writer2027LogMessage("documentkit.popup.weld", u"fire"_ustr);
        try
        {
            auto xPopup = std::make_unique<svx::writer2027::Writer2027DocumentKitPopup>(
                getFrameInterface(), m_pToolbar);
            xPopup->connect_select(LINK(this, Writer2027DocumentKitToolBoxControl, OnSelect));
            return xPopup;
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027DocumentKitToolBoxControl::weldPopupWindow", rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027DocumentKitToolBoxControl::weldPopupWindow", rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027DocumentKitToolBoxControl::weldPopupWindow");
        }
        return nullptr;
    }

    virtual VclPtr<vcl::Window> createVclPopupWindow(vcl::Window* pParent) override
    {
        svx::writer2027::Writer2027LogMessage("documentkit.popup.vcl", u"fire"_ustr);
        try
        {
            if (mxInterimPopover)
                mxInterimPopover.disposeAndClear();
            auto xPopup = std::make_unique<svx::writer2027::Writer2027DocumentKitPopup>(
                getFrameInterface(), pParent->GetFrameWeld());
            xPopup->connect_select(LINK(this, Writer2027DocumentKitToolBoxControl, OnSelect));
            mxInterimPopover = VclPtr<InterimToolbarPopup>::Create(
                getFrameInterface(), pParent, std::move(xPopup));
            mxInterimPopover->Show();
            return mxInterimPopover;
        }
        catch (const css::uno::Exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027DocumentKitToolBoxControl::createVclPopupWindow", rEx);
        }
        catch (const std::exception& rEx)
        {
            svx::writer2027::Writer2027LogException(
                "Writer2027DocumentKitToolBoxControl::createVclPopupWindow", rEx);
        }
        catch (...)
        {
            svx::writer2027::Writer2027LogUnknownException(
                "Writer2027DocumentKitToolBoxControl::createVclPopupWindow");
        }
        return nullptr;
    }

private:
    DECL_LINK(OnSelect, const OUString&, void);
};

void SAL_CALL Writer2027DocumentKitToolBoxControl::initialize(
    const css::uno::Sequence<css::uno::Any>& rArguments)
{
    PopupWindowController::initialize(rArguments);

    // On a weld toolbar the inherited PopupWindowController::createPopupWindow()
    // routes through mxPopoverContainer, so have one ready before first click.
    // On the classic VCL SidebarToolBox (m_pToolbar null, the notebookbar case)
    // createVclPopupWindow() is used instead and this is a harmless no-op.
    if (m_pToolbar)
    {
        mxPopoverContainer.reset(new ToolbarPopupContainer(m_pToolbar));
        m_pToolbar->set_item_popover(m_aCommandURL, mxPopoverContainer->getTopLevel());
    }
}

IMPL_LINK(Writer2027DocumentKitToolBoxControl, OnSelect, const OUString&, rKitId, void)
{
    try
    {
        svx::writer2027::Writer2027LogMessage(
            "documentkit.apply", OUString::Concat(u"kit=") + rKitId);
        if (SwView* pView = lcl_FrameSwView(m_xFrame))
        {
            SwDocShell* pDocShell = pView->GetDocShell();
            if (!pDocShell || pDocShell->IsReadOnly())
            {
                EndPopupMode();
                return;
            }
            const svx::writer2027::DocumentKit* pKit
                = svx::writer2027::Writer2027DocumentKitCatalog::Get().FindKit(rKitId);
            if (!pKit)
            {
                EndPopupMode();
                return;
            }
            // Session aid: remember the active kit and offer the kit's
            // recommended Type System (the document stays the source of truth).
            if (!pKit->maTypeSystemId.isEmpty())
            {
                const svx::writer2027::TypeSystemPreset* pPreset
                    = svx::writer2027::Writer2027TypeSystemCatalog::Get().FindPreset(
                        pKit->maTypeSystemId);
                if (pPreset)
                {
                    const SvxFontListItem* pFontListItem
                        = pDocShell->GetItem(SID_ATTR_CHAR_FONTLIST);
                    const FontList* pFontList
                        = pFontListItem ? pFontListItem->GetFontList() : nullptr;
                    const OUString aCurrent
                        = sw::writer2027typesystem::DetectCurrentTypeSystem(*pDocShell->GetDoc(),
                                                                            pFontList);
                    if (aCurrent != pKit->maTypeSystemId
                        && pView->AskApplyRecommendedTypeSystem(*pKit))
                    {
                        const svx::writer2027::ResolvedTypeSystem aResolved
                            = svx::writer2027::ResolveTypeSystem(*pPreset, pFontList);
                        sw::writer2027typesystem::ApplyTypeSystem(*pDocShell->GetDoc(), *pPreset,
                                                                  aResolved, pFontList);
                    }
                }
            }
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027DocumentKitToolBoxControl::OnSelect",
                                                rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027DocumentKitToolBoxControl::OnSelect",
                                                rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027DocumentKitToolBoxControl::OnSelect");
    }
    EndPopupMode();
}

} // namespace

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
lo_writer_Writer2027TypeSystemToolBoxControl_get_implementation(
    css::uno::XComponentContext* rContext, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Writer2027TypeSystemToolBoxControl(rContext));
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
lo_writer_Writer2027InsertBlockToolBoxControl_get_implementation(
    css::uno::XComponentContext* rContext, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Writer2027InsertBlockToolBoxControl(rContext));
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
lo_writer_Writer2027DocumentKitToolBoxControl_get_implementation(
    css::uno::XComponentContext* rContext, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Writer2027DocumentKitToolBoxControl(rContext));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */