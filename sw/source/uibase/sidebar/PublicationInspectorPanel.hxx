/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_SOURCE_UIBASE_SIDEBAR_PUBLICATIONINSPECTORPANEL_HXX
#define INCLUDED_SW_SOURCE_UIBASE_SIDEBAR_PUBLICATIONINSPECTORPANEL_HXX

#include <sfx2/bindings.hxx>
#include <sfx2/sidebar/PanelLayout.hxx>
#include <svl/lstner.hxx>
#include <tools/link.hxx>
#include <vcl/timer.hxx>
#include <vcl/weld/Box.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/ComboBox.hxx>
#include <vcl/weld/Container.hxx>
#include <vcl/weld/Expander.hxx>
#include <vcl/weld/Label.hxx>
#include <vcl/weld/LinkButton.hxx>

#include <com/sun/star/ui/XSidebar.hpp>

#include <writer2027preflight.hxx>

#include <memory>
#include <vector>

class SwDoc;

namespace com::sun::star::ui
{
class XSidebar;
}

namespace sw::sidebar
{
class PublicationInspectorPanel;

/** One issue card in the Publication Inspector panel. */
class PublicationInspectorIssueEntry final
{
public:
    PublicationInspectorIssueEntry(weld::Container* pParent,
                                   sw::writer2027preflight::PreflightIssue aIssue,
                                   PublicationInspectorPanel& rPanel);

    weld::Widget* get_widget() const { return m_xContainer.get(); }
    void set_visible(bool bVisible) const { m_xContainer->set_visible(bVisible); }
    bool is_visible() const { return m_xContainer->is_visible(); }

private:
    DECL_LINK(NavigateHdl, weld::LinkButton&, bool);
    DECL_LINK(FixHdl, weld::Button&, void);

    std::unique_ptr<weld::Builder> m_xBuilder;
    std::unique_ptr<weld::Box> m_xContainer;
    std::unique_ptr<weld::Label> m_xSeverity;
    std::unique_ptr<weld::Label> m_xTarget;
    std::unique_ptr<weld::Label> m_xTitle;
    std::unique_ptr<weld::Label> m_xDescription;
    std::unique_ptr<weld::LinkButton> m_xNavigate;
    std::unique_ptr<weld::Button> m_xFix;

    sw::writer2027preflight::PreflightIssue maIssue;
    PublicationInspectorPanel& mrPanel;
};

/** Writer 2027 Publication Inspector (Phase 10): a modeless sidebar deck that
    runs the deterministic preflight engine and lets the user navigate to
    issues and open canonical fix dialogs. */
class PublicationInspectorPanel final : public PanelLayout, public SfxListener
{
public:
    static std::unique_ptr<PanelLayout>
    Create(weld::Widget* pParent, SfxBindings* pBindings,
           css::uno::Reference<css::ui::XSidebar> xSidebar);

    weld::Widget* get_widget() const { return m_xContainer.get(); }

    virtual ~PublicationInspectorPanel() override;

    /** Debounced entry point: document changed -> Quick rescan. */
    void NotifyDocumentModified();

    /** Read-only navigation to an issue (heading/text/image/frame/table). */
    void NavigateToIssue(const sw::writer2027preflight::PreflightIssue& rIssue);

    /** Explicit safe fix entry point (opens canonical editing UI only). */
    void FixIssue(const sw::writer2027preflight::PreflightIssue& rIssue);

    /** SfxListener: document changed -> debounced Quick rescan. */
    void Notify(SfxBroadcaster& rBC, const SfxHint& rHint) override;

private:
    PublicationInspectorPanel(weld::Widget* pParent, SfxBindings* pBindings,
                              css::uno::Reference<css::ui::XSidebar> xSidebar);

    void RunScan(sw::writer2027preflight::PreflightScanMode eMode);
    void RebuildEntries();
    void UpdateSummary();

    DECL_LINK(ProfileChangedHdl, weld::ComboBox&, void);
    DECL_LINK(FilterChangedHdl, weld::ComboBox&, void);
    DECL_LINK(FullScanHdl, weld::Button&, void);
    DECL_LINK(ExportReportHdl, weld::Button&, void);
    DECL_LINK(ScanTimerHdl, Timer*, void);
    DECL_LINK(InitialScanHdl, void*, void);

    std::unique_ptr<weld::ComboBox> m_xProfile;
    std::unique_ptr<weld::ComboBox> m_xFilter;
    std::unique_ptr<weld::Label> m_xSummary;
    std::unique_ptr<weld::Button> m_xFullScan;
    std::unique_ptr<weld::Button> m_xExportReport;

    std::unique_ptr<weld::Expander> m_xGroups[8];
    std::unique_ptr<weld::Box> m_xBoxes[8];

    std::vector<std::unique_ptr<PublicationInspectorIssueEntry>> m_aEntries[8];

    SfxBindings* mpBindings;
    SwDoc* mpDoc;
    css::uno::Reference<css::ui::XSidebar> mxSidebar;

    Timer maScanTimer;

    sw::writer2027preflight::PreflightReport maReport;
    bool mbHasReport = false;
};

} // namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UIBASE_SIDEBAR_PUBLICATIONINSPECTORPANEL_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */