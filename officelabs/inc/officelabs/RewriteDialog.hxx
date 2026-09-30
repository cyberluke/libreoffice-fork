/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_OFFICELABS_REWRITEDIALOG_HXX
#define INCLUDED_OFFICELABS_REWRITEDIALOG_HXX

#include <officelabs/officelabsdllapi.h>
#include <officelabs/DocumentController.hxx>

#include <com/sun/star/frame/XModel.hpp>
#include <tools/link.hxx>
// LOCAL (V271): upstream split vcl/weld.hxx into per-class headers
// (include/vcl/weld/*.hxx) -- include exactly the classes this dialog uses.
#include <vcl/weld/Button.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/ComboBox.hxx>
#include <vcl/weld/Dialog.hxx>
#include <vcl/weld/DialogController.hxx>
#include <vcl/weld/Label.hxx>
#include <vcl/weld/TextView.hxx>
#include <vcl/weld/Window.hxx>
#include <vcl/vclenum.hxx>

#include <memory>

namespace officelabs {

struct AgentResponse;

/// Native "Rewrite with AI" dialog (Writer). The suggestion can be applied
/// directly as an undoable replacement of the selection, or edited in place
/// first. Ghost-text preview is deliberately NOT offered here: the ghost
/// window must be a child of the document edit window, which a modal dialog
/// does not own -- use the panel's rewriteApply "ghost" mode instead.
///
/// Ported from the abandoned gerrit change 194671 (same dialog shape,
/// UNO-only document access, agent /completions/ rewrite mode).
class OFFICELABS_DLLPUBLIC RewriteDialog final : public weld::GenericDialogController
{
public:
    RewriteDialog(weld::Window* pParent,
                  const css::uno::Reference<css::frame::XModel>& xModel);
    virtual ~RewriteDialog() override;

private:
    DECL_LINK(GenerateHdl, weld::Button&, void);
    DECL_LINK(ReplaceHdl, weld::Button&, void);

    /// Agent reply handler, run on the VCL thread by the posted callback.
    void onFetchDone(const AgentResponse& rResp);

    void setBusy(bool bBusy);

    DocumentController m_aDoc;

    std::unique_ptr<weld::ComboBox> m_xStyleBox;
    std::unique_ptr<weld::TextView> m_xOriginalText;
    std::unique_ptr<weld::TextView> m_xSuggestionText;
    std::unique_ptr<weld::Label> m_xStatusLabel;
    std::unique_ptr<weld::Button> m_xGenerateBtn;
    std::unique_ptr<weld::Button> m_xReplaceBtn;

    // Keeps the async fetch callback from touching a destroyed dialog: the
    // worker thread captures the shared slot, the posted VCL callback checks
    // it, the dialog nulls it in the destructor.
    struct Shared
    {
        RewriteDialog* pOwner = nullptr;
    };
    std::shared_ptr<Shared> m_pShared;

    bool m_bHasSuggestion = false;
    bool m_bBusy = false;
};

} // namespace officelabs

#endif // INCLUDED_OFFICELABS_REWRITEDIALOG_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */