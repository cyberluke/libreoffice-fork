/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027SEMANTIC_HXX
#define INCLUDED_SW_WRITER2027SEMANTIC_HXX

#include <cppuhelper/weak.hxx>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/text/XWriter2027Semantic.hpp>

namespace sw::uihandle
{

/** UNO bridge to the canonical Writer 2027 semantic document layer.

    Implements ::com::sun::star::text::XWriter2027Semantic on behalf of the
    extension's AI tool runtime. Every operation resolves the owning Writer
    view from the caller-supplied document model and delegates to the
    canonical implementation (Type System manager, Kit / Block catalogs,
    Story view, Preflight engine) - never to a Python copy of the product.
 */
class Writer2027Semantic final
    : public cppu::WeakImplHelper<css::text::XWriter2027Semantic,
                                  css::lang::XServiceInfo>
{
public:
    explicit Writer2027Semantic(const css::uno::Reference<css::uno::XComponentContext>& xContext);

    // XWriter2027Semantic
    sal_Bool SAL_CALL
    applyTypeSystem(const css::uno::Reference<css::frame::XModel>& rxModel,
                    const OUString& rPresetId) override;
    css::uno::Sequence<css::beans::NamedValue> SAL_CALL listTypeSystems() override;
    css::uno::Sequence<css::beans::NamedValue> SAL_CALL listKits() override;
    sal_Bool SAL_CALL applyKit(const css::uno::Reference<css::frame::XModel>& rxModel,
                               const OUString& rKitId,
                               sal_Bool bApplyRecommendedTypeSystem) override;
    css::uno::Sequence<css::beans::NamedValue> SAL_CALL
    listBlocks(const OUString& rActiveKitId) override;
    sal_Bool SAL_CALL insertBlock(const css::uno::Reference<css::frame::XModel>& rxModel,
                                  const OUString& rBlockId) override;
    void SAL_CALL setStoryMode(const css::uno::Reference<css::frame::XModel>& rxModel,
                               sal_Bool bEnable) override;
    sal_Bool SAL_CALL isStoryMode(const css::uno::Reference<css::frame::XModel>& rxModel) override;
    OUString SAL_CALL runPreflight(const css::uno::Reference<css::frame::XModel>& rxModel,
                                   const OUString& rProfile, sal_Bool bFullMode) override;
    sal_Bool SAL_CALL setActiveKit(const OUString& rKitId) override;

    // XServiceInfo
    OUString SAL_CALL getImplementationName() override;
    sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override;
    css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;

private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    OUString m_aActiveKitId; // session-level, mirrors SwView::m_aActiveKitId
};

} // namespace sw::uihandle

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */