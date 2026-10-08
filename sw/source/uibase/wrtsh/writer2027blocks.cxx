/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027blocks.hxx>

#include <svx/writer2027blocks.hxx>

#include <o3tl/safeint.hxx>
#include <tools/color.hxx>

#include <algorithm>

#include <IDocumentContentOperations.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <IDocumentUndoRedo.hxx>
#include <SwRewriter.hxx>
#include <doc.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/borderline.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/cmapitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/svxenum.hxx>
#include <editeng/ulspitem.hxx>
#include <fmtanchr.hxx>
#include <fmtclds.hxx>
#include <fmtcntnt.hxx>
#include <fmtfsize.hxx>
#include <format.hxx>
#include <frmatr.hxx>
#include <frame.hxx>
#include <hintids.hxx>
#include <layfrm.hxx>
#include <ndarr.hxx>
#include <ndtxt.hxx>
#include <node.hxx>
#include <nodeoffset.hxx>
#include <pam.hxx>
#include <poolfmt.hxx>
#include <section.hxx>
#include <strings.hrc>
#include <svl/itemset.hxx>
#include <svl/whichranges.hxx>
#include <swundo.hxx>
#include <swtypes.hxx>
#include <fmtcol.hxx>
#include <wrtsh.hxx>
#include <writer2027.hxx>
#include <writer2027view.hxx>

namespace sw::writer2027blocks
{

namespace
{

/// Paragraph + character + frame attribute ranges (same as the Phase 6
/// Type System service; block styles are ordinary paragraph styles).
SfxItemSet lcl_MakeItemSet(SwDoc& rDoc)
{
    return SfxItemSet(rDoc.GetAttrPool(),
                      svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END - 1, RES_PARATR_BEGIN,
                                 RES_PARATR_END - 1, RES_FRMATR_BEGIN, RES_FRMATR_END - 1>);
}

void lcl_ApplyFormat(SwDoc& rDoc, SwFormat& rFormat, const SfxItemSet& rSet)
{
    if (rSet.Count() == 0)
        return;
    // Skip when nothing actually differs: re-inserting a block must not
    // create no-op style-change undo entries.
    SfxItemSet aDiff(rSet);
    aDiff.Differentiate(rFormat.GetAttrSet());
    if (aDiff.Count() == 0)
        return;
    rDoc.ChgFormat(rFormat, rSet);
}

void lcl_PutColor(SfxItemSet& rSet, const Color& rColor)
{
    rSet.Put(SvxColorItem(rColor, RES_CHRATR_COLOR));
}

void lcl_PutUpperLower(SfxItemSet& rSet, tools::Long nUpper, tools::Long nLower)
{
    if (nUpper <= 0 && nLower <= 0)
        return;
    SvxULSpaceItem aUL(RES_UL_SPACE);
    if (nUpper > 0)
        aUL.SetUpper(o3tl::narrowing<sal_uInt16>(nUpper));
    if (nLower > 0)
        aUL.SetLower(o3tl::narrowing<sal_uInt16>(nLower));
    rSet.Put(aUL);
}

void lcl_PutLeftRight(SfxItemSet& rSet, tools::Long nLeft, tools::Long nRight)
{
    if (nLeft <= 0 && nRight <= 0)
        return;
    SvxLRSpaceItem aLR(RES_LR_SPACE);
    if (nLeft > 0)
        aLR.SetLeft(SvxIndentValue::twips(nLeft));
    if (nRight > 0)
        aLR.SetRight(SvxIndentValue::twips(nRight));
    rSet.Put(aLR);
}

/// Left accent rule in a neutral border color: hairline-width on dark
/// documents (Writer2027 hairline), a medium-width neutral gray on light /
/// imported documents so the block stays readable there too.
editeng::SvxBorderLine lcl_MakeAccentLine(bool bDark)
{
    Color aLineColor(bDark ? sw::writer2027::Hairline : Color(0x8A, 0x94, 0x9E));
    editeng::SvxBorderLine aLine(&aLineColor,
                                 bDark ? SvxBorderLineWidth::Thin : SvxBorderLineWidth::Medium);
    return aLine;
}

/// Subtle tonal elevation for callout / code backgrounds. Only applied on
/// the digital-dark Writer 2027 page; light documents stay text-only.
constexpr Color CalloutFill(0x1B, 0x22, 0x2B);

/// The ten role-based block styles. Each derives from a canonical pool
/// style, so blocks inherit the active Phase 6 Type System through the
/// derivation chain (display / heading / body / mono / caption roles) and
/// Custom typography is simply "the actual current pool styles".
enum class BlockStyleId
{
    HeroEyebrow,
    HeroTitle,
    PullQuote,
    StatValue,
    StatLabel,
    ImagePlaceholder,
    Callout,
    ResearchNote,
    Code,
    ClosingStatement,
};

TranslateId lcl_StyleNameRes(BlockStyleId eId)
{
    switch (eId)
    {
        case BlockStyleId::HeroEyebrow:
            return STR_WRITER2027_STYLE_HERO_EYEBROW;
        case BlockStyleId::HeroTitle:
            return STR_WRITER2027_STYLE_HERO_TITLE;
        case BlockStyleId::PullQuote:
            return STR_WRITER2027_STYLE_PULL_QUOTE;
        case BlockStyleId::StatValue:
            return STR_WRITER2027_STYLE_STAT_VALUE;
        case BlockStyleId::StatLabel:
            return STR_WRITER2027_STYLE_STAT_LABEL;
        case BlockStyleId::ImagePlaceholder:
            return STR_WRITER2027_STYLE_IMAGE_PLACEHOLDER;
        case BlockStyleId::Callout:
            return STR_WRITER2027_STYLE_CALLOUT;
        case BlockStyleId::ResearchNote:
            return STR_WRITER2027_STYLE_RESEARCH_NOTE;
        case BlockStyleId::Code:
            return STR_WRITER2027_STYLE_CODE;
        case BlockStyleId::ClosingStatement:
            return STR_WRITER2027_STYLE_CLOSING_STATEMENT;
    }
    return STR_WRITER2027_STYLE_CLOSING_STATEMENT;
}

SwPoolFormatId lcl_StyleBaseId(BlockStyleId eId)
{
    switch (eId)
    {
        case BlockStyleId::HeroEyebrow:
        case BlockStyleId::StatLabel:
            return SwPoolFormatId::COLL_LABEL; // caption role
        case BlockStyleId::HeroTitle:
        case BlockStyleId::StatValue:
        case BlockStyleId::ClosingStatement:
            return SwPoolFormatId::COLL_DOC_TITLE; // display role
        case BlockStyleId::PullQuote:
            return SwPoolFormatId::COLL_HTML_BLOCKQUOTE; // quote role
        case BlockStyleId::ImagePlaceholder:
        case BlockStyleId::Callout:
        case BlockStyleId::ResearchNote:
            return SwPoolFormatId::COLL_STANDARD; // body role
        case BlockStyleId::Code:
            return SwPoolFormatId::COLL_HTML_PRE; // mono role
    }
    return SwPoolFormatId::COLL_STANDARD;
}

/// Create a role-based block style in rDoc (idempotent). The style derives
/// from the canonical pool style and carries only the small delta needed for
/// its role; everything else inherits the current Type System.
SwTextFormatColl* lcl_EnsureBlockStyle(SwDoc& rDoc, BlockStyleId eId, bool bDark)
{
    const OUString aName = SwResId(lcl_StyleNameRes(eId));
    if (SwTextFormatColl* pExisting = rDoc.FindTextFormatCollByName(UIName(aName)))
        return pExisting;

    SwTextFormatColl* pBase
        = rDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(lcl_StyleBaseId(eId));
    SwTextFormatColl* pStyle = rDoc.MakeTextFormatColl(UIName(aName), pBase);
    if (!pStyle)
        return nullptr;

    SfxItemSet aSet = lcl_MakeItemSet(rDoc);
    switch (eId)
    {
        case BlockStyleId::HeroEyebrow:
            // Small muted uppercase label above the hero title.
            if (bDark)
                lcl_PutColor(aSet, sw::writer2027::TextSecondary);
            aSet.Put(SvxCaseMapItem(SvxCaseMap::Uppercase, RES_CHRATR_CASEMAP));
            lcl_PutUpperLower(aSet, 0, 120);
            break;
        case BlockStyleId::HeroTitle:
        case BlockStyleId::ClosingStatement:
            // Pure display-role inheritance: the Type System drives family,
            // size and weight through the Title pool style.
            break;
        case BlockStyleId::PullQuote:
            // Indented quote with generous space; the quote accent color
            // comes from the Block Quotation pool style (Phase 6 preset).
            lcl_PutLeftRight(aSet, 500, 500);
            break;
        case BlockStyleId::StatValue:
            lcl_PutUpperLower(aSet, 120, 0);
            break;
        case BlockStyleId::StatLabel:
            if (bDark)
                lcl_PutColor(aSet, sw::writer2027::TextSecondary);
            lcl_PutUpperLower(aSet, 0, 80);
            break;
        case BlockStyleId::ImagePlaceholder:
            // Centered muted "Insert image" prompt; dashed region drawn by
            // the paragraph border inside the frame.
            aSet.Put(SvxAdjustItem(SvxAdjust::Center, RES_PARATR_ADJUST));
            if (bDark)
                lcl_PutColor(aSet, sw::writer2027::TextSecondary);
            {
                Color aDashColor(bDark ? sw::writer2027::Hairline : Color(0x8A, 0x94, 0x9E));
                editeng::SvxBorderLine aDash(&aDashColor, SvxBorderLineWidth::Thin);
                aDash.SetBorderLineStyle(SvxBorderLineStyle::DASHED);
                SvxBoxItem aBox(RES_BOX);
                aBox.SetAllDistances(110);
                aBox.SetLine(&aDash, SvxBoxItemLine::TOP);
                aBox.SetLine(&aDash, SvxBoxItemLine::BOTTOM);
                aBox.SetLine(&aDash, SvxBoxItemLine::LEFT);
                aBox.SetLine(&aDash, SvxBoxItemLine::RIGHT);
                aSet.Put(aBox);
            }
            lcl_PutUpperLower(aSet, 400, 400);
            break;
        case BlockStyleId::Callout:
            // Subtle tonal elevation + left accent rule on the dark page;
            // text-only with a neutral rule on light/imported documents.
            if (bDark)
            {
                aSet.Put(SvxBrushItem(CalloutFill, RES_BACKGROUND));
            }
            {
                editeng::SvxBorderLine aAccent = lcl_MakeAccentLine(bDark);
                SvxBoxItem aBox(RES_BOX);
                aBox.SetDistance(160, SvxBoxItemLine::LEFT);
                aBox.SetLine(&aAccent, SvxBoxItemLine::LEFT);
                aSet.Put(aBox);
            }
            lcl_PutLeftRight(aSet, 400, 400);
            lcl_PutUpperLower(aSet, 150, 150);
            break;
        case BlockStyleId::ResearchNote:
            lcl_PutLeftRight(aSet, 300, 300);
            break;
        case BlockStyleId::Code:
            // Mono role (Preformatted Text) + subtle tonal background on the
            // dark page only.
            if (bDark)
                aSet.Put(SvxBrushItem(CalloutFill, RES_BACKGROUND));
            lcl_PutLeftRight(aSet, 250, 250);
            lcl_PutUpperLower(aSet, 120, 120);
            break;
    }
    lcl_ApplyFormat(rDoc, *pStyle, aSet);
    return pStyle;
}

/// True when the document is a brand-new empty Writer document (start node +
/// one empty text node + end node). Used by the ReplaceEmptyDocument policy.
bool lcl_IsEmptyDocument(SwWrtShell& rSh)
{
    SwDoc& rDoc = *rSh.GetDoc();
    if (rDoc.GetNodes().Count() != SwNodeOffset(3))
        return false;
    const SwTextNode* pText = rDoc.GetNodes()[SwNodeOffset(1)]->GetTextNode();
    return pText && pText->GetText().isEmpty();
}

/// Current paragraph at the shell cursor, or nullptr.
SwTextNode* lcl_GetCursorTextNode(SwWrtShell& rSh)
{
    SwPaM* pCursor = rSh.GetCursor();
    if (!pCursor)
        return nullptr;
    return pCursor->GetPoint()->GetNode().GetTextNode();
}

/// Text width of the current frame (fallback 6.5in) - used to size the
/// image placeholder frames proportionally.
tools::Long lcl_CurrentTextWidth(SwWrtShell& rSh)
{
    if (SwContentFrame* pFrame = rSh.GetCurrFrame(false))
    {
        const tools::Long nWidth = pFrame->getFramePrintArea().Width();
        if (nWidth > 0)
            return nWidth;
    }
    return 9360;
}

/** Small builder that composes a block from canonical Writer structures at
    the shell cursor. One block = one logical undo action (the caller wraps
    everything in StartUndo/EndUndo); the caret is placed into the most
    useful editable field with the placeholder text selected. */
class BlockBuilder
{
public:
    BlockBuilder(SwWrtShell& rSh)
        : mrSh(rSh)
        , mrDoc(*rSh.GetDoc())
        , mbDark(sw::writer2027view::IsWriter2027DarkDocument(mrDoc))
    {
    }

    bool Start() // returns false when the block cannot be built (read-only etc.)
    {
        if (!mrSh.CanInsert())
            return false;
        mrSh.StartAllAction();
        mrSh.StartUndo(SwUndoId::WRITER2027_BLOCK_INSERT, nullptr);
        return true;
    }

    void Finish()
    {
        mrSh.EndUndo(SwUndoId::WRITER2027_BLOCK_INSERT, nullptr);
        mrSh.EndAllAction();
        PlaceCaret();
    }

    /// Insert a hard page break unless the document is brand-new and empty.
    void InsertLeadingPageBreak()
    {
        if (lcl_IsEmptyDocument(mrSh))
            return;
        mrSh.InsertPageBreak();
    }

    /// Ensure the cursor sits at the start of an empty paragraph (reuse an
    /// empty one, otherwise split).
    void EnsureFreshParagraph()
    {
        SwTextNode* pNode = lcl_GetCursorTextNode(mrSh);
        if (!pNode || !pNode->GetText().isEmpty())
            mrSh.SplitNode();
    }

    /// Apply a paragraph style to the current paragraph and insert text.
    /// Returns the paragraph the text was inserted into. The paragraph must
    /// be fresh (see EnsureFreshParagraph) for caret offsets to be
    /// deterministic.
    SwTextNode* WriteParagraph(SwTextFormatColl* pStyle, const OUString& rText, bool bSplitAfter)
    {
        SwTextNode* pNode = lcl_GetCursorTextNode(mrSh);
        mrSh.SetTextFormatColl(pStyle, /*bResetListAttrs=*/true, /*bResetAllCharAttrs=*/true);
        mrSh.Insert(rText);
        if (bSplitAfter)
            mrSh.SplitNode();
        return pNode;
    }

    /// Write a paragraph and make its text the post-insertion focus field
    /// (the caret lands there with the text selected, so typing replaces it).
    SwTextNode* WriteFocusParagraph(SwTextFormatColl* pStyle, const OUString& rText,
                                    bool bSplitAfter)
    {
        SwTextNode* pNode = lcl_GetCursorTextNode(mrSh);
        const sal_Int32 nStart = pNode ? pNode->GetText().getLength() : 0;
        mrSh.SetTextFormatColl(pStyle, /*bResetListAttrs=*/true, /*bResetAllCharAttrs=*/true);
        mrSh.Insert(rText);
        if (pNode)
            SetFocus(pNode, nStart, nStart + rText.getLength());
        if (bSplitAfter)
            mrSh.SplitNode();
        return pNode;
    }

    /// Create an as-character anchored frame (full text width, 16:9) with a
    /// placeholder paragraph inside. Returns the frame format (may be null).
    SwFlyFrameFormat* InsertImageFrame(const OUString& rFrameName, const OUString& rPlaceholder)
    {
        SwTextNode* pPara = lcl_GetCursorTextNode(mrSh);
        if (!pPara)
            return nullptr;

        SwTextFormatColl* pPlaceholderStyle
            = lcl_EnsureBlockStyle(mrDoc, BlockStyleId::ImagePlaceholder, mbDark);

        const tools::Long nWidth = lcl_CurrentTextWidth(mrSh);
        const tools::Long nHeight = nWidth * 9 / 16;

        SfxItemSet aSet(SfxItemSet::makeFixedSfxItemSet<RES_FRM_SIZE, RES_FRM_SIZE, RES_ANCHOR,
                                                        RES_ANCHOR>(mrDoc.GetAttrPool()));
        aSet.Put(SwFormatAnchor(RndStdIds::FLY_AS_CHAR));
        aSet.Put(SwFormatFrameSize(SwFrameSize::Fixed, nWidth, nHeight));

        SwPosition aPos(*pPara, pPara->GetText().getLength());
        SwFlyFrameFormat* pFly = mrDoc.MakeFlySection(RndStdIds::FLY_AS_CHAR, &aPos, &aSet,
                                                      nullptr, /*bCalledFromShell=*/true);
        if (!pFly)
            return nullptr;

        pFly->SetFormatName(UIName(rFrameName), /*bBroadcast=*/true);

        // Placeholder paragraph inside the frame: canonical content node of
        // the fly section, styled + filled at the model level.
        const SwFormatContent& rContent = pFly->GetContent();
        const SwNodeIndex* pIdx = rContent.GetContentIdx();
        if (pIdx)
        {
            SwNodeIndex aFirst(*pIdx, 1);
            if (SwTextNode* pFrameText = aFirst.GetNode().GetTextNode())
            {
                if (pPlaceholderStyle)
                    pFrameText->ChgFormatColl(pPlaceholderStyle, /*bSetListLevel=*/true);
                mrDoc.getIDocumentContentOperations().InsertString(
                    SwPaM(SwPosition(*pFrameText, 0)), rPlaceholder, SwInsertFlags::EMPTYEXPAND);
            }
        }

        // Move the shell cursor after the frame so following content flows
        // normally: the as-char fly occupies exactly one content position.
        if (const SwPosition* pAnchor = pFly->GetAnchor().GetContentAnchor())
        {
            SwPosition aAfterFrame(*pAnchor);
            aAfterFrame.nContent += 1;
            mrSh.SetSelection(SwPaM(aAfterFrame));
        }
        return pFly;
    }

    /// Wrap the current selection in a named section with nCols columns.
    void WrapInColumnSection(const OUString& rSectionName, sal_uInt16 nCols)
    {
        if (nCols < 2)
            return;
        SfxItemSet aSet(SfxItemSet::makeFixedSfxItemSet<RES_COL, RES_COL>(mrDoc.GetAttrPool()));
        SwFormatCol aCol;
        // 12pt gutter; nAct = the current text width so the initial column
        // geometry is sane (the layout recalculates against the section frame).
        const sal_uInt16 nActWidth
            = std::min<tools::Long>(lcl_CurrentTextWidth(mrSh), SAL_MAX_UINT16);
        aCol.Init(nCols, /*nGutterWidth=*/240, nActWidth);
        aSet.Put(aCol);

        SwSectionData aData(SectionType::Content, UIName(rSectionName));
        mrSh.InsertSection(aData, &aSet);
    }

    /// Select from the start of rStartNode to the end of the cursor node.
    void SelectFromToEnd(SwTextNode* pStart)
    {
        SwTextNode* pEnd = lcl_GetCursorTextNode(mrSh);
        if (!pStart || !pEnd)
            return;
        SwPosition aStart(*pStart, 0);
        SwPosition aEnd(*pEnd, pEnd->GetText().getLength());
        mrSh.SetSelection(SwPaM(aStart, aEnd));
    }

    void SetFocus(SwTextNode* pNode, sal_Int32 nStart, sal_Int32 nEnd)
    {
        mpFocusNode = pNode;
        mnFocusStart = nStart;
        mnFocusEnd = nEnd;
    }

    void PlaceCaret()
    {
        if (!mpFocusNode)
            return;
        SwPosition aStart(*mpFocusNode, mnFocusStart);
        SwPosition aEnd(*mpFocusNode, mnFocusEnd);
        mrSh.SetSelection(SwPaM(aStart, aEnd));
    }

    SwWrtShell& mrSh;
    SwDoc& mrDoc;
    bool mbDark;

    SwTextNode* mpFocusNode = nullptr;
    sal_Int32 mnFocusStart = 0;
    sal_Int32 mnFocusEnd = 0;
};

SwTextFormatColl* lcl_PoolColl(SwDoc& rDoc, SwPoolFormatId nId)
{
    return rDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(nId);
}

/// Hero / Cover: eyebrow, large title, subtitle, metadata row, optional
/// hero image frame. New page unless the document is brand-new and empty.
void lcl_BuildHero(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.InsertLeadingPageBreak();
    rB.EnsureFreshParagraph();

    rB.WriteParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::HeroEyebrow, rB.mbDark),
                      SwResId(STR_WRITER2027_PLACEHOLDER_EYEBROW), /*bSplitAfter=*/true);
    rB.WriteFocusParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::HeroTitle, rB.mbDark),
                           SwResId(STR_WRITER2027_PLACEHOLDER_TITLE), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_DOC_SUBTITLE),
                      SwResId(STR_WRITER2027_PLACEHOLDER_SUBTITLE), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_META), /*bSplitAfter=*/true);

    rB.InsertImageFrame(SwResId(STR_WRITER2027_FRAME_HERO_IMAGE),
                        SwResId(STR_WRITER2027_PLACEHOLDER_IMAGE));
    rB.mrSh.SplitNode(); // end the frame paragraph; following content flows normally
}

/// Section Opener: number/eyebrow, real Heading 1, short intro.
void lcl_BuildSectionIntro(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.WriteParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::HeroEyebrow, rB.mbDark),
                      SwResId(STR_WRITER2027_PLACEHOLDER_SECTION_NUMBER), /*bSplitAfter=*/true);
    rB.WriteFocusParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_HEADLINE1),
                           SwResId(STR_WRITER2027_PLACEHOLDER_SECTION_HEADING),
                           /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_STANDARD),
                      SwResId(STR_WRITER2027_PLACEHOLDER_SECTION_INTRO), /*bSplitAfter=*/false);
}

/// Pull Quote: oversized quote + attribution. Stays plain editable text.
void lcl_BuildPullQuote(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.WriteFocusParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::PullQuote, rB.mbDark),
                           SwResId(STR_WRITER2027_PLACEHOLDER_QUOTE), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_ATTRIBUTION), /*bSplitAfter=*/false);
}

/// Key Stat / KPI: large value, short label, optional supporting line.
/// Plain paragraphs: the best editing/export behavior for a single stat
/// (no cell chrome, no frame anchoring, identical ODT/DOCX mapping).
void lcl_BuildKeyStat(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.WriteFocusParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::StatValue, rB.mbDark),
                           SwResId(STR_WRITER2027_PLACEHOLDER_STAT_VALUE), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::StatLabel, rB.mbDark),
                      SwResId(STR_WRITER2027_PLACEHOLDER_STAT_LABEL), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_STAT_LINE), /*bSplitAfter=*/false);
}

/// Image + Caption: frame placeholder, caption, source/credit line.
void lcl_BuildImageCaption(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.InsertImageFrame(SwResId(STR_WRITER2027_FRAME_IMAGE),
                        SwResId(STR_WRITER2027_PLACEHOLDER_IMAGE));
    rB.mrSh.SplitNode(); // end the frame paragraph

    rB.WriteFocusParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                           SwResId(STR_WRITER2027_PLACEHOLDER_CAPTION), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_CREDIT), /*bSplitAfter=*/false);
}

/// Two-Column Editorial: bounded section with two columns of balanced text.
/// The section is created from canonical Writer section/column structures;
/// the user can edit or remove it through the standard Section dialog.
void lcl_BuildTwoColumn(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    const OUString aColText = SwResId(STR_WRITER2027_PLACEHOLDER_COLUMN_TEXT);
    SwTextNode* pFirst = rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_STANDARD),
                                           aColText, /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_STANDARD), aColText,
                      /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_STANDARD), aColText,
                      /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_STANDARD), aColText,
                      /*bSplitAfter=*/false);

    rB.SelectFromToEnd(pFirst);
    rB.WrapInColumnSection(SwResId(STR_WRITER2027_SECTION_TWO_COLUMN), 2);
    rB.SetFocus(pFirst, 0, aColText.getLength());
}

/// Callout: real heading (Heading 3) + accented body paragraph.
void lcl_BuildCallout(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_HEADLINE3),
                      SwResId(STR_WRITER2027_PLACEHOLDER_CALLOUT_HEADING), /*bSplitAfter=*/true);
    rB.WriteFocusParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::Callout, rB.mbDark),
                           SwResId(STR_WRITER2027_PLACEHOLDER_CALLOUT_BODY),
                           /*bSplitAfter=*/false);
}

/// Research Note: muted label, title, body, source/reference line.
void lcl_BuildResearchNote(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_RESEARCH_LABEL), /*bSplitAfter=*/true);
    rB.WriteFocusParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_HEADLINE3),
                           SwResId(STR_WRITER2027_PLACEHOLDER_RESEARCH_TITLE),
                           /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::ResearchNote, rB.mbDark),
                      SwResId(STR_WRITER2027_PLACEHOLDER_RESEARCH_BODY), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_RESEARCH_SOURCE), /*bSplitAfter=*/false);
}

/// Code / Technical: caption-like title row + monospace body via the Type
/// System mono role (Preformatted Text).
void lcl_BuildCode(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.EnsureFreshParagraph();

    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_LABEL),
                      SwResId(STR_WRITER2027_PLACEHOLDER_CODE_TITLE), /*bSplitAfter=*/true);
    rB.WriteFocusParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::Code, rB.mbDark),
                           SwResId(STR_WRITER2027_PLACEHOLDER_CODE_BODY), /*bSplitAfter=*/false);
}

/// Closing Statement: large final statement + small supporting line, on a
/// fresh page.
void lcl_BuildClosingStatement(BlockBuilder& rB)
{
    SwDoc& rDoc = rB.mrDoc;
    rB.InsertLeadingPageBreak();
    rB.EnsureFreshParagraph();

    rB.WriteFocusParagraph(lcl_EnsureBlockStyle(rDoc, BlockStyleId::ClosingStatement, rB.mbDark),
                           SwResId(STR_WRITER2027_PLACEHOLDER_CLOSING), /*bSplitAfter=*/true);
    rB.WriteParagraph(lcl_PoolColl(rDoc, SwPoolFormatId::COLL_DOC_SUBTITLE),
                      SwResId(STR_WRITER2027_PLACEHOLDER_CLOSING_LINE), /*bSplitAfter=*/false);
}

} // namespace

bool InsertEditorialBlock(SwWrtShell& rSh,
                          const svx::writer2027::EditorialBlockDefinition& rBlock,
                          const FontList* /*pFontList*/)
{
    if (!rSh.CanInsert())
        return false;

    BlockBuilder aBuilder(rSh);
    if (!aBuilder.Start())
        return false;

    if (rBlock.maId == u"hero"_ustr)
        lcl_BuildHero(aBuilder);
    else if (rBlock.maId == u"section-intro"_ustr)
        lcl_BuildSectionIntro(aBuilder);
    else if (rBlock.maId == u"pull-quote"_ustr)
        lcl_BuildPullQuote(aBuilder);
    else if (rBlock.maId == u"key-stat"_ustr)
        lcl_BuildKeyStat(aBuilder);
    else if (rBlock.maId == u"image-caption"_ustr)
        lcl_BuildImageCaption(aBuilder);
    else if (rBlock.maId == u"two-column"_ustr)
        lcl_BuildTwoColumn(aBuilder);
    else if (rBlock.maId == u"callout"_ustr)
        lcl_BuildCallout(aBuilder);
    else if (rBlock.maId == u"research-note"_ustr)
        lcl_BuildResearchNote(aBuilder);
    else if (rBlock.maId == u"code"_ustr)
        lcl_BuildCode(aBuilder);
    else if (rBlock.maId == u"closing-statement"_ustr)
        lcl_BuildClosingStatement(aBuilder);
    else
    {
        aBuilder.mrSh.EndUndo(SwUndoId::WRITER2027_BLOCK_INSERT, nullptr);
        aBuilder.mrSh.EndAllAction();
        return false;
    }

    aBuilder.Finish();
    return true;
}

} // namespace sw::writer2027blocks

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */