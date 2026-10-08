/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_WRITER2027PUBLISHDIALOG_HXX
#define INCLUDED_SW_WRITER2027PUBLISHDIALOG_HXX

#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Button.hxx>
#include <vcl/weld/CheckButton.hxx>
#include <vcl/weld/Dialog.hxx>
#include <vcl/weld/RadioButton.hxx>

#include <memory>

class SwDoc;

namespace sw::writer2027web
{
struct WebPublishOptions;
}

namespace sw::writer2027publish
{

/** Writer 2027 "Publish as Web" dialog (Phase 8 + 9).

    Output format (Web Package / Single HTML) plus options: responsive
    layout, document metadata, print stylesheet, generated navigation,
    image optimization, and permitted-font embedding. Two actions: Publish,
    or Publish and open in the system browser. The export itself is
    read-only with respect to the Writer document. */
class Writer2027PublishDialog
{
public:
    /** Runs the dialog for rDoc. Returns true when the user published
        successfully; rOptions receives the chosen settings. */
    bool Run(SwDoc& rDoc, sw::writer2027web::WebPublishOptions& rOptions);

private:
    DECL_LINK(PublishHdl, weld::Button&, void);
    DECL_LINK(PublishOpenHdl, weld::Button&, void);

    void Publish(bool bOpenInBrowser);

    /** Full preflight gate: returns false when the user chose to review
        issues instead of publishing. Warnings never block; errors are
        overrideable. */
    bool RunPreflightGate();

    std::unique_ptr<weld::Builder> m_xBuilder;
    std::unique_ptr<weld::Dialog> m_xDialog;
    std::unique_ptr<weld::RadioButton> m_xWebPackage;
    std::unique_ptr<weld::RadioButton> m_xSingleHtml;
    std::unique_ptr<weld::CheckButton> m_xResponsive;
    std::unique_ptr<weld::CheckButton> m_xMetadata;
    std::unique_ptr<weld::CheckButton> m_xPrintCss;
    std::unique_ptr<weld::CheckButton> m_xGenerateNav;
    std::unique_ptr<weld::CheckButton> m_xOptimizeImages;
    std::unique_ptr<weld::CheckButton> m_xEmbedFonts;
    std::unique_ptr<weld::Button> m_xPublish;
    std::unique_ptr<weld::Button> m_xPublishOpen;

    SwDoc* mpDoc = nullptr;
    sw::writer2027web::WebPublishOptions* mpOptions = nullptr;
    bool mbPublished = false;
};

} // namespace sw::writer2027publish

#endif // INCLUDED_SW_WRITER2027PUBLISHDIALOG_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */