/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027preflight.hxx>
#include <writer2027capabilities.hxx>

#include <editeng/brushitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/langitem.hxx>
#include <i18nlangtag/lang.h>
#include <svx/writer2027typesystem.hxx>
#include <svtools/ctrltool.hxx>

#include <IDocumentMarkAccess.hxx>
#include <IMark.hxx>
#include <charatr.hxx>
#include <charformats.hxx>
#include <doc.hxx>
#include <docary.hxx>
#include <docsh.hxx>
#include <docufld.hxx>
#include <fldbas.hxx>
#include <fmtanchr.hxx>
#include <fmtflcnt.hxx>
#include <fmtfld.hxx>
#include <fmtfsize.hxx>
#include <fmtinfmt.hxx>
#include <fmtrfmrk.hxx>
#include <frameformats.hxx>
#include <frmfmt.hxx>
#include <hintids.hxx>
#include <ndarr.hxx>
#include <ndgrf.hxx>
#include <ndhints.hxx>
#include <ndtxt.hxx>
#include <node.hxx>
#include <pagedesc.hxx>
#include <reffld.hxx>
#include <section.hxx>
#include <strings.hrc>
#include <swtypes.hxx>
#include <tox.hxx>
#include <txatbase.hxx>
#include <writer2027typesystem.hxx>

#include <osl/diagnose.h>
#include <rtl/math.hxx>
#include <rtl/ustrbuf.hxx>
#include <tools/color.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace sw::writer2027preflight;

namespace
{
// ---------------------------------------------------------------------------
// JSON escaping (deterministic report serialization)
// ---------------------------------------------------------------------------

OUString lcl_JsonEscape(const OUString& rText)
{
    OUStringBuffer aBuf;
    aBuf.append('"');
    for (sal_Int32 i = 0; i < rText.getLength(); ++i)
    {
        const sal_Unicode c = rText[i];
        switch (c)
        {
            case '"':
                aBuf.append(u"\\\""_ustr);
                break;
            case '\\':
                aBuf.append(u"\\\\"_ustr);
                break;
            case '\n':
                aBuf.append(u"\\n"_ustr);
                break;
            case '\r':
                aBuf.append(u"\\r"_ustr);
                break;
            case '\t':
                aBuf.append(u"\\t"_ustr);
                break;
            default:
                if (c < 0x20)
                {
                    aBuf.append(u"\\u"_ustr);
                    OUString aHex = OUString::number(c, 16);
                    while (aHex.getLength() < 4)
                        aHex = u"0"_ustr + aHex;
                    aBuf.append(aHex);
                }
                else
                    aBuf.append(c);
                break;
        }
    }
    aBuf.append('"');
    return aBuf.makeStringAndClear();
}

// ---------------------------------------------------------------------------
// WCAG relative-luminance helpers (numerical diagnostic only, never a claim)
// ---------------------------------------------------------------------------

double lcl_Channel(double v)
{
    v /= 255.0;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double lcl_Luminance(const Color& rColor)
{
    return 0.2126 * lcl_Channel(rColor.GetRed()) + 0.7152 * lcl_Channel(rColor.GetGreen())
           + 0.0722 * lcl_Channel(rColor.GetBlue());
}

double lcl_ContrastRatio(const Color& rA, const Color& rB)
{
    const double l1 = lcl_Luminance(rA);
    const double l2 = lcl_Luminance(rB);
    const double hi = std::max(l1, l2);
    const double lo = std::min(l1, l2);
    return (hi + 0.05) / (lo + 0.05);
}

// ---------------------------------------------------------------------------
// The scanner
// ---------------------------------------------------------------------------

class PreflightScanner
{
public:
    PreflightScanner(SwDoc& rDoc, const PreflightOptions& rOptions)
        : mrDoc(rDoc)
        , maOptions(rOptions)
    {
    }

    PreflightReport Run();

private:
    bool IsWeb() const
    {
        return maOptions.meProfile == PreflightProfile::WebPackage
               || maOptions.meProfile == PreflightProfile::SingleHtml;
    }
    bool IsPrintOrPdf() const
    {
        return maOptions.meProfile == PreflightProfile::PdfDigital
               || maOptions.meProfile == PreflightProfile::PrintFriendly
               || maOptions.meProfile == PreflightProfile::PrintDigital;
    }
    bool IsFull() const { return maOptions.meMode == PreflightScanMode::Full; }

    void AddIssue(OUString aCheckId, PreflightSeverity eSev, PreflightCategory eCat,
                  const OUString& rTitle, const OUString& rDescription, OUString aTargetId,
                  PreflightTargetKind eKind, sal_Int64 nNode = -1, OUString aObjName = {},
                  sal_Int32 nStart = -1, sal_Int32 nEnd = -1,
                  PreflightFixKind eFix = PreflightFixKind::None, bool bNavigable = false);

    // sub-scans
    void ScanGeneral();
    void ScanHeadingsAndLinks(); // headings + text-hint links/fields/bookmarks
    void ScanMedia();            // frames (alt, OLE, SVG, format, PPI) + tables
    void ScanFonts();
    void ScanTypeSystem();
    void ScanContrast();
    void ScanWebSpecific();

    SwDoc& mrDoc;
    const PreflightOptions& maOptions;
    PreflightReport maReport;

    // deterministic target-id counters
    sal_Int32 mnHeadingSeq = 0;
    sal_Int32 mnImageSeq = 0;
    sal_Int32 mnTableSeq = 0;
    sal_Int32 mnFrameSeq = 0;

    // collected state
    std::set<OUString> maBookmarkNames;              // duplicate detection + resolution
    std::map<OUString, sal_Int64> maHeadingTargets;  // heading text (lower) -> node index
    sal_Int32 mnPaperFields = 0;
    bool mbHasToxSection = false;
};

void PreflightScanner::AddIssue(OUString aCheckId, PreflightSeverity eSev, PreflightCategory eCat,
                                const OUString& rTitle, const OUString& rDescription,
                                OUString aTargetId, PreflightTargetKind eKind, sal_Int64 nNode,
                                OUString aObjName, sal_Int32 nStart, sal_Int32 nEnd,
                                PreflightFixKind eFix, bool bNavigable)
{
    PreflightIssue aIssue;
    aIssue.maCheckId = std::move(aCheckId);
    aIssue.meSeverity = eSev;
    aIssue.meCategory = eCat;
    aIssue.maTitle = rTitle;
    aIssue.maDescription = rDescription;
    aIssue.maTargetId = std::move(aTargetId);
    aIssue.meTargetKind = eKind;
    aIssue.mnNodeIndex = nNode;
    aIssue.maObjectName = std::move(aObjName);
    aIssue.mnStart = nStart;
    aIssue.mnEnd = nEnd;
    aIssue.meFixKind = eFix;
    // An issue with a concrete target is navigable by definition.
    aIssue.mbNavigable = bNavigable || nNode >= 0 || !aObjName.isEmpty();
    switch (eSev)
    {
        case PreflightSeverity::Error:
            ++maReport.mnErrors;
            break;
        case PreflightSeverity::Warning:
            ++maReport.mnWarnings;
            break;
        case PreflightSeverity::Info:
            ++maReport.mnInfos;
            break;
    }
    maReport.maIssues.push_back(std::move(aIssue));
}

void PreflightScanner::ScanGeneral()
{
    // Empty document.
    bool bHasContent = false;
    const SwNodes& rNodes = mrDoc.GetNodes();
    for (SwNodeOffset n = SwNodeOffset(1); n < rNodes.Count() - 1 && !bHasContent; ++n)
    {
        const SwTextNode* pText = rNodes[n]->GetTextNode();
        if (pText && !pText->GetText().trim().isEmpty())
            bHasContent = true;
        else if (rNodes[n]->IsGrfNode() || rNodes[n]->IsTableNode())
            bHasContent = true;
    }
    if (!bHasContent)
    {
        AddIssue(u"W27-DOC-001"_ustr, PreflightSeverity::Info, PreflightCategory::Document,
                 SwResId(STR_WRITER2027_PF_EMPTY_DOCUMENT_TITLE),
                 SwResId(STR_WRITER2027_PF_EMPTY_DOCUMENT_DESC), u"document"_ustr,
                 PreflightTargetKind::Document);
    }

    // Document title metadata.
    if (SwDocShell* pShell = mrDoc.GetDocShell())
    {
        if (pShell->GetTitle().trim().isEmpty())
        {
            AddIssue(u"W27-DOC-002"_ustr, PreflightSeverity::Info, PreflightCategory::Document,
                     SwResId(STR_WRITER2027_PF_NO_TITLE_TITLE),
                     SwResId(STR_WRITER2027_PF_NO_TITLE_DESC), u"document"_ustr,
                     PreflightTargetKind::Document, -1, OUString(), -1, -1,
                     PreflightFixKind::OpenDocumentLanguage);
        }
    }

    // Document language (accessibility + PDF metadata).
    if (const SwTextFormatColls* pColls = mrDoc.GetTextFormatColls())
    {
        for (const SwTextFormatColl* pColl : *pColls)
        {
            if (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_STANDARD)
            {
                if (const SvxLanguageItem* pLang = pColl->GetItemIfSet(RES_CHRATR_LANGUAGE, false))
                {
                    if (pLang->GetLanguage() == LANGUAGE_NONE)
                    {
                        AddIssue(u"W27-A11Y-003"_ustr, PreflightSeverity::Warning,
                                 PreflightCategory::Accessibility,
                                 SwResId(STR_WRITER2027_PF_NO_LANGUAGE_TITLE),
                                 SwResId(STR_WRITER2027_PF_NO_LANGUAGE_DESC), u"document"_ustr,
                                 PreflightTargetKind::Document, -1, OUString(), -1, -1,
                                 PreflightFixKind::OpenDocumentLanguage);
                    }
                }
                break;
            }
        }
    }
}

void PreflightScanner::ScanHeadingsAndLinks()
{
    const SwNodes& rNodes = mrDoc.GetNodes();
    int nPrevOutline = 0;
    for (SwNodeOffset n = SwNodeOffset(1); n < rNodes.Count() - 1; ++n)
    {
        const SwTextNode* pText = rNodes[n]->GetTextNode();
        if (!pText)
        {
            // TOC / index sections.
            if (rNodes[n]->IsSectionNode())
            {
                const SwSectionNode* pSec = rNodes[n]->GetSectionNode();
                if (pSec->GetSection().GetType() == SectionType::ToxContent)
                    mbHasToxSection = true;
            }
            continue;
        }

        const OUString aText = pText->GetText();
        const int nOutline = pText->GetAttrOutlineLevel();

        if (nOutline > 0 && nOutline <= 10)
        {
            ++mnHeadingSeq;
            const OUString aTargetId = u"heading-"_ustr + OUString::number(mnHeadingSeq);

            if (aText.trim().isEmpty())
            {
                AddIssue(u"W27-DOC-003"_ustr, PreflightSeverity::Warning,
                         PreflightCategory::Document, SwResId(STR_WRITER2027_PF_EMPTY_HEADING_TITLE),
                         SwResId(STR_WRITER2027_PF_EMPTY_HEADING_DESC), aTargetId,
                         PreflightTargetKind::Heading, static_cast<sal_Int64>(n.get()), OUString(), 0,
                         aText.getLength());
            }
            else
            {
                // Register the heading as a resolvable reference target.
                const OUString aKey = aText.toAsciiLowerCase().trim();
                if (!aKey.isEmpty())
                    maHeadingTargets.emplace(aKey, static_cast<sal_Int64>(n.get()));

                // Hierarchy gaps (real outline levels only).
                if (nPrevOutline > 0 && nOutline > nPrevOutline + 1)
                {
                    AddIssue(
                        u"W27-A11Y-002"_ustr, PreflightSeverity::Warning,
                        PreflightCategory::Accessibility,
                        SwResId(STR_WRITER2027_PF_HEADING_GAP_TITLE),
                        SwResId(STR_WRITER2027_PF_HEADING_GAP_DESC).replaceAll(
                            u"$1"_ustr, OUString::number(nPrevOutline))
                            .replaceAll(u"$2"_ustr, OUString::number(nOutline)),
                        aTargetId, PreflightTargetKind::Heading, static_cast<sal_Int64>(n.get()),
                        OUString(), 0, aText.getLength());
                }
            }
            nPrevOutline = nOutline;
            continue;
        }

        // Text-hint scanning: hyperlinks, cross-reference fields, refmarks.
        const SwpHints* pHints = pText->GetpSwpHints();
        if (!pHints)
            continue;
        for (size_t i = 0; i < pHints->Count(); ++i)
        {
            const SwTextAttr* pH = pHints->Get(i);
            const sal_Int32 nStart = pH->GetStart();
            const sal_Int32 nEnd = pH->GetEnd() ? *pH->GetEnd() : nStart + 1;
            switch (pH->Which())
            {
                case RES_TXTATR_INETFMT:
                {
                    const OUString aURL = pH->GetINetFormat().GetValue();
                    if (aURL.startsWith("#"))
                    {
                        // Internal anchor: must resolve to a heading or bookmark.
                        OUString aName = aURL.copy(1).trim();
                        const OUString aKey = aName.toAsciiLowerCase();
                        const bool bResolved
                            = maHeadingTargets.count(aKey)
                              || (maBookmarkNames.count(aName) || maBookmarkNames.count(aKey));
                        if (!bResolved)
                        {
                            AddIssue(
                                u"W27-REF-001"_ustr, PreflightSeverity::Error,
                                PreflightCategory::LinksReferences,
                                SwResId(STR_WRITER2027_PF_BROKEN_REF_TITLE),
                                SwResId(STR_WRITER2027_PF_BROKEN_REF_DESC).replaceAll(
                                    u"$1"_ustr, aName),
                                u"para-"_ustr + OUString::number(static_cast<sal_Int64>(n.get())),
                                PreflightTargetKind::TextPosition, static_cast<sal_Int64>(n.get()),
                                OUString(), nStart, nEnd);
                        }
                    }
                    else
                    {
                        const OUString aSafe = sw::writer2027capabilities::SafeHref(aURL);
                        if (aSafe.isEmpty() && !aURL.isEmpty())
                        {
                            AddIssue(
                                u"W27-LINK-001"_ustr, PreflightSeverity::Error,
                                PreflightCategory::LinksReferences,
                                SwResId(STR_WRITER2027_PF_UNSAFE_URL_TITLE),
                                SwResId(STR_WRITER2027_PF_UNSAFE_URL_DESC),
                                u"para-"_ustr + OUString::number(static_cast<sal_Int64>(n.get())),
                                PreflightTargetKind::TextPosition, static_cast<sal_Int64>(n.get()),
                                OUString(), nStart, nEnd);
                        }
                    }
                    // Link text quality (empty or URL-only visible text).
                    const OUString aLabel = aText.copy(nStart, std::max(nEnd - nStart, sal_Int32(1)))
                                                .trim();
                    if (aLabel.isEmpty() || aLabel.equalsIgnoreAsciiCase(aURL))
                    {
                        AddIssue(u"W27-A11Y-004"_ustr, PreflightSeverity::Warning,
                                 PreflightCategory::Accessibility,
                                 SwResId(STR_WRITER2027_PF_LINK_TEXT_TITLE),
                                 SwResId(STR_WRITER2027_PF_LINK_TEXT_DESC),
                                 u"para-"_ustr + OUString::number(static_cast<sal_Int64>(n.get())),
                                 PreflightTargetKind::TextPosition, static_cast<sal_Int64>(n.get()),
                                 OUString(), nStart, nEnd);
                    }
                }
                break;
                case RES_TXTATR_FIELD:
                {
                    const SwField* pField = pH->GetFormatField().GetField();
                    if (!pField)
                        break;
                    switch (pField->Which())
                    {
                        case SwFieldIds::GetRef:
                        {
                            const auto* pRef = dynamic_cast<const SwGetRefField*>(pField);
                            if (pRef)
                            {
                                const OUString aName = pRef->GetSetRefName().toString();
                                const OUString aKey = aName.toAsciiLowerCase();
                                const bool bResolved
                                    = maHeadingTargets.count(aKey)
                                      || maBookmarkNames.count(aName)
                                      || maBookmarkNames.count(aKey);
                                if (!bResolved)
                                {
                                    AddIssue(
                                        u"W27-REF-001"_ustr, PreflightSeverity::Error,
                                        PreflightCategory::LinksReferences,
                                        SwResId(STR_WRITER2027_PF_BROKEN_REF_TITLE),
                                        SwResId(STR_WRITER2027_PF_BROKEN_REF_DESC).replaceAll(
                                            u"$1"_ustr, aName),
                                        u"para-"_ustr
                                            + OUString::number(static_cast<sal_Int64>(n.get())),
                                        PreflightTargetKind::TextPosition,
                                        static_cast<sal_Int64>(n.get()), OUString(), nStart, nEnd);
                                }
                            }
                        }
                        break;
                        case SwFieldIds::PageNumber:
                        case SwFieldIds::RefPageGet:
                        case SwFieldIds::RefPageSet:
                            ++mnPaperFields;
                            break;
                        case SwFieldIds::DocStat:
                            if (const auto* pStat = dynamic_cast<const SwDocStatField*>(pField))
                            {
                                const SwDocStatSubType eSub = pStat->GetSubType();
                                if (eSub == SwDocStatSubType::Page
                                    || eSub == SwDocStatSubType::PageRange)
                                    ++mnPaperFields;
                            }
                            break;
                        default:
                            break;
                    }
                }
                break;
                case RES_TXTATR_REFMARK:
                {
                    const OUString aName = pH->GetRefMark().GetRefName().toString();
                    if (aName.isEmpty())
                        break;
                    if (!maBookmarkNames.insert(aName).second)
                    {
                        // Duplicate bookmark name.
                        AddIssue(u"W27-STRUCT-001"_ustr, PreflightSeverity::Error,
                                 PreflightCategory::Document,
                                 SwResId(STR_WRITER2027_PF_DUP_BOOKMARK_TITLE),
                                 SwResId(STR_WRITER2027_PF_DUP_BOOKMARK_DESC).replaceAll(
                                     u"$1"_ustr, aName),
                                 u"bookmark-"_ustr + aName,
                                 PreflightTargetKind::TextPosition, static_cast<sal_Int64>(n.get()),
                                 OUString(), nStart, nEnd);
                    }
                }
                break;
                default:
                    break;
            }
        }
    }

    // Bookmark names from the document mark model (includes heading and
    // numbered-item cross-reference bookmarks).
    if (auto* pMarks = mrDoc.getIDocumentMarkAccess())
    {
        for (auto it = pMarks->getAllMarksBegin(); it != pMarks->getAllMarksEnd(); ++it)
        {
            sw::mark::MarkBase* pMark = *it;
            if (!pMark)
                continue;
            const OUString aName = pMark->GetName().toString();
            if (aName.isEmpty())
                continue;
            if (!maBookmarkNames.insert(aName).second)
            {
                AddIssue(u"W27-STRUCT-001"_ustr, PreflightSeverity::Error,
                         PreflightCategory::Document,
                         SwResId(STR_WRITER2027_PF_DUP_BOOKMARK_TITLE),
                         SwResId(STR_WRITER2027_PF_DUP_BOOKMARK_DESC).replaceAll(u"$1"_ustr,
                                                                                 aName),
                         u"bookmark-"_ustr + aName, PreflightTargetKind::TextPosition);
            }
        }
    }
}

void PreflightScanner::ScanMedia()
{
    const SwNodes& rNodes = mrDoc.GetNodes();

    // ---- Frames (anchored objects): alt text, OLE, SVG, formats, PPI ----
    const sw::FrameFormats<sw::SpzFrameFormat*>* pFormats = mrDoc.GetSpzFrameFormats();
    if (pFormats)
    {
        for (sw::SpzFrameFormat* pFly : *pFormats)
        {
            if (!pFly)
                continue;
            const OUString aFlyName = pFly->GetName().toString();
            const SwFormatAnchor& rAnchor = pFly->GetAnchor();
            const RndStdIds eAnchor = rAnchor.GetAnchorId();

            const bool bImageLike = eAnchor == RndStdIds::FLY_AS_CHAR
                                    || eAnchor == RndStdIds::FLY_AT_CHAR
                                    || eAnchor == RndStdIds::FLY_AT_PARA;

            // Alt text (explicit description only; never guessed).
            const SwFlyFrameFormat* pFlyFmt = dynamic_cast<const SwFlyFrameFormat*>(pFly);
            const OUString aDesc = pFlyFmt ? pFlyFmt->GetObjDescription() : OUString();
            if (aDesc.trim().isEmpty() && (!pFlyFmt || !pFlyFmt->IsDecorative()))
            {
                ++mnImageSeq;
                const OUString aTargetId = bImageLike ? u"image-"_ustr
                                                           + OUString::number(mnImageSeq)
                                                      : u"frame-"_ustr
                                                            + OUString::number(++mnFrameSeq);
                AddIssue(u"W27-A11Y-001"_ustr, PreflightSeverity::Warning,
                         PreflightCategory::Accessibility,
                         SwResId(STR_WRITER2027_PF_NO_ALT_TITLE),
                         SwResId(STR_WRITER2027_PF_NO_ALT_DESC), aTargetId,
                         bImageLike ? PreflightTargetKind::Image : PreflightTargetKind::Frame,
                         -1, aFlyName, -1, -1, PreflightFixKind::OpenImageProperties, true);
            }

            // Page-anchored frame without a logical anchor node (Web).
            if (eAnchor == RndStdIds::FLY_AT_PAGE && !rAnchor.GetAnchorNode() && IsWeb())
            {
                AddIssue(u"W27-MEDIA-007"_ustr, PreflightSeverity::Warning,
                         PreflightCategory::Media,
                         SwResId(STR_WRITER2027_PF_PAGE_ANCHOR_TITLE),
                         SwResId(STR_WRITER2027_PF_PAGE_ANCHOR_DESC),
                         u"frame-"_ustr + OUString::number(++mnFrameSeq),
                         PreflightTargetKind::Frame, -1, aFlyName);
            }

            // Content walk: graphics, OLE, SVG, formats, PPI.
            const SwFormatContent& rContent = pFly->GetContent();
            const SwNodeIndex* pIdx = rContent.GetContentIdx();
            if (!pIdx)
                continue;
            const SwNodeOffset nEnd = pIdx->GetNode().EndOfSectionIndex();
            for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
            {
                const SwNode& rNode = *pIdx->GetNodes()[n];
                if (const SwGrfNode* pGrf = rNode.GetGrfNode())
                {
                    const Graphic& rGraphic = pGrf->GetGrfObj().GetGraphic();
                    if (rGraphic.GetType() == GraphicType::NONE)
                    {
                        AddIssue(u"W27-MEDIA-001"_ustr, PreflightSeverity::Warning,
                                 PreflightCategory::Media,
                                 SwResId(STR_WRITER2027_PF_BROKEN_GRAPHIC_TITLE),
                                 SwResId(STR_WRITER2027_PF_BROKEN_GRAPHIC_DESC),
                                 u"frame-"_ustr + OUString::number(++mnFrameSeq),
                                 PreflightTargetKind::Frame, -1, aFlyName);
                        continue;
                    }

                    // Web-native format + SVG safety (shared capability model).
                    if (IsWeb())
                    {
                        const GfxLink& rLink = rGraphic.GetGfxLink();
                        OUString aExt, aMime;
                        if (rLink.IsNative()
                            && !sw::writer2027capabilities::GfxTypeToWeb(rLink.GetType(), aExt,
                                                                         aMime))
                        {
                            AddIssue(u"W27-MEDIA-004"_ustr, PreflightSeverity::Info,
                                     PreflightCategory::Media,
                                     SwResId(STR_WRITER2027_PF_FORMAT_CONVERTED_TITLE),
                                     SwResId(STR_WRITER2027_PF_FORMAT_CONVERTED_DESC),
                                     u"frame-"_ustr + OUString::number(++mnFrameSeq),
                                     PreflightTargetKind::Frame, -1, aFlyName);
                        }
                        else if (rLink.IsNative() && aExt == u".svg"_ustr)
                        {
                            const sal_uInt8* pData = rLink.GetData();
                            const sal_uInt32 nSize = rLink.GetDataSize();
                            if (pData && nSize > 0)
                            {
                                std::vector<sal_uInt8> aBytes(pData, pData + nSize);
                                if (!sw::writer2027capabilities::SvgIsSafe(aBytes))
                                {
                                    AddIssue(u"W27-MEDIA-005"_ustr, PreflightSeverity::Warning,
                                             PreflightCategory::Media,
                                             SwResId(STR_WRITER2027_PF_UNSAFE_SVG_TITLE),
                                             SwResId(STR_WRITER2027_PF_UNSAFE_SVG_DESC),
                                             u"frame-"_ustr + OUString::number(++mnFrameSeq),
                                             PreflightTargetKind::Frame, -1, aFlyName);
                                }
                            }
                        }
                    }

                    // Pixel size (Quick + Full) and effective PPI (Full, print).
                    const Size aPix = rGraphic.GetSizePixel();
                    if (aPix.Width() > 1920)
                    {
                        AddIssue(u"W27-MEDIA-003"_ustr, PreflightSeverity::Info,
                                 PreflightCategory::Media,
                                 SwResId(STR_WRITER2027_PF_LARGE_IMAGE_TITLE),
                                 SwResId(STR_WRITER2027_PF_LARGE_IMAGE_DESC)
                                     .replaceAll(u"$1"_ustr,
                                                 OUString::number(aPix.Width())),
                                 u"frame-"_ustr + OUString::number(++mnFrameSeq),
                                 PreflightTargetKind::Frame, -1, aFlyName);
                    }
                    if (IsFull() && IsPrintOrPdf() && aPix.Width() > 0)
                    {
                        const tools::Long nRenderTwips = pFly->GetFormatAttr(RES_FRM_SIZE)
                                                             .GetSize()
                                                             .Width();
                        if (nRenderTwips > 0)
                        {
                            const double fPpi = aPix.Width() * 1440.0 / nRenderTwips;
                            if (fPpi < 96.0)
                            {
                                AddIssue(u"W27-MEDIA-002"_ustr, PreflightSeverity::Warning,
                                         PreflightCategory::Media,
                                         SwResId(STR_WRITER2027_PF_LOW_PPI_TITLE),
                                         SwResId(STR_WRITER2027_PF_LOW_PPI_DESC).replaceAll(
                                             u"$1"_ustr,
                                             rtl::math::doubleToUString(
                                                 fPpi, rtl_math_StringFormat_Automatic,
                                                 rtl_math_DecimalPlaces(0), '.')),
                                         u"frame-"_ustr + OUString::number(++mnFrameSeq),
                                         PreflightTargetKind::Frame, -1, aFlyName);
                            }
                            else if (fPpi < 150.0)
                            {
                                AddIssue(u"W27-MEDIA-002"_ustr, PreflightSeverity::Info,
                                         PreflightCategory::Media,
                                         SwResId(STR_WRITER2027_PF_LOW_PPI_TITLE),
                                         SwResId(STR_WRITER2027_PF_LOW_PPI_INFO_DESC).replaceAll(
                                             u"$1"_ustr,
                                             rtl::math::doubleToUString(
                                                 fPpi, rtl_math_StringFormat_Automatic,
                                                 rtl_math_DecimalPlaces(0), '.')),
                                         u"frame-"_ustr + OUString::number(++mnFrameSeq),
                                         PreflightTargetKind::Frame, -1, aFlyName);
                            }
                        }
                    }
                }
                else if (rNode.IsOLENode())
                {
                    // OLE: General sees an Info, Web sees a Warning (omitted).
                    AddIssue(u"W27-MEDIA-006"_ustr,
                             IsWeb() ? PreflightSeverity::Warning : PreflightSeverity::Info,
                             PreflightCategory::Media,
                             SwResId(STR_WRITER2027_PF_OLE_TITLE),
                             IsWeb() ? SwResId(STR_WRITER2027_PF_OLE_WEB_DESC)
                                     : SwResId(STR_WRITER2027_PF_OLE_GENERAL_DESC),
                             u"frame-"_ustr + OUString::number(++mnFrameSeq),
                             PreflightTargetKind::Frame, -1, aFlyName);
                }
            }
        }
    }

    // ---- Tables ----
    for (SwNodeOffset n = SwNodeOffset(1); n < rNodes.Count() - 1; ++n)
    {
        if (!rNodes[n]->IsTableNode())
            continue;
        const SwTableNode* pTableNode = rNodes[n]->GetTableNode();
        const SwTable& rTable = pTableNode->GetTable();
        const SwTableLines& rLines = rTable.GetTabLines();
        if (rLines.empty())
            continue;

        ++mnTableSeq;
        const OUString aTargetId = u"table-"_ustr + OUString::number(mnTableSeq);
        const OUString aTableName = rTable.GetFrameFormat()->GetName().toString();

        const sal_uInt16 nHeaderRows = rTable.GetRowsToRepeat();
        const size_t nRowCount = rLines.size();

        // Empty table.
        bool bHasCellContent = false;
        bool bHasMerged = false;
        size_t nMaxBoxes = 0;
        for (const SwTableLine* pLine : rLines)
        {
            nMaxBoxes = std::max(nMaxBoxes, pLine->GetTabBoxes().size());
            for (const SwTableBox* pBox : pLine->GetTabBoxes())
            {
                if (pBox->getRowSpan() > 1)
                    bHasMerged = true;
                const SwStartNode* pStt = pBox->GetSttNd();
                if (!pStt)
                    continue;
                const SwNodeOffset nEndBox = pStt->EndOfSectionIndex();
                for (SwNodeOffset nb = pStt->GetIndex() + 1; nb < nEndBox && !bHasCellContent;
                     ++nb)
                {
                    const SwTextNode* pText = pStt->GetNodes()[nb]->GetTextNode();
                    if (pText && !pText->GetText().trim().isEmpty())
                        bHasCellContent = true;
                }
            }
        }
        if (!bHasCellContent)
        {
            AddIssue(u"W27-TABLE-001"_ustr, PreflightSeverity::Info, PreflightCategory::Document,
                     SwResId(STR_WRITER2027_PF_EMPTY_TABLE_TITLE),
                     SwResId(STR_WRITER2027_PF_EMPTY_TABLE_DESC), aTargetId,
                     PreflightTargetKind::Table, static_cast<sal_Int64>(n.get()), aTableName);
        }

        // Data-like table without header semantics (no bold/text inference).
        if (nRowCount >= 3 && nHeaderRows == 0)
        {
            AddIssue(u"W27-A11Y-006"_ustr, PreflightSeverity::Info,
                     PreflightCategory::Accessibility,
                     SwResId(STR_WRITER2027_PF_TABLE_HEADERS_TITLE),
                     SwResId(STR_WRITER2027_PF_TABLE_HEADERS_DESC), aTargetId,
                     PreflightTargetKind::Table, static_cast<sal_Int64>(n.get()), aTableName);
        }
        if (bHasMerged)
        {
            AddIssue(u"W27-A11Y-007"_ustr, PreflightSeverity::Info,
                     PreflightCategory::Accessibility,
                     SwResId(STR_WRITER2027_PF_MERGED_CELLS_TITLE),
                     SwResId(STR_WRITER2027_PF_MERGED_CELLS_DESC), aTargetId,
                     PreflightTargetKind::Table, static_cast<sal_Int64>(n.get()), aTableName);
        }
        if (nMaxBoxes >= 7)
        {
            AddIssue(u"W27-A11Y-008"_ustr, PreflightSeverity::Info,
                     PreflightCategory::Accessibility,
                     SwResId(STR_WRITER2027_PF_WIDE_TABLE_TITLE),
                     IsWeb() ? SwResId(STR_WRITER2027_PF_WIDE_TABLE_WEB_DESC)
                             : SwResId(STR_WRITER2027_PF_WIDE_TABLE_DESC),
                     aTargetId, PreflightTargetKind::Table, static_cast<sal_Int64>(n.get()),
                     aTableName);
        }
    }
}

void PreflightScanner::ScanFonts()
{
    if (!maOptions.mpFontList)
        return;

    // Font usage enumeration: canonical paragraph + character style tables.
    // Deliberately style-based (no glyph-by-glyph scanning); direct character
    // formatting is not enumerated (documented limitation).
    std::set<OUString> aFamilies;
    if (const SwTextFormatColls* pColls = mrDoc.GetTextFormatColls())
    {
        for (const SwTextFormatColl* pColl : *pColls)
        {
            if (const SvxFontItem* pFont = pColl->GetItemIfSet(RES_CHRATR_FONT, false))
            {
                if (!pFont->GetFamilyName().isEmpty())
                    aFamilies.insert(pFont->GetFamilyName());
            }
        }
    }
    if (const SwCharFormats* pCharFormats = mrDoc.GetCharFormats())
    {
        for (const SwCharFormat* pFmt : *pCharFormats)
        {
            if (const SvxFontItem* pFont = pFmt->GetItemIfSet(RES_CHRATR_FONT, false))
            {
                if (!pFont->GetFamilyName().isEmpty())
                    aFamilies.insert(pFont->GetFamilyName());
            }
        }
    }

    for (const OUString& rFamily : aFamilies)
    {
        const bool bAvailable = maOptions.mpFontList->IsAvailable(rFamily);
        if (bAvailable)
            continue;
        if (IsWeb())
        {
            AddIssue(u"W27-FONT-003"_ustr, PreflightSeverity::Info,
                     PreflightCategory::Typography,
                     SwResId(STR_WRITER2027_PF_FONT_FALLBACK_TITLE),
                     SwResId(STR_WRITER2027_PF_FONT_FALLBACK_DESC).replaceAll(u"$1"_ustr,
                                                                              rFamily),
                     u"font-"_ustr + rFamily, PreflightTargetKind::Style);
        }
        else
        {
            AddIssue(u"W27-FONT-001"_ustr, PreflightSeverity::Warning,
                     PreflightCategory::Typography,
                     SwResId(STR_WRITER2027_PF_FONT_MISSING_TITLE),
                     SwResId(STR_WRITER2027_PF_FONT_MISSING_DESC).replaceAll(u"$1"_ustr, rFamily),
                     u"font-"_ustr + rFamily, PreflightTargetKind::Style);
        }
    }
}

void PreflightScanner::ScanTypeSystem()
{
    if (!maOptions.mpFontList)
        return;

    // Current preset id; empty means Custom (valid, never a problem).
    const OUString aPresetId
        = sw::writer2027typesystem::DetectCurrentTypeSystem(mrDoc, maOptions.mpFontList);
    if (aPresetId.isEmpty())
        return;

    const svx::writer2027::TypeSystemPreset* pPreset
        = svx::writer2027::Writer2027TypeSystemCatalog::Get().FindPreset(aPresetId);
    if (!pPreset)
        return;

    const svx::writer2027::ResolvedTypeSystem aResolved
        = svx::writer2027::ResolveTypeSystem(*pPreset, maOptions.mpFontList);
    for (const auto eRole : { svx::writer2027::TypeSystemFontRole::Body,
                              svx::writer2027::TypeSystemFontRole::Heading,
                              svx::writer2027::TypeSystemFontRole::Mono,
                              svx::writer2027::TypeSystemFontRole::Display })
    {
        const svx::writer2027::TypeSystemResolvedRole& rRole = aResolved.Get(eRole);
        if (rRole.mbUnresolved || rRole.mbFallbackUsed)
        {
            const OUString aRoleName
                = OUString::number(static_cast<int>(eRole)); // role index, not localized
            const OUString aDesc
                = rRole.mbUnresolved
                      ? SwResId(STR_WRITER2027_PF_TYPESYSTEM_MISSING_DESC)
                      : SwResId(STR_WRITER2027_PF_TYPESYSTEM_FALLBACK_DESC);
            AddIssue(u"W27-FONT-002"_ustr, PreflightSeverity::Warning,
                     PreflightCategory::Typography,
                     SwResId(STR_WRITER2027_PF_TYPESYSTEM_TITLE),
                     aDesc.replaceAll(u"$1"_ustr, aRoleName),
                     u"style-typesystem-"_ustr + aPresetId, PreflightTargetKind::Style, -1,
                     OUString(), -1, -1, PreflightFixKind::OpenTypeSystem);
        }
    }
}

void PreflightScanner::ScanContrast()
{
    if (!IsFull())
        return;

    // Body text vs page background (deterministic style/object colors only).
    Color aText(COL_BLACK);
    if (const SwTextFormatColls* pColls = mrDoc.GetTextFormatColls())
    {
        for (const SwTextFormatColl* pColl : *pColls)
        {
            if (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_STANDARD)
            {
                if (const SvxColorItem* pColor = pColl->GetItemIfSet(RES_CHRATR_COLOR, false))
                {
                    aText = pColor->GetValue();
                    break;
                }
            }
        }
    }
    Color aBg(Color(0xff, 0xff, 0xff));
    std::unique_ptr<SvxBrushItem> pBrush;
    const SfxItemState eState
        = mrDoc.GetPageDesc(0).GetMaster().GetBackgroundState(pBrush);
    if (eState == SfxItemState::SET && pBrush && pBrush->GetColor() != COL_TRANSPARENT)
        aBg = pBrush->GetColor();

    const double fRatio = lcl_ContrastRatio(aText, aBg);
    if (fRatio < 4.5)
    {
        AddIssue(u"W27-A11Y-005"_ustr, PreflightSeverity::Warning,
                 PreflightCategory::Accessibility,
                 SwResId(STR_WRITER2027_PF_CONTRAST_TITLE),
                 SwResId(STR_WRITER2027_PF_CONTRAST_DESC).replaceAll(
                     u"$1"_ustr,
                     rtl::math::doubleToUString(fRatio, rtl_math_StringFormat_Automatic,
                                                rtl_math_DecimalPlaces(1), '.')),
                 u"document"_ustr, PreflightTargetKind::Document);
    }
    else if (fRatio < 7.0)
    {
        AddIssue(u"W27-A11Y-005"_ustr, PreflightSeverity::Info,
                 PreflightCategory::Accessibility,
                 SwResId(STR_WRITER2027_PF_CONTRAST_TITLE),
                 SwResId(STR_WRITER2027_PF_CONTRAST_INFO_DESC).replaceAll(
                     u"$1"_ustr,
                     rtl::math::doubleToUString(fRatio, rtl_math_StringFormat_Automatic,
                                                rtl_math_DecimalPlaces(1), '.')),
                 u"document"_ustr, PreflightTargetKind::Document);
    }
}

void PreflightScanner::ScanWebSpecific()
{
    if (!IsWeb())
        return;

    // TOC / index page-number semantics.
    if (mbHasToxSection)
    {
        AddIssue(u"W27-REF-002"_ustr, PreflightSeverity::Info,
                 PreflightCategory::LinksReferences,
                 SwResId(STR_WRITER2027_PF_TOC_PAGENUM_TITLE),
                 SwResId(STR_WRITER2027_PF_TOC_PAGENUM_DESC), u"document"_ustr,
                 PreflightTargetKind::Document);
    }

    // Paper-only page-number fields.
    if (mnPaperFields > 0)
    {
        AddIssue(u"W27-WEB-002"_ustr, PreflightSeverity::Info, PreflightCategory::Web,
                 SwResId(STR_WRITER2027_PF_PAGEFIELDS_TITLE),
                 SwResId(STR_WRITER2027_PF_PAGEFIELDS_DESC).replaceAll(
                     u"$1"_ustr, OUString::number(mnPaperFields)),
                 u"document"_ustr, PreflightTargetKind::Document);
    }

    // Single HTML size estimate (Full only, bounded).
    if (IsFull() && maOptions.meProfile == PreflightProfile::SingleHtml)
    {
        sal_uInt64 nEstimate = 0;
        const sw::FrameFormats<sw::SpzFrameFormat*>* pFormats = mrDoc.GetSpzFrameFormats();
        if (pFormats)
        {
            for (sw::SpzFrameFormat* pFly : *pFormats)
            {
                if (!pFly)
                    continue;
                const SwFormatContent& rContent = pFly->GetContent();
                const SwNodeIndex* pIdx = rContent.GetContentIdx();
                if (!pIdx)
                    continue;
                const SwNodeOffset nEnd = pIdx->GetNode().EndOfSectionIndex();
                for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
                {
                    if (const SwGrfNode* pGrf = pIdx->GetNodes()[n]->GetGrfNode())
                    {
                        const Graphic& rGraphic = pGrf->GetGrfObj().GetGraphic();
                        const GfxLink& rLink = rGraphic.GetGfxLink();
                        if (rLink.IsNative() && rLink.GetDataSize() > 0)
                            nEstimate += rLink.GetDataSize();
                        else
                        {
                            const Size aPix = rGraphic.GetSizePixel();
                            nEstimate += static_cast<sal_uInt64>(aPix.Width())
                                         * static_cast<sal_uInt64>(aPix.Height()) * 4;
                        }
                    }
                }
            }
        }
        if (nEstimate > 2 * 1024 * 1024)
        {
            AddIssue(u"W27-WEB-001"_ustr, PreflightSeverity::Info, PreflightCategory::Web,
                     SwResId(STR_WRITER2027_PF_SINGLEHTML_SIZE_TITLE),
                     SwResId(STR_WRITER2027_PF_SINGLEHTML_SIZE_DESC).replaceAll(
                         u"$1"_ustr, OUString::number(nEstimate / (1024 * 1024))),
                     u"document"_ustr, PreflightTargetKind::Document);
        }
    }
}

PreflightReport PreflightScanner::Run()
{
    ScanGeneral();
    ScanHeadingsAndLinks();
    ScanMedia();
    ScanFonts();
    ScanTypeSystem();
    ScanContrast();
    ScanWebSpecific();
    maReport.maProfile = OUString::fromUtf8(ProfileToString(maOptions.meProfile));
    maReport.meMode = maOptions.meMode;
    return std::move(maReport);
}

} // namespace

namespace sw::writer2027preflight
{
const char* ProfileToString(PreflightProfile eProfile)
{
    switch (eProfile)
    {
        case PreflightProfile::General:
            return "general";
        case PreflightProfile::PdfDigital:
            return "pdf-digital";
        case PreflightProfile::PrintFriendly:
            return "print-friendly";
        case PreflightProfile::PrintDigital:
            return "print-digital";
        case PreflightProfile::WebPackage:
            return "web-package";
        case PreflightProfile::SingleHtml:
            return "single-html";
    }
    return "general";
}

PreflightReport RunPreflight(SwDoc& rDoc, const PreflightOptions& rOptions)
{
    PreflightScanner aScanner(rDoc, rOptions);
    return aScanner.Run();
}

OUString PreflightReportToJson(const PreflightReport& rReport)
{
    const char* pSeverity = nullptr;
    const char* pCategory = nullptr;
    OUStringBuffer aJson;
    aJson.append(u"{\n"_ustr);
    aJson.append(u"  \"format\": \"writer2027-preflight\",\n"_ustr);
    aJson.append(u"  \"version\": 1,\n"_ustr);
    aJson.append(u"  \"profile\": ").append(lcl_JsonEscape(rReport.maProfile)).append(u",\n"_ustr);
    aJson.append(u"  \"mode\": ")
        .append(lcl_JsonEscape(rReport.meMode == PreflightScanMode::Full ? OUString(u"full"_ustr)
                                                                         : OUString(u"quick"_ustr)))
        .append(u",\n"_ustr);
    aJson.append(u"  \"summary\": {\n"_ustr);
    aJson.append(u"    \"errors\": ").append(OUString::number(rReport.mnErrors)).append(u",\n"_ustr);
    aJson.append(u"    \"warnings\": ")
        .append(OUString::number(rReport.mnWarnings))
        .append(u",\n"_ustr);
    aJson.append(u"    \"info\": ").append(OUString::number(rReport.mnInfos)).append(u"\n"_ustr);
    aJson.append(u"  },\n"_ustr);
    aJson.append(u"  \"issues\": ["_ustr);
    bool bFirst = true;
    for (const PreflightIssue& rIssue : rReport.maIssues)
    {
        if (!bFirst)
            aJson.append(u","_ustr);
        bFirst = false;
        switch (rIssue.meSeverity)
        {
            case PreflightSeverity::Error:
                pSeverity = "error";
                break;
            case PreflightSeverity::Warning:
                pSeverity = "warning";
                break;
            case PreflightSeverity::Info:
                pSeverity = "info";
                break;
        }
        switch (rIssue.meCategory)
        {
            case PreflightCategory::Document:
                pCategory = "document";
                break;
            case PreflightCategory::Accessibility:
                pCategory = "accessibility";
                break;
            case PreflightCategory::Typography:
                pCategory = "typography";
                break;
            case PreflightCategory::Media:
                pCategory = "media";
                break;
            case PreflightCategory::LinksReferences:
                pCategory = "links-references";
                break;
            case PreflightCategory::Web:
                pCategory = "web";
                break;
            case PreflightCategory::Pdf:
                pCategory = "pdf";
                break;
            case PreflightCategory::Print:
                pCategory = "print";
                break;
        }
        aJson.append(u"\n    { "_ustr);
        aJson.append(u"\"checkId\": ").append(lcl_JsonEscape(rIssue.maCheckId));
        aJson.append(u", \"severity\": ").append(OUString::fromUtf8(pSeverity));
        aJson.append(u", \"category\": ").append(OUString::fromUtf8(pCategory));
        aJson.append(u", \"target\": ").append(lcl_JsonEscape(rIssue.maTargetId));
        aJson.append(u", \"message\": ").append(lcl_JsonEscape(rIssue.maTitle));
        aJson.append(u" }"_ustr);
    }
    if (!bFirst)
        aJson.append(u"\n"_ustr);
    aJson.append(u"  ]\n}\n"_ustr);
    return aJson.makeStringAndClear();
}

} // namespace sw::writer2027preflight

// ---------------------------------------------------------------------------
// sw::writer2027capabilities implementations (shared pure helpers)
// ---------------------------------------------------------------------------

namespace sw::writer2027capabilities
{
OUString SafeHref(const OUString& rUrl)
{
    if (rUrl.isEmpty())
        return OUString();
    if (rUrl.startsWith("#") || rUrl.startsWith("/"))
        return rUrl;
    const sal_Int32 nColon = rUrl.indexOf(':');
    if (nColon < 0)
        return rUrl; // relative path without a scheme
    const OUString aScheme = rUrl.copy(0, nColon).toAsciiLowerCase();
    if (aScheme == "http" || aScheme == "https" || aScheme == "mailto" || aScheme == "tel")
        return rUrl;
    return OUString();
}

bool GfxTypeToWeb(GfxLinkType eType, OUString& rExt, OUString& rMime)
{
    switch (eType)
    {
        case GfxLinkType::NativePng:
            rExt = u".png"_ustr;
            rMime = u"image/png"_ustr;
            return true;
        case GfxLinkType::NativeJpg:
            rExt = u".jpg"_ustr;
            rMime = u"image/jpeg"_ustr;
            return true;
        case GfxLinkType::NativeGif:
            rExt = u".gif"_ustr;
            rMime = u"image/gif"_ustr;
            return true;
        case GfxLinkType::NativeSvg:
            rExt = u".svg"_ustr;
            rMime = u"image/svg+xml"_ustr;
            return true;
        case GfxLinkType::NativeWebp:
            rExt = u".webp"_ustr;
            rMime = u"image/webp"_ustr;
            return true;
        default:
            return false;
    }
}

bool SvgIsSafe(const std::vector<sal_uInt8>& rData)
{
    if (rData.empty())
        return false;
    const OUString aSvg = OUString::fromUtf8(
        OString(reinterpret_cast<const char*>(rData.data()),
                static_cast<sal_Int32>(rData.size())));
    if (aSvg.isEmpty())
        return false;
    const OUString aLower = aSvg.toAsciiLowerCase();
    if (aLower.indexOf("script") >= 0)
        return false;
    if (aLower.indexOf("foreignobject") >= 0)
        return false;
    for (const OUString& rEvt : { OUString(u"onclick"_ustr), OUString(u"onload"_ustr),
                                  OUString(u"onerror"_ustr), OUString(u"onmouse"_ustr),
                                  OUString(u"onkey"_ustr) })
        if (aLower.indexOf(rEvt) >= 0)
            return false;
    sal_Int32 nPos = 0;
    while ((nPos = aLower.indexOf("href", nPos)) >= 0)
    {
        const sal_Int32 nEq = aLower.indexOf('=', nPos + 4);
        if (nEq > 0 && nEq + 1 < aLower.getLength())
        {
            const sal_Unicode cQuote = aLower[nEq + 1];
            if (cQuote == '"' || cQuote == '\'')
            {
                const sal_Int32 nEnd = aLower.indexOf(cQuote, nEq + 2);
                if (nEnd > nEq + 1)
                {
                    const OUString aTarget = aLower.copy(nEq + 2, nEnd - nEq - 2).trim();
                    if (!aTarget.isEmpty() && !aTarget.startsWith("#"))
                        return false;
                }
            }
        }
        nPos += 4;
    }
    return true;
}

FrameCapability ClassifyFrameAnchor(RndStdIds eAnchor)
{
    switch (eAnchor)
    {
        case RndStdIds::FLY_AS_CHAR:
        case RndStdIds::FLY_AT_CHAR:
        case RndStdIds::FLY_AT_PARA:
        case RndStdIds::FLY_AT_FLY:
            return FrameCapability::Full;
        case RndStdIds::FLY_AT_PAGE:
            return FrameCapability::Degraded;
        default:
            return FrameCapability::Omitted;
    }
}

} // namespace sw::writer2027capabilities

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */