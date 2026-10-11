/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <config_features.h>

#include <rtl/ustring.hxx>
#include <sal/log.hxx>

#include <vector>

#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <cppuhelper/factory.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/compbase.hxx>

#include <comphelper/getexpandeduri.hxx>
#include <comphelper/sequence.hxx>

#include <vcl/svapp.hxx>

#include <svx/dialmgr.hxx>
#include <svx/strings.hrc>
#include <svx/writer2027blocks.hxx>
#include <svx/writer2027typesystem.hxx>

#include <unotxdoc.hxx>
#include <view.hxx>
#include <editsh.hxx>
#include <docsh.hxx>
#include <wrtsh.hxx>
#include <cmdid.h>
#include <writer2027blocks.hxx>
#include <writer2027preflight.hxx>
#include <writer2027semantic.hxx>
#include <writer2027typographymanager.hxx>
#include <writer2027view.hxx>

#include <sfx2/dispatch.hxx>
#include <sfx2/viewsh.hxx>
#include <sfx2/objsh.hxx>

#include <comphelper/servicehelper.hxx>
#include <com/sun/star/lang/XSingleComponentFactory.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::lang;

namespace sw::uihandle
{

// ── frame / document resolution ──────────────────────────────────────────

/// Resolve the Writer SwView for a given frame, mirroring the Writer 2027
/// toolbar controllers (never SfxObjectShell::Current / SwModule::GetFirstView).
static SwView* lcl_FrameSwView(const Reference<XFrame>& rxFrame)
{
    if (!rxFrame.is())
        return nullptr;
    try
    {
        Reference<XController> xController = rxFrame->getController();
        if (!xController.is())
            return nullptr;
        SwXTextDocument* pTextDoc
            = comphelper::getFromUnoTunnel<SwXTextDocument>(xController->getModel());
        if (!pTextDoc || !pTextDoc->GetDocShell())
            return nullptr;
        return pTextDoc->GetDocShell()->GetView();
    }
    catch (const Exception&)
    {
    }
    return nullptr;
}

/// Resolve the owning frame from a Writer document model.
static Reference<XFrame> lcl_ModelFrame(const Reference<XModel>& rxModel)
{
    if (!rxModel.is())
        return {};
    try
    {
        Reference<XController> xController = rxModel->getCurrentController();
        if (xController.is())
            return xController->getFrame();
    }
    catch (const Exception&)
    {
    }
    return {};
}

/// Resolve the Writer SwView for a model, or nullptr.
static SwView* lcl_ModelSwView(const Reference<XModel>& rxModel)
{
    return lcl_FrameSwView(lcl_ModelFrame(rxModel));
}

// ── service ──────────────────────────────────────────────────────────────

Writer2027Semantic::Writer2027Semantic(
    const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
{
}

sal_Bool SAL_CALL Writer2027Semantic::applyTypeSystem(const Reference<XModel>& rxModel,
                                                      const OUString& rPresetId)
{
    SolarMutexGuard aGuard;
    Reference<XFrame> xFrame = lcl_ModelFrame(rxModel);
    if (!xFrame.is())
        return false;
    try
    {
        // Deleagates to the canonical Typography Manager transaction
        // (semantic styles + managed-override cleanup + insertion cleanup).
        auto aResult
            = sw::writer2027typographymanager::ApplyPresetTransaction(xFrame, rPresetId);
        return aResult.success;
    }
    catch (const Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    return false;
}

Sequence<NamedValue> SAL_CALL Writer2027Semantic::listTypeSystems()
{
    SolarMutexGuard aGuard;
    std::vector<NamedValue> aResult;
    try
    {
        const auto& aPresets
            = svx::writer2027::Writer2027TypeSystemCatalog::Get().GetPresets();
        aResult.reserve(aPresets.size());
        for (const auto& rPreset : aPresets)
        {
            std::vector<NamedValue> aFields;
            aFields.emplace_back(u"id"_ustr, Any(rPreset.maId));
            aFields.emplace_back(u"name"_ustr, Any(SvxResId(rPreset.maNameResId)));
            aFields.emplace_back(u"description"_ustr, Any(OUString()));
            Any aValue;
            aValue <<= comphelper::containerToSequence(aFields);
            aResult.emplace_back(u"typesystem"_ustr, aValue);
        }
    }
    catch (const Exception&)
    {
    }
    return comphelper::containerToSequence(aResult);
}

Sequence<NamedValue> SAL_CALL Writer2027Semantic::listKits()
{
    SolarMutexGuard aGuard;
    std::vector<NamedValue> aResult;
    try
    {
        const auto& aKits = svx::writer2027::Writer2027DocumentKitCatalog::Get().GetKits();
        aResult.reserve(aKits.size());
        for (const auto& rKit : aKits)
        {
            std::vector<NamedValue> aFields;
            aFields.emplace_back(u"id"_ustr, Any(rKit.maId));
            aFields.emplace_back(u"name"_ustr, Any(SvxResId(rKit.maNameResId)));
            aFields.emplace_back(u"description"_ustr, Any(SvxResId(rKit.maDescriptionResId)));
            aFields.emplace_back(u"typesystem_id"_ustr, Any(rKit.maTypeSystemId));
            Any aValue;
            aValue <<= comphelper::containerToSequence(aFields);
            aResult.emplace_back(u"kit"_ustr, aValue);
        }
    }
    catch (const Exception&)
    {
    }
    return comphelper::containerToSequence(aResult);
}

sal_Bool SAL_CALL Writer2027Semantic::applyKit(const Reference<XModel>& rxModel,
                                               const OUString& rKitId,
                                               sal_Bool bApplyRecommendedTypeSystem)
{
    SolarMutexGuard aGuard;
    SwView* pView = lcl_ModelSwView(rxModel);
    if (!pView || !pView->GetDocShell() || pView->GetDocShell()->IsReadOnly())
        return false;
    try
    {
        const svx::writer2027::DocumentKit* pKit
            = svx::writer2027::Writer2027DocumentKitCatalog::Get().FindKit(rKitId);
        if (!pKit)
            return false;

        // The kit is a session-level insertion aid; the document remains the
        // source of truth (spec 27).
        m_aActiveKitId = rKitId;

        if (bApplyRecommendedTypeSystem && !pKit->maTypeSystemId.isEmpty())
        {
            const svx::writer2027::TypeSystemPreset* pPreset
                = svx::writer2027::Writer2027TypeSystemCatalog::Get().FindPreset(
                    pKit->maTypeSystemId);
            if (pPreset)
            {
                // Apply without the interactive confirmation dialog, since the
                // caller (an AI runtime) has already decided to apply.
                auto aResult
                    = sw::writer2027typographymanager::ApplyPresetTransaction(
                        lcl_ModelFrame(rxModel), pKit->maTypeSystemId);
                return aResult.success;
            }
        }
        return true;
    }
    catch (const Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    return false;
}

Sequence<NamedValue> SAL_CALL
Writer2027Semantic::listBlocks(const OUString& rActiveKitId)
{
    SolarMutexGuard aGuard;
    std::vector<NamedValue> aResult;
    try
    {
        // Recommended group first when a kit is active (block gallery order).
        const auto& aBlocks = svx::writer2027::Writer2027EditorialBlockCatalog::Get().GetBlocks();
        aResult.reserve(aBlocks.size());
        for (const auto& rBlock : aBlocks)
        {
            (void)rActiveKitId; // ordering refinement reserved for later
            std::vector<NamedValue> aFields;
            aFields.emplace_back(u"id"_ustr, Any(rBlock.maId));
            aFields.emplace_back(u"name"_ustr, Any(SvxResId(rBlock.maNameResId)));
            aFields.emplace_back(u"description"_ustr,
                                 Any(SvxResId(rBlock.maDescriptionResId)));
            aFields.emplace_back(u"category"_ustr,
                                 Any(sal_Int32(rBlock.meCategory)));
            aFields.emplace_back(u"insertion_policy"_ustr,
                                 Any(sal_Int32(rBlock.meInsertionPolicy)));
            Any aValue;
            aValue <<= comphelper::containerToSequence(aFields);
            aResult.emplace_back(u"block"_ustr, aValue);
        }
    }
    catch (const Exception&)
    {
    }
    return comphelper::containerToSequence(aResult);
}

sal_Bool SAL_CALL Writer2027Semantic::insertBlock(const Reference<XModel>& rxModel,
                                                  const OUString& rBlockId)
{
    SolarMutexGuard aGuard;
    SwView* pView = lcl_ModelSwView(rxModel);
    if (!pView || !pView->GetDocShell() || pView->GetDocShell()->IsReadOnly())
        return false;
    try
    {
        const svx::writer2027::EditorialBlockDefinition* pBlock
            = svx::writer2027::Writer2027EditorialBlockCatalog::Get().FindBlock(rBlockId);
        if (!pBlock)
            return false;
        // Canonical insertion builds the block from Writer primitives only.
        return sw::writer2027blocks::InsertEditorialBlock(*pView->GetWrtShellPtr(), *pBlock,
                                                          nullptr);
    }
    catch (const Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    return false;
}

void SAL_CALL Writer2027Semantic::setStoryMode(const Reference<XModel>& rxModel,
                                               sal_Bool bEnable)
{
    SolarMutexGuard aGuard;
    SwView* pView = lcl_ModelSwView(rxModel);
    if (!pView || !pView->GetWrtShellPtr())
        return;
    try
    {
        const bool bStory
            = sw::writer2027view::GetCurrentAuthoringMode(*pView->GetWrtShellPtr())
              == sw::writer2027view::AuthoringMode::Story;
        if (bStory == static_cast<bool>(bEnable))
            return;
        // Route through the canonical Writer 2027 Story command so the full
        // view-option apply logic (draft/browse, zoom, ruler) is reused.
        pView->GetDispatcher().ExecuteList(FN_WRITER2027_STORY, SfxCallMode::SLOT, {});
    }
    catch (const Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
}

sal_Bool SAL_CALL Writer2027Semantic::isStoryMode(const Reference<XModel>& rxModel)
{
    SolarMutexGuard aGuard;
    SwView* pView = lcl_ModelSwView(rxModel);
    if (!pView || !pView->GetWrtShellPtr())
        return false;
    try
    {
        return sw::writer2027view::GetCurrentAuthoringMode(*pView->GetWrtShellPtr())
               == sw::writer2027view::AuthoringMode::Story;
    }
    catch (const Exception&)
    {
    }
    return false;
}

OUString SAL_CALL Writer2027Semantic::runPreflight(const Reference<XModel>& rxModel,
                                                   const OUString& rProfile,
                                                   sal_Bool bFullMode)
{
    SolarMutexGuard aGuard;
    SwView* pView = lcl_ModelSwView(rxModel);
    if (!pView || !pView->GetDocShell() || !pView->GetDocShell()->GetDoc())
        return u"{}"_ustr;
    try
    {
        sw::writer2027preflight::PreflightOptions aOptions;
        aOptions.meMode = bFullMode ? sw::writer2027preflight::PreflightScanMode::Full
                                    : sw::writer2027preflight::PreflightScanMode::Quick;
        // Stable, language-neutral profile id mapping.
        if (rProfile == "pdf_digital")
            aOptions.meProfile = sw::writer2027preflight::PreflightProfile::PdfDigital;
        else if (rProfile == "print_friendly")
            aOptions.meProfile = sw::writer2027preflight::PreflightProfile::PrintFriendly;
        else if (rProfile == "print_digital")
            aOptions.meProfile = sw::writer2027preflight::PreflightProfile::PrintDigital;
        else if (rProfile == "web_package")
            aOptions.meProfile = sw::writer2027preflight::PreflightProfile::WebPackage;
        else if (rProfile == "single_html")
            aOptions.meProfile = sw::writer2027preflight::PreflightProfile::SingleHtml;
        else
            aOptions.meProfile = sw::writer2027preflight::PreflightProfile::General;

        auto aReport
            = sw::writer2027preflight::RunPreflight(*pView->GetDocShell()->GetDoc(), aOptions);
        return sw::writer2027preflight::PreflightReportToJson(aReport);
    }
    catch (const Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    return u"{}"_ustr;
}

sal_Bool SAL_CALL Writer2027Semantic::setActiveKit(const OUString& rKitId)
{
    SolarMutexGuard aGuard;
    if (svx::writer2027::Writer2027DocumentKitCatalog::Get().FindKit(rKitId))
    {
        m_aActiveKitId = rKitId;
        return true;
    }
    return false;
}

// ── XServiceInfo ─────────────────────────────────────────────────────────

OUString SAL_CALL Writer2027Semantic::getImplementationName()
{
    return u"com.sun.star.comp.writer.Writer2027Semantic"_ustr;
}

sal_Bool SAL_CALL Writer2027Semantic::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence<OUString> SAL_CALL Writer2027Semantic::getSupportedServiceNames()
{
    return { u"com.sun.star.text.Writer2027Semantic"_ustr };
}

} // namespace sw::uihandle

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
sw_writer2027semantic_get_implementation(css::uno::XComponentContext* pCtx,
                                         css::uno::Sequence<css::uno::Any> const&)
{
    SolarMutexGuard aGuard;
    css::uno::Reference<css::uno::XComponentContext> xContext(pCtx);
    return cppu::acquire(new sw::uihandle::Writer2027Semantic(xContext));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */