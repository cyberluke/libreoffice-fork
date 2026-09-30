/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <officelabs/RewriteDialog.hxx>
#include <officelabs/RewriteProtocol.hxx>

#include <com/sun/star/text/XTextDocument.hpp>
#include <sal/log.hxx>
#include <tools/link.hxx>
#include <vcl/svapp.hxx>

#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace officelabs {

namespace {

void runOnVclThread(void* pData, void*)
{
    std::unique_ptr<std::function<void()>> pFn(static_cast<std::function<void()>*>(pData));
    (*pFn)();
}

} // namespace

RewriteDialog::RewriteDialog(weld::Window* pParent,
                             const css::uno::Reference<css::frame::XModel>& xModel)
    : GenericDialogController(pParent, u"officelabs/ui/rewrite.ui"_ustr,
                              u"RewriteDialog"_ustr)
    , m_xStyleBox(m_xBuilder->weld_combo_box(u"stylebox"_ustr))
    , m_xOriginalText(m_xBuilder->weld_text_view(u"originaltext"_ustr))
    , m_xSuggestionText(m_xBuilder->weld_text_view(u"suggestiontext"_ustr))
    , m_xStatusLabel(m_xBuilder->weld_label(u"statuslabel"_ustr))
    , m_xGenerateBtn(m_xBuilder->weld_button(u"generate"_ustr))
    , m_xReplaceBtn(m_xBuilder->weld_button(u"replace"_ustr))
{
    // Wide enough to fit the button row; the two text panes split the rest.
    m_xDialog->set_size_request(660, 500);

    m_aDoc.setModel(xModel);
    if (xModel.is())
    {
        m_aDoc.setController(xModel->getCurrentController());
        css::uno::Reference<css::text::XTextDocument> xTextDoc(xModel, css::uno::UNO_QUERY);
        if (xTextDoc.is())
            m_aDoc.setDocument(xTextDoc);
    }

    // Pre-fill from the current selection; without one the dialog is inert.
    const OUString sSelection = m_aDoc.getSelectedText();
    m_xOriginalText->set_text(sSelection);
    m_xGenerateBtn->set_sensitive(!sSelection.isEmpty());
    m_xReplaceBtn->set_sensitive(false);

    // Default style: Improve clarity.
    m_xStyleBox->set_active_id(u"clarity"_ustr);

    m_xGenerateBtn->connect_clicked(LINK(this, RewriteDialog, GenerateHdl));
    m_xReplaceBtn->connect_clicked(LINK(this, RewriteDialog, ReplaceHdl));

    m_pShared = std::make_shared<Shared>();
    m_pShared->pOwner = this;
}

RewriteDialog::~RewriteDialog()
{
    // The fetch thread may still be running; its posted callback must find no
    // owner. The fetch itself is discarded.
    if (m_pShared)
        m_pShared->pOwner = nullptr;
}

void RewriteDialog::setBusy(bool bBusy)
{
    m_bBusy = bBusy;
    m_xGenerateBtn->set_sensitive(!bBusy && !m_xOriginalText->get_text().isEmpty());
    m_xReplaceBtn->set_sensitive(!bBusy && m_bHasSuggestion);
}

IMPL_LINK(RewriteDialog, GenerateHdl, weld::Button&, rButton, void)
{
    if (m_bBusy)
        return;

    const OUString sStyle = m_xStyleBox->get_active_id();
    const OUString sText = m_xOriginalText->get_text();
    if (sText.isEmpty())
        return;

    setBusy(true);
    m_xStatusLabel->set_label(u"Generating\u2026"_ustr);

    auto pShared = m_pShared;
    std::thread([pShared, sText, sStyle]() {
        const AgentResponse aResp = fetchRewrite(sText, sStyle);
        auto* pFn = new std::function<void()>([pShared, aResp]() {
            if (pShared->pOwner)
                pShared->pOwner->onFetchDone(aResp);
        });
        Application::PostUserEvent(LINK_NONMEMBER(pFn, runOnVclThread));
    }).detach();
}

// The agent reply lands on the VCL thread; the dialog is guaranteed alive
// because the Shared slot is only nulled by the destructor, which runs after
// run() returns -- and the modal loop is still active while this runs.
void RewriteDialog::onFetchDone(const AgentResponse& rResp)
{
    setBusy(false);

    if (rResp.nStatus != 200)
    {
        SAL_WARN("officelabs.rewrite", "rewrite agent request failed with " << rResp.nStatus);
        m_xStatusLabel->set_label(u"AI request failed"_ustr);
        return;
    }

    const OUString sSuggestion = parseRewriteSuggestion(rResp.aBody);
    if (sSuggestion.isEmpty())
    {
        SAL_WARN("officelabs.rewrite", "rewrite agent returned no suggestion");
        m_xStatusLabel->set_label(u"AI returned no suggestion"_ustr);
        return;
    }

    m_xSuggestionText->set_text(sSuggestion);
    m_bHasSuggestion = true;
    m_xReplaceBtn->set_sensitive(true);
    m_xStatusLabel->set_label(OUString());
}

IMPL_LINK(RewriteDialog, ReplaceHdl, weld::Button&, rButton, void)
{
    const OUString sSuggestion = m_xSuggestionText->get_text();
    if (sSuggestion.isEmpty())
        return;

    if (m_aDoc.replaceSelection(sSuggestion))
        m_xDialog->response(RET_OK);
    else
        m_xStatusLabel->set_label(u"Could not replace the selection"_ustr);
}

} // namespace officelabs

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */