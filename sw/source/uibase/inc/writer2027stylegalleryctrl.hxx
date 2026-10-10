/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027STYLEGALLERYCTRL_HXX
#define INCLUDED_SW_WRITER2027STYLEGALLERYCTRL_HXX

#include <svtools/toolboxcontroller.hxx>
#include <com/sun/star/lang/XServiceInfo.hpp>

#include <cppuhelper/implbase.hxx>

#include <rtl/ustring.hxx>
#include <tools/link.hxx>

#include <writer2027stylegallery.hxx>

using Writer2027StyleGalleryToolBoxControl_Base
    = cppu::ImplInheritanceHelper<svt::ToolboxController, css::lang::XServiceInfo>;

/** Toolbar controller hosting the Writer 2027 semantic Styles gallery in the
    Home notebookbar (spec V3 32/33/39/57).

    Frame-bound: all document lookup and dispatch use this controller's frame
    (never document-global Current()). The gallery model is rebuilt from the
    actual document style pool so it reflects real styles and Type System
    changes. Card activation applies the style via the canonical
    .uno:StyleApply dispatch (spec 38).
 */
class Writer2027StyleGalleryToolBoxControl final : public Writer2027StyleGalleryToolBoxControl_Base
{
    VclPtr<sw::writer2027stylegallery::Writer2027StyleGallery> mxGallery;

public:
    explicit Writer2027StyleGalleryToolBoxControl(
        const css::uno::Reference<css::uno::XComponentContext>& rContext);

    // XToolbarController
    virtual css::uno::Reference<css::awt::XWindow>
        SAL_CALL createItemWindow(const css::uno::Reference<css::awt::XWindow>& xParent) override;
    virtual void SAL_CALL execute(sal_Int16 nKeyModifier) override;
    virtual void SAL_CALL update() override;
    virtual void SAL_CALL statusChanged(const css::frame::FeatureStateEvent& rEvent) override;

    // XComponent
    virtual void SAL_CALL disposing(std::unique_lock<std::mutex>& rGuard) override;

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;

private:
    void RebuildGallery(VclPtr<sw::writer2027stylegallery::Writer2027StyleGallery> const& pGallery);
    void ApplyStyle(const OUString& rStyleName);
    DECL_LINK(OnStyleActivate, const OUString&, void);
};

#endif // INCLUDED_SW_WRITER2027STYLEGALLERYCTRL_HXX