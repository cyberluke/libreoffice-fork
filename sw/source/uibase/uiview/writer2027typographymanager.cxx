/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027typographymanager.hxx>
#include <writer2027typesystem.hxx>

#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027log.hxx>

#include <editeng/flstitem.hxx>
#include <svl/itemset.hxx>

#include <IDocumentStylePoolAccess.hxx>
#include <doc.hxx>
#include <docsh.hxx>
#include <poolfmt.hxx>
#include <format.hxx>

#include <comphelper/servicehelper.hxx>
#include <unotxdoc.hxx>

#include <com/sun/star/frame/Frame.hpp>
#include <com/sun/star/frame/XController.hpp>

#include <svl/itemset.hxx>
#include <svl/whichranges.hxx>

namespace sw::writer2027typographymanager
{

using svx::writer2027::TypeSystemFontRole;

namespace
{
// One canonical descriptor row per semantic style. mnPoolId is a
// SwPoolFormatId value (COLL_* / CHR_*); stored as int to keep the shared table
// free of the sw enum typedef. Font roles follow spec 22 (DefaultBody/Body ->
// Body, Heading* -> Heading, Title/Subtitle -> Display, Quote/Caption -> Body,
// CodeBlock/InlineCode -> Mono).
const std::vector<Writer2027SemanticStyleDescriptor>& lcl_Descriptors()
{
    using R = Writer2027SemanticStyle;
    static const std::vector<Writer2027SemanticStyleDescriptor> aTable{
        // role                         poolId                  char?  fontRole               scaleSlot
        { R::DefaultBody,  static_cast<int>(SwPoolFormatId::COLL_STANDARD), false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Body, u"Default Paragraph Style"_ustr },
        { R::Body,         static_cast<int>(SwPoolFormatId::COLL_TEXT),     false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Body, u"Text Body"_ustr },
        { R::Heading1,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE1), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading1, u"Heading 1"_ustr },
        { R::Heading2,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE2), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading2, u"Heading 2"_ustr },
        { R::Heading3,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE3), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading3, u"Heading 3"_ustr },
        { R::Heading4,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE4), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading4, u"Heading 4"_ustr },
        { R::Heading5,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE5), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading5, u"Heading 5"_ustr },
        { R::Heading6,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE6), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading6, u"Heading 6"_ustr },
        { R::Title,        static_cast<int>(SwPoolFormatId::COLL_DOC_TITLE), false, TypeSystemFontRole::Display, TypeSystemScaleSlot::Title, u"Title"_ustr },
        { R::Subtitle,     static_cast<int>(SwPoolFormatId::COLL_DOC_SUBTITLE), false, TypeSystemFontRole::Display, TypeSystemScaleSlot::Subtitle, u"Subtitle"_ustr },
        { R::Quote,        static_cast<int>(SwPoolFormatId::COLL_HTML_BLOCKQUOTE), false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Quote, u"Quote"_ustr },
        { R::Caption,      static_cast<int>(SwPoolFormatId::COLL_LABEL),     false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Caption, u"Caption"_ustr },
        { R::CodeBlock,    static_cast<int>(SwPoolFormatId::COLL_HTML_PRE),  false, TypeSystemFontRole::Mono, TypeSystemScaleSlot::Mono, u"Code"_ustr },
        { R::InlineCode,   static_cast<int>(SwPoolFormatId::CHR_HTML_CODE),  true,  TypeSystemFontRole::Mono, TypeSystemScaleSlot::Mono, u"Inline Code"_ustr },
    };
    return aTable;
}
} // namespace

const std::vector<Writer2027SemanticStyleDescriptor>&
GetWriter2027SemanticStyleDescriptors()
{
    return lcl_Descriptors();
}

const Writer2027SemanticStyleDescriptor*
GetWriter2027SemanticStyleDescriptor(Writer2027SemanticStyle eRole)
{
    for (const auto& rDesc : lcl_Descriptors())
        if (rDesc.meRole == eRole)
            return &rDesc;
    return nullptr;
}

Writer2027DocumentTypographyContext
ResolveWriter2027TypographyContext(const css::uno::Reference<css::frame::XFrame>& rFrame)
{
    Writer2027DocumentTypographyContext aContext;
    aContext.xFrame = rFrame;
    if (!rFrame.is())
        return aContext;

    css::uno::Reference<css::frame::XController> xController = rFrame->getController();
    if (!xController.is())
        return aContext;

    SwXTextDocument* pTextDoc
        = comphelper::getFromUnoTunnel<SwXTextDocument>(xController->getModel());
    if (!pTextDoc || !pTextDoc->GetDocShell())
        return aContext;

    aContext.pDocShell = pTextDoc->GetDocShell();
    aContext.pDoc = aContext.pDocShell->GetDoc();

    // The canonical Writer FontList lives on the doc shell's item set
    // (SID_ATTR_CHAR_FONTLIST -> SvxFontListItem::GetFontList), exactly as the
    // rest of Writer acquires it (spec 8). We never invent a second font
    // discovery system.
    if (aContext.pDocShell)
    {
        if (const SvxFontListItem* pFontListItem
            = aContext.pDocShell->GetItem(SID_ATTR_CHAR_FONTLIST))
            aContext.pFontList = pFontListItem->GetFontList();
    }
    return aContext;
}

bool EnsureSemanticStylesMaterialized(SwDoc& rDoc)
{
    // Materialize every style the canonical semantic map promises, so each
    // exists and can be previewed/applied/updated (spec 20).
    for (const auto& rDesc : lcl_Descriptors())
    {
        const auto ePoolId = static_cast<SwPoolFormatId>(rDesc.mnPoolId);
        if (rDesc.mbCharacterStyle)
        {
            if (!rDoc.getIDocumentStylePoolAccess().GetCharFormatFromPool(ePoolId))
            {
                svx::writer2027::Writer2027LogMessage(
                    "typographymanager.materialize",
                    OUString::Concat(u"char format missing poolId=")
                        + OUString::number(rDesc.mnPoolId));
                return false;
            }
        }
        else
        {
            if (!rDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(ePoolId))
            {
                svx::writer2027::Writer2027LogMessage(
                    "typographymanager.materialize",
                    OUString::Concat(u"text coll missing poolId=")
                        + OUString::number(rDesc.mnPoolId));
                return false;
            }
        }
    }
    return true;
}

} // namespace sw::writer2027typographymanager

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */