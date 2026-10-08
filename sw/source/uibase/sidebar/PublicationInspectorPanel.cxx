/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "PublicationInspectorPanel.hxx"

#include <calbck.hxx>
#include <cmdid.h>
#include <doc.hxx>
#include <docsh.hxx>
#include <editeng/editids.hrc>
#include <editeng/flstitem.hxx>
#include <flyenum.hxx>
#include <fesh.hxx>
#include <frmfmt.hxx>
#include <names.hxx>
#include <ndarr.hxx>
#include <node.hxx>
#include <rtl/string.hxx>
#include <strings.hrc>
#include <swtypes.hxx>
#include <vcl/vclenum.hxx>
#include <view.hxx>
#include <wrtsh.hxx>

#include <comphelper/dispatchcommand.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <osl/file.hxx>
#include <rtl/ustrbuf.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/filedlghelper.hxx>
#include <sfx2/viewsh.hxx>
#include <svx/svxdlg.hxx>

#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <svl/style.hxx>
#include <com/sun/star/ui/dialogs/XFolderPicker2.hpp>

#include <o3tl/typed_flags_set.hxx>

using namespace sw::writer2027preflight;

namespace sw::sidebar
{

void PublicationInspectorPanel::Notify(SfxBroadcaster& /*rBC*/, const SfxHint& rHint)
{
    if (rHint.GetId() == SfxHintId::DocChanged)
        NotifyDocumentModified();
}

namespace
{

size_t lcl_CategoryIndex(PreflightCategory eCategory)
{
    switch (eCategory)
    {
        case PreflightCategory::Document:
            return 0;
        case PreflightCategory::Accessibility:
            return 1;
        case PreflightCategory::Typography:
            return 2;
        case PreflightCategory::Media:
            return 3;
        case PreflightCategory::LinksReferences:
            return 4;
        case PreflightCategory::Web:
            return 5;
        case PreflightCategory::Pdf:
            return 6;
        case PreflightCategory::Print:
            return 7;
    }
    return 0;
}

bool lcl_MatchesFilter(PreflightSeverity eSeverity, sal_Int32 nFilter)
{
    switch (nFilter)
    {
        case 1:
            return eSeverity == PreflightSeverity::Error;
        case 2:
            return eSeverity == PreflightSeverity::Warning;
        case 3:
            return eSeverity == PreflightSeverity::Info;
        default:
            return true;
    }
}

OUString lcl_SeverityText(PreflightSeverity eSeverity)
{
    switch (eSeverity)
    {
        case PreflightSeverity::Error:
            return SwResId(STR_WRITER2027_PF_SEV_ERROR);
        case PreflightSeverity::Warning:
            return SwResId(STR_WRITER2027_PF_SEV_WARNING);
        case PreflightSeverity::Info:
            return SwResId(STR_WRITER2027_PF_SEV_INFO);
    }
    return OUString();
}
} // namespace

// ---------------------------------------------------------------------------
// Issue entry
// ---------------------------------------------------------------------------

PublicationInspectorIssueEntry::PublicationInspectorIssueEntry(
    weld::Container* pParent, PreflightIssue aIssue, PublicationInspectorPanel& rPanel)
    : m_xBuilder(Application::CreateBuilder(pParent,
                                            u"modules/swriter/ui/publicationinspectorissue.ui"_ustr))
    , m_xContainer(m_xBuilder->weld_box(u"publicationInspectorIssueBox"_ustr))
    , m_xSeverity(m_xBuilder->weld_label(u"severityLabel"_ustr))
    , m_xTarget(m_xBuilder->weld_label(u"targetLabel"_ustr))
    , m_xTitle(m_xBuilder->weld_label(u"titleLabel"_ustr))
    , m_xDescription(m_xBuilder->weld_label(u"descriptionLabel"_ustr))
    , m_xNavigate(m_xBuilder->weld_link_button(u"navigateButton"_ustr))
    , m_xFix(m_xBuilder->weld_button(u"fixButton"_ustr))
    , maIssue(std::move(aIssue))
    , mrPanel(rPanel)
{
    m_xSeverity->set_label(lcl_SeverityText(maIssue.meSeverity));
    m_xTitle->set_label(maIssue.maTitle);
    m_xTitle->set_tooltip_text(maIssue.maDescription);
    m_xDescription->set_label(maIssue.maDescription);
    m_xTarget->set_label(maIssue.maTargetId);

    if (maIssue.mbNavigable)
        m_xNavigate->connect_activate_link(LINK(this, PublicationInspectorIssueEntry, NavigateHdl));
    else
        m_xNavigate->set_visible(false);

    if (maIssue.meFixKind != PreflightFixKind::None)
        m_xFix->connect_clicked(LINK(this, PublicationInspectorIssueEntry, FixHdl));
    else
        m_xFix->set_visible(false);
}

IMPL_LINK_NOARG(PublicationInspectorIssueEntry, NavigateHdl, weld::LinkButton&, bool)
{
    mrPanel.NavigateToIssue(maIssue);
    return true;
}

IMPL_LINK_NOARG(PublicationInspectorIssueEntry, FixHdl, weld::Button&, void)
{
    mrPanel.FixIssue(maIssue);
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

std::unique_ptr<PanelLayout>
PublicationInspectorPanel::Create(weld::Widget* pParent, SfxBindings* pBindings,
                                  css::uno::Reference<css::ui::XSidebar> xSidebar)
{
    if (pParent == nullptr)
        throw css::lang::IllegalArgumentException(
            u"no parent window given to PublicationInspectorPanel::Create"_ustr, nullptr, 0);
    return std::unique_ptr<PublicationInspectorPanel>(
        new PublicationInspectorPanel(pParent, pBindings, xSidebar));
}

PublicationInspectorPanel::PublicationInspectorPanel(
    weld::Widget* pParent, SfxBindings* pBindings, css::uno::Reference<css::ui::XSidebar> xSidebar)
    : PanelLayout(pParent, u"PublicationInspectorPanel"_ustr,
                  u"modules/swriter/ui/publicationinspector.ui"_ustr)
, m_xProfile(m_xBuilder->weld_combo_box(u"profile"_ustr))
, m_xFilter(m_xBuilder->weld_combo_box(u"filter"_ustr))
    , m_xSummary(m_xBuilder->weld_label(u"summary"_ustr))
    , m_xFullScan(m_xBuilder->weld_button(u"full-scan"_ustr))
    , m_xExportReport(m_xBuilder->weld_button(u"export-report"_ustr))
    , mpBindings(pBindings)
    , mpDoc(nullptr)
    , mxSidebar(std::move(xSidebar))
    , maScanTimer("PublicationInspectorPanel")
{
    m_xGroups[0] = m_xBuilder->weld_expander(u"expand-document"_ustr);
    m_xGroups[1] = m_xBuilder->weld_expander(u"expand-accessibility"_ustr);
    m_xGroups[2] = m_xBuilder->weld_expander(u"expand-typography"_ustr);
    m_xGroups[3] = m_xBuilder->weld_expander(u"expand-media"_ustr);
    m_xGroups[4] = m_xBuilder->weld_expander(u"expand-links"_ustr);
    m_xGroups[5] = m_xBuilder->weld_expander(u"expand-web"_ustr);
    m_xGroups[6] = m_xBuilder->weld_expander(u"expand-pdf"_ustr);
    m_xGroups[7] = m_xBuilder->weld_expander(u"expand-print"_ustr);
    m_xBoxes[0] = m_xBuilder->weld_box(u"box-document"_ustr);
    m_xBoxes[1] = m_xBuilder->weld_box(u"box-accessibility"_ustr);
    m_xBoxes[2] = m_xBuilder->weld_box(u"box-typography"_ustr);
    m_xBoxes[3] = m_xBuilder->weld_box(u"box-media"_ustr);
    m_xBoxes[4] = m_xBuilder->weld_box(u"box-links"_ustr);
    m_xBoxes[5] = m_xBuilder->weld_box(u"box-web"_ustr);
    m_xBoxes[6] = m_xBuilder->weld_box(u"box-pdf"_ustr);
    m_xBoxes[7] = m_xBuilder->weld_box(u"box-print"_ustr);

    m_xProfile->connect_changed(LINK(this, PublicationInspectorPanel, ProfileChangedHdl));
    m_xFilter->connect_changed(LINK(this, PublicationInspectorPanel, FilterChangedHdl));
    m_xFullScan->connect_clicked(LINK(this, PublicationInspectorPanel, FullScanHdl));
    m_xExportReport->connect_clicked(LINK(this, PublicationInspectorPanel, ExportReportHdl));

    maScanTimer.SetInvokeHandler(LINK(this, PublicationInspectorPanel, ScanTimerHdl));
    maScanTimer.SetTimeout(800); // debounce document changes

    SwDocShell* pDocSh = dynamic_cast<SwDocShell*>(SfxObjectShell::Current());
    if (!pDocSh)
        return;
    mpDoc = pDocSh->GetDoc();
    if (mpBindings)
        StartListening(*mpBindings);

    // Initial scan (asynchronous so the panel appears immediately).
    Application::PostUserEvent(LINK(this, PublicationInspectorPanel, InitialScanHdl));
}

IMPL_LINK_NOARG(PublicationInspectorPanel, InitialScanHdl, void*, void)
{
    RunScan(PreflightScanMode::Quick);
}

PublicationInspectorPanel::~PublicationInspectorPanel()
{
    maScanTimer.Stop();
}

void PublicationInspectorPanel::NotifyDocumentModified()
{
    // Debounced: repeated modifications restart the timer.
    if (mpDoc)
        maScanTimer.Start();
}

void PublicationInspectorPanel::RunScan(PreflightScanMode eMode)
{
    if (!mpDoc)
        return;
    PreflightOptions aOptions;
    aOptions.meProfile = static_cast<PreflightProfile>(m_xProfile->get_active());
    aOptions.meMode = eMode;
    if (SwDocShell* pShell = mpDoc->GetDocShell())
    {
        if (const SvxFontListItem* pItem = pShell->GetItem(SID_ATTR_CHAR_FONTLIST))
            aOptions.mpFontList = pItem->GetFontList();
    }
    maReport = RunPreflight(*mpDoc, aOptions);
    mbHasReport = true;
    RebuildEntries();
    UpdateSummary();
}

void PublicationInspectorPanel::RebuildEntries()
{
    for (size_t nGroup = 0; nGroup < 8; ++nGroup)
    {
        for (auto const& xEntry : m_aEntries[nGroup])
            m_xBoxes[nGroup]->move(xEntry->get_widget(), nullptr);
        m_aEntries[nGroup].clear();
    }

    const sal_Int32 nFilter = m_xFilter->get_active();
    bool bGroupHasVisible[8] = { false, false, false, false, false, false, false, false };
    for (const PreflightIssue& rIssue : maReport.maIssues)
    {
        if (!lcl_MatchesFilter(rIssue.meSeverity, nFilter))
            continue;
        const size_t nGroup = lcl_CategoryIndex(rIssue.meCategory);
        auto xEntry = std::make_unique<PublicationInspectorIssueEntry>(
            m_xBoxes[nGroup].get(), rIssue, *this);
        m_xBoxes[nGroup]->reorder_child(xEntry->get_widget(), -1);
        m_aEntries[nGroup].push_back(std::move(xEntry));
        bGroupHasVisible[nGroup] = true;
    }
    for (size_t nGroup = 0; nGroup < 8; ++nGroup)
    {
        if (bGroupHasVisible[nGroup])
            m_xGroups[nGroup]->show();
        else
            m_xGroups[nGroup]->hide();
    }
    if (mxSidebar.is())
        mxSidebar->requestLayout();
}

void PublicationInspectorPanel::UpdateSummary()
{
    if (!mbHasReport)
    {
        m_xSummary->set_label(SwResId(STR_WRITER2027_PF_SUMMARY_IDLE));
        return;
    }
    if (maReport.mnErrors == 0 && maReport.mnWarnings == 0 && maReport.mnInfos == 0)
    {
        m_xSummary->set_label(SwResId(STR_WRITER2027_PF_SUMMARY_CLEAN));
        return;
    }
    OUString aText = SwResId(STR_WRITER2027_PF_SUMMARY);
    aText = aText.replaceAll(u"$1"_ustr, OUString::number(maReport.mnErrors));
    aText = aText.replaceAll(u"$2"_ustr, OUString::number(maReport.mnWarnings));
    aText = aText.replaceAll(u"$3"_ustr, OUString::number(maReport.mnInfos));
    m_xSummary->set_label(aText);
}

IMPL_LINK_NOARG(PublicationInspectorPanel, ScanTimerHdl, Timer*, void)
{
    RunScan(PreflightScanMode::Quick);
}

IMPL_LINK_NOARG(PublicationInspectorPanel, ProfileChangedHdl, weld::ComboBox&, void)
{
    if (mbHasReport)
        RunScan(PreflightScanMode::Quick);
}

IMPL_LINK_NOARG(PublicationInspectorPanel, FilterChangedHdl, weld::ComboBox&, void)
{
    if (mbHasReport)
        RebuildEntries();
}

IMPL_LINK_NOARG(PublicationInspectorPanel, FullScanHdl, weld::Button&, void)
{
    RunScan(PreflightScanMode::Full);
}

void PublicationInspectorPanel::NavigateToIssue(const PreflightIssue& rIssue)
{
    SwDocShell* pShell = mpDoc ? mpDoc->GetDocShell() : nullptr;
    if (!pShell)
        return;
    SwWrtShell* pWrtShell = pShell->GetWrtShell();
    if (!pWrtShell)
        return;
    pWrtShell->AssureStdMode();

    switch (rIssue.meTargetKind)
    {
        case PreflightTargetKind::Image:
        case PreflightTargetKind::Frame:
            pWrtShell->GotoFly(UIName(rIssue.maObjectName), FLYCNTTYPE_ALL, true);
            break;
        case PreflightTargetKind::Table:
            pWrtShell->GotoTable(UIName(rIssue.maObjectName));
            break;
        case PreflightTargetKind::Heading:
        case PreflightTargetKind::TextPosition:
        {
            const SwNodes& rNodes = mpDoc->GetNodes();
            if (rIssue.mnNodeIndex < 0 || rIssue.mnNodeIndex >= static_cast<sal_Int64>(rNodes.Count().get()))
                break;
            SwContentNode* pContentNode
                = rNodes[SwNodeOffset(rIssue.mnNodeIndex)]->GetContentNode();
            if (!pContentNode)
                break;
            const sal_Int32 nStart = std::max(rIssue.mnStart, sal_Int32(0));
            const sal_Int32 nEnd
                = std::max(rIssue.mnEnd, nStart + 1); // at least one character
            SwPosition aStart(*pContentNode, nStart);
            SwPosition aEnd(*pContentNode, std::min(nEnd, pContentNode->Len()));
            pWrtShell->StartAllAction();
            SwPaM* pPaM = pWrtShell->GetCursor();
            *pPaM->GetPoint() = std::move(aEnd);
            pPaM->SetMark();
            *pPaM->GetMark() = std::move(aStart);
            pWrtShell->EndAllAction();
            pWrtShell->GetView().BringToAttention(pContentNode);
            break;
        }
        default:
            break;
    }
    pWrtShell->GetView().GetEditWin().GrabFocus();
}

void PublicationInspectorPanel::FixIssue(const PreflightIssue& rIssue)
{
    SwDocShell* pShell = mpDoc ? mpDoc->GetDocShell() : nullptr;
    if (!pShell)
        return;

    switch (rIssue.meFixKind)
    {
        case PreflightFixKind::OpenImageProperties:
        {
            // Select the object first, then open the canonical image
            // properties dialog (title + description = alt text).
            if (rIssue.mbNavigable)
                NavigateToIssue(rIssue);
            SwFlyFrameFormat* pFlyFormat
                = rIssue.maObjectName.isEmpty()
                      ? nullptr
                      : const_cast<SwFlyFrameFormat*>(
                          mpDoc->FindFlyByName(UIName(rIssue.maObjectName)));
            if (!pFlyFormat)
                break;
            SwWrtShell* pWrtShell = pShell->GetWrtShell();
            SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
            VclPtr<AbstractSvxObjectTitleDescDialog> pDlg(pFact->CreateSvxObjectTitleDescDialog(
                pWrtShell->GetView().GetFrameWeld(), pFlyFormat->GetObjTitle(),
                pFlyFormat->GetObjDescription(), pFlyFormat->IsDecorative()));
            pDlg->StartExecuteAsync(
                [this, pDlg, pFlyFormat, pWrtShell](sal_Int32 nResult) -> void {
                    if (nResult == RET_OK)
                    {
                        mpDoc->SetFlyFrameTitle(*pFlyFormat, pDlg->GetTitle());
                        mpDoc->SetFlyFrameDescription(*pFlyFormat, pDlg->GetDescription());
                        mpDoc->SetFlyFrameDecorative(*pFlyFormat, pDlg->IsDecorative());
                        pWrtShell->SetModified();
                    }
                    pDlg->disposeOnce();
                    if (mpDoc)
                        NotifyDocumentModified();
                });
            break;
        }
        case PreflightFixKind::OpenDocumentLanguage:
        {
            const css::uno::Reference<css::frame::XModel> xModel(pShell->GetModel(),
                                                                 css::uno::UNO_QUERY_THROW);
            css::uno::Sequence<css::beans::PropertyValue> aArgs{
                comphelper::makePropertyValue(u"Language"_ustr, u"*"_ustr)
            };
            comphelper::dispatchCommand(u".uno:LanguageStatus"_ustr,
                                        xModel->getCurrentController()->getFrame(), aArgs);
            break;
        }
        case PreflightFixKind::OpenTypeSystem:
        {
            if (mpBindings)
                mpBindings->GetDispatcher()->Execute(FN_WRITER2027_TYPE_SYSTEM,
                                                     SfxCallMode::SYNCHRON);
            break;
        }
        case PreflightFixKind::OpenParagraphStyle:
        {
            const css::uno::Reference<css::frame::XModel> xModel(pShell->GetModel(),
                                                                 css::uno::UNO_QUERY_THROW);
            css::uno::Sequence<css::beans::PropertyValue> aArgs{
                comphelper::makePropertyValue(u"Param"_ustr, rIssue.maObjectName),
                comphelper::makePropertyValue(u"Family"_ustr,
                                              sal_Int16(SfxStyleFamily::Para))
            };
            comphelper::dispatchCommand(u".uno:EditStylePara"_ustr,
                                        xModel->getCurrentController()->getFrame(), aArgs);
            break;
        }
        case PreflightFixKind::OpenCharacterStyle:
        {
            const css::uno::Reference<css::frame::XModel> xModel(pShell->GetModel(),
                                                                 css::uno::UNO_QUERY_THROW);
            css::uno::Sequence<css::beans::PropertyValue> aArgs{
                comphelper::makePropertyValue(u"Param"_ustr, rIssue.maObjectName),
                comphelper::makePropertyValue(u"Family"_ustr,
                                              sal_Int16(SfxStyleFamily::Char))
            };
            comphelper::dispatchCommand(u".uno:EditStyleFont"_ustr,
                                        xModel->getCurrentController()->getFrame(), aArgs);
            break;
        }
        default:
            break;
    }
}

IMPL_LINK_NOARG(PublicationInspectorPanel, ExportReportHdl, weld::Button&, void)
{
    if (!mbHasReport)
        RunScan(PreflightScanMode::Quick);
    if (!mbHasReport)
        return;

    weld::Window* pParent = dynamic_cast<weld::Window*>(get_widget());
    const css::uno::Reference<css::ui::dialogs::XFolderPicker2> xPicker
        = sfx2::createFolderPicker(comphelper::getProcessComponentContext(), pParent);
    if (xPicker->execute() != RET_OK)
        return;

    const OUString aDir = xPicker->getDirectory();
    const OUString aOutUrl = aDir + u"/preflight-report.json"_ustr;
    const OUString aJson = PreflightReportToJson(maReport);
    const OString aData = OUStringToOString(aJson, RTL_TEXTENCODING_UTF8);

    osl::File aFile(aOutUrl);
    if (aFile.open(osl_File_OpenFlag_Create | osl_File_OpenFlag_Write) != osl::FileBase::E_None)
        return;
    sal_uInt64 nWritten = 0;
    aFile.write(aData.getStr(), aData.getLength(), nWritten);
    aFile.close();

    m_xSummary->set_label(
        SwResId(STR_WRITER2027_PF_REPORT_WRITTEN).replaceAll(u"$1"_ustr, aOutUrl));
}

} // namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */