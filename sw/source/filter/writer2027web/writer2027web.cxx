/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027web.hxx>

#include <com/sun/star/document/XDocumentProperties.hpp>
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <comphelper/base64.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <osl/file.hxx>
#include <rtl/math.hxx>
#include <rtl/ustrbuf.hxx>
#include <svx/swframetypes.hxx>
#include <tools/color.hxx>
#include <tools/stream.hxx>
#include <unicode/uchar.h>
#include <vcl/bitmap.hxx>
#include <vcl/embeddedfontsmanager.hxx>
#include <vcl/gfxlink.hxx>
#include <vcl/graphicfilter.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>

#include <editeng/brushitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/langitem.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/wghtitem.hxx>

#include <IDocumentStylePoolAccess.hxx>
#include <charatr.hxx>
#include <doc.hxx>
#include <docsh.hxx>
#include <docufld.hxx>
#include <fldbas.hxx>
#include <fmtanchr.hxx>
#include <fmtclds.hxx>
#include <fmtflcnt.hxx>
#include <fmtfsize.hxx>
#include <fmtftn.hxx>
#include <fmtinfmt.hxx>
#include <fmtrfmrk.hxx>
#include <frameformats.hxx>
#include <frmatr.hxx>
#include <frmfmt.hxx>
#include <hintids.hxx>
#include <ndarr.hxx>
#include <ndgrf.hxx>
#include <ndhints.hxx>
#include <ndtxt.hxx>
#include <node.hxx>
#include <numrule.hxx>
#include <poolfmt.hxx>
#include <reffld.hxx>
#include <reffldsubtype.hxx>
#include <section.hxx>
#include <strings.hrc>
#include <swtypes.hxx>
#include <tox.hxx>
#include <fmtcol.hxx>
#include <txtftn.hxx>
#include <txatbase.hxx>
#include <writer2027capabilities.hxx>

#include <algorithm>
#include <map>
#include <vector>

namespace sw::writer2027web
{

namespace
{

constexpr OUStringLiteral HTML_GENERATOR = u"Writer 2027 Web Publication";
constexpr sal_Int64 MAX_EMBED_IMAGE_BYTES = 256 * 1024; // 256 KiB data-URI threshold
constexpr sal_Int32 MAX_HEADING_LEVEL = 6;

// ---------------------------------------------------------------------------
// Small string + color helpers
// ---------------------------------------------------------------------------

OUString lcl_Escape(std::u16string_view rText)
{
    OUStringBuffer aBuf(rText.size());
    for (sal_Int32 i = 0; i < static_cast<sal_Int32>(rText.size()); ++i)
    {
        const sal_Unicode c = rText[i];
        switch (c)
        {
            case '&':
                aBuf.append(u"&amp;"_ustr);
                break;
            case '<':
                aBuf.append(u"&lt;"_ustr);
                break;
            case '>':
                aBuf.append(u"&gt;"_ustr);
                break;
            case '"':
                aBuf.append(u"&quot;"_ustr);
                break;
            case '\'':
                aBuf.append(u"&#39;"_ustr);
                break;
            case 0xA0:
                aBuf.append(u"&nbsp;"_ustr);
                break;
            case 0x0A:
                aBuf.append(u"<br>"_ustr);
                break;
            case 0x09:
                aBuf.append(u"\u00A0\u00A0\u00A0\u00A0"_ustr);
                break;
            default:
                aBuf.append(c);
                break;
        }
    }
    return aBuf.makeStringAndClear();
}

OUString lcl_HexByte(sal_uInt8 n)
{
    OUString a = OUString::number(n, 16).toAsciiUpperCase();
    while (a.getLength() < 2)
        a = u"0"_ustr + a;
    return a;
}

OUString lcl_ToCssColor(const Color& rColor)
{
    return u"#"_ustr + lcl_HexByte(rColor.GetRed()) + lcl_HexByte(rColor.GetGreen())
           + lcl_HexByte(rColor.GetBlue());
}

/// Blend rA (base) with rB; nPercentB is the share of rB (0..255).
Color lcl_Blend(const Color& rA, const Color& rB, sal_uInt8 nPercentB)
{
    const sal_uInt16 nB = nPercentB;
    const sal_uInt16 nA = 255 - nB;
    return Color(static_cast<sal_uInt8>((rA.GetRed() * nA + rB.GetRed() * nB) / 255),
                 static_cast<sal_uInt8>((rA.GetGreen() * nA + rB.GetGreen() * nB) / 255),
                 static_cast<sal_uInt8>((rA.GetBlue() * nA + rB.GetBlue() * nB) / 255));
}

/// Relative luminance (0..255-ish): used to decide dark-first vs light.
sal_uInt16 lcl_Luminance(const Color& rColor)
{
    return static_cast<sal_uInt16>((rColor.GetRed() * 299 + rColor.GetGreen() * 587
                                    + rColor.GetBlue() * 114)
                                   / 1000);
}

/// CSS string literal escaping (defined below; used by lcl_FontStack).
OUString lcl_CssString(const OUString& rText);

OUString lcl_FontStack(const OUString& rFamily, FontFamily eGeneric)
{
    OUStringBuffer aStack;
    if (!rFamily.isEmpty())
        aStack.append(u"\"").append(lcl_CssString(rFamily)).append(u"\", ");
    switch (eGeneric)
    {
        case FAMILY_ROMAN:
            aStack.append(u"Georgia, \"Times New Roman\", serif"_ustr);
            break;
        case FAMILY_MODERN:
            aStack.append(u"ui-monospace, \"Cascadia Mono\", Consolas, monospace"_ustr);
            break;
        case FAMILY_SWISS:
        default:
            aStack.append(u"system-ui, \"Segoe UI\", Roboto, sans-serif"_ustr);
            break;
    }
    return aStack.makeStringAndClear();
}

OUString lcl_TwipsToRem(sal_uInt16 nTwips)
{
    // 1 rem = 16 px = 320 twips.
    return rtl::math::doubleToUString(static_cast<double>(nTwips) / 320.0,
                                      rtl_math_StringFormat_Automatic,
                                      rtl_math_DecimalPlaces(2), '.')
           + u"rem"_ustr;
}

OUString lcl_Pad3(sal_Int32 n)
{
    OUString a = OUString::number(n);
    while (a.getLength() < 3)
        a = u"0"_ustr + a;
    return a;
}

bool lcl_IsW27Style(const SwTextFormatColl& rColl, TranslateId rResId)
{
    return rColl.GetName().toString() == SwResId(rResId);
}

// ---------------------------------------------------------------------------
// @font-face embedding
//
// Fonts used by the document (body/heading/display/mono plus custom styles)
// are resolved to physical font files through EmbeddedFontsManager - the same
// helper the DOCX and ODF exporters use - and shipped next to the generated
// HTML: as sibling asset files for the Web Package, as base64 data URIs for
// the single-HTML variant. The CSS variable stacks already start with the
// document family names, so a matching @font-face makes the browser render
// with the embedded face and keep the platform fallback after it.
// ---------------------------------------------------------------------------

/// A font family used by the document (collects the style-relevant attributes
/// needed to resolve the physical faces).
struct FontFamilyUse
{
    OUString maFamily;
    FontFamily meGeneric = FAMILY_SWISS;
    FontPitch mePitch = PITCH_DONTKNOW;
};

/// A single physical face that will be embedded (one @font-face declaration).
struct FontFaceUse
{
    OUString maFamily;  // CSS family name (the document's family name)
    OUString maUrl;     // source file:// URL of the physical font file
    OUString maExt;     // ".ttf", ".otf", ...
    OUString maFormat;  // CSS format token ("truetype", "opentype", ...)
    OUString maMime;    // data-URI mime type (Single HTML variant)
    FontWeight meWeight = WEIGHT_NORMAL;
    FontItalic meItalic = ITALIC_NONE;
};

/// Deduplicated font family collection (case-insensitive on the family name).
void lcl_CollectFont(std::vector<FontFamilyUse>& rFamilies, const OUString& rFamily,
                     FontFamily eGeneric, FontPitch ePitch)
{
    if (rFamily.isEmpty())
        return;
    for (const auto& rUse : rFamilies)
        if (rUse.maFamily.equalsIgnoreAsciiCase(rFamily))
            return;
    rFamilies.push_back({ rFamily, eGeneric, ePitch });
}

/// Fills the CSS format token and data-URI mime for a font file URL. Returns
/// false for formats browsers cannot use in @font-face (e.g. TTC collections).
bool lcl_FontFileFormat(const OUString& rUrl, OUString& rExt, OUString& rFormat, OUString& rMime)
{
    if (rUrl.endsWithIgnoreAsciiCase(u".woff2"_ustr))
    {
        rExt = u".woff2"_ustr;
        rFormat = u"woff2"_ustr;
        rMime = u"font/woff2"_ustr;
        return true;
    }
    if (rUrl.endsWithIgnoreAsciiCase(u".woff"_ustr))
    {
        rExt = u".woff"_ustr;
        rFormat = u"woff"_ustr;
        rMime = u"font/woff"_ustr;
        return true;
    }
    if (rUrl.endsWithIgnoreAsciiCase(u".ttf"_ustr))
    {
        rExt = u".ttf"_ustr;
        rFormat = u"truetype"_ustr;
        rMime = u"font/ttf"_ustr;
        return true;
    }
    if (rUrl.endsWithIgnoreAsciiCase(u".otf"_ustr))
    {
        rExt = u".otf"_ustr;
        rFormat = u"opentype"_ustr;
        rMime = u"font/otf"_ustr;
        return true;
    }
    return false;
}

/// Reads a whole file (file:// URL) into rOut. Returns false on failure.
bool lcl_ReadFileBytes(const OUString& rUrl, std::vector<sal_uInt8>& rOut)
{
    SvFileStream aIn(rUrl, StreamMode::READ);
    if (!aIn.IsOpen())
        return false;
    const sal_uInt64 nSize = aIn.remainingSize();
    rOut.resize(nSize);
    if (nSize > 0)
        aIn.ReadBytes(rOut.data(), nSize);
    return aIn.GetError() == ERRCODE_NONE;
}

/// CSS string literal escaping (font-family names inside @font-face).
OUString lcl_CssString(const OUString& rText)
{
    OUStringBuffer aBuf;
    for (sal_Int32 i = 0; i < rText.getLength(); ++i)
    {
        const sal_Unicode c = rText[i];
        switch (c)
        {
            case '\\':
                aBuf.append(u"\\\\"_ustr);
                break;
            case '"':
                aBuf.append(u"\\\""_ustr);
                break;
            default:
                aBuf.append(c);
                break;
        }
    }
    return aBuf.makeStringAndClear();
}

// ---------------------------------------------------------------------------
// Phase 9 helpers: ids, URLs, JSON, image formats
// ---------------------------------------------------------------------------

/// Unicode-aware, URL-safe, deterministic slug for heading ids. Letters and
/// digits (any script) are kept, everything else collapses to a single '-';
/// the result is lower-cased and never empty.
OUString lcl_Slugify(const OUString& rText)
{
    OUStringBuffer aBuf;
    bool bLastDash = false;
    for (sal_Int32 i = 0; i < rText.getLength(); ++i)
    {
        const sal_Unicode c = rText[i];
        if (u_isalnum(c))
        {
            const sal_Unicode cLow = static_cast<sal_Unicode>(u_tolower(c));
            aBuf.append(cLow);
            bLastDash = false;
        }
        else if (!bLastDash)
        {
            aBuf.append('-');
            bLastDash = true;
        }
    }
    OUString aSlug = aBuf.makeStringAndClear();
    while (aSlug.startsWith("-"))
        aSlug = aSlug.copy(1);
    while (aSlug.endsWith("-"))
        aSlug = aSlug.copy(0, aSlug.getLength() - 1);
    return aSlug;
}

/// URL-scheme policy: allows empty/relative links, fragment-only anchors,
/// root-relative paths and the http(s)/mailto/tel schemes. Everything else
/// (javascript:, vbscript:, data:text/html, ...) is rejected by returning an
/// empty string so the caller can drop the link and keep the visible text.
///
/// Shared pure helper: sw::writer2027capabilities::SafeHref (Phase 10).

/// JSON string literal escaping (publication.json).
OUString lcl_JsonString(const OUString& rText)
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

/// Maps a native GfxLink type to a browser-usable asset (extension + mime).
/// Returns false for formats that must not be shipped to a web page as-is
/// (TIFF, WMF/EMF, EPS, PDF, MOV, ...).
///
/// Shared pure helper: sw::writer2027capabilities::GfxTypeToWeb (Phase 10).

/// Conservative SVG safety check: rejects scripts, event handlers,
/// foreignObject and external references so no ad-hoc sanitizer is needed.
///
/// Shared pure helper: sw::writer2027capabilities::SvgIsSafe (Phase 10).

// ---------------------------------------------------------------------------
// Resolved document style info (drives the CSS variables)
// ---------------------------------------------------------------------------

struct DocumentStyle
{
    Color maBg = Color(0xFF, 0xFF, 0xFF);
    Color maText = Color(0x1A, 0x1A, 0x1A);
    Color maSecondary;
    Color maHairline;
    Color maAccent;
    Color maCodeBg;
    OUString maBodyFont;
    FontFamily meBodyGeneric = FAMILY_SWISS;
    OUString maHeadingFont;
    FontFamily meHeadingGeneric = FAMILY_SWISS;
    OUString maDisplayFont;
    FontFamily meDisplayGeneric = FAMILY_SWISS;
    OUString maMonoFont;
    FontFamily meMonoGeneric = FAMILY_MODERN;
    sal_uInt16 mnBodySize = 240; // twips (12 pt)
    sal_uInt16 mnHeadingSize = 480; // twips (24 pt)
    sal_uInt16 mnTitleSize = 640; // twips (32 pt)
    double mfLineHeight = 1.5;
    bool mbDark = false;

    void Finalize()
    {
        mbDark = lcl_Luminance(maBg) < 128;
        if (mbDark)
        {
            maSecondary = lcl_Blend(maText, maBg, 140);
            maHairline = lcl_Blend(maText, maBg, 220);
            maCodeBg = lcl_Blend(maBg, maText, 40);
        }
        else
        {
            maSecondary = lcl_Blend(maText, maBg, 140);
            maHairline = lcl_Blend(maText, maBg, 200);
            maCodeBg = lcl_Blend(maBg, maText, 25);
        }
        if (maAccent == Color(0, 0, 0))
            maAccent = mbDark ? Color(0x4F, 0xC3, 0xF7) : Color(0x0B, 0x6E, 0x99);
    }
};

// ---------------------------------------------------------------------------
// Phase 9: image assets, manifest, navigation
// ---------------------------------------------------------------------------

/// Result of exporting one image: base src plus optional responsive srcset.
struct ImageAsset
{
    OUString maSrc;    // base src (relative path or data URI)
    OUString maSrcset; // responsive srcset attribute content (may be empty)
    OUString maSizes;  // sizes attribute (may be empty)
};

/// One entry of publication.json.
struct ManifestAsset
{
    OUString maPath;      // relative, portable, '/'-separated
    OUString maMediaType; // e.g. "text/html"
    OUString maRole;      // "entry" | "stylesheet" | "image" | "font"
    sal_Int32 mnWidth = 0;
    sal_Int32 mnHeight = 0;
};

/// One outline heading collected for the generated navigation.
struct NavItem
{
    int mnLevel = 1;
    OUString maId;
    OUString maText;
};

// ---------------------------------------------------------------------------
// The exporter
// ---------------------------------------------------------------------------

class WebExporter
{
public:
    WebExporter(SwDoc& rDoc, const WebPublishOptions& rOptions, const OUString& rOutputDirUrl)
        : mrDoc(rDoc)
        , maOptions(rOptions)
        , maOutputDirUrl(rOutputDirUrl)
    {
    }

    WebPublishResult Run();

private:
    // ---- helpers ----
    void ScanForTitle();
    void ResolveDocumentStyle();
    void CollectMetadata();

    void WriteNodes(SwNodeOffset nStart, SwNodeOffset nEnd);
    void WriteTextNode(const SwTextNode& rNode);
    void WriteParagraph(const SwTextNode& rNode, const OUString& rTag, const OUString& rClass,
                        bool bPre, bool bCloseable);
    void WriteRuns(const SwTextNode& rNode, sal_Int32 nFrom, sal_Int32 nTo);
    void WriteTable(const SwTableNode& rNode);
    void WriteTableBox(const SwTableBox& rBox);
    void WriteGraphicNode(const SwGrfNode& rNode);
    void WriteFlyFrame(const SwFrameFormat& rFly, bool bInline);
    void WriteFrameContent(const SwFrameFormat& rFly, bool bInline);
    ImageAsset ExportImage(const Graphic& rGraphic);
    void WriteFootnote(const SwTextFootnote& rFootnote);
    void WriteNoteSection();

    void ClosePendingFigure();
    void CloseOpenList();
    void CloseHeroGroup();
    void CloseStatGroup();
    void CloseClosingGroup();

    // returns true when the paragraph belongs to the open hero group
    bool IsCaptionParagraph(const SwTextNode& rNode) const;

    /// Emit <td>..</td> for a cell (no row/colspan in this phase).
    void WriteCellText(const SwNode& rStart);

    OUString MakeCss();
    bool WriteTextFile(const OUString& rUrl, const OUString& rContent);

    // ---- @font-face embedding ----
    void CollectFont(const OUString& rFamily, FontFamily eGeneric, FontPitch ePitch);
    void BuildFontFaces();
    bool WriteFontFile(const OUString& rRelPath, const std::vector<sal_uInt8>& rData);

    // ---- Phase 9: ids / references ----
    void CollectBookmarks();
    OUString ClaimId(OUString rBase);
    void RegisterHeadingTarget(const OUString& rText, const OUString& rId);
    OUString ResolveTarget(const OUString& rName) const;

    // ---- Phase 9: anchored frames ----
    void CollectAnchoredFrames();
    void WriteAnchoredFrames(SwNodeOffset nNodeIndex);

    // ---- Phase 9: assets / manifest ----
    bool WriteAssetFile(const OUString& rRelPath, const std::vector<sal_uInt8>& rData);
    bool WriteScaledBitmap(const Bitmap& rBmp, bool bJpg, const OUString& rRelPath);
    void AddAsset(const OUString& rRelPath, const OUString& rMediaType, const OUString& rRole,
                  sal_Int32 nWidth = 0, sal_Int32 nHeight = 0);

    // ---- Phase 9: toc / indexes / generated nav ----
    void WriteTocSection(const SwSectionNode& rSecNode, const SwTOXBase& rTOX);
    void WriteIndexSection(const SwSectionNode& rSecNode, const SwTOXBase& rTOX);
    void WriteTocEntry(const SwTextNode& rNode);
    void BuildGeneratedNav();
    void WriteManifest();

    // ---- state ----
    SwDoc& mrDoc;
    WebPublishOptions maOptions;
    OUString maOutputDirUrl;

    OUStringBuffer maBody;
    OUStringBuffer maCssExtra; // custom style rules collected during traversal
    OUStringBuffer maNotes;

    DocumentStyle maStyle;
    bool mbHasTitle = false;

    // deterministic id counters
    sal_Int32 mnImageSeq = 0;
    sal_Int32 mnSectionSeq = 0;
    sal_Int32 mnHeadingSeq = 0;
    sal_Int32 mnNoteSeq = 0;

    // figure / group state
    bool mbFigurePending = false;
    bool mbInHero = false;
    bool mbInStat = false;
    bool mbInClosing = false;

    // list state
    SwNumRule const* mpOpenListRule = nullptr;
    int mnOpenListLevel = -1;
    bool mbOpenListOrdered = false;

    // metadata
    OUString maTitle;
    OUString maAuthor;
    OUString maSubject;
    OUString maKeywords;
    OUString maLang;

    // custom style CSS: class name -> declarations
    std::map<OUString, OUString> maParaStyleCss;
    std::map<OUString, OUString> maCharStyleCss;

    // @font-face embedding state
    std::vector<FontFamilyUse> maFontFamilies;
    std::vector<FontFaceUse> maFontFaces;
    std::map<OUString, OUString> maFontUrlToSrc; // source url -> css src payload
    OUStringBuffer maFontFaceCss;
    sal_Int32 mnFontSeq = 0;

    // Phase 9 state
    std::set<OUString> maUsedIds;                       // claimed ids (duplicate guard)
    std::map<OUString, OUString> maHeadingTargets;      // heading text (lower) and slug -> id
    std::map<SwNodeOffset, std::vector<sw::SpzFrameFormat*>> maAnchoredFrames;
    std::vector<NavItem> maNavItems;
    OUString maGeneratedNav;
    std::vector<ManifestAsset> maManifestAssets;
    std::map<BitmapChecksum, ImageAsset> maImageCache;  // dedupe reused graphics
    bool mbExportedToc = false;
};

// ---------------------------------------------------------------------------

void WebExporter::ScanForTitle()
{
    const SwNodes& rNodes = mrDoc.GetNodes();
    for (SwNodeOffset n = SwNodeOffset(1); n < rNodes.Count() - 1; ++n)
    {
        const SwTextNode* pText = rNodes[n]->GetTextNode();
        if (!pText)
            continue;
        const SwTextFormatColl* pColl = pText->GetTextColl();
        if (pColl && pColl->GetPoolFormatId() == SwPoolFormatId::COLL_DOC_TITLE)
        {
            mbHasTitle = true;
            return;
        }
    }
}

void WebExporter::ResolveDocumentStyle()
{
    auto& rPool = mrDoc.getIDocumentStylePoolAccess();

    // Background: page style master.
    const SwPageDesc& rPageDesc = mrDoc.GetPageDesc(0);
    const SvxBrushItem* pBrush
        = rPageDesc.GetMaster().GetAttrSet().GetItemIfSet(RES_BACKGROUND, false);
    if (pBrush && pBrush->GetColor() != COL_TRANSPARENT)
        maStyle.maBg = pBrush->GetColor();

    // Text: Standard paragraph style color + font + size (effective values,
    // i.e. the real current typography even when Custom).
    if (SwTextFormatColl* pStandard = rPool.GetTextCollFromPool(SwPoolFormatId::COLL_STANDARD))
    {
        const SwAttrSet& rSet = pStandard->GetAttrSet();
        const SvxColorItem& rColor = static_cast<const SvxColorItem&>(rSet.Get(RES_CHRATR_COLOR));
        if (rColor.GetValue() != COL_AUTO)
            maStyle.maText = rColor.GetValue();
        const SvxFontItem& rFont = static_cast<const SvxFontItem&>(rSet.Get(RES_CHRATR_FONT));
        if (!rFont.GetFamilyName().isEmpty())
        {
            maStyle.maBodyFont = rFont.GetFamilyName();
            maStyle.meBodyGeneric = rFont.GetFamily();
            CollectFont(rFont.GetFamilyName(), rFont.GetFamily(), rFont.GetPitch());
        }
        const SvxFontHeightItem& rSize
            = static_cast<const SvxFontHeightItem&>(rSet.Get(RES_CHRATR_FONTSIZE));
        maStyle.mnBodySize = rSize.GetHeight();
        const SvxLineSpacingItem& rSpacing
            = static_cast<const SvxLineSpacingItem&>(rSet.Get(RES_PARATR_LINESPACING));
        if (rSpacing.GetInterLineSpaceRule() == SvxInterLineSpaceRule::Prop
            && rSpacing.GetPropLineSpace() > 0)
            maStyle.mfLineHeight = static_cast<double>(rSpacing.GetPropLineSpace()) / 100.0;
    }

    // Headings: Heading 1 font/size.
    if (SwTextFormatColl* pHeading = rPool.GetTextCollFromPool(SwPoolFormatId::COLL_HEADLINE1))
    {
        const SwAttrSet& rSet = pHeading->GetAttrSet();
        const SvxFontItem& rFont = static_cast<const SvxFontItem&>(rSet.Get(RES_CHRATR_FONT));
        if (!rFont.GetFamilyName().isEmpty())
        {
            maStyle.maHeadingFont = rFont.GetFamilyName();
            maStyle.meHeadingGeneric = rFont.GetFamily();
            CollectFont(rFont.GetFamilyName(), rFont.GetFamily(), rFont.GetPitch());
        }
        const SvxFontHeightItem& rSize
            = static_cast<const SvxFontHeightItem&>(rSet.Get(RES_CHRATR_FONTSIZE));
        maStyle.mnHeadingSize = rSize.GetHeight();
    }

    // Display: Title font/size.
    if (SwTextFormatColl* pTitle = rPool.GetTextCollFromPool(SwPoolFormatId::COLL_DOC_TITLE))
    {
        const SwAttrSet& rSet = pTitle->GetAttrSet();
        const SvxFontItem& rFont = static_cast<const SvxFontItem&>(rSet.Get(RES_CHRATR_FONT));
        if (!rFont.GetFamilyName().isEmpty())
        {
            maStyle.maDisplayFont = rFont.GetFamilyName();
            maStyle.meDisplayGeneric = rFont.GetFamily();
            CollectFont(rFont.GetFamilyName(), rFont.GetFamily(), rFont.GetPitch());
        }
        const SvxFontHeightItem& rSize
            = static_cast<const SvxFontHeightItem&>(rSet.Get(RES_CHRATR_FONTSIZE));
        maStyle.mnTitleSize = rSize.GetHeight();
    }

    // Mono: Preformatted Text font.
    if (SwTextFormatColl* pPre = rPool.GetTextCollFromPool(SwPoolFormatId::COLL_HTML_PRE))
    {
        const SwAttrSet& rSet = pPre->GetAttrSet();
        const SvxFontItem& rFont = static_cast<const SvxFontItem&>(rSet.Get(RES_CHRATR_FONT));
        if (!rFont.GetFamilyName().isEmpty())
        {
            maStyle.maMonoFont = rFont.GetFamilyName();
            maStyle.meMonoGeneric = rFont.GetFamily();
            CollectFont(rFont.GetFamilyName(), rFont.GetFamily(), rFont.GetPitch());
        }
    }

    // Accent: Internet Link character style color.
    if (SwCharFormat* pLink = rPool.GetCharFormatFromPool(SwPoolFormatId::CHR_INET_NORMAL))
    {
        const SvxColorItem* pColor = pLink->GetItemIfSet(RES_CHRATR_COLOR, false);
        if (pColor)
            maStyle.maAccent = pColor->GetValue();
    }

    maStyle.Finalize();
}

void WebExporter::CollectMetadata()
{
    SwDocShell* pShell = mrDoc.GetDocShell();
    if (!pShell)
        return;
    try
    {
        uno::Reference<document::XDocumentPropertiesSupplier> xDPS(
            pShell->GetModel(), uno::UNO_QUERY);
        if (!xDPS.is())
            return;
        uno::Reference<document::XDocumentProperties> xProps = xDPS->getDocumentProperties();
        maTitle = xProps->getTitle();
        maAuthor = xProps->getAuthor();
        maSubject = xProps->getSubject();
        const uno::Sequence<OUString> aKeywords = xProps->getKeywords();
        for (const auto& rKeyword : aKeywords)
        {
            if (!maKeywords.isEmpty())
                maKeywords += u", "_ustr;
            maKeywords += rKeyword;
        }
    }
    catch (const uno::Exception&)
    {
        // metadata is best-effort
    }

    // Language: document default character language; fall back to the
    // Standard style language; finally the application locale.
    LanguageType eLang = LANGUAGE_SYSTEM;
    const SvxLanguageItem& rDefault = static_cast<const SvxLanguageItem&>(
        mrDoc.GetDefault(RES_CHRATR_LANGUAGE));
    eLang = rDefault.GetLanguage();
    if (eLang == LANGUAGE_SYSTEM)
    {
        if (SwTextFormatColl* pStandard
            = mrDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(
                SwPoolFormatId::COLL_STANDARD))
        {
            const SvxLanguageItem* pLang = pStandard->GetItemIfSet(RES_CHRATR_LANGUAGE, false);
            if (pLang)
                eLang = pLang->GetLanguage();
        }
    }
    if (eLang == LANGUAGE_SYSTEM)
        eLang = Application::GetSettings().GetLanguageTag().getLanguageType();
    maLang = LanguageTag(eLang).getBcp47();
}

/// Resolve a custom paragraph style's salient properties to CSS declarations.
OUString lcl_ResolveParagraphCss(const SwTextFormatColl& rColl,
                                 std::vector<FontFamilyUse>& rFontFamilies)
{
    OUStringBuffer aDecl;
    auto add = [&aDecl](std::u16string_view rProp, const OUString& rValue) {
        aDecl.append(rProp);
        aDecl.append(u": "_ustr);
        aDecl.append(rValue);
        aDecl.append(u"; "_ustr);
    };
    const SvxFontItem* pFont = rColl.GetItemIfSet(RES_CHRATR_FONT, false);
    if (pFont && !pFont->GetFamilyName().isEmpty())
    {
        add(u"font-family", lcl_FontStack(pFont->GetFamilyName(), pFont->GetFamily()));
        lcl_CollectFont(rFontFamilies, pFont->GetFamilyName(), pFont->GetFamily(),
                        pFont->GetPitch());
    }
    if (const SvxFontHeightItem* pSize = rColl.GetItemIfSet(RES_CHRATR_FONTSIZE, false))
        add(u"font-size", lcl_TwipsToRem(pSize->GetHeight()));
    if (const SvxWeightItem* pWeight = rColl.GetItemIfSet(RES_CHRATR_WEIGHT, false))
    {
        if (pWeight->GetWeight() >= WEIGHT_BOLD)
            add(u"font-weight", u"bold"_ustr);
    }
    if (const SvxPostureItem* pPosture = rColl.GetItemIfSet(RES_CHRATR_POSTURE, false))
    {
        if (pPosture->GetPosture() != ITALIC_NONE)
            add(u"font-style", u"italic"_ustr);
    }
    if (const SvxColorItem* pColor = rColl.GetItemIfSet(RES_CHRATR_COLOR, false))
        add(u"color", lcl_ToCssColor(pColor->GetValue()));
    if (const SvxULSpaceItem* pUL = rColl.GetItemIfSet(RES_UL_SPACE, false))
    {
        add(u"margin-top",
            rtl::math::doubleToUString(static_cast<double>(pUL->GetUpper()) / 320.0,
                                       rtl_math_StringFormat_Automatic,
                                       rtl_math_DecimalPlaces(2), '.')
                + u"rem"_ustr);
        add(u"margin-bottom",
            rtl::math::doubleToUString(static_cast<double>(pUL->GetLower()) / 320.0,
                                       rtl_math_StringFormat_Automatic,
                                       rtl_math_DecimalPlaces(2), '.')
                + u"rem"_ustr);
    }
    if (const SvxBrushItem* pBrush = rColl.GetItemIfSet(RES_BACKGROUND, false))
    {
        if (pBrush->GetColor() != COL_TRANSPARENT)
            add(u"background-color", lcl_ToCssColor(pBrush->GetColor()));
    }
    return aDecl.makeStringAndClear();
}

OUString lcl_ResolveCharCss(const SwCharFormat& rCharFormat,
                            std::vector<FontFamilyUse>& rFontFamilies)
{
    OUStringBuffer aDecl;
    auto add = [&aDecl](std::u16string_view rProp, const OUString& rValue) {
        aDecl.append(rProp);
        aDecl.append(u": "_ustr);
        aDecl.append(rValue);
        aDecl.append(u"; "_ustr);
    };
    const SvxFontItem* pFont = rCharFormat.GetItemIfSet(RES_CHRATR_FONT, false);
    if (pFont && !pFont->GetFamilyName().isEmpty())
    {
        add(u"font-family", lcl_FontStack(pFont->GetFamilyName(), pFont->GetFamily()));
        lcl_CollectFont(rFontFamilies, pFont->GetFamilyName(), pFont->GetFamily(),
                        pFont->GetPitch());
    }
    if (const SvxFontHeightItem* pSize = rCharFormat.GetItemIfSet(RES_CHRATR_FONTSIZE, false))
        add(u"font-size", lcl_TwipsToRem(pSize->GetHeight()));
    if (const SvxWeightItem* pWeight = rCharFormat.GetItemIfSet(RES_CHRATR_WEIGHT, false))
    {
        if (pWeight->GetWeight() >= WEIGHT_BOLD)
            add(u"font-weight", u"bold"_ustr);
    }
    if (const SvxPostureItem* pPosture = rCharFormat.GetItemIfSet(RES_CHRATR_POSTURE, false))
    {
        if (pPosture->GetPosture() != ITALIC_NONE)
            add(u"font-style", u"italic"_ustr);
    }
    if (const SvxColorItem* pColor = rCharFormat.GetItemIfSet(RES_CHRATR_COLOR, false))
        add(u"color", lcl_ToCssColor(pColor->GetValue()));
    return aDecl.makeStringAndClear();
}

// ---------------------------------------------------------------------------
// @font-face embedding
// ---------------------------------------------------------------------------

void WebExporter::CollectFont(const OUString& rFamily, FontFamily eGeneric, FontPitch ePitch)
{
    lcl_CollectFont(maFontFamilies, rFamily, eGeneric, ePitch);
}

bool WebExporter::WriteFontFile(const OUString& rRelPath, const std::vector<sal_uInt8>& rData)
{
    const OUString aOutUrl = maOutputDirUrl + u"/"_ustr + rRelPath;
    SvFileStream aOut(aOutUrl, StreamMode::WRITE | StreamMode::TRUNC);
    if (!aOut.IsOpen())
        return false;
    if (!rData.empty())
        aOut.WriteBytes(rData.data(), rData.size());
    return aOut.GetError() == ERRCODE_NONE;
}

/// Resolves every collected font family to its physical faces (normal/bold x
/// regular/italic), writes the font files (or builds data URIs for the
/// single-HTML variant) and assembles the @font-face CSS block.
void WebExporter::BuildFontFaces()
{
    maFontFaceCss = OUStringBuffer();
    if (!maOptions.mbEmbedFonts || maFontFamilies.empty())
        return;

    const bool bSingle = maOptions.meFormat == WebPublishFormat::SingleHtml;
    if (!bSingle)
        osl::Directory::createPath(maOutputDirUrl + u"/assets/fonts"_ustr);

    // Fonts that are available on every common platform are not embedded -
    // the same policy the DOCX and ODF exporters apply, so the CSS fallback
    // stack after the family name covers them.
    for (const FontFamilyUse& rFamily : maFontFamilies)
    {
        if (EmbeddedFontsManager::isCommonFont(rFamily.maFamily))
            continue;
        for (const FontItalic eItalic : { ITALIC_NONE, ITALIC_NORMAL })
        {
            for (const FontWeight eWeight : { WEIGHT_NORMAL, WEIGHT_BOLD })
            {
                const OUString aUrl = EmbeddedFontsManager::fontFileUrl(
                    rFamily.maFamily, rFamily.meGeneric, eItalic, eWeight, rFamily.mePitch,
                    EmbeddedFontsManager::FontRights::ViewingAllowed);
                if (aUrl.isEmpty())
                    continue;
                OUString aExt, aFormat, aMime;
                if (!lcl_FontFileFormat(aUrl, aExt, aFormat, aMime))
                    continue; // not a web-usable format (e.g. a TTC collection)
                maFontFaces.push_back({ rFamily.maFamily, aUrl, aExt, aFormat, aMime, eWeight,
                                        eItalic });
            }
        }
    }

    for (const FontFaceUse& rFace : maFontFaces)
    {
        OUString aSrc;
        const auto it = maFontUrlToSrc.find(rFace.maUrl);
        if (it != maFontUrlToSrc.end())
        {
            aSrc = it->second;
        }
        else if (bSingle)
        {
            // Single HTML: inline the font bytes as a data URI.
            std::vector<sal_uInt8> aData;
            if (!lcl_ReadFileBytes(rFace.maUrl, aData))
                continue;
            uno::Sequence<sal_Int8> aSeq(static_cast<sal_Int32>(aData.size()));
            std::copy(aData.begin(), aData.end(), aSeq.getArray());
            OStringBuffer aB64;
            comphelper::Base64::encode(aB64, aSeq);
            aSrc = u"data:"_ustr + rFace.maMime + u";base64,"_ustr
                   + OUString::fromUtf8(aB64.makeStringAndClear());
        }
        else
        {
            // Web Package: sibling asset under assets/fonts/. The CSS lives
            // in styles/, so the relative url climbs one level up.
            std::vector<sal_uInt8> aData;
            if (!lcl_ReadFileBytes(rFace.maUrl, aData))
                continue;
            ++mnFontSeq;
            const OUString aRel
                = u"assets/fonts/font-"_ustr + lcl_Pad3(mnFontSeq) + rFace.maExt;
            if (!WriteFontFile(aRel, aData))
                continue;
            AddAsset(aRel, rFace.maMime, u"font"_ustr);
            aSrc = u"../"_ustr + aRel;
        }
        if (aSrc.isEmpty())
            continue;
        maFontUrlToSrc.emplace(rFace.maUrl, aSrc);

        maFontFaceCss.append(u"@font-face {\n"_ustr);
        maFontFaceCss.append(u"  font-family: \"").append(lcl_CssString(rFace.maFamily))
            .append(u"\";\n"_ustr);
        maFontFaceCss.append(u"  font-style: ")
            .append(rFace.meItalic == ITALIC_NONE ? u"normal"_ustr : u"italic"_ustr)
            .append(u";\n"_ustr);
        maFontFaceCss.append(u"  font-weight: ")
            .append(rFace.meWeight >= WEIGHT_BOLD ? u"700"_ustr : u"400"_ustr).append(u";\n"_ustr);
        maFontFaceCss.append(u"  font-display: swap;\n"_ustr);
        maFontFaceCss.append(u"  src: url(\"").append(aSrc).append(u"\") format(\"")
            .append(rFace.maFormat).append(u"\");\n"_ustr);
        maFontFaceCss.append(u"}\n"_ustr);
    }
}

// ---------------------------------------------------------------------------
// Phase 9: ids / references
// ---------------------------------------------------------------------------

/// Pre-claims bookmark ids for every reference mark and link anchor name in
/// the document, so cross-references and TOC entries can resolve determin-
/// istically even when the target appears later in the output.
void WebExporter::CollectBookmarks()
{
    const SwNodes& rNodes = mrDoc.GetNodes();
    for (SwNodeOffset n = SwNodeOffset(1); n < rNodes.Count() - 1; ++n)
    {
        const SwTextNode* pText = rNodes[n]->GetTextNode();
        if (!pText)
            continue;
        const SwpHints* pHints = pText->GetpSwpHints();
        if (!pHints)
            continue;
        for (size_t i = 0; i < pHints->Count(); ++i)
        {
            const SwTextAttr* pH = pHints->Get(i);
            switch (pH->Which())
            {
                case RES_TXTATR_REFMARK:
                {
                    const OUString aName = pH->GetRefMark().GetRefName().toString();
                    if (!aName.isEmpty())
                        maUsedIds.insert(u"bm-"_ustr + SanitizeName(aName));
                    break;
                }
                case RES_TXTATR_INETFMT:
                {
                    const OUString aName = pH->GetINetFormat().GetName();
                    if (!aName.isEmpty())
                        maUsedIds.insert(u"bm-"_ustr + SanitizeName(aName));
                    break;
                }
                default:
                    break;
            }
        }
    }
}

/// Claims a deterministic, collision-safe id: the base is returned as-is on
/// first use, later collisions get "-2", "-3", ... suffixes.
OUString WebExporter::ClaimId(OUString rBase)
{
    if (rBase.isEmpty())
        rBase = u"item"_ustr;
    OUString aId = rBase;
    sal_Int32 n = 2;
    while (!maUsedIds.insert(aId).second)
    {
        aId = rBase + "-" + OUString::number(n);
        ++n;
    }
    return aId;
}

/// Makes a heading reachable by both its plain text and its slug.
void WebExporter::RegisterHeadingTarget(const OUString& rText, const OUString& rId)
{
    const OUString aKey = rText.toAsciiLowerCase().trim();
    if (!aKey.isEmpty())
        maHeadingTargets.emplace(aKey, rId);
    const OUString aSlug = lcl_Slugify(rText);
    if (!aSlug.isEmpty())
        maHeadingTargets.emplace(aSlug, rId);
}

/// Resolves a reference name (bookmark/heading/TOX target) to an exported
/// element id. Returns an empty string when nothing is resolvable so the
/// caller keeps the visible text without a broken link.
OUString WebExporter::ResolveTarget(const OUString& rName) const
{
    OUString aName = rName;
    while (aName.startsWith("#"))
        aName = aName.copy(1);
    aName = aName.trim();
    if (aName.isEmpty())
        return OUString();

    const OUString aLower = aName.toAsciiLowerCase();
    const auto it = maHeadingTargets.find(aLower);
    if (it != maHeadingTargets.end())
        return it->second;
    const OUString aSlug = lcl_Slugify(aName);
    if (!aSlug.isEmpty())
    {
        const auto itSlug = maHeadingTargets.find(aSlug);
        if (itSlug != maHeadingTargets.end())
            return itSlug->second;
    }
    const OUString aBm = u"bm-"_ustr + SanitizeName(aName);
    return maUsedIds.count(aBm) ? aBm : OUString();
}

// ---------------------------------------------------------------------------
// Phase 9: anchored frames
// ---------------------------------------------------------------------------

/// Collects all body-level floating frames (at-paragraph / at-character /
/// at-page) keyed by their anchor node, in deterministic order. As-character
/// frames are exported through their text hints instead.
void WebExporter::CollectAnchoredFrames()
{
    const sw::FrameFormats<sw::SpzFrameFormat*>* pFormats = mrDoc.GetSpzFrameFormats();
    if (!pFormats)
        return;
    for (sw::SpzFrameFormat* pFormat : *pFormats)
    {
        if (!pFormat)
            continue;
        const SwFormatAnchor& rAnchor = pFormat->GetAnchor();
        const RndStdIds eId = rAnchor.GetAnchorId();
        if (eId == RndStdIds::FLY_AS_CHAR || eId == RndStdIds::FLY_AT_FLY)
            continue;
        SwNode* pAnchorNode = rAnchor.GetAnchorNode();
        if (!pAnchorNode)
            continue;
        // Only body-level anchors; frames inside tables/footnotes/frames are
        // exported with their parent content.
        const SwStartNode* pStart = pAnchorNode->GetStartNode();
        if (!pStart || pStart->GetStartNodeType() != SwNormalStartNode)
            continue;
        maAnchoredFrames[pAnchorNode->GetIndex()].push_back(pFormat);
    }
    // Deterministic order per anchor node: character offset first.
    for (auto& rPair : maAnchoredFrames)
    {
        std::sort(rPair.second.begin(), rPair.second.end(),
                  [](const sw::SpzFrameFormat* pA, const sw::SpzFrameFormat* pB) {
                      const sal_Int32 nA
                          = pA->GetAnchor().GetContentAnchor()
                                ? pA->GetAnchor().GetContentAnchor()->GetContentIndex()
                                : 0;
                      const sal_Int32 nB
                          = pB->GetAnchor().GetContentAnchor()
                                ? pB->GetAnchor().GetContentAnchor()->GetContentIndex()
                                : 0;
                      return nA < nB;
                  });
    }
}

/// Emits the frames anchored at nNodeIndex (semantic reading order: near the
/// anchor paragraph, never at absolute desktop coordinates).
void WebExporter::WriteAnchoredFrames(SwNodeOffset nNodeIndex)
{
    const auto it = maAnchoredFrames.find(nNodeIndex);
    if (it == maAnchoredFrames.end())
        return;
    for (sw::SpzFrameFormat* pFormat : it->second)
    {
        if (pFormat)
            WriteFrameContent(*pFormat, /*bInline=*/false);
    }
}

// ---------------------------------------------------------------------------
// Phase 9: assets / manifest
// ---------------------------------------------------------------------------

bool WebExporter::WriteAssetFile(const OUString& rRelPath, const std::vector<sal_uInt8>& rData)
{
    const OUString aOutUrl = maOutputDirUrl + u"/"_ustr + rRelPath;
    SvFileStream aOut(aOutUrl, StreamMode::WRITE | StreamMode::TRUNC);
    if (!aOut.IsOpen())
        return false;
    if (!rData.empty())
        aOut.WriteBytes(rData.data(), rData.size());
    return aOut.GetError() == ERRCODE_NONE;
}

/// Encodes a scaled bitmap as JPEG or PNG and writes it under rRelPath.
bool WebExporter::WriteScaledBitmap(const Bitmap& rBmp, bool bJpg, const OUString& rRelPath)
{
    const Graphic aG(rBmp);
    SvMemoryStream aMem;
    GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
    if (bJpg)
    {
        const sal_uInt16 nFmt = rFilter.GetExportFormatNumberForShortName(u"jpg");
        if (nFmt == GRFILTER_FORMAT_NOTFOUND)
            return false;
        if (rFilter.ExportGraphic(aG, u""_ustr, aMem, nFmt) != ERRCODE_NONE)
            return false;
    }
    else if (rFilter.compressAsPNG(aG, aMem) != ERRCODE_NONE)
    {
        return false;
    }
    std::vector<sal_uInt8> aData(static_cast<size_t>(aMem.GetSize()));
    if (!aData.empty())
    {
        aMem.Seek(0);
        aMem.ReadBytes(aData.data(), aData.size());
    }
    return WriteAssetFile(rRelPath, aData);
}

/// Registers an asset for publication.json. Paths are normalized to use '/'
/// and never start with a separator.
void WebExporter::AddAsset(const OUString& rRelPath, const OUString& rMediaType,
                           const OUString& rRole, sal_Int32 nWidth, sal_Int32 nHeight)
{
    OUString aPath = rRelPath.replace('\\', '/');
    while (aPath.startsWith("/"))
        aPath = aPath.copy(1);
    if (aPath.isEmpty())
        return;
    maManifestAssets.push_back({ aPath, rMediaType, rRole, nWidth, nHeight });
}

// ---------------------------------------------------------------------------
// Phase 9: frame content
// ---------------------------------------------------------------------------

/// Exports one frame's content. bInline selects the as-character-in-text
/// variant (inline <img>/<span> only, so no block element can end up inside
/// a <p>); otherwise a block-level figure/aside is emitted.
void WebExporter::WriteFrameContent(const SwFrameFormat& rFly, bool bInline)
{
    const SwFormatContent& rContent = rFly.GetContent();
    const SwNodeIndex* pIdx = rContent.GetContentIdx();
    if (!pIdx)
        return;

    const SwNodeOffset nEnd = pIdx->GetNode().EndOfSectionIndex();
    bool bHasGraphic = false;
    bool bHasText = false;
    for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
    {
        const SwNode& rNode = *pIdx->GetNodes()[n];
        if (rNode.IsGrfNode())
            bHasGraphic = true;
        else if (const SwTextNode* pText = rNode.GetTextNode())
        {
            if (!pText->GetText().isEmpty() || pText->HasHints())
                bHasText = true;
        }
    }
    if (!bHasGraphic && !bHasText)
        return;

    const SwFlyFrameFormat* pFlyFmt = dynamic_cast<const SwFlyFrameFormat*>(&rFly);
    const OUString aAlt = pFlyFmt ? pFlyFmt->GetObjDescription() : OUString();

    if (bInline)
    {
        // True inline media: never emit block elements inside a <p>.
        if (bHasGraphic && !bHasText)
        {
            for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
            {
                if (const SwGrfNode* pGrf = pIdx->GetNodes()[n]->GetGrfNode())
                {
                    const Graphic& rGraphic = pGrf->GetGrfObj().GetGraphic();
                    if (rGraphic.GetType() == GraphicType::NONE)
                        continue;
                    const ImageAsset aImg = ExportImage(rGraphic);
                    if (aImg.maSrc.isEmpty())
                        continue;
                    maBody.append(u"<img class=\"w27-inline-image\" src=\""_ustr);
                    maBody.append(aImg.maSrc);
                    maBody.append(u"\" alt=\""_ustr);
                    maBody.append(lcl_Escape(aAlt));
                    maBody.append(u"\">"_ustr);
                }
            }
            return;
        }
        // Text-bearing frame inside text: inline span with the plain text.
        maBody.append(u"<span class=\"w27-frame-inline\">"_ustr);
        for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
        {
            if (const SwTextNode* pText = pIdx->GetNodes()[n]->GetTextNode())
                maBody.append(lcl_Escape(pText->GetText()));
        }
        maBody.append(u"</span>"_ustr);
        return;
    }

    // Block-level frame.
    if (bHasGraphic)
    {
        maBody.append(u"<figure class=\"w27-image\">"_ustr);
        for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
        {
            const SwNode& rNode = *pIdx->GetNodes()[n];
            if (const SwGrfNode* pGrf = rNode.GetGrfNode())
            {
                const Graphic& rGraphic = pGrf->GetGrfObj().GetGraphic();
                if (rGraphic.GetType() == GraphicType::NONE)
                    continue;
                const ImageAsset aImg = ExportImage(rGraphic);
                if (aImg.maSrc.isEmpty())
                    continue;
                maBody.append(u"<img src=\""_ustr);
                maBody.append(aImg.maSrc);
                maBody.append(u"\" alt=\""_ustr);
                maBody.append(lcl_Escape(aAlt));
                maBody.append(u"\">"_ustr);
            }
            else if (const SwTextNode* pText = rNode.GetTextNode())
            {
                if (!pText->GetText().isEmpty() || pText->HasHints())
                    WriteParagraph(*pText, u"p"_ustr, u"w27-image-placeholder"_ustr, false, true);
            }
            else if (rNode.IsTableNode())
            {
                WriteTable(*rNode.GetTableNode());
            }
        }
        // Leave the figure open: a directly following Caption paragraph
        // becomes the <figcaption> (closed in WriteTextNode).
        mbFigurePending = true;
        return;
    }

    // Text-bearing frame: semantic container.
    maBody.append(u"<aside class=\"w27-frame\">"_ustr);
    for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
    {
        const SwNode& rNode = *pIdx->GetNodes()[n];
        if (const SwTextNode* pText = rNode.GetTextNode())
        {
            if (!pText->GetText().isEmpty() || pText->HasHints())
                WriteParagraph(*pText, u"p"_ustr, u"w27-frame-para"_ustr, false, true);
        }
        else if (rNode.IsTableNode())
        {
            WriteTable(*rNode.GetTableNode());
        }
    }
    maBody.append(u"</aside>"_ustr);
}

void WebExporter::WriteFlyFrame(const SwFrameFormat& rFly, bool bInline)
{
    WriteFrameContent(rFly, bInline);
}

// ---------------------------------------------------------------------------
// Phase 9: image export
// ---------------------------------------------------------------------------

ImageAsset WebExporter::ExportImage(const Graphic& rGraphic)
{
    ++mnImageSeq;
    ImageAsset aAsset;

    // Identical reused graphics export to the same files (dedupe).
    const BitmapChecksum aChecksum = rGraphic.GetChecksum();
    const auto itCached = maImageCache.find(aChecksum);
    if (itCached != maImageCache.end())
        return itCached->second;

    const bool bSingle = maOptions.meFormat == WebPublishFormat::SingleHtml;

    // 1. Source-format-preserving bytes (original native data when safe).
    OUString aExt, aMime;
    std::vector<sal_uInt8> aBytes;
    const GfxLink& rLink = rGraphic.GetGfxLink();
    if (rLink.IsNative()
        && sw::writer2027capabilities::GfxTypeToWeb(rLink.GetType(), aExt, aMime))
    {
        const sal_uInt8* pData = rLink.GetData();
        const sal_uInt32 nSize = rLink.GetDataSize();
        if (pData && nSize > 0)
            aBytes.assign(pData, pData + nSize);
        if (aExt == u".svg"_ustr && (aBytes.empty() || !sw::writer2027capabilities::SvgIsSafe(aBytes)))
            aBytes.clear(); // unsafe SVG: fall back to raster, not raw embed
    }

    // 2. Raster fallback (canonical safe PNG conversion).
    Size aPix;
    Bitmap aBmp;
    bool bRaster = false;
    if (aBytes.empty() && rGraphic.GetType() == GraphicType::Bitmap)
    {
        aBmp = rGraphic.GetBitmap();
        if (!aBmp.IsEmpty())
        {
            SvMemoryStream aMem;
            if (GraphicFilter::GetGraphicFilter().compressAsPNG(rGraphic, aMem) == ERRCODE_NONE)
            {
                const sal_uInt64 nSize = aMem.GetSize();
                aBytes.resize(nSize);
                aMem.Seek(0);
                if (nSize > 0)
                    aMem.ReadBytes(aBytes.data(), nSize);
                aExt = u".png"_ustr;
                aMime = u"image/png"_ustr;
                bRaster = true;
            }
        }
    }
    if (aBytes.empty())
    {
        // Nothing embeddable: callers still render the alt text.
        maImageCache.emplace(aChecksum, aAsset);
        return aAsset;
    }
    if (aExt == u".jpg"_ustr || aExt == u".png"_ustr || aExt == u".gif"_ustr)
    {
        bRaster = true;
        aPix = rGraphic.GetSizePixel();
        if (aPix.Width() <= 0 || aPix.Height() <= 0)
        {
            const Bitmap aBmp2 = rGraphic.GetBitmap();
            aPix = aBmp2.GetSizePixel();
        }
    }

    const OUString aBaseName = u"img-"_ustr + lcl_Pad3(mnImageSeq) + aExt;

    if (bSingle)
    {
        // Single HTML stays one file: small images as data URIs, large ones
        // as one best sibling representation (no responsive srcset).
        const bool bEmbed = aBytes.size() <= static_cast<size_t>(MAX_EMBED_IMAGE_BYTES);
        if (bEmbed)
        {
            uno::Sequence<sal_Int8> aSeq(static_cast<sal_Int32>(aBytes.size()));
            std::copy(aBytes.begin(), aBytes.end(), aSeq.getArray());
            OStringBuffer aB64;
            comphelper::Base64::encode(aB64, aSeq);
            aAsset.maSrc = u"data:"_ustr + aMime + u";base64,"_ustr
                           + OUString::fromUtf8(aB64.makeStringAndClear());
        }
        else
        {
            const OUString aRel = u"assets/images/"_ustr + aBaseName;
            osl::Directory::createPath(maOutputDirUrl + u"/assets/images"_ustr);
            if (WriteAssetFile(aRel, aBytes))
                aAsset.maSrc = aRel;
        }
        AddAsset(u"assets/images/"_ustr + aBaseName, aMime, u"image"_ustr, aPix.Width(),
                 aPix.Height());
        maImageCache.emplace(aChecksum, aAsset);
        return aAsset;
    }

    // Web Package.
    osl::Directory::createPath(maOutputDirUrl + u"/assets/images"_ustr);

    const bool bVariants = maOptions.mbOptimizeImages && bRaster && aPix.Width() > 0;
    const bool bJpgVariants = bVariants && aExt == u".jpg"_ustr;
    const OUString aVarExt = bJpgVariants ? OUString(u".jpg"_ustr) : OUString(u".png"_ustr);
    const OUString aVarMime = bJpgVariants ? OUString(u"image/jpeg"_ustr) : OUString(u"image/png"_ustr);

    OUStringBuffer aSrcset;
    OUString aBaseRel;

    if (bVariants)
    {
        Bitmap aWork = aBmp.IsEmpty() ? rGraphic.GetBitmap() : aBmp;
        for (const sal_Int32 nW : { 640, 1280, 1920 })
        {
            if (nW >= aPix.Width())
                continue; // never upscale
            const sal_Int32 nH = (aPix.Height() * nW) / aPix.Width();
            Bitmap aScaled(aWork);
            if (!aScaled.Scale(Size(nW, nH), BmpScaleFlag::BestQuality))
                continue;
            const OUString aName = u"img-"_ustr + lcl_Pad3(mnImageSeq) + u"-"_ustr
                                   + OUString::number(nW) + aVarExt;
            const OUString aRel = u"assets/images/"_ustr + aName;
            if (!WriteScaledBitmap(aScaled, bJpgVariants, aRel))
                continue;
            if (!aSrcset.isEmpty())
                aSrcset.append(u", "_ustr);
            aSrcset.append(aRel);
            aSrcset.append(u" "_ustr);
            aSrcset.append(OUString::number(nW));
            aSrcset.append(u"w"_ustr);
            AddAsset(aRel, aVarMime, u"image"_ustr, nW, nH);
        }
        // Base: the original bytes, or a 1920-capped re-encode for oversized
        // JPEG originals.
        const sal_Int32 nBaseW = std::min<sal_Int32>(static_cast<sal_Int32>(aPix.Width()), 1920);
        const sal_Int32 nBaseH = (aPix.Height() * nBaseW) / aPix.Width();
        if (nBaseW < aPix.Width() && bJpgVariants)
        {
            Bitmap aBase(aWork);
            if (aBase.Scale(Size(nBaseW, nBaseH), BmpScaleFlag::BestQuality))
            {
                const OUString aName = u"img-"_ustr + lcl_Pad3(mnImageSeq) + aVarExt;
                const OUString aRel = u"assets/images/"_ustr + aName;
                if (WriteScaledBitmap(aBase, true, aRel))
                {
                    aBaseRel = aRel;
                    AddAsset(aRel, aVarMime, u"image"_ustr, nBaseW, nBaseH);
                    if (!aSrcset.isEmpty())
                        aSrcset.append(u", "_ustr);
                    aSrcset.append(aRel);
                    aSrcset.append(u" "_ustr);
                    aSrcset.append(OUString::number(nBaseW));
                    aSrcset.append(u"w"_ustr);
                }
            }
        }
        else
        {
            const OUString aRel = u"assets/images/"_ustr + aBaseName;
            if (WriteAssetFile(aRel, aBytes))
            {
                aBaseRel = aRel;
                AddAsset(aRel, aMime, u"image"_ustr, aPix.Width(), aPix.Height());
                if (!aSrcset.isEmpty())
                    aSrcset.append(u", "_ustr);
                aSrcset.append(aRel);
                aSrcset.append(u" "_ustr);
                aSrcset.append(OUString::number(aPix.Width()));
                aSrcset.append(u"w"_ustr);
            }
        }
    }
    else
    {
        const OUString aRel = u"assets/images/"_ustr + aBaseName;
        if (WriteAssetFile(aRel, aBytes))
        {
            aBaseRel = aRel;
            AddAsset(aRel, aMime, u"image"_ustr, aPix.Width(), aPix.Height());
        }
    }

    if (!aBaseRel.isEmpty())
    {
        aAsset.maSrc = aBaseRel;
        if (bVariants && !aSrcset.isEmpty())
        {
            aAsset.maSrcset = aSrcset.makeStringAndClear();
            aAsset.maSizes = u"(min-width: 721px) 72ch, 100vw"_ustr;
        }
    }
    maImageCache.emplace(aChecksum, aAsset);
    return aAsset;
}

// ---------------------------------------------------------------------------
// Phase 9: TOC / indexes / generated navigation
// ---------------------------------------------------------------------------

/// Exports a real Writer table of contents as a semantic, nested <nav>.
void WebExporter::WriteTocSection(const SwSectionNode& rSecNode, const SwTOXBase& rTOX)
{
    mbExportedToc = true;
    const SwNodeOffset nEnd = rSecNode.EndOfSectionIndex();
    maBody.append(u"<nav class=\"w27-toc\" aria-label=\"Table of contents\">"_ustr);
    const OUString aTitle = rTOX.GetTitle();
    if (!aTitle.isEmpty())
    {
        maBody.append(u"<h2 class=\"w27-toc-title\">"_ustr);
        maBody.append(lcl_Escape(aTitle));
        maBody.append(u"</h2>"_ustr);
    }
    maBody.append(u"<ol>"_ustr);
    sal_Int32 nOpenLevel = 1;
    for (SwNodeOffset n = rSecNode.GetIndex() + 1; n < nEnd; ++n)
    {
        const SwTextNode* pText = rSecNode.GetNodes()[n]->GetTextNode();
        if (!pText)
            continue;
        const SwTextFormatColl* pColl = pText->GetTextColl();
        if (!pColl)
            continue;
        const SwPoolFormatId nPool = pColl->GetPoolFormatId();
        const bool bEntry = (nPool >= SwPoolFormatId::COLL_TOX_CNTNT1
                             && nPool <= SwPoolFormatId::COLL_TOX_CNTNT5)
                            || (nPool >= SwPoolFormatId::COLL_TOX_CNTNT6
                                && nPool <= SwPoolFormatId::COLL_TOX_CNTNT10);
        if (!bEntry)
            continue;
        const int nLevel
            = static_cast<int>(nPool) - static_cast<int>(SwPoolFormatId::COLL_TOX_CNTNT1) + 1;
        while (nOpenLevel < nLevel)
        {
            maBody.append(u"<ol>"_ustr);
            ++nOpenLevel;
        }
        while (nOpenLevel > nLevel)
        {
            maBody.append(u"</ol>"_ustr);
            --nOpenLevel;
        }
        WriteTocEntry(*pText);
    }
    while (nOpenLevel > 1)
    {
        maBody.append(u"</ol>"_ustr);
        --nOpenLevel;
    }
    maBody.append(u"</ol></nav>"_ustr);
}

/// One TOC entry: link text with the page-number field and tab leaders
/// omitted, hyperlinked to the resolved heading/bookmark id.
void WebExporter::WriteTocEntry(const SwTextNode& rNode)
{
    const OUString& rText = rNode.GetText();
    const SwpHints* pHints = rNode.GetpSwpHints();

    OUStringBuffer aLabel;
    OUString aHref;
    const sal_Int32 nLen = rText.getLength();
    for (sal_Int32 nPos = 0; nPos < nLen; ++nPos)
    {
        bool bHintHandled = false;
        if (pHints)
        {
            for (size_t i = 0; i < pHints->Count(); ++i)
            {
                const SwTextAttr* pH = pHints->Get(i);
                if (pH->GetStart() == nPos)
                {
                    if (pH->Which() == RES_TXTATR_INETFMT)
                    {
                        // The link hint covers the entry label text; record
                        // the target and keep the characters as the label.
                        aHref = ResolveTarget(pH->GetINetFormat().GetValue());
                    }
                    else
                    {
                        // Field hints (page numbers) and others: their dummy
                        // characters are omitted from the label.
                        bHintHandled = true;
                    }
                    break;
                }
            }
        }
        if (bHintHandled)
            continue;
        const sal_Unicode c = rText[nPos];
        if (c != 0x09 && c != 0x0A) // tab leaders and soft breaks
            aLabel.append(c);
    }

    OUString aLabelStr = aLabel.makeStringAndClear();
    while (aLabelStr.endsWith(" "))
        aLabelStr = aLabelStr.copy(0, aLabelStr.getLength() - 1);
    if (aLabelStr.isEmpty())
        return;

    maBody.append(u"<li>"_ustr);
    if (!aHref.isEmpty())
    {
        maBody.append(u"<a href=\""_ustr);
        maBody.append(lcl_Escape(OUString(u"#"_ustr + aHref)));
        maBody.append(u"\">"_ustr);
        maBody.append(lcl_Escape(aLabelStr));
        maBody.append(u"</a>"_ustr);
    }
    else
    {
        maBody.append(lcl_Escape(aLabelStr));
    }
    maBody.append(u"</li>"_ustr);
}

/// Faithful export of non-TOC indexes (alphabetical index, table of figures,
/// table of tables, bibliography, user-defined): title plus generated text.
void WebExporter::WriteIndexSection(const SwSectionNode& rSecNode, const SwTOXBase& rTOX)
{
    const SwNodeOffset nEnd = rSecNode.EndOfSectionIndex();
    maBody.append(u"<section class=\"w27-index\">"_ustr);
    const OUString aTitle = rTOX.GetTitle();
    if (!aTitle.isEmpty())
    {
        maBody.append(u"<h2 class=\"w27-index-title\">"_ustr);
        maBody.append(lcl_Escape(aTitle));
        maBody.append(u"</h2>"_ustr);
    }
    for (SwNodeOffset n = rSecNode.GetIndex() + 1; n < nEnd; ++n)
    {
        const SwTextNode* pText = rSecNode.GetNodes()[n]->GetTextNode();
        if (!pText)
            continue;
        if (pText->GetText().isEmpty() && !pText->HasHints())
            continue;
        WriteParagraph(*pText, u"p"_ustr, u"w27-index-entry"_ustr, false, true);
    }
    maBody.append(u"</section>"_ustr);
}

/// Optional navigation derived from real outline headings. Skipped when the
/// document already contains a real TOC (no duplicate navigation).
void WebExporter::BuildGeneratedNav()
{
    if (!maOptions.mbGenerateNav || maNavItems.empty() || mbExportedToc)
        return;
    OUStringBuffer aNav;
    aNav.append(u"<nav class=\"w27-nav\" aria-label=\"Contents\">"_ustr);
    aNav.append(u"<ol>"_ustr);
    int nOpenLevel = 1;
    for (const NavItem& rItem : maNavItems)
    {
        const int nLevel = std::max(1, std::min(rItem.mnLevel, 6));
        while (nOpenLevel < nLevel)
        {
            aNav.append(u"<ol>"_ustr);
            ++nOpenLevel;
        }
        while (nOpenLevel > nLevel)
        {
            aNav.append(u"</ol>"_ustr);
            --nOpenLevel;
        }
        aNav.append(u"<li><a href=\""_ustr);
        aNav.append(lcl_Escape(OUString(u"#"_ustr + rItem.maId)));
        aNav.append(u"\">"_ustr);
        aNav.append(lcl_Escape(rItem.maText));
        aNav.append(u"</a></li>"_ustr);
    }
    while (nOpenLevel > 1)
    {
        aNav.append(u"</ol>"_ustr);
        --nOpenLevel;
    }
    aNav.append(u"</ol></nav>"_ustr);
    maGeneratedNav = aNav.makeStringAndClear();
}

/// Writes publication.json (Web Package only): deterministic, relative paths
/// only, no secrets, no absolute or machine-specific locations.
void WebExporter::WriteManifest()
{
    OUStringBuffer aJson;
    aJson.append(u"{\n"_ustr);
    aJson.append(u"  \"format\": \"writer2027-web\",\n"_ustr);
    aJson.append(u"  \"version\": 1,\n"_ustr);
    aJson.append(u"  \"entry\": \"index.html\",\n"_ustr);
    const OUString aTitle
        = maTitle.isEmpty() ? OUString(u"Writer 2027 Publication"_ustr) : maTitle;
    aJson.append(u"  \"title\": ").append(lcl_JsonString(aTitle)).append(u",\n"_ustr);
    const OUString aLang = maLang.isEmpty() ? OUString(u"en"_ustr) : maLang;
    aJson.append(u"  \"language\": ").append(lcl_JsonString(aLang)).append(u",\n"_ustr);

    std::sort(maManifestAssets.begin(), maManifestAssets.end(),
              [](const ManifestAsset& rA, const ManifestAsset& rB) {
                  return rA.maPath < rB.maPath;
              });
    aJson.append(u"  \"assets\": ["_ustr);
    bool bFirst = true;
    for (const ManifestAsset& rA : maManifestAssets)
    {
        if (!bFirst)
            aJson.append(u","_ustr);
        bFirst = false;
        aJson.append(u"\n    { "_ustr);
        aJson.append(u"\"path\": ").append(lcl_JsonString(rA.maPath));
        aJson.append(u", \"mediaType\": ").append(lcl_JsonString(rA.maMediaType));
        aJson.append(u", \"role\": ").append(lcl_JsonString(rA.maRole));
        if (rA.mnWidth > 0 && rA.mnHeight > 0)
        {
            aJson.append(u", \"width\": ").append(OUString::number(rA.mnWidth));
            aJson.append(u", \"height\": ").append(OUString::number(rA.mnHeight));
        }
        aJson.append(u" }"_ustr);
    }
    if (!bFirst)
        aJson.append(u"\n"_ustr);
    aJson.append(u"  ]\n}\n"_ustr);
    WriteTextFile(maOutputDirUrl + u"/publication.json"_ustr, aJson.makeStringAndClear());
}

// ---------------------------------------------------------------------------
// Traversal
// ---------------------------------------------------------------------------

void WebExporter::WriteNodes(SwNodeOffset nStart, SwNodeOffset nEnd)
{
    const SwNodes& rNodes = mrDoc.GetNodes();
    SwNodeOffset n = nStart;
    while (n < nEnd)
    {
        const SwNode& rNode = *rNodes[n];

        if (rNode.IsTextNode())
        {
            WriteTextNode(*rNode.GetTextNode());
            // Floating frames anchored to this paragraph follow it in DOM
            // order (semantic reading order, not desktop coordinates).
            WriteAnchoredFrames(rNode.GetIndex());
            ++n;
        }
        else if (rNode.IsGrfNode())
        {
            WriteGraphicNode(*rNode.GetGrfNode());
            ++n;
        }
        else if (rNode.IsTableNode())
        {
            CloseOpenList();
            ClosePendingFigure();
            CloseHeroGroup();
            CloseStatGroup();
            CloseClosingGroup();
            const SwTableNode* pTableNode = rNode.GetTableNode();
            WriteTable(*pTableNode);
            // Skip the table's box content nodes: they are exported through
            // the table model above.
            n = pTableNode->EndOfSectionIndex() + 1;
        }
        else if (rNode.IsSectionNode())
        {
            CloseOpenList();
            ClosePendingFigure();
            CloseHeroGroup();
            CloseStatGroup();
            CloseClosingGroup();

            const SwSectionNode* pSecNode = rNode.GetSectionNode();
            const SwSection& rSection = pSecNode->GetSection();

            // Real Writer TOC / index sections export as semantic nav.
            if (rSection.GetType() == SectionType::ToxContent)
            {
                const SwTOXBase* pTOX = rSection.GetTOXBase();
                if (pTOX)
                {
                    if (pTOX->GetType() == TOX_CONTENT)
                        WriteTocSection(*pSecNode, *pTOX);
                    else
                        WriteIndexSection(*pSecNode, *pTOX);
                }
                n = pSecNode->EndOfSectionIndex() + 1;
                continue;
            }

            const SwFormatCol& rCol = rSection.GetFormat()->GetCol();
            const bool bTwoCol = rCol.GetNumCols() > 1;

            ++mnSectionSeq;
            maBody.append(u"<section class=\"w27-section"_ustr);
            if (bTwoCol)
                maBody.append(u" w27-columns"_ustr);
            maBody.append(u"\" id=\"sec-"_ustr);
            maBody.append(OUString::number(mnSectionSeq));
            maBody.append(u"\">"_ustr);

            // Section content: nodes between the section start and its end.
            const SwNodeOffset nSecEnd = pSecNode->EndOfSectionIndex();
            WriteNodes(n + 1, nSecEnd);
            CloseOpenList();
            ClosePendingFigure();
            CloseHeroGroup();
            CloseStatGroup();
            CloseClosingGroup();
            maBody.append(u"</section>"_ustr);
            n = nSecEnd + 1;
        }
        else if (const SwStartNode* pStart = rNode.GetStartNode())
        {
            // Fly / footnote / header / footer / table-box content sections
            // are exported through their anchors or table model; skip their
            // content in the body walk to avoid double export.
            if (pStart->GetStartNodeType() != SwNormalStartNode)
                n = pStart->EndOfSectionIndex() + 1;
            else
                ++n;
        }
        else
        {
            // End nodes are jumped past by their matching start nodes.
            ++n;
        }
    }
}

// ---------------------------------------------------------------------------
// Paragraph classification + output
// ---------------------------------------------------------------------------

bool WebExporter::IsCaptionParagraph(const SwTextNode& rNode) const
{
    const SwTextFormatColl* pColl = rNode.GetTextColl();
    return pColl && pColl->GetPoolFormatId() == SwPoolFormatId::COLL_LABEL;
}

void WebExporter::ClosePendingFigure()
{
    if (mbFigurePending)
    {
        maBody.append(u"</figure>"_ustr);
        mbFigurePending = false;
    }
}

void WebExporter::CloseOpenList()
{
    if (mpOpenListRule)
    {
        maBody.append(mbOpenListOrdered ? u"</ol>"_ustr : u"</ul>"_ustr);
        mpOpenListRule = nullptr;
        mnOpenListLevel = -1;
        mbOpenListOrdered = false;
    }
}

void WebExporter::CloseHeroGroup()
{
    if (mbInHero)
    {
        maBody.append(u"</header>"_ustr);
        mbInHero = false;
    }
}

void WebExporter::CloseStatGroup()
{
    if (mbInStat)
    {
        maBody.append(u"</aside>"_ustr);
        mbInStat = false;
    }
}

void WebExporter::CloseClosingGroup()
{
    if (mbInClosing)
    {
        maBody.append(u"</section>"_ustr);
        mbInClosing = false;
    }
}

void WebExporter::WriteTextNode(const SwTextNode& rNode)
{
    const SwTextFormatColl* pColl = rNode.GetTextColl();
    const OUString aText = rNode.GetText();

    // Skip truly empty paragraphs (they carry no structure).
    if (aText.isEmpty() && !rNode.HasHints())
        return;

    // Paragraph consisting only of anchored as-char frames: export each fly
    // as a standalone block figure (a following Caption paragraph becomes
    // the <figcaption>). Group state stays intact so e.g. the hero image
    // remains part of the hero composition.
    if (aText.isEmpty() && rNode.HasHints())
    {
        const SwpHints* pHints = rNode.GetpSwpHints();
        bool bOnlyFlyCnt = true;
        for (size_t i = 0; i < pHints->Count(); ++i)
            if (pHints->Get(i)->Which() != RES_TXTATR_FLYCNT)
                bOnlyFlyCnt = false;
        if (bOnlyFlyCnt && pHints->Count() > 0)
        {
            CloseOpenList();
            ClosePendingFigure();
            for (size_t i = 0; i < pHints->Count(); ++i)
            {
                if (SwFrameFormat* pFly = pHints->Get(i)->GetFlyCnt().GetFrameFormat())
                    WriteFlyFrame(*pFly, /*bInline=*/false);
                if (i + 1 < pHints->Count())
                    ClosePendingFigure();
            }
            return;
        }
    }

    // Figure continuation: a Caption-styled paragraph right after an image
    // becomes the <figcaption>; anything else closes the figure.
    if (mbFigurePending)
    {
        if (IsCaptionParagraph(rNode))
        {
            WriteParagraph(rNode, u"figcaption"_ustr, u"w27-caption"_ustr, false, true);
            maBody.append(u"</figure>"_ustr);
            mbFigurePending = false;
            return;
        }
        ClosePendingFigure();
    }

    CloseOpenList();

    // Editorial block grouping: a hero/stat group stays open only while its
    // own paragraph family continues (starts are the dedicated block styles;
    // Caption/Subtitle paragraphs continue an already-open group).
    const bool bHeroStart = lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_HERO_EYEBROW)
                            || lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_HERO_TITLE);
    const bool bHeroContinue
        = mbInHero
          && (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_DOC_SUBTITLE
              || pColl->GetPoolFormatId() == SwPoolFormatId::COLL_LABEL);
    if (!bHeroStart && !bHeroContinue)
        CloseHeroGroup();

    const bool bStatStart = lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_STAT_VALUE)
                            || lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_STAT_LABEL);
    const bool bStatContinue
        = mbInStat && pColl->GetPoolFormatId() == SwPoolFormatId::COLL_LABEL;
    if (!bStatStart && !bStatContinue)
        CloseStatGroup();

    if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_CLOSING_STATEMENT))
    {
        maBody.append(u"<section class=\"w27-closing\">"_ustr);
        mbInClosing = true;
        WriteParagraph(rNode, u"p"_ustr, u"w27-closing-statement"_ustr, false, false);
        return;
    }
    if (mbInClosing)
    {
        // The immediate Subtitle line belongs to the closing composition;
        // anything else closes the group and flows normally.
        if (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_DOC_SUBTITLE)
        {
            WriteParagraph(rNode, u"p"_ustr, u"w27-closing-line"_ustr, false, true);
            CloseClosingGroup();
            return;
        }
        CloseClosingGroup();
    }

    if (bHeroStart || bHeroContinue)
    {
        if (!mbInHero)
        {
            maBody.append(u"<header class=\"w27-hero\">"_ustr);
            mbInHero = true;
        }
        if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_HERO_EYEBROW))
            WriteParagraph(rNode, u"p"_ustr, u"w27-hero-eyebrow"_ustr, false, true);
        else if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_HERO_TITLE))
            WriteParagraph(rNode, u"h1"_ustr, u"w27-hero-title"_ustr, false, true);
        else if (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_DOC_SUBTITLE)
            WriteParagraph(rNode, u"p"_ustr, u"w27-hero-subtitle"_ustr, false, true);
        else
            WriteParagraph(rNode, u"p"_ustr, u"w27-hero-meta"_ustr, false, true);
        return;
    }

    if (bStatStart || bStatContinue)
    {
        if (!mbInStat)
        {
            maBody.append(u"<aside class=\"w27-stat\">"_ustr);
            mbInStat = true;
        }
        if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_STAT_VALUE))
            WriteParagraph(rNode, u"p"_ustr, u"w27-stat-value"_ustr, false, true);
        else if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_STAT_LABEL))
            WriteParagraph(rNode, u"p"_ustr, u"w27-stat-label"_ustr, false, true);
        else
            WriteParagraph(rNode, u"p"_ustr, u"w27-stat-line"_ustr, false, true);
        return;
    }

    if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_PULL_QUOTE))
    {
        WriteParagraph(rNode, u"blockquote"_ustr, u"w27-pullquote"_ustr, false, true);
        return;
    }

    if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_CALLOUT))
    {
        WriteParagraph(rNode, u"aside"_ustr, u"w27-callout"_ustr, false, true);
        return;
    }

    if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_RESEARCH_NOTE))
    {
        WriteParagraph(rNode, u"aside"_ustr, u"w27-research-note"_ustr, false, true);
        return;
    }

    if (lcl_IsW27Style(*pColl, STR_WRITER2027_STYLE_CODE)
        || pColl->GetPoolFormatId() == SwPoolFormatId::COLL_HTML_PRE)
    {
        maBody.append(u"<pre class=\"w27-code\"><code>"_ustr);
        WriteRuns(rNode, 0, aText.getLength());
        maBody.append(u"</code></pre>"_ustr);
        return;
    }

    if (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_DOC_TITLE)
    {
        WriteParagraph(rNode, u"h1"_ustr, u"w27-title"_ustr, false, true);
        return;
    }

    if (pColl->GetPoolFormatId() == SwPoolFormatId::COLL_HTML_BLOCKQUOTE)
    {
        WriteParagraph(rNode, u"blockquote"_ustr, OUString(), false, true);
        return;
    }

    // Headings: hierarchy from the real outline level, never font size.
    const int nOutline = rNode.GetAttrOutlineLevel();
    if (nOutline > 0 && nOutline <= 10)
    {
        ++mnHeadingSeq;
        int nLevel = nOutline;
        if (mbHasTitle)
            nLevel = std::min(nLevel + 1, static_cast<int>(MAX_HEADING_LEVEL));
        nLevel = std::max(1, std::min(nLevel, static_cast<int>(MAX_HEADING_LEVEL)));

        // Stable, readable, collision-safe id (slug from the heading text,
        // deterministic numeric fallback when slugging produces nothing).
        const OUString aText = rNode.GetText();
        OUString aSlug = lcl_Slugify(aText);
        if (aSlug.isEmpty())
            aSlug = u"h-"_ustr + OUString::number(mnHeadingSeq);
        const OUString aId = ClaimId(aSlug);
        RegisterHeadingTarget(aText, aId);
        if (maOptions.mbGenerateNav)
            maNavItems.push_back({ nLevel, aId, aText });

        maBody.append(u"<h"_ustr);
        maBody.append(OUString::number(nLevel));
        maBody.append(u" id=\""_ustr);
        maBody.append(lcl_Escape(aId));
        maBody.append(u"\">"_ustr);
        WriteRuns(rNode, 0, aText.getLength());
        maBody.append(u"</h"_ustr);
        maBody.append(OUString::number(nLevel));
        maBody.append(u">"_ustr);
        return;
    }

    if (IsCaptionParagraph(rNode))
    {
        WriteParagraph(rNode, u"p"_ustr, u"w27-caption"_ustr, false, true);
        return;
    }

    // Lists: a numbered/bulleted paragraph becomes a list item.
    if (SwNumRule* pRule = rNode.GetNumRule())
    {
        const int nLevel = std::max(0, rNode.GetActualListLevel());
        const SwNumFormat* pNumFmt = pRule->GetNumFormat(0);
        const bool bOrdered = pNumFmt && pNumFmt->IsEnumeration();
        if (mpOpenListRule != pRule || mnOpenListLevel != nLevel
            || mbOpenListOrdered != bOrdered)
        {
            CloseOpenList();
            mpOpenListRule = pRule;
            mnOpenListLevel = nLevel;
            mbOpenListOrdered = bOrdered;
            maBody.append(bOrdered ? u"<ol>"_ustr : u"<ul>"_ustr);
        }
        WriteParagraph(rNode, u"li"_ustr, OUString(), false, true);
        return;
    }

    // Custom paragraph style: semantic tag where inferable, CSS class from
    // the actual style properties (see MakeCss).
    if ((USER_FMT & sal_uInt16(pColl->GetPoolFormatId())) && !pColl->IsDefault())
    {
        const OUString aClass = u"w27-custom-"_ustr + SanitizeName(pColl->GetName().toString());
        if (maParaStyleCss.find(aClass) == maParaStyleCss.end())
            maParaStyleCss.emplace(aClass, lcl_ResolveParagraphCss(*pColl, maFontFamilies));
        WriteParagraph(rNode, u"p"_ustr, aClass, false, true);
        return;
    }

    // Plain body paragraph.
    WriteParagraph(rNode, u"p"_ustr, u"w27-body"_ustr, false, true);
}

void WebExporter::WriteParagraph(const SwTextNode& rNode, const OUString& rTag,
                                 const OUString& rClass, bool bPre, bool bCloseable)
{
    maBody.append(u"<"_ustr);
    maBody.append(rTag);
    if (!rClass.isEmpty())
    {
        maBody.append(u" class=\""_ustr);
        maBody.append(rClass);
        maBody.append(u"\""_ustr);
    }
    maBody.append(u">"_ustr);
    if (bPre)
        maBody.append(lcl_Escape(rNode.GetText()));
    else
        WriteRuns(rNode, 0, rNode.GetText().getLength());
    if (bCloseable)
    {
        maBody.append(u"</"_ustr);
        maBody.append(rTag);
        maBody.append(u">"_ustr);
    }
}

// ---------------------------------------------------------------------------
// Character runs
// ---------------------------------------------------------------------------

void WebExporter::WriteRuns(const SwTextNode& rNode, sal_Int32 nFrom, sal_Int32 nTo)
{
    const OUString& rText = rNode.GetText();
    const SwpHints* pHints = rNode.GetpSwpHints();

    sal_Int32 nPos = nFrom;
    // Simple linear scan: per paragraph the hint count is small.
    while (nPos < nTo)
    {
        // Find the next hint that starts at or after nPos (and before nTo).
        const SwTextAttr* pHint = nullptr;
        if (pHints)
        {
            for (size_t i = 0; i < pHints->Count(); ++i)
            {
                const SwTextAttr* pH = pHints->Get(i);
                if (pH->GetStart() >= nPos && pH->GetStart() < nTo)
                {
                    pHint = pH;
                    break;
                }
                if (pH->GetStart() >= nTo)
                    break;
            }
        }
        if (!pHint)
        {
            maBody.append(lcl_Escape(std::u16string_view(rText).substr(nPos, nTo - nPos)));
            nPos = nTo;
            break;
        }

        const sal_Int32 nHintStart = pHint->GetStart();
        if (nHintStart > nPos)
        {
            maBody.append(lcl_Escape(std::u16string_view(rText).substr(nPos, nHintStart - nPos)));
            nPos = nHintStart;
        }

        const sal_Int32* pEnd = pHint->GetEnd();
        const sal_Int32 nHintEnd = pEnd ? std::min(*pEnd, nTo) : (nHintStart + 1);

        switch (pHint->Which())
        {
            case RES_TXTATR_INETFMT:
            {
                const SwFormatINetFormat& rINet = pHint->GetINetFormat();
                const OUString aURL = rINet.GetValue();
                const OUString aName = rINet.GetName();
                // Internal links resolve to exported heading/bookmark ids;
                // external URLs go through the scheme allow-list. Unresolvable
                // targets keep their visible text without a broken link.
                OUString aHref;
                if (aURL.startsWith("#"))
                {
                    const OUString aTargetId = ResolveTarget(aURL);
                    if (!aTargetId.isEmpty())
                        aHref = u"#"_ustr + aTargetId;
                }
                else
                    aHref = sw::writer2027capabilities::SafeHref(aURL);
                if (aHref.isEmpty())
                {
                    if (nHintStart < nHintEnd)
                        maBody.append(lcl_Escape(
                            std::u16string_view(rText).substr(nHintStart, nHintEnd - nHintStart)));
                    break;
                }
                maBody.append(u"<a href=\""_ustr);
                maBody.append(lcl_Escape(aHref));
                maBody.append(u"\""_ustr);
                if (!aName.isEmpty())
                {
                    maBody.append(u" id=\""_ustr);
                    maBody.append(ClaimId(u"bm-"_ustr + SanitizeName(aName)));
                    maBody.append(u"\""_ustr);
                }
                maBody.append(u">"_ustr);
                // The range [start, end) is the link text; write the first
                // character directly, then recurse to avoid re-triggering
                // this same hint.
                if (nHintStart < nHintEnd)
                    maBody.append(lcl_Escape(std::u16string_view(rText).substr(nHintStart, 1)));
                WriteRuns(rNode, nHintStart + 1, nHintEnd);
                maBody.append(u"</a>"_ustr);
                break;
            }
            case RES_TXTATR_CHARFMT:
            {
                SwCharFormat* pCharFormat = pHint->GetCharFormat().GetCharFormat();
                if (pCharFormat)
                {
                    const OUString aClass
                        = u"w27-char-"_ustr + SanitizeName(pCharFormat->GetName().toString());
                    if (maCharStyleCss.find(aClass) == maCharStyleCss.end())
                        maCharStyleCss.emplace(aClass, lcl_ResolveCharCss(*pCharFormat,
                                                                          maFontFamilies));
                    maBody.append(u"<span class=\""_ustr);
                    maBody.append(aClass);
                    maBody.append(u"\">"_ustr);
                    if (nHintStart < nHintEnd)
                        maBody.append(
                            lcl_Escape(std::u16string_view(rText).substr(nHintStart, 1)));
                    WriteRuns(rNode, nHintStart + 1, nHintEnd);
                    maBody.append(u"</span>"_ustr);
                }
                else if (nHintStart < nHintEnd)
                {
                    maBody.append(
                        lcl_Escape(std::u16string_view(rText).substr(nHintStart,
                                                                     nHintEnd - nHintStart)));
                }
                break;
            }
            case RES_TXTATR_FTN:
                WriteFootnote(*static_cast<const SwTextFootnote*>(pHint));
                break;
            case RES_TXTATR_REFMARK:
            {
                const OUString aName = pHint->GetRefMark().GetRefName().toString();
                if (!aName.isEmpty())
                {
                    maBody.append(u"<span id=\""_ustr);
                    maBody.append(ClaimId(u"bm-"_ustr + SanitizeName(aName)));
                    maBody.append(u"\"></span>"_ustr);
                }
                break;
            }
            case RES_TXTATR_FIELD:
            {
                // Fields export their current visible value. Page-number and
                // page-count fields are omitted (no responsive page model);
                // cross-references become links when the target is exported.
                const SwFormatField& rFmtField = pHint->GetFormatField();
                const SwField* pField = rFmtField.GetField();
                OUString aText;
                OUString aHref;
                if (pField)
                {
                    switch (pField->Which())
                    {
                        case SwFieldIds::PageNumber:
                        case SwFieldIds::RefPageGet:
                        case SwFieldIds::RefPageSet:
                            break; // paper-only
                        case SwFieldIds::DocStat:
                            if (const SwDocStatField* pStat
                                = dynamic_cast<const SwDocStatField*>(pField))
                            {
                                const SwDocStatSubType eSub = pStat->GetSubType();
                                if (eSub == SwDocStatSubType::Page
                                    || eSub == SwDocStatSubType::PageRange)
                                    break; // page counts are paper-only
                            }
                            aText = pField->ExpandField(true, nullptr);
                            break;
                        case SwFieldIds::GetRef:
                            if (const auto* pRef = dynamic_cast<const SwGetRefField*>(pField))
                            {
                                const OUString aTargetId
                                    = ResolveTarget(pRef->GetSetRefName().toString());
                                if (!aTargetId.isEmpty())
                                    aHref = u"#"_ustr + aTargetId;
                            }
                            aText = pField->ExpandField(true, nullptr);
                            break;
                        default:
                            aText = pField->ExpandField(true, nullptr);
                            break;
                    }
                }
                if (!aHref.isEmpty())
                {
                    maBody.append(u"<a href=\""_ustr);
                    maBody.append(lcl_Escape(aHref));
                    maBody.append(u"\">"_ustr);
                    maBody.append(lcl_Escape(aText));
                    maBody.append(u"</a>"_ustr);
                }
                else if (!aText.isEmpty())
                    maBody.append(lcl_Escape(aText));
                break;
            }
            case RES_TXTATR_FLYCNT:
                if (SwFrameFormat* pFly = pHint->GetFlyCnt().GetFrameFormat())
                    WriteFlyFrame(*pFly, /*bInline=*/true);
                break;
            case RES_CHRATR_WEIGHT:
                maBody.append(u"<b>"_ustr);
                if (nHintStart < nHintEnd)
                    maBody.append(lcl_Escape(std::u16string_view(rText).substr(nHintStart, 1)));
                WriteRuns(rNode, nHintStart + 1, nHintEnd);
                maBody.append(u"</b>"_ustr);
                break;
            case RES_CHRATR_POSTURE:
                maBody.append(u"<i>"_ustr);
                if (nHintStart < nHintEnd)
                    maBody.append(lcl_Escape(std::u16string_view(rText).substr(nHintStart, 1)));
                WriteRuns(rNode, nHintStart + 1, nHintEnd);
                maBody.append(u"</i>"_ustr);
                break;
            default:
                // Unknown attributes export as plain text (fields, TOX marks,
                // content controls etc. are not structural in this phase).
                if (nHintStart < nHintEnd)
                    maBody.append(lcl_Escape(std::u16string_view(rText).substr(
                        nHintStart, nHintEnd - nHintStart)));
                break;
        }
        nPos = nHintEnd;
    }
}

void WebExporter::WriteFootnote(const SwTextFootnote& rFootnote)
{
    ++mnNoteSeq;
    const sal_Int32 nNote = mnNoteSeq;
    const OUString aNum
        = rFootnote.GetFootnote().GetNumStr().isEmpty()
              ? OUString::number(rFootnote.GetFootnote().GetNumber())
              : rFootnote.GetFootnote().GetNumStr();

    maBody.append(u"<sup id=\"fnref-"_ustr);
    maBody.append(OUString::number(nNote));
    maBody.append(u"\"><a href=\"#fn-"_ustr);
    maBody.append(OUString::number(nNote));
    maBody.append(u"\">"_ustr);
    maBody.append(lcl_Escape(aNum));
    maBody.append(u"</a></sup>"_ustr);

    // Collect the note text from the footnote content nodes.
    OUStringBuffer aNoteText;
    if (const SwNodeIndex* pIdx = rFootnote.GetStartNode())
    {
        const SwNodeOffset nEnd = pIdx->GetNode().EndOfSectionIndex();
        for (SwNodeOffset n = pIdx->GetIndex() + 1; n < nEnd; ++n)
        {
            const SwTextNode* pText = pIdx->GetNodes()[n]->GetTextNode();
            if (pText && !pText->GetText().isEmpty())
            {
                if (!aNoteText.isEmpty())
                    aNoteText.append(u" "_ustr);
                aNoteText.append(pText->GetText());
            }
        }
    }
    maNotes.append(u"<li id=\"fn-"_ustr);
    maNotes.append(OUString::number(nNote));
    maNotes.append(u"\">"_ustr);
    maNotes.append(lcl_Escape(aNoteText.makeStringAndClear()));
    maNotes.append(u" <a class=\"w27-note-back\" href=\"#fnref-"_ustr);
    maNotes.append(OUString::number(nNote));
    maNotes.append(u"\" aria-label=\"\u2190\">\u21A9</a></li>"_ustr);
}

void WebExporter::WriteNoteSection()
{
    if (mnNoteSeq == 0)
        return;
    maBody.append(u"<section class=\"w27-notes\" aria-label=\"Footnotes\"><ol>"_ustr);
    maBody.append(maNotes);
    maBody.append(u"</ol></section>"_ustr);
}

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------

void WebExporter::WriteTable(const SwTableNode& rNode)
{
    const SwTable& rTable = rNode.GetTable();
    const SwTableLines& rLines = rTable.GetTabLines();
    if (rLines.empty())
        return;

    // Deterministic column grid: cumulative end position of every box across
    // all rows (the same grid algorithm the flat ODF exporter uses).
    std::vector<sal_uInt32> aColPos;
    for (const SwTableLine* pLine : rLines)
    {
        sal_uInt32 nCPos = 0;
        for (const SwTableBox* pBox : pLine->GetTabBoxes())
        {
            nCPos += sal_uInt32(
                pBox->GetFrameFormat()->GetFormatAttr(RES_FRM_SIZE).GetSize().Width());
            aColPos.push_back(nCPos);
        }
    }
    std::sort(aColPos.begin(), aColPos.end());
    aColPos.erase(std::unique(aColPos.begin(), aColPos.end()), aColPos.end());
    const sal_uInt32 nTableWidth = aColPos.empty() ? 0 : aColPos.back();
    const auto nColIndexOf = [&aColPos](sal_uInt32 nPos) -> size_t {
        const auto it = std::lower_bound(aColPos.begin(), aColPos.end(), nPos);
        return (it == aColPos.end() || *it != nPos) ? aColPos.size()
                                                    : static_cast<size_t>(it - aColPos.begin());
    };

    const sal_uInt16 nHeaderRows = rTable.GetRowsToRepeat();

    maBody.append(u"<div class=\"w27-table-wrap\"><table class=\"w27-table\">"_ustr);
    bool bInHead = false;
    const size_t nRows = rLines.size();
    for (size_t nRow = 0; nRow < nRows; ++nRow)
    {
        const SwTableLine* pLine = rLines[nRow];
        const bool bHeadRow = nRow < nHeaderRows;
        if (bHeadRow && !bInHead)
        {
            maBody.append(u"<thead>"_ustr);
            bInHead = true;
        }
        else if (!bHeadRow && bInHead)
        {
            maBody.append(u"</thead><tbody>"_ustr);
            bInHead = false;
        }

        maBody.append(u"<tr>"_ustr);
        sal_uInt32 nCPos = 0;
        size_t nCol = 0;
        const SwTableBoxes& rBoxes = pLine->GetTabBoxes();
        for (size_t nBox = 0; nBox < rBoxes.size(); ++nBox)
        {
            const SwTableBox* pBox = rBoxes[nBox];
            const size_t nOldCol = nCol;
            if (nBox + 1 < rBoxes.size())
            {
                nCPos += sal_uInt32(
                    pBox->GetFrameFormat()->GetFormatAttr(RES_FRM_SIZE).GetSize().Width());
            }
            else
            {
                nCPos = nTableWidth;
            }
            const sal_Int32 nRowSpan = pBox->getRowSpan();
            size_t nNewCol = nColIndexOf(nCPos);
            if (nNewCol == aColPos.size() || nNewCol < nOldCol)
                nNewCol = nOldCol; // fault tolerance for corrupted grids
            nCol = nNewCol;
            const sal_uInt32 nColSpan = static_cast<sal_uInt32>(nCol - nOldCol + 1);

            if (nRowSpan >= 1)
            {
                // Covered cells (nRowSpan < 1) are already covered by the
                // master cell's rowspan; they are not duplicated.
                const bool bTh = bHeadRow && pBox->IsInHeadline(&rTable);
                maBody.append(bTh ? u"<th scope=\"col\""_ustr : u"<td"_ustr);
                if (nRowSpan > 1)
                {
                    maBody.append(u" rowspan=\""_ustr);
                    maBody.append(OUString::number(nRowSpan));
                    maBody.append(u"\""_ustr);
                }
                if (nColSpan > 1)
                {
                    maBody.append(u" colspan=\""_ustr);
                    maBody.append(OUString::number(nColSpan));
                    maBody.append(u"\""_ustr);
                }
                maBody.append(u">"_ustr);
                WriteTableBox(*pBox);
                maBody.append(bTh ? u"</th>"_ustr : u"</td>"_ustr);
            }
            ++nCol;
        }
        maBody.append(u"</tr>"_ustr);
    }
    if (bInHead)
        maBody.append(u"</thead>"_ustr);
    else
        maBody.append(u"</tbody>"_ustr);
    maBody.append(u"</table></div>"_ustr);
}

void WebExporter::WriteTableBox(const SwTableBox& rBox)
{
    maBody.append(u"<td>"_ustr);
    const SwStartNode* pStt = rBox.GetSttNd();
    if (pStt)
    {
        const SwNodeOffset nEnd = pStt->EndOfSectionIndex();
        for (SwNodeOffset n = pStt->GetIndex() + 1; n < nEnd; ++n)
        {
            const SwNode& rNode = *pStt->GetNodes()[n];
            if (const SwTextNode* pText = rNode.GetTextNode())
            {
                if (!pText->GetText().isEmpty() || pText->HasHints())
                    WriteParagraph(*pText, u"p"_ustr, u"w27-body"_ustr, false, true);
                WriteAnchoredFrames(n);
            }
            else if (const SwGrfNode* pGrf = rNode.GetGrfNode())
            {
                WriteGraphicNode(*pGrf);
                ClosePendingFigure();
            }
            else if (rNode.IsTableNode())
            {
                // nested table
                WriteTable(*rNode.GetTableNode());
            }
        }
        ClosePendingFigure();
    }
    maBody.append(u"</td>"_ustr);
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

void WebExporter::WriteGraphicNode(const SwGrfNode& rNode)
{
    const Graphic& rGraphic = rNode.GetGrfObj().GetGraphic();
    if (rGraphic.GetType() == GraphicType::NONE)
        return;

    OUString aAlt;
    if (SwFrameFormat* pFly = const_cast<SwGrfNode&>(rNode).GetFlyFormat())
    {
        const SwFlyFrameFormat* pFlyFmt = dynamic_cast<const SwFlyFrameFormat*>(pFly);
        const OUString aDesc = pFlyFmt ? pFlyFmt->GetObjDescription() : OUString();
        if (!aDesc.isEmpty())
            aAlt = aDesc;
    }

    CloseOpenList();
    CloseHeroGroup();
    CloseStatGroup();

    const ImageAsset aImg = ExportImage(rGraphic);
    maBody.append(u"<figure class=\"w27-image\">"_ustr);
    if (aImg.maSrc.isEmpty())
    {
        // Not embeddable: keep the alternative text instead of silent loss.
        maBody.append(u"<p class=\"w27-image-placeholder\">"_ustr);
        maBody.append(lcl_Escape(aAlt));
        maBody.append(u"</p>"_ustr);
        maBody.append(u"</figure>"_ustr);
        return;
    }
    maBody.append(u"<img src=\""_ustr);
    maBody.append(aImg.maSrc);
    maBody.append(u"\" alt=\""_ustr);
    maBody.append(lcl_Escape(aAlt));
    maBody.append(u"\""_ustr);
    if (!aImg.maSrcset.isEmpty())
    {
        maBody.append(u" srcset=\""_ustr);
        maBody.append(lcl_Escape(aImg.maSrcset));
        maBody.append(u"\""_ustr);
        maBody.append(u" sizes=\""_ustr);
        maBody.append(lcl_Escape(aImg.maSizes));
        maBody.append(u"\""_ustr);
    }
    maBody.append(u">"_ustr);
    mbFigurePending = true; // a following Caption paragraph becomes <figcaption>
}

// ---------------------------------------------------------------------------
// CSS
// ---------------------------------------------------------------------------

OUString WebExporter::MakeCss()
{
    OUStringBuffer aCss;

    // 0. embedded fonts (@font-face) - resolves, writes and inlines the font
    //    files before any rule that references the family names.
    BuildFontFaces();
    if (!maFontFaceCss.isEmpty())
    {
        aCss.append(maFontFaceCss);
        aCss.append(u"\n"_ustr);
    }

    // 1. document variables
    aCss.append(u":root {\n"_ustr);
    aCss.append(u"  --w27-bg: ").append(lcl_ToCssColor(maStyle.maBg)).append(u";\n"_ustr);
    aCss.append(u"  --w27-text: ").append(lcl_ToCssColor(maStyle.maText)).append(u";\n"_ustr);
    aCss.append(u"  --w27-secondary: ").append(lcl_ToCssColor(maStyle.maSecondary))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-hairline: ").append(lcl_ToCssColor(maStyle.maHairline))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-accent: ").append(lcl_ToCssColor(maStyle.maAccent)).append(u";\n"_ustr);
    aCss.append(u"  --w27-code-bg: ").append(lcl_ToCssColor(maStyle.maCodeBg)).append(u";\n"_ustr);
    aCss.append(u"  --w27-font-body: ")
        .append(lcl_FontStack(maStyle.maBodyFont, maStyle.meBodyGeneric)).append(u";\n"_ustr);
    aCss.append(u"  --w27-font-heading: ")
        .append(lcl_FontStack(maStyle.maHeadingFont, maStyle.meHeadingGeneric))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-font-display: ")
        .append(lcl_FontStack(maStyle.maDisplayFont, maStyle.meDisplayGeneric))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-font-mono: ")
        .append(lcl_FontStack(maStyle.maMonoFont, maStyle.meMonoGeneric)).append(u";\n"_ustr);
    aCss.append(u"  --w27-size-body: ").append(lcl_TwipsToRem(maStyle.mnBodySize))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-size-h1: ").append(lcl_TwipsToRem(maStyle.mnHeadingSize))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-size-title: ").append(lcl_TwipsToRem(maStyle.mnTitleSize))
        .append(u";\n"_ustr);
    aCss.append(u"  --w27-line-body: ")
        .append(rtl::math::doubleToUString(maStyle.mfLineHeight, rtl_math_StringFormat_Automatic,
                                           rtl_math_DecimalPlaces(2), '.'))
        .append(u";\n"_ustr);
    aCss.append(u"}\n\n"_ustr);

    // 2. reset / base
    aCss.append(u"*, *::before, *::after { box-sizing: border-box; }\n"_ustr);
    aCss.append(u"html { ").append(maStyle.mbDark ? u"color-scheme: dark;" : u"color-scheme: light;")
        .append(u" }\n"_ustr);
    aCss.append(u"body {\n"_ustr);
    aCss.append(u"  margin: 0;\n"_ustr);
    aCss.append(u"  background: var(--w27-bg);\n"_ustr);
    aCss.append(u"  color: var(--w27-text);\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-body);\n"_ustr);
    aCss.append(u"  font-size: var(--w27-size-body);\n"_ustr);
    aCss.append(u"  line-height: var(--w27-line-body);\n"_ustr);
    aCss.append(u"  -webkit-text-size-adjust: 100%;\n"_ustr);
    aCss.append(u"  text-rendering: optimizeLegibility;\n"_ustr);
    aCss.append(u"}\n\n"_ustr);

    // 3. responsive canvas
    aCss.append(u"main {\n"_ustr);
    aCss.append(u"  width: min(72ch, 100% - 2 * 1.5rem);\n"_ustr);
    aCss.append(u"  margin-inline: auto;\n"_ustr);
    aCss.append(u"  padding-block: clamp(2rem, 6vw, 5rem);\n"_ustr);
    aCss.append(u"}\n\n"_ustr);

    // 4. typography
    aCss.append(u"h1, h2, h3, h4, h5, h6 {\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-heading);\n"_ustr);
    aCss.append(u"  line-height: 1.15;\n"_ustr);
    aCss.append(u"  margin-block: 1.4em 0.4em;\n"_ustr);
    aCss.append(u"  text-wrap: balance;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u"h1 { font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(1.5rem, 1.1rem + 1.6vw, var(--w27-size-h1));"_ustr
                                                  : u"var(--w27-size-h1);"_ustr)
        .append(u" }\n"_ustr);
    aCss.append(u"h2 { font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(1.3rem, 1.05rem + 1vw, 1.8rem);"_ustr
                                                  : u"1.5rem;"_ustr)
        .append(u" }\n"_ustr);
    aCss.append(u"h3 { font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(1.15rem, 1rem + 0.6vw, 1.4rem);"_ustr
                                                  : u"1.25rem;"_ustr)
        .append(u" }\n"_ustr);
    aCss.append(u"h4 { font-size: 1.1rem; }\n"_ustr);
    aCss.append(u"h5, h6 { font-size: 1rem; }\n"_ustr);
    aCss.append(u"p { margin-block: 0.9em 0; }\n"_ustr);
    aCss.append(u"a { color: var(--w27-accent); text-decoration-thickness: 0.08em; }\n"_ustr);
    aCss.append(u"a:hover { text-decoration-style: solid; }\n"_ustr);
    aCss.append(u"img { max-width: 100%; height: auto; }\n"_ustr);
    aCss.append(u"figure { margin: 2em 0; }\n"_ustr);
    aCss.append(u"figcaption, .w27-caption { color: var(--w27-secondary); font-size: 0.9em; }\n"_ustr);
    aCss.append(u"blockquote {\n"_ustr);
    aCss.append(u"  margin-inline: 0;\n"_ustr);
    aCss.append(u"  padding-inline-start: 1.2em;\n"_ustr);
    aCss.append(u"  border-inline-start: 3px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  font-size: 1.15em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u"pre { overflow-x: auto; }\n"_ustr);
    aCss.append(u"code { font-family: var(--w27-font-mono); }\n"_ustr);
    aCss.append(u"sup { line-height: 0; }\n"_ustr);

    // 5. structure + editorial blocks
    aCss.append(u".w27-title {\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-display);\n"_ustr);
    aCss.append(u"  font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(1.9rem, 1.3rem + 3vw, var(--w27-size-title));"_ustr
                                                  : u"var(--w27-size-title);"_ustr)
        .append(u"\n"_ustr);
    aCss.append(u"  margin-block: 0 0.5em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-hero {\n"_ustr);
    aCss.append(u"  margin-block: clamp(1rem, 4vw, 3rem);\n"_ustr);
    aCss.append(u"  padding-block: clamp(1.5rem, 5vw, 4rem);\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-hero-eyebrow {\n"_ustr);
    aCss.append(u"  text-transform: uppercase;\n"_ustr);
    aCss.append(u"  letter-spacing: 0.14em;\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  font-size: 0.85em;\n"_ustr);
    aCss.append(u"  margin-block: 0 1rem;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-hero-title {\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-display);\n"_ustr);
    aCss.append(u"  font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(2rem, 1.3rem + 4.5vw, 4.2rem);"_ustr
                                                  : u"3rem;"_ustr)
        .append(u"\n"_ustr);
    aCss.append(u"  line-height: 1.05;\n"_ustr);
    aCss.append(u"  margin: 0;\n"_ustr);
    aCss.append(u"  max-width: 20ch;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-hero-subtitle {\n"_ustr);
    aCss.append(u"  font-size: clamp(1.1rem, 1rem + 0.8vw, 1.35rem);\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  max-width: 55ch;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-hero-meta {\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  font-size: 0.9em;\n"_ustr);
    aCss.append(u"  border-block-start: 1px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"  padding-block-start: 1em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-stat {\n"_ustr);
    aCss.append(u"  margin-block: 2rem;\n"_ustr);
    aCss.append(u"  padding-block: 1.5rem;\n"_ustr);
    aCss.append(u"  border-block: 1px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-stat-value {\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-display);\n"_ustr);
    aCss.append(u"  font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(2.4rem, 1.8rem + 3.5vw, 4.5rem);"_ustr
                                                  : u"3rem;"_ustr)
        .append(u"\n"_ustr);
    aCss.append(u"  line-height: 1;\n"_ustr);
    aCss.append(u"  margin: 0;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-stat-label {\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  text-transform: uppercase;\n"_ustr);
    aCss.append(u"  letter-spacing: 0.1em;\n"_ustr);
    aCss.append(u"  font-size: 0.85em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-stat-line { color: var(--w27-secondary); }\n"_ustr);
    aCss.append(u".w27-pullquote {\n"_ustr);
    aCss.append(u"  font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(1.3rem, 1.1rem + 1.2vw, 1.8rem);"_ustr
                                                  : u"1.4rem;"_ustr)
        .append(u"\n"_ustr);
    aCss.append(u"  border-inline-start: none;\n"_ustr);
    aCss.append(u"  margin-block: 2.2rem;\n"_ustr);
    aCss.append(u"  padding-inline: 1em 0;\n"_ustr);
    aCss.append(u"  color: var(--w27-text);\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-callout, .w27-research-note {\n"_ustr);
    aCss.append(u"  background: var(--w27-code-bg);\n"_ustr);
    aCss.append(u"  border-inline-start: 3px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"  padding: 1.2em 1.4em;\n"_ustr);
    aCss.append(u"  margin-block: 1.8em;\n"_ustr);
    aCss.append(u"  border-radius: 6px;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-code {\n"_ustr);
    aCss.append(u"  background: var(--w27-code-bg);\n"_ustr);
    aCss.append(u"  padding: 1em 1.2em;\n"_ustr);
    aCss.append(u"  border-radius: 6px;\n"_ustr);
    aCss.append(u"  font-size: 0.9em;\n"_ustr);
    aCss.append(u"  line-height: 1.5;\n"_ustr);
    aCss.append(u"  white-space: pre;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-closing {\n"_ustr);
    aCss.append(u"  margin-block: clamp(2rem, 8vw, 6rem) 0;\n"_ustr);
    aCss.append(u"  text-align: center;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-closing-statement {\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-display);\n"_ustr);
    aCss.append(u"  font-size: ").append(maOptions.mbResponsive
                                                  ? u"clamp(1.7rem, 1.2rem + 2.5vw, 3rem);"_ustr
                                                  : u"2rem;"_ustr)
        .append(u"\n"_ustr);
    aCss.append(u"  max-width: 24ch;\n"_ustr);
    aCss.append(u"  margin-inline: auto;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-closing-line { color: var(--w27-secondary); }\n"_ustr);
    aCss.append(u".w27-section { margin-block: 2.5rem 0; }\n"_ustr);
    aCss.append(u".w27-image { margin-block: 2rem; }\n"_ustr);
    aCss.append(u".w27-image img { display: block; border-radius: 6px; }\n"_ustr);
    aCss.append(u".w27-image-placeholder {\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  font-style: italic;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-notes {\n"_ustr);
    aCss.append(u"  margin-block: 3rem 0;\n"_ustr);
    aCss.append(u"  padding-block-start: 1.5rem;\n"_ustr);
    aCss.append(u"  border-block-start: 1px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"  color: var(--w27-secondary);\n"_ustr);
    aCss.append(u"  font-size: 0.9em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-note-back { text-decoration: none; }\n"_ustr);

    // 5b. navigation, TOC, indexes, frames
    aCss.append(u".w27-toc {\n"_ustr);
    aCss.append(u"  margin-block: 1.5rem 3rem;\n"_ustr);
    aCss.append(u"  padding: 1.1em 1.4em;\n"_ustr);
    aCss.append(u"  background: var(--w27-code-bg);\n"_ustr);
    aCss.append(u"  border-radius: 6px;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-toc ol, .w27-nav ol, .w27-index ol { margin: 0; padding-inline-start: 1.4em; }\n"_ustr);
    aCss.append(u".w27-toc-title, .w27-index-title {\n"_ustr);
    aCss.append(u"  font-family: var(--w27-font-heading);\n"_ustr);
    aCss.append(u"  font-size: 1.1rem;\n"_ustr);
    aCss.append(u"  margin: 0 0 0.6em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-nav { margin-block: 0 2.5rem; padding-block-end: 1rem; }\n"_ustr);
    aCss.append(u".w27-index { margin-block: 2.5rem 0; }\n"_ustr);
    aCss.append(u".w27-index-entry { margin-block: 0.4em 0; }\n"_ustr);
    aCss.append(u".w27-frame {\n"_ustr);
    aCss.append(u"  background: var(--w27-code-bg);\n"_ustr);
    aCss.append(u"  border-inline-start: 3px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"  padding: 1.2em 1.4em;\n"_ustr);
    aCss.append(u"  margin-block: 1.8em;\n"_ustr);
    aCss.append(u"  border-radius: 6px;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-frame-para { margin-block: 0.6em 0; }\n"_ustr);
    aCss.append(u".w27-inline-image { vertical-align: middle; max-width: 100%; height: auto; }\n"_ustr);
    aCss.append(u".w27-frame-inline { font-style: italic; }\n"_ustr);

    // 6. tables
    aCss.append(u".w27-table-wrap {\n"_ustr);
    aCss.append(u"  overflow-x: auto;\n"_ustr);
    aCss.append(u"  margin-block: 1.5em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-table {\n"_ustr);
    aCss.append(u"  width: 100%;\n"_ustr);
    aCss.append(u"  border-collapse: collapse;\n"_ustr);
    aCss.append(u"  font-size: 0.95em;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-table td, .w27-table th {\n"_ustr);
    aCss.append(u"  padding: 0.55em 0.8em;\n"_ustr);
    aCss.append(u"  border-block-end: 1px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"  vertical-align: top;\n"_ustr);
    aCss.append(u"  text-align: start;\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-table thead th {\n"_ustr);
    aCss.append(u"  border-block-end: 2px solid var(--w27-hairline);\n"_ustr);
    aCss.append(u"}\n"_ustr);
    aCss.append(u".w27-table p { margin: 0; }\n"_ustr);

    // 7. custom style classes
    for (const auto& rPair : maParaStyleCss)
        aCss.append(u".").append(rPair.first).append(u" { ").append(rPair.second)
            .append(u"}\n"_ustr);
    for (const auto& rPair : maCharStyleCss)
        aCss.append(u".").append(rPair.first).append(u" { ").append(rPair.second)
            .append(u"}\n"_ustr);

    // 8. responsive breakpoints
    if (maOptions.mbResponsive)
    {
        aCss.append(u"@media (max-width: 720px) {\n"_ustr);
        aCss.append(u"  .w27-columns { display: block; }\n"_ustr);
        aCss.append(u"  main { padding-block: 1.5rem; }\n"_ustr);
        aCss.append(u"  .w27-hero { padding-block: 1rem; }\n"_ustr);
        aCss.append(u"}\n"_ustr);
        aCss.append(u"@media (min-width: 721px) {\n"_ustr);
        aCss.append(u"  .w27-columns { column-count: 2; column-gap: 2.5em; }\n"_ustr);
        aCss.append(u"}\n"_ustr);
    }
    else
    {
        aCss.append(u".w27-columns { column-count: 2; column-gap: 2.5em; }\n"_ustr);
    }

    // 9. print
    if (maOptions.mbIncludePrintCss)
    {
        aCss.append(u"@media print {\n"_ustr);
        aCss.append(u"  html { color-scheme: light; }\n"_ustr);
        aCss.append(u"  body { background: #ffffff; color: #111111; }\n"_ustr);
        aCss.append(u"  main { width: 100%; padding: 0; }\n"_ustr);
        aCss.append(u"  .w27-code, .w27-callout, .w27-research-note, .w27-frame, .w27-toc { background: #f4f4f4; }\n"_ustr);
        aCss.append(u"  .w27-table-wrap { overflow: visible; }\n"_ustr);
        aCss.append(u"  a { color: #0b6e99; }\n"_ustr);
        aCss.append(u"}\n"_ustr);
    }

    return aCss.makeStringAndClear();
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

bool WebExporter::WriteTextFile(const OUString& rUrl, const OUString& rContent)
{
    SvFileStream aStream(rUrl, StreamMode::WRITE | StreamMode::TRUNC);
    if (!aStream.IsOpen())
        return false;
    aStream.WriteOString(OUStringToOString(rContent, RTL_TEXTENCODING_UTF8));
    return aStream.GetError() == ERRCODE_NONE;
}

WebPublishResult WebExporter::Run()
{
    WebPublishResult aResult;
    try
    {
        CollectMetadata();
        ScanForTitle();
        ResolveDocumentStyle();
        CollectBookmarks();
        CollectAnchoredFrames();

        // Body: nodes between the root start node and the root end node.
        const SwNodes& rNodes = mrDoc.GetNodes();
        WriteNodes(SwNodeOffset(1), rNodes.Count() - 1);
        CloseOpenList();
        ClosePendingFigure();
        CloseHeroGroup();
        CloseStatGroup();
        CloseClosingGroup();
        WriteNoteSection();

        BuildGeneratedNav();

        const OUString aCss = MakeCss();

        OUStringBuffer aHtml;
        aHtml.append(u"<!DOCTYPE html>\n"_ustr);
        const OUString aLang = maLang.isEmpty() ? OUString(u"en"_ustr) : maLang;
        aHtml.append(u"<html lang=\""_ustr);
        aHtml.append(lcl_Escape(aLang));
        aHtml.append(u"\">\n<head>\n"_ustr);
        aHtml.append(u"<meta charset=\"utf-8\">\n"_ustr);
        aHtml.append(u"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"_ustr);
        if (maOptions.mbIncludeMetadata)
        {
            const OUString aTitle = maTitle.isEmpty() ? OUString(u"Writer 2027 Publication"_ustr)
                                                      : maTitle;
            aHtml.append(u"<title>"_ustr);
            aHtml.append(lcl_Escape(aTitle));
            aHtml.append(u"</title>\n"_ustr);
            if (!maAuthor.isEmpty())
            {
                aHtml.append(u"<meta name=\"author\" content=\""_ustr);
                aHtml.append(lcl_Escape(maAuthor));
                aHtml.append(u"\">\n"_ustr);
            }
            if (!maSubject.isEmpty())
            {
                aHtml.append(u"<meta name=\"description\" content=\""_ustr);
                aHtml.append(lcl_Escape(maSubject));
                aHtml.append(u"\">\n"_ustr);
            }
            if (!maKeywords.isEmpty())
            {
                aHtml.append(u"<meta name=\"keywords\" content=\""_ustr);
                aHtml.append(lcl_Escape(maKeywords));
                aHtml.append(u"\">\n"_ustr);
            }
        }
        aHtml.append(u"<meta name=\"generator\" content=\""_ustr);
        aHtml.append(HTML_GENERATOR);
        aHtml.append(u"\">\n"_ustr);

        if (maOptions.meFormat == WebPublishFormat::WebPackage)
        {
            aHtml.append(u"<link rel=\"stylesheet\" href=\"styles/writer2027.css\">\n"_ustr);
        }
        else
        {
            aHtml.append(u"<style>\n"_ustr);
            aHtml.append(aCss);
            aHtml.append(u"\n</style>\n"_ustr);
        }
        aHtml.append(u"</head>\n<body>\n<main>\n"_ustr);
        aHtml.append(maGeneratedNav);
        aHtml.append(maBody);
        aHtml.append(u"\n</main>\n</body>\n</html>\n"_ustr);

        // Write files.
        if (maOptions.meFormat == WebPublishFormat::WebPackage)
        {
            osl::Directory::createPath(maOutputDirUrl + u"/styles"_ustr);
            if (!WriteTextFile(maOutputDirUrl + u"/index.html"_ustr, aHtml.makeStringAndClear())
                || !WriteTextFile(maOutputDirUrl + u"/styles/writer2027.css"_ustr, aCss))
            {
                aResult.maError = u"Failed to write the publication files."_ustr;
                return aResult;
            }
            // publication.json (deterministic asset inventory).
            AddAsset(u"index.html"_ustr, u"text/html"_ustr, u"entry"_ustr);
            AddAsset(u"styles/writer2027.css"_ustr, u"text/css"_ustr, u"stylesheet"_ustr);
            WriteManifest();
        }
        else
        {
            if (!WriteTextFile(maOutputDirUrl + u"/index.html"_ustr, aHtml.makeStringAndClear()))
            {
                aResult.maError = u"Failed to write index.html."_ustr;
                return aResult;
            }
        }

        aResult.mbSuccess = true;
        aResult.maIndexUrl = maOutputDirUrl + u"/index.html"_ustr;
    }
    catch (const css::uno::Exception&)
    {
        aResult.maError = u"Failed to read the document while publishing."_ustr;
    }
    catch (const std::exception& rEx)
    {
        aResult.maError = OUString::fromUtf8(rEx.what());
    }
    return aResult;
}

} // namespace

OUString SanitizeName(const OUString& rName)
{
    OUStringBuffer aBuf;
    for (sal_Int32 i = 0; i < rName.getLength(); ++i)
    {
        const sal_Unicode c = rName[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_')
            aBuf.append(static_cast<sal_Unicode>(c >= 'A' && c <= 'Z' ? c + 32 : c));
        else
            aBuf.append('-');
    }
    OUString aResult = aBuf.makeStringAndClear();
    while (aResult.startsWith("-"))
        aResult = aResult.copy(1);
    while (aResult.endsWith("-"))
        aResult = aResult.copy(0, aResult.getLength() - 1);
    if (aResult.isEmpty())
        aResult = u"item"_ustr;
    return aResult;
}

WebPublishResult PublishWeb(SwDoc& rDoc, const WebPublishOptions& rOptions,
                            const OUString& rOutputDirUrl)
{
    WebExporter aExporter(rDoc, rOptions, rOutputDirUrl);
    return aExporter.Run();
}

} // namespace sw::writer2027web

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */