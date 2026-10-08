/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027publishdialog.hxx>
#include <writer2027web.hxx>

#include <sfx2/filedlghelper.hxx>
#include <sfx2/sidebar/SidebarController.hxx>
#include <sfx2/viewsh.hxx>
#include <comphelper/processfactory.hxx>

#include <com/sun/star/system/SystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecuteFlags.hpp>
#include <com/sun/star/ui/dialogs/XFolderPicker2.hpp>

#include <doc.hxx>
#include <docsh.hxx>
#include <sfx2/docfile.hxx>
#include <strings.hrc>
#include <swtypes.hxx>
#include <tools/urlobj.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld/MessageDialog.hxx>
#include <writer2027preflight.hxx>

#include <com/sun/star/uno/Reference.hxx>

namespace sw::writer2027publish
{

namespace
{

/// Open a file URL with the system default application (browser).
void lcl_OpenInBrowser(const OUString& rFileUrl)
{
    try
    {
        uno::Reference<css::system::XSystemShellExecute> xShell(
            css::system::SystemShellExecute::create(comphelper::getProcessComponentContext()));
        xShell->execute(rFileUrl, OUString(), css::system::SystemShellExecuteFlags::DEFAULTS);
    }
    catch (const css::uno::Exception&)
    {
        // best-effort preview: never block the user because of it
    }
}

} // namespace

bool Writer2027PublishDialog::Run(SwDoc& rDoc, sw::writer2027web::WebPublishOptions& rOptions)
{
    mpDoc = &rDoc;
    mpOptions = &rOptions;
    mbPublished = false;

    if (!m_xBuilder)
    {
        m_xBuilder = Application::CreateBuilder(nullptr, u"sw/ui/writer2027publishdialog.ui"_ustr);
        m_xDialog = m_xBuilder->weld_dialog(u"Writer2027PublishDialog"_ustr);
        m_xWebPackage = m_xBuilder->weld_radio_button(u"web-package"_ustr);
        m_xSingleHtml = m_xBuilder->weld_radio_button(u"single-html"_ustr);
        m_xResponsive = m_xBuilder->weld_check_button(u"responsive"_ustr);
        m_xMetadata = m_xBuilder->weld_check_button(u"metadata"_ustr);
        m_xPrintCss = m_xBuilder->weld_check_button(u"print-css"_ustr);
        m_xGenerateNav = m_xBuilder->weld_check_button(u"generate-nav"_ustr);
        m_xOptimizeImages = m_xBuilder->weld_check_button(u"optimize-images"_ustr);
        m_xEmbedFonts = m_xBuilder->weld_check_button(u"embed-fonts"_ustr);
        m_xPublish = m_xBuilder->weld_button(u"publish"_ustr);
        m_xPublishOpen = m_xBuilder->weld_button(u"publish-open"_ustr);

        m_xPublish->connect_clicked(LINK(this, Writer2027PublishDialog, PublishHdl));
        m_xPublishOpen->connect_clicked(LINK(this, Writer2027PublishDialog, PublishOpenHdl));
        // Enter activates Publish (default button); the failure path keeps
        // the dialog open so the user can retry or cancel.
        m_xDialog->change_default_button(nullptr, m_xPublish.get());
    }

    m_xDialog->run();
    return mbPublished;
}

IMPL_LINK(Writer2027PublishDialog, PublishHdl, weld::Button&, rButton, void)
{
    Publish(/*bOpenInBrowser=*/false);
}

IMPL_LINK(Writer2027PublishDialog, PublishOpenHdl, weld::Button&, rButton, void)
{
    Publish(/*bOpenInBrowser=*/true);
}

void Writer2027PublishDialog::Publish(bool bOpenInBrowser)
{
    if (!mpDoc || !mpOptions)
        return;

    mpOptions->meFormat = m_xSingleHtml->get_active()
                              ? sw::writer2027web::WebPublishFormat::SingleHtml
                              : sw::writer2027web::WebPublishFormat::WebPackage;
    mpOptions->mbResponsive = m_xResponsive->get_active();
    mpOptions->mbIncludeMetadata = m_xMetadata->get_active();
    mpOptions->mbIncludePrintCss = m_xPrintCss->get_active();
    mpOptions->mbGenerateNav = m_xGenerateNav->get_active();
    mpOptions->mbOptimizeImages = m_xOptimizeImages->get_active();
    mpOptions->mbEmbedFonts = m_xEmbedFonts->get_active();

    // Phase 10: Full preflight gate before export (errors overrideable).
    if (!RunPreflightGate())
        return; // user chose to review issues; keep the dialog open

    // Choose the output folder.
    weld::Window* pParent = m_xDialog.get();
    uno::Reference<css::ui::dialogs::XFolderPicker2> xFolderPicker
        = sfx2::createFolderPicker(comphelper::getProcessComponentContext(), pParent);
    xFolderPicker->setTitle(SwResId(STR_WRITER2027_PUBLISH_CHOOSE_FOLDER));

    // Default to the document's own folder when it lives on disk.
    OUString aDefaultDir;
    if (SwDocShell* pShell = mpDoc->GetDocShell())
    {
        if (const SfxMedium* pMedium = pShell->GetMedium())
        {
            INetURLObject aURL(pMedium->GetURLObject());
            if (aURL.GetProtocol() == INetProtocol::File)
            {
                aURL.removeSegment();
                aDefaultDir = aURL.GetFull();
            }
        }
    }
    if (!aDefaultDir.isEmpty())
        xFolderPicker->setDisplayDirectory(aDefaultDir);

    if (xFolderPicker->execute() != RET_OK)
        return; // cancelled: keep the dialog open so the user can retry

    const OUString aOutDirUrl = xFolderPicker->getDirectory();
    if (aOutDirUrl.isEmpty())
        return;

    const sw::writer2027web::WebPublishResult aResult
        = sw::writer2027web::PublishWeb(*mpDoc, *mpOptions, aOutDirUrl);
    if (!aResult.mbSuccess)
    {
        OUString aMsg = SwResId(STR_WRITER2027_PUBLISH_FAILED);
        aMsg = aMsg.replaceFirst(u"%1"_ustr,
                                 aResult.maError.isEmpty() ? OUString(u"unknown error"_ustr)
                                                           : aResult.maError);
        std::unique_ptr<weld::MessageDialog> xError(Application::CreateMessageDialog(
            m_xDialog.get(), VclMessageType::Error, VclButtonsType::Ok, aMsg));
        xError->run();
        return; // keep the dialog open so the user can retry
    }

    if (bOpenInBrowser)
        lcl_OpenInBrowser(aResult.maIndexUrl);

    mbPublished = true;
    m_xDialog->response(RET_OK);
}

bool Writer2027PublishDialog::RunPreflightGate()
{
    if (!mpDoc || !mpOptions)
        return true;

    sw::writer2027preflight::PreflightOptions aPf;
    aPf.meProfile = mpOptions->meFormat == sw::writer2027web::WebPublishFormat::SingleHtml
                        ? sw::writer2027preflight::PreflightProfile::SingleHtml
                        : sw::writer2027preflight::PreflightProfile::WebPackage;
    aPf.meMode = sw::writer2027preflight::PreflightScanMode::Full;
    const sw::writer2027preflight::PreflightReport aReport
        = sw::writer2027preflight::RunPreflight(*mpDoc, aPf);

    // Warnings never block; errors are overrideable.
    if (aReport.mnErrors == 0)
        return true;

    OUString aMsg = SwResId(STR_WRITER2027_PUBLISH_PREFLIGHT_GATE);
    aMsg = aMsg.replaceAll(u"$1"_ustr, OUString::number(aReport.mnErrors))
               .replaceAll(u"$2"_ustr, OUString::number(aReport.mnWarnings));
    std::unique_ptr<weld::MessageDialog> xDlg(Application::CreateMessageDialog(
        m_xDialog.get(), VclMessageType::Warning, VclButtonsType::NONE, aMsg));
    xDlg->add_button(SwResId(STR_WRITER2027_PUBLISH_PREFLIGHT_REVIEW), RET_YES);
    xDlg->add_button(SwResId(STR_WRITER2027_PUBLISH_PREFLIGHT_CONTINUE), RET_NO);
    const sal_Int32 nResult = xDlg->run();
    if (nResult == RET_YES)
    {
        // Open the Publication Inspector deck for review.
        if (SfxViewShell* pViewShell = SfxViewShell::Current())
        {
            if (sfx2::sidebar::SidebarController* pController
                = sfx2::sidebar::SidebarController::GetSidebarControllerForView(pViewShell))
            {
                pController->OpenThenSwitchToDeck(u"PublicationInspectorDeck"_ustr);
            }
        }
        return false;
    }
    return true;
}

} // namespace sw::writer2027publish

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */