/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027stylegalleryctrl.hxx>
#include <writer2027stylegallery.hxx>
#include <writer2027typographymanager.hxx>

#include <svx/writer2027log.hxx>

#include <svtools/popupwindowcontroller.hxx>
#include <svtools/toolboxcontroller.hxx>
#include <svtools/toolbarmenu.hxx>

#include <comphelper/servicehelper.hxx>
#include <unotxdoc.hxx>

#include <vcl/toolbox.hxx>
#include <vcl/svapp.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/implbase.hxx>

#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/DispatchResultEvent.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <com/sun/star/util/URL.hpp>

#include <sfx2/dispatch.hxx>
#include <sfx2/viewsh.hxx>
#include <sfx2/app.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/sfxsids.hrc>
#include <svl/itemset.hxx>

#include <swmodule.hxx>
#include <wrtsh.hxx>
#include <docsh.hxx>
#include <edtwin.hxx>
#include <view.hxx>
#include <SwStyleNameMapper.hxx>

#include <memory>

using namespace com::sun::star;

using sw::writer2027stylegallery::BuildStyleGalleryModel;
using sw::writer2027stylegallery::Writer2027StyleGallery;

namespace
{
// Internal dispatch helper for .uno:StyleApply. Cards always apply a paragraph
// style, so Family=Para and Template carries the programmatic style name.
void lcl_PostStyleApply(const css::uno::Reference<css::frame::XFrame>& rFrame,
                        const OUString& rStyleName)
{
    if (!rFrame.is())
        return;
    css::uno::Reference<css::frame::XDispatchProvider> xProv(rFrame, css::uno::UNO_QUERY);
    if (!xProv.is())
        return;

    css::util::URL aURL;
    aURL.Complete = u".uno:StyleApply"_ustr;
    css::uno::Reference<css::frame::XDispatch> xDispatch
        = xProv->queryDispatch(aURL, OUString(), 0);
    if (!xDispatch.is())
        return;
    css::uno::Sequence<css::beans::PropertyValue> aArgs(2);
    css::beans::PropertyValue& rTempl = aArgs.getArray()[0];
    rTempl.Name = u"Template"_ustr;
    rTempl.Value <<= rStyleName;
    css::beans::PropertyValue& rFam = aArgs.getArray()[1];
    rFam.Name = u"Family"_ustr;
    rFam.Value <<= sal_Int32(0); // SfxStyleFamily::Para
    xDispatch->dispatch(aURL, aArgs);
}

int lcl_CurrentParaStylePoolId(const css::uno::Reference<css::frame::XFrame>& rFrame)
{
    // Frame-bound lookup of the current paragraph style pool id.
    if (!rFrame.is())
        return -1;
    try
    {
        css::uno::Reference<css::frame::XController> xController = rFrame->getController();
        if (!xController.is())
            return -1;
        SwXTextDocument* pTextDoc
            = comphelper::getFromUnoTunnel<SwXTextDocument>(xController->getModel());
        if (!pTextDoc || !pTextDoc->GetDocShell())
            return -1;
        SwView* pView = pTextDoc->GetDocShell()->GetView();
        if (!pView)
            return -1;
        SwWrtShell* pWrt = pView->GetWrtShellPtr();
        if (!pWrt)
            return -1;
        const SwTextFormatColl* pColl = pWrt->GetCurTextFormatColl();
        return pColl ? static_cast<int>(pColl->GetPoolFormatId()) : -1;
    }
    catch (const css::uno::Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    return -1;
}
} // namespace

Writer2027StyleGalleryToolBoxControl::Writer2027StyleGalleryToolBoxControl(
    const css::uno::Reference<css::uno::XComponentContext>& rxContext)
    : Writer2027StyleGalleryToolBoxControl_Base(
          rxContext, css::uno::Reference<css::frame::XFrame>(),
          u".uno:Writer2027StyleGallery"_ustr)
{
}

void SAL_CALL Writer2027StyleGalleryToolBoxControl::initialize(
    const css::uno::Sequence<css::uno::Any>& rArguments)
{
    svt::ToolboxController::initialize(rArguments);
    svx::writer2027::Writer2027LogMessage(
        "writer2027.build",
        u"schema=4 commandURL=.uno:Writer2027StyleGallery"_ustr);
    svx::writer2027::Writer2027LogMessage(
        "writer2027.gallery.controller.initialize",
        u"implementation=lo.writer.Writer2027StyleGalleryToolBoxControl "
        u"git=<see build stamp>"_ustr);
}

css::uno::Reference<css::awt::XWindow> Writer2027StyleGalleryToolBoxControl::createItemWindow(
    const css::uno::Reference<css::awt::XWindow>& rParent)
{
    svx::writer2027::Writer2027LogMessage(
        "writer2027.gallery.controller.createItemWindow",
        u"commandURL=.uno:Writer2027StyleGallery"_ustr);
    css::uno::Reference<css::awt::XWindow> xItemWindow;
    VclPtr<vcl::Window> pParent = VCLUnoHelper::GetWindow(rParent);
    if (!pParent)
        return xItemWindow;
    try
    {
        SolarMutexGuard aGuard;
        mxGallery = VclPtr<Writer2027StyleGallery>::Create(pParent);
        mxGallery->connect_activate(LINK(this, Writer2027StyleGalleryToolBoxControl,
                                         OnStyleActivate));
        RebuildGallery(mxGallery);
        xItemWindow = VCLUnoHelper::GetInterface(mxGallery);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "writer2027.gallery.controller.createItemWindow", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "writer2027.gallery.controller.createItemWindow", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException(
            "writer2027.gallery.controller.createItemWindow");
    }
    return xItemWindow;
}

void Writer2027StyleGalleryToolBoxControl::execute(sal_Int16) {}

void Writer2027StyleGalleryToolBoxControl::update()
{
    if (mxGallery)
        RebuildGallery(mxGallery);
}

void Writer2027StyleGalleryToolBoxControl::statusChanged(
    const css::frame::FeatureStateEvent& /*rEvent*/)
{
    if (mxGallery)
        RebuildGallery(mxGallery);
}

void SAL_CALL Writer2027StyleGalleryToolBoxControl::disposing(
    std::unique_lock<std::mutex>& rGuard)
{
    svt::ToolboxController::disposing(rGuard);
    SolarMutexGuard aSolarMutexGuard;
    if (mxGallery)
        mxGallery.disposeAndClear();
}

void Writer2027StyleGalleryToolBoxControl::RebuildGallery(
    VclPtr<Writer2027StyleGallery> const& pGallery)
{
    if (!pGallery || !m_xFrame.is())
        return;
    try
    {
        const auto aContext
            = sw::writer2027typographymanager::ResolveWriter2027TypographyContext(m_xFrame);
        if (!aContext.pDoc)
        {
            // The frame has no document attached yet (e.g. toolbar built while
            // the view is still initialising). Keep the gallery blank and let a
            // later update()/statusChanged() fill it once the doc is present.
            svx::writer2027::Writer2027LogMessage(
                "writer2027.gallery.model.rebuild",
                u"RESET: no document yet (will retry on next update)"_ustr);
            return;
        }
        // The gallery is the Type-System-semantic style picker: materialize the
        // canonical semantic paragraph styles so a fresh/blank document shows
        // the real cards (they do not exist in a pool until a Type System or a
        // prior apply created them). Idempotent. (spec V3 55/56, V4 20)
        sw::writer2027typographymanager::EnsureSemanticStylesMaterialized(*aContext.pDoc);
        const int nCurrentPoolId = lcl_CurrentParaStylePoolId(m_xFrame);
        auto aItems
            = sw::writer2027stylegallery::BuildStyleGalleryModel(*aContext.pDoc, nCurrentPoolId);
        pGallery->SetFontList(aContext.pFontList);
        pGallery->SetItems(std::move(aItems));
        // Runtime provenance (spec V4 18): itemCount must be >= 9 on a normal
        // Writer document; fewer is logged as a failure, never silently shown.
        const sal_Int32 nCount = static_cast<sal_Int32>(pGallery->GetItems().size());
        svx::writer2027::Writer2027LogMessage(
            "writer2027.gallery.model.rebuild",
            OUString::Concat(u"itemCount=") + OUString::number(nCount)
                + u" (expect>=9)");
        if (nCount < 9)
            svx::writer2027::Writer2027LogMessage(
                "writer2027.gallery.model.rebuild",
                OUString::Concat(u"FAIL: itemCount<9 value=")
                    + OUString::number(nCount));
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "Writer2027StyleGalleryToolBoxControl::RebuildGallery", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException(
            "Writer2027StyleGalleryToolBoxControl::RebuildGallery", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException(
            "Writer2027StyleGalleryToolBoxControl::RebuildGallery");
    }
}

void Writer2027StyleGalleryToolBoxControl::ApplyStyle(const OUString& rStyleName)
{
    if (!rStyleName.isEmpty())
    {
        lcl_PostStyleApply(m_xFrame, rStyleName);
        svx::writer2027::Writer2027LogMessage(
            "stylegallery.apply",
            OUString::Concat(u"style=") + rStyleName);
    }
}

IMPL_LINK(Writer2027StyleGalleryToolBoxControl, OnStyleActivate, const OUString&, rStyleName, void)
{
    ApplyStyle(rStyleName);
}

OUString Writer2027StyleGalleryToolBoxControl::getImplementationName()
{
    return u"com.sun.star.comp.sw.Writer2027StyleGalleryToolBoxControl"_ustr;
}

sal_Bool Writer2027StyleGalleryToolBoxControl::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

css::uno::Sequence<OUString> Writer2027StyleGalleryToolBoxControl::getSupportedServiceNames()
{
    return { u"com.sun.star.frame.ToolbarController"_ustr };
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
sw_Writer2027StyleGalleryToolBoxControl_get_implementation(
    css::uno::XComponentContext* rContext, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Writer2027StyleGalleryToolBoxControl(rContext));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */